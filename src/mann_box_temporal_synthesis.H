// ============================================================================
// mann_box_temporal_synthesis.H
// 
// Temporal & Stability Physics: Temporal Synthesis
// 
// Implements time-lag correlation structures and Taylor frozen turbulence
// hypothesis for Mann Box spectral tensor model. Extends spatial
// correlations to full space-time correlations R_ij(r, τ).
//
// Key Features:
// - Time-lag correlation functions R_ij(r, τ)
// - Taylor frozen turbulence (swept wave) approximation
// - Eulerian vs Lagrangian framework support
// - Time-lag covariance matrices for transient applications
// - GPU-ready device-compatible kernels
//
// Mathematical Foundation:
// 
// 1. Taylor Frozen Turbulence Hypothesis
//    The correlation evolves as: R_ij(r⃗, τ) = R_ij(r⃗ - U_h*τ, 0)
//    This is valid when convective timescale >> local timescale
//    Condition: τ * (∂u/∂x) << u'_rms (typically valid for τ < 10 s)
//
// 2. Time-Lag Spectrum Relationship
//    The frequency spectrum is related to spatial spectrum via:
//    S_ii(f) = |S_ii(k, f)| at k = 2π*f/U_h (wavenumber-frequency conversion)
//
// 3. Eulerian Autocorrelation
//    At fixed point: R_ij(0, τ) = ∫ S_ij(k) cos(ω(k)*τ) dk
//    where ω(k) is the temporal frequency-wavenumber relation
//
// 4. Lagrangian Autocorrelation
//    Following a fluid particle: R_ij^L(τ) involves particle motion
//    Different decay rate than Eulerian
//
// References:
//   - Mann, J. (1994). The spatial structure of neutral atmospheric surface-layer
//     turbulence. Journal of Fluid Mechanics, 273, 141-168.
//   - Tennekes, H., & Lumley, J. L. (1972). A first course in turbulence.
//   - Kristensen, L., et al. (2001). The spectral velocity tensor in
//     homogeneous boundary layer turbulence.
//
// ============================================================================

#ifndef MANN_BOX_TEMPORAL_SYNTHESIS_H
#define MANN_BOX_TEMPORAL_SYNTHESIS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <algorithm>
#include "solver_math_constants.H"

// ============================================================================
// Time-Lag Correlation Parameters Structure
// ============================================================================

struct TimeLagParams {
    // Mean horizontal wind speed [m/s]
    amrex::Real U_horizontal = 10.0;
    
    // Integral timescale [s] (default ~ L/U)
    amrex::Real T_integral = 1.0;
    
    // Temporal decay model: 0=exponential, 1=gaussian
    int temporal_model = 0;
    
    // Lagrangian/Eulerian flag: true=Lagrangian, false=Eulerian
    bool use_lagrangian = false;
    
    // Virtual time shift for synthetic phase lag [s]
    amrex::Real time_shift = 0.0;
};

// ============================================================================
// Namespace for temporal synthesis operations
// ============================================================================

namespace MannBoxTemporalSynthesis {

// ============================================================================
// PART 1: Time-Lag Correlation Functions
// ============================================================================

/// Compute Eulerian time-lag autocorrelation
/// Uses exponential or Gaussian decay model
/// 
/// @param time_lag      Time lag τ [s]
/// @param integral_time Integral timescale T_int [s]
/// @param model_type    0 = exponential, 1 = Gaussian
/// @return              Autocorrelation ρ_E(τ) ∈ [0, 1]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_eulerian_autocorrelation(
    amrex::Real time_lag,
    amrex::Real integral_time,
    int model_type = 0
) noexcept
{
    // Avoid division by zero
    if (integral_time < 1.0e-6) return 0.0;
    
    // Normalized time lag
    amrex::Real tau_norm = time_lag / integral_time;
    
    if (model_type == 0) {
        // Exponential decay: ρ_E(τ) = exp(-|τ|/T_int)
        return std::exp(-std::abs(tau_norm));
    } else {
        // Gaussian decay: ρ_E(τ) = exp(-(τ/T_int)²)
        return std::exp(-tau_norm * tau_norm);
    }
}

/// Compute Lagrangian time-lag autocorrelation
/// Lagrangian correlation decays faster than Eulerian
/// Typical ratio: ρ_L(τ) ≈ ρ_E(τ/2) or similar compression
/// 
/// @param time_lag      Time lag τ [s]
/// @param integral_time Integral timescale T_int [s]
/// @param model_type    0 = exponential, 1 = Gaussian
/// @return              Autocorrelation ρ_L(τ) ∈ [0, 1]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_lagrangian_autocorrelation(
    amrex::Real time_lag,
    amrex::Real integral_time,
    int model_type = 0
) noexcept
{
    // Avoid division by zero
    if (integral_time < 1.0e-6) return 0.0;
    
    // Lagrangian timescale is typically 40-60% of Eulerian
    // Using compression factor of 0.5 (conservative)
    amrex::Real lagrangian_scale = integral_time * amrex::Real(0.5);
    
    return compute_eulerian_autocorrelation(time_lag, lagrangian_scale, model_type);
}

