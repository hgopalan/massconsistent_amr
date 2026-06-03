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

// ---------------------------------------------------------------------------
// Pasquill-Gifford Stability Classes
// Classic atmospheric stability classification system (1960s)
// Maps wind speed and solar radiation to discrete stability classes A-F
// ---------------------------------------------------------------------------

// =========================================================================
// Phase 4: Bulk Richardson Number and Adaptive Stability Model Selection
//
// Computes bulk Richardson number (Ri_b) to determine atmospheric stability
// and automatically selects appropriate stability parameterization:
// - Weak stability (Ri_b < 0.1): Use Businger-Dyer (1971)
// - Very stable (Ri_b >= 0.1): Use Holtslag-De Bruin (1988)
//
// The bulk Richardson number is defined as:
//   Ri_b = (g/θ_ref) * Δθ * h / U_ref²
// 
// where:
//   g = gravitational acceleration [m/s²]
//   θ_ref = reference potential temperature [K]
//   Δθ = potential temperature difference between height h and surface [K]
//   h = height above ground [m]
//   U_ref = reference wind speed [m/s]
//
// References:
// - Holtslag, A.A.M., & De Bruin, H.A.R. (1988). Applied modeling of the
//   nighttime surface energy balance over land. J. Appl. Meteor., 27, 689-704.
// =========================================================================

// Compute bulk Richardson number (Ri_b) for adaptive stability model selection
//
// Inputs:
//   theta_surface      - Potential temperature at surface [K]
//   theta_height       - Potential temperature at height h [K]
//   height             - Height above ground [m]
//   wind_speed         - Wind speed magnitude [m/s]
//   theta_ref          - Reference potential temperature [K]
//
// Returns:
//   Bulk Richardson number (dimensionless)
//   Positive values indicate stable conditions
//   Negative values indicate unstable conditions
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_bulk_richardson_number(
    amrex::Real theta_surface,
    amrex::Real theta_height,
    amrex::Real height,
    amrex::Real wind_speed,
    amrex::Real theta_ref = amrex::Real(300.0)) noexcept
{
    // Ensure valid inputs
    const amrex::Real min_wind_speed = amrex::Real(0.1);
    wind_speed = std::max(wind_speed, min_wind_speed);
    
    if (height < amrex::Real(1.0e-6)) {
        return amrex::Real(0.0);  // No height, neutral
    }
    
    if (theta_ref < amrex::Real(1.0)) {
        theta_ref = amrex::Real(300.0);  // Default reference temperature
    }
    
    // Potential temperature difference
    const amrex::Real delta_theta = theta_height - theta_surface;
    
    // Gravitational acceleration [m/s²]
    constexpr amrex::Real g = amrex::Real(9.81);
    
    // Ri_b = (g/θ_ref) * Δθ * h / U²
    const amrex::Real wind_squared = wind_speed * wind_speed;
    const amrex::Real Ri_b = (g / theta_ref) * delta_theta * height / wind_squared;
    
    // Clamp to reasonable range (-2 to 10 typical)
    const amrex::Real Ri_b_min = amrex::Real(-2.0);
    const amrex::Real Ri_b_max = amrex::Real(10.0);
    
    return std::max(Ri_b_min, std::min(Ri_b, Ri_b_max));
}

// Adaptive stability model selection based on bulk Richardson number
//
// Selects between Businger-Dyer (weak stability) and Holtslag-De Bruin
// (very stable) parameterizations based on Ri_b threshold.
//
// Inputs:
//   Ri_b               - Bulk Richardson number (dimensionless)
//   Ri_b_threshold     - Threshold for model switching (typically 0.1)
//
// Returns:
//   true if Holtslag-De Bruin should be used, false if Businger-Dyer
//
// Thresholds:
//   Ri_b < 0.1:  Weak stability → Use Businger-Dyer (more flexible)
//   Ri_b >= 0.1: Very stable → Use Holtslag-De Bruin (stronger damping)
//
// Hysteresis (to prevent rapid switching):
//   - Use 5% margin around threshold for transition region
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
bool select_holtslag_for_stability(
    amrex::Real Ri_b,
    amrex::Real Ri_b_threshold = amrex::Real(0.1)) noexcept
{
    // Holtslag-De Bruin provides better performance for very stable conditions
    // (high Ri_b values typical of nighttime, polar regions, katabatic flows)
    return (Ri_b >= Ri_b_threshold);
}

