// ============================================================================
// reactive_chemistry_simple.H
// Simple reactive chemistry for passive plume transport (AERMOD TOXICS level)
//
// Supported species:
// - NO₂ (nitrogen dioxide): photochemical decay to NO
// - SO₂ (sulfur dioxide): oxidation to SO₄²⁻
// - HCl (hydrogen chloride): hydrolysis to Cl⁻
// - NH₃ (ammonia): oxidation to NO₂/NO₃⁻ or absorption over water
// - Custom species with user-specified half-lives
//
// Mechanism: First-order exponential decay
// C(t) = C(0) × exp(-t / τ) where τ = ln(2) × t_1/2
//
// Features:
// - Stoichiometric product formation (mass conservation)
// - Optional seasonal adjustment (winter/summer variation)
// - Optional temperature correction (Q10 model)
// - Gas-liquid exchange parameterization for ammonia over water
// - GPU-compatible inline functions
//
// References:
// - EPA AERMOD TOXICS Module
// - Finlayson-Pitts & Pitts (2000). Atmospheric Chemistry
// - Atkinson et al. (2004). Evaluated kinetic data
// - Sutton et al. (2013). Ammonia in the environment: From ancient times to the present
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
    NH3 = 7,            // Ammonia (lighter-than-air, oxidizes in atmosphere)
    NH4_PLUS = 8,       // Ammonium ion (product of NH₃ oxidation or aqueous absorption)
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
    } else if (species == SpeciesType::NH3) {
        // NH₃ oxidation faster in summer (more photochemistry, higher temp)
        if (month >= 6 && month <= 8) {        // Summer
            factor = 1.5;
        } else if (month >= 12 || month <= 2) { // Winter
            factor = 0.5;
        } else {                                 // Spring/Fall
            factor = 1.0;
        }
    }
     
    return factor;
}

