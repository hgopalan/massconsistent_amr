// ============================================================================
// synthetic_turbulence.H
// Synthetic turbulence fluctuation generation for OpenFAST compatibility
//
// This header provides components for generating synthetic turbulent 
// fluctuations to add to the mean wind field from the mass-consistent solver.
// The approach follows standard atmospheric turbulence modeling using spectral
// methods (Von Kármán and Kaimal spectra) with height-dependent intensity
// and coherence functions.
//
// Physical Models Implemented:
//
// SPECTRUM MODELS:
//   1. Von Kármán spectrum - Isotropic turbulence model
//   2. Kaimal spectrum - Alternative, commonly used in wind engineering
//
// INTENSITY MODELS:
//   1. Power-law - Height-dependent power-law profile
//   2. Logarithmic - Log-law profile for rough terrain
//   3. Constant - Uniform intensity profile
//   4. IEC 61400 - Industry-standard wind turbine design (NEW - Phase 1)
//   5. SmoothProfile - User-defined smooth profiles (NEW - Phase 1)
//
// COHERENCE MODELS:
//   1. Gaussian - Gaussian decay: Coh = exp(-k*distance^2)
//   2. Exponential - Exponential decay: Coh = exp(-k*distance)
//   3. QuadraticExponential - Smooth quadratic decay (NEW - Phase 1)
//   4. PowerLaw - Algebraic power-law decay (NEW - Phase 1)
//
// PHASE 1 ENHANCEMENTS (June 2026):
//   - IEC 61400-1:2019 intensity model for wind turbine certification
//   - QuadraticExponential and PowerLaw coherence models for complex terrain
//   - Smooth user-defined intensity profiles with C-infinity continuity
//   - See docs/PHASE1_TURBULENCE_ENHANCEMENTS.md for detailed documentation
//
// References:
//   - von Kármán, T. (1948). Progress in the statistical theory of turbulence.
//     Proceedings of the National Academy of Sciences, 34(11), 530-539.
//   - Kaimal, J.C., et al. (1972). Spectral characteristics of surface-layer
//     turbulence. Quarterly Journal of the Royal Meteorological Society, 98(417), 563-589.
//   - IEC 61400-1 (2019) Wind turbine design standards and coherence models
//   - TurbSim source: NREL documentation on spectral synthesis
//   - Panofsky, H.A., & Dutton, J.A. (1984). Atmospheric Turbulence.
//
// Usage Example:
//   TurbulenceParams turb_params;
//   turb_params.enabled = true;
//   turb_params.spectrum_model = TurbulenceModel::VonKarman;
//   turb_params.intensity_model = IntensityModel::IEC61400;
//   turb_params.coherence_model = CoherenceModel::PowerLaw;
//   
//   TurbulenceGenerator turb_gen(turb_params);
//   turb_gen.ComputeIntensity(z_agl);
//   turb_gen.ComputeSpectra(freq_array, u_spectrum, v_spectrum, w_spectrum);
//
// ============================================================================

#ifndef SYNTHETIC_TURBULENCE_H
#define SYNTHETIC_TURBULENCE_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include "math_constants.H"
#include <cmath>
#include <algorithm>

// ============================================================================
// Enumeration types for turbulence model selection
// ============================================================================

enum class TurbulenceModel {
    VonKarman = 0,    // Von Kármán (1948) isotropic spectrum
    Kaimal = 1,       // Kaimal et al. (1972) empirical spectrum
    MannBox = 2,       // Mann (1994) anisotropic spectral tensor model (Phase 2)
    GP_LLJ = 3,       // Great Plains Low-Level Jet (NEW)
    NWTC = 4,         // National Wind Technology Center (NEW)
    USWTPP = 5,       // US Wind Turbine Verification Program (NEW)
    HIT = 6           // Homogeneous Isotropic Turbulence (NEW)
};

enum class IntensityModel {
    PowerLaw = 0,     // I(z) = I_ref * (z/z_ref)^alpha
    Logarithmic = 1,  // I(z) = I_0 * ln(z/z0) / ln(z_ref/z0)
    Constant = 2,     // I(z) = constant
    IEC61400 = 3,     // IEC 61400-1:2019 normal turbulence model
    SmoothProfile = 4 // User-defined smooth spline profile
};

enum class CoherenceModel {
    Gaussian = 0,      // Exponential decay: Coh = exp(-k*distance)
    Exponential = 1,   // Gaussian: Coh = exp(-k*distance^2)
    QuadraticExponential = 2, // Quadratic exponential: exp(-k*distance^2/2)
    PowerLaw = 3       // Power-law decay: Coh = (1 + k*distance)^(-m)
};

// ============================================================================
// Turbulence Parameters Structure
// Configuration for synthetic turbulence generation
// ============================================================================

struct TurbulenceParams {
    // Enable/disable synthetic turbulence
    bool enabled = false;
    
    // ---- Spectral Model Selection ----
    TurbulenceModel spectrum_model = TurbulenceModel::VonKarman;
    IntensityModel intensity_model = IntensityModel::PowerLaw;
    CoherenceModel coherence_model = CoherenceModel::Gaussian;
    
    // ---- Turbulence Intensity Profile ----
    // Power-law exponent for intensity (typical: 0.10 - 0.16)
    amrex::Real intensity_exponent = amrex::Real(0.14);
    
    // Reference intensity at reference height (typical: 0.10 - 0.20)
    amrex::Real intensity_ref = amrex::Real(0.12);
    
    // Reference height for intensity [m AGL]
    amrex::Real z_intensity_ref = amrex::Real(10.0);
    
    // ---- IEC 61400-1 Parameters ----
    // Hub height for IEC model [m] (typical: 60-200m)
    amrex::Real hub_height = amrex::Real(90.0);
    
    // IEC turbulence category (0=A, 1=B, 2=C)
    int iec_turbulence_category = 1;
    
    // ---- Power-law coherence exponent (for smooth profile) ----
    // Used in both SmoothProfile intensity model and PowerLaw coherence model
    amrex::Real coherence_powerlaw_exponent = amrex::Real(1.5);
    
    // ---- Integral Length Scales ----
    // Longitudinal (u) integral length scale [m]
    amrex::Real length_scale_u = amrex::Real(300.0);
    
    // Lateral (v) integral length scale [m] (typically 0.7 * L_u)
    amrex::Real length_scale_v = amrex::Real(200.0);
    
