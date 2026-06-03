// ============================================================================
// boundary_layer_decay_models.H
// Exponential wind decay above the atmospheric boundary layer
//
// This header provides functions for computing wind speed reduction above
// the boundary layer depth, where shear stress becomes negligible and
// wind speed decays exponentially.
//
// Physical basis:
// - Within the PBL (z < z_BL): Wind speed varies logarithmically (log-law)
// - Above the PBL (z > z_BL): Wind speed decays exponentially toward geostrophic wind
// - Transition zone: Smooth exponential decay from log-law profile to free atmosphere
//
// Model: u(z) = u_ref·exp[-(z - z_BL) / H_decay]  for z > z_BL
//
// where:
//   u_ref = reference wind speed at z_BL [m/s]
//   z_BL = boundary layer depth [m]
//   H_decay = decay height scale [m] (typically 500-2000 m)
//   z = height above ground [m]
//
// References:
// - Stull, R.B. (1988). An Introduction to Boundary Layer Meteorology.
// - Holton, J.R. (2004). An Introduction to Dynamic Meteorology.
// ============================================================================

#ifndef BOUNDARY_LAYER_DECAY_MODELS_H
#define BOUNDARY_LAYER_DECAY_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <algorithm>

// ============================================================================
// Boundary layer decay parameters structure
// ============================================================================
struct BoundaryLayerDecayParams {
    bool enabled = false;                          // Enable BL decay above BL depth
    amrex::Real bl_depth = 1000.0;                 // Boundary layer depth z_BL [m]
    amrex::Real decay_height_scale = 1000.0;       // Decay height scale H_decay [m]
    amrex::Real transition_zone_height = 200.0;    // Smooth transition zone above z_BL [m]
};

// Physical constants for boundary layer decay
namespace BoundaryLayerDecayConstants {
    // Typical boundary layer depths [m]
    constexpr amrex::Real TYPICAL_BL_DEPTH_MIN = amrex::Real(500.0);    // Stable/nighttime
    constexpr amrex::Real TYPICAL_BL_DEPTH_MAX = amrex::Real(2500.0);   // Unstable/daytime
    
    // Typical decay height scales [m] above the BL
    constexpr amrex::Real TYPICAL_DECAY_HEIGHT_MIN = amrex::Real(500.0);
    constexpr amrex::Real TYPICAL_DECAY_HEIGHT_MAX = amrex::Real(2000.0);
    
    // Minimum decay height to prevent unrealistic decay [m]
    constexpr amrex::Real MIN_DECAY_HEIGHT = amrex::Real(100.0);
}

// ============================================================================
// Compute wind speed reduction above boundary layer
//
// Applies exponential decay to wind speed profile above the boundary layer depth.
// Within the BL (z < z_BL): wind speed is from log-law profile (unchanged).
// Above BL (z > z_BL): wind speed decays exponentially.
//
// u(z) = u_BL · exp[-(z - z_BL) / H_decay]    for z > z_BL
// u(z) = u_log (unchanged)                    for z < z_BL
//
// Smooth transition: Apply blending over transition_zone_height to avoid discontinuities
//
// Inputs:
//   u_log              - Wind speed from log-law profile [m/s]
//   z_agl              - Height above ground level [m]
//   params             - BoundaryLayerDecayParams structure
//
// Returns:
//   Wind speed with BL decay applied [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real apply_bl_exponential_decay(
    amrex::Real u_log,
    amrex::Real z_agl,
    const BoundaryLayerDecayParams& params)
{
    if (!params.enabled) {
        return u_log;  // No decay applied
    }
    
    // Below BL depth: use log-law profile as-is
    if (z_agl < params.bl_depth) {
        return u_log;
    }
    
    // Above BL depth: apply exponential decay
    const amrex::Real z_above_bl = z_agl - params.bl_depth;
    const amrex::Real decay_scale = std::max(params.decay_height_scale, 
                                             BoundaryLayerDecayConstants::MIN_DECAY_HEIGHT);
    
    // Compute exponential decay factor: exp(-(z-z_BL)/H_decay)
    const amrex::Real decay_factor = std::exp(-z_above_bl / decay_scale);
    
    // Apply decay to wind speed
    const amrex::Real u_decayed = u_log * decay_factor;
    
    return u_decayed;
}

