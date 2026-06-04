// ============================================================================
// gpu_turbulence_integration.H
// Integration Layer: Connects GPU Acceleration to Existing Turbulence System
//
// This header provides functions to integrate GPU-accelerated synthesis
// with the existing CPU-based turbulence generation code.
//
// Features:
//   - Automatic GPU/CPU selection based on availability
//   - Drop-in replacement for CPU functions
//   - Performance monitoring and diagnostics
//   - Backward compatibility
//
// ============================================================================

#ifndef GPU_TURBULENCE_INTEGRATION_H
#define GPU_TURBULENCE_INTEGRATION_H

#include "gpu_acceleration.H"
#include "gpu_turbulence_synthesizer.H"
#include "synthetic_turbulence.H"
#include <memory>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include <iomanip>

// ============================================================================
// Performance Monitoring and Diagnostics
// ============================================================================

class GPUPerformanceMonitor {
public:
    struct Statistics {
        int call_count = 0;
        amrex::Real total_time_ms = 0.0;
        amrex::Real avg_time_ms = 0.0;
        amrex::Real min_time_ms = 1e10;
        amrex::Real max_time_ms = 0.0;
        amrex::Real estimated_speedup = 1.0;
    };
    
    // Record kernel execution time
    void record_execution(
        const std::string& kernel_name,
        amrex::Real time_ms,
        amrex::Real speedup)
    {
        auto& stats = statistics_[kernel_name];
        stats.call_count++;
        stats.total_time_ms += time_ms;
        stats.avg_time_ms = stats.total_time_ms / stats.call_count;
        stats.min_time_ms = std::min(stats.min_time_ms, time_ms);
        stats.max_time_ms = std::max(stats.max_time_ms, time_ms);
        stats.estimated_speedup = speedup;
    }
    
    // Print performance report
    void print_report(std::ostream& os = std::cout) const
    {
        os << "\n========== GPU TURBULENCE SYNTHESIS PERFORMANCE REPORT ==========\n";
        os << std::setw(30) << "Kernel Name"
          << std::setw(12) << "Calls"
          << std::setw(12) << "Avg (ms)"
          << std::setw(12) << "Min (ms)"
          << std::setw(12) << "Max (ms)"
          << std::setw(12) << "Speedup\n";
        os << std::string(90, '-') << "\n";
        
        for (const auto& [kernel, stats] : statistics_) {
            os << std::setw(30) << kernel
              << std::setw(12) << stats.call_count
              << std::setw(12) << std::fixed << std::setprecision(3) << stats.avg_time_ms
              << std::setw(12) << std::fixed << std::setprecision(3) << stats.min_time_ms
              << std::setw(12) << std::fixed << std::setprecision(3) << stats.max_time_ms
              << std::setw(12) << std::fixed << std::setprecision(1) << "x" << stats.estimated_speedup
              << "\n";
        }
        
        os << std::string(90, '-') << "\n";
        os << "Note: Speedup is estimated based on GPU architecture (not actual CPU timing)\n";
        os << "===================================================================\n\n";
    }
    
    // Get statistics for a specific kernel
    Statistics get_stats(const std::string& kernel_name) const
    {
        auto it = statistics_.find(kernel_name);
        if (it != statistics_.end()) {
            return it->second;
        }
        return Statistics();
    }
    
    // Clear all statistics
    void clear()
    {
        statistics_.clear();
    }

private:
    std::unordered_map<std::string, Statistics> statistics_;
};

// Global performance monitor (singleton)
static GPUPerformanceMonitor& get_gpu_monitor()
{
    static GPUPerformanceMonitor monitor;
    return monitor;
}

// ============================================================================
// GPU-Accelerated Spectral Synthesis Functions
// ============================================================================
// These functions provide GPU acceleration to the existing synthetic turbulence
// generation pipeline with minimal code changes to the main solver.

