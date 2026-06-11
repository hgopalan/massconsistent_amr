// ============================================================================
// optical_models.H
// Optical properties and visibility calculations based on IMPROVE algorithm
//
// This header provides functions for computing:
// - Extinction coefficients from aerosol and gas species concentrations
// - Relative humidity-dependent particle growth factors
// - Visual range and deciview (logarithmic visibility scale)
// - Light scattering and absorption coefficients
//
// Model: IMPROVE (Interagency Monitoring of Protected Visual Environments)
// Algorithm for computing light extinction from aerosol species.
//
// References:
// - IMPROVE Algorithm: Pitchford, M., et al. (2007). Integration of Air
//   Quality Modeling and Monitoring for Estimating Visibility.
// - IMPROVE Manual: http://vista.cira.colostate.edu/improve/Publications/
// ============================================================================

#ifndef OPTICAL_MODELS_H
#define OPTICAL_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <algorithm>

using namespace amrex;

// Physical constants for visibility calculations
static constexpr Real RAYLEIGH_SCATTERING = Real(10.0);  // Rayleigh scattering [Mm^-1]
static constexpr Real MIE_SCATTERING_BASELINE = Real(0.0);  // Background Mie scattering
static constexpr Real VISUAL_RANGE_REFERENCE = Real(3912.0);  // Reference constant for RVR

// ============================================================================
// Aerosol Optical Properties Database
// ============================================================================

