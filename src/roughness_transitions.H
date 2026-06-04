#ifndef ROUGHNESS_TRANSITIONS_H
#define ROUGHNESS_TRANSITIONS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <algorithm>
#include <cmath>

namespace RoughnessTransitions {

using amrex::Real;

// Compute internal boundary layer height based on fetch distance
// Uses empirical formula from Elliott (1958) and Garratt (1990)
// for transition from upstream roughness z01 to downstream roughness z02
//
// Parameters:
//   fetch      - Distance from roughness change [m]
//   z01        - Upstream roughness length [m]
//   z02        - Downstream roughness length [m]
//   blend_height - Blending height scale [m] (controls transition rate)
//
// Returns: Internal boundary layer height [m]
//
// The internal boundary layer (IBL) is the region where the flow adjusts
// to the new surface roughness. Above the IBL, the flow still "remembers"
// the upstream surface characteristics.
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real internal_boundary_layer_height(Real fetch,
                                    Real z01,
                                    Real z02,
                                    Real blend_height) noexcept
{
    // Empirical formula: h_IBL ≈ blend_height * (fetch / L_fetch)^0.8
    // where L_fetch is a characteristic length scale
    // For simplicity, use blend_height as the asymptotic IBL height
    
    if (fetch < Real(1.0)) {
        return Real(0.0);  // No IBL development yet
    }
    
    // Roughness ratio influences IBL growth rate
    Real roughness_ratio = z02 / std::max(z01, Real(1.0e-10));
    Real growth_factor = std::log(std::max(roughness_ratio, Real(0.1)));
    
    // IBL height grows as power law with fetch
    Real h_ibl = blend_height * std::pow(fetch / blend_height, Real(0.8));
    h_ibl *= std::abs(growth_factor) / (Real(1.0) + std::abs(growth_factor));
    
    // Cap at blend_height
    return std::min(h_ibl, blend_height);
}

// Blend roughness lengths based on fetch and height
// Returns effective roughness length that transitions from upwind to local value
//
// Parameters:
//   z_agl      - Height above ground [m]
//   z0_upwind  - Upwind/upstream roughness length [m]
//   z0_local   - Local roughness length [m]
//   fetch      - Distance from roughness change [m]
//   blend_height - Blending height scale [m]
//
// Returns: Blended roughness length [m]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real blend_roughness_fetch(Real z_agl,
                           Real z0_upwind,
                           Real z0_local,
                           Real fetch,
                           Real blend_height) noexcept
{
    // Compute IBL height
    Real h_ibl = internal_boundary_layer_height(fetch, z0_upwind, z0_local, blend_height);
    
    // Below IBL: blend from upwind to local based on height ratio
    // Above IBL: use upwind roughness
    if (z_agl >= h_ibl) {
        return z0_upwind;  // Above IBL, use upwind value
    } else {
        // Below IBL: smooth transition from local (at surface) to upwind (at IBL top)
        Real blend_factor = z_agl / std::max(h_ibl, Real(1.0));
        blend_factor = Real(1.0) - blend_factor;  // 1 at surface, 0 at IBL top
        
        // Logarithmic blending (roughness lengths combine logarithmically)
        Real log_z0_blend = blend_factor * std::log(z0_local) + 
                            (Real(1.0) - blend_factor) * std::log(std::max(z0_upwind, Real(1.0e-10)));
        return std::exp(log_z0_blend);
    }
}

} // namespace RoughnessTransitions

#endif // ROUGHNESS_TRANSITIONS_H
