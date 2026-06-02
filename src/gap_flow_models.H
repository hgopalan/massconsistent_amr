// ============================================================================
// gap_flow_models.H
// Gap flow parameterization for mountain passes and valleys
//
// This header provides functions for computing enhanced flow through mountain
// gaps and passes due to pressure-driven channeling. Gap flows can reach
// 2-4× synoptic wind speeds and are critical for wind energy assessment.
//
// Models implemented:
// 1. Gap detection algorithm - identify terrain constrictions
// 2. Pressure-driven gap flow - enhanced flow through gaps
//
// Physical basis:
// - Pressure difference drives flow through gap
// - Speed-up factor 2-4× synoptic wind (e.g., Columbia River Gorge)
// - Flow aligns with gap axis
// - Important for wind energy, navigation, regional patterns
//
// References:
// - Mass, C.F., & Albright, M.D. (1987). Coastal southerlies and alongshore
//   surges of the west coast of North America: Evidence of mesoscale
//   topographically trapped response to synoptic forcing. *Monthly Weather
//   Review*, 115(8), 1707-1738.
// - Sharp, J., & Mass, C.F. (2004). Columbia Gorge gap winds: Their
//   climatological influence and synoptic evolution. *Weather and
//   Forecasting*, 19(6), 970-992.
// - Overland, J.E., & Bond, N.A. (1993). The influence of coastal orography:
//   The Yakutat storm. *Monthly Weather Review*, 121(5), 1388-1397.
// - Jackson, P.L., Mayr, G., & Vosper, S. (2013). Dynamically-driven winds.
//   *Mountain Weather Research and Forecasting*, Springer, 121-218.
// ============================================================================

#ifndef GAP_FLOW_MODELS_H
#define GAP_FLOW_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <algorithm>
#include <cmath>

namespace {
    using namespace amrex;
}

// ============================================================================
// Gap flow parameters structure
// ============================================================================
struct GapFlowParams {
    bool enabled = false;                           // Enable gap flow parameterization
    amrex::Real gap_orientation_deg = 0.0;          // Gap axis orientation [degrees, 0=east, 90=north]
    amrex::Real gap_width = 1000.0;                 // Gap width [m]
    amrex::Real gap_depth = 500.0;                  // Gap depth (elevation range) [m]
    amrex::Real pressure_coefficient = 1.0;         // Pressure-driven flow coefficient (dimensionless)
    amrex::Real speedup_factor_max = 3.0;           // Maximum gap flow speedup (typically 2-4)
    amrex::Real gap_center_x = 0.0;                 // Gap center X coordinate [m]
    amrex::Real gap_center_y = 0.0;                 // Gap center Y coordinate [m]
    amrex::Real transition_width = 500.0;           // Transition zone width [m]
    amrex::Real vertical_extent = 1000.0;           // Vertical extent of gap flow influence [m]
};

// Physical constants for gap flow
namespace GapFlowConstants {
    // Air density at sea level [kg/m³]
    constexpr amrex::Real RHO_0 = amrex::Real(1.225);
    
    // Gravitational acceleration [m/s²]
    constexpr amrex::Real g = amrex::Real(9.81);
    
    // Minimum synoptic wind speed for gap flow activation [m/s]
    constexpr amrex::Real MIN_SYNOPTIC_WIND = amrex::Real(2.0);
    
    // Maximum gap flow speed enhancement to prevent unrealistic values [m/s]
    constexpr amrex::Real MAX_GAP_WIND = amrex::Real(40.0);
    
    // Pi constant for angle conversions
    constexpr amrex::Real PI = amrex::Real(3.14159265358979323846);
}

