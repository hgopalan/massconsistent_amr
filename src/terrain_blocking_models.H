// ============================================================================
// terrain_blocking_models.H
// Froude number terrain blocking for flow around obstacles
//
// This header provides functions for computing flow blocking effects when
// the Froude number is low (Fr < 1), indicating that the flow cannot overtop
// the terrain and instead flows around it.
//
// Models implemented:
// 1. Froude number terrain blocking - reduce wind speed upstream of steep terrain
//
// Physical basis:
// - Fr = U/(N*H) where U = wind speed, N = Brunt-Väisälä frequency, H = obstacle height
// - Fr > 1: Flow overtops (wave regime)
// - Fr < 1: Flow blocked, splits around obstacle
// - Creates stagnant zones, strong channeling
//
// References:
// - Baines, P.G. (1995). Topographic Effects in Stratified Flows. Cambridge University Press.
// - Ólafsson, H., & Bougeault, P. (1996). Nonlinear flow past an elliptic mountain ridge.
//   Journal of the Atmospheric Sciences, 53(17), 2465-2489.
// - Epifanio, C.C., & Durran, D.R. (2001). Three-dimensional effects in high-drag-state
//   flows over long ridges. Journal of the Atmospheric Sciences, 58(9), 1051-1065.
// ============================================================================

#ifndef TERRAIN_BLOCKING_MODELS_H
#define TERRAIN_BLOCKING_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <algorithm>
#include <cmath>

namespace {
    using namespace amrex;
}

// ============================================================================
// Terrain blocking parameters structure
// ============================================================================
struct TerrainBlockingParams {
    bool enabled = false;                           // Enable terrain blocking
    amrex::Real brunt_vaisala_frequency = 0.01;     // Brunt-Väisälä frequency N [1/s]
    amrex::Real blocking_reduction_factor = 0.5;    // Maximum reduction factor (0-1)
    amrex::Real transition_froude = 1.0;            // Froude number for transition (typically ~1)
    amrex::Real flank_enhancement_factor = 1.2;     // Speed enhancement on obstacle flanks
};

// Physical constants for terrain blocking
namespace TerrainBlockingConstants {
    // Critical Froude number below which blocking becomes significant
    // Typically Fr ~ 1 is the transition between blocked and overtopping flow
    constexpr amrex::Real CRITICAL_FROUDE = amrex::Real(1.0);
    
    // Minimum Froude number to prevent division by zero
    constexpr amrex::Real MIN_FROUDE = amrex::Real(0.01);
    
    // Minimum wind speed for Froude number calculation [m/s]
    constexpr amrex::Real MIN_WIND_SPEED = amrex::Real(0.1);
}

// ============================================================================
// Compute Brunt-Väisälä frequency from temperature profile
//
// The Brunt-Väisälä frequency (buoyancy frequency) characterizes atmospheric
// stability and determines whether flow can overtop or must go around terrain.
//
// N² = (g/T) * (dT/dz + Γ_d)
//
// where:
//   g = gravitational acceleration [m/s²]
//   T = reference temperature [K]
//   dT/dz = temperature lapse rate [K/m]
//   Γ_d = dry adiabatic lapse rate ≈ 0.0098 K/m
//
// Inputs:
//   temperature       - Reference temperature [K]
//   lapse_rate        - Temperature lapse rate dT/dz [K/m] (positive = temperature increases with height)
//
// Returns:
//   Brunt-Väisälä frequency N [1/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real brunt_vaisala_frequency(
    amrex::Real temperature,
    amrex::Real lapse_rate)
{
    // Physical constants
    constexpr amrex::Real g = amrex::Real(9.81);           // gravity [m/s²]
    constexpr amrex::Real Gamma_d = amrex::Real(0.0098);   // dry adiabatic lapse rate [K/m]
    
    // Ensure valid temperature
    if (temperature < amrex::Real(1.0)) {
        temperature = amrex::Real(300.0);  // default to 300 K
    }
    
    // N² = (g/T) * (dT/dz + Γ_d)
    const amrex::Real N_squared = (g / temperature) * (lapse_rate + Gamma_d);
    
    // Return N (take square root, ensure positive)
    if (N_squared > amrex::Real(0.0)) {
        return std::sqrt(N_squared);
    } else {
        // Neutral or unstable: use small default value
        return amrex::Real(0.001);  // Very weakly stratified
    }
}

