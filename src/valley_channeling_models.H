// ============================================================================
// valley_channeling_models.H
// Valley channeling factor for wind alignment with valley topography
//
// This header provides functions for computing valley channeling effects
// that align wind direction with valley axis and adjust speed based on
// valley geometry (width, depth, synoptic direction).
//
// Physical basis:
// - Valleys strongly channel flow regardless of synoptic direction
// - Wind aligns within ±30° of valley axis (observed behavior)
// - Speed-up in narrow valleys (venturi effect)
// - Speed reduction in wide valleys (friction)
//
// Models implemented:
// 1. Valley axis detection from terrain (ridge-line detection)
// 2. Channeling strength based on valley geometry
// 3. Wind vector rotation toward valley axis
//
// References:
// - Whiteman, C.D. (2000). Mountain Meteorology: Fundamentals and Applications.
//   Oxford University Press.
// - Zardi, D., & Whiteman, C.D. (2013). Diurnal mountain wind systems. In
//   Mountain Weather Research and Forecasting (pp. 35-119). Springer.
// - Rampanelli, G., Zardi, D., & Rotunno, R. (2004). Mechanisms of up-valley
//   winds. Journal of the Atmospheric Sciences, 61(24), 3097-3111.
// ============================================================================

#ifndef VALLEY_CHANNELING_MODELS_H
#define VALLEY_CHANNELING_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <algorithm>
#include <cmath>

namespace {
    using namespace amrex;
}

// ============================================================================
// Valley channeling parameters structure
// ============================================================================
struct ValleyChannelingParams {
    bool enabled = false;                           // Enable valley channeling
    amrex::Real valley_axis_angle = 0.0;            // Valley axis angle [radians] (CCW from x-axis)
    amrex::Real valley_width = 1000.0;              // Valley width [m]
    amrex::Real valley_depth = 300.0;               // Valley depth [m]
    amrex::Real channeling_strength_max = 0.8;      // Maximum channeling strength (0-1)
    amrex::Real speedup_factor_narrow = 1.3;        // Speed-up for narrow valleys (< 500m)
    amrex::Real slowdown_factor_wide = 0.85;        // Slowdown for wide valleys (> 2000m)
};

// Physical constants for valley channeling
namespace ValleyChannelingConstants {
    // Maximum alignment angle deviation from valley axis (±30° typical)
    constexpr amrex::Real MAX_ALIGNMENT_DEVIATION_DEG = amrex::Real(30.0);
    
    // Valley width thresholds for speed modification
    constexpr amrex::Real NARROW_VALLEY_WIDTH = amrex::Real(500.0);   // [m]
    constexpr amrex::Real WIDE_VALLEY_WIDTH = amrex::Real(2000.0);    // [m]
    
    // Minimum valley depth for channeling effects [m]
    constexpr amrex::Real MIN_VALLEY_DEPTH = amrex::Real(50.0);
}