// ============================================================================
// PART 2: Space-Time Correlation R_ij(r, τ)
// ============================================================================

/// Compute Taylor frozen turbulence spatial-temporal correlation
/// R_ij(r⃗, τ) ≈ R_ij(r⃗ - U_h*τ, 0)
/// 
/// This assumes the turbulence pattern is frozen and convects with mean wind.
/// Valid when: convective time >> local correlation time
/// 
/// @param spatial_separation  Spatial separation r [m]
/// @param time_lag           Time lag τ [s]
/// @param mean_wind          Mean horizontal wind U_h [m/s]
/// @param length_scale       Spatial correlation length scale L [m]
/// @return                   Space-time correlation value
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_taylor_frozen_turbulence_correlation(
    amrex::Real spatial_separation,
    amrex::Real time_lag,
    amrex::Real mean_wind,
    amrex::Real length_scale
) noexcept
{
    // Compute effective spatial separation due to wind advection
    // r_eff = |r - U*τ|
    amrex::Real advected_distance = mean_wind * time_lag;
    amrex::Real effective_separation = 
        std::sqrt(spatial_separation * spatial_separation + 
                  advected_distance * advected_distance);
    
    // Use exponential spatial decay (standard for Mann Box)
    // Correlation ~ exp(-|r|/L)
    amrex::Real spatial_corr = std::exp(-effective_separation / length_scale);
    
    return spatial_corr;
}

/// Compute combined space-time correlation
/// Combines spatial decay and temporal decay
/// R_ij(r, τ) = R_ij(r) * ρ(τ) (separable assumption)
/// 
/// @param spatial_separation     Spatial separation r [m]
/// @param time_lag              Time lag τ [s]
/// @param length_scale          Spatial correlation length L [m]
/// @param integral_time         Temporal correlation time T_int [s]
/// @param temporal_model_type   0=exponential, 1=Gaussian
/// @return                      Combined correlation value ∈ [-1, 1]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_space_time_correlation(
    amrex::Real spatial_separation,
    amrex::Real time_lag,
    amrex::Real length_scale,
    amrex::Real integral_time,
    int temporal_model_type = 0
) noexcept
{
    // Spatial correlation: exponential decay
    amrex::Real spatial_corr = std::exp(-spatial_separation / length_scale);
    
    // Temporal correlation: Eulerian
    amrex::Real temporal_corr = 
        compute_eulerian_autocorrelation(time_lag, integral_time, temporal_model_type);
    
    // Combined (separable): R(r,τ) ≈ R(r) * ρ(τ)
    return spatial_corr * temporal_corr;
}

// ============================================================================
// PART 3: Frequency-Wavenumber Relationships
// ============================================================================

/// Convert wavenumber to frequency using Taylor hypothesis
/// ω = k * U_h (linear relationship at small scales)
/// 
/// @param wavenumber     Wavenumber k [1/m]
/// @param mean_wind      Mean horizontal wind U_h [m/s]
/// @return               Frequency ω = 2π*f [rad/s]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real wavenumber_to_frequency_taylor(
    amrex::Real wavenumber,
    amrex::Real mean_wind
) noexcept
{
    if (mean_wind < 1.0e-6) return 0.0;
    return wavenumber * mean_wind;
}

/// Compute frequency spectrum from spatial spectrum via Taylor hypothesis
/// S_ii(f) = S_ii(k)|_{k=2πf/U_h} * (U_h / 2π)
/// 
/// @param frequency      Frequency f [Hz]
/// @param spatial_spectrum  S_ii(k) at wavenumber k=2πf/U_h
/// @param mean_wind      Mean horizontal wind U_h [m/s]
/// @return               Frequency spectrum S_ii(f) [m²/s]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real frequency_spectrum_from_spatial(
    amrex::Real frequency,
    amrex::Real spatial_spectrum,
    amrex::Real mean_wind
) noexcept
{
    if (mean_wind < 1.0e-6) return 0.0;
    
    // Jacobian: dk/df = 2π/U_h
    // Therefore: S(f) = S(k) * dk/df = S(k) * 2π/U_h
    amrex::Real jacobian = amrex::Real(2.0) * PI / mean_wind;
    
    return spatial_spectrum * jacobian;
}

// ============================================================================
// PART 4: Temporal Variance and Energy
// ============================================================================