// ============================================================================
// Compute Froude number for terrain blocking
//
// Fr = U / (N * H)
//
// where:
//   U = upstream wind speed [m/s]
//   N = Brunt-Väisälä frequency [1/s]
//   H = obstacle height [m]
//
// Physical interpretation:
//   Fr >> 1: Flow easily overtops obstacle (linear wave regime)
//   Fr ~ 1:  Transition regime (partial blocking, wave breaking possible)
//   Fr << 1: Flow strongly blocked (flow splitting, stagnation zones)
//
// Inputs:
//   wind_speed        - Upstream wind speed [m/s]
//   obstacle_height   - Terrain obstacle height above base [m]
//   params            - Terrain blocking parameters structure
//
// Returns:
//   Froude number (dimensionless)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_froude_number(
    amrex::Real wind_speed,
    amrex::Real obstacle_height,
    const TerrainBlockingParams& params)
{
    // Ensure positive values and avoid division by zero
    wind_speed = std::max(wind_speed, TerrainBlockingConstants::MIN_WIND_SPEED);
    obstacle_height = std::max(obstacle_height, amrex::Real(1.0));
    
    const amrex::Real N = std::max(params.brunt_vaisala_frequency, 
                                   TerrainBlockingConstants::MIN_FROUDE);
    
    // Fr = U / (N * H)
    const amrex::Real Fr = wind_speed / (N * obstacle_height);
    
    return Fr;
}

// ============================================================================
// Compute blocking factor based on Froude number
//
// Returns a reduction factor (0-1) to be applied to wind speed:
//   Fr > Fr_crit:  No blocking (factor = 1.0)
//   Fr < Fr_crit:  Partial to full blocking (factor < 1.0)
//
// The blocking factor varies smoothly from 1 (no blocking) to a minimum
// value based on the Froude number.
//
// Inputs:
//   froude_number     - Froude number (dimensionless)
//   params            - Terrain blocking parameters structure
//
// Returns:
//   Blocking factor (0-1, where 1 = no blocking, 0 = full blocking)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real blocking_factor_from_froude(
    amrex::Real froude_number,
    const TerrainBlockingParams& params)
{
    if (!params.enabled) {
        return amrex::Real(1.0);  // No blocking
    }
    
    const amrex::Real Fr_crit = params.transition_froude;
    
    if (froude_number >= Fr_crit) {
        // High Froude number: flow overtops, no blocking
        return amrex::Real(1.0);
    }
    
    // Low Froude number: blocking occurs
    // Use smooth transition function
    // f(Fr) = 1 - reduction_factor * (1 - Fr/Fr_crit)²
    // This gives f(Fr_crit) = 1 and f(0) = 1 - reduction_factor
    
    const amrex::Real Fr_ratio = froude_number / Fr_crit;
    const amrex::Real blocking_strength = amrex::Real(1.0) - Fr_ratio;
    
    // Quadratic decay for smooth transition
    const amrex::Real reduction = params.blocking_reduction_factor 
                                 * blocking_strength 
                                 * blocking_strength;
    
    const amrex::Real factor = amrex::Real(1.0) - reduction;
    
    // Clamp to reasonable range [0.1, 1.0]
    // Don't completely eliminate flow even in strong blocking
    return std::max(amrex::Real(0.1), std::min(factor, amrex::Real(1.0)));
}

