// ============================================================================
// flux_diagnostics.H
// Diagnostic flux calculations for surface heat, moisture, and momentum
//
// This header provides functions for computing surface fluxes at each point:
// - Sensible Heat Flux (SHF) = ρ * Cp * u* * θ*
// - Latent Heat Flux (LHF) = ρ * Lv * u* * q*
// - Momentum Flux (τ) = ρ * u*²
// - Drag Coefficient (Cd) = (u* / |U|)²
//
// These are diagnostic fields (no feedback to wind field) but critical for:
// - Fire-atmosphere coupling
// - Dust emission parameterization
// - Surface energy balance diagnostics
// - Dispersion model forcing
//
// Physical constants:
// - ρ (air density) = 1.225 kg/m³ at sea level, standard conditions
// - Cp (specific heat at constant pressure) = 1005 J/(kg·K)
// - Lv (latent heat of vaporization) = 2.5e6 J/kg
// - κ (von Karman constant) = 0.41
// ============================================================================

#ifndef FLUX_DIAGNOSTICS_H
#define FLUX_DIAGNOSTICS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>

// Physical constants for flux calculations
namespace FluxConstants {
    // Standard atmospheric properties at sea level, 15°C
    constexpr amrex::Real rho_air = amrex::Real(1.225);        // Air density [kg/m³]
    constexpr amrex::Real cp_air = amrex::Real(1005.0);        // Specific heat, const. press. [J/(kg·K)]
    constexpr amrex::Real lv_water = amrex::Real(2.5e6);       // Latent heat vaporization [J/kg]
    constexpr amrex::Real lv_sublimation = amrex::Real(2.834e6); // Latent heat sublimation [J/kg]
    
    // Temperature reference (K)
    constexpr amrex::Real T_ref = amrex::Real(288.15);         // 15°C in Kelvin
    
    // Monin-Obukhov scaling
    constexpr amrex::Real kappa = amrex::Real(0.41);           // von Karman constant
    constexpr amrex::Real g = amrex::Real(9.81);               // Gravitational acceleration [m/s²]
}

// Note: compute_friction_velocity() is defined in wall_functions.H
// We reuse that function here instead of duplicating

// ============================================================================
// Compute sensible heat flux from friction velocity and temperature scale
//
// Sensible Heat Flux: SHF = ρ * Cp * u* * θ*  [W/m²]
//
// Where:
//   ρ    = air density [kg/m³]
//   Cp   = specific heat at constant pressure [J/(kg·K)]
//   u*   = friction velocity [m/s]
//   θ*   = potential temperature scale [K]
//
// The temperature scale θ* is usually computed from surface heat flux
// or from temperature difference: θ* ≈ ΔT * u* / U_bulk
//
// Inputs:
//   u_star      - Friction velocity [m/s]
//   theta_star  - Temperature scale [K]
//   rho         - Air density [kg/m³] (default: standard at sea level)
//   cp          - Specific heat [J/(kg·K)] (default: dry air)
//
// Returns:
//   SHF         - Sensible heat flux [W/m²]
//                 Positive: upward (surface warms atmosphere)
//                 Negative: downward (surface cools atmosphere)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_sensible_heat_flux(
    amrex::Real u_star,
    amrex::Real theta_star,
    amrex::Real rho = FluxConstants::rho_air,
    amrex::Real cp = FluxConstants::cp_air)
{
    return rho * cp * u_star * theta_star;
}

