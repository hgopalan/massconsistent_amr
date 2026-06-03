// ============================================================================
// directional_bias_correction.H
// Wind direction bias correction for systematic model errors
//
// This header provides functions for correcting systematic directional and
// speed biases in wind forcing from NWP models. Common biases include:
//
// 1. Directional bias: systematic deviation from observed direction
//    Example: Model wind always 30° too far north
//
// 2. Speed bias: wind speed that depends on direction
//    Example: Model overestimates speed from NE, underestimates from SW
//
// 3. Channeling bias: terrain-induced bias in complex terrain
//    Example: Valley models over/underestimate channel flow
//
// Correction is applied as:
//   U_corrected = U_input * f_speed(direction)
//   direction_corrected = direction_input + bias_angle(direction)
//
// References:
// - Durai, V. R., & Bhradwaj, R. (2009). Improved wind power forecasting
//   using an ensemble method. Journal of Applied Meteorology, 48, 1373.
// - Wilczak, J. M., et al. (2015). The Wind Forecast Improvement Project
//   Phase 2: A collaborative approach to improve short-term wind power
//   forecasting. Bulletin of the American Meteorological Society.
// ============================================================================

#ifndef DIRECTIONAL_BIAS_CORRECTION_H
#define DIRECTIONAL_BIAS_CORRECTION_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include "math_constants.H"
#include <cmath>

// ============================================================================
// Bias correction parameters
// ============================================================================
struct DirectionalBiasCorrectionParams {
    bool enabled = false;                    // Master enable/disable
    
    // Constant bias (applied to all directions)
    amrex::Real direction_bias_constant = 0.0;  // [degrees] constant directional offset
    amrex::Real speed_bias_factor = 1.0;        // [dimensionless] constant speed multiplier
    
    // Periodic sinusoidal bias (varies with direction)
    bool enable_periodic_bias = false;
    amrex::Real direction_bias_amplitude = 0.0;  // [degrees] amplitude of periodic bias
    amrex::Real direction_bias_phase = 0.0;      // [degrees] phase offset (0=bias max at 0°)
    
    // Speed correction as function of direction
    bool enable_direction_dependent_speed = false;
    amrex::Real speed_bias_amplitude = 0.0;      // [dimensionless] amplitude of speed variation
};

// ============================================================================
// Convert wind component (U, V) to direction and speed
//
// Wind direction is measured clockwise from North (0° = from North, 90° = from East)
// This is the standard meteorological convention.
//
// Inputs:
//   u           - Zonal wind component (eastward, typically x-component) [m/s]
//   v           - Meridional wind component (northward, typically y-component) [m/s]
//
// Outputs:
//   speed       - Wind speed magnitude [m/s]
//   direction   - Wind direction (from where wind blows) [degrees, 0-360]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void wind_components_to_direction_speed(
    amrex::Real u,
    amrex::Real v,
    amrex::Real& speed,
    amrex::Real& direction)
{
    // Compute speed
    speed = std::sqrt(u * u + v * v);
    
    // Compute direction: atan2(u, v) gives angle from north, clockwise
    // atan2(y, x) computes angle from x-axis, so we need atan2(u, v) for north-based
    amrex::Real angle_rad = std::atan2(u, v);
    direction = angle_rad * 180.0 / MathConstants::pi;
    
    // Normalize to [0, 360) degrees
    if (direction < 0.0) {
        direction += 360.0;
    }
}

// ============================================================================
// Convert direction and speed back to wind components (U, V)
//
// Inputs:
//   speed       - Wind speed magnitude [m/s]
//   direction   - Wind direction (from where wind blows) [degrees]
//
// Outputs:
//   u           - Zonal wind component (eastward) [m/s]
//   v           - Meridional wind component (northward) [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void direction_speed_to_wind_components(
    amrex::Real speed,
    amrex::Real direction,
    amrex::Real& u,
    amrex::Real& v)
{
    // Convert direction from degrees to radians
    amrex::Real dir_rad = direction * MathConstants::pi / 180.0;
    
    // Standard meteorological convention: direction is from where wind comes
    // u = speed * sin(direction)
    // v = speed * cos(direction)
    u = speed * std::sin(dir_rad);
    v = speed * std::cos(dir_rad);
}