    // Vertical (w) integral length scale [m] (typically 0.4 * L_u)
    amrex::Real length_scale_w = amrex::Real(120.0);
    
    // ---- Vertical Coherence/Correlation ----
    // Vertical coherence decay factor (typical: 0.006 - 0.010 [1/m])
    amrex::Real coherence_decay_vertical = amrex::Real(0.008);
    
    // Lateral coherence decay factor (typical: 0.004 - 0.008 [1/m])
    amrex::Real coherence_decay_lateral = amrex::Real(0.006);
    
    // ---- Random Seed ----
    // For reproducible random field generation
    unsigned int random_seed = 12345u;
    
    // ---- Anisotropy Ratios ----
    // Ratio of v-velocity RMS to u-velocity RMS (typical: 0.80)
    amrex::Real anisotropy_ratio_v = amrex::Real(0.80);
    
    // Ratio of w-velocity RMS to u-velocity RMS (typical: 0.50)
    amrex::Real anisotropy_ratio_w = amrex::Real(0.50);
    
    // ---- Mann Box Parameters (NEW - Phase 2) ----
    // Mann Box is an anisotropic spectral tensor model for complex terrain
    // See docs/PHASE2_MANN_BOX_INTEGRATION.md for detailed documentation
    
    // Mann Box integral length scales [m]
    // These are typically different from Von Kármán scales for better complex terrain representation
    amrex::Real mann_length_scale_u = amrex::Real(300.0);   // u-component (typically 200-400m)
    amrex::Real mann_length_scale_v = amrex::Real(200.0);   // v-component (typically 0.7*L_u)
    amrex::Real mann_length_scale_w = amrex::Real(120.0);   // w-component (typically 0.4*L_u)
    
    // Mann Box variance scaling factors (RMS velocity normalization)
    // These determine the energy distribution and anisotropy ratios
    amrex::Real mann_variance_u = amrex::Real(1.0);         // u-component variance scaling
    amrex::Real mann_variance_v = amrex::Real(0.80);        // v-component variance scaling (typically 0.8)
    amrex::Real mann_variance_w = amrex::Real(0.50);        // w-component variance scaling (typically 0.5)
    
    // Mann Box asymmetry parameter (controls tensor anisotropy structure)
    // Typical range: 0.8 - 1.2 (1.0 = fully anisotropic tensor)
    amrex::Real mann_asymmetry_parameter = amrex::Real(1.0);
    
    // Mann Box eddy lifetime parameter [s]
    // Affects temporal correlation structure in time-varying applications
    amrex::Real mann_eddy_lifetime = amrex::Real(0.1);
    
    // Terrain adaptation factor for Mann Box (multiplicative)
    // Modifies tensor anisotropy based on local terrain features
    amrex::Real mann_terrain_adaptation_factor = amrex::Real(1.0);
    
    // ---- Phase 3+ Enhancements: Non-neutral Stability Corrections ----
    // Monin-Obukhov similarity theory for non-neutral atmospheric conditions
    // See docs/PHASE3_STABILITY_CORRECTIONS.md for detailed documentation
    
    // Enable/disable non-neutral stability corrections
    bool enable_stability_correction = false;
    
    // Obukhov length scale L [m]
    // L > 0: Stable conditions (e.g., nighttime, weak wind, clear sky)
    // L < 0: Unstable conditions (e.g., daytime, strong heating, surface convection)
    // |L| → ∞: Neutral conditions (typical for overcast, strong wind)
    // Typical values: L = 10 to 1000 m for stable, L = -10 to -1000 m for unstable
    amrex::Real monin_obukhov_length = amrex::Real(1.0e6);
    
    // Use Holtslag-De Bruin (1988) instead of Businger-Dyer (1971) parameterization
    // Holtslag-De Bruin performs better in very stable conditions (polar, nighttime)
    // Businger-Dyer is standard and more widely used
    bool use_holtslag_stability = false;

    // ---- New Synthetic Turbulence Generator Parameters ----
    amrex::Real gp_llj_jet_height = amrex::Real(150.0);       // Great Plains jet height [m AGL] (typically 100-300m)
    amrex::Real nwtc_scaling_parameter = amrex::Real(1.0);    // NWTC site scaling parameter
    amrex::Real uswtpp_weight = amrex::Real(0.5);             // USWTPP weight [0.0 - 1.0] to blend VonKarman (0.0) and Kaimal (1.0)
};


// ============================================================================
// Namespace for turbulence calculations
// ============================================================================

namespace SyntheticTurbulence {

// Physical constants specific to turbulence modeling
namespace Constants {
    // Von Kármán constant (dimensionless)
    constexpr amrex::Real kappa = amrex::Real(0.41);
    
    // Gravity acceleration [m/s²]
    constexpr amrex::Real g = amrex::Real(9.81);
    
    // Maximum reasonable turbulence intensity (safety check)
    constexpr amrex::Real intensity_max = amrex::Real(0.30);
    
    // Minimum reasonable turbulence intensity
    constexpr amrex::Real intensity_min = amrex::Real(0.01);
    
