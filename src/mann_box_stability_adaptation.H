// ============================================================================
// mann_box_stability_adaptation.H
// 
// PHASE 4: Temporal & Stability Physics - Part 2: Stability Adaptation
// 
// Implements atmospheric stability classification and stability-dependent
// modifications to the Mann Box spectral tensor. Adapts turbulence parameters
// based on Obukhov length and Richardson number.
//
// Key Features:
// - Bulk Richardson number (Ri_b) classification
// - Obukhov length (L_MO) computation
// - Stability-dependent spectral tensor modifications
// - Convective scaling for unstable conditions
// - GPU-ready device-compatible kernels
//
// Mathematical Foundation:
// 
// 1. Bulk Richardson Number
//    Ri_b = (g/θ) * (dθ/dz) * z² / ((du/dz)² + (dv/dz)²)
//    
//    Classification:
//    - Ri_b > 0.25:     Stable (nocturnal, nighttime)
//    - 0.25 > Ri_b > 0: Slightly unstable (low convection)
//    - Ri_b ≈ 0:        Neutral (well-mixed, weak winds)
//    - Ri_b < 0:        Unstable (daytime, heating)
//
// 2. Obukhov Length
//    L_MO = -u_*³ / (κ * g * H_s / (ρ * c_p * T))
//    
//    where:
//      u_* = friction velocity [m/s]
//      κ = von Kármán constant ≈ 0.41
//      H_s = sensible heat flux [W/m²]
//      ρ = air density [kg/m³]
//      c_p = specific heat [J/(kg·K)]
//      T = temperature [K]
//    
//    Interpretation:
//    - L_MO > 0: Stable (strong inversion, negative buoyancy)
//    - L_MO < 0: Unstable (heating, positive buoyancy)
//    - |L_MO| → ∞: Neutral (no buoyancy effects)
//
// 3. Stability-Dependent Tensor Modification
//    The anisotropy tensor Γ_ij is modified by stability factor f(Ri_b):
//    
//    Γ_ij^stab = Γ_ij × f(Ri_b)
//    
//    where:
//    - Stable: Reduce w-component, enhance vertical correlations
//    - Unstable: Enhance w-component, boost convective scales
//    - Neutral: Use baseline (f ≈ 1)
//
// 4. Convective Velocity Scale
//    For unstable conditions, convective velocity w_* drives turbulence:
//    w_* = (g * H_s * z_i / (ρ * c_p * T))^(1/3)
//    
//    This becomes dominant over shear-driven turbulence when:
//    w_* > (z/L) * u_*
//
// References:
//   - Obukhov, A. M. (1946). Turbulence in an atmosphere with non-uniform
//     temperature. Boundary-Layer Meteorology.
//   - Stull, R. B. (1988). An Introduction to Boundary Layer Meteorology.
//   - Högström, U. (1996). Review of some basic characteristics of the
//     atmospheric surface layer. Boundary-Layer Meteorology, 78, 215-246.
//
// ============================================================================

#ifndef MANN_BOX_STABILITY_ADAPTATION_H
#define MANN_BOX_STABILITY_ADAPTATION_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <algorithm>
#include "solver_math_constants.H"

// ============================================================================
// Stability Parameters Structure
// ============================================================================

struct StabilityParams {
    // Mean potential temperature [K]
    amrex::Real theta_mean = 300.0;
    
    // Temperature gradient dθ/dz [K/m]
    amrex::Real dtheta_dz = 0.0;
    
    // Wind shear du/dz [1/s]
    amrex::Real du_dz = 0.1;
    
    // Wind shear dv/dz [1/s]
    amrex::Real dv_dz = 0.05;
    
    // Height for Richardson number computation [m]
    amrex::Real height = 10.0;
    
    // Friction velocity [m/s]
    amrex::Real friction_velocity = 0.5;
    
    // Sensible heat flux [W/m²]
    amrex::Real heat_flux = 0.0;
    
    // Boundary layer height [m]
    amrex::Real boundary_layer_height = 1000.0;
};

// ============================================================================
// Stability Classification Constants
// ============================================================================

namespace StabilityConstants {
    // Gravitational acceleration [m/s²]
    constexpr amrex::Real g = amrex::Real(9.81);
    
    // Von Kármán constant
    constexpr amrex::Real kappa = amrex::Real(0.41);
    
    // Air density [kg/m³]
    constexpr amrex::Real rho = amrex::Real(1.225);
    
    // Specific heat capacity [J/(kg·K)]
    constexpr amrex::Real c_p = amrex::Real(1005.0);
    