// ============================================================================
// Detect valley orientation from terrain slope and curvature
//
// Analyzes terrain to identify valley axis direction using ridge-line detection.
// The valley axis is perpendicular to the cross-valley gradient direction.
//
// Method:
// 1. Compute terrain gradients (dz/dx, dz/dy) in neighborhood
// 2. Identify ridge directions (positive curvature peaks)
// 3. Valley axis is perpendicular to ridge-line direction
//
// Inputs:
//   z_center       - Terrain elevation at center point [m]
//   z_xm, z_xp     - Terrain elevations at x-minus and x-plus neighbors [m]
//   z_ym, z_yp     - Terrain elevations at y-minus and y-plus neighbors [m]
//   dx, dy         - Grid spacings [m]
//
// Returns:
//   Valley axis angle [radians] (counter-clockwise from x-axis)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real detect_valley_axis(
    amrex::Real z_center,
    amrex::Real z_xm, amrex::Real z_xp,
    amrex::Real z_ym, amrex::Real z_yp,
    amrex::Real dx, amrex::Real dy)
{
    // Central difference gradients
    const amrex::Real dz_dx = (z_xp - z_xm) / (amrex::Real(2.0) * dx);
    const amrex::Real dz_dy = (z_yp - z_ym) / (amrex::Real(2.0) * dy);
    
    // Cross-valley gradient direction (points from low to high terrain)
    // atan2(y, x) returns angle in range [-π, π]
    const amrex::Real cross_valley_angle = std::atan2(dz_dy, dz_dx);
    
    // Valley axis is perpendicular to cross-valley gradient
    // Rotate by π/2 to get along-valley direction
    amrex::Real valley_axis_angle = cross_valley_angle + amrex::Real(M_PI / 2.0);
    
    // Normalize to [0, 2π)
    if (valley_axis_angle < amrex::Real(0.0)) {
        valley_axis_angle += amrex::Real(2.0 * M_PI);
    } else if (valley_axis_angle >= amrex::Real(2.0 * M_PI)) {
        valley_axis_angle -= amrex::Real(2.0 * M_PI);
    }
    
    return valley_axis_angle;
}

// ============================================================================
// Compute valley width from terrain cross-section
//
// Estimates valley width by analyzing terrain elevation profile perpendicular
// to the valley axis.
//
// Method:
// 1. Find terrain peaks (ridges) on both sides of valley
// 2. Measure horizontal distance between peaks
//
// Simplified implementation: uses terrain variability as proxy
//
// Inputs:
//   z_center       - Terrain elevation at center point [m]
//   z_xm, z_xp     - Terrain elevations at x-minus and x-plus neighbors [m]
//   z_ym, z_yp     - Terrain elevations at y-minus and y-plus neighbors [m]
//   dx, dy         - Grid spacings [m]
//
// Returns:
//   Estimated valley width [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real estimate_valley_width(
    amrex::Real z_center,
    amrex::Real z_xm, amrex::Real z_xp,
    amrex::Real z_ym, amrex::Real z_yp,
    amrex::Real dx, amrex::Real dy)
{
    // Simplified estimation: use local terrain variability
    // In practice, this would analyze a wider cross-section
    
    // Terrain range in neighborhood
    const amrex::Real z_max = std::max({z_center, z_xm, z_xp, z_ym, z_yp});
    const amrex::Real z_min = std::min({z_center, z_xm, z_xp, z_ym, z_yp});
    const amrex::Real terrain_range = z_max - z_min;
    
    // Empirical relationship: wider valleys for gentler slopes
    // Typical values: 500-3000m
    const amrex::Real slope_factor = terrain_range / std::sqrt(dx * dx + dy * dy);
    
    // Valley width estimate (empirical formula)
    amrex::Real width = amrex::Real(1000.0);  // Default 1 km
    if (slope_factor > amrex::Real(0.1)) {
        // Steep terrain → narrow valley
        width = amrex::Real(500.0) / slope_factor;
    } else {
        // Gentle terrain → wide valley
        width = amrex::Real(2000.0);
    }
    
    // Clamp to reasonable range
    width = std::max(amrex::Real(200.0), std::min(width, amrex::Real(5000.0)));
    
    return width;
}

// ============================================================================
// Compute valley depth from terrain elevation range
//
// Estimates valley depth as the vertical difference between valley floor
// and surrounding ridge tops.
//
// Inputs:
//   z_center       - Terrain elevation at center point (assumed valley floor) [m]
//   z_xm, z_xp     - Terrain elevations at x-minus and x-plus neighbors [m]
//   z_ym, z_yp     - Terrain elevations at y-minus and y-plus neighbors [m]
//
// Returns:
//   Estimated valley depth [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real estimate_valley_depth(
    amrex::Real z_center,
    amrex::Real z_xm, amrex::Real z_xp,
    amrex::Real z_ym, amrex::Real z_yp)
{
    // Valley depth = difference between local elevation and surrounding peaks
    const amrex::Real z_max = std::max({z_xm, z_xp, z_ym, z_yp});
    
    // Depth is positive if center is lower than surroundings
    const amrex::Real depth = z_max - z_center;
    
    // Ensure depth is positive
    return std::max(amrex::Real(0.0), depth);
}