// Compute adaptive stability correction using Ri_b-dependent model selection
//
// Automatically chooses between Businger-Dyer and Holtslag-De Bruin based
// on stability conditions indicated by bulk Richardson number.
//
// Inputs:
//   z_agl              - Height above ground level [m]
//   z0                 - Aerodynamic roughness length [m]
//   ustar              - Friction velocity [m/s]
//   kappa              - Von Kármán constant (typically 0.41)
//   L_obukhov          - Obukhov length [m]
//   theta_surface      - Surface potential temperature [K]
//   theta_height       - Potential temperature at height z [K]
//   wind_speed         - Wind speed magnitude [m/s]
//
// Returns:
//   Wind speed with adaptive stability correction applied
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real wind_profile_adaptive_stability(
    amrex::Real z_agl,
    amrex::Real z0,
    amrex::Real ustar,
    amrex::Real kappa,
    amrex::Real L_obukhov,
    amrex::Real theta_surface,
    amrex::Real theta_height,
    amrex::Real wind_speed) noexcept
{
    // Compute bulk Richardson number to assess stability
    const amrex::Real Ri_b = compute_bulk_richardson_number(
        theta_surface, theta_height, z_agl, wind_speed);
    
    // Select appropriate stability model
    const bool use_holtslag = select_holtslag_for_stability(Ri_b);
    
    // Apply standard stability correction with selected model
    return wind_profile_stability(z_agl, z0, ustar, kappa, L_obukhov, use_holtslag);
}

// Pasquill-Gifford stability class enumeration
// A = very unstable (strong convection, daytime)
// B = moderately unstable
// C = slightly unstable
// D = neutral (overcast or moderate wind)
// E = slightly stable
// F = moderately stable (strong inversion, nighttime)
enum class PGStabilityClass : int {
    A = 0,  // Very unstable
    B = 1,  // Moderately unstable
    C = 2,  // Slightly unstable
    D = 3,  // Neutral
    E = 4,  // Slightly stable
    F = 5   // Moderately stable
};