// ============================================================================
// Compute gap geometry detection
//
// Detects if a point is within or near a mountain gap based on distance
// from gap center and alignment with gap axis.
//
// Inputs:
//   x, y              - Point coordinates [m]
//   params            - Gap flow parameters structure
//
// Returns:
//   Gap influence factor (0-1): 1 = inside gap, 0 = outside, 0-1 = transition
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_gap_influence(
    amrex::Real x,
    amrex::Real y,
    const GapFlowParams& params)
{
    if (!params.enabled) {
        return amrex::Real(0.0);
    }
    
    // Convert gap orientation to radians
    const amrex::Real gap_angle_rad = params.gap_orientation_deg * GapFlowConstants::PI / amrex::Real(180.0);
    
    // Gap axis direction vector (unit vector along gap)
    const amrex::Real gap_axis_x = std::cos(gap_angle_rad);
    const amrex::Real gap_axis_y = std::sin(gap_angle_rad);
    
    // Perpendicular to gap axis (across gap)
    const amrex::Real gap_perp_x = -std::sin(gap_angle_rad);
    const amrex::Real gap_perp_y = std::cos(gap_angle_rad);
    
    // Vector from gap center to point
    const amrex::Real dx = x - params.gap_center_x;
    const amrex::Real dy = y - params.gap_center_y;
     
    // Distance across gap (perpendicular to gap axis)
    const amrex::Real dist_across = std::abs(dx * gap_perp_x + dy * gap_perp_y);
    
    // Compute influence based on distance across gap
    // Full influence inside gap (dist_across < gap_width/2)
    // Transition zone from gap_width/2 to gap_width/2 + transition_width
    const amrex::Real half_width = params.gap_width / amrex::Real(2.0);
    
    if (dist_across < half_width) {
        // Inside gap core
        return amrex::Real(1.0);
    } else if (dist_across < half_width + params.transition_width) {
        // Transition zone - smooth decay
        const amrex::Real normalized_dist = (dist_across - half_width) / params.transition_width;
        // Cosine taper for smooth transition
        return amrex::Real(0.5) * (amrex::Real(1.0) + std::cos(GapFlowConstants::PI * normalized_dist));
    } else {
        // Outside gap influence
        return amrex::Real(0.0);
    }
}

// ============================================================================
// Compute pressure difference across gap
//
// Calculates pressure difference that drives flow through the gap based on
// synoptic wind orientation and gap alignment.
//
// Inputs:
//   U_synoptic, V_synoptic - Synoptic wind components [m/s]
//   params                 - Gap flow parameters structure
//
// Returns:
//   Pressure difference [Pa]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_gap_pressure_difference(
    amrex::Real U_synoptic,
    amrex::Real V_synoptic,
    const GapFlowParams& params)
{
    if (!params.enabled) {
        return amrex::Real(0.0);
    }
    
    // Synoptic wind speed and direction
    const amrex::Real U_mag = std::sqrt(U_synoptic * U_synoptic + V_synoptic * V_synoptic);
    
    if (U_mag < GapFlowConstants::MIN_SYNOPTIC_WIND) {
        return amrex::Real(0.0);  // No gap flow for weak synoptic winds
    }
    
    // Synoptic wind direction [radians]
    const amrex::Real wind_dir = std::atan2(V_synoptic, U_synoptic);
    
    // Gap orientation [radians]
    const amrex::Real gap_angle_rad = params.gap_orientation_deg * GapFlowConstants::PI / amrex::Real(180.0);
    
    // Angle between wind and gap axis
    amrex::Real angle_diff = std::abs(wind_dir - gap_angle_rad);
    
    // Normalize to [0, π]
    if (angle_diff > GapFlowConstants::PI) {
        angle_diff = amrex::Real(2.0) * GapFlowConstants::PI - angle_diff;
    }
    
    // Projection of wind onto gap axis (positive = along gap, negative = against gap)
    const amrex::Real wind_alignment = std::cos(angle_diff);
    
    // Pressure difference based on dynamic pressure and gap geometry
    // Δp = C * ρ * U² * f(alignment, gap_geometry)
    // 
    // The pressure gradient across mountain gaps is proportional to:
    // - Synoptic wind speed squared (dynamic pressure)
    // - Alignment between wind and gap (maximum when perpendicular to terrain)
    // - Gap constriction factor
    
    const amrex::Real dynamic_pressure = amrex::Real(0.5) * GapFlowConstants::RHO_0 * U_mag * U_mag;
    
    // Gap constriction factor (narrower gap = larger pressure gradient)
    // Typical gap widths: 1-10 km, depths: 100-1000 m
    const amrex::Real aspect_ratio = params.gap_depth / params.gap_width;
    const amrex::Real constriction_factor = amrex::Real(1.0) + aspect_ratio;
    
    // Pressure difference (alignment can reverse flow if wind opposes gap)
    const amrex::Real delta_p = params.pressure_coefficient 
                                * dynamic_pressure 
                                * wind_alignment 
                                * constriction_factor;
    
    return delta_p;
}

