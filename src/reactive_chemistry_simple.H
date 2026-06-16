// ============================================================================
// reactive_chemistry_simple.H
// Simple reactive chemistry for passive plume transport (AERMOD TOXICS level)
//
// Supported species:
// - NO₂ (nitrogen dioxide): photochemical decay to NO
// - SO₂ (sulfur dioxide): oxidation to SO₄²⁻
// - HCl (hydrogen chloride): hydrolysis to Cl⁻
// - Custom species with user-specified half-lives
//
// Mechanism: First-order exponential decay
// C(t) = C(0) × exp(-t / τ) where τ = ln(2) × t_1/2
//
// Features:
// - Stoichiometric product formation (mass conservation)
// - Optional seasonal adjustment (winter/summer variation)
// - Optional temperature correction (Q10 model)
// - GPU-compatible inline functions
//
// References:
// - EPA AERMOD TOXICS Module
// - Finlayson-Pitts & Pitts (2000). Atmospheric Chemistry
// - Atkinson et al. (2004). Evaluated kinetic data
// ============================================================================

#ifndef REACTIVE_CHEMISTRY_SIMPLE_H
#define REACTIVE_CHEMISTRY_SIMPLE_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>

// ============================================================================
// Chemistry species enumeration
// ============================================================================
enum class SpeciesType : int {
    NO2 = 0,            // Nitrogen dioxide
    SO2 = 1,            // Sulfur dioxide
    HCL = 2,            // Hydrogen chloride
    NO = 3,             // Nitric oxide (product of NO₂ decay)
    SO4 = 4,            // Sulfate ion (product of SO₂ oxidation)
    CL_MINUS = 5,       // Chloride ion (product of HCl)
    CUSTOM = 6          // User-defined species
};

// ============================================================================
// Compute decay constant from half-life
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_decay_constant(amrex::Real half_life_hours)
{
    // λ = ln(2) / t_1/2
    // t_1/2 in seconds = half_life_hours × 3600
    constexpr amrex::Real LN2 = 0.693147180559945;
    constexpr amrex::Real SECONDS_PER_HOUR = 3600.0;
    
    if (half_life_hours < 1.0e-10) return 0.0;  // No decay
    
    amrex::Real t_half_sec = half_life_hours * SECONDS_PER_HOUR;
    return LN2 / t_half_sec;
}

// ============================================================================
// Apply 1st-order exponential decay to concentration
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real apply_exponential_decay(
    amrex::Real C_initial,           // Initial concentration [ppb or μg/m³]
    amrex::Real decay_constant,      // λ [1/s]
    amrex::Real dt)                  // Time step [s]
{
    // C(t) = C(0) × exp(-λ × t)
    
    if (decay_constant < 1e-20) return C_initial;  // No decay
    
    amrex::Real decay_factor = std::exp(-decay_constant * dt);
    return C_initial * decay_factor;
}

// ============================================================================
// Compute product concentration from decay (mass conservation)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_product_concentration(
    amrex::Real C_reactant_initial,  // Initial reactant [ppb]
    amrex::Real C_reactant_current,  // Current reactant [ppb]
    amrex::Real C_product_initial,   // Initial product [ppb]
    amrex::Real stoichiometric_ratio) // Moles product / moles reactant
{
    // Conservation: Product formed = (Initial - Current) × stoichiometry
    // C_product(t) = C_product(0) + (C_reactant(0) - C_reactant(t)) × ratio
    
    amrex::Real C_reactant_decayed = C_reactant_initial - C_reactant_current;
    return C_product_initial + (C_reactant_decayed * stoichiometric_ratio);
}

// ============================================================================
// Temperature correction factor (Q10 model)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_temperature_correction(
    amrex::Real T_current,           // Current temperature [K]
    amrex::Real T_reference,         // Reference temperature [K]
    amrex::Real Q10 = 2.0)           // Fold per 10 K
{
    // λ(T) = λ(T_ref) × Q10^((T - T_ref) / 10)
    
    amrex::Real dT = T_current - T_reference;
    return std::pow(Q10, dT / 10.0);
}

