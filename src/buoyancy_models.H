// ==========================================================================
// buoyancy_models.H
// Thermal stratification and buoyancy effects for non-neutral conditions
// Implements simple Boussinesq approximation for buoyancy-driven flow
// ==========================================================================

#ifndef BUOYANCY_MODELS_H
#define BUOYANCY_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include "solver_math_constants.H"
#include <cmath>

// ---------------------------------------------------------------------------
// Physical constants
// ---------------------------------------------------------------------------
namespace BuoyancyConstants {
    constexpr amrex::Real g = amrex::Real(9.81);  // gravitational acceleration [m/s²]
}

// ---------------------------------------------------------------------------
// Temperature interpolation utilities
// ---------------------------------------------------------------------------

// Linear interpolation between two temperature profile points
// z: query height [m]
// z1, T1: lower data point (height, temperature)
// z2, T2: upper data point (height, temperature)
// Returns: interpolated temperature [K]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real temperature_linear_interp(amrex::Real z,
                                      amrex::Real z1, amrex::Real T1,
                                      amrex::Real z2, amrex::Real T2) noexcept
{
    // Linear interpolation: T(z) = T1 + (T2-T1)*(z-z1)/(z2-z1)
    if (std::abs(z2 - z1) < amrex::Real(1.0e-10)) {
        return T1;  // avoid division by zero
    }
    
    amrex::Real frac = (z - z1) / (z2 - z1);
    frac = std::max(amrex::Real(0.0), std::min(amrex::Real(1.0), frac));
    
    return T1 + (T2 - T1) * frac;
}

// ---------------------------------------------------------------------------
// Buoyancy computation
// ---------------------------------------------------------------------------

// Compute buoyancy-induced vertical velocity using Boussinesq approximation
// Buoyancy term: ∂w/∂t ~ g(T-T₀)/T₀
// Integrated over characteristic time: w_buoyancy = g(T-T₀)/T₀ * Δt
//
// T: local temperature [K]
// T0: reference temperature [K]
// dt: characteristic time scale [s]
// coefficient: tuning parameter (default=1.0)
// Returns: buoyancy-induced vertical velocity [m/s]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real buoyancy_velocity(amrex::Real T,
                              amrex::Real T0,
                              amrex::Real dt,
                              amrex::Real coefficient = amrex::Real(1.0)) noexcept
{
    // Boussinesq approximation: w_buoyancy = g * (T - T0) / T0 * dt * coefficient
    // Positive when T > T0 (unstable, upward motion)
    // Negative when T < T0 (stable, downward motion)
    
    if (T0 < amrex::Real(1.0)) {
        // Invalid reference temperature, return zero
        return amrex::Real(0.0);
    }
    
    return BuoyancyConstants::g * (T - T0) / T0 * dt * coefficient;
}

// Compute reduced gravity for buoyancy effects
// g' = g * (T - T0) / T0
//
// T: local temperature [K]
// T0: reference temperature [K]
// Returns: reduced gravity [m/s²]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real reduced_gravity(amrex::Real T, amrex::Real T0) noexcept
{
    if (T0 < amrex::Real(1.0)) {
        return amrex::Real(0.0);
    }
    
    return BuoyancyConstants::g * (T - T0) / T0;
}

// Compute buoyancy source term for RHS of lambda equation
// Source term: g(T-T₀)/T₀
// This represents the divergence contribution from buoyancy effects
//
// T: local temperature [K]
// T0: reference temperature [K]
// coefficient: tuning parameter (default=1.0)
// Returns: buoyancy source term [1/s] for RHS
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real buoyancy_rhs_term(amrex::Real T,
                              amrex::Real T0,
                              amrex::Real coefficient = amrex::Real(1.0)) noexcept
{
    // Buoyancy source for RHS: g * (T - T0) / T0 * coefficient
    // Positive when T > T0 (unstable, upward motion)
    // Negative when T < T0 (stable, downward motion)
    
    if (T0 < amrex::Real(1.0)) {
        // Invalid reference temperature, return zero
        return amrex::Real(0.0);
    }
    
    return BuoyancyConstants::g * (T - T0) / T0 * coefficient;
}

// ---------------------------------------------------------------------------
// Stability assessment from temperature gradient
// ---------------------------------------------------------------------------

// Compute Brunt-Väisälä frequency squared from temperature gradient
// N² = (g/T₀) * (dT/dz + Γ_d)
// where Γ_d = g/c_p ≈ 0.0098 K/m is the dry adiabatic lapse rate
//
// dT_dz: vertical temperature gradient [K/m]
// T0: reference temperature [K]
// Returns: N² [1/s²] (positive = stable, negative = unstable, ~0 = neutral)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real brunt_vaisala_squared(amrex::Real dT_dz, amrex::Real T0) noexcept
{
    const amrex::Real gamma_d = amrex::Real(0.0098);  // dry adiabatic lapse rate [K/m]
    
    if (T0 < amrex::Real(1.0)) {
        return amrex::Real(0.0);
    }
    
    return (BuoyancyConstants::g / T0) * (dT_dz + gamma_d);
}

// ---------------------------------------------------------------------------
// Diurnal temperature variations
// ---------------------------------------------------------------------------

