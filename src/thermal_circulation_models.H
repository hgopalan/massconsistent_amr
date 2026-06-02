// ============================================================================
// thermal_circulation_models.H
// Sea breeze and land breeze parameterization for coastal thermal circulation
//
// This header provides functions for computing thermal circulation effects
// driven by land-sea temperature contrasts. The model adds onshore/offshore
// winds based on the thermal gradient between land and water surfaces.
//
// Models implemented:
// 1. Simple sea breeze parameterization - thermal wind from temperature contrast
//
// Physical basis:
// - Daytime: Cool ocean air flows inland (sea breeze)
// - Nighttime: Warm land air flows seaward (land breeze)
// - Affects pollution transport, weather, and fire behavior
//
// References:
// - Simpson, J.E. (1994). Sea Breeze and Local Winds. Cambridge University Press.
// - Atkinson, B.W. (1981). Meso-scale Atmospheric Circulations. Academic Press.
// ============================================================================

#ifndef THERMAL_CIRCULATION_MODELS_H
#define THERMAL_CIRCULATION_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <algorithm>
#include <cmath>

namespace {
    using namespace amrex;
}

// ============================================================================
// Thermal circulation parameters structure
// ============================================================================
struct ThermalCirculationParams {
    bool enabled = false;                         // Enable thermal circulation
    amrex::Real temperature_contrast = 0.0;       // Land-sea temperature difference ΔT [K]
    amrex::Real reference_temperature = 300.0;    // Reference temperature T₀ [K]
    amrex::Real thermal_coefficient = 1.0;        // Scaling coefficient (dimensionless)
    amrex::Real vertical_decay_height = 1000.0;   // Vertical decay height scale [m]
    amrex::Real distance_scale = 5000.0;          // Horizontal distance scale from coast [m]
};

// Physical constants for thermal circulation
namespace ThermalCirculationConstants {
    // Gravitational acceleration [m/s²]
    constexpr amrex::Real g = amrex::Real(9.81);
    
    // Maximum thermal wind amplitude as fraction of geostrophic balance
    // Typically sea breeze reaches 5-10 m/s in strong cases
    constexpr amrex::Real MAX_THERMAL_WIND = amrex::Real(10.0);
}

// ============================================================================
// Compute thermal wind velocity from temperature contrast
//
// Based on thermal wind balance: V_thermal ∝ g * ΔT/T * distance
// 
// The thermal wind is proportional to the horizontal temperature gradient
// and varies with:
// - Temperature contrast (ΔT): Larger contrast → stronger circulation
// - Distance from coast: Linear increase inland, peaks at ~distance_scale
// - Height: Strongest near surface, decays exponentially aloft
//
// Inputs:
//   distance_from_coast - Distance from coastline [m] (positive inland, negative over water)
//   z_agl              - Height above ground level [m]
//   coast_normal_x     - X-component of unit normal vector pointing inland
//   coast_normal_y     - Y-component of unit normal vector pointing inland
//   params             - Thermal circulation parameters structure
//
// Returns:
//   Thermal wind speed magnitude [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real thermal_wind_speed(
    amrex::Real distance_from_coast,
    amrex::Real z_agl,
    const ThermalCirculationParams& params)
{
    if (!params.enabled) {
        return amrex::Real(0.0);  // No thermal circulation
    }
    
    // Thermal wind formula: V_thermal = coefficient * g * ΔT/T₀ * distance_factor * height_factor
    
    // Temperature effect: reduced gravity based on thermal contrast
    // g' = g * ΔT / T₀
    const amrex::Real reduced_g = ThermalCirculationConstants::g 
                                 * params.temperature_contrast 
                                 / params.reference_temperature;
    
    // Distance factor: ramps up from coast to inland
    // Peak strength at distance_scale, then gradually decreases
    const amrex::Real dist_normalized = distance_from_coast / params.distance_scale;
    
    // Use a function that increases from coast, peaks around x=1, then decreases
    // f(x) = x * exp(-x/2) has maximum at x=2
    const amrex::Real distance_factor = dist_normalized 
                                       * std::exp(-std::abs(dist_normalized) * amrex::Real(0.5));
    
    // Vertical decay: strongest near surface, decays exponentially aloft
    // exp(-z/H) where H is the vertical decay height
    const amrex::Real height_factor = (params.vertical_decay_height > amrex::Real(1.0e-10)) 
        ? std::exp(-z_agl / params.vertical_decay_height) 
        : amrex::Real(0.0);
    
    // Combine factors to get thermal wind magnitude
    amrex::Real V_thermal = params.thermal_coefficient 
                           * reduced_g 
                           * params.distance_scale  // [m]
                           * distance_factor        // [dimensionless]
                           * height_factor;         // [dimensionless]
    
    // Result has units: [dimensionless] * [m/s²] * [m] = [m/s]
    
    // Clamp to reasonable physical limits
    constexpr amrex::Real max_thermal_wind = amrex::Real(10.0);
    V_thermal = std::max(-max_thermal_wind,
                         std::min(V_thermal, max_thermal_wind));
    
    return V_thermal;
}

