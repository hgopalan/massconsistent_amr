// ==========================================================================
// surface_layer_transition.H
// Smooth surface-layer-to-mixed-layer transition function
//
// Provides smooth interpolation between log-law (surface layer) and
// constant-stress (mixed layer) wind profiles to avoid discontinuities
// at the boundary layer depth.
//
// The transition function blends between:
//   u_surface = (u*/κ) * ln((z + z0)/z0)        [surface layer, z < H_transition]
//   u_mixed = U_ref * (z/H_BL)                   [mixed layer, z > H_transition]
//
// ==========================================================================

#ifndef SURFACE_LAYER_TRANSITION_H
#define SURFACE_LAYER_TRANSITION_H

#include <AMReX.H>
#include <AMReX_REAL.H>
#include <cmath>

namespace surface_layer_transition {

// Physical constants
static constexpr amrex::Real VON_KARMAN = amrex::Real(0.4);
static constexpr amrex::Real GRAV = amrex::Real(9.81);

// =========================================================================
// Compute smooth transition weight function
// Returns 0 in surface layer (z < z_transition - h_blend)
// Returns 1 in mixed layer (z > z_transition + h_blend)
// Uses smooth s-curve in transition zone
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real smooth_transition_weight(
    amrex::Real z_agl,
    amrex::Real z_transition,
    amrex::Real transition_height_scale) noexcept
{
    const amrex::Real z_lower = z_transition - transition_height_scale;
    const amrex::Real z_upper = z_transition + transition_height_scale;

    if (z_agl <= z_lower) {
        return amrex::Real(0.0);  // Full surface layer
    } else if (z_agl >= z_upper) {
        return amrex::Real(1.0);  // Full mixed layer
    } else {
        // Smooth s-curve transition: 3t² - 2t³ (smoothstep)
        amrex::Real t = (z_agl - z_lower) / (amrex::Real(2.0) * transition_height_scale);
        return amrex::Real(3.0) * t * t - amrex::Real(2.0) * t * t * t;
    }
}

// =========================================================================
// Compute log-law wind profile (surface layer)
// u(z) = (u*/κ) * ln((z + z0)/z0)
// where u* = κ * U_ref / ln((z_ref + z0)/z0)
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real loglaw_profile(
    amrex::Real z_agl,
    amrex::Real z0,
    amrex::Real u_star) noexcept
{
    if (z_agl <= 0.0) return amrex::Real(0.0);
    
    amrex::Real arg = (z_agl + z0) / std::max(z0, amrex::Real(1.0e-10));
    return (u_star / VON_KARMAN) * std::log(std::max(arg, amrex::Real(1.0)));
}

// =========================================================================
// Compute mixed-layer wind profile (constant-stress layer above PBL)
// u(z) = U_ref * (z / z_ref) ^ exponent
// Exponential decay above boundary layer depth
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real mixed_layer_profile(
    amrex::Real z_agl,
    amrex::Real z_transition,
    amrex::Real u_transition,
    amrex::Real bl_depth,
    amrex::Real decay_scale) noexcept
{
    if (z_agl <= z_transition) {
        return u_transition;
    }

    // Above transition: exponential decay with height
    amrex::Real decay_factor = std::exp(-(z_agl - z_transition) / std::max(decay_scale, amrex::Real(1.0)));
    return u_transition * decay_factor;
}

// =========================================================================
// Compute smooth blended wind profile combining surface and mixed layers
// u_blended(z) = (1 - w) * u_loglaw(z) + w * u_mixed(z)
// where w is the smooth transition weight
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real blended_wind_profile(
    amrex::Real z_agl,
    amrex::Real z0,
    amrex::Real u_star,
    amrex::Real z_transition,
    amrex::Real bl_depth,
    amrex::Real decay_scale,
    amrex::Real transition_height_scale) noexcept
{
    // Compute surface layer profile
    amrex::Real u_surface = loglaw_profile(z_agl, z0, u_star);
    
    // Velocity at transition height (for consistency)
    amrex::Real u_at_transition = loglaw_profile(z_transition, z0, u_star);
    
    // Compute mixed layer profile
    amrex::Real u_mixed = mixed_layer_profile(z_agl, z_transition, u_at_transition, bl_depth, decay_scale);
    
    // Compute smooth weight
    amrex::Real weight = smooth_transition_weight(z_agl, z_transition, transition_height_scale);
    
    // Blend
    return (amrex::Real(1.0) - weight) * u_surface + weight * u_mixed;
}

// =========================================================================
// Compute vertical wind shear with smooth transition
// du/dz = d/dz[(1-w)*u_surface + w*u_mixed]
//       = (1-w)*du_surface/dz + w*du_mixed/dz - dw/dz*(u_surface - u_mixed)
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real blended_vertical_shear(
    amrex::Real z_agl,
    amrex::Real z0,
    amrex::Real u_star,
    amrex::Real z_transition,
    amrex::Real bl_depth,
    amrex::Real decay_scale,
    amrex::Real transition_height_scale,
    amrex::Real dz_numerical) noexcept
{
    // Compute velocity at z + dz/2
    amrex::Real u_plus = blended_wind_profile(z_agl + dz_numerical / amrex::Real(2.0),
                                              z0, u_star, z_transition, bl_depth, 
                                              decay_scale, transition_height_scale);
    
    // Compute velocity at z - dz/2
    amrex::Real u_minus = blended_wind_profile(z_agl - dz_numerical / amrex::Real(2.0),
                                               z0, u_star, z_transition, bl_depth,
                                               decay_scale, transition_height_scale);
    
    // Central difference
    return (u_plus - u_minus) / dz_numerical;
}

// =========================================================================
// Compute Richardson number for stability assessment
// Ri = (g/θ) * (dθ/dz) / [(du/dz)² + (dv/dz)²]
// Used to determine transition zone stability
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real richardson_number_estimate(
    amrex::Real dtheta_dz,
    amrex::Real du_dz,
    amrex::Real dv_dz,
    amrex::Real theta_ref) noexcept
{
    amrex::Real shear_sq = du_dz * du_dz + dv_dz * dv_dz;
    
    if (std::abs(dtheta_dz) < amrex::Real(1.0e-10) || shear_sq < amrex::Real(1.0e-10)) {
        return amrex::Real(0.0);  // Neutral or very weak gradients
    }
    
    return (GRAV / theta_ref) * dtheta_dz / shear_sq;
}

// =========================================================================
// Compute adaptive transition height based on stability
// Stable conditions: transition height decreases (sharper layer separation)
// Unstable conditions: transition height increases (more gradual blending)
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real adaptive_transition_height(
    amrex::Real z_transition_base,
    amrex::Real richardson_number,
    amrex::Real richardson_critical) noexcept
{
    // Adjust transition based on stability
    // Ri > Ri_critical: strong stable stratification, narrow transition
    // Ri < -Ri_critical: strong instability, wide transition
    
    amrex::Real ri_ratio = richardson_number / std::max(richardson_critical, amrex::Real(1.0e-6));
    amrex::Real stability_factor = amrex::Real(1.0) / (amrex::Real(1.0) + ri_ratio);
    
    // Vary transition height: ±50% around base value
    return z_transition_base * (amrex::Real(0.5) + amrex::Real(1.5) * stability_factor);
}

// =========================================================================
// Compute effective roughness length considering transition zone
// In transition zone, effective z0 increases due to wind profile blending
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real effective_roughness_with_transition(
    amrex::Real z0_base,
    amrex::Real z_agl,
    amrex::Real z_transition,
    amrex::Real transition_height_scale) noexcept
{
    // Weight from smooth transition function
    amrex::Real weight = smooth_transition_weight(z_agl, z_transition, transition_height_scale);
    
    // Effective z0 increases slightly in transition zone (accounting for profile blending)
    amrex::Real z0_transition = z0_base * (amrex::Real(1.0) + amrex::Real(0.2) * weight);
    
    return z0_transition;
}

} // namespace surface_layer_transition

#endif // SURFACE_LAYER_TRANSITION_H
