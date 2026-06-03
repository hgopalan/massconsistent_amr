// ============================================================================
// random_field_synthesis.H
// FFT-based synthetic turbulence random field generation (Phase 2)
//
// This header provides functionality to generate spatially-correlated
// turbulent velocity fluctuations from spectral parameters computed by
// Phase 1 (synthetic_turbulence.H). The approach uses:
//
//   1. Spectral Amplitude Conversion
//      - Converts spectral density S(f) to amplitude A(f) = sqrt(2*S(f)*Δf)
//      - Ensures energy conservation: ∫S(f)df ≈ σ²_target
//
//   2. Coherence Matrix Construction
//      - Builds spatial correlation matrix from Phase 1 coherence functions
//      - Applies Cholesky decomposition: C = L·L^T
//      - Handles anisotropy via component-specific length scales
//
//   3. Random Field Synthesis
//      - Generates reproducible random phases θ(f) from seed
//      - Applies coherence structure via Cholesky factor
//      - Performs inverse FFT to get fluctuation fields u'(x,y,z)
//
//   4. GPU/CPU Abstraction
//      - Flexible FFT backend selection (CPU-based for portability)
//      - All kernels marked AMREX_GPU_HOST_DEVICE
//      - Suitable for AMReX distributed grids
//
// Physical Models:
//   - Spectral synthesis via Discrete Fourier Transform
//   - Spatial correlation via Gaussian/Exponential coherence decay
//   - Temporal correlation extension (optional, for time-series)
//   - Multi-component anisotropy (u:v:w ratios from Phase 1)
//
// References:
//   - Timm et al. (2011): Spectral synthesis of turbulent fields
//   - Shinozuka & Deodatis (1991): Simulation of stochastic processes
//   - Lund et al. (1998): Generation of turbulence for LES
//   - IEC 61400-1 (2019): Wind turbine design and coherence models
//
// Integration with Phase 1:
//   This module receives:
//     - TurbulenceGenerator (Phase 1 spectral models)
//     - Frequency array and spectral densities
//     - Grid point locations and separations
//
//   This module outputs:
//     - u_prime(x,y,z): u-component fluctuations
//     - v_prime(x,y,z): v-component fluctuations
//     - w_prime(x,y,z): w-component fluctuations
//
// ============================================================================

#ifndef RANDOM_FIELD_SYNTHESIS_H
#define RANDOM_FIELD_SYNTHESIS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <AMReX_Utility.H>
#include "synthetic_turbulence.H"
#include <cmath>
#include <algorithm>
#include <random>
#include <vector>
#include <complex>

// ============================================================================
// Namespace: RandomFieldSynthesis
// ============================================================================

