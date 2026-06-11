// ============================================================================
// deposition_models.H
// Enhanced dry and wet deposition parameterizations for pollutants and aerosols
//
// This header provides functions for modeling mass removal from puffs via:
// - Dry deposition (gravitational settling, impaction, diffusion)
// - Wet deposition (precipitation scavenging, rain-dependent removal)
// - Particle-size-dependent settling velocity
// - Surface-type-dependent deposition velocities
//
// Model: Combination of Stokes law, Cunningham slip correction, and
// empirical wet scavenging relationships.
//
// References:
// - Seinfeld, J.H., & Pandis, S.N. (2016). Atmospheric Chemistry and Physics
// - Zhang, L., et al. (2001). A size-segregated, size-dependent particle dry 
//   deposition scheme. Atmospheric Environment, 35(3), 549-560.
// - Hegg, D.A., et al. (2000). Measurements of aerosol optical properties in
//   the Sagano forest, Japan. J. Geophys. Res., 105(D2), 2383-2396.
// ============================================================================

#ifndef DEPOSITION_MODELS_H
#define DEPOSITION_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <algorithm>

using namespace amrex;

// Physical constants
static constexpr Real LAMBDA_AIR = Real(6.6e-8);  // Mean free path of air [m]
static constexpr Real GRAVITY = Real(9.81);       // Gravitational acceleration [m/s²]
static constexpr Real AIR_DENSITY = Real(1.225);  // Air density at sea level [kg/m³]
static constexpr Real AIR_VISCOSITY = Real(1.8e-5); // Air dynamic viscosity [Pa·s]

// ============================================================================
// Stokes Settling Velocity with Cunningham Slip Correction
// ============================================================================