/**
 * Standard IMPROVE species optical properties at 550 nm
 *
 * Returns:
 *   Mass extinction coefficient [m²/g] at reference RH
 *
 * Parameters:
 *   species_id - Species identifier (0=SO4, 1=NO3, 2=OC, 3=BC, 4=Dust)
 *
 * Returns:
 *   Mass extinction coefficient [m²/g]
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real get_species_mass_extinction_coefficient(int species_id)
{
    // IMPROVE standard values [m²/g] at 550 nm wavelength
    switch (species_id) {
        case 0:  // Sulfate (SO₄)
            return Real(3.0);
        case 1:  // Nitrate (NO₃)
            return Real(2.8);
        case 2:  // Organic carbon (OC)
            return Real(4.0);
        case 3:  // Black carbon (BC)
            return Real(10.0);
        case 4:  // Dust
            return Real(1.0);
        default:
            return Real(0.0);
    }
}

/**
 * Compute RH-dependent growth factor for particles
 *
 * Particles grow with increasing relative humidity due to hygroscopic
 * water uptake. Growth factor f(RH) relates optical properties at
 * high RH to dry properties.
 *
 * Uses Gaussian interpolation with reference points at RH=50% and RH=80%
 *
 * Parameters:
 *   rh - Relative humidity [%]
 *   growth_50 - Growth factor at RH=50%
 *   growth_80 - Growth factor at RH=80%
 *
 * Returns:
 *   RH-dependent growth factor (≥ 1.0)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_rh_growth_factor(
    Real rh,
    Real growth_50,
    Real growth_80)
{
    // Clamp RH to valid range
    rh = std::max(Real(0.0), std::min(Real(99.0), rh));
    
    if (rh <= Real(50.0)) {
        // Below 50%: use dry properties (linearly interpolate from 0%)
        return Real(1.0) + (growth_50 - Real(1.0)) * (rh / Real(50.0));
    } else if (rh >= Real(80.0)) {
        // Above 80%: saturate at growth_80
        return growth_80;
    } else {
        // Between 50% and 80%: linear interpolation
        Real alpha = (rh - Real(50.0)) / Real(30.0);
        return growth_50 + alpha * (growth_80 - growth_50);
    }
}

// ============================================================================
// Extinction Coefficient Computation
// ============================================================================

/**
 * Compute extinction coefficient from SO₄ and NO₃ concentrations
 *
 * Classic IMPROVE algorithm for two-species system.
 * Includes RH-dependent growth factors.
 *
 * Parameters:
 *   so4_conc - Sulfate concentration [μg/m³]
 *   no3_conc - Nitrate concentration [μg/m³]
 *   rh - Relative humidity [%]
 *   b_ray - Rayleigh scattering coefficient [Mm^-1]
 *
 * Returns:
 *   Extinction coefficient [Mm^-1] (1 Mm^-1 = 10^-6 m^-1)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_extinction_coefficient_basic(
    Real so4_conc,
    Real no3_conc,
    Real rh,
    Real b_ray = RAYLEIGH_SCATTERING)
{
    // RH-dependent growth factors for aerosols
    Real f_rh_so4 = Real(1.0);
    if (rh > Real(0.0) && rh < Real(100.0)) {
        f_rh_so4 = Real(1.0) + Real(0.12) * std::pow(rh / (Real(100.0) - rh), Real(0.6));
    }
    
    Real f_rh_no3 = Real(1.0);
    if (rh > Real(0.0) && rh < Real(100.0)) {
        f_rh_no3 = Real(1.1) + Real(0.15) * std::pow(rh / (Real(100.0) - rh), Real(0.5));
    }
    
    // Mass extinction coefficients [m²/g]
    Real bext_so4 = Real(3.0) * f_rh_so4 * so4_conc;
    Real bext_no3 = Real(2.8) * f_rh_no3 * no3_conc;
    
    // Total extinction = Rayleigh + aerosol scattering
    Real b_ext = bext_so4 + bext_no3 + b_ray;
    
    return b_ext;
}

/**
 * Compute extinction coefficient from 5-species IMPROVE formula
 *
 * Extended IMPROVE algorithm including OC, BC, and dust.
 *
 * Parameters:
 *   so4_conc - Sulfate [μg/m³]
 *   no3_conc - Nitrate [μg/m³]
 *   oc_conc - Organic carbon [μg/m³]
 *   bc_conc - Black carbon [μg/m³]
 *   dust_conc - Dust (coarse particles) [μg/m³]
 *   rh - Relative humidity [%]
 *   b_ray - Rayleigh scattering [Mm^-1]
 *
 * Returns:
 *   Total extinction coefficient [Mm^-1]
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_extinction_coefficient_extended(
    Real so4_conc,
    Real no3_conc,
    Real oc_conc,
    Real bc_conc,
    Real dust_conc,
    Real rh,
    Real b_ray = RAYLEIGH_SCATTERING)
{
    // RH-dependent growth factors
    Real f_rh_so4 = Real(1.0);
    if (rh > Real(0.0) && rh < Real(100.0)) {
        f_rh_so4 = Real(1.0) + Real(0.12) * std::pow(rh / (Real(100.0) - rh), Real(0.6));
    }
    
    Real f_rh_no3 = Real(1.0);
    if (rh > Real(0.0) && rh < Real(100.0)) {
        f_rh_no3 = Real(1.1) + Real(0.15) * std::pow(rh / (Real(100.0) - rh), Real(0.5));
    }
    
    // OC and dust show weak RH dependence
    Real f_rh_oc = Real(1.05) + Real(0.05) * (rh / Real(100.0));
    Real f_rh_dust = Real(1.0) + Real(0.02) * (rh / Real(100.0));
    
    // Mass extinction coefficients with RH adjustment
    Real bext_so4 = Real(3.0) * f_rh_so4 * so4_conc;
    Real bext_no3 = Real(2.8) * f_rh_no3 * no3_conc;
    Real bext_oc = Real(4.0) * f_rh_oc * oc_conc;
    Real bext_bc = Real(10.0) * bc_conc;  // BC shows no RH dependence
    Real bext_dust = Real(1.0) * f_rh_dust * dust_conc;
    
    // Total extinction
    Real b_ext = bext_so4 + bext_no3 + bext_oc + bext_bc + bext_dust + b_ray;
    
    return b_ext;
}

// ============================================================================
// Visibility Metrics
// ============================================================================

/**
 * Compute visual range from extinction coefficient
 *
 * Koschmieder equation:
 *   V = 3.912 / b_ext   [km when b_ext in Mm^-1]
 * 
 * Relates perceptual visual range to extinction coefficient.
 *
 * Parameters:
 *   b_ext - Extinction coefficient [Mm^-1]
 *   reference_constant - Koschmieder constant (default 3.912)
 *
 * Returns:
 *   Visual range [km]
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_visual_range(
    Real b_ext,
    Real reference_constant = VISUAL_RANGE_REFERENCE)
{
    b_ext = std::max(b_ext, Real(0.1));  // Avoid division issues
    return reference_constant / b_ext;
}

/**
 * Compute deciview index from extinction coefficient
 *
 * Deciview (dV) is a logarithmic visibility scale:
 *   dV = 10 * log₁₀(b_ext / b_ref)
 *
 * where b_ref = 10 Mm^-1 (baseline extinction in clear conditions)
 *
 * Advantages: Linear perception, emphasizes visibility changes at high visibility
 *
 * Parameters:
 *   b_ext - Extinction coefficient [Mm^-1]
 *   b_ref - Reference extinction coefficient [Mm^-1] (default 10 Mm^-1)
 *
 * Returns:
 *   Deciview index [dV] (higher = hazier)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_deciview(
    Real b_ext,
    Real b_ref = Real(10.0))
{
    b_ext = std::max(b_ext, Real(0.1));
    Real dV = Real(10.0) * std::log10(b_ext / b_ref);
    return dV;
}

/**
 * Compute contrast ratio (visibility metric)
 *
 * Contrast C between an object and sky background:
 *   C = C₀ * exp(-b_ext * d)
 *
 * where d is distance to object
 *
 * Parameters:
 *   b_ext - Extinction coefficient [m^-1]
 *   distance - Distance to object [m]
 *   initial_contrast - Initial contrast at d=0 (typical 0.02)
 *
 * Returns:
 *   Visibility contrast (dimensionless, 0-1)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_visibility_contrast(
    Real b_ext,
    Real distance,
    Real initial_contrast = Real(0.02))
{
    // Convert b_ext from Mm^-1 to m^-1
    Real b_ext_m_inv = b_ext * Real(1.0e-6);
    
    Real contrast = initial_contrast * std::exp(-b_ext_m_inv * distance);
    
    return std::max(Real(0.0), std::min(Real(1.0), contrast));
}

// ============================================================================
// Advanced Optical Properties
// ============================================================================

/**
 * Compute single scattering albedo (SSA)
 *
 * SSA = scattering / (scattering + absorption)
 * 
 * High SSA (near 1) = scattering aerosol (SO₄, OC)
 * Low SSA (< 0.8) = absorbing aerosol (BC, dust)
 *
 * Parameters:
 *   bc_conc - Black carbon concentration [μg/m³]
 *   total_extinction - Total extinction coefficient [Mm^-1]
 *
 * Returns:
 *   Single scattering albedo (0-1)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_single_scattering_albedo(
    Real bc_conc,
    Real total_extinction)
{
    // BC absorption coefficient [m²/g]
    Real b_abs_bc = Real(10.0) * bc_conc;
    
    // Total scattering ≈ total_extinction - absorption
    Real b_scat = std::max(total_extinction - b_abs_bc * Real(1.0e-6), Real(0.0));
    
    if (total_extinction <= Real(0.0)) {
        return Real(1.0);  // Pure scattering for clear air
    }
    
    Real ssa = b_scat / total_extinction;
    return std::max(Real(0.0), std::min(Real(1.0), ssa));
}

/**
 * Compute aerosol optical depth (AOD) at 550 nm
 *
 * AOD = ∫ b_ext(z) dz (vertical integral of extinction)
 *
 * Parameters:
 *   b_ext - Extinction coefficient [m^-1]
 *   scale_height - Effective scale height for aerosol [m]
 *
 * Returns:
 *   Aerosol optical depth (dimensionless)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Real compute_aerosol_optical_depth(
    Real b_ext,
    Real scale_height = Real(1000.0))
{
    // Approximate: AOD ≈ b_ext * H (assuming exponential decay)
    Real aod = b_ext * scale_height;
    
    return aod;
}

// ============================================================================
// Air Quality Index from Visibility
// ============================================================================

/**
 * Compute Air Quality Index (AQI) equivalent from visibility
 *
 * Maps visibility (deciview) to standard AQI categories:
 * - Good: dV < 5
 * - Moderate: 5 ≤ dV < 10
 * - Unhealthy for Sensitive: 10 ≤ dV < 15
 * - Unhealthy: 15 ≤ dV < 20
 * - Very Unhealthy: 20 ≤ dV < 25
 * - Hazardous: dV ≥ 25
 *
 * Parameters:
 *   deciview - Deciview index
 *
 * Returns:
 *   AQI category (0=Good, 1=Moderate, 2=Sensitive, 3=Unhealthy, 4=V.Unhealthy, 5=Hazardous)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
int deciview_to_aqi_category(Real deciview)
{
    if (deciview < Real(5.0)) return 0;      // Good
    if (deciview < Real(10.0)) return 1;     // Moderate
    if (deciview < Real(15.0)) return 2;     // Unhealthy for Sensitive
    if (deciview < Real(20.0)) return 3;     // Unhealthy
    if (deciview < Real(25.0)) return 4;     // Very Unhealthy
    return 5;                                 // Hazardous
}

#endif // OPTICAL_MODELS_H