// Simple diurnal (sinusoidal) temperature profile
// T(z,t) = T_mean(z) + ΔT * sin(2π * (t - t_phase) / period)
// where:
//   T_mean: mean temperature at height z [K]
//   delta_T: diurnal temperature amplitude [K]
//   time_of_day: current time [hours, 0-24]
//   t_phase: time of maximum temperature [hours, default 14:00]
//   period: diurnal period [hours, default 24]
//
// Returns: Temperature at height z and time t [K]
//
// Typical usage:
//   - Morning (6-12h): stable → neutral → unstable transition
//   - Afternoon (12-18h): unstable, maximum heating
//   - Evening (18-24h): unstable → neutral → stable transition
//   - Night (0-6h): stable, minimum temperature
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real diurnal_temperature(amrex::Real T_mean,
                                amrex::Real delta_T,
                                amrex::Real time_of_day,
                                amrex::Real t_phase = amrex::Real(14.0),
                                amrex::Real period = amrex::Real(24.0)) noexcept
{
    // Sinusoidal variation with specified amplitude
    // Phase shift to put maximum at t_phase (typically 14:00 for solar heating)
    const amrex::Real two_pi = amrex::Real(2.0) * MathConstants::pi;
    amrex::Real phase = two_pi * (time_of_day - t_phase) / period;
    
    return T_mean + delta_T * std::sin(phase);
}

// ---------------------------------------------------------------------------
// Vegetation effects on roughness
// ---------------------------------------------------------------------------

// Vegetation attenuation factor for roughness length
// z0_effective = f_veg * z0_base
// where f_veg varies with vegetation state (LAI, snow, burn severity)
//
// veg_state: vegetation state parameter (e.g., LAI, snow depth, burn fraction)
// state_type: type of vegetation state modifier
//   0 = LAI-based (Leaf Area Index, 0-8)
//   1 = snow depth-based (depth in meters, smooths surface)
//   2 = burn severity (0-1, reduces canopy roughness)
//   3 = crop growth stage (0-1, increases roughness during growth)
//
// Returns: attenuation factor f_veg ∈ [0.5, 1.5]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real vegetation_roughness_factor(amrex::Real veg_state,
                                        int state_type = 0) noexcept
{
    amrex::Real f_veg = amrex::Real(1.0);  // Default: no modification
    
    if (state_type == 0) {
        // LAI-based: higher LAI → higher roughness
        // Typical LAI: 0 (bare), 1-2 (grassland), 3-6 (forest), >6 (dense canopy)
        // f_veg = 0.5 + 0.1 * LAI (capped at 1.5)
        f_veg = amrex::Real(0.5) + amrex::Real(0.1) * veg_state;
        f_veg = std::min(f_veg, amrex::Real(1.5));
        f_veg = std::max(f_veg, amrex::Real(0.5));
        
    } else if (state_type == 1) {
        // Snow depth-based: snow smooths surface, reduces roughness
        // Snow depth in meters: 0 (no snow) → 1.0 (deep snow)
        // f_veg = 1.0 - 0.5 * min(depth, 1.0)
        amrex::Real snow_frac = std::min(veg_state, amrex::Real(1.0));
        f_veg = amrex::Real(1.0) - amrex::Real(0.5) * snow_frac;
        f_veg = std::max(f_veg, amrex::Real(0.5));
        
    } else if (state_type == 2) {
        // Burn severity: fire damage reduces canopy, lowers roughness
        // Burn severity 0-1: 0 (unburned) → 1 (complete burn)
        // f_veg = 1.0 - 0.4 * burn_severity
        f_veg = amrex::Real(1.0) - amrex::Real(0.4) * veg_state;
        f_veg = std::max(f_veg, amrex::Real(0.5));
        
    } else if (state_type == 3) {
        // Crop growth stage: increases roughness as crops grow
        // Growth stage 0-1: 0 (bare soil) → 1 (mature crop)
        // f_veg = 0.6 + 0.6 * growth_stage
        f_veg = amrex::Real(0.6) + amrex::Real(0.6) * veg_state;
        f_veg = std::min(f_veg, amrex::Real(1.5));
    }
    
    return f_veg;
}

// ---------------------------------------------------------------------------
// Terrain gradient utilities
// ---------------------------------------------------------------------------

// Compute terrain-following kinematic boundary condition
// At terrain surface: w = u·∇h = u*(∂h/∂x) + v*(∂h/∂y)
//
// u: horizontal velocity in x-direction [m/s]
// v: horizontal velocity in y-direction [m/s]
// dh_dx: terrain gradient in x-direction [dimensionless]
// dh_dy: terrain gradient in y-direction [dimensionless]
// relaxation: relaxation factor (1.0 = strict, <1.0 = relaxed)
// Returns: vertical velocity at terrain surface [m/s]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real terrain_kinematic_w(amrex::Real u,
                                amrex::Real v,
                                amrex::Real dh_dx,
                                amrex::Real dh_dy,
                                amrex::Real relaxation = amrex::Real(1.0)) noexcept
{
    // Kinematic boundary condition: w = u·∇h
    // This ensures no flow through the terrain surface
    return relaxation * (u * dh_dx + v * dh_dy);
}

#endif // BUOYANCY_MODELS_H