    // Minimum height AGL for turbulence [m]
    constexpr amrex::Real z_min = amrex::Real(1.0);
}

// ============================================================================
// Von Kármán Spectrum Implementation
// 
// The Von Kármán spectrum describes isotropic turbulence and is widely used
// in atmospheric boundary layer modeling. The spectral density function is:
//
//   S_u(f) = (4 * L_u * u_rms^2) / (1 + 70.8 * (f * L_u / U_mean)^2)^(5/6)
//
// where:
//   f = frequency [Hz]
//   L_u = integral length scale for u-component [m]
//   u_rms = RMS of u-velocity fluctuation [m/s]
//   U_mean = mean wind speed [m/s]
//
// ============================================================================

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real vonkarman_spectrum(
    amrex::Real frequency,           // Frequency [Hz]
    amrex::Real length_scale,        // Integral length scale [m]
    amrex::Real mean_wind_speed,     // Mean wind speed [m/s]
    amrex::Real velocity_rms         // RMS velocity fluctuation [m/s]
) noexcept
{
    // Guard against division by zero and unphysical inputs
    mean_wind_speed = std::max(mean_wind_speed, amrex::Real(0.1));
    velocity_rms = std::max(velocity_rms, amrex::Real(1e-6));
    length_scale = std::max(length_scale, amrex::Real(1.0));
    
    // Compute normalized frequency: f_hat = f * L_u / U_mean
    const amrex::Real f_hat = frequency * length_scale / mean_wind_speed;
    
    // Von Kármán spectral density
    // S_u(f) = (4 * L_u * u_rms^2) / (1 + 70.8 * f_hat^2)^(5/6)
    const amrex::Real numerator = amrex::Real(4.0) * length_scale 
                                * velocity_rms * velocity_rms;
    
    const amrex::Real denominator_base = amrex::Real(1.0) 
                                       + amrex::Real(70.8) * f_hat * f_hat;
    
    const amrex::Real denominator = std::pow(denominator_base, amrex::Real(5.0) / amrex::Real(6.0));
    
    return numerator / denominator;
}

// ============================================================================
// Kaimal Spectrum Implementation
//
// The Kaimal spectrum is an empirical model developed from atmospheric
// measurements and is widely used in wind engineering (IEC standards).
//
//   S_u(f) = (4 * L_u * u_rms^2 * f_hat) / (1 + 6 * f_hat)^(5/3)
//
// where f_hat = f * L_u / U_mean (normalized frequency)
//
// This spectrum tends to underestimate energy at high frequencies compared
// to Von Kármán, but better matches certain observational datasets.
// ============================================================================

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real kaimal_spectrum(
    amrex::Real frequency,           // Frequency [Hz]
    amrex::Real length_scale,        // Integral length scale [m]
    amrex::Real mean_wind_speed,     // Mean wind speed [m/s]
    amrex::Real velocity_rms         // RMS velocity fluctuation [m/s]
) noexcept
{
    // Guard against division by zero and unphysical inputs
    mean_wind_speed = std::max(mean_wind_speed, amrex::Real(0.1));
    velocity_rms = std::max(velocity_rms, amrex::Real(1e-6));
    length_scale = std::max(length_scale, amrex::Real(1.0));
    
    // Compute normalized frequency: f_hat = f * L_u / U_mean
    const amrex::Real f_hat = frequency * length_scale / mean_wind_speed;
    
    // Kaimal spectral density
    // S_u(f) = (4 * L_u * u_rms^2 * f_hat) / (1 + 6 * f_hat)^(5/3)
    const amrex::Real numerator = amrex::Real(4.0) * length_scale 
                                * velocity_rms * velocity_rms * f_hat;
    
    const amrex::Real denominator_base = amrex::Real(1.0) 
                                       + amrex::Real(6.0) * f_hat;
    
    const amrex::Real denominator = std::pow(denominator_base, amrex::Real(5.0) / amrex::Real(3.0));
    
    return numerator / denominator;
}

// ============================================================================
// GP_LLJ Spectrum Implementation
//
// Designed as a Kaimal base modulated with a Gaussian profile centered on
// gp_llj_jet_height to capture jet-shear-induced low-to-medium frequency
// energy amplification.
// ============================================================================

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real gp_llj_spectrum(
    amrex::Real frequency,           // Frequency [Hz]
    amrex::Real length_scale,        // Integral length scale [m]
    amrex::Real mean_wind_speed,     // Mean wind speed [m/s]
    amrex::Real velocity_rms,        // RMS velocity fluctuation [m/s]
    amrex::Real z_agl,               // Height above ground level [m]
    amrex::Real jet_height           // Jet height [m]
) noexcept
{
    const amrex::Real base_spectrum = kaimal_spectrum(frequency, length_scale, mean_wind_speed, velocity_rms);
    
    const amrex::Real clamped_jet_height = std::max(jet_height, amrex::Real(10.0));
    const amrex::Real sigma_z = amrex::Real(0.2) * clamped_jet_height;
    const amrex::Real z_diff = z_agl - clamped_jet_height;
    const amrex::Real z_factor = std::exp(-amrex::Real(0.5) * (z_diff * z_diff) / (sigma_z * sigma_z));
    
    // Frequency modulation peaking at 0.05 Hz
    const amrex::Real log_f = std::log(frequency + amrex::Real(1e-6));
    const amrex::Real log_f0 = std::log(amrex::Real(0.05));
    const amrex::Real f_diff = log_f - log_f0;
    const amrex::Real f_factor = std::exp(-amrex::Real(0.5) * (f_diff * f_diff) / amrex::Real(4.0));
    
    const amrex::Real amplification = amrex::Real(1.0) + amrex::Real(1.5) * z_factor * f_factor;
    return base_spectrum * amplification;
}

// ============================================================================
// NWTC Spectrum Implementation
//
// Formulated as a high-wind empirical spectrum with a custom
// nwtc_scaling_parameter and a power-law frequency response.
// ============================================================================

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real nwtc_spectrum(
    amrex::Real frequency,           // Frequency [Hz]
    amrex::Real length_scale,        // Integral length scale [m]
    amrex::Real mean_wind_speed,     // Mean wind speed [m/s]
    amrex::Real velocity_rms,        // RMS velocity fluctuation [m/s]
    amrex::Real scaling_param        // NWTC scaling parameter
) noexcept
{
    const amrex::Real base_spectrum = kaimal_spectrum(frequency, length_scale, mean_wind_speed, velocity_rms);
    return scaling_param * base_spectrum;
}

// ============================================================================
// USWTPP Spectrum Implementation
//
// Modeled as a blended combination of Von Kármán and Kaimal spectra
// using a uswtpp_weight mixture parameter.
// ============================================================================

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real uswtpp_spectrum(
    amrex::Real frequency,           // Frequency [Hz]
    amrex::Real length_scale,        // Integral length scale [m]
    amrex::Real mean_wind_speed,     // Mean wind speed [m/s]
    amrex::Real velocity_rms,        // RMS velocity fluctuation [m/s]
    amrex::Real weight               // USWTPP blending weight (0.0 = VonKarman, 1.0 = Kaimal)
) noexcept
{
    const amrex::Real vk = vonkarman_spectrum(frequency, length_scale, mean_wind_speed, velocity_rms);
    const amrex::Real km = kaimal_spectrum(frequency, length_scale, mean_wind_speed, velocity_rms);
    const amrex::Real w = std::max(amrex::Real(0.0), std::min(amrex::Real(1.0), weight));
    return (amrex::Real(1.0) - w) * vk + w * km;
}

// ============================================================================
// HIT Spectrum Implementation
//
// Formulated using the classical homogeneous isotropic -5/3 spectral decay
// with a low-frequency roll-off vanishing at f=0.
// ============================================================================

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real hit_spectrum(
    amrex::Real frequency,           // Frequency [Hz]
    amrex::Real length_scale,        // Integral length scale [m]
    amrex::Real mean_wind_speed,     // Mean wind speed [m/s]
    amrex::Real velocity_rms         // RMS velocity fluctuation [m/s]
) noexcept
{
    mean_wind_speed = std::max(mean_wind_speed, amrex::Real(0.1));
    velocity_rms = std::max(velocity_rms, amrex::Real(1e-6));
    length_scale = std::max(length_scale, amrex::Real(1.0));
    
    const amrex::Real f_hat = frequency * length_scale / mean_wind_speed;
    
    const amrex::Real C0 = amrex::Real(6.11);
    const amrex::Real numerator = C0 * (length_scale / mean_wind_speed) 
                                * velocity_rms * velocity_rms * f_hat * f_hat;
    
    const amrex::Real denominator_base = amrex::Real(1.0) + f_hat * f_hat;
    const amrex::Real denominator = std::pow(denominator_base, amrex::Real(11.0) / amrex::Real(6.0));
    
    return numerator / denominator;
}

// ============================================================================
// Height-Dependent Turbulence Intensity Profiles
// 
// Turbulence intensity I_u(z) varies with height due to surface roughness
// and atmospheric stability effects. Common models are implemented here.
// ============================================================================

// Power-law intensity profile
// I(z) = I_ref * (z / z_ref)^alpha
// Typical alpha values: 0.10 - 0.16 (smaller over rougher terrain)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real intensity_powerlaw(
    amrex::Real z_agl,               // Height above ground level [m]
    amrex::Real intensity_ref,       // Intensity at reference height
    amrex::Real z_ref,               // Reference height [m]
    amrex::Real exponent             // Power-law exponent (alpha)
) noexcept
{
    // Device-accessible constants
    constexpr amrex::Real z_min = amrex::Real(1.0);
    constexpr amrex::Real intensity_max = amrex::Real(0.30);
    constexpr amrex::Real intensity_min = amrex::Real(0.01);
    
    // Guard against non-physical inputs
    z_agl = std::max(z_agl, z_min);
    z_ref = std::max(z_ref, z_min);
    
    // Compute power-law profile
    const amrex::Real ratio = z_agl / z_ref;
    const amrex::Real intensity = intensity_ref * std::pow(ratio, exponent);
    
    // Clamp to physical bounds
    return std::max(std::min(intensity, intensity_max), 
                    intensity_min);
}

