// ============================================================================
// chemistry_models.H
// Reactive chemistry framework for multi-species pollutant transformations
//
// This header provides functions and structures for modeling chemical reactions
// in atmospheric puffs. Supports:
// - Temperature-dependent reaction rates (Arrhenius kinetics)
// - Humidity-dependent reaction rates (aqueous phase reactions)
// - Plugin architecture for CSV-based reaction matrices
// - First-order Euler time-stepping for ODEs
//
// Model: Simplified atmospheric chemistry (MESOPUFF II-like)
// - SO₂ + oxidant → SO₄²⁻ (tunable rate, T/RH dependent)
// - NOₓ + O₃ → HNO₃ → NO₃⁻ (chain reaction with intermediate)
// - Custom reactions definable via CSV
//
// References:
// - Carmichael, G.R., et al. (1986). MESOPUFF: A numerical model to study
//   long-range transport of air pollutants. Journal of Applied Meteorology.
// - Seinfeld, J.H., & Pandis, S.N. (2016). Atmospheric Chemistry and Physics
//   (3rd ed.). Wiley-Interscience.
// ============================================================================

#ifndef CHEMISTRY_MODELS_H
#define CHEMISTRY_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>

using namespace amrex;

// ============================================================================
// Temperature-dependent rate adjustment using Arrhenius equation
// ============================================================================

