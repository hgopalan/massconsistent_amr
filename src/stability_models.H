// ==========================================================================
// stability_models.H
// Atmospheric stability corrections for non-neutral boundary layer
// Implements Businger-Dyer (1971) Monin-Obukhov similarity theory
// ==========================================================================

#ifndef STABILITY_MODELS_H
#define STABILITY_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include "math_constants.H"
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

// Holtslag-De Bruin (1988) momentum stability function for stable conditions
// Alternative formulation widely used in European models
// Better performance in very stable conditions (nighttime, polar regions)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real psi_m_holtslag_stable(amrex::Real zeta) noexcept
{
    // Holtslag-De Bruin (1988) formulation for stable conditions
    // psi_m = -(a*zeta + b*(zeta - c/d)*exp(-d*zeta) + b*c/d)
    // where a=1.0, b=0.667, c=5.0, d=0.35 are standard coefficients
    const amrex::Real a = amrex::Real(1.0);
    const amrex::Real b = amrex::Real(0.667);
    const amrex::Real c = amrex::Real(5.0);
    const amrex::Real d = amrex::Real(0.35);
    
    return -(a * zeta + b * (zeta - c / d) * std::exp(-d * zeta) + b * c / d);
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
    
    return amrex::Real(2.0) * std::log((amrex::Real(1.0) + x) / amrex::Real(2.0))
         + std::log((amrex::Real(1.0) + x2) / amrex::Real(2.0))
         - amrex::Real(2.0) * std::atan(x)
         + MathConstants::pi / amrex::Real(2.0);
}