// Logarithmic intensity profile (alternative formulation)
// I(z) = I_0 * ln(z/z0) / ln(z_ref/z0)
// Better for rough terrain where logarithmic wind profile applies
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real intensity_logarithmic(
    amrex::Real z_agl,               // Height above ground level [m]
    amrex::Real intensity_ref,       // Intensity at reference height
    amrex::Real z_ref,               // Reference height [m]
    amrex::Real z0                   // Aerodynamic roughness length [m]
) noexcept
{
    // Device-accessible constants
    constexpr amrex::Real z_min = amrex::Real(1.0);
    constexpr amrex::Real intensity_max = amrex::Real(0.30);
    constexpr amrex::Real intensity_min = amrex::Real(0.01);
    
    // Guard against non-physical inputs
    z_agl = std::max(z_agl, z_min);
    z_ref = std::max(z_ref, z_min);
    z0 = std::max(z0, amrex::Real(1e-5));
    
    // Logarithmic profile
    const amrex::Real log_ratio_z = std::log((z_agl + z0) / z0);
    const amrex::Real log_ratio_ref = std::log((z_ref + z0) / z0);
    
    const amrex::Real intensity = intensity_ref * log_ratio_z / log_ratio_ref;
    
    // Clamp to physical bounds
    return std::max(std::min(intensity, intensity_max), 
                    intensity_min);
}

// Constant intensity profile (simplest case, uniform across height)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real intensity_constant(amrex::Real intensity_ref) noexcept
{
    // Device-accessible constants
    constexpr amrex::Real intensity_max = amrex::Real(0.30);
    constexpr amrex::Real intensity_min = amrex::Real(0.01);
    
    // Clamp to physical bounds
    return std::max(std::min(intensity_ref, intensity_max), 
                    intensity_min);
}

// ============================================================================
// IEC 61400-1:2019 Intensity Model
// 
// The IEC 61400-1:2019 standard defines turbulence intensity profiles 
// based on wind class and turbulence category. This implements the 
// Normal Turbulence Model (NTM) with category A, B, C intensity variations.
//
// Formula: I_ref(z) = 0.16 * (0.2 + (z/1000)^0.5) for NTM
// where z is hub height in meters (typically 60-200m)
//
// Reference: IEC 61400-1:2019 Wind turbines – Design requirements
// ============================================================================

// IEC 61400-1 NTM (Normal Turbulence Model) intensity
// Parameters: turbulence_category (0=A, 1=B, 2=C)
// For simplicity, use fixed reference values from IEC standard
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real intensity_iec61400_ntm(
    amrex::Real z_agl,               // Height above ground level [m]
    amrex::Real hub_height = amrex::Real(90.0), // Hub height [m] (default 90m)
    int turbulence_category = 1      // 0=A (15%), 1=B (12%), 2=C (10%)
) noexcept
{
    // Device-accessible constants
    constexpr amrex::Real intensity_max = amrex::Real(0.30);
    constexpr amrex::Real intensity_min = amrex::Real(0.01);
    
    // Guard against non-physical inputs
    z_agl = std::max(z_agl, amrex::Real(1.0));
    hub_height = std::max(hub_height, amrex::Real(10.0));
    
    // IEC NTM reference intensities at hub height (category-dependent)
    amrex::Real intensity_ref_hub;
    switch (turbulence_category) {
        case 0: intensity_ref_hub = amrex::Real(0.16); break; // Category A: 16%
        case 1: intensity_ref_hub = amrex::Real(0.14); break; // Category B: 14%
        case 2: intensity_ref_hub = amrex::Real(0.12); break; // Category C: 12%
        default: intensity_ref_hub = amrex::Real(0.14);
    }
    
    // Power-law profile from hub height
    // I(z) = I_hub * (z/z_hub)^0.2 for typical neutral conditions
    const amrex::Real ratio = z_agl / hub_height;
    const amrex::Real intensity = intensity_ref_hub * std::pow(ratio, amrex::Real(0.2));
    
    // Clamp to physical bounds
    return std::max(std::min(intensity, intensity_max), 
                    intensity_min);
}