    // Critical Richardson number for stable/unstable transition
    constexpr amrex::Real RICHARDSON_CRITICAL = amrex::Real(0.25);
    
    // Neutral Richardson number threshold
    constexpr amrex::Real RICHARDSON_NEUTRAL_THRESHOLD = amrex::Real(0.05);
}

// ============================================================================
// Namespace for stability adaptation operations
// ============================================================================

namespace MannBoxStabilityAdaptation {

// ============================================================================
// PART 1: Richardson Number Classification
// ============================================================================

/// Compute bulk Richardson number
/// Ri_b = (g/θ) * (dθ/dz) * z² / ((du/dz)² + (dv/dz)²)
/// 
/// @param temperature_gradient  dθ/dz [K/m]
/// @param wind_shear_u         du/dz [1/s]
/// @param wind_shear_v         dv/dz [1/s]
/// @param mean_temperature     Mean temperature θ [K]
/// @param height               Height z [m]
/// @return                     Richardson number (dimensionless)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_richardson_number(
    amrex::Real temperature_gradient,
    amrex::Real wind_shear_u,
    amrex::Real wind_shear_v,
    amrex::Real mean_temperature = amrex::Real(300.0),
    amrex::Real height = amrex::Real(10.0)
) noexcept
{
    using namespace StabilityConstants;
    
    // Avoid division by zero
    amrex::Real shear_squared = wind_shear_u * wind_shear_u + 
                                wind_shear_v * wind_shear_v;
    
    if (shear_squared < 1.0e-8) {
        // Very weak shear: return based on temperature gradient sign
        return (temperature_gradient > 0.0) ? 1.0 : -1.0;
    }
    
    // Compute Richardson number
    amrex::Real numerator = g * temperature_gradient * height * height / mean_temperature;
    amrex::Real richardson = numerator / shear_squared;
    
    return richardson;
}

/// Classify stability regime based on Richardson number
/// 
/// @param richardson_number    Bulk Richardson number (dimensionless)
/// @return  Stability regime: -1=unstable, 0=neutral, 1=stable
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
int classify_stability_regime(amrex::Real richardson_number) noexcept
{
    using namespace StabilityConstants;
    
    if (richardson_number > RICHARDSON_CRITICAL) {
        return 1;  // Stable
    } else if (richardson_number < -RICHARDSON_CRITICAL) {
        return -1;  // Unstable
    } else {
        return 0;  // Neutral
    }
}

/// Get stability name as string (for diagnostics)
/// Note: This is CPU-only; use with care
/// 
/// @param stability_regime  Regime from classify_stability_regime
/// @return                  Descriptive name (CPU-only)
inline const char* get_stability_name(int stability_regime) {
    if (stability_regime > 0) return "Stable";
    if (stability_regime < 0) return "Unstable";
    return "Neutral";
}

// ============================================================================
// PART 2: Obukhov Length Computation
// ============================================================================

/// Compute Obukhov length from flux Richardson number
/// L_MO ≈ -u_*³ / (κ * (g/T) * H_s / (ρ * c_p))
/// 
/// @param friction_velocity  Friction velocity u_* [m/s]
/// @param heat_flux         Sensible heat flux H_s [W/m²]
/// @param mean_temperature  Mean temperature [K]
/// @return                  Obukhov length L_MO [m]
///                         (positive=stable, negative=unstable, large|L|=neutral)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_obukhov_length(
    amrex::Real friction_velocity,
    amrex::Real heat_flux,
    amrex::Real mean_temperature = amrex::Real(300.0)
) noexcept
{
    using namespace StabilityConstants;
    
    // Avoid division by zero
    if (std::abs(friction_velocity) < 1.0e-4) {
        // Return neutral limit
        return (heat_flux > 0.0) ? -1.0e6 : 1.0e6;
    }
    
    // Heat flux effect: H_s / (ρ * c_p * T)
    amrex::Real heat_effect = heat_flux / (rho * c_p * mean_temperature);
    
    // Avoid very small denominators
    if (std::abs(heat_effect) < 1.0e-6) {
        return (heat_flux > 0.0) ? -1.0e6 : 1.0e6;
    }
    
    // Obukhov length: L = -u_*³ / (κ * (g/T) * H_s/(ρ*c_p))
    amrex::Real u_cubed = friction_velocity * friction_velocity * friction_velocity;
    amrex::Real denominator = kappa * (g / mean_temperature) * heat_effect;
    
    amrex::Real obukhov_length = -u_cubed / denominator;
    
    // Clamp to reasonable bounds to avoid numerical issues
    amrex::Real max_L = 1.0e6;
    amrex::Real min_L = -1.0e6;
    
    return std::max(std::min(obukhov_length, max_L), min_L);
}