// ============================================================================
// Apply periodic sinusoidal directional bias correction
//
// Bias varies sinusoidally with wind direction:
//   bias(dir) = amplitude * sin(dir - phase)
//
// This models systematic errors that repeat with direction (e.g., model
// consistently overestimates NE/SW winds but underestimates NW/SE winds).
//
// Inputs:
//   direction       - Wind direction [degrees, 0-360]
//   amplitude       - Bias amplitude [degrees]
//   phase           - Phase offset [degrees] (0 = max bias at north)
//
// Returns:
//   directional_bias - Bias to ADD to direction [degrees]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_periodic_direction_bias(
    amrex::Real direction,
    amrex::Real amplitude,
    amrex::Real phase)
{
    // Convert to radians
    amrex::Real dir_rad = direction * MathConstants::pi / 180.0;
    amrex::Real phase_rad = phase * MathConstants::pi / 180.0;
    
    // Compute sinusoidal bias
    amrex::Real bias = amplitude * std::sin(dir_rad - phase_rad);
    return bias;
}

// ============================================================================
// Apply direction-dependent speed correction
//
// Speed correction varies with wind direction:
//   f_speed(dir) = 1 + amplitude * sin(2*dir - phase)
//
// This models directional dependence of speed errors, e.g., NWP models
// consistently underestimate downslope/upslope winds in certain directions.
//
// Inputs:
//   direction       - Wind direction [degrees]
//   amplitude       - Correction amplitude [dimensionless]
//   phase           - Phase offset [degrees]
//
// Returns:
//   speed_multiplier - Factor to multiply wind speed [dimensionless]
//                      Typical range: [0.7, 1.3]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_direction_dependent_speed_correction(
    amrex::Real direction,
    amrex::Real amplitude,
    amrex::Real phase)
{
    // Convert to radians
    amrex::Real dir_rad = direction * MathConstants::pi / 180.0;
    amrex::Real phase_rad = phase * MathConstants::pi / 180.0;
    
    // Compute speed correction: 1 + amplitude * sin(2*dir - phase)
    // 2*dir gives 0-720° over full circle, creating 2-cycle variation
    amrex::Real correction = amrex::Real(1.0) + amplitude * std::sin(amrex::Real(2.0) * dir_rad - phase_rad);
    
    // Clamp to reasonable range [0.5, 1.5] to avoid extreme corrections
    correction = (correction < amrex::Real(0.5)) ? amrex::Real(0.5) : correction;
    correction = (correction > amrex::Real(1.5)) ? amrex::Real(1.5) : correction;
    
    return correction;
}

// ============================================================================
// Apply full directional bias correction to wind components
//
// Applies all enabled corrections (constant, periodic, speed) to convert
// input wind (typically from NWP model) to bias-corrected wind field.
//
// Inputs:
//   u_input         - Input zonal wind [m/s]
//   v_input         - Input meridional wind [m/s]
//   params          - Bias correction parameters
//
// Outputs:
//   u_corrected     - Corrected zonal wind [m/s]
//   v_corrected     - Corrected meridional wind [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void apply_directional_bias_correction(
    amrex::Real u_input,
    amrex::Real v_input,
    const DirectionalBiasCorrectionParams& params,
    amrex::Real& u_corrected,
    amrex::Real& v_corrected)
{
    if (!params.enabled) {
        u_corrected = u_input;
        v_corrected = v_input;
        return;
    }
    
    // Step 1: Extract direction and speed from input
    amrex::Real speed, direction;
    wind_components_to_direction_speed(u_input, v_input, speed, direction);
    
    // Step 2: Apply constant speed bias
    speed *= params.speed_bias_factor;
    
    // Step 3: Apply constant directional bias
    direction += params.direction_bias_constant;
    
    // Step 4: Apply periodic directional bias
    if (params.enable_periodic_bias && params.direction_bias_amplitude > amrex::Real(1.0e-6)) {
        amrex::Real periodic_bias = compute_periodic_direction_bias(
            direction,
            params.direction_bias_amplitude,
            params.direction_bias_phase);
        direction += periodic_bias;
    }
    
    // Step 5: Apply direction-dependent speed correction
    if (params.enable_direction_dependent_speed && params.speed_bias_amplitude > amrex::Real(1.0e-6)) {
        amrex::Real speed_multiplier = compute_direction_dependent_speed_correction(
            direction,
            params.speed_bias_amplitude,
            amrex::Real(0.0));  // phase = 0 for default behavior
        speed *= speed_multiplier;
    }
    
    // Step 6: Normalize direction to [0, 360)
    while (direction < amrex::Real(0.0)) {
        direction += amrex::Real(360.0);
    }
    while (direction >= amrex::Real(360.0)) {
        direction -= amrex::Real(360.0);
    }
    
    // Step 7: Convert corrected direction & speed back to components
    direction_speed_to_wind_components(speed, direction, u_corrected, v_corrected);
}

#endif  // DIRECTIONAL_BIAS_CORRECTION_H