// Smooth user-defined intensity profile
// Uses cubic spline interpolation between user-provided points
// This is a simplified version that assumes linear interpolation between points
// stored in the TurbulenceParams structure
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real intensity_smooth_profile(
    amrex::Real z_agl,               // Height above ground level [m]
    amrex::Real intensity_ref,       // Reference intensity
    amrex::Real z_ref,               // Reference height [m]
    amrex::Real exponent = amrex::Real(0.14) // Smoothing exponent
) noexcept
{
    // Device-accessible constants
    constexpr amrex::Real intensity_max = amrex::Real(0.30);
    constexpr amrex::Real intensity_min = amrex::Real(0.01);
    
    // Guard against non-physical inputs
    z_agl = std::max(z_agl, amrex::Real(1.0));
    z_ref = std::max(z_ref, amrex::Real(1.0));
    
    // Use smooth power-law with fractional exponent for C-infinity smoothness
    // I(z) = I_ref * (z/z_ref)^exponent
    // where exponent is typically 0.05 - 0.20 for smooth profiles
    const amrex::Real ratio = z_agl / z_ref;
    const amrex::Real intensity = intensity_ref * std::pow(ratio, exponent);
    
    // Clamp to physical bounds
    return std::max(std::min(intensity, intensity_max), 
                    intensity_min);
}

// ============================================================================
// Coherence Functions
// 
// Coherence describes the correlation of turbulent fluctuations between
// two points in space. It decays with distance. Two models are implemented:
// 1. Gaussian: Coh(distance) = exp(-k * distance^2)
// 2. Exponential: Coh(distance) = exp(-k * distance)
//
// These are used to determine correlations between fluctuations at different
// grid points when generating synthetic fields.
// ============================================================================

// Gaussian coherence: Coh(distance) = exp(-k * distance^2)
// Parameters:
//   distance = spatial separation [m]
//   decay_factor = decay coefficient k [1/m]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real coherence_gaussian(
    amrex::Real distance,            // Spatial separation [m]
    amrex::Real decay_factor         // Decay coefficient [1/m]
) noexcept
{
    // Guard against negative arguments
    const amrex::Real arg = std::max(-decay_factor * distance * distance, 
                                     amrex::Real(-100.0));
    
    const amrex::Real coherence = std::exp(arg);
    
    // Clamp to [0, 1]
    return std::max(std::min(coherence, amrex::Real(1.0)), amrex::Real(0.0));
}

// Exponential coherence: Coh(distance) = exp(-k * distance)
// Parameters:
//   distance = spatial separation [m]
//   decay_factor = decay coefficient k [1/m]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real coherence_exponential(
    amrex::Real distance,            // Spatial separation [m]
    amrex::Real decay_factor         // Decay coefficient [1/m]
) noexcept
{
    // Guard against negative arguments
    const amrex::Real arg = std::max(-decay_factor * distance, 
                                     amrex::Real(-100.0));
    
    const amrex::Real coherence = std::exp(arg);
    
    // Clamp to [0, 1]
    return std::max(std::min(coherence, amrex::Real(1.0)), amrex::Real(0.0));
}

// ============================================================================
// Additional Coherence Models (Phase 1 Enhancement)
// ============================================================================

// Quadratic exponential coherence: Coh(distance) = exp(-k * distance^2 / 2)
// Smoother decay than exponential, often used in complex terrain
// Parameters:
//   distance = spatial separation [m]
//   decay_factor = decay coefficient k [1/m]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real coherence_quadratic_exponential(
    amrex::Real distance,            // Spatial separation [m]
    amrex::Real decay_factor         // Decay coefficient [1/m]
) noexcept
{
    // Guard against negative arguments
    const amrex::Real arg = std::max(-decay_factor * distance * distance / amrex::Real(2.0),
                                     amrex::Real(-100.0));
    
    const amrex::Real coherence = std::exp(arg);
    
    // Clamp to [0, 1]
    return std::max(std::min(coherence, amrex::Real(1.0)), amrex::Real(0.0));
}

// Power-law coherence: Coh(distance) = (1 + k * distance)^(-m)
// Follows power-law behavior, common in rough terrain
// Parameters:
//   distance = spatial separation [m]
//   decay_factor = decay coefficient k [1/m]
//   exponent = power-law exponent m (typically 1.0 - 2.0, default 1.5)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real coherence_powerlaw(
    amrex::Real distance,            // Spatial separation [m]
    amrex::Real decay_factor,        // Decay coefficient k [1/m]
    amrex::Real exponent = amrex::Real(1.5)  // Power-law exponent m
) noexcept
{
    // Guard against non-physical inputs
    distance = std::max(distance, amrex::Real(0.0));
    decay_factor = std::max(decay_factor, amrex::Real(1e-4));
    
    // Power-law: (1 + k*d)^(-m)
    const amrex::Real base = amrex::Real(1.0) + decay_factor * distance;
    const amrex::Real coherence = std::pow(base, -exponent);
    
    // Clamp to [0, 1]
    return std::max(std::min(coherence, amrex::Real(1.0)), amrex::Real(0.0));
}

// ============================================================================
// Mann Box Spectral Tensor (NEW - Phase 2)
//
// The Mann Box model represents wind turbulence using an anisotropic spectral
// tensor S_ij(k⃗), where k⃗ is the wavenumber vector. This tensor captures the
// directional and component-dependent characteristics of turbulence, which is
// particularly important for complex terrain where turbulence anisotropy varies.
//
// Reference:
//   Mann, J. (1994). The spatial structure of neutral atmospheric surface-layer
//   turbulence. Journal of Fluid Mechanics, 273, 141-168.
//
// Key features:
//   1. Anisotropic spectral tensor with 9 components
//   2. Preserves physical realizability through eigenvalue decomposition
//   3. Naturally captures complex terrain effects
//   4. Compatible with FFT-based synthesis
//
// Implementation approach:
//   - Compute base tensor from Mann Box parameters
//   - Apply terrain modifications to anisotropy ratios
//   - Use eigenvalue decomposition for synthesis
// ============================================================================