/// Compute temporal variance from autocorrelation
/// Variance = ∫ S(f) df = ∫ ρ(τ) dτ
/// 
/// @param integral_time     Integral timescale T_int [s]
/// @param rms_fluctuation   RMS fluctuation amplitude u'_rms [m/s]
/// @return                  Temporal variance [m²/s²]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_temporal_variance(
    amrex::Real integral_time,
    amrex::Real rms_fluctuation
) noexcept
{
    // Temporal variance is proportional to u'_rms²
    // The integral timescale T_int relates to spectral width
    // For correlation ρ(τ): variance = ∫_0^∞ ρ(τ) dτ ≈ T_int
    
    amrex::Real variance = rms_fluctuation * rms_fluctuation;
    return variance;
}

/// Compute spectral energy at frequency
/// Uses relationship between temporal and spatial spectra
/// 
/// @param frequency         Frequency f [Hz]
/// @param peak_frequency    Peak frequency f_peak [Hz]
/// @param spectral_width    Spectral width parameter
/// @return                  Spectral energy at frequency [m²/s/Hz]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_spectral_energy_at_frequency(
    amrex::Real frequency,
    amrex::Real peak_frequency,
    amrex::Real spectral_width = 1.0
) noexcept
{
    if (frequency < 1.0e-6 || peak_frequency < 1.0e-6) return 0.0;
    
    // Normalized frequency
    amrex::Real f_norm = frequency / peak_frequency;
    
    // Spectral shape: f^4 * exp(-5*f/4*f_peak) (Von Kármán-like)
    amrex::Real shape = f_norm * f_norm * f_norm * f_norm * 
                        std::exp(-amrex::Real(1.25) * f_norm);
    
    return shape / spectral_width;
}

// ============================================================================
// PART 5: Time Series Generation Parameters
// ============================================================================

/// Compute time step for stable time series generation
/// Following Taylor hypothesis: Δt < Δx / U_h for good resolution
/// 
/// @param grid_spacing      Grid spacing Δx [m]
/// @param mean_wind         Mean wind speed U_h [m/s]
/// @param courant_number    Courant number for stability (typically 0.1-0.5)
/// @return                  Maximum time step [s]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_stable_time_step(
    amrex::Real grid_spacing,
    amrex::Real mean_wind,
    amrex::Real courant_number = amrex::Real(0.25)
) noexcept
{
    if (mean_wind < 1.0e-6) return 0.1;  // Default fallback
    
    // Time for wind to cross one grid cell: Δt = Δx / U
    // Apply Courant number for numerical stability
    amrex::Real crossing_time = grid_spacing / mean_wind;
    
    return courant_number * crossing_time;
}

/// Compute number of time steps for simulation
/// Ensures sufficient samples for spectral estimation
/// 
/// @param total_time        Total simulation time [s]
/// @param integral_time     Integral timescale [s]
/// @param samples_per_period Samples per correlation period (typically 10-20)
/// @return                  Minimum number of time steps
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
int compute_min_time_steps(
    amrex::Real total_time,
    amrex::Real integral_time,
    int samples_per_period = 10
) noexcept
{
    if (integral_time < 1.0e-6) return 100;
    
    // Number of correlation periods in total time
    amrex::Real periods = total_time / integral_time;
    
    // Multiply by samples per period to get minimum steps
    int min_steps = static_cast<int>(periods * samples_per_period + 1);
    
    return std::max(min_steps, 10);  // At least 10 steps
}

// ============================================================================
// PART 6: Coherence Reduction Due to Time Separation
// ============================================================================

/// Compute coherence reduction factor for temporal separation
/// Coherence between two points separated in time reduces due to
/// decorrelation of turbulent scales
/// 
/// @param time_separation   Time separation Δt [s]
/// @param integral_time     Integral timescale T_int [s]
/// @param reference_height  Reference height for decay [m]
/// @return                  Coherence factor ∈ [0, 1]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_temporal_coherence_reduction(
    amrex::Real time_separation,
    amrex::Real integral_time,
    amrex::Real reference_height = 10.0
) noexcept
{
    // Coherence decays as: Coh(Δt) = exp(-|Δt| * C_t / T_int)
    // where C_t is a decay constant (typically 0.1-0.5)
    
    if (integral_time < 1.0e-6) return 0.0;
    
    // Decay constant increases with height (weaker coherence aloft)
    amrex::Real decay_const = amrex::Real(0.2) * 
        (1.0 + reference_height / 100.0);
    
    amrex::Real normalized_sep = decay_const * std::abs(time_separation) / integral_time;
    
    return std::exp(-normalized_sep);
}

} // namespace MannBoxTemporalSynthesis

#endif // MANN_BOX_TEMPORAL_SYNTHESIS_H