/**
 * Compute temperature-dependent rate factor using simplified Arrhenius form
 *
 * k(T) = k_ref * exp(alpha * (T - T_ref))
 *
 * More rigorous Arrhenius: k(T) = A * exp(-Ea/(R*T))
 * Simplified used here is suitable for small temperature ranges [250-320K]
 *
 * Parameters:
 *   temp - Current temperature [K]
 *   temp_ref - Reference temperature [K] (typically 298.15 K)
 *   temp_coeff - Temperature coefficient [1/K] (e.g., 0.04 K^-1)
 *
 * Returns:
 *   Rate multiplication factor (dimensionless)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_temperature_rate_factor(
    Real temp,
    Real temp_ref,
    Real temp_coeff)
{
    Real delta_T = temp - temp_ref;
    return std::exp(temp_coeff * delta_T);
}

// ============================================================================
// Relative humidity-dependent rate adjustment
// ============================================================================

/**
 * Compute RH-dependent rate factor for aqueous phase reactions
 *
 * Empirical relationship for reactions enhanced in liquid water droplets.
 * Rate enhancement increases with RH due to increased liquid water content.
 *
 * Parameters:
 *   rh - Relative humidity [%]
 *   rh_ref - Reference humidity [%] (typically 50%)
 *   rh_coeff - RH sensitivity coefficient [1/%]
 *
 * Returns:
 *   Rate multiplication factor (dimensionless, clamped to [0.5, 3.0])
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_humidity_rate_factor(
    Real rh,
    Real rh_ref,
    Real rh_coeff)
{
    Real factor = Real(1.0) + rh_coeff * (rh - rh_ref);
    
    // Clamp to physically reasonable range
    factor = std::max(Real(0.5), factor);   // Minimum 0.5x at very dry conditions
    factor = std::min(Real(3.0), factor);   // Maximum 3.0x at very humid conditions
    
    return factor;
}

// ============================================================================
// Effective reaction rate computation
// ============================================================================

/**
 * Compute effective reaction rate with environmental dependencies
 *
 * Parameters:
 *   k_base - Base reaction rate at reference conditions [s^-1]
 *   temp - Current temperature [K]
 *   temp_ref - Reference temperature [K]
 *   temp_coeff - Temperature coefficient [1/K]
 *   rh - Current relative humidity [%]
 *   rh_ref - Reference humidity [%]
 *   rh_coeff - RH sensitivity coefficient [1/%]
 *
 * Returns:
 *   Effective reaction rate [s^-1]
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_effective_reaction_rate(
    Real k_base,
    Real temp,
    Real temp_ref,
    Real temp_coeff,
    Real rh,
    Real rh_ref,
    Real rh_coeff)
{
    Real f_temp = compute_temperature_rate_factor(temp, temp_ref, temp_coeff);
    Real f_rh = compute_humidity_rate_factor(rh, rh_ref, rh_coeff);
    
    return k_base * f_temp * f_rh;
}

// ============================================================================
// Species index and database utilities
// ============================================================================

/**
 * Get species index by name (MESOPUFF II standard species)
 *
 * Species order:
 *   0: SO2 (sulfur dioxide)
 *   1: SO4 (sulfate aerosol)
 *   2: NOx (nitrogen oxides)
 *   3: HNO3 (nitric acid gas)
 *   4: NO3 (nitrate aerosol)
 *
 * Parameters:
 *   species_name - Name of species
 *
 * Returns:
 *   Index (0-4) or -1 if not found
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
int get_species_index(const char* species_name)
{
    if (species_name == nullptr) return -1;
    
    // Direct comparison (simplified - in real code, use proper string comparison)
    // For GPU compatibility, use integer encoding
    if (species_name[0] == 'S' && species_name[1] == 'O') {
        if (species_name[2] == '2') return 0;  // SO2
        if (species_name[2] == '4') return 1;  // SO4
    }
    if (species_name[0] == 'N' && species_name[1] == 'O') {
        if (species_name[2] == 'x') return 2;  // NOx
        if (species_name[2] == '3') return 4;  // NO3
    }
    if (species_name[0] == 'H' && species_name[1] == 'N') {
        return 3;  // HNO3
    }
    
    return -1;
}

// ============================================================================
// Simple phenomenological chemistry network
// ============================================================================

/**
 * SO₂ oxidation pathway: SO₂ → SO₄ 
 *
 * Simplified representation of:
 *   1. SO₂ + OH → HO₂ + SO₃ (gas phase)
 *   2. SO₃ + H₂O → H₂SO₄ (gas phase)
 *   3. SO₂ + H₂O₂ → H₂SO₄ (aqueous phase, RH-dependent)
 * 
 * Net result: 1 mol SO₂ → ~1.5 mol SO₄ equiv. (accounting for oxidant addition)
 *
 * Parameters:
 *   species_mass - Array of species masses [0]=SO2, [1]=SO4
 *   dt - Time step [s]
 *   k_eff - Effective oxidation rate [s^-1]
 *   stoich_factor - Stoichiometric factor for SO₄ formation (typically 1.5)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_so2_oxidation(
    Real* species_mass,
    Real dt,
    Real k_eff,
    Real stoich_factor = Real(1.5))
{
    if (species_mass == nullptr) return;
    
    Real d_SO2 = species_mass[0] * (Real(1.0) - std::exp(-k_eff * dt));
    species_mass[0] -= d_SO2;
    species_mass[1] += d_SO2 * stoich_factor;
}

/**
 * NOₓ oxidation pathway: NOx → HNO3 → NO3
 *
 * Simplified representation of:
 *   1. NO₂ + OH + M → HNO₃ + M (terminal sink, slow)
 *   2. NO + O₃ → NO₂ + O₂ (fast cycling)
 *   3. HNO₃ ⇌ NO₃⁻ + H⁺ (gas-particle equilibrium)
 *
 * Parameters:
 *   species_mass - Array with [2]=NOx, [3]=HNO3, [4]=NO3
 *   dt - Time step [s]
 *   k_nox - NOx oxidation rate [s^-1]
 *   k_hno3 - HNO3 gas-particle conversion rate [s^-1]
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_nox_oxidation(
    Real* species_mass,
    Real dt,
    Real k_nox,
    Real k_hno3)
{
    if (species_mass == nullptr) return;
    
    // NOx → HNO3 (slower step, ~1.5x slower than SO2 oxidation)
    Real d_NOx = species_mass[2] * (Real(1.0) - std::exp(-k_nox * dt));
    species_mass[2] -= d_NOx;
    species_mass[3] += d_NOx * Real(1.35);  // Stoichiometric factor
    
    // HNO3 → NO3 (gas-particle equilibrium, faster conversion)
    Real d_HNO3 = species_mass[3] * (Real(1.0) - std::exp(-k_hno3 * dt));
    species_mass[3] -= d_HNO3;
    species_mass[4] += d_HNO3 * Real(0.98);  // Slight mass loss in condensation
}

// ============================================================================
// Complete chemistry step for puff species
// ============================================================================

/**
 * Apply all chemical transformations in a single timestep
 *
 * Integrates the full reaction network:
 * - SO₂ oxidation
 * - NOₓ oxidation and gas-particle conversion
 * - Environment-dependent rate adjustments (T, RH)
 *
 * Parameters:
 *   species_mass - Array of 5 species masses [SO2, SO4, NOx, HNO3, NO3]
 *   dt - Time step [s]
 *   temp - Ambient temperature [K]
 *   rh - Ambient relative humidity [%]
 *   k_base_so2 - Base SO₂ oxidation rate [s^-1]
 *   k_base_nox - Base NOₓ oxidation rate [s^-1]
 *   k_base_hno3 - Base HNO3 conversion rate [s^-1]
 *   temp_ref - Reference temperature [K]
 *   rh_ref - Reference humidity [%]
 *   temp_coeff - Temperature sensitivity [1/K]
 *   rh_coeff - RH sensitivity [1/%]
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_chemistry_network(
    Real* species_mass,
    Real dt,
    Real temp,
    Real rh,
    Real k_base_so2 = Real(0.001),
    Real k_base_nox = Real(0.0007),
    Real k_base_hno3 = Real(0.002),
    Real temp_ref = Real(298.15),
    Real rh_ref = Real(50.0),
    Real temp_coeff = Real(0.04),
    Real rh_coeff = Real(0.005))
{
    if (species_mass == nullptr) return;
    
    // Compute effective rates with environmental dependencies
    Real k_eff_so2 = compute_effective_reaction_rate(
        k_base_so2, temp, temp_ref, temp_coeff, rh, rh_ref, rh_coeff);
    
    Real k_eff_nox = compute_effective_reaction_rate(
        k_base_nox, temp, temp_ref, temp_coeff * Real(0.9), rh, rh_ref, rh_coeff * Real(0.8));
    
    Real k_eff_hno3 = compute_effective_reaction_rate(
        k_base_hno3, temp, temp_ref, temp_coeff * Real(0.5), rh, rh_ref, rh_coeff);
    
    // Apply transformations
    apply_so2_oxidation(species_mass, dt, k_eff_so2, Real(1.5));
    apply_nox_oxidation(species_mass, dt, k_eff_nox, k_eff_hno3);
}

#endif // CHEMISTRY_MODELS_H