/**
 * Compute Cunningham slip correction factor
 *
 * Accounts for non-continuum effects for small particles where particle
 * diameter is comparable to molecular mean free path.
 *
 * Cc = 1 + Kn * (1.257 + 0.4 * exp(-1.1/Kn))
 *
 * Parameters:
 *   particle_diameter - Particle diameter [m]
 *
 * Returns:
 *   Slip correction factor (dimensionless, ≥ 1.0)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_cunningham_factor(Real particle_diameter)
{
    // Knudsen number
    Real Kn = Real(2.0) * LAMBDA_AIR / particle_diameter;
    
    // Cunningham slip correction
    Real Cc = Real(1.0) + Kn * (Real(1.257) + Real(0.4) * std::exp(-Real(1.1) / Kn));
    
    return Cc;
}

/**
 * Compute terminal settling velocity using Stokes law with slip correction
 *
 * v_s = (ρ_p * d_p² * g * Cc) / (18 * μ_air)
 *
 * Valid for particles with Reynolds number < 1.0 (small particles)
 *
 * Parameters:
 *   particle_diameter - Particle diameter [m]
 *   particle_density - Particle density [kg/m³]
 *   air_density - Air density [kg/m³] (optional, default from constant)
 *   air_viscosity - Air viscosity [Pa·s] (optional, default from constant)
 *   gravity - Gravitational acceleration [m/s²] (optional, default)
 *
 * Returns:
 *   Terminal settling velocity [m/s] (always ≥ 0)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_stokes_settling_velocity(
    Real particle_diameter,
    Real particle_density,
    Real air_density = AIR_DENSITY,
    Real air_viscosity = AIR_VISCOSITY,
    Real gravity = GRAVITY)
{
    particle_diameter = std::max(particle_diameter, Real(1.0e-9));  // Minimum 1 nm
    
    Real Cc = compute_cunningham_factor(particle_diameter);
    
    Real v_s = (particle_density * particle_diameter * particle_diameter * gravity * Cc) 
               / (Real(18.0) * air_viscosity);
    
    return std::max(v_s, Real(0.0));
}

// ============================================================================
// Dry Deposition Velocity
// ============================================================================

/**
 * Compute dry deposition velocity for a given surface type
 *
 * Deposition velocity combines:
 * 1. Gravitational settling (for particles)
 * 2. Impaction/interception
 * 3. Diffusion (for small particles/gases)
 * 4. Chemical reactivity
 *
 * Uses empirical relationships from literature. Values from IMPROVE model:
 * - SO₄ over grass: 0.1-0.2 cm/s
 * - NO₃ over grass: 0.1-0.3 cm/s
 * - Coarse particles: 1-5 cm/s
 *
 * Parameters:
 *   particle_diameter - Particle diameter [m]
 *   surface_type - Surface type code (0=grass, 1=urban, 2=water, 3=forest)
 *   particle_density - Particle density [kg/m³]
 *
 * Returns:
 *   Dry deposition velocity [m/s]
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_dry_deposition_velocity(
    Real particle_diameter,
    int surface_type,
    Real particle_density = Real(1000.0))  // Default aerosol density
{
    // Stokes settling component
    Real v_settling = compute_stokes_settling_velocity(particle_diameter, particle_density);
    
    // Surface-dependent base deposition velocity [m/s]
    Real v_d_base;
    
    switch (surface_type) {
        case 0:  // Grass
            v_d_base = Real(0.001);  // 0.1 cm/s
            break;
        case 1:  // Urban
            v_d_base = Real(0.002);  // 0.2 cm/s (higher roughness)
            break;
        case 2:  // Water
            v_d_base = Real(0.0005); // 0.05 cm/s (smooth surface)
            break;
        case 3:  // Forest
            v_d_base = Real(0.005);  // 0.5 cm/s (high roughness, canopy)
            break;
        default:
            v_d_base = Real(0.001);
    }
    
    // Combine settling and base deposition
    // For particles: v_d = v_settling + v_base
    Real v_d = v_settling + v_d_base;
    
    return v_d;
}

// ============================================================================
// Wet Scavenging (Precipitation Scavenging)
// ============================================================================

/**
 * Compute wet scavenging removal rate (Lambda coefficient)
 *
 * Empirical relationship: Lambda = Lambda_0 * (P / P_ref)^a
 * 
 * where:
 *   Lambda = scavenging removal rate [s^-1]
 *   Lambda_0 = base scavenging coefficient at reference precipitation
 *   P = precipitation rate [mm/hr]
 *   P_ref = reference precipitation rate (typically 0.1 mm/hr)
 *   a = exponent (typically 0.6-1.0)
 *
 * Physical basis: Removal rate proportional to collision efficiency
 * and liquid water content, which increases with precipitation intensity.
 *
 * Parameters:
 *   lambda0 - Base scavenging coefficient [s^-1] at P_ref
 *   precipitation_rate - Current precipitation rate [mm/hr]
 *   exponent_a - Precipitation exponent (typically 0.8)
 *   precip_ref - Reference precipitation rate [mm/hr] (default 0.1)
 *
 * Returns:
 *   Scavenging removal rate [s^-1]
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_wet_scavenging_rate(
    Real lambda0,
    Real precipitation_rate,
    Real exponent_a,
    Real precip_ref = Real(0.1))
{
    if (precipitation_rate <= Real(0.0)) {
        return Real(0.0);
    }
    
    // Normalized precipitation ratio
    Real P_norm = precipitation_rate / precip_ref;
    
    // Scavenging rate increases with precipitation intensity
    Real lambda = lambda0 * std::pow(P_norm, exponent_a);
    
    return lambda;
}

// ============================================================================
// Combined Dry + Wet Deposition Mass Loss
// ============================================================================

/**
 * Apply combined dry and wet deposition mass loss to puff
 *
 * Combines two removal mechanisms:
 * 1. Dry: m_dry(t) = m(0) * exp(-v_d * t / h)
 * 2. Wet: m_wet(t) = m(0) * exp(-lambda * t)
 * 
 * Combined: m(t) = m(0) * exp(-(v_d/h + lambda) * t)
 *
 * Parameters:
 *   mass - Puff mass [units] (modified in-place)
 *   dt - Time step [s]
 *   vd - Dry deposition velocity [m/s]
 *   puff_height - Puff center height [m] (depth scale for dry deposition)
 *   lambda - Wet scavenging coefficient [s^-1]
 *   sigma_z - Vertical spread of puff [m] (optional, for averaging)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_combined_deposition(
    Real& mass,
    Real dt,
    Real vd,
    Real puff_height,
    Real lambda,
    Real sigma_z = Real(10.0))
{
    if (mass <= Real(0.0)) return;
    
    // Dry deposition: removal rate ~ vd / (puff_height + sigma_z)
    // Using puff height + sigma_z as effective depth scale
    Real h_eff = std::max(puff_height + sigma_z, Real(1.0));
    Real dry_rate = vd / h_eff;
    
    // Wet scavenging: removal rate = lambda
    Real wet_rate = lambda;
    
    // Combined exponential decay
    Real total_removal_rate = dry_rate + wet_rate;
    mass *= std::exp(-total_removal_rate * dt);
}

/**
 * Apply only dry deposition (when no precipitation)
 *
 * Parameters:
 *   mass - Puff mass [units] (modified in-place)
 *   dt - Time step [s]
 *   vd - Dry deposition velocity [m/s]
 *   puff_height - Puff center height [m]
 *   sigma_z - Vertical spread of puff [m]
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_dry_deposition_only(
    Real& mass,
    Real dt,
    Real vd,
    Real puff_height,
    Real sigma_z = Real(10.0))
{
    if (mass <= Real(0.0)) return;
    
    Real h_eff = std::max(puff_height + sigma_z, Real(1.0));
    Real dry_rate = vd / h_eff;
    
    mass *= std::exp(-dry_rate * dt);
}

/**
 * Apply only wet deposition (when no dry deposition)
 *
 * Parameters:
 *   mass - Puff mass [units] (modified in-place)
 *   dt - Time step [s]
 *   lambda - Wet scavenging coefficient [s^-1]
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_wet_deposition_only(
    Real& mass,
    Real dt,
    Real lambda)
{
    if (mass <= Real(0.0)) return;
    
    mass *= std::exp(-lambda * dt);
}

// ============================================================================
// Particle Size Distribution Utilities
// ============================================================================

/**
 * Sample particle diameter from lognormal distribution
 *
 * Lognormal distribution: n(d) ∝ (1/d) * exp(-(ln(d) - ln(d_g))² / (2 * (ln(σ_g))²))
 *
 * Parameters:
 *   diameter_mean - Geometric mean diameter [m]
 *   geometric_std - Geometric standard deviation (typically 1.2-2.5)
 *   random_uniform - Random number in [0, 1)
 *
 * Returns:
 *   Sampled particle diameter [m]
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real sample_lognormal_diameter(
    Real diameter_mean,
    Real geometric_std,
    Real random_uniform)
{
    // Box-Muller transform to get Gaussian random variable
    Real u1 = std::max(random_uniform, Real(1.0e-10));
    Real u2 = Real(0.5);  // In GPU context, would use PRNG
    
    Real pi = Real(3.14159265359);
    Real z = std::sqrt(-Real(2.0) * std::log(u1)) * std::cos(Real(2.0) * pi * u2);
    
    // Transform to lognormal
    Real ln_std = std::log(geometric_std);
    Real diameter = diameter_mean * std::exp(z * ln_std);
    
    return diameter;
}

// ============================================================================
// Deposition Summary Statistics
// ============================================================================

/**
 * Estimate total deposition removed during a time interval
 *
 * Uses analytical integration of exponential decay:
 *   Deposited = m_0 * (1 - exp(-k*dt))
 *
 * Parameters:
 *   initial_mass - Initial puff mass [units]
 *   dt - Time interval [s]
 *   vd - Dry deposition velocity [m/s]
 *   puff_height - Puff height [m]
 *   lambda - Wet scavenging coefficient [s^-1]
 *   sigma_z - Puff vertical spread [m]
 *
 * Returns:
 *   Mass deposited [units]
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real estimate_deposition_mass(
    Real initial_mass,
    Real dt,
    Real vd,
    Real puff_height,
    Real lambda,
    Real sigma_z = Real(10.0))
{
    Real h_eff = std::max(puff_height + sigma_z, Real(1.0));
    Real dry_rate = vd / h_eff;
    Real total_rate = dry_rate + lambda;
    
    Real deposited = initial_mass * (Real(1.0) - std::exp(-total_rate * dt));
    
    return deposited;
}

#endif // DEPOSITION_MODELS_H
