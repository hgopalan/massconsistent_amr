// ============================================================================
// dense_gas_models.H
// SLAB/UGC-based dense gas dispersion model for hazardous material releases
//
// This header provides functions for computing dense gas dispersion using
// the Source-Layer Analysis with Buoyancy (SLAB) and Upper-layer Gravity-driven
// Current (UGC) approach, designed for hazmat applications (CO₂, HF, Cl₂, NH₃).
//
// Physical Effects:
// - Density-ratio tracking: ρ_gas / ρ_air
// - Froude number regime detection: Fr = U / sqrt(g' × H)
// - SLAB layer height decay: H(x) = H₀ × [1-(x/x_max)^(2/3)]
// - Two-layer concentration: dense lower layer + buoyant upper layer
// - Lateral gravity-driven spreading
// - Vertical buoyancy-driven mixing
//
// Model Assumptions:
// - Assumes incompressible flow (valid for low-speed dense gas flow)
// - Gaussian horizontal distribution at initial stages
// - Height-dependent density stratification
// - Gravity-dominated spreading in near-field
// - Transition to passive Gaussian plume in far-field
//
// References:
// - Saiz-Lopez, A., et al. (2012). Atmospheric Chemistry Review
// - Britter, R.E., & McQuaid, J. (1988). Workbook on the dispersion of
//   dense gas releases. HSE Contract Research Report No. 17/1988
// - USEPA ALOHA (Areal Locations of Hazardous Atmospheres) model
// - Havens, J.A., & Spicer, T.O. (1985). Development of practical models
//   for toxic chemical hazards assessment. Journal of Hazardous Materials
// ============================================================================

#ifndef DENSE_GAS_MODELS_H
#define DENSE_GAS_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>

// ============================================================================
// Physical Constants
// ============================================================================
namespace DenseGas {
    // Standard atmospheric parameters
    static constexpr amrex::Real GRAVITY = 9.81;           // Gravitational acceleration [m/s²]
    static constexpr amrex::Real RAIR_SPECIFIC = 287.0;    // Specific gas constant for air [J/(kg·K)]
    static constexpr amrex::Real TSTD = 298.15;            // Standard temperature [K]
    static constexpr amrex::Real PSTD = 101325.0;          // Standard pressure [Pa]
    static constexpr amrex::Real RHOSTD = 1.225;           // Standard air density [kg/m³] at 15°C

    // Common hazmat species (molecular weight in g/mol)
    static constexpr amrex::Real MW_CO2 = 44.01;           // Carbon dioxide
    static constexpr amrex::Real MW_HF = 20.01;            // Hydrogen fluoride (DENSER)
    static constexpr amrex::Real MW_CL2 = 70.90;           // Chlorine (DENSER)
    static constexpr amrex::Real MW_NH3 = 17.03;           // Ammonia (LIGHTER)
    static constexpr amrex::Real MW_AIR = 28.97;           // Air
}

// ============================================================================
// Dense Gas Species Properties
// ============================================================================
enum class DenseGasSpecies : int {
    PASSIVE = 0,      // Passive tracer (for testing)
    CO2 = 1,          // Carbon dioxide
    HF = 2,           // Hydrogen fluoride
    CL2 = 3,          // Chlorine
    NH3 = 4,          // Ammonia
    CUSTOM = 5        // User-defined species
};

// ============================================================================
// Compute gas density from molecular weight at standard conditions
//
// Using ideal gas law: ρ = (P × MW) / (R_universal × T)
// where R_universal = 8.314 J/(mol·K)
//
// Inputs:
//   molecular_weight - Molar mass [g/mol]
//   temperature      - Temperature [K] (default: standard 298.15 K)
//
// Returns:
//   Density [kg/m³]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_gas_density(
    amrex::Real molecular_weight,
    amrex::Real temperature = DenseGas::TSTD)
{
    // Ideal gas law: ρ = (P × M) / (R × T)
    // R = 8.314 J/(mol·K)
    const amrex::Real R_universal = 8.314;
    const amrex::Real density = (DenseGas::PSTD * (molecular_weight / 1000.0)) /
                                (R_universal * temperature);
    return amrex::max(density, amrex::Real(1.0e-10));
}

