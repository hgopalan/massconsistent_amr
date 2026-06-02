// ============================================================================
// slope_flow_models.H
// Katabatic and Anabatic Slope Flow Parameterization
//
// This header provides functions for computing thermally-driven up-slope
// (anabatic, daytime) and down-slope (katabatic, nighttime) flows on inclined
// terrain due to differential heating/cooling between the surface and air.
//
// Physical basis:
// - Daytime: Surface heating creates warm air layer that flows upslope (anabatic)
// - Nighttime: Surface cooling creates cold air drainage down slopes (katabatic)
// - Dominant feature in mountainous terrain, can override synoptic winds
// - Critical for valley cold air drainage, fire spread, pollutant transport
//
// References:
// - Whiteman, C.D. (1990). Observations of thermally developed wind systems
//   in mountainous terrain. Atmospheric Processes over Complex Terrain,
//   Meteorological Monographs, 23, 5-42.
// - Manins, P.C., & Sawford, B.L. (1979). A model of katabatic winds.
//   Journal of the Atmospheric Sciences, 36(4), 619-630.
// - Zardi, D., & Whiteman, C.D. (2013). Diurnal mountain wind systems.
//   Mountain Weather Research and Forecasting, 35-119.
// ============================================================================

#ifndef SLOPE_FLOW_MODELS_H
#define SLOPE_FLOW_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <algorithm>
#include <cmath>

// ============================================================================
// Slope flow parameters structure
// ============================================================================
struct SlopeFlowParams {
    bool enabled = false;                        // Enable slope flow parameterization
    amrex::Real temperature_diff = 0.0;          // Surface-air temperature difference ΔT [K]
                                                 // Positive = upslope (anabatic)
                                                 // Negative = downslope (katabatic)
    amrex::Real reference_temperature = 300.0;   // Reference temperature T [K]
    amrex::Real empirical_coefficient = 2.5;     // Empirical constant C [m/s] (typically 1-5)
    amrex::Real vertical_decay_height = 50.0;    // Vertical decay height scale [m]
                                                 // Slope flows typically confined to ~50-200 m
    amrex::Real min_slope = 0.05;                // Minimum slope for flow (dimensionless)
                                                 // ~3 degrees, below this slope flows negligible
};

// Physical constants for slope flow model
namespace SlopeFlowConstants {
    // Gravitational acceleration [m/s²]
    constexpr amrex::Real g = amrex::Real(9.81);
    
    // Maximum slope flow velocity magnitude [m/s]
    // Observed katabatic/anabatic flows typically 1-8 m/s, cap at 10 m/s
    constexpr amrex::Real MAX_SLOPE_FLOW_VELOCITY = amrex::Real(10.0);
}

// ============================================================================
// Compute slope flow velocity magnitude
//
// Based on simplified parameterization from observational studies:
//   V_slope = C * g * (ΔT/T) * sin(slope_angle) * height_factor
//
// where:
//   C = empirical coefficient (1-5 m/s, depends on surface roughness)
//   g = gravitational acceleration (9.81 m/s²)
//   ΔT = surface - air temperature difference [K]
//   T = reference temperature [K]
//   slope_angle = terrain slope angle [radians]
//   height_factor = vertical decay function
//
// The flow is:
//   - Upslope (anabatic) when ΔT > 0 (daytime heating)
//   - Downslope (katabatic) when ΔT < 0 (nighttime cooling)
//
// Inputs:
//   slope_magnitude - Terrain slope (rise/run, dimensionless)
//   slope_aspect_x  - X-component of unit vector in upslope direction
//   slope_aspect_y  - Y-component of unit vector in upslope direction
//   z_agl          - Height above ground level [m]
//   params         - Slope flow parameters structure
//
// Returns:
//   Slope flow speed magnitude [m/s] (positive = upslope, negative = downslope)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real slope_flow_speed(
    amrex::Real slope_magnitude,
    amrex::Real z_agl,
    const SlopeFlowParams& params)
{
    if (!params.enabled) {
        return amrex::Real(0.0);  // No slope flow
    }
    
    // Ignore very gentle slopes (below ~3 degrees)
    if (slope_magnitude < params.min_slope) {
        return amrex::Real(0.0);
    }
    
    // Compute slope angle from slope magnitude
    // For small angles: sin(θ) ≈ tan(θ) = slope_magnitude
    // For accuracy, use proper trigonometry: sin(θ) = slope / sqrt(1 + slope²)
    const amrex::Real slope_angle = std::atan(slope_magnitude);
    const amrex::Real sin_slope = std::sin(slope_angle);
    
    // Buoyancy force: g * ΔT/T (reduced gravity)
    // This represents the thermal forcing driving the slope flow
    const amrex::Real buoyancy = SlopeFlowConstants::g 
                                * params.temperature_diff 
                                / params.reference_temperature;
    
    // Vertical decay: slope flows confined to shallow layer near surface
    // Typically strongest at surface (z=0), decay exponentially with height
    // exp(-z/H) where H is the vertical decay height (50-200 m typical)
    const amrex::Real height_factor = (params.vertical_decay_height > amrex::Real(1.0e-10)) 
        ? std::exp(-z_agl / params.vertical_decay_height) 
        : amrex::Real(0.0);
    
    // Slope flow velocity: V = C * g * (ΔT/T) * sin(θ) * f(z)
    // Units: [m/s] = [m/s] * [m/s²] * [K]/[K] * [dimensionless] * [dimensionless]
    //             = [m/s] * [m/s²/m/s²] * [dimensionless] * [dimensionless]
    // The empirical coefficient C already has units [m/s] to balance the equation
    amrex::Real V_slope = params.empirical_coefficient 
                         * buoyancy           // [m/s²]
                         * sin_slope          // [dimensionless]
                         * height_factor;     // [dimensionless]
    
    // Result has units: [m/s]
    
    // Clamp to reasonable physical limits
    constexpr amrex::Real max_slope_flow_velocity = amrex::Real(10.0);
    V_slope = std::max(-max_slope_flow_velocity,
                       std::min(V_slope, max_slope_flow_velocity));
    
    return V_slope;
}

