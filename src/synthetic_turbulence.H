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
//   1. Von Kármán spectrum - Isotropic turbulence model
//   2. Kaimal spectrum - Alternative, commonly used in wind engineering
//   3. Coherence functions - Gaussian and exponential decay models
//   4. Height-dependent intensity profiles - Power-law or logarithmic
//   5. Random field generation framework - FFT-ready utilities
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
//   turb_params.model = TurbulenceModel::VonKarman;
//   turb_params.intensity_profile = IntensityModel::PowerLaw;
//   
//   TurbulenceGenerator turb_gen(turb_params);
//   turb_gen.ComputeIntensity(z_agl);
//   turb_gen.ComputeSpectra(freq_array, u_spectrum, v_spectrum, w_spectrum);
//   // Fluctuations generated via FFT in subsequent phases
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
    Kaimal = 1        // Kaimal et al. (1972) empirical spectrum
};

enum class IntensityModel {
    PowerLaw = 0,     // I(z) = I_ref * (z/z_ref)^alpha
    Logarithmic = 1,  // I(z) = I_0 * ln(z/z0) / ln(z_ref/z0)
    Constant = 2      // I(z) = constant
};

enum class CoherenceModel {
    Gaussian = 0,     // Exponential decay: Coh = exp(-k*distance)
    Exponential = 1   // Gaussian: Coh = exp(-k*distance^2)
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
            default:
                return coherence_gaussian(distance, decay);
        }
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