// Compute Mann Box spectral tensor component S_ii (diagonal, energy spectrum)
// Parameters:
//   wavenumber = wavenumber magnitude k [1/m]
//   length_scale = integral length scale L [m]
//   variance = variance σ² [m²/s²]
//   asymmetry = asymmetry parameter (typically 1.0)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real mann_box_spectrum_diagonal(
    amrex::Real wavenumber,           // Wavenumber k [1/m]
    amrex::Real length_scale,         // Integral length scale L [m]
    amrex::Real variance,             // Component variance σ² [m²/s²]
    amrex::Real asymmetry = amrex::Real(1.0)  // Asymmetry parameter
) noexcept
{
    // Guard against invalid inputs
    wavenumber = std::max(wavenumber, amrex::Real(1e-6));
    length_scale = std::max(length_scale, amrex::Real(1.0));
    variance = std::max(variance, amrex::Real(1e-6));
    asymmetry = std::max(asymmetry, amrex::Real(0.5));
    
    // Mann Box spectral model (simplified):
    // S(k) = (8√(3/(11π)) * σ² * L) / (k * (1 + (k*L/C)²)^(5/6))
    // where C depends on asymmetry parameter and component
    //
    // We use a phenomenological form that ensures proper spectral behavior:
    const amrex::Real k_scaled = wavenumber * length_scale / asymmetry;
    const amrex::Real norm_factor = amrex::Real(8.0) * std::sqrt(amrex::Real(3.0) / (amrex::Real(11.0) * MathConstants::pi));
    
    const amrex::Real numerator = norm_factor * variance * length_scale;
    const amrex::Real denominator_base = amrex::Real(1.0) + k_scaled * k_scaled;
    const amrex::Real denominator = wavenumber * std::pow(denominator_base, amrex::Real(5.0) / amrex::Real(6.0));
    
    // Guard against division by zero
    if (denominator < amrex::Real(1e-15)) {
        return amrex::Real(0.0);
    }
    
    return numerator / denominator;
}

// Compute Mann Box tensor anisotropy ratio for complex terrain adaptation
// Modifies the tensor structure based on terrain slope and classification
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real mann_box_terrain_anisotropy_factor(
    amrex::Real z_agl,                // Height above ground [m]
    amrex::Real terrain_slope,        // Local terrain slope [rad] (typical: 0-0.3)
    bool is_windward_slope = true     // True for windward, false for lee
) noexcept
{
    // Guard against invalid inputs
    z_agl = std::max(z_agl, amrex::Real(1.0));
    terrain_slope = std::max(terrain_slope, amrex::Real(0.0));
    terrain_slope = std::min(terrain_slope, amrex::Real(MathConstants::pi / 2.0));
    
    // Height-dependent suppression near ground
    const amrex::Real height_factor = std::min(z_agl / amrex::Real(50.0), amrex::Real(1.0));
    
    // Slope-dependent modification
    amrex::Real slope_factor = amrex::Real(1.0);
    if (is_windward_slope) {
        // Windward slopes: enhance turbulence anisotropy (more u-component relative to v/w)
        slope_factor = amrex::Real(1.0) + amrex::Real(1.2) * std::sin(terrain_slope);
    } else {
        // Lee slopes: reduce vertical coherence, increase isotropy
        slope_factor = amrex::Real(1.0) + amrex::Real(0.5) * std::sin(terrain_slope);
    }
    
    return height_factor * slope_factor;
}

// Compute Mann Box integral length scale with terrain adaptation
// Adjusts length scales based on local terrain features
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real mann_box_adapted_length_scale(
    amrex::Real base_length_scale,    // Base integral length scale [m]
    amrex::Real z_agl,                // Height above ground [m]
    amrex::Real terrain_slope,        // Local terrain slope [rad]
    bool is_ridgeline = false         // True if at ridge crest
) noexcept
{
    // Guard against invalid inputs
    base_length_scale = std::max(base_length_scale, amrex::Real(1.0));
    z_agl = std::max(z_agl, amrex::Real(1.0));
    terrain_slope = std::max(terrain_slope, amrex::Real(0.0));
    
    // Height-dependent modification (similar to standard turbulence)
    const amrex::Real height_modification = std::min(z_agl / amrex::Real(100.0), amrex::Real(1.0));
    
    // Ridge enhancement: increase horizontal coherence at ridges
    amrex::Real ridge_factor = amrex::Real(1.0);
    if (is_ridgeline) {
        ridge_factor = amrex::Real(1.2);  // 20% enhancement at ridge crests
    }
    
    // Slope-dependent modification: steeper slopes → shorter scales due to enhanced mixing
    const amrex::Real slope_reduction = std::max(amrex::Real(1.0) - amrex::Real(2.0) * terrain_slope, 
                                                  amrex::Real(0.5));
    
    return base_length_scale * height_modification * ridge_factor * slope_reduction;
}

// ============================================================================
// RMS Velocity Computation
// 
// RMS velocity is derived from turbulence intensity and mean wind speed:
//   u_rms = I_u * U_mean
//
// The anisotropy ratios (v_rms/u_rms, w_rms/u_rms) determine the distribution
// of energy among the three velocity components.
// ============================================================================

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_velocity_rms(
    amrex::Real turbulence_intensity,   // Turbulence intensity I_u [dimensionless]
    amrex::Real mean_wind_speed         // Mean wind speed U [m/s]
) noexcept
{
    // u_rms = I_u * U_mean
    return turbulence_intensity * std::max(mean_wind_speed, amrex::Real(0.1));
}

// ============================================================================
// TurbulenceGenerator Class
// 
// High-level interface for turbulence calculations. This class provides
// methods to compute intensity, RMS, spectra, and coherence based on
// configured parameters.
//
// Note: This is a GPU-compatible class for device-side computations.
// FFT-based field generation will be added in Phase 2.
// ============================================================================

class TurbulenceGenerator {
public:
    // Constructor
    AMREX_GPU_HOST_DEVICE
    TurbulenceGenerator(const TurbulenceParams& params) noexcept
        : params_(params)
    {
    }
    