// ============================================================================
// Compute channeling strength based on valley geometry and wind direction
//
// The channeling strength determines how much the wind is forced to align
// with the valley axis. Depends on:
// 1. Valley depth (deeper valleys → stronger channeling)
// 2. Valley width (narrower valleys → stronger channeling)
// 3. Angle between synoptic wind and valley axis (larger angle → weaker channeling)
//
// Formula:
//   C = C_max × f_depth × f_width × f_angle
//
// where:
//   f_depth = tanh(depth / 200m)        - depth factor
//   f_width = exp(-width / 2000m)       - width factor
//   f_angle = cos²(θ_diff)              - angle factor
//
// Inputs:
//   valley_depth       - Valley depth [m]
//   valley_width       - Valley width [m]
//   synoptic_angle     - Synoptic wind direction [radians]
//   valley_axis_angle  - Valley axis direction [radians]
//   C_max              - Maximum channeling strength (0-1)
//
// Returns:
//   Channeling strength C ∈ [0, 1]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_channeling_strength(
    amrex::Real valley_depth,
    amrex::Real valley_width,
    amrex::Real synoptic_angle,
    amrex::Real valley_axis_angle,
    amrex::Real C_max)
{
    // Depth factor: deeper valleys → stronger channeling
    const amrex::Real depth_scale = amrex::Real(200.0);  // [m]
    const amrex::Real f_depth = std::tanh(valley_depth / depth_scale);
    
    // Width factor: narrower valleys → stronger channeling
    const amrex::Real width_scale = amrex::Real(2000.0);  // [m]
    const amrex::Real f_width = std::exp(-valley_width / width_scale);
    
    // Angle difference between synoptic wind and valley axis
    amrex::Real angle_diff = synoptic_angle - valley_axis_angle;
    
    // Normalize angle difference to [-π, π]
    if (angle_diff > amrex::Real(M_PI)) {
        angle_diff -= amrex::Real(2.0 * M_PI);
    } else if (angle_diff < amrex::Real(-M_PI)) {
        angle_diff += amrex::Real(2.0 * M_PI);
    }
    
    // Angle factor: wind aligned with valley → stronger channeling
    const amrex::Real cos_angle = std::cos(angle_diff);
    const amrex::Real f_angle = cos_angle * cos_angle;
    
    // Combined channeling strength
    const amrex::Real C = C_max * f_depth * f_width * f_angle;
    
    // Clamp to [0, 1]
    return std::max(amrex::Real(0.0), std::min(C, amrex::Real(1.0)));
}