// ============================================================================
// Compute latent heat flux from friction velocity and moisture scale
//
// Latent Heat Flux: LHF = ρ * Lv * u* * q*  [W/m²]
//
// Where:
//   ρ    = air density [kg/m³]
//   Lv   = latent heat of vaporization [J/kg] (~2.5e6 J/kg for water)
//   u*   = friction velocity [m/s]
//   q*   = specific humidity scale [kg/kg]
//
// The moisture scale q* is usually computed from surface evaporation:
//   q* = (q_sat(T_sfc) - q_air) * u* / U_bulk
//
// Note: This is diagnostic; we use a typical value or external forcing.
//
// Inputs:
//   u_star      - Friction velocity [m/s]
//   q_star      - Moisture scale [kg/kg]
//   rho         - Air density [kg/m³] (default: standard)
//   lv          - Latent heat [J/kg] (default: vaporization at 15°C)
//
// Returns:
//   LHF         - Latent heat flux [W/m²]
//                 Positive: upward (surface evaporating)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_latent_heat_flux(
    amrex::Real u_star,
    amrex::Real q_star,
    amrex::Real rho = FluxConstants::rho_air,
    amrex::Real lv = FluxConstants::lv_water)
{
    return rho * lv * u_star * q_star;
}

// ============================================================================
// Compute momentum flux (surface shear stress)
//
// Momentum Flux: τ = ρ * u*²  [Pa = N/m²]
//
// This is the magnitude of surface shear stress, important for:
// - Wind erosion parameterization
// - Saltation and dust emission
// - Surface drag quantification
//
// Inputs:
//   u_star      - Friction velocity [m/s]
//   rho         - Air density [kg/m³] (default: standard)
//
// Returns:
//   τ           - Momentum flux / shear stress [Pa]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_momentum_flux(
    amrex::Real u_star,
    amrex::Real rho = FluxConstants::rho_air)
{
    return rho * u_star * u_star;
}

// ============================================================================
// Compute drag coefficient from friction velocity and wind speed
//
// Drag Coefficient: Cd = (u* / |U|)²  [dimensionless]
//
// This is the aerodynamic drag coefficient at a given height.
// Typical ranges:
//   - Smooth water: Cd ~ 0.0006 - 0.0015 (height-dependent)
//   - Grassland:    Cd ~ 0.004 - 0.01
//   - Urban/forest: Cd ~ 0.02 - 0.1
//
// Used in bulk aerodynamic flux formulas:
//   F = ρ * Cd * |U| * (scalar_surface - scalar_air)
//
// Inputs:
//   u_star      - Friction velocity [m/s]
//   U_mag       - Wind speed magnitude [m/s]
//
// Returns:
//   Cd          - Drag coefficient [dimensionless]
//                 Returns 0 if wind speed is negligible
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_drag_coefficient(
    amrex::Real u_star,
    amrex::Real U_mag)
{
    if (U_mag < amrex::Real(1.0e-6)) {
        return amrex::Real(0.0);
    }
    
    amrex::Real cd = u_star / U_mag;
    return cd * cd;  // Cd = (u*/U)²
}

// ============================================================================
// Compute all flux diagnostics at once (efficient batch computation)
//
// This function computes friction velocity, momentum flux, drag coefficient,
// and prepares output fields for sensible/latent heat fluxes.
//
// Inputs:
//   U_mag       - Wind speed magnitude [m/s]
//   z_agl       - Height above ground level [m]
//   z0          - Aerodynamic roughness length [m]
//   theta_star  - Temperature scale [K] (if computing SHF)
//   q_star      - Moisture scale [kg/kg] (if computing LHF)
//   compute_heat - Whether to compute heat flux diagnostics
//   compute_moist - Whether to compute moisture flux diagnostics
//
// Outputs (via reference):
//   u_star      - Friction velocity [m/s]
//   tau_flux    - Momentum flux [Pa]
//   cd          - Drag coefficient [dimensionless]
//   shf         - Sensible heat flux [W/m²]
//   lhf         - Latent heat flux [W/m²]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void compute_all_surface_fluxes(
    amrex::Real U_mag,
    amrex::Real z_agl,
    amrex::Real z0,
    amrex::Real theta_star,
    amrex::Real q_star,
    bool compute_heat,
    bool compute_moist,
    amrex::Real& u_star,
    amrex::Real& tau_flux,
    amrex::Real& cd,
    amrex::Real& shf,
    amrex::Real& lhf)
{
    // Compute friction velocity from log-law: u* = κ * |U| / ln((z+z0)/z0)
    if (U_mag < amrex::Real(1.0e-10) || z_agl < z0) {
        u_star = amrex::Real(0.0);
    } else {
        amrex::Real log_term = std::log((z_agl + z0) / z0);
        if (log_term < amrex::Real(1.0e-10)) {
            u_star = amrex::Real(0.0);
        } else {
            u_star = FluxConstants::kappa * U_mag / log_term;
        }
    }
    
    // Compute momentum flux
    tau_flux = compute_momentum_flux(u_star);
    
    // Compute drag coefficient
    cd = compute_drag_coefficient(u_star, U_mag);
    
    // Compute heat fluxes (optional)
    if (compute_heat) {
        shf = compute_sensible_heat_flux(u_star, theta_star);
    } else {
        shf = amrex::Real(0.0);
    }
    
    if (compute_moist) {
        lhf = compute_latent_heat_flux(u_star, q_star);
    } else {
        lhf = amrex::Real(0.0);
    }
}