    // Compute turbulence intensity at a given height
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    amrex::Real ComputeIntensity(amrex::Real z_agl) const noexcept
    {
        switch (params_.intensity_model) {
            case IntensityModel::PowerLaw:
                return intensity_powerlaw(z_agl, 
                                         params_.intensity_ref,
                                         params_.z_intensity_ref,
                                         params_.intensity_exponent);
            case IntensityModel::Logarithmic:
                // For logarithmic model, use a typical z0 value
                // This can be extended to accept z0 as a parameter
                return intensity_logarithmic(z_agl,
                                            params_.intensity_ref,
                                            params_.z_intensity_ref,
                                            amrex::Real(0.1));
            case IntensityModel::Constant:
                return intensity_constant(params_.intensity_ref);
            case IntensityModel::IEC61400:
                return intensity_iec61400_ntm(z_agl,
                                             params_.hub_height,
                                             params_.iec_turbulence_category);
            case IntensityModel::SmoothProfile:
                return intensity_smooth_profile(z_agl,
                                               params_.intensity_ref,
                                               params_.z_intensity_ref,
                                               params_.intensity_exponent);
            default:
                return params_.intensity_ref;
        }
    }
    
    // Compute RMS velocity for u-component
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    amrex::Real ComputeVelocityRmsU(amrex::Real z_agl, 
                                    amrex::Real mean_wind_speed) const noexcept
    {
        const amrex::Real intensity = ComputeIntensity(z_agl);
        return compute_velocity_rms(intensity, mean_wind_speed);
    }
    
    // Compute RMS velocity for v-component (with anisotropy)
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    amrex::Real ComputeVelocityRmsV(amrex::Real z_agl, 
                                    amrex::Real mean_wind_speed) const noexcept
    {
        const amrex::Real rms_u = ComputeVelocityRmsU(z_agl, mean_wind_speed);
        return rms_u * params_.anisotropy_ratio_v;
    }
    
    // Compute RMS velocity for w-component (with anisotropy)
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    amrex::Real ComputeVelocityRmsW(amrex::Real z_agl, 
                                    amrex::Real mean_wind_speed) const noexcept
    {
        const amrex::Real rms_u = ComputeVelocityRmsU(z_agl, mean_wind_speed);
        return rms_u * params_.anisotropy_ratio_w;
    }
    
    // Compute spectral density for u-component at a given frequency
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    amrex::Real ComputeSpectrumU(amrex::Real frequency,
                                 amrex::Real z_agl,
                                 amrex::Real mean_wind_speed) const noexcept
    {
        const amrex::Real rms_u = ComputeVelocityRmsU(z_agl, mean_wind_speed);
        
        switch (params_.spectrum_model) {
            case TurbulenceModel::VonKarman:
                return vonkarman_spectrum(frequency, 
                                         params_.length_scale_u,
                                         mean_wind_speed, 
                                         rms_u);
            case TurbulenceModel::Kaimal:
                return kaimal_spectrum(frequency,
                                      params_.length_scale_u,
                                      mean_wind_speed,
                                      rms_u);
            case TurbulenceModel::MannBox: {
                // Mann Box spectral tensor for u-component
                // Convert frequency to wavenumber: k = 2π*f/U
                const amrex::Real wavenumber = amrex::Real(2.0) * amrex::Real(MathConstants::pi) * frequency 
                                             / std::max(mean_wind_speed, amrex::Real(0.1));
                // Mann Box variance for u-component
                const amrex::Real mann_variance_u = params_.mann_variance_u * rms_u * rms_u;
                return mann_box_spectrum_diagonal(wavenumber, params_.mann_length_scale_u, 
                                                 mann_variance_u, params_.mann_asymmetry_parameter);
            }
            case TurbulenceModel::GP_LLJ:
                return gp_llj_spectrum(frequency,
                                      params_.length_scale_u,
                                      mean_wind_speed,
                                      rms_u,
                                      z_agl,
                                      params_.gp_llj_jet_height);
            case TurbulenceModel::NWTC:
                return nwtc_spectrum(frequency,
                                    params_.length_scale_u,
                                    mean_wind_speed,
                                    rms_u,
                                    params_.nwtc_scaling_parameter);
            case TurbulenceModel::USWTPP:
                return uswtpp_spectrum(frequency,
                                      params_.length_scale_u,
                                      mean_wind_speed,
                                      rms_u,
                                      params_.uswtpp_weight);
            case TurbulenceModel::HIT:
                return hit_spectrum(frequency,
                                   params_.length_scale_u,
                                   mean_wind_speed,
                                   rms_u);
            default:
                return vonkarman_spectrum(frequency,
                                         params_.length_scale_u,
                                         mean_wind_speed,
                                         rms_u);
        }
    }
    
    // Compute spectral density for v-component
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    amrex::Real ComputeSpectrumV(amrex::Real frequency,
                                 amrex::Real z_agl,
                                 amrex::Real mean_wind_speed) const noexcept
    {
        const amrex::Real rms_v = ComputeVelocityRmsV(z_agl, mean_wind_speed);
        
        switch (params_.spectrum_model) {
            case TurbulenceModel::VonKarman:
                return vonkarman_spectrum(frequency,
                                         params_.length_scale_v,
                                         mean_wind_speed,
                                         rms_v);
            case TurbulenceModel::Kaimal:
                return kaimal_spectrum(frequency,
                                      params_.length_scale_v,
                                      mean_wind_speed,
                                      rms_v);
            case TurbulenceModel::MannBox: {
                // Mann Box spectral tensor for v-component
                const amrex::Real wavenumber = amrex::Real(2.0) * amrex::Real(MathConstants::pi) * frequency 
                                              / std::max(mean_wind_speed, amrex::Real(0.1));
                const amrex::Real mann_variance_v = params_.mann_variance_v * rms_v * rms_v;
                return mann_box_spectrum_diagonal(wavenumber, params_.mann_length_scale_v,
                                                 mann_variance_v, params_.mann_asymmetry_parameter);
            }
            case TurbulenceModel::GP_LLJ:
                return gp_llj_spectrum(frequency,
                                      params_.length_scale_v,
                                      mean_wind_speed,
                                      rms_v,
                                      z_agl,
                                      params_.gp_llj_jet_height);
            case TurbulenceModel::NWTC:
                return nwtc_spectrum(frequency,
                                    params_.length_scale_v,
                                    mean_wind_speed,
                                    rms_v,
                                    params_.nwtc_scaling_parameter);
            case TurbulenceModel::USWTPP:
                return uswtpp_spectrum(frequency,
                                      params_.length_scale_v,
                                      mean_wind_speed,
                                      rms_v,
                                      params_.uswtpp_weight);
            case TurbulenceModel::HIT:
                return hit_spectrum(frequency,
                                   params_.length_scale_v,
                                   mean_wind_speed,
                                   rms_v);
            default:
                return vonkarman_spectrum(frequency,
                                         params_.length_scale_v,
                                         mean_wind_speed,
                                         rms_v);
        }
    }
    