// ============================================================================
// Apply thermal circulation to wind velocity components
//
// Adds thermal wind velocity to the initial wind field based on distance
// from the coastline and the coast-normal direction.
//
// For positive temperature contrast (ΔT > 0, land warmer than sea):
//   - Creates onshore flow (sea breeze) during daytime
//   - Velocity directed inland (normal to coast)
//
// For negative temperature contrast (ΔT < 0, sea warmer than land):
//   - Creates offshore flow (land breeze) during nighttime
//   - Velocity directed seaward (opposite to coast normal)
//
// Inputs:
//   u, v               - Horizontal velocity components [m/s] (modified in place)
//   distance_from_coast - Distance from coastline [m]
//   z_agl              - Height above ground level [m]
//   coast_normal_x     - X-component of unit normal vector pointing inland
//   coast_normal_y     - Y-component of unit normal vector pointing inland
//   params             - Thermal circulation parameters structure
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_thermal_circulation(
    amrex::Real& u,
    amrex::Real& v,
    amrex::Real distance_from_coast,
    amrex::Real z_agl,
    amrex::Real coast_normal_x,
    amrex::Real coast_normal_y,
    const ThermalCirculationParams& params)
{
    if (!params.enabled) {
        return;  // No modification
    }
    
    // Compute thermal wind speed magnitude
    const amrex::Real V_thermal = thermal_wind_speed(distance_from_coast, z_agl, params);
    
    // Add thermal wind component in the coast-normal direction
    // Positive ΔT (land warmer) → inland flow → add coast_normal direction
    // Negative ΔT (sea warmer) → seaward flow → subtract coast_normal direction
    u += V_thermal * coast_normal_x;
    v += V_thermal * coast_normal_y;
}

// ============================================================================
// Compute distance from coastline (land-sea mask based)
//
// Simple implementation: uses a land-sea mask value (0 = water, 1 = land)
// and computes a proxy distance based on the mask value and neighbors.
//
// For more sophisticated implementations, this could be replaced with
// a pre-computed distance field from actual coastline geometry.
//
// Inputs:
//   land_sea_mask      - Mask value (0.0 = water, 1.0 = land, fractional for coastal cells)
//   x, y               - Coordinates of current point [m]
//   coastline_x        - Representative coastline X coordinate [m]
//   coastline_y        - Representative coastline Y coordinate [m]
//
// Returns:
//   Distance from coast [m] (positive inland, negative over water)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_distance_from_coast(
    amrex::Real land_sea_mask,
    amrex::Real x,
    amrex::Real y,
    amrex::Real coastline_x,
    amrex::Real coastline_y)
{
    // Simple proxy: distance from reference coastline point
    const amrex::Real dx = x - coastline_x;
    const amrex::Real dy = y - coastline_y;
    const amrex::Real dist = std::sqrt(dx * dx + dy * dy);
    
    // Sign based on land/sea mask
    // land_sea_mask = 1.0 (land) → positive distance
    // land_sea_mask = 0.0 (water) → negative distance
    return dist * (amrex::Real(2.0) * land_sea_mask - amrex::Real(1.0));
}

#endif // THERMAL_CIRCULATION_MODELS_H
