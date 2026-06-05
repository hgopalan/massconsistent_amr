// ============================================================================
// diurnal_roughness_models.H
// Diurnal variations in aerodynamic roughness length z₀
//
// This header provides functions for computing time-varying aerodynamic
// roughness length z₀(t) based on diurnal (24-hour) cycles.
//
// Physical basis:
// - Vegetation canopy height and porosity vary with time of day
// - Nighttime: Reduced turbulence mixing, effective z₀ increases
// - Daytime: Increased convective turbulence, effective z₀ may decrease
// - Seasonal and local weather effects
//
// Model: z₀(t) = z₀_base × [1 + A·sin(πt/12 + φ)]
//
// where:
//   z₀_base = base roughness length [m]
//   A = amplitude of modulation (typically 0.1-0.5)
//   t = time of day [hours, 0-24]
//   φ = phase offset [radians] (typically 0, π, or π/2)
//
// References:
// - Brutsaert, W. (1982). Evaporation into the Atmosphere.
// - Paulson, C.A. (1970). The Mathematical Representation of Wind Speed...
// ============================================================================

#ifndef DIURNAL_ROUGHNESS_MODELS_H
#define DIURNAL_ROUGHNESS_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include "solver_math_constants.H"
#include <cmath>

// ============================================================================
// Diurnal roughness parameters structure
// ============================================================================
struct DiurnalRoughnessParams {
    bool enabled = false;                   // Enable diurnal z₀ variations
    amrex::Real z0_base = 0.1;              // Base roughness length [m]
    amrex::Real roughness_amplitude = 0.3;  // Amplitude of modulation (0-1)
    amrex::Real time_of_day = 12.0;         // Current time [hours, 0-24]
    amrex::Real phase_offset = 0.0;         // Phase offset [radians]
};

// Physical constants for diurnal roughness
namespace DiurnalRoughnessConstants {
    // Maximum allowed roughness length ratio (z₀_max / z₀_base)
    constexpr amrex::Real MAX_Z0_RATIO = amrex::Real(2.0);
    
    // Minimum allowed roughness length ratio (z₀_min / z₀_base)
    constexpr amrex::Real MIN_Z0_RATIO = amrex::Real(0.2);
    
    // Conversion factor for hourly time to sinusoidal period
    // 12 hours → half period of diurnal cycle (24 hours)
    constexpr amrex::Real HALF_PERIOD_HOURS = amrex::Real(12.0);
}

// ============================================================================
// Compute time-varying roughness length from diurnal cycle
//
// Applies sinusoidal modulation to base roughness length based on time of day.
// The diurnal cycle reflects changes in canopy structure and atmospheric mixing.
//
// z₀(t) = z₀_base × [1 + A·sin(πt/12 + φ)]
//
// Typical behavior:
// - t = 0 (midnight): low (stable) → higher effective z₀ possible
// - t = 6 (dawn): transition
// - t = 12 (noon): high mixing → lower z₀
// - t = 18 (dusk): transition
// - t = 24 (next midnight): low (stable) → higher effective z₀
//
// Inputs:
//   time_of_day [hours]     - Time of day in 24-hour format (0-24)
//   params                  - DiurnalRoughnessParams structure
//
// Returns:
//   Time-varying roughness length z₀(t) [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_diurnal_z0(
    amrex::Real time_of_day,
    const DiurnalRoughnessParams& params)
{
    if (!params.enabled) {
        return params.z0_base;  // No modulation
    }
    
    // Normalize time to 0-24 hour range
    amrex::Real t_normalized = time_of_day;
    while (t_normalized >= amrex::Real(24.0)) {
        t_normalized -= amrex::Real(24.0);
    }
    while (t_normalized < amrex::Real(0.0)) {
        t_normalized += amrex::Real(24.0);
    }
    
    // Convert time to phase angle: θ = π·t/12 + φ
    // This gives one complete 2π cycle over 24 hours (half period = 12 hours)
    const amrex::Real theta = MathConstants::pi * (t_normalized / DiurnalRoughnessConstants::HALF_PERIOD_HOURS) 
                             + params.phase_offset;
    
    // Compute z₀(t) = z₀_base × [1 + A·sin(θ)]
    // sin(θ) ranges from -1 to +1
    // 1 + A·sin(θ) ranges from (1-A) to (1+A)
    const amrex::Real modulation_factor = amrex::Real(1.0) 
                                         + params.roughness_amplitude * std::sin(theta);
    
    // Apply bounds to prevent unrealistic z₀ values
    amrex::Real z0_modulated = params.z0_base * modulation_factor;
    
    // Ensure z₀ stays within reasonable bounds
    const amrex::Real z0_max = params.z0_base * DiurnalRoughnessConstants::MAX_Z0_RATIO;
    const amrex::Real z0_min = params.z0_base * DiurnalRoughnessConstants::MIN_Z0_RATIO;
    
    z0_modulated = std::max(z0_min, std::min(z0_modulated, z0_max));
    
    return z0_modulated;
}