// ============================================================================
// Season-dependent adjustment (empirical)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_seasonal_factor(
    int month,                       // Month (1=Jan, 12=Dec)
    SpeciesType species)             // Species identifier
{
    // Simplified seasonal variation
    // Winter (Dec-Feb): lower oxidation rates
    // Summer (Jun-Aug): higher oxidation rates
    
    amrex::Real factor = 1.0;
    
    if (species == SpeciesType::SO2) {
        // SO₂ oxidation is slower in winter (less aqueous phase, less H₂O₂)
        // Faster in summer (more aqueous chemistry)
        if (month >= 6 && month <= 8) {        // Summer
            factor = 1.5;
        } else if (month >= 12 || month <= 2) { // Winter
            factor = 0.5;
        } else {                                 // Spring/Fall
            factor = 1.0;
        }
    } else if (species == SpeciesType::NO2) {
        // NO₂ photolysis faster in summer (more UV light)
        if (month >= 6 && month <= 8) {        // Summer
            factor = 1.3;
        } else if (month >= 12 || month <= 2) { // Winter
            factor = 0.7;
        } else {                                 // Spring/Fall
            factor = 1.0;
        }
    } else if (species == SpeciesType::HCL) {
        // HCl hydrolysis relatively constant
        factor = 1.0;
    }
    
    return factor;
}

// ============================================================================
// Main reactive chemistry update (per puff or grid cell)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void update_chemistry_decay(
    // Input/Output concentrations [ppb or μg/m³]
    amrex::Real& C_NO2,
    amrex::Real& C_SO2,
    amrex::Real& C_HCL,
    // Output products
    amrex::Real& C_NO,
    amrex::Real& C_SO4,
    amrex::Real& C_CL,
    // Parameters
    amrex::Real dt,                  // Time step [s]
    amrex::Real half_life_NO2,       // Half-life [hours]
    amrex::Real half_life_SO2,       // Half-life [hours]
    amrex::Real half_life_HCL,       // Half-life [hours]
    bool enable_products = true,
    bool enable_seasonal_adjust = false,
    bool enable_temp_adjust = false,
    amrex::Real T_ambient = 298.15,  // Temperature [K]
    int month = 6,                   // Month (1-12)
    amrex::Real T_ref = 298.15,      // Reference temperature [K]
    amrex::Real Q10 = 2.0)           // Q10 temperature sensitivity
{
    // Save initial concentrations
    amrex::Real C_NO2_init = C_NO2;
    amrex::Real C_SO2_init = C_SO2;
    amrex::Real C_HCL_init = C_HCL;
    
    // Compute decay constants
    amrex::Real lambda_NO2 = compute_decay_constant(half_life_NO2);
    amrex::Real lambda_SO2 = compute_decay_constant(half_life_SO2);
    amrex::Real lambda_HCL = compute_decay_constant(half_life_HCL);
    
    // Apply seasonal adjustments (if enabled)
    if (enable_seasonal_adjust) {
        lambda_NO2 *= compute_seasonal_factor(month, SpeciesType::NO2);
        lambda_SO2 *= compute_seasonal_factor(month, SpeciesType::SO2);
        lambda_HCL *= compute_seasonal_factor(month, SpeciesType::HCL);
    }
    
    // Apply temperature adjustments (if enabled)
    if (enable_temp_adjust) {
        lambda_NO2 *= compute_temperature_correction(T_ambient, T_ref, Q10);
        lambda_SO2 *= compute_temperature_correction(T_ambient, T_ref, Q10);
        lambda_HCL *= compute_temperature_correction(T_ambient, T_ref, Q10);
    }
    
    // Apply decay to reactants
    C_NO2 = apply_exponential_decay(C_NO2_init, lambda_NO2, dt);
    C_SO2 = apply_exponential_decay(C_SO2_init, lambda_SO2, dt);
    C_HCL = apply_exponential_decay(C_HCL_init, lambda_HCL, dt);
    
    // Compute product species (if enabled)
    if (enable_products) {
        // NO₂ → NO (1:1 molar stoichiometry)
        C_NO = compute_product_concentration(C_NO2_init, C_NO2, C_NO, 1.0);
        
        // SO₂ → SO₄²⁻ 
        // Stoichiometry: 1 mole SO₂ → 1 mole SO₄
        // But mass-wise: MW_SO4 / MW_SO2 = 96 / 64 = 1.5
        C_SO4 = compute_product_concentration(C_SO2_init, C_SO2, C_SO4, 1.5);
        
        // HCl → Cl⁻
        // Stoichiometry: 1 mole HCl → 1 mole Cl⁻
        // But mass-wise: MW_Cl / MW_HCl = 35.5 / 36.5 ≈ 0.97
        C_CL = compute_product_concentration(C_HCL_init, C_HCL, C_CL, 0.97);
    }
}

#endif // REACTIVE_CHEMISTRY_SIMPLE_H