// ============================================================================
// Compute spatially-varying ABL height H_abl(x, y)
//
// Dynamically computes H_abl based on Monin-Obukhov similarity theory:
// - Stable: Zilitinkevich (1972) formula: H_abl = C_s * sqrt(u* * L / |f|)
// - Unstable: Convective scaling: H_abl = 0.2 * u* * |L| / (1 + w* / u*)
// - Neutral: H_abl = C_n * u* / |f|
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_spatially_varying_habl(
    amrex::Real ustar,
    amrex::Real heat_flux,
    amrex::Real z0,
    amrex::Real theta_ref = amrex::Real(300.0),
    amrex::Real f_coriolis = amrex::Real(1.0e-4))
{
    (void)z0;
    using namespace FluxConstants;
    
    ustar = std::max(ustar, amrex::Real(0.01));
    
    // Obukhov length L_MO = -ustar^3 * theta_ref / (kappa * g * heat_flux / (rho * cp))
    amrex::Real L = amrex::Real(1.0e6);
    if (std::abs(heat_flux) > amrex::Real(1.0e-4)) {
        amrex::Real heat_effect = heat_flux / (rho_air * cp_air * theta_ref);
        if (std::abs(heat_effect) > amrex::Real(1.0e-10)) {
            L = -(ustar * ustar * ustar) / (kappa * (g / theta_ref) * heat_effect);
        }
    }
    
    // Clamp L
    if (L > amrex::Real(1.0e6)) L = amrex::Real(1.0e6);
    if (L < amrex::Real(-1.0e6)) L = amrex::Real(-1.0e6);
    
    amrex::Real h_abl = amrex::Real(1000.0);
    amrex::Real abs_f = std::max(std::abs(f_coriolis), amrex::Real(1.0e-5));
    
    if (L > amrex::Real(0.0) && L < amrex::Real(1.0e5)) {
        // Stable conditions: Zilitinkevich (1972) formula
        h_abl = amrex::Real(0.4) * std::sqrt(ustar * L / abs_f);
    } else if (L < amrex::Real(0.0) && L > amrex::Real(-1.0e5)) {
        // Unstable conditions: Convective scaling using w_*
        // Compute w_* with nominal H_abl of 1000m (delta_0)
        const amrex::Real delta_0 = amrex::Real(1000.0);
        amrex::Real w_star = amrex::Real(0.0);
        amrex::Real numerator = g * heat_flux * delta_0;
        amrex::Real denominator = rho_air * cp_air * theta_ref;
        if (denominator > amrex::Real(1.0e-6)) {
            w_star = std::cbrt(numerator / denominator);
        }
        amrex::Real denom = amrex::Real(1.0) + (w_star / ustar);
        h_abl = amrex::Real(0.2) * ustar * std::abs(L) / denom;
    } else {
        // Neutral: H_abl = C_n * u* / |f|
        h_abl = amrex::Real(0.15) * ustar / abs_f;
    }
    
    // Clamp H_abl to physical bounds
    h_abl = std::max(amrex::Real(100.0), std::min(h_abl, amrex::Real(3000.0)));
    return h_abl;
}

#endif  // FLUX_DIAGNOSTICS_H