// Determine Pasquill-Gifford stability class from wind speed and solar radiation
// Uses the classic Turner (1970) lookup table approach
//
// Parameters:
//   wind_speed         - Wind speed at 10m height [m/s]
//   solar_radiation    - Solar radiation [W/m²]
//   is_nighttime       - True for nighttime conditions (use cloud cover instead)
//   cloud_cover        - Cloud cover fraction [0-1] (for nighttime only)
//
// Returns: PGStabilityClass (A-F)
//
// Daytime lookup table (based on solar radiation):
//   - Strong insolation: > 700 W/m²
//   - Moderate insolation: 350-700 W/m²
//   - Slight insolation: < 350 W/m²
//
// Nighttime lookup table (based on cloud cover):
//   - Clear: cloud_cover < 0.4
//   - Partly cloudy: 0.4 <= cloud_cover < 0.8
//   - Overcast: cloud_cover >= 0.8
//
// References:
//   - Pasquill, F. (1961). The estimation of dispersion of windborne material.
//     Meteorological Magazine, 90, 33-49.
//   - Gifford, F.A. (1961). Use of routine meteorological observations for
//     estimating atmospheric dispersion. Nuclear Safety, 2(4), 47-51.
//   - Turner, D.B. (1970). Workbook of atmospheric dispersion estimates.
//     US EPA, AP-26.
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
PGStabilityClass pasquill_gifford_class(amrex::Real wind_speed,
                                        amrex::Real solar_radiation,
                                        bool is_nighttime = false,
                                        amrex::Real cloud_cover = 0.5) noexcept
{
    // Daytime classification (based on solar radiation)
    if (!is_nighttime) {
        // Strong insolation (> 700 W/m²)
        if (solar_radiation > amrex::Real(700.0)) {
            if (wind_speed < amrex::Real(2.0)) return PGStabilityClass::A;
            if (wind_speed < amrex::Real(3.0)) return PGStabilityClass::A;
            if (wind_speed < amrex::Real(5.0)) return PGStabilityClass::B;
            if (wind_speed < amrex::Real(6.0)) return PGStabilityClass::C;
            return PGStabilityClass::D;
        }
        // Moderate insolation (350-700 W/m²)
        else if (solar_radiation > amrex::Real(350.0)) {
            if (wind_speed < amrex::Real(2.0)) return PGStabilityClass::A;
            if (wind_speed < amrex::Real(3.0)) return PGStabilityClass::B;
            if (wind_speed < amrex::Real(5.0)) return PGStabilityClass::B;
            if (wind_speed < amrex::Real(6.0)) return PGStabilityClass::C;
            return PGStabilityClass::D;
        }
        // Slight insolation (< 350 W/m²)
        else {
            if (wind_speed < amrex::Real(2.0)) return PGStabilityClass::B;
            if (wind_speed < amrex::Real(3.0)) return PGStabilityClass::C;
            if (wind_speed < amrex::Real(5.0)) return PGStabilityClass::C;
            if (wind_speed < amrex::Real(6.0)) return PGStabilityClass::D;
            return PGStabilityClass::D;
        }
    }
    // Nighttime classification (based on cloud cover)
    else {
        // Clear sky (cloud_cover < 0.4)
        if (cloud_cover < amrex::Real(0.4)) {
            if (wind_speed < amrex::Real(2.0)) return PGStabilityClass::F;
            if (wind_speed < amrex::Real(3.0)) return PGStabilityClass::F;
            if (wind_speed < amrex::Real(5.0)) return PGStabilityClass::E;
            if (wind_speed < amrex::Real(6.0)) return PGStabilityClass::D;
            return PGStabilityClass::D;
        }
        // Partly cloudy (0.4 <= cloud_cover < 0.8)
        else if (cloud_cover < amrex::Real(0.8)) {
            if (wind_speed < amrex::Real(2.0)) return PGStabilityClass::E;
            if (wind_speed < amrex::Real(3.0)) return PGStabilityClass::E;
            if (wind_speed < amrex::Real(5.0)) return PGStabilityClass::D;
            if (wind_speed < amrex::Real(6.0)) return PGStabilityClass::D;
            return PGStabilityClass::D;
        }
        // Overcast (cloud_cover >= 0.8)
        else {
            return PGStabilityClass::D;  // Neutral for all wind speeds
        }
    }
}

// Map Pasquill-Gifford stability class to Obukhov length
// Provides a simple conversion from discrete PG classes to continuous L values
//
// Parameters:
//   pg_class           - Pasquill-Gifford stability class (A-F)
//
// Returns: Approximate Obukhov length L [m]
//
// Typical mapping (approximate):
//   Class A (very unstable):      L ≈ -50 m
//   Class B (moderately unstable): L ≈ -100 m
//   Class C (slightly unstable):   L ≈ -200 m
//   Class D (neutral):             L ≈ 10000 m (large positive)
//   Class E (slightly stable):     L ≈ 100 m
//   Class F (moderately stable):   L ≈ 50 m
//
// Note: These are representative values; actual L varies with surface conditions
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real pg_class_to_obukhov_length(PGStabilityClass pg_class) noexcept
{
    switch (pg_class) {
        case PGStabilityClass::A:
            return amrex::Real(-50.0);    // Very unstable
        case PGStabilityClass::B:
            return amrex::Real(-100.0);   // Moderately unstable
        case PGStabilityClass::C:
            return amrex::Real(-200.0);   // Slightly unstable
        case PGStabilityClass::D:
            return amrex::Real(10000.0);  // Neutral (large positive)
        case PGStabilityClass::E:
            return amrex::Real(100.0);    // Slightly stable
        case PGStabilityClass::F:
            return amrex::Real(50.0);     // Moderately stable
        default:
            return amrex::Real(10000.0);  // Default to neutral
    }
}

#endif // STABILITY_MODELS_H
