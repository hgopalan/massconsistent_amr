// ============================================================================
// spectral_validation.H
// Spectral properties validation
//
// Validates spectral characteristics of generated turbulent fluctuations:
// 1. Energy spectrum matches Von Kármán or Kaimal models
// 2. Spectral energy conservation (Parseval's theorem)
// 3. Integral length scale recovery from spectra
// 4. Frequency domain characteristics (peak frequency, decay rates)
//
// References:
//   - Von Kármán, T. (1948). Progress in the statistical theory of turbulence.
//   - Kaimal et al. (1972). Spectral characteristics of surface-layer turbulence.
//   - IEC 61400-1:2019 - Wind turbine design standards
//   - NREL TurbSim documentation
//
// ============================================================================

#ifndef SPECTRAL_VALIDATION_H
#define SPECTRAL_VALIDATION_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include "synthetic_turbulence.H"
#include "math_constants.H"
#include <cmath>
#include <algorithm>
#include <vector>

namespace Phase4Validation {

// ============================================================================
// Spectral Validation Utilities
// ============================================================================

/**
 * Validates spectral energy conservation using Parseval's theorem
 * 
 * Theory: ∫₀^∞ S(f)df = σ²_target
 * 
 * For discrete frequencies: Σᵢ S(fᵢ)·Δfᵢ ≈ σ²_target (within tolerance)
 */
class SpectralEnergyValidator {
public:
    /**
     * Compute integrated spectral energy
     * 
     * @param spectrum_u Spectral density array [m²/s²/Hz]
     * @param frequencies Frequency bin centers [Hz]
     * @return Integrated energy [m²/s²]
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static amrex::Real ComputeIntegratedEnergy(
        const amrex::Real* spectrum_u,
        const amrex::Real* frequencies,
        int num_frequencies)
    {
        amrex::Real energy = 0.0;
        
        for (int i = 0; i < num_frequencies; ++i) {
            amrex::Real f_curr = frequencies[i];
            amrex::Real f_prev = (i > 0) ? frequencies[i-1] : 0.0;
            amrex::Real delta_f = (i > 0) ? (f_curr - f_prev) : f_curr;
            
            if (i == 0 && f_curr > 0.0) {
                delta_f = f_curr; // Handle first bin
            }
            
            energy += spectrum_u[i] * delta_f;
        }
        
        return energy;
    }
    
    /**
     * Validate energy conservation
     * 
     * Checks: |integrated_energy - target_rms²| / (target_rms²) < tolerance
     * Typical tolerance: ±5%
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateEnergyConservation(
        amrex::Real integrated_energy,
        amrex::Real target_rms,
        amrex::Real tolerance = 0.05)
    {
        amrex::Real target_energy = target_rms * target_rms;
        amrex::Real error = std::abs(integrated_energy - target_energy) / 
                            std::max(std::abs(target_energy), 1.0e-10);
        return error <= tolerance;
    }
    
    /**
     * Compute relative energy error (useful for diagnostics)
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static amrex::Real ComputeEnergyError(
        amrex::Real integrated_energy,
        amrex::Real target_rms)
    {
        amrex::Real target_energy = target_rms * target_rms;
        return (integrated_energy - target_energy) / 
               std::max(std::abs(target_energy), 1.0e-10);
    }
};

// ============================================================================
// Von Kármán Spectrum Validator
// ============================================================================

/**
 * Validates Von Kármán spectrum characteristics
 * 
 * Model: S_u(f) = (4·L_u·u_rms²) / (1 + 70.8·(f·L_u/U)²)^(5/6)
 * 
 * Properties:
 * - Peak frequency: f_peak ≈ 0.029·U/L_u
 * - High-frequency decay: S(f) ∝ f^(-5/3) for f >> f_peak
 * - Energy: ∫S df = u_rms²
 */
class VonKarmanSpectrumValidator {
public:
    /**
     * Compute reference Von Kármán spectrum
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static amrex::Real ComputeSpectrum(
        amrex::Real freq,
        amrex::Real u_rms,
        amrex::Real length_scale_u,
        amrex::Real u_mean)
    {
        // Guard against invalid inputs
        if (u_mean < 0.1 || length_scale_u < 1.0 || freq < 0.0 || u_rms < 0.001) {
            return 0.0;
        }
        
        // Von Kármán spectrum
        amrex::Real f_norm = 70.8 * freq * length_scale_u / u_mean;
        amrex::Real denominator = std::pow(1.0 + f_norm * f_norm, 5.0 / 6.0);
        
        amrex::Real spectrum = 4.0 * length_scale_u * u_rms * u_rms / denominator;
        
        return spectrum;
    }
    
    /**
     * Validate peak frequency location
     * 
     * Theory: f_peak ≈ 0.029 · U/L_u
     * Tolerance: ±50% (accounting for estimation uncertainty)
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidatePeakFrequency(
        amrex::Real observed_peak_freq,
        amrex::Real u_mean,
        amrex::Real length_scale_u,
        amrex::Real tolerance = 0.5)
    {
        // Guard against invalid inputs
        if (u_mean < 0.1 || length_scale_u < 1.0) {
            return false;
        }
        
        amrex::Real theoretical_peak = 0.029 * u_mean / length_scale_u;
        amrex::Real error = std::abs(observed_peak_freq - theoretical_peak) / 
                            std::max(theoretical_peak, 1.0e-10);
        return error <= tolerance;
    }
    
    /**
     * Validate high-frequency decay slope
     * 
     * Theory: S(f) ∝ f^(-5/3) for f >> f_peak
     * Compute slope between two high-frequency points
     * Tolerance: slope error ±0.2 (5/3 ≈ 1.667)
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateHighFrequencyDecay(
        amrex::Real s_f1,
        amrex::Real s_f2,
        amrex::Real f1,
        amrex::Real f2,
        amrex::Real tolerance = 0.2)
    {
        // Guard against invalid inputs
        if (s_f1 < 1.0e-10 || s_f2 < 1.0e-10 || f1 >= f2) {
            return false;
        }
        
        // Compute slope: log(S2/S1) / log(f2/f1)
        amrex::Real log_ratio_spectrum = std::log(s_f2 / s_f1);
        amrex::Real log_ratio_frequency = std::log(f2 / f1);
        amrex::Real observed_slope = log_ratio_spectrum / log_ratio_frequency;
        
        // Expected slope (negative for decay)
        amrex::Real expected_slope = -5.0 / 3.0;
        
        // Compare absolute slopes (both should be negative)
        amrex::Real slope_error = std::abs(observed_slope - expected_slope);
        return slope_error <= tolerance;
    }
};

// ============================================================================
// Kaimal Spectrum Validator
// ============================================================================

/**
 * Validates Kaimal spectrum characteristics
 * 
 * Model: S_u(f) = (4·L_u·u_rms²·f_hat) / (1 + 6·f_hat)^(5/3)
 *        where f_hat = f·L_u/U
 * 
 * Properties:
 * - Peak frequency: f_peak ≈ 0.093·U/L_u
 * - High-frequency decay: S(f) ∝ f^(-5/3)
 * - More realistic for atmospheric boundary layer
 */
class KaimalSpectrumValidator {
public:
    /**
     * Compute reference Kaimal spectrum
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static amrex::Real ComputeSpectrum(
        amrex::Real freq,
        amrex::Real u_rms,
        amrex::Real length_scale_u,
        amrex::Real u_mean)
    {
        // Guard against invalid inputs
        if (u_mean < 0.1 || length_scale_u < 1.0 || freq < 0.0 || u_rms < 0.001) {
            return 0.0;
        }
        
        // Kaimal spectrum
        amrex::Real f_hat = freq * length_scale_u / u_mean;
        amrex::Real numerator = 4.0 * length_scale_u * u_rms * u_rms * f_hat;
        amrex::Real denominator = std::pow(1.0 + 6.0 * f_hat, 5.0 / 3.0);
        
        amrex::Real spectrum = numerator / denominator;
        
        return spectrum;
    }
    
    /**
     * Validate peak frequency location
     * 
     * Theory: f_peak ≈ 0.093 · U/L_u
     * Tolerance: ±50%
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidatePeakFrequency(
        amrex::Real observed_peak_freq,
        amrex::Real u_mean,
        amrex::Real length_scale_u,
        amrex::Real tolerance = 0.5)
    {
        // Guard against invalid inputs
        if (u_mean < 0.1 || length_scale_u < 1.0) {
            return false;
        }
        
        amrex::Real theoretical_peak = 0.093 * u_mean / length_scale_u;
        amrex::Real error = std::abs(observed_peak_freq - theoretical_peak) / 
                            std::max(theoretical_peak, 1.0e-10);
        return error <= tolerance;
    }
};

// ============================================================================
// Integral Length Scale Validator
// ============================================================================

/**
 * Validates integral length scale recovery from spectra
 * 
 * Theory: L_u = (π/2) · ∫₀^∞ ρ(Δ)dΔ = (π/2U) · ∫₀^∞ S(f)df/f
 * 
 * Alternative: L_u ≈ (π·U) / (2·f_peak)
 */
class IntegralLengthScaleValidator {
public:
    /**
     * Recover integral length scale from spectrum
     * 
     * @param spectrum Array of spectral densities [m²/s²/Hz]
     * @param frequencies Array of frequency bin centers [Hz]
     * @param num_frequencies Number of frequency bins
     * @param u_mean Mean wind speed [m/s]
     * @return Recovered integral length scale [m]
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static amrex::Real RecoverLengthScaleFromSpectrum(
        const amrex::Real* spectrum,
        const amrex::Real* frequencies,
        int num_frequencies,
        amrex::Real u_mean)
    {
        if (u_mean < 0.1 || num_frequencies < 2) {
            return 0.0;
        }
        
        // Integrate S(f)/f weighted integral
        amrex::Real integral = 0.0;
        
        for (int i = 0; i < num_frequencies; ++i) {
            amrex::Real f_curr = frequencies[i];
            amrex::Real f_prev = (i > 0) ? frequencies[i-1] : f_curr;
            amrex::Real delta_f = f_curr - f_prev;
            
            if (i == 0) {
                delta_f = f_curr;
            }
            
            if (f_curr > 1.0e-10) {  // Avoid division by zero
                integral += (spectrum[i] / f_curr) * delta_f;
            }
        }
        
        // L_u = (π/2U) · integral
        amrex::Real length_scale = (MathConstants::pi / (2.0 * u_mean)) * integral;
        
        return length_scale;
    }
    
    /**
     * Validate recovered length scale against input
     * 
     * Tolerance: ±20% (typical for frequency-domain recovery)
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateLengthScale(
        amrex::Real recovered_length_scale,
        amrex::Real input_length_scale,
        amrex::Real tolerance = 0.2)
    {
        if (input_length_scale < 1.0) {
            return false;
        }
        
        amrex::Real error = std::abs(recovered_length_scale - input_length_scale) / 
                            input_length_scale;
        return error <= tolerance;
    }
};

// ============================================================================
// Spectral Properties Validator (Master Class)
// ============================================================================

/**
 * Comprehensive spectral validation combining multiple checks
 */
class SpectralValidator {
public:
    struct ValidationResult {
        bool energy_conserved = false;
        bool peak_frequency_valid = false;
        bool high_freq_decay_valid = false;
        bool length_scale_valid = false;
        bool all_checks_passed = false;
        