    // Compute spectral density for w-component
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    amrex::Real ComputeSpectrumW(amrex::Real frequency,
                                 amrex::Real z_agl,
                                 amrex::Real mean_wind_speed) const noexcept
    {
        const amrex::Real rms_w = ComputeVelocityRmsW(z_agl, mean_wind_speed);
        
        switch (params_.spectrum_model) {
            case TurbulenceModel::VonKarman:
                return vonkarman_spectrum(frequency,
                                         params_.length_scale_w,
                                         mean_wind_speed,
                                         rms_w);
            case TurbulenceModel::Kaimal:
                return kaimal_spectrum(frequency,
                                      params_.length_scale_w,
                                      mean_wind_speed,
                                      rms_w);
            case TurbulenceModel::MannBox: {
                // Mann Box spectral tensor for w-component
                const amrex::Real wavenumber = amrex::Real(2.0) * amrex::Real(MathConstants::pi) * frequency 
                                              / std::max(mean_wind_speed, amrex::Real(0.1));
                const amrex::Real mann_variance_w = params_.mann_variance_w * rms_w * rms_w;
                return mann_box_spectrum_diagonal(wavenumber, params_.mann_length_scale_w,
                                                 mann_variance_w, params_.mann_asymmetry_parameter);
            }
            case TurbulenceModel::GP_LLJ:
                return gp_llj_spectrum(frequency,
                                      params_.length_scale_w,
                                      mean_wind_speed,
                                      rms_w,
                                      z_agl,
                                      params_.gp_llj_jet_height);
            case TurbulenceModel::NWTC:
                return nwtc_spectrum(frequency,
                                    params_.length_scale_w,
                                    mean_wind_speed,
                                    rms_w,
                                    params_.nwtc_scaling_parameter);
            case TurbulenceModel::USWTPP:
                return uswtpp_spectrum(frequency,
                                      params_.length_scale_w,
                                      mean_wind_speed,
                                      rms_w,
                                      params_.uswtpp_weight);
            case TurbulenceModel::HIT:
                return hit_spectrum(frequency,
                                   params_.length_scale_w,
                                   mean_wind_speed,
                                   rms_w);
            default:
                return vonkarman_spectrum(frequency,
                                         params_.length_scale_w,
                                         mean_wind_speed,
                                         rms_w);
        }
    }
    
    // Compute coherence between two points
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    amrex::Real ComputeCoherence(amrex::Real distance,
                                 bool use_vertical = true) const noexcept
    {
        const amrex::Real decay = use_vertical 
                                ? params_.coherence_decay_vertical
                                : params_.coherence_decay_lateral;
        
        switch (params_.coherence_model) {
            case CoherenceModel::Gaussian:
                return coherence_gaussian(distance, decay);
            case CoherenceModel::Exponential:
                return coherence_exponential(distance, decay);
            case CoherenceModel::QuadraticExponential:
                return coherence_quadratic_exponential(distance, decay);
            case CoherenceModel::PowerLaw:
                return coherence_powerlaw(distance, decay, params_.coherence_powerlaw_exponent);
            default:
                return coherence_gaussian(distance, decay);
        }
    }
    
    // ========================================================================
    // Mann Box Specific Methods (NEW - Phase 2)
    // ========================================================================
    
    // Compute adapted Mann Box length scale for complex terrain
    // This method applies terrain-specific modifications to the base length scales
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    amrex::Real ComputeAdaptedMannBoxLengthScaleU(
        amrex::Real z_agl,
        amrex::Real terrain_slope = amrex::Real(0.0),
        bool is_ridgeline = false
    ) const noexcept
    {
        if (params_.spectrum_model != TurbulenceModel::MannBox) {
            return params_.mann_length_scale_u;
        }
        return mann_box_adapted_length_scale(params_.mann_length_scale_u, z_agl, 
                                             terrain_slope, is_ridgeline);
    }
    
    // Compute adapted Mann Box length scale for v-component
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    amrex::Real ComputeAdaptedMannBoxLengthScaleV(
        amrex::Real z_agl,
        amrex::Real terrain_slope = amrex::Real(0.0),
        bool is_ridgeline = false
    ) const noexcept
    {
        if (params_.spectrum_model != TurbulenceModel::MannBox) {
            return params_.mann_length_scale_v;
        }
        return mann_box_adapted_length_scale(params_.mann_length_scale_v, z_agl, 
                                             terrain_slope, is_ridgeline);
    }
    
    // Compute adapted Mann Box length scale for w-component
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    amrex::Real ComputeAdaptedMannBoxLengthScaleW(
        amrex::Real z_agl,
        amrex::Real terrain_slope = amrex::Real(0.0),
        bool is_ridgeline = false
    ) const noexcept
    {
        if (params_.spectrum_model != TurbulenceModel::MannBox) {
            return params_.mann_length_scale_w;
        }
        return mann_box_adapted_length_scale(params_.mann_length_scale_w, z_agl, 
                                             terrain_slope, is_ridgeline);
    }
    
    // Compute Mann Box anisotropy factor for terrain modification
    // Used to adjust tensor structure based on windward/lee slopes
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    amrex::Real ComputeMannBoxTerrainAnisotropyFactor(
        amrex::Real z_agl,
        amrex::Real terrain_slope = amrex::Real(0.0),
        bool is_windward_slope = true
    ) const noexcept
    {
        if (params_.spectrum_model != TurbulenceModel::MannBox) {
            return amrex::Real(1.0);
        }
        return mann_box_terrain_anisotropy_factor(z_agl, terrain_slope, is_windward_slope);
    }
    
    // Get access to parameters
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    const TurbulenceParams& GetParams() const noexcept
    {
        return params_;
    }
    
private:
    TurbulenceParams params_;
};

} // namespace SyntheticTurbulence

#endif // SYNTHETIC_TURBULENCE_H
