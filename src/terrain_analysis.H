// ==========================================================================
// terrain_analysis.H
// Multi-scale terrain classification and adaptive parameterization
//
// Classifies terrain into categories (flat, moderate, steep) based on
// local slope and curvature, then applies region-appropriate parameterization
// sets for roughness, stability, and orographic effects.
//
// Terrain Types:
//   0 = Flat (|∇h| < slope_threshold_moderate)
//   1 = Moderate (slope_threshold_moderate <= |∇h| < slope_threshold_steep)
//   2 = Steep (|∇h| >= slope_threshold_steep)
//
// ==========================================================================

#ifndef TERRAIN_ANALYSIS_H
#define TERRAIN_ANALYSIS_H

#include <AMReX.H>
#include <AMReX_REAL.H>
#include <AMReX_Box.H>
#include <AMReX_FArrayBox.H>
#include <cmath>

namespace terrain_analysis {

// =========================================================================
// Compute terrain slope (magnitude of gradient) at cell (i,j)
// |∇h| = sqrt((∂h/∂x)² + (∂h/∂y)²)
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_terrain_slope(
    int i, int j,
    const amrex::FArrayBox& terrain_fab,
    amrex::Real dx,
    amrex::Real dy) noexcept
{
    const amrex::Real inv_2dx = amrex::Real(1.0) / (amrex::Real(2.0) * dx);
    const amrex::Real inv_2dy = amrex::Real(1.0) / (amrex::Real(2.0) * dy);

    // Centered differences for terrain height gradient
    amrex::Real dh_dx = (terrain_fab(i+1, j) - terrain_fab(i-1, j)) * inv_2dx;
    amrex::Real dh_dy = (terrain_fab(i, j+1) - terrain_fab(i, j-1)) * inv_2dy;

    return std::sqrt(dh_dx * dh_dx + dh_dy * dh_dy);
}

// =========================================================================
// Compute terrain curvature (Laplacian of terrain height)
// ∇²h = ∂²h/∂x² + ∂²h/∂y²
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_terrain_curvature(
    int i, int j,
    const amrex::FArrayBox& terrain_fab,
    amrex::Real dx,
    amrex::Real dy) noexcept
{
    const amrex::Real inv_dx2 = amrex::Real(1.0) / (dx * dx);
    const amrex::Real inv_dy2 = amrex::Real(1.0) / (dy * dy);

    // Second derivatives
    amrex::Real d2h_dx2 = (terrain_fab(i+1, j) - amrex::Real(2.0) * terrain_fab(i, j) + terrain_fab(i-1, j)) * inv_dx2;
    amrex::Real d2h_dy2 = (terrain_fab(i, j+1) - amrex::Real(2.0) * terrain_fab(i, j) + terrain_fab(i, j-1)) * inv_dy2;

    return d2h_dx2 + d2h_dy2;
}

// =========================================================================
// Compute aspect ratio of terrain (direction of maximum slope)
// Returns angle in radians: atan2(∂h/∂y, ∂h/∂x)
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_aspect_ratio(
    int i, int j,
    const amrex::FArrayBox& terrain_fab,
    amrex::Real dx,
    amrex::Real dy) noexcept
{
    const amrex::Real inv_2dx = amrex::Real(1.0) / (amrex::Real(2.0) * dx);
    const amrex::Real inv_2dy = amrex::Real(1.0) / (amrex::Real(2.0) * dy);

    amrex::Real dh_dx = (terrain_fab(i+1, j) - terrain_fab(i-1, j)) * inv_2dx;
    amrex::Real dh_dy = (terrain_fab(i, j+1) - terrain_fab(i, j-1)) * inv_2dy;

    return std::atan2(dh_dy, dh_dx);
}

// =========================================================================
// Classify terrain type (0=flat, 1=moderate, 2=steep)
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
int classify_terrain_type(
    amrex::Real slope,
    amrex::Real slope_threshold_moderate,
    amrex::Real slope_threshold_steep) noexcept
{
    if (slope < slope_threshold_moderate) {
        return 0;  // Flat
    } else if (slope < slope_threshold_steep) {
        return 1;  // Moderate
    } else {
        return 2;  // Steep
    }
}

// =========================================================================
// Get adaptive roughness length based on terrain type
// Flat terrain: use base z0
// Moderate terrain: increase z0 by ~10-30%
// Steep terrain: increase z0 by ~50-100% (more effective drag)
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_adaptive_roughness(
    int terrain_type,
    amrex::Real z0_base,
    amrex::Real roughness_factor_moderate,
    amrex::Real roughness_factor_steep) noexcept
{
    switch (terrain_type) {
        case 0:  // Flat
            return z0_base;
        case 1:  // Moderate
            return z0_base * (amrex::Real(1.0) + roughness_factor_moderate);
        case 2:  // Steep
            return z0_base * (amrex::Real(1.0) + roughness_factor_steep);
        default:
            return z0_base;
    }
}

// =========================================================================
// Get adaptive stability parameter based on terrain type
// Steep terrain: enhanced stability correction
// Moderate terrain: neutral baseline
// Flat terrain: minimal correction
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_adaptive_stability_scaling(
    int terrain_type,
    amrex::Real stability_scale_flat,
    amrex::Real stability_scale_moderate,
    amrex::Real stability_scale_steep) noexcept
{
    switch (terrain_type) {
        case 0:  // Flat
            return stability_scale_flat;
        case 1:  // Moderate
            return stability_scale_moderate;
        case 2:  // Steep
            return stability_scale_steep;
        default:
            return amrex::Real(1.0);
    }
}

// =========================================================================
// Smooth transition function for terrain-adaptive parameterization
// Avoids discontinuities at terrain type boundaries
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real smooth_terrain_transition(
    amrex::Real slope,
    amrex::Real slope_threshold_moderate,
    amrex::Real slope_threshold_steep,
    amrex::Real transition_width) noexcept
{
    // Smooth step function: s-curve transition
    // 0 at slope = threshold - transition_width
    // 1 at slope = threshold + transition_width
    
    if (slope < slope_threshold_moderate - transition_width) {
        return amrex::Real(0.0);
    } else if (slope > slope_threshold_moderate + transition_width) {
        return amrex::Real(1.0);
    } else {
        // S-curve: 3t² - 2t³ (smoothstep)
        amrex::Real t = (slope - (slope_threshold_moderate - transition_width)) / 
                        (amrex::Real(2.0) * transition_width);
        return amrex::Real(3.0) * t * t - amrex::Real(2.0) * t * t * t;
    }
}

// =========================================================================
// Compute region-weighted parameter for adaptive zones
// Blends between flat/moderate/steep parameterizations
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real blend_adaptive_parameter(
    amrex::Real slope,
    amrex::Real slope_threshold_moderate,
    amrex::Real slope_threshold_steep,
    amrex::Real param_flat,
    amrex::Real param_moderate,
    amrex::Real param_steep,
    amrex::Real transition_width) noexcept
{
    // Weight 1: transition from flat to moderate
    amrex::Real w_moderate = smooth_terrain_transition(
        slope, slope_threshold_moderate, slope_threshold_moderate + transition_width, 
        transition_width / amrex::Real(2.0));

    // Weight 2: transition from moderate to steep
    amrex::Real w_steep = smooth_terrain_transition(
        slope, slope_threshold_steep, slope_threshold_steep + transition_width,
        transition_width / amrex::Real(2.0));

    // Blend: flat + (moderate-flat)*w_moderate + (steep-moderate)*w_steep
    return param_flat * (amrex::Real(1.0) - w_moderate) +
           param_moderate * w_moderate * (amrex::Real(1.0) - w_steep) +
           param_steep * w_steep;
}

// =========================================================================
// Compute multi-scale terrain roughness (sum of variances at multiple scales)
// Used for effective drag representation
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_terrain_roughness_index(
    amrex::Real slope_local,
    amrex::Real curvature_local,
    amrex::Real aspect_angle) noexcept
{
    // Combined roughness index combining slope and curvature
    // Higher values for more complex terrain
    amrex::Real roughness = slope_local + amrex::Real(0.1) * std::abs(curvature_local);
    
    return roughness;
}

} // namespace terrain_analysis

#endif // TERRAIN_ANALYSIS_H