// ============================================================================
// Compute terrain slope aspect (upslope direction)
//
// The aspect is the direction of maximum upslope gradient, represented as
// a unit vector in the horizontal plane.
//
// From terrain gradients dz/dx and dz/dy, the upslope direction is:
//   aspect = (dz/dx, dz/dy) / |grad(z)|
//
// Note: For downslope (katabatic) flow with ΔT < 0, the flow will be
//       opposite to this aspect direction.
//
// Inputs:
//   z_xm, z_xp     - Terrain elevations at x-minus and x-plus neighbors [m]
//   z_ym, z_yp     - Terrain elevations at y-minus and y-plus neighbors [m]
//   dx, dy         - Grid spacings [m]
//
// Outputs:
//   aspect_x       - X-component of upslope unit vector
//   aspect_y       - Y-component of upslope unit vector
//   slope_magnitude - Slope magnitude (rise/run, dimensionless)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_slope_aspect(
    amrex::Real z_xm, amrex::Real z_xp,
    amrex::Real z_ym, amrex::Real z_yp,
    amrex::Real dx, amrex::Real dy,
    amrex::Real& aspect_x,
    amrex::Real& aspect_y,
    amrex::Real& slope_magnitude)
{
    // Central difference gradients
    const amrex::Real dz_dx = (z_xp - z_xm) / (amrex::Real(2.0) * dx);
    const amrex::Real dz_dy = (z_yp - z_ym) / (amrex::Real(2.0) * dy);
    
    // Slope magnitude (gradient magnitude)
    slope_magnitude = std::sqrt(dz_dx * dz_dx + dz_dy * dz_dy);
    
    // Normalize to get aspect direction (unit vector)
    if (slope_magnitude > amrex::Real(1.0e-10)) {
        aspect_x = dz_dx / slope_magnitude;
        aspect_y = dz_dy / slope_magnitude;
    } else {
        // Flat terrain - no preferred aspect
        aspect_x = amrex::Real(0.0);
        aspect_y = amrex::Real(0.0);
    }
}

// ============================================================================
// Apply slope flow to wind velocity components
//
// Adds slope flow velocity to the wind field based on terrain slope and
// thermal forcing. The flow is directed parallel to the slope:
//   - Upslope when ΔT > 0 (surface warmer than air, daytime anabatic)
//   - Downslope when ΔT < 0 (surface cooler than air, nighttime katabatic)
//
// This function should be called during wind field initialization to add
// the slope flow component to the base wind field.
//
// Inputs:
//   u, v           - Horizontal velocity components [m/s] (modified in place)
//   z_xm, z_xp     - Terrain elevations at neighboring points [m]
//   z_ym, z_yp     - Terrain elevations at neighboring points [m]
//   dx, dy         - Grid spacings [m]
//   z_agl          - Height above ground level [m]
//   params         - Slope flow parameters structure
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_slope_flow(
    amrex::Real& u,
    amrex::Real& v,
    amrex::Real z_xm, amrex::Real z_xp,
    amrex::Real z_ym, amrex::Real z_yp,
    amrex::Real dx, amrex::Real dy,
    amrex::Real z_agl,
    const SlopeFlowParams& params)
{
    if (!params.enabled) {
        return;  // No slope flow
    }
    
    // Compute terrain slope and aspect
    amrex::Real aspect_x, aspect_y, slope_magnitude;
    compute_slope_aspect(z_xm, z_xp, z_ym, z_yp, dx, dy,
                        aspect_x, aspect_y, slope_magnitude);
    
    // Compute slope flow velocity magnitude
    const amrex::Real V_slope = slope_flow_speed(slope_magnitude, z_agl, params);
    
    // Add slope flow component to velocity field
    // Direction: upslope (aspect direction) for anabatic (ΔT > 0)
    //           downslope (-aspect direction) for katabatic (ΔT < 0)
    // The sign of V_slope already encodes this through the sign of ΔT
    u += V_slope * aspect_x;
    v += V_slope * aspect_y;
}

#endif // SLOPE_FLOW_MODELS_H