// ============================================================================
// Compute gap flow velocity
//
// Calculates the enhanced wind speed through a mountain gap based on
// pressure-driven flow. Uses Bernoulli principle: U_gap = sqrt(2*Δp/ρ).
//
// Inputs:
//   delta_p           - Pressure difference across gap [Pa]
//   U_base, V_base    - Base wind components [m/s]
//   z_agl             - Height above ground level [m]
//   gap_influence     - Gap influence factor (0-1)
//   params            - Gap flow parameters structure
//
// Outputs:
//   U_gap, V_gap      - Gap-enhanced wind components [m/s] (modified in place)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_gap_flow_velocity(
    amrex::Real delta_p,
    amrex::Real U_base,
    amrex::Real V_base,
    amrex::Real z_agl,
    amrex::Real gap_influence,
    const GapFlowParams& params,
    amrex::Real& U_gap,
    amrex::Real& V_gap)
{
    // Initialize with base wind
    U_gap = U_base;
    V_gap = V_base;
    
    if (!params.enabled || gap_influence < amrex::Real(1e-6)) {
        return;  // No gap flow influence
    }
    
    // Pressure-driven gap flow speed
    // From Bernoulli equation: U_gap = C * sqrt(2 * Δp / ρ)
    amrex::Real U_gap_pressure = amrex::Real(0.0);
    
    if (std::abs(delta_p) > amrex::Real(1e-6)) {
        const amrex::Real pressure_term = amrex::Real(2.0) * std::abs(delta_p) / GapFlowConstants::RHO_0;
        U_gap_pressure = std::sqrt(pressure_term);
        
        // Apply sign based on pressure gradient direction
        if (delta_p < amrex::Real(0.0)) {
            U_gap_pressure = -U_gap_pressure;
        }
    }
    
    // Limit gap flow speed
    const amrex::Real base_speed = std::sqrt(U_base * U_base + V_base * V_base);
    const amrex::Real max_enhancement = params.speedup_factor_max * base_speed;
    constexpr amrex::Real max_gap_wind = amrex::Real(40.0);
    const amrex::Real max_gap_speed = std::min(max_enhancement, max_gap_wind);
    U_gap_pressure = std::max(-max_gap_speed, std::min(U_gap_pressure, max_gap_speed));
    
    // Gap axis direction
    const amrex::Real gap_angle_rad = params.gap_orientation_deg * GapFlowConstants::PI / amrex::Real(180.0);
    const amrex::Real gap_axis_x = std::cos(gap_angle_rad);
    const amrex::Real gap_axis_y = std::sin(gap_angle_rad);
    
    // Gap flow velocity components (aligned with gap axis)
    const amrex::Real U_gap_x = U_gap_pressure * gap_axis_x;
    const amrex::Real V_gap_y = U_gap_pressure * gap_axis_y;
    
    // Vertical decay of gap flow influence
    // Gap flows are strongest near surface, decay with height
    amrex::Real vertical_factor = amrex::Real(1.0);
    if (z_agl > amrex::Real(0.0) && params.vertical_extent > amrex::Real(0.0)) {
        vertical_factor = std::exp(-z_agl / params.vertical_extent);
    }
    
    // Blend gap flow with base wind based on gap influence and height
    const amrex::Real blend_factor = gap_influence * vertical_factor;
    
    U_gap = U_base + blend_factor * U_gap_x;
    V_gap = V_base + blend_factor * V_gap_y;
}

// ============================================================================
// Apply gap flow parameterization
//
// Main function to apply gap flow effects to wind field.
// This function combines gap detection, pressure calculation, and
// velocity enhancement.
//
// Inputs:
//   x, y              - Point coordinates [m]
//   z_agl             - Height above ground level [m]
//   U_in, V_in        - Input wind components [m/s]
//   U_synoptic        - Synoptic wind U component [m/s]
//   V_synoptic        - Synoptic wind V component [m/s]
//   params            - Gap flow parameters structure
//
// Outputs:
//   U_out, V_out      - Gap-modified wind components [m/s] (modified in place)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_gap_flow(
    amrex::Real x,
    amrex::Real y,
    amrex::Real z_agl,
    amrex::Real U_in,
    amrex::Real V_in,
    amrex::Real U_synoptic,
    amrex::Real V_synoptic,
    const GapFlowParams& params,
    amrex::Real& U_out,
    amrex::Real& V_out)
{
    // Initialize output with input
    U_out = U_in;
    V_out = V_in;
    
    if (!params.enabled) {
        return;
    }
    
    // 1. Detect gap geometry and compute influence factor
    const amrex::Real gap_influence = compute_gap_influence(x, y, params);
    
    if (gap_influence < amrex::Real(1e-6)) {
        return;  // Outside gap influence zone
    }
    
    // 2. Compute pressure difference driving gap flow
    const amrex::Real delta_p = compute_gap_pressure_difference(U_synoptic, V_synoptic, params);
    
    // 3. Compute gap flow velocity
    compute_gap_flow_velocity(delta_p, U_in, V_in, z_agl, gap_influence, params, U_out, V_out);
}

#endif // GAP_FLOW_MODELS_H