// ============================================================================
// Ammonia gas-liquid exchange coefficient over water
//
// Computes the effective removal velocity for ammonia dissolution in water
// using a simplified Whitman two-film theory approach.
//
// Physical basis:
// - Ammonia has very high solubility in water (Henry's constant ~50 M/atm at 25°C)
// - Gas-side diffusivity controlled by wind speed
// - Liquid-side diffusivity controlled by wind-induced convection
// - Temperature affects both Henry's constant and diffusivities
//
// Inputs:
//   wind_speed        - Wind speed at 10 m [m/s]
//   water_temperature - Water surface temperature [K]
//   salinity          - Water salinity [PSU] (0 = fresh, 35 = seawater)
//
// Returns:
//   Removal velocity [m/s] (equivalent to effective deposition velocity)
//
// References:
// - Jähne et al. (1987). Measurement of the diffusion coefficients of sparingly
//   soluble gases in water. Journal of Geophysical Research
// - Deacon (1977). Gas transfer to and across the air-water interface
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_ammonia_water_exchange(
   amrex::Real wind_speed,
   amrex::Real water_temperature,
   amrex::Real salinity = amrex::Real(0.0))
{
   // Henry's law constant at 25°C (298.15 K) [M/atm]
   const amrex::Real K_H_ref = 50.0;
    
   // Temperature dependence of Henry's constant [K]
   // d(ln K_H)/d(1/T) ≈ 2400 K for ammonia
   const amrex::Real d_ln_K = 2400.0;
   const amrex::Real T_ref = 298.15;
    
   // Compute temperature-dependent Henry's constant
   amrex::Real K_H = K_H_ref * std::exp(d_ln_K * (amrex::Real(1.0)/water_temperature - 
                                                    amrex::Real(1.0)/T_ref));
    
   // Salinity correction (Setschenow equation, ~-2.5% per PSU)
   // Only apply if salinity > 0 (seawater case)
   if (salinity > amrex::Real(0.1)) {
       K_H *= (amrex::Real(1.0) - amrex::Real(0.025) * salinity);
   }
    
   // Air-side mass transfer coefficient [m/s]
   // Based on Jähne relationship: k_a ≈ (0.31 m/s) × (U_10/13)^2 × (Sc_a/Sc_w)^(-1/2)
   // Simplified: k_a ≈ 0.0005 × U_10 [m/s]
   amrex::Real U_10 = std::max(wind_speed, amrex::Real(0.1));
   amrex::Real k_a = amrex::Real(0.0005) * U_10;
    
   // Water-side mass transfer coefficient [m/s]
   // Based on Deacon's relation: k_w ≈ 0.0005 × sqrt(U_10) [m/s]
   // This is wind-dependent but slower than air-side
   amrex::Real k_w = amrex::Real(0.0005) * std::sqrt(U_10);
    
   // Overall two-film resistance:
   // 1/K_overall = 1/(k_a × K_H) + 1/k_w
   amrex::Real resistance_air = amrex::Real(1.0) / (k_a * K_H);
   amrex::Real resistance_water = amrex::Real(1.0) / k_w;
   amrex::Real resistance_total = resistance_air + resistance_water;
    
   // Overall mass transfer coefficient [m/s]
   amrex::Real K_overall = amrex::Real(1.0) / amrex::max(resistance_total, amrex::Real(1.0e-10));
    
   return K_overall;
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

// ============================================================================
// Ammonia-specific chemistry and removal (for over-land and over-water)
//
// Simplified model for ammonia:
// 1. Over land: First-order oxidation to NOₓ/NO₃⁻ (slow, days to weeks)
// 2. Over water: Fast dissolution via gas-liquid exchange (minutes)
//
// Inputs:
//   C_NH3          - Ammonia concentration [ppb or μg/m³] (in/out)
//   C_NH4          - Ammonium concentration [ppb or μg/m³] (out)
//   dt             - Time step [s]
//   half_life_NH3  - Oxidation half-life over land [hours] (typical: 5-10 days)
//   enable_water_exchange - Enable water exchange (true over water/wet surfaces)
//   wind_speed     - Wind speed [m/s]
//   water_temp     - Water temperature [K] (only used if over_water=true)
//   salinity       - Water salinity [PSU] (only used if over_water=true)
//   enable_seasonal_adjust - Enable seasonal oxidation adjustment
//   enable_temp_adjust - Enable temperature-dependent oxidation
//   T_ambient      - Ambient temperature [K]
//   month          - Month for seasonal adjustment (1-12)
//   T_ref          - Reference temperature [K]
//   Q10            - Q10 temperature sensitivity
//
// References:
// - Gall et al. (2002). Ammonia oxidation rates in California
// - Asman (1998). Ammonia emission reduction by the large combustion plants
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void update_ammonia_chemistry(
   amrex::Real& C_NH3,                // Ammonia concentration (in/out)
   amrex::Real& C_NH4,                // Ammonium concentration (out)
   amrex::Real dt,                    // Time step [s]
   amrex::Real half_life_NH3 = 216.0, // Default: 9 days = 216 hours (over land)
   bool enable_water_exchange = false, // Use water exchange instead of oxidation
   amrex::Real wind_speed = 5.0,      // Wind speed [m/s]
   amrex::Real water_temp = 288.15,   // Water temperature [K]
   amrex::Real salinity = 0.0,        // Water salinity [PSU]
   bool enable_seasonal_adjust = false,
   bool enable_temp_adjust = false,
   amrex::Real T_ambient = 298.15,
   int month = 6,
   amrex::Real T_ref = 298.15,
   amrex::Real Q10 = 2.0)
{
   if (C_NH3 <= amrex::Real(1.0e-10) || dt <= amrex::Real(0.0)) {
       return;  // No ammonia to process
   }
    
   amrex::Real C_NH3_init = C_NH3;
    
   if (enable_water_exchange) {
       // Over water: Use gas-liquid exchange as effective removal
       amrex::Real k_exchange = compute_ammonia_water_exchange(wind_speed, water_temp, salinity);
        
       // Convert to effective removal rate [1/s]
       // Flux = k_exchange * C => effective decay constant
       // For now, treat as simple deposition: m_removed = k_exchange * area * C * dt
       // Simplified: removal fraction = 1 - exp(-k_exchange * dt)
       amrex::Real removal_fraction = amrex::Real(1.0) - std::exp(-k_exchange * dt);
       C_NH3 *= (amrex::Real(1.0) - removal_fraction);
        
       // Absorbed ammonia becomes ammonium (NH₃ + H₂O → NH₄⁺ + OH⁻)
       // Stoichiometry: MW_NH4 / MW_NH3 = 18 / 17 ≈ 1.06
       amrex::Real C_NH3_removed = C_NH3_init - C_NH3;
       C_NH4 += C_NH3_removed * amrex::Real(1.06);
        
   } else {
       // Over land: First-order oxidation (slow process)
       amrex::Real lambda_NH3 = compute_decay_constant(half_life_NH3);
        
       // Apply seasonal adjustments (if enabled)
       if (enable_seasonal_adjust) {
           lambda_NH3 *= compute_seasonal_factor(month, SpeciesType::NH3);
       }
        
       // Apply temperature adjustments (if enabled)
       if (enable_temp_adjust) {
           lambda_NH3 *= compute_temperature_correction(T_ambient, T_ref, Q10);
       }
        
       // Apply decay (oxidation)
       C_NH3 = apply_exponential_decay(C_NH3_init, lambda_NH3, dt);
        
       // Oxidation product: NH₃ → NOₓ/NO₃⁻
       // Simplified stoichiometry: 1 mole NH₃ → ~1 mole NO₃⁻
       // Mass ratio: MW_NO3 / MW_NH3 = 62 / 17 ≈ 3.65 (but this is incomplete oxidation)
       // More realistically: NH₃ → 0.5 NO₂ + products, use simplified ratio
       amrex::Real C_NH3_oxidized = C_NH3_init - C_NH3;
       C_NH4 += C_NH3_oxidized * amrex::Real(0.5);  // Simplified product formation
   }
}

#endif // REACTIVE_CHEMISTRY_SIMPLE_H