// ============================================================================
// Determine if point is upstream or downstream of obstacle
//
// Uses wind direction and terrain slope to determine flow orientation
// relative to the obstacle.
//
// Inputs:
//   u, v              - Wind components [m/s]
//   terrain_slope_x   - Terrain slope in x-direction (dimensionless)
//   terrain_slope_y   - Terrain slope in y-direction (dimensionless)
//
// Returns:
//   True if point is on upstream side of obstacle (flow approaching obstacle)
//   False if point is on downstream side or flow is parallel to terrain
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool is_upstream_of_obstacle(
    amrex::Real u,
    amrex::Real v,
    amrex::Real terrain_slope_x,
    amrex::Real terrain_slope_y)
{
    // Dot product of wind vector and terrain gradient
    // If positive: wind is blowing up the slope (upstream side)
    // If negative: wind is blowing down the slope (downstream side)
    const amrex::Real dot_product = u * terrain_slope_x + v * terrain_slope_y;
    
    return (dot_product > amrex::Real(0.0));
}

// ============================================================================
// Determine if point is on obstacle flank (side)
//
// Points on the flanks of an obstacle experience flow channeling and
// potential speed enhancement as the flow diverts around the obstacle.
//
// Inputs:
//   terrain_curvature - Terrain curvature [1/m] (from second derivatives)
//   slope_magnitude   - Terrain slope magnitude (dimensionless)
//
// Returns:
//   True if point is on obstacle flank (significant curvature without excessive slope)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool is_on_obstacle_flank(
    amrex::Real terrain_curvature,
    amrex::Real slope_magnitude)
{
    // Flanks have moderate curvature but are not at the crest
    // Avoid very steep slopes (those are more likely crest or cliff)
    const bool has_curvature = std::abs(terrain_curvature) > amrex::Real(1.0e-5);
    const bool moderate_slope = (slope_magnitude > amrex::Real(0.1) && 
                                 slope_magnitude < amrex::Real(0.5));
    
    return has_curvature && moderate_slope;
}

// ============================================================================
// Apply terrain blocking to wind velocity components
//
// Modifies wind velocity based on Froude number criterion:
// - Upstream side of obstacle: reduce speed if Fr < 1 (blocking)
// - Flanks of obstacle: enhance speed (channeling effect)
// - Downstream side: no modification (handled by other models like wake)
//
// Inputs:
//   u, v              - Horizontal velocity components [m/s] (modified in place)
//   wind_speed        - Current wind speed magnitude [m/s]
//   obstacle_height   - Terrain obstacle height [m]
//   terrain_slope_x   - Terrain slope in x-direction
//   terrain_slope_y   - Terrain slope in y-direction
//   terrain_curvature - Terrain curvature [1/m]
//   params            - Terrain blocking parameters structure
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_terrain_blocking(
    amrex::Real& u,
    amrex::Real& v,
    amrex::Real wind_speed,
    amrex::Real obstacle_height,
    amrex::Real terrain_slope_x,
    amrex::Real terrain_slope_y,
    amrex::Real terrain_curvature,
    const TerrainBlockingParams& params)
{
    if (!params.enabled || obstacle_height < amrex::Real(1.0)) {
        return;  // No modification
    }
    
    // Compute Froude number
    const amrex::Real Fr = compute_froude_number(wind_speed, obstacle_height, params);
    
    // Compute slope magnitude
    const amrex::Real slope_mag = std::sqrt(terrain_slope_x * terrain_slope_x + 
                                            terrain_slope_y * terrain_slope_y);
    
    // Check flow orientation relative to obstacle
    const bool upstream = is_upstream_of_obstacle(u, v, terrain_slope_x, terrain_slope_y);
    const bool on_flank = is_on_obstacle_flank(terrain_curvature, slope_mag);
    
    if (upstream && Fr < params.transition_froude) {
        // Upstream side with low Froude number: apply blocking
        const amrex::Real block_factor = blocking_factor_from_froude(Fr, params);
        u *= block_factor;
        v *= block_factor;
        
    } else if (on_flank && Fr < params.transition_froude) {
        // On obstacle flanks: apply flow channeling enhancement
        // Flow speeds up as it diverts around the obstacle
        const amrex::Real enhance_factor = params.flank_enhancement_factor;
        u *= enhance_factor;
        v *= enhance_factor;
    }
    // Downstream: no modification (wake effects handled separately)
}

#endif // TERRAIN_BLOCKING_MODELS_H