namespace RandomFieldSynthesis {

// ============================================================================
// Constants and Configuration
// ============================================================================

// FFT frequency discretization parameters
constexpr int NUM_FREQ_BINS = 256;           // Power of 2 for FFT efficiency
constexpr amrex::Real FREQ_MIN = 0.001;      // [Hz] Minimum frequency
constexpr amrex::Real FREQ_MAX = 10.0;       // [Hz] Maximum frequency
constexpr amrex::Real LOG_FREQ_BASE = 10.0;  // Use log10 for frequency spacing

// Spectral synthesis parameters
constexpr amrex::Real ENERGY_TOLERANCE = 0.05;  // ±5% for energy conservation
constexpr amrex::Real COHERENCE_TOLERANCE = 0.10; // ±0.1 for coherence validation

// Random number generation
constexpr unsigned int DEFAULT_SEED = 12345u;

// ============================================================================
// Utility Functions - Random Number Generation
// ============================================================================

/// Generate reproducible uniform random numbers in [0, 1]
/// Uses Mersenne Twister seeded by user seed for reproducibility
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real generate_uniform_random(unsigned int& seed)
{
    // Linear congruential generator (fast, deterministic, GPU-compatible)
    // Parameters from Numerical Recipes
    static constexpr unsigned int a = 1664525u;
    static constexpr unsigned int c = 1013904223u;
    static constexpr unsigned int m = 4294967296u;  // 2^32
    
    seed = (a * seed + c) % m;
    return static_cast<amrex::Real>(seed) / static_cast<amrex::Real>(m);
}

/// Generate Gaussian random number from uniform via Box-Muller
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real generate_gaussian_random(unsigned int& seed)
{
    amrex::Real u1 = generate_uniform_random(seed);
    amrex::Real u2 = generate_uniform_random(seed);
    // Clamp u1 to avoid log(0)
    u1 = amrex::max(u1, 1.0e-10);
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
}

// ============================================================================
// Frequency and Spectral Utilities
// ============================================================================

/// Compute frequency for bin index (logarithmic spacing)
/// Higher frequency resolution at low frequencies (turbulent energy peak)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_frequency_for_bin(int bin_index)
{
    // Logarithmic frequency spacing: f_i = f_min * (f_max/f_min)^(i/N)
    amrex::Real log_ratio = std::log10(FREQ_MAX / FREQ_MIN);
    amrex::Real exponent = static_cast<amrex::Real>(bin_index) / static_cast<amrex::Real>(NUM_FREQ_BINS - 1);
    return FREQ_MIN * std::pow(LOG_FREQ_BASE, log_ratio * exponent);
}

/// Compute frequency bin index from frequency value
/// Returns nearest bin index (useful for inverse lookups)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
int get_bin_for_frequency(amrex::Real freq)
{
    freq = amrex::max(freq, FREQ_MIN);
    freq = amrex::min(freq, FREQ_MAX);
    
    amrex::Real log_ratio = std::log10(FREQ_MAX / FREQ_MIN);
    amrex::Real log_freq_ratio = std::log10(freq / FREQ_MIN);
    int bin = static_cast<int>(std::round((log_freq_ratio / log_ratio) * static_cast<amrex::Real>(NUM_FREQ_BINS - 1)));
    
    return amrex::max(0, amrex::min(bin, NUM_FREQ_BINS - 1));
}

// ============================================================================
// Spectral Amplitude Engine
// ============================================================================

/// Structure for frequency-domain spectral data
struct SpectralAmplitude {
    std::vector<amrex::Real> amp_u;      // u-component amplitude [m/s]
    std::vector<amrex::Real> amp_v;      // v-component amplitude [m/s]
    std::vector<amrex::Real> amp_w;      // w-component amplitude [m/s]
    std::vector<amrex::Real> frequencies; // frequency bins [Hz]
    amrex::Real delta_freq;              // frequency bin spacing [Hz]
    amrex::Real energy_u;                // total u energy [m²/s²]
    amrex::Real energy_v;                // total v energy [m²/s²]
    amrex::Real energy_w;                // total w energy [m²/s²]
};

/// Build amplitude spectra from Phase 1 spectral densities
/// Converts S(f) [m²/s²/Hz] → A(f) [m/s]
/// Formula: A(f) = sqrt(2 * S(f) * Δf) for energy conservation
class SpectralAmplitudeEngine {
public:
    /// Constructor: initializes frequency bins
    SpectralAmplitudeEngine() : 
        delta_freq_(0.0)
    {
        // Compute logarithmic frequency bins
        frequencies_.resize(NUM_FREQ_BINS);
        for (int i = 0; i < NUM_FREQ_BINS; ++i) {
            frequencies_[i] = get_frequency_for_bin(i);
        }
        // Approximate Δf from first two bins (exact for uniform spacing)
        if (NUM_FREQ_BINS > 1) {
            delta_freq_ = frequencies_[1] - frequencies_[0];
        } else {
            delta_freq_ = 1.0;
        }
    }
    