        amrex::Real energy_error = 0.0;
        amrex::Real observed_peak_freq = 0.0;
        amrex::Real recovered_length_scale = 0.0;
    };
    
    /**
     * Run comprehensive spectral validation
     * 
     * @param spectrum_u U-component spectral density [m²/s²/Hz]
     * @param frequencies Frequency bin centers [Hz]
     * @param num_frequencies Number of frequency bins
     * @param u_rms RMS velocity [m/s]
     * @param length_scale_u Integral length scale [m]
     * @param u_mean Mean wind speed [m/s]
     * @param is_von_karman True for Von Kármán, false for Kaimal
     * @return Validation result structure
     */
    static ValidationResult ValidateSpectrum(
        const amrex::Real* spectrum_u,
        const amrex::Real* frequencies,
        int num_frequencies,
        amrex::Real u_rms,
        amrex::Real length_scale_u,
        amrex::Real u_mean,
        bool is_von_karman = true)
    {
        ValidationResult result;
        
        // Check 1: Energy conservation
        amrex::Real integrated_energy = 
            SpectralEnergyValidator::ComputeIntegratedEnergy(
                spectrum_u, frequencies, num_frequencies);
        
        result.energy_error = 
            SpectralEnergyValidator::ComputeEnergyError(integrated_energy, u_rms);
        result.energy_conserved = 
            SpectralEnergyValidator::ValidateEnergyConservation(
                integrated_energy, u_rms, 0.05);
        
        // Check 2 & 3: Peak frequency and high-frequency decay
        // Find peak in spectrum
        amrex::Real peak_value = 0.0;
        int peak_idx = 0;
        for (int i = 0; i < num_frequencies; ++i) {
            if (spectrum_u[i] > peak_value) {
                peak_value = spectrum_u[i];
                peak_idx = i;
            }
        }
        
        if (peak_idx > 0 && peak_idx < num_frequencies - 1) {
            result.observed_peak_freq = frequencies[peak_idx];
            
            if (is_von_karman) {
                result.peak_frequency_valid = 
                    VonKarmanSpectrumValidator::ValidatePeakFrequency(
                        result.observed_peak_freq, u_mean, length_scale_u, 0.5);
                
                // High-frequency check: use last two bins if available
                if (num_frequencies >= 2) {
                    int i1 = num_frequencies - 3;
                    int i2 = num_frequencies - 1;
                    if (i1 >= 0) {
                        result.high_freq_decay_valid = 
                            VonKarmanSpectrumValidator::ValidateHighFrequencyDecay(
                                spectrum_u[i1], spectrum_u[i2],
                                frequencies[i1], frequencies[i2], 0.2);
                    }
                }
            } else {
                result.peak_frequency_valid = 
                    KaimalSpectrumValidator::ValidatePeakFrequency(
                        result.observed_peak_freq, u_mean, length_scale_u, 0.5);
            }
        }
        
        // Check 4: Length scale recovery
        result.recovered_length_scale = 
            IntegralLengthScaleValidator::RecoverLengthScaleFromSpectrum(
                spectrum_u, frequencies, num_frequencies, u_mean);
        
        result.length_scale_valid = 
            IntegralLengthScaleValidator::ValidateLengthScale(
                result.recovered_length_scale, length_scale_u, 0.2);
        
        // Overall result
        result.all_checks_passed = 
            result.energy_conserved && 
            result.peak_frequency_valid && 
            result.length_scale_valid;
        
        return result;
    }
};

} // namespace Phase4Validation

#endif // SPECTRAL_VALIDATION_H