/// Convert Obukhov length to stability parameter
/// ζ = z / L_MO (dimensionless)
/// 
/// @param height          Height z [m]
/// @param obukhov_length  Obukhov length L_MO [m]
/// @return                Stability parameter ζ (dimensionless)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_stability_parameter_zeta(
    amrex::Real height,
    amrex::Real obukhov_length
) noexcept
{
    if (std::abs(obukhov_length) < 1.0e-6) {
        return 0.0;  // Neutral
    }
    
    return height / obukhov_length;
}

// ============================================================================
// PART 3: Stability-Dependent Tensor Modification Factors
// ============================================================================

/// Compute stability modification factor for spectral tensor
/// Modifies anisotropy components based on atmospheric stability
/// 
/// Stable (Ri > 0.25):
///   - Reduce w-component: multiply by factor < 1
///   - Enhance u,v components
///   - Increase vertical coherence
/// 
/// Unstable (Ri < -0.25):
///   - Enhance w-component: multiply by factor > 1
///   - Reduce u,v components
///   - Convective scales dominate
/// 
/// @param richardson_number   Bulk Richardson number
/// @param component_index     0=u, 1=v, 2=w
/// @return                    Modification factor (typically 0.5-2.0)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_stability_modification_factor(
    amrex::Real richardson_number,
    int component_index
) noexcept
{
    using namespace StabilityConstants;
    
    // Base factor (neutral)
    amrex::Real base_factor = amrex::Real(1.0);
    
    int stability = classify_stability_regime(richardson_number);
    
    // Clamp Richardson number to reasonable range for interpolation
    amrex::Real clamped_ri = std::max(std::min(richardson_number, 1.0), -1.0);
    
    if (stability > 0) {
        // Stable conditions (Ri > 0.25)
        // Temperature stratification suppresses vertical motion
        if (component_index == 2) {  // w-component
            // Reduce vertical turbulence: f_w = 0.5 - 0.5*Ri
            amrex::Real factor = amrex::Real(0.5) - amrex::Real(0.5) * clamped_ri;
            return std::max(factor, amrex::Real(0.1));  // Don't reduce below 0.1
        } else {
            // Enhance u,v components: f_u,v = 1.0 + 0.3*Ri
            amrex::Real factor = amrex::Real(1.0) + amrex::Real(0.3) * clamped_ri;
            return std::min(factor, amrex::Real(1.5));  // Cap at 1.5
        }
    } else if (stability < 0) {
        // Unstable conditions (Ri < -0.25)
        // Convection enhances vertical motion
        if (component_index == 2) {  // w-component
            // Enhance vertical turbulence: f_w = 1.0 - 0.5*Ri (Ri is negative)
            amrex::Real factor = amrex::Real(1.0) - amrex::Real(0.5) * clamped_ri;
            return std::min(factor, amrex::Real(2.0));  // Cap at 2.0
        } else {
            // Reduce u,v components: f_u,v = 1.0 + 0.2*Ri (Ri is negative)
            amrex::Real factor = amrex::Real(1.0) + amrex::Real(0.2) * clamped_ri;
            return std::max(factor, amrex::Real(0.5));  // Don't reduce below 0.5
        }
    }
    
    // Neutral: return unmodified
    return base_factor;
}

// ============================================================================
// PART 4: Convective Scaling
// ============================================================================

/// Compute convective velocity scale
/// w_* = (g * H_s * z_i / (ρ * c_p * T))^(1/3)
/// 
/// @param heat_flux              Sensible heat flux H_s [W/m²]
/// @param boundary_layer_height  Boundary layer depth z_i [m]
/// @param mean_temperature       Mean temperature T [K]
/// @return                       Convective velocity w_* [m/s]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_convective_velocity(
    amrex::Real heat_flux,
    amrex::Real boundary_layer_height,
    amrex::Real mean_temperature = amrex::Real(300.0)
) noexcept
{
    using namespace StabilityConstants;
    
    // Avoid negative heat flux in unstable regime
    if (heat_flux <= 0.0) return 0.0;
    
    // w_* = (g * H_s * z_i / (ρ * c_p * T))^(1/3)
    amrex::Real numerator = g * heat_flux * boundary_layer_height;
    amrex::Real denominator = rho * c_p * mean_temperature;
    
    if (denominator < 1.0e-6) return 0.0;
    
    amrex::Real convective_vel_cubed = numerator / denominator;
    
    return (convective_vel_cubed > 0.0) ? 
           std::cbrt(convective_vel_cubed) : 0.0;
}