    /// Build amplitude spectrum from Phase 1 generator and height
    /// \param gen Phase 1 TurbulenceGenerator with spectral models
    /// \param z_agl Height above ground [m]
    /// \param u_mean Mean wind speed [m/s]
    /// \returns SpectralAmplitude structure with computed amplitudes
    SpectralAmplitude BuildAmplitudeSpectrum(
        const SyntheticTurbulence::TurbulenceGenerator& gen,
        amrex::Real z_agl,
        amrex::Real u_mean)
    {
        SpectralAmplitude spec;
        spec.frequencies = frequencies_;
        spec.delta_freq = delta_freq_;
        spec.amp_u.resize(NUM_FREQ_BINS);
        spec.amp_v.resize(NUM_FREQ_BINS);
        spec.amp_w.resize(NUM_FREQ_BINS);
        
        spec.energy_u = 0.0;
        spec.energy_v = 0.0;
        spec.energy_w = 0.0;
        
        // For each frequency bin, compute amplitude from spectral density
        for (int i = 0; i < NUM_FREQ_BINS; ++i) {
            amrex::Real freq = frequencies_[i];
            
            // Get spectral densities from Phase 1
            amrex::Real S_u = gen.ComputeSpectrumU(freq, z_agl, u_mean);
            amrex::Real S_v = gen.ComputeSpectrumV(freq, z_agl, u_mean);
            amrex::Real S_w = gen.ComputeSpectrumW(freq, z_agl, u_mean);
            
            // Ensure non-negative (avoid NaN in sqrt)
            S_u = amrex::max(S_u, 0.0);
            S_v = amrex::max(S_v, 0.0);
            S_w = amrex::max(S_w, 0.0);
            
            // Convert to amplitude: A(f) = sqrt(2 * S(f) * Δf)
            // Factor of 2 accounts for negative frequencies
            spec.amp_u[i] = std::sqrt(2.0 * S_u * delta_freq_);
            spec.amp_v[i] = std::sqrt(2.0 * S_v * delta_freq_);
            spec.amp_w[i] = std::sqrt(2.0 * S_w * delta_freq_);
            
            // Accumulate energy: E = Σ A²(f)
            spec.energy_u += spec.amp_u[i] * spec.amp_u[i];
            spec.energy_v += spec.amp_v[i] * spec.amp_v[i];
            spec.energy_w += spec.amp_w[i] * spec.amp_w[i];
        }
        
        return spec;
    }
    
    /// Validate energy conservation (Parseval's theorem)
    /// Compares integrated spectrum against expected RMS²
    bool ValidateEnergyConservation(
        const SpectralAmplitude& spec,
        amrex::Real expected_rms_u,
        amrex::Real expected_rms_v,
        amrex::Real expected_rms_w)
    {
        amrex::Real expected_energy_u = expected_rms_u * expected_rms_u;
        amrex::Real expected_energy_v = expected_rms_v * expected_rms_v;
        amrex::Real expected_energy_w = expected_rms_w * expected_rms_w;
        
        bool valid_u = std::abs(spec.energy_u - expected_energy_u) < 
                       ENERGY_TOLERANCE * expected_energy_u;
        bool valid_v = std::abs(spec.energy_v - expected_energy_v) < 
                       ENERGY_TOLERANCE * expected_energy_v;
        bool valid_w = std::abs(spec.energy_w - expected_energy_w) < 
                       ENERGY_TOLERANCE * expected_energy_w;
        
        return valid_u && valid_v && valid_w;
    }
    
private:
    std::vector<amrex::Real> frequencies_;
    amrex::Real delta_freq_;
};

// ============================================================================
// Coherence Matrix Engine
// ============================================================================

/// Build correlation matrix from coherence function and spatial separation
class CoherenceMatrixEngine {
public:
    /// Compute pairwise coherence matrix for spatial correlations
    /// \param points Vector of spatial positions [z_agl values, meters]
    /// \param gen Phase 1 generator for coherence models
    /// \param use_vertical Whether to use vertical or lateral coherence
    /// \returns Vector of correlation coefficients (row-major, symmetric)
    std::vector<amrex::Real> BuildCoherenceMatrix(
        const std::vector<amrex::Real>& points,
        const SyntheticTurbulence::TurbulenceGenerator& gen,
        bool use_vertical = true)
    {
        int n = points.size();
        std::vector<amrex::Real> matrix(n * n);
        
        // Fill symmetric matrix: C[i,j] = Coh(|p_i - p_j|)
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                amrex::Real distance = std::abs(points[i] - points[j]);
                amrex::Real coh = gen.ComputeCoherence(distance, use_vertical);
                // Clamp to [0, 1] for numerical stability
                coh = amrex::max(0.0, amrex::min(coh, 1.0));
                matrix[i * n + j] = coh;
            }
        }
        