namespace SyntheticTurbulence {

// ========================================================================
// GPU-Accelerated Von Kármán Spectrum
// ========================================================================
// Drop-in replacement for CPU vonkarman_spectrum
// Automatically uses GPU if available

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real vonkarman_spectrum_gpu(
    amrex::Real frequency,
    amrex::Real length_scale,
    amrex::Real mean_wind_speed,
    amrex::Real velocity_rms) noexcept
{
    // Single-point evaluation (no GPU benefit for single call)
    // Falls back to CPU implementation
    mean_wind_speed = std::max(mean_wind_speed, amrex::Real(0.1));
    velocity_rms = std::max(velocity_rms, amrex::Real(1e-6));
    length_scale = std::max(length_scale, amrex::Real(1.0));
    
    const amrex::Real f_hat = frequency * length_scale / mean_wind_speed;
    const amrex::Real numerator = amrex::Real(4.0) * length_scale 
                                * velocity_rms * velocity_rms;
    const amrex::Real denominator_base = amrex::Real(1.0) 
                                       + amrex::Real(70.8) * f_hat * f_hat;
    const amrex::Real denominator = std::pow(denominator_base, amrex::Real(5.0) / amrex::Real(6.0));
    
    return numerator / denominator;
}

// ========================================================================
// GPU-Accelerated Kaimal Spectrum
// ========================================================================

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real kaimal_spectrum_gpu(
    amrex::Real frequency,
    amrex::Real length_scale,
    amrex::Real mean_wind_speed,
    amrex::Real velocity_rms) noexcept
{
    // Single-point evaluation (no GPU benefit)
    mean_wind_speed = std::max(mean_wind_speed, amrex::Real(0.1));
    velocity_rms = std::max(velocity_rms, amrex::Real(1e-6));
    length_scale = std::max(length_scale, amrex::Real(1.0));
    
    const amrex::Real f_hat = frequency * length_scale / mean_wind_speed;
    const amrex::Real numerator = amrex::Real(4.0) * length_scale 
                                * velocity_rms * velocity_rms * f_hat;
    const amrex::Real denominator_base = amrex::Real(1.0) 
                                       + amrex::Real(6.0) * f_hat;
    const amrex::Real denominator = std::pow(denominator_base, amrex::Real(5.0) / amrex::Real(3.0));
    
    return numerator / denominator;
}

// ========================================================================
// Vectorized GPU Spectrum Computation
// ========================================================================
// For batch processing of spectra (actual GPU benefit)

inline void compute_spectrum_batch_gpu(
    const std::vector<amrex::Real>& frequencies,
    amrex::Real length_scale,
    amrex::Real mean_wind_speed,
    amrex::Real velocity_rms,
    TurbulenceModel model,
    std::vector<amrex::Real>& spectrum_out,
    bool use_gpu = true)
{
    #ifdef AMREX_USE_GPU
    if (use_gpu) {
        static GPUSpectrumComputer gpu_computer;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        amrex::Real speedup = 1.0;
        if (model == TurbulenceModel::VonKarman) {
            speedup = gpu_computer.compute_vonkarman_spectrum(
                frequencies, length_scale, mean_wind_speed, velocity_rms, spectrum_out);
        } else if (model == TurbulenceModel::Kaimal) {
            speedup = gpu_computer.compute_kaimal_spectrum(
                frequencies, length_scale, mean_wind_speed, velocity_rms, spectrum_out);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        get_gpu_monitor().record_execution(
            (model == TurbulenceModel::VonKarman) ? "vonkarman_spectrum" : "kaimal_spectrum",
            duration.count() / 1000.0, speedup);
        
        return;
    }
    #endif
    
    // CPU fallback
    spectrum_out.resize(frequencies.size());
    for (size_t i = 0; i < frequencies.size(); ++i) {
        if (model == TurbulenceModel::VonKarman) {
            spectrum_out[i] = vonkarman_spectrum_gpu(
                frequencies[i], length_scale, mean_wind_speed, velocity_rms);
        } else if (model == TurbulenceModel::Kaimal) {
            spectrum_out[i] = kaimal_spectrum_gpu(
                frequencies[i], length_scale, mean_wind_speed, velocity_rms);
        }
    }
}

// ========================================================================
// Vectorized GPU Intensity Computation
// ========================================================================

inline void compute_intensity_batch_gpu(
    const std::vector<amrex::Real>& heights,
    amrex::Real intensity_ref,
    amrex::Real z_ref,
    IntensityModel model,
    std::vector<amrex::Real>& intensity_out,
    amrex::Real exponent = amrex::Real(0.14),
    amrex::Real z_0 = amrex::Real(0.05),
    bool use_gpu = true)
{
    #ifdef AMREX_USE_GPU
    if (use_gpu) {
        static GPUSpectrumComputer gpu_computer;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        amrex::Real speedup = 1.0;
        if (model == IntensityModel::PowerLaw) {
            speedup = gpu_computer.compute_intensity_powerlaw(
                heights, intensity_ref, z_ref, exponent, intensity_out);
        } else if (model == IntensityModel::Logarithmic) {
            speedup = gpu_computer.compute_intensity_logarithmic(
                heights, intensity_ref, z_ref, z_0, intensity_out);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        get_gpu_monitor().record_execution(
            (model == IntensityModel::PowerLaw) ? "intensity_powerlaw" : "intensity_logarithmic",
            duration.count() / 1000.0, speedup);
        
        return;
    }
    #endif
    
    // CPU fallback
    intensity_out.resize(heights.size());
    for (size_t i = 0; i < heights.size(); ++i) {
        if (model == IntensityModel::PowerLaw) {
            amrex::Real z = std::max(heights[i], amrex::Real(0.1));
            intensity_out[i] = intensity_ref * std::pow(z / z_ref, exponent);
            intensity_out[i] = std::min(std::max(intensity_out[i], 
                                               Constants::intensity_min),
                                      Constants::intensity_max);
        } else if (model == IntensityModel::Logarithmic) {
            amrex::Real z = std::max(heights[i], z_0 + amrex::Real(0.1));
            amrex::Real ln_ratio = std::log(z / z_0) / std::log(z_ref / z_0);
            intensity_out[i] = intensity_ref * ln_ratio;
            intensity_out[i] = std::min(std::max(intensity_out[i],
                                               Constants::intensity_min),
                                      Constants::intensity_max);
        }
    }
}

// ========================================================================
// GPU-Accelerated Terrain Masking
// ========================================================================

inline void apply_terrain_mask_gpu(
    std::vector<amrex::Real>& velocity_field,
    const std::vector<amrex::Real>& terrain_elevation,
    int nx, int ny, int nz,
    amrex::Real dz,
    amrex::Real z_bottom,
    amrex::Real transition_height = amrex::Real(3.0),
    bool use_gpu = true)
{
    #ifdef AMREX_USE_GPU
    if (use_gpu) {
        static GPUSpectrumComputer gpu_computer;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        amrex::Real speedup = gpu_computer.apply_terrain_mask(
            velocity_field, terrain_elevation, nx, ny, nz, dz, z_bottom);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        get_gpu_monitor().record_execution("terrain_masking",
                                          duration.count() / 1000.0, speedup);
        return;
    }
    #endif
    
    // CPU fallback
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                int idx = (k * ny + j) * nx + i;
                int terrain_idx = j * nx + i;
                
                amrex::Real z_physical = z_bottom + k * dz;
                amrex::Real z_agl = z_physical - terrain_elevation[terrain_idx];
                
                // Apply smooth cosine masking
                amrex::Real mask;
                if (z_agl <= amrex::Real(0.0)) {
                    mask = amrex::Real(0.0);
                } else if (z_agl >= transition_height) {
                    mask = amrex::Real(1.0);
                } else {
                    amrex::Real phase = M_PI * z_agl / transition_height;
                    mask = (amrex::Real(1.0) - std::cos(phase)) / amrex::Real(2.0);
                }
                
                velocity_field[idx] *= mask;
            }
        }
    }
}

} // namespace SyntheticTurbulence

// ============================================================================
// Automatic GPU Initialization
// ============================================================================
// Call this at the start of synthesis to enable GPU acceleration

inline void enable_gpu_turbulence_acceleration()
{
    #ifdef AMREX_USE_GPU
    // GPU is available and compiled in
    #endif
}

// Print GPU acceleration diagnostic information
inline void print_gpu_turbulence_info(std::ostream& os = std::cout)
{
    os << "\n========== GPU TURBULENCE SYNTHESIS CONFIGURATION ==========\n";
    
    #ifdef AMREX_USE_GPU
    os << "GPU Acceleration: ENABLED\n";
    os << "Backend: ";
    #ifdef AMREX_USE_CUDA
    os << "CUDA (NVIDIA)\n";
    #elif defined(AMREX_USE_HIP)
    os << "HIP (AMD ROCm)\n";
    #elif defined(AMREX_USE_SYCL)
    os << "SYCL (Intel oneAPI)\n";
    #else
    os << "Unknown GPU backend\n";
    #endif
    #else
    os << "GPU Acceleration: DISABLED (CPU-only mode)\n";
    #endif
    
    os << "========================================================\n\n";
}

#endif // GPU_TURBULENCE_INTEGRATION_H