// ============================================================================
// Compute density ratio for hazmat species
//
// The density ratio ρ_gas / ρ_air determines the dominant dispersion mechanism:
// - ρ_ratio > 1.0: Dense gas (sinks and spreads laterally, e.g., CO₂, Cl₂, HF)
// - ρ_ratio ≈ 1.0: Passive gas (normal Gaussian plume)
// - ρ_ratio < 1.0: Light gas (rises, buoyant plume, e.g., NH₃ at high T)
//
// Inputs:
//   gas_density      - Density of released gas [kg/m³]
//   air_density      - Density of ambient air [kg/m³] (default: standard)
//
// Returns:
//   Density ratio [dimensionless]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_density_ratio(
    amrex::Real gas_density,
    amrex::Real air_density = DenseGas::RHOSTD)
{
    if (air_density <= amrex::Real(1.0e-10)) {
        return amrex::Real(1.0);
    }
    return gas_density / air_density;
}

// ============================================================================
// Compute reduced gravity for dense gas (Boussinesq approximation)
//
// The reduced (or buoyancy-modified) gravity g' drives gravity-driven spreading:
// g' = g × (ρ_gas - ρ_air) / ρ_air
//
// For dense gas (ρ_gas > ρ_air): g' > 0, drives downward spreading
// For light gas (ρ_gas < ρ_air): g' < 0, drives upward plume rise
//
// Inputs:
//   density_ratio    - ρ_gas / ρ_air
//   gravity          - Gravitational acceleration [m/s²] (default: 9.81)
//
// Returns:
//   Reduced gravity [m/s²]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_reduced_gravity(
    amrex::Real density_ratio,
    amrex::Real gravity = DenseGas::GRAVITY)
{
    return gravity * (density_ratio - amrex::Real(1.0));
}

// ============================================================================
// Froude number calculation
//
// The Froude number Fr = U / sqrt(g' × H) determines the dominant spreading mode:
// - Fr >> 1: Wind-dominated (plume extends downwind)
// - Fr ~ 1: Transitional regime (balanced gravity and wind effects)
// - Fr << 1: Gravity-dominated (spreads laterally, slow downwind motion)
//
// Inputs:
//   wind_speed       - Ambient wind speed [m/s]
//   reduced_gravity  - g' = g × (ρ_ratio - 1) [m/s²]
//   layer_height     - Current SLAB layer height [m]
//   min_height       - Avoid singularities (default: 0.1 m)
//
// Returns:
//   Froude number [dimensionless]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_froude_number(
    amrex::Real wind_speed,
    amrex::Real reduced_gravity,
    amrex::Real layer_height,
    amrex::Real min_height = 0.1)
{
    const amrex::Real H = amrex::max(layer_height, min_height);
    const amrex::Real g_prime = amrex::abs(reduced_gravity);
    
    if (g_prime <= amrex::Real(1.0e-10) || wind_speed <= amrex::Real(1.0e-10)) {
        return amrex::Real(1.0);  // Return neutral value for no density difference
    }
    
    const amrex::Real sqrt_g_H = std::sqrt(g_prime * H);
    return wind_speed / sqrt_g_H;
}

// ============================================================================
// SLAB layer height decay formula
//
// The Britter-McQuaid SLAB model describes dense gas layer height evolution:
// H(x) = H₀ × [1 - (x / x_max)^n]
//
// where:
// - H₀ = initial layer height [m]
// - x = downwind distance [m]
// - x_max = characteristic decay length scale [m]
// - n = power-law exponent (typically 2/3 for SLAB)
//
// The layer height decays as gravity-driven spreading causes lateral expansion
// and atmospheric entrainment mixes in lighter air from above.
//
// Inputs:
//   distance         - Downwind distance from source [m]
//   H0               - Initial layer height [m]
//   x_max            - Characteristic decay scale [m]
//   exponent         - Power-law exponent (default: 2/3)
//
// Returns:
//   Current layer height [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real slab_layer_height(
    amrex::Real distance,
    amrex::Real H0,
    amrex::Real x_max,
    amrex::Real exponent = 2.0/3.0)
{
    if (x_max <= amrex::Real(1.0e-10) || H0 <= amrex::Real(1.0e-10)) {
        return H0;
    }
    
    const amrex::Real x_norm = amrex::max(distance, amrex::Real(0.0)) / x_max;
    
    // Avoid overflow in power function
    if (x_norm >= amrex::Real(1.0)) {
        return amrex::Real(0.1) * H0;  // Return small non-zero height
    }
    
    const amrex::Real decay_factor = std::pow(amrex::Real(1.0) - x_norm, exponent);
    return H0 * amrex::max(decay_factor, amrex::Real(0.001));
}