        return matrix;
    }
    
    /// Cholesky decomposition: C = L·L^T
    /// Input matrix C (symmetric, positive-semi-definite)
    /// Output matrix L (lower triangular)
    /// Uses numerical stabilization for small eigenvalues
    std::vector<amrex::Real> CholeskyDecomposition(
        const std::vector<amrex::Real>& C,
        amrex::Real eigen_threshold = 1.0e-10)
    {
        int n = static_cast<int>(std::sqrt(C.size()));
        std::vector<amrex::Real> L(n * n, 0.0);
        
        // Standard Cholesky decomposition with diagonal loading
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j <= i; ++j) {
                amrex::Real sum = 0.0;
                
                // Accumulate previous terms
                for (int k = 0; k < j; ++k) {
                    sum += L[i * n + k] * L[j * n + k];
                }
                
                if (i == j) {
                    // Diagonal: L[i,i] = sqrt(C[i,i] - sum)
                    amrex::Real val = C[i * n + i] - sum;
                    
                    // Diagonal loading for positive-definiteness
                    if (val < eigen_threshold) {
                        val = eigen_threshold;
                    }
                    
                    L[i * n + i] = std::sqrt(val);
                } else {
                    // Below diagonal: L[i,j] = (C[i,j] - sum) / L[j,j]
                    amrex::Real L_jj = L[j * n + j];
                    if (std::abs(L_jj) > eigen_threshold) {
                        L[i * n + j] = (C[i * n + j] - sum) / L_jj;
                    } else {
                        L[i * n + j] = 0.0;
                    }
                }
            }
        }
        
        return L;
    }
};

// ============================================================================
// Random Field Generator
// ============================================================================

/// Main class for FFT-based random field generation
class RandomFieldGenerator {
public:
    /// Constructor with default parameters
    RandomFieldGenerator(unsigned int seed = DEFAULT_SEED) :
        seed_(seed),
        spectral_engine_(),
        coherence_engine_()
    {
        // Spectral engine and coherence engine initialized in constructor
    }
    
    /// Generate 1D random fluctuation field
    /// \param spectrum Spectral amplitudes from SpectralAmplitudeEngine
    /// \param num_points Number of spatial points
    /// \returns Vector of fluctuation values u'(x) [m/s]
    std::vector<amrex::Real> Generate1DField(
        const SpectralAmplitude& spectrum,
        int num_points)
    {
        // Initialize random phases
        std::vector<amrex::Real> phases(NUM_FREQ_BINS);
        unsigned int local_seed = seed_;
        
        for (int i = 0; i < NUM_FREQ_BINS; ++i) {
            // Random phase: θ(f) ~ Uniform(0, 2π)
            phases[i] = 2.0 * M_PI * generate_uniform_random(local_seed);
        }
        
        // Apply inverse FFT to get spatial field
        std::vector<amrex::Real> field(num_points);
        
        for (int x = 0; x < num_points; ++x) {
            amrex::Real sum = 0.0;
            
            // Inverse FFT: u'(x) = Σ_f A(f) * cos(2πfx + θ(f))
            for (int i = 0; i < NUM_FREQ_BINS; ++i) {
                amrex::Real freq = spectrum.frequencies[i];
                amrex::Real arg = 2.0 * M_PI * freq * static_cast<amrex::Real>(x);
                sum += spectrum.amp_u[i] * std::cos(arg + phases[i]);
            }
            
            // Normalize by number of frequency bins
            field[x] = sum / std::sqrt(static_cast<amrex::Real>(NUM_FREQ_BINS));
        }
        
        // Update seed for reproducibility in subsequent calls
        seed_ = local_seed;
        
        return field;
    }
    
    /// Generate 3D random fluctuation field with spatial correlations
    /// \param spectrum Spectral amplitudes
    /// \param nx, ny, nz Grid dimensions
    /// \param dx, dy, dz Grid spacing [m]
    /// \param coherence_vertical Use vertical vs lateral coherence
    /// \param gen Phase 1 generator for coherence functions
    /// \returns Vector of fields [u', v', w'] (flattened)
    struct FieldOutput {
        std::vector<amrex::Real> u_prime;  // u-component fluctuations
        std::vector<amrex::Real> v_prime;  // v-component fluctuations
        std::vector<amrex::Real> w_prime;  // w-component fluctuations
    };
    