/// Compute convective length scale for convective boundary layer
/// L_* = z_i (boundary layer height becomes the relevant scale)
/// 
/// @param boundary_layer_height  Boundary layer depth z_i [m]
/// @return                       Convective length scale [m]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_convective_length_scale(
    amrex::Real boundary_layer_height
) noexcept
{
    // In convective conditions, z_i becomes the dominant length scale
    return boundary_layer_height;
}

/// Scale turbulence intensity for unstable/stable conditions
/// In unstable: increase intensity (more vigorous turbulence)
/// In stable: decrease intensity (suppressed turbulence)
/// 
/// @param neutral_intensity      Turbulence intensity in neutral conditions [dimensionless]
/// @param richardson_number      Bulk Richardson number
/// @return                       Modified turbulence intensity
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real scale_turbulence_intensity(
    amrex::Real neutral_intensity,
    amrex::Real richardson_number
) noexcept
{
    // Clamp Richardson number for safety
    amrex::Real clamped_ri = std::max(std::min(richardson_number, 1.0), -1.0);
    
    int stability = classify_stability_regime(richardson_number);
    
    if (stability > 0) {
        // Stable: reduce intensity as Ri increases
        // TI_stable ≈ TI_neutral * (1 - 0.4*Ri)
        amrex::Real factor = amrex::Real(1.0) - amrex::Real(0.4) * clamped_ri;
        return neutral_intensity * std::max(factor, amrex::Real(0.3));
    } else if (stability < 0) {
        // Unstable: increase intensity (convection adds energy)
        // TI_unstable ≈ TI_neutral * (1 - 0.3*Ri) [Ri is negative]
        amrex::Real factor = amrex::Real(1.0) - amrex::Real(0.3) * clamped_ri;
        return neutral_intensity * std::min(factor, amrex::Real(1.8));
    }
    
    // Neutral: no modification
    return neutral_intensity;
}

// ============================================================================
// PART 5: Vertical Coherence Modifications
// ============================================================================

/// Compute effective length scale factor for vertical coherence based on stability
/// Stable conditions enhance vertical coherence (larger effective scale)
/// Unstable conditions reduce vertical coherence (smaller effective scale)
/// 
/// @param richardson_number    Bulk Richardson number
/// @return                     Length scale multiplication factor (0.5-2.0)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_vertical_coherence_length_scale_factor(
    amrex::Real richardson_number
) noexcept
{
    constexpr amrex::Real CRITICAL = amrex::Real(0.25);
    
    // Base factor (neutral)
    amrex::Real base_factor = amrex::Real(1.0);
    
    // Clamp Ri for safety
    amrex::Real clamped_ri = std::max(std::min(richardson_number, 1.0), -1.0);
    
    int stability = classify_stability_regime(richardson_number);
    
    if (stability > 0) {
        // Stable: enhance coherence (increase effective scale)
        // f_L = 1.0 + 0.5*Ri (larger scale = slower decay = stronger coherence)
        amrex::Real factor = base_factor + amrex::Real(0.5) * clamped_ri;
        return std::min(factor, amrex::Real(2.0));
    } else if (stability < 0) {
        // Unstable: reduce coherence (decrease effective scale)
        // f_L = 1.0 + 0.3*Ri (Ri negative, so factor < 1)
        amrex::Real factor = base_factor + amrex::Real(0.3) * clamped_ri;
        return std::max(factor, amrex::Real(0.5));
    }
    
    // Neutral: base factor
    return base_factor;
}

/// Compute vertical correlation coefficient at given height separation
/// Based on Mann Box model with stability modifications
/// 
/// @param height_separation      Vertical separation Δz [m]
/// @param vertical_length_scale  Vertical length scale L_w [m]
/// @param richardson_number      Bulk Richardson number
/// @return                       Correlation coefficient ∈ [0, 1]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_vertical_correlation_coefficient(
    amrex::Real height_separation,
    amrex::Real vertical_length_scale,
    amrex::Real richardson_number
) noexcept
{
    if (vertical_length_scale < 1.0e-6) return 0.0;
    
    // Get stability-adjusted effective length scale factor
    amrex::Real length_factor = 
        compute_vertical_coherence_length_scale_factor(richardson_number);
    
    // Effective length scale with stability adjustment
    amrex::Real effective_L = vertical_length_scale * length_factor;
    
    // Standard exponential decay with adjusted length scale
    amrex::Real normalized_sep = height_separation / effective_L;
    
    return std::exp(-std::abs(normalized_sep));
}

} // namespace MannBoxStabilityAdaptation

#endif // MANN_BOX_STABILITY_ADAPTATION_H