// ============================================================================
// Characteristic decay length scale x_max
//
// The SLAB model x_max represents the distance scale over which gravity-driven
// spreading dominates. It depends on the initial layer height, density ratio,
// and wind speed through empirical correlations.
//
// Britter-McQuaid correlation:
// x_max = C × sqrt(g' × H₀³) / U
//
// where C ≈ 1.0-2.0 (empirical tuning parameter)
//
// Inputs:
//   reduced_gravity  - g' = g × (ρ_ratio - 1) [m/s²]
//   H0               - Initial layer height [m]
//   wind_speed       - Wind speed [m/s] (minimum: 0.1 m/s)
//   coefficient      - Empirical coefficient (default: 1.5)
//
// Returns:
//   Characteristic decay scale [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_slab_decay_scale(
    amrex::Real reduced_gravity,
    amrex::Real H0,
    amrex::Real wind_speed,
    amrex::Real coefficient = 1.5)
{
    const amrex::Real g_prime = amrex::abs(reduced_gravity);
    const amrex::Real U = amrex::max(wind_speed, amrex::Real(0.1));
    const amrex::Real H = amrex::max(H0, amrex::Real(0.1));
    
    // x_max = C × sqrt(g' × H³) / U
    const amrex::Real numerator = std::sqrt(g_prime * H * H * H);
    return coefficient * numerator / U;
}

// ============================================================================
// Two-layer concentration model
//
// Dense gas dispersion exhibits distinct concentration profiles:
// 1. Dense lower layer (0 to z_layer): high concentration, gravity-dominated
// 2. Upper buoyant layer (z_layer to z_top): transitional concentration
//
// The lower layer concentration follows SLAB theory with horizontal spreading.
// The upper layer concentration decays exponentially.
//
// Inputs:
//   z                - Current height [m]
//   z_layer          - Dense layer top height [m]
//   C_dense_center   - Center concentration of dense layer [units/m³]
//   C_upper_scale    - Scale concentration for upper layer [units/m³]
//   K_z_upper        - Vertical mixing scale for upper layer [m]
//   sigma_y          - Lateral standard deviation [m]
//   lateral_dist     - Lateral distance from plume axis [m]
//
// Returns:
//   Concentration [units/m³]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real two_layer_concentration(
    amrex::Real z,
    amrex::Real z_layer,
    amrex::Real C_dense_center,
    amrex::Real C_upper_scale,
    amrex::Real K_z_upper,
    amrex::Real sigma_y,
    amrex::Real lateral_dist)
{
    // Lateral Gaussian profile (both layers)
    const amrex::Real lateral_factor = std::exp(-(lateral_dist * lateral_dist) /
                                                (2.0 * sigma_y * sigma_y + 1.0e-10));
    
    amrex::Real concentration = amrex::Real(0.0);
    
    if (z <= z_layer) {
        // Dense lower layer: uniform vertical profile
        concentration = C_dense_center * lateral_factor;
    } else {
        // Upper buoyant layer: exponential decay with height
        const amrex::Real z_above = z - z_layer;
        if (K_z_upper > amrex::Real(1.0e-10)) {
            concentration = C_upper_scale * lateral_factor *
                          std::exp(-z_above / K_z_upper);
        } else {
            concentration = amrex::Real(0.0);
        }
    }
    
    return amrex::max(concentration, amrex::Real(0.0));
}

// ============================================================================
// Lateral gravity-driven spreading
//
// Dense gas spreads laterally due to gravity. The spreading width grows
// as a function of downwind distance according to:
// σ_y(x) = σ_y0 + C_spread × sqrt(g' × H) × x
//
// where:
// - σ_y0 = initial lateral width [m]
// - C_spread ≈ 0.1-0.2 (empirical spreading coefficient)
// - g' = reduced gravity [m/s²]
// - H = current layer height [m]
// - x = downwind distance [m]
//
// Inputs:
//   distance         - Downwind distance [m]
//   sigma_y0         - Initial lateral width [m]
//   reduced_gravity  - g' = g × (ρ_ratio - 1) [m/s²]
//   layer_height     - Current SLAB layer height [m]
//   coefficient      - Spreading coefficient (default: 0.15)
//
// Returns:
//   Current lateral standard deviation [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real lateral_spreading_slab(
    amrex::Real distance,
    amrex::Real sigma_y0,
    amrex::Real reduced_gravity,
    amrex::Real layer_height,
    amrex::Real coefficient = 0.15)
{
    const amrex::Real g_prime = amrex::abs(reduced_gravity);
    const amrex::Real H = amrex::max(layer_height, amrex::Real(0.01));
    const amrex::Real x = amrex::max(distance, amrex::Real(0.0));
    
    // Spreading rate: sqrt(g' × H) has units [m/s]
    const amrex::Real spreading_rate = std::sqrt(g_prime * H);
    
    // Total lateral width
    const amrex::Real sigma_y = sigma_y0 + coefficient * spreading_rate * x;
    
    return amrex::max(sigma_y, amrex::Real(0.1));
}