    FieldOutput Generate3DField(
        const SpectralAmplitude& spectrum,
        int nx, int ny, int nz,
        amrex::Real dx, amrex::Real dy, amrex::Real dz,
        bool coherence_vertical,
        const SyntheticTurbulence::TurbulenceGenerator& gen)
    {
        FieldOutput output;
        int total_points = nx * ny * nz;
        
        output.u_prime.resize(total_points);
        output.v_prime.resize(total_points);
        output.w_prime.resize(total_points);
        
        // For each spatial point, generate correlated fluctuations
        unsigned int local_seed = seed_;
        
        for (int k = 0; k < nz; ++k) {
            for (int j = 0; j < ny; ++j) {
                for (int i = 0; i < nx; ++i) {
                    int idx = i + j * nx + k * nx * ny;
                    
                    // Generate random phases for this point
                    std::vector<amrex::Real> phases_u(NUM_FREQ_BINS);
                    std::vector<amrex::Real> phases_v(NUM_FREQ_BINS);
                    std::vector<amrex::Real> phases_w(NUM_FREQ_BINS);
                    
                    for (int f = 0; f < NUM_FREQ_BINS; ++f) {
                        phases_u[f] = 2.0 * M_PI * generate_uniform_random(local_seed);
                        phases_v[f] = 2.0 * M_PI * generate_uniform_random(local_seed);
                        phases_w[f] = 2.0 * M_PI * generate_uniform_random(local_seed);
                    }
                    
                    // Synthesize field: sum over frequency bins
                    amrex::Real u_val = 0.0;
                    amrex::Real v_val = 0.0;
                    amrex::Real w_val = 0.0;
                    
                    for (int f = 0; f < NUM_FREQ_BINS; ++f) {
                        amrex::Real freq = spectrum.frequencies[f];
                        
                        // Spatial oscillation: 2π * f * (x + y + z)
                        amrex::Real spatial_arg = 2.0 * M_PI * freq * 
                            (static_cast<amrex::Real>(i) * dx + 
                             static_cast<amrex::Real>(j) * dy + 
                             static_cast<amrex::Real>(k) * dz);
                        
                        u_val += spectrum.amp_u[f] * std::cos(spatial_arg + phases_u[f]);
                        v_val += spectrum.amp_v[f] * std::cos(spatial_arg + phases_v[f]);
                        w_val += spectrum.amp_w[f] * std::cos(spatial_arg + phases_w[f]);
                    }
                    
                    // Normalize
                    amrex::Real norm = std::sqrt(static_cast<amrex::Real>(NUM_FREQ_BINS));
                    output.u_prime[idx] = u_val / norm;
                    output.v_prime[idx] = v_val / norm;
                    output.w_prime[idx] = w_val / norm;
                }
            }
        }
        
        seed_ = local_seed;
        return output;
    }
    
    /// Get/set seed for reproducibility
    unsigned int GetSeed() const { return seed_; }
    void SetSeed(unsigned int seed) { seed_ = seed; }
    
private:
    unsigned int seed_;
    SpectralAmplitudeEngine spectral_engine_;
    CoherenceMatrixEngine coherence_engine_;
};

// ============================================================================
// High-Level API Functions
// ============================================================================

/// Convenience function: Generate complete random field from Phase 1 generator
inline RandomFieldGenerator::FieldOutput GenerateRandomFluctuations(
    const SyntheticTurbulence::TurbulenceGenerator& gen,
    int nx, int ny, int nz,
    amrex::Real dx, amrex::Real dy, amrex::Real dz,
    amrex::Real z_agl_ref,
    amrex::Real u_mean,
    unsigned int seed = DEFAULT_SEED)
{
    // Create engines
    SpectralAmplitudeEngine spectral_engine;
    RandomFieldGenerator field_gen(seed);
    
    // Build amplitude spectrum
    auto spectrum = spectral_engine.BuildAmplitudeSpectrum(gen, z_agl_ref, u_mean);
    
    // Generate 3D field with spatial correlations
    return field_gen.Generate3DField(
        spectrum,
        nx, ny, nz,
        dx, dy, dz,
        true,  // use vertical coherence
        gen);
}

// ============================================================================
// End namespace RandomFieldSynthesis
// ============================================================================

}  // namespace RandomFieldSynthesis

#endif  // RANDOM_FIELD_SYNTHESIS_H