// ============================================================================
// Compute the blending function for smooth transition around BL depth
//
// Provides a smooth transition from the log-law profile to the exponential
// decay profile in a transition zone around z_BL to avoid discontinuities.
//
// Blending function (smooth step):
//   f(z) = 0                    for z < z_BL - H_trans
//   f(z) = smooth transition    for z_BL - H_trans < z < z_BL + H_trans
//   f(z) = 1                    for z > z_BL + H_trans
//
// Inputs:
//   z_agl              - Height above ground level [m]
//   params             - BoundaryLayerDecayParams structure
//
// Returns:
//   Blending factor from 0 (log-law) to 1 (exponential decay) [dimensionless]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_bl_decay_blending_factor(
    amrex::Real z_agl,
    const BoundaryLayerDecayParams& params)
{
    if (!params.enabled) {
        return amrex::Real(0.0);  // No blending needed
    }
    
    const amrex::Real z_BL = params.bl_depth;
    const amrex::Real H_trans = params.transition_zone_height;
    
    // Below transition zone: fully log-law (blend = 0)
    if (z_agl < z_BL - H_trans) {
        return amrex::Real(0.0);
    }
    
    // Above transition zone: fully exponential decay (blend = 1)
    if (z_agl > z_BL + H_trans) {
        return amrex::Real(1.0);
    }
    
    // In transition zone: smooth cubic Hermite interpolation
    const amrex::Real xi = (z_agl - (z_BL - H_trans)) / (amrex::Real(2.0) * H_trans);
    
    // Smooth step function: 3*xi² - 2*xi³ (Hermite interpolation)
    const amrex::Real blend = amrex::Real(3.0) * xi * xi - amrex::Real(2.0) * xi * xi * xi;
    
    return std::max(amrex::Real(0.0), std::min(blend, amrex::Real(1.0)));
}

// ============================================================================
// Apply smoothly-blended BL exponential decay
//
// Combines log-law profile with exponential decay using smooth blending
// to avoid discontinuities.
//
// u(z) = (1 - f(z)) · u_log(z) + f(z) · u_exp(z)
//
// where f(z) is the blending factor from 0 to 1.
//
// Inputs:
//   u_log              - Wind speed from log-law profile [m/s]
//   z_agl              - Height above ground level [m]
//   params             - BoundaryLayerDecayParams structure
//
// Returns:
//   Wind speed with smooth BL decay applied [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real apply_bl_decay_smooth(
    amrex::Real u_log,
    amrex::Real z_agl,
    const BoundaryLayerDecayParams& params)
{
    if (!params.enabled) {
        return u_log;  // No decay applied
    }
    
    // Get blending factor
    const amrex::Real blend_factor = get_bl_decay_blending_factor(z_agl, params);
    
    // If not in transition zone, use simple exponential decay
    if (blend_factor >= amrex::Real(1.0) - amrex::Real(1.0e-10)) {
        return apply_bl_exponential_decay(u_log, z_agl, params);
    }
    
    // Apply smooth blending between log-law and exponential decay
    const amrex::Real u_decayed = apply_bl_exponential_decay(u_log, z_agl, params);
    const amrex::Real u_blended = (amrex::Real(1.0) - blend_factor) * u_log 
                                 + blend_factor * u_decayed;
    
    return u_blended;
}

// ============================================================================
// Compute boundary layer depth from external source or diagnostic
//
// This function can be extended to compute BL depth from:
// - Richardson number profile
// - Temperature inversion detection
// - TKE profile
// - Static input parameter
//
// For now, returns the configured depth parameter.
//
// Inputs:
//   params             - BoundaryLayerDecayParams structure
//
// Returns:
//   Boundary layer depth z_BL [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_boundary_layer_depth(const BoundaryLayerDecayParams& params)
{
    return params.bl_depth;
}

// ============================================================================
// Estimate decay height scale from PBL depth and wind shear
//
// The decay height scale H_decay is typically related to the BL depth
// and the vertical wind shear structure:
//
// H_decay ≈ (1-2) × z_BL
//
// This function provides a simple estimate based on the BL depth.
//
// Inputs:
//   bl_depth           - Boundary layer depth [m]
//
// Returns:
//   Estimated decay height scale [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real estimate_decay_height_scale(amrex::Real bl_depth)
{
    // Typical relationship: H_decay ≈ 1.0 × z_BL
    // Range: 0.8 - 1.5 × z_BL depending on stability
    const amrex::Real min_decay = bl_depth * amrex::Real(0.8);
    const amrex::Real max_decay = bl_depth * amrex::Real(1.5);
    
    // Return middle estimate
    return (min_decay + max_decay) / amrex::Real(2.0);
}

#endif // BOUNDARY_LAYER_DECAY_MODELS_H