// ============================================================================
// Vertical mixing depth in dense gas layer
//
// The depth of active mixing in the dense layer grows with distance due to
// entrainment of ambient air:
// h_mix(x) = h_mix0 + β × sqrt(g' × H₀) × x / U
//
// where β is an entrainment coefficient.
//
// Inputs:
//   distance         - Downwind distance [m]
//   h_mix0           - Initial mixing depth [m]
//   reduced_gravity  - g' = g × (ρ_ratio - 1) [m/s²]
//   H0               - Initial layer height [m]
//   wind_speed       - Wind speed [m/s]
//   beta             - Entrainment coefficient (default: 0.1)
//
// Returns:
//   Mixing depth [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real vertical_mixing_depth(
    amrex::Real distance,
    amrex::Real h_mix0,
    amrex::Real reduced_gravity,
    amrex::Real H0,
    amrex::Real wind_speed,
    amrex::Real beta = 0.1)
{
    const amrex::Real g_prime = amrex::abs(reduced_gravity);
    const amrex::Real U = amrex::max(wind_speed, amrex::Real(0.01));
    const amrex::Real H = amrex::max(H0, amrex::Real(0.1));
    const amrex::Real x = amrex::max(distance, amrex::Real(0.0));
    
    const amrex::Real entrainment_rate = beta * std::sqrt(g_prime * H) / U;
    return h_mix0 + entrainment_rate * x;
}

// ============================================================================
// Identify the dense gas regime based on Froude number
//
// Returns a regime classification:
// - GRAVITY_DOMINATED (Fr < 0.5): Lateral spreading dominates
// - TRANSITIONAL (0.5 ≤ Fr ≤ 2.0): Mixed gravity and wind effects
// - WIND_DOMINATED (Fr > 2.0): Wind transport dominates
// ============================================================================
enum class DenseGasRegime : int {
    GRAVITY_DOMINATED = 0,
    TRANSITIONAL = 1,
    WIND_DOMINATED = 2
};

AMREX_GPU_HOST_DEVICE AMREX_INLINE
DenseGasRegime classify_regime(amrex::Real froude_number)
{
    if (froude_number < amrex::Real(0.5)) {
        return DenseGasRegime::GRAVITY_DOMINATED;
    } else if (froude_number < amrex::Real(2.0)) {
        return DenseGasRegime::TRANSITIONAL;
    } else {
        return DenseGasRegime::WIND_DOMINATED;
    }
}

// ============================================================================
// Transition to passive Gaussian plume
//
// At large downwind distances, the dense gas layer dissipates and the plume
// transitions to passive Gaussian dispersion. This occurs when:
// - Layer height becomes comparable to initial puff size
// - Froude number becomes large (wind dominates)
//
// Returns:
//   Transition factor: 0.0 (pure SLAB) to 1.0 (pure Gaussian)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_transition_factor(
    amrex::Real layer_height,
    amrex::Real initial_height,
    amrex::Real froude_number,
    amrex::Real distance,
    amrex::Real x_transition = 5000.0)
{
    // Transition based on layer height erosion
    const amrex::Real height_factor = amrex::min(
        amrex::Real(1.0),
        amrex::max(amrex::Real(0.0),
                   (initial_height - layer_height) / initial_height)
    );
    
    // Transition based on Froude number (wind dominance)
    const amrex::Real froude_factor = amrex::min(
        amrex::Real(1.0),
        amrex::max(amrex::Real(0.0),
                   (froude_number - amrex::Real(1.0)) / amrex::Real(3.0))
    );
    
    // Transition based on downwind distance
    const amrex::Real distance_factor = amrex::min(
        amrex::Real(1.0),
        amrex::max(amrex::Real(0.0),
                   distance / x_transition)
    );
    
    // Combine factors (geometric mean)
    return std::pow(height_factor * froude_factor * distance_factor, 1.0/3.0);
}

#endif // DENSE_GAS_MODELS_H