// ============================================================================
// Apply valley channeling to wind vector
//
// Rotates wind direction toward valley axis and adjusts speed based on
// valley geometry.
//
// Rotation formula:
//   θ_new = (1 - C) × θ_synoptic + C × θ_valley
//
// Speed adjustment:
//   - Narrow valleys (W < 500m): speed up by venturi effect
//   - Wide valleys (W > 2000m): slow down by friction
//   - Medium valleys: no speed adjustment
//
// Inputs:
//   u, v              - Wind velocity components [m/s]
//   params            - Valley channeling parameters
//   channeling_strength - Computed channeling strength (0-1)
//
// Outputs:
//   u, v              - Modified wind velocity components [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_valley_channeling(
    amrex::Real& u,
    amrex::Real& v,
    const ValleyChannelingParams& params,
    amrex::Real channeling_strength)
{
    // Current wind speed and direction
    const amrex::Real speed = std::sqrt(u * u + v * v);
    
    if (speed < amrex::Real(1.0e-10)) {
        return;  // No wind, no channeling
    }
    
    // Current wind direction [radians]
    const amrex::Real wind_angle = std::atan2(v, u);
    
    // Rotate toward valley axis
    // θ_new = (1 - C) × θ_wind + C × θ_valley
    const amrex::Real angle_diff = params.valley_axis_angle - wind_angle;
    
    // Handle angle wrapping
    amrex::Real angle_correction = angle_diff;
    if (angle_correction > amrex::Real(M_PI)) {
        angle_correction -= amrex::Real(2.0 * M_PI);
    } else if (angle_correction < amrex::Real(-M_PI)) {
        angle_correction += amrex::Real(2.0 * M_PI);
    }
    
    const amrex::Real new_angle = wind_angle + channeling_strength * angle_correction;
    
    // Speed adjustment based on valley width
    amrex::Real speed_factor = amrex::Real(1.0);
    
    if (params.valley_width < ValleyChannelingConstants::NARROW_VALLEY_WIDTH) {
        // Narrow valley: venturi effect (speed up)
        speed_factor = params.speedup_factor_narrow;
    } else if (params.valley_width > ValleyChannelingConstants::WIDE_VALLEY_WIDTH) {
        // Wide valley: friction (slow down)
        speed_factor = params.slowdown_factor_wide;
    }
    
    // Apply channeling: blend speed factor based on strength
    const amrex::Real adjusted_speed = speed * (amrex::Real(1.0) - channeling_strength + 
                                                  channeling_strength * speed_factor);
    
    // Compute new wind components
    u = adjusted_speed * std::cos(new_angle);
    v = adjusted_speed * std::sin(new_angle);
}

// ============================================================================
// Compute valley channeling with automatic valley detection
//
// High-level function that:
// 1. Detects valley orientation from terrain
// 2. Estimates valley width and depth
// 3. Computes channeling strength
// 4. Applies channeling to wind vector
//
// Inputs:
//   u, v              - Wind velocity components [m/s]
//   z_terrain         - Terrain elevations (center and neighbors) [m]
//   dx, dy            - Grid spacings [m]
//   params            - Valley channeling parameters
//
// Outputs:
//   u, v              - Modified wind velocity components [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_valley_channeling(
    amrex::Real& u,
    amrex::Real& v,
    amrex::Real z_center,
    amrex::Real z_xm, amrex::Real z_xp,
    amrex::Real z_ym, amrex::Real z_yp,
    amrex::Real dx, amrex::Real dy,
    const ValleyChannelingParams& params)
{
    // Estimate valley properties from terrain
    const amrex::Real valley_depth = estimate_valley_depth(
        z_center, z_xm, z_xp, z_ym, z_yp);
    
    // Check if valley is deep enough for channeling
    if (valley_depth < ValleyChannelingConstants::MIN_VALLEY_DEPTH) {
        return;  // Not a significant valley
    }
    
    const amrex::Real valley_width = estimate_valley_width(
        z_center, z_xm, z_xp, z_ym, z_yp, dx, dy);
    
    const amrex::Real valley_axis_angle = detect_valley_axis(
        z_center, z_xm, z_xp, z_ym, z_yp, dx, dy);
    
    // Current wind direction
    const amrex::Real speed = std::sqrt(u * u + v * v);
    if (speed < amrex::Real(1.0e-10)) {
        return;  // No wind
    }
    const amrex::Real synoptic_angle = std::atan2(v, u);
    
    // Compute channeling strength
    const amrex::Real C = compute_channeling_strength(
        valley_depth, valley_width, synoptic_angle, valley_axis_angle,
        params.channeling_strength_max);
    
    // Apply channeling with detected valley properties
    ValleyChannelingParams local_params = params;
    local_params.valley_axis_angle = valley_axis_angle;
    local_params.valley_width = valley_width;
    local_params.valley_depth = valley_depth;
    
    apply_valley_channeling(u, v, local_params, C);
}

#endif // VALLEY_CHANNELING_MODELS_H