// ============================================================================
// Compute derivative of diurnal z₀ with respect to time
//
// Used for computing how quickly roughness is changing
//
// dz₀/dt = z₀_base × A × (π/12) × cos(πt/12 + φ)
//
// Inputs:
//   time_of_day [hours]     - Time of day in 24-hour format (0-24)
//   params                  - DiurnalRoughnessParams structure
//
// Returns:
//   Time derivative of z₀ dz₀/dt [m/hour]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_diurnal_z0_derivative(
    amrex::Real time_of_day,
    const DiurnalRoughnessParams& params)
{
    if (!params.enabled) {
        return amrex::Real(0.0);  // No time variation
    }
    
    // Normalize time to 0-24 hour range
    amrex::Real t_normalized = time_of_day;
    while (t_normalized >= amrex::Real(24.0)) {
        t_normalized -= amrex::Real(24.0);
    }
    while (t_normalized < amrex::Real(0.0)) {
        t_normalized += amrex::Real(24.0);
    }
    
    // Convert time to phase angle
    const amrex::Real theta = MathConstants::pi * (t_normalized / DiurnalRoughnessConstants::HALF_PERIOD_HOURS) 
                             + params.phase_offset;
    
    // dz₀/dt = z₀_base × A × (π/12) × cos(θ)
    const amrex::Real dtheta_dt = MathConstants::pi / DiurnalRoughnessConstants::HALF_PERIOD_HOURS;
    const amrex::Real dz0_dt = params.z0_base * params.roughness_amplitude 
                              * dtheta_dt * std::cos(theta);
    
    return dz0_dt;
}

// ============================================================================
// Apply diurnal z₀ variation to friction velocity
//
// When z₀ changes, the friction velocity u* also changes due to the log-law
// relationship. This function updates u_star accounting for z₀(t).
//
// u* = κ·|U_ref| / ln((z_ref + z₀(t)) / z₀(t))
//
// Inputs:
//   u_ref, v_ref           - Reference wind components [m/s]
//   z_ref                  - Reference height [m]
//   time_of_day            - Time of day [hours]
//   params                 - DiurnalRoughnessParams structure
//
// Returns:
//   Updated friction velocity u_star [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real apply_diurnal_z0_to_friction_velocity(
    amrex::Real u_ref,
    amrex::Real v_ref,
    amrex::Real z_ref,
    amrex::Real time_of_day,
    const DiurnalRoughnessParams& params)
{
    // Compute time-varying z₀
    const amrex::Real z0_t = compute_diurnal_z0(time_of_day, params);
    
    // Compute wind speed magnitude at reference height
    const amrex::Real u_ref_mag = std::sqrt(u_ref * u_ref + v_ref * v_ref);
    
    // Compute friction velocity from log-law
    // u* = κ·|U_ref| / ln((z_ref + z₀) / z₀)
    const amrex::Real kappa = amrex::Real(0.4);  // von Kármán constant
    const amrex::Real log_term = std::log((z_ref + z0_t) / z0_t);
    
    if (log_term > amrex::Real(1.0e-10)) {
        return kappa * u_ref_mag / log_term;
    } else {
        return amrex::Real(0.0);
    }
}

#endif // DIURNAL_ROUGHNESS_MODELS_H
