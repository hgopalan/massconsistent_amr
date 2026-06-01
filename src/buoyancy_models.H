// ==========================================================================
// buoyancy_models.H
// Thermal stratification and buoyancy effects for non-neutral conditions
// Implements simple Boussinesq approximation for buoyancy-driven flow
// ==========================================================================

#ifndef BUOYANCY_MODELS_H
#define BUOYANCY_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include "math_constants.H"
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