// Combined stability function psi_m(zeta)
// Handles both stable and unstable conditions
// use_holtslag: if true, use Holtslag-De Bruin for stable; if false, use Businger-Dyer
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real psi_m(amrex::Real zeta, bool use_holtslag = false) noexcept
{
    // Limit zeta to prevent numerical issues
    const amrex::Real zeta_max = amrex::Real(2.0);   // stable limit
    const amrex::Real zeta_min = amrex::Real(-2.0);  // unstable limit
    zeta = (zeta > zeta_max) ? zeta_max : zeta;
    zeta = (zeta < zeta_min) ? zeta_min : zeta;
    
    if (zeta > amrex::Real(0.0)) {
        // Stable conditions - choose formulation
        return use_holtslag ? psi_m_holtslag_stable(zeta) : psi_m_stable(zeta);
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
// use_holtslag: if true, use Holtslag-De Bruin for stable; if false, use Businger-Dyer
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real wind_profile_stability(amrex::Real z_agl,
                                    amrex::Real z0,
                                    amrex::Real ustar,
                                    amrex::Real kappa,
                                    amrex::Real L_obukhov,
                                    bool use_holtslag = false) noexcept
{
    // Neutral log-law component
    amrex::Real speed = (ustar / kappa) * std::log((z_agl + z0) / z0);
    
    // Apply stability correction if L is finite
    if (std::abs(L_obukhov) > amrex::Real(1.0e-10)) {
        amrex::Real zeta = z_agl / L_obukhov;
        amrex::Real zeta0 = z0 / L_obukhov;
        
        // Stability correction: subtract psi_m(z/L) and add psi_m(z0/L)
        // For z0 << L, psi_m(z0/L) ≈ 0, but we include it for completeness
        speed = (ustar / kappa) * (std::log((z_agl + z0) / z0) - psi_m(zeta, use_holtslag) + psi_m(zeta0, use_holtslag));
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

// ---------------------------------------------------------------------------
// Ekman spiral wind veer correction
// ---------------------------------------------------------------------------

// Compute wind veer angle (in radians) due to Ekman spiral effects
// Wind direction rotates with height due to Coriolis force and surface friction
//
// Parameters:
//   z_agl          - Height above ground level [m]
//   veer_height    - Height scale for veer profile [m]
//   total_veer     - Total veer from surface to top [radians]
//
// Returns: veer angle in radians (positive = clockwise rotation in NH)
//
// The veer profile uses an exponential form:
//   θ(z) = total_veer × [1 - exp(-z / veer_height)]
//
// This gives:
//   - Zero veer at surface (z=0)
//   - Asymptotic approach to total_veer at large z
//   - Most veer occurs in lowest veer_height meters
//
// Typical values:
//   - Mid-latitudes: total_veer = 15-30° (0.26-0.52 rad)
//   - veer_height = 100-200 m (boundary layer depth scale)
//
// References:
//   - Ekman, V.W. (1905). On the influence of the Earth's rotation
//   - Arya, S.P. (1988). Introduction to Micrometeorology, Ch. 9
//   - Stull, R.B. (1988). An Introduction to Boundary Layer Meteorology
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real ekman_veer_angle(amrex::Real z_agl,
                              amrex::Real veer_height,
                              amrex::Real total_veer) noexcept
{
    // Exponential veer profile
    // Approaches total_veer asymptotically with height
    // Most veer occurs within first veer_height meters
    if (veer_height > amrex::Real(1.0e-10)) {
        return total_veer * (amrex::Real(1.0) - std::exp(-z_agl / veer_height));
    } else {
        // Zero veer height means no veer
        return amrex::Real(0.0);
    }
}

// Apply Ekman veer rotation to wind components
// Rotates the horizontal wind vector by the veer angle
//
// Parameters:
//   u_in, v_in     - Input wind components [m/s]
//   veer_angle     - Veer angle in radians (from ekman_veer_angle)
//   u_out, v_out   - Output wind components [m/s]
//
// The rotation is applied as:
//   u_out = u_in × cos(θ) - v_in × sin(θ)
//   v_out = u_in × sin(θ) + v_in × cos(θ)
//
// This corresponds to a counter-clockwise rotation in the (u,v) plane.
// In the Northern Hemisphere, winds typically veer (rotate clockwise)
// with height, so positive veer_angle rotates the wind vector clockwise.
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void apply_ekman_veer(amrex::Real u_in,
                      amrex::Real v_in,
                      amrex::Real veer_angle,
                      amrex::Real& u_out,
                      amrex::Real& v_out) noexcept
{
    // Standard 2D rotation matrix
    // For clockwise rotation (Northern Hemisphere veer), use positive angle
    amrex::Real cos_theta = std::cos(veer_angle);
    amrex::Real sin_theta = std::sin(veer_angle);
    
    u_out = u_in * cos_theta - v_in * sin_theta;
    v_out = u_in * sin_theta + v_in * cos_theta;
}

// Compute wind direction gradient angle (linear directional shear with height)
// Simpler than Ekman spiral - uniform rate of direction change with height
//
// Parameters:
//   z_agl          - Height above ground level [m]
//   shear_rate     - Rate of direction change [rad/m]
//
// Returns: Direction change angle [rad]
//
// Unlike Ekman veer which uses exponential profile, this implements a simple
// linear directional shear: angle = shear_rate × z
// Useful for idealized wind direction gradients or observational studies
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real wind_direction_gradient_angle(amrex::Real z_agl,
                                          amrex::Real shear_rate) noexcept
{
    return shear_rate * z_agl;
}

// Compute Coriolis parameter f = 2 × Ω × sin(latitude)
// where Ω = 7.2921e-5 rad/s is Earth's angular velocity
//
// Parameters:
//   latitude_deg   - Latitude in degrees (positive = North)
//
// Returns: Coriolis parameter f [rad/s]
//
// Used for theoretical Ekman height calculation:
//   h_e = sqrt(2 × K_m / f)
// where K_m is eddy viscosity [m²/s]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real coriolis_parameter(amrex::Real latitude_deg) noexcept
{
    const amrex::Real omega = amrex::Real(7.2921e-5);  // Earth's angular velocity [rad/s]
    amrex::Real latitude_rad = latitude_deg * MathConstants::pi / amrex::Real(180.0);
    return amrex::Real(2.0) * omega * std::sin(latitude_rad);
}

#endif // STABILITY_MODELS_H
