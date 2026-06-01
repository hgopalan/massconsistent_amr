// ==========================================================================
// stability_models.H
// Atmospheric stability corrections for non-neutral boundary layer
// Implements Businger-Dyer (1971) Monin-Obukhov similarity theory
// ==========================================================================

#ifndef STABILITY_MODELS_H
#define STABILITY_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>

// ---------------------------------------------------------------------------
// Stability correction functions (Businger-Dyer profiles)
// ---------------------------------------------------------------------------

// Momentum stability function psi_m(zeta) for stable conditions (zeta > 0)
// zeta = z/L where L is Obukhov length
// Based on Businger et al. (1971) and Dyer (1974)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real psi_m_stable(amrex::Real zeta) noexcept
{
    // For stable conditions: psi_m = -beta * zeta
    // beta = 5.0 is standard value (Businger et al. 1971)
    const amrex::Real beta = amrex::Real(5.0);
    return -beta * zeta;
}

// Momentum stability function psi_m(zeta) for unstable conditions (zeta < 0)
// Based on Paulson (1970) and Businger et al. (1971)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real psi_m_unstable(amrex::Real zeta) noexcept
{
    // For unstable conditions: psi_m = 2*ln((1+x)/2) + ln((1+x^2)/2) - 2*arctan(x) + pi/2
    // where x = (1 - 16*zeta)^0.25
    const amrex::Real x = std::pow(amrex::Real(1.0) - amrex::Real(16.0) * zeta, amrex::Real(0.25));
    const amrex::Real x2 = x * x;
    const amrex::Real pi = amrex::Real(3.14159265358979323846);
    
    return amrex::Real(2.0) * std::log((amrex::Real(1.0) + x) / amrex::Real(2.0))
         + std::log((amrex::Real(1.0) + x2) / amrex::Real(2.0))
         - amrex::Real(2.0) * std::atan(x)
         + pi / amrex::Real(2.0);
}

// Combined stability function psi_m(zeta)
// Handles both stable and unstable conditions
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real psi_m(amrex::Real zeta) noexcept
{
    // Limit zeta to prevent numerical issues
    const amrex::Real zeta_max = amrex::Real(2.0);   // stable limit
    const amrex::Real zeta_min = amrex::Real(-2.0);  // unstable limit
    zeta = (zeta > zeta_max) ? zeta_max : zeta;
    zeta = (zeta < zeta_min) ? zeta_min : zeta;
    
    if (zeta > amrex::Real(0.0)) {
        // Stable conditions
        return psi_m_stable(zeta);
    } else if (zeta < amrex::Real(0.0)) {
        // Unstable conditions
        return psi_m_unstable(zeta);
    } else {
        // Neutral conditions
        return amrex::Real(0.0);
    }
}

// Non-neutral log-law wind speed profile with stability correction
// u(z) = (u*/kappa) * [ln((z+z0)/z0) - psi_m(z/L) + psi_m(z0/L)]
// For most applications, psi_m(z0/L) ≈ 0 since z0 << z
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real wind_profile_stability(amrex::Real z_agl,
                                    amrex::Real z0,
                                    amrex::Real ustar,
                                    amrex::Real kappa,
                                    amrex::Real L_obukhov) noexcept
{
    // Neutral log-law component
    amrex::Real speed = (ustar / kappa) * std::log((z_agl + z0) / z0);
    
    // Apply stability correction if L is finite
    if (std::abs(L_obukhov) > amrex::Real(1.0e-10)) {
        amrex::Real zeta = z_agl / L_obukhov;
        amrex::Real zeta0 = z0 / L_obukhov;
        
        // Stability correction: subtract psi_m(z/L) and add psi_m(z0/L)
        // For z0 << L, psi_m(z0/L) ≈ 0, but we include it for completeness
        speed = (ustar / kappa) * (std::log((z_agl + z0) / z0) - psi_m(zeta) + psi_m(zeta0));
    }
    
    return speed;
}

// Elevation-dependent wind speed scaling
// Adjusts reference wind speed based on terrain elevation
// Common approach: reduce wind speed with elevation increase for valley flows
// U_scaled = U_ref * exp(-alpha * z_terrain / H_scale)
// where alpha is scaling factor and H_scale is characteristic height scale
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real elevation_wind_scaling(amrex::Real U_ref,
                                    amrex::Real z_terrain,
                                    amrex::Real z_terrain_min,
                                    amrex::Real scaling_factor,
                                    amrex::Real height_scale) noexcept
{
    // Elevation difference from minimum terrain
    amrex::Real delta_z = z_terrain - z_terrain_min;
    
    // Exponential scaling (reduces wind at higher elevations)
    // scaling_factor > 0: wind decreases with elevation (valley effect)
    // scaling_factor < 0: wind increases with elevation (ridge effect)
    // scaling_factor = 0: no elevation dependence
    amrex::Real scale = std::exp(-scaling_factor * delta_z / height_scale);
    
    return U_ref * scale;
}

#endif // STABILITY_MODELS_H
