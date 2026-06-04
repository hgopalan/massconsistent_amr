// ============================================================================
// gpu_turbulence_synthesizer.H
// GPU-Accelerated Turbulence Synthesis Manager
//
// This header provides a high-level manager class for GPU-accelerated
// turbulence synthesis. It handles:
//   - Spectral computation on GPU
//   - Memory transfers and synchronization
//   - Kernel launch configuration
//   - Fallback to CPU when GPU is unavailable
//
// Performance Targets:
//   - Von Kármán spectrum: 5-8× speedup
//   - Kaimal spectrum: 4-6× speedup
//   - Intensity computation: 6-10× speedup
//   - Total synthesis: 5-10× speedup
//
// ============================================================================

#ifndef GPU_TURBULENCE_SYNTHESIZER_H
#define GPU_TURBULENCE_SYNTHESIZER_H

#include "gpu_acceleration.H"
#include <memory>
#include <vector>
#include <cmath>

// ============================================================================
// GPU Turbulence Spectrum Computer
// ============================================================================
// High-level interface for GPU-accelerated spectral computations

class GPUSpectrumComputer {
public:
    // Constructor
    GPUSpectrumComputer()
        : use_gpu_(false), block_size_(256), n_blocks_(0)
    {
        #ifdef AMREX_USE_GPU
        use_gpu_ = true;
        #endif
    }
    
    // Destructor
    ~GPUSpectrumComputer() = default;
    
    // ====================================================================
    // Von Kármán Spectrum Computation
    // ====================================================================
    // Computes Von Kármán spectral density for frequency array
    // Returns speedup factor for profiling/diagnostics
    // ====================================================================
    
    amrex::Real compute_vonkarman_spectrum(
        const std::vector<amrex::Real>& frequencies,
        amrex::Real length_scale,
        amrex::Real mean_wind_speed,
        amrex::Real velocity_rms,
        std::vector<amrex::Real>& spectrum_out)
    {
        int n_freq = frequencies.size();
        
        if (!use_gpu_) {
            // CPU fallback
            return compute_vonkarman_spectrum_cpu(frequencies, length_scale,
                                                 mean_wind_speed, velocity_rms,
                                                 spectrum_out);
        }
        
        #ifdef AMREX_USE_GPU
        // GPU version
        return compute_vonkarman_spectrum_gpu(frequencies, length_scale,
                                             mean_wind_speed, velocity_rms,
                                             spectrum_out);
        #else
        return compute_vonkarman_spectrum_cpu(frequencies, length_scale,
                                             mean_wind_speed, velocity_rms,
                                             spectrum_out);
        #endif
    }
    
    // ====================================================================
    // Kaimal Spectrum Computation
    // ====================================================================
    
    amrex::Real compute_kaimal_spectrum(
        const std::vector<amrex::Real>& frequencies,
        amrex::Real length_scale,
        amrex::Real mean_wind_speed,
        amrex::Real velocity_rms,
        std::vector<amrex::Real>& spectrum_out)
    {
        int n_freq = frequencies.size();
        
        if (!use_gpu_) {
            // CPU fallback
            return compute_kaimal_spectrum_cpu(frequencies, length_scale,
                                              mean_wind_speed, velocity_rms,
                                              spectrum_out);
        }
        
        #ifdef AMREX_USE_GPU
        // GPU version
        return compute_kaimal_spectrum_gpu(frequencies, length_scale,
                                          mean_wind_speed, velocity_rms,
                                          spectrum_out);
        #else
        return compute_kaimal_spectrum_cpu(frequencies, length_scale,
                                          mean_wind_speed, velocity_rms,
                                          spectrum_out);
        #endif
    }
    
    // ====================================================================
    // Intensity Computation (Power-Law Model)
    // ====================================================================
    
    amrex::Real compute_intensity_powerlaw(
        const std::vector<amrex::Real>& heights,
        amrex::Real intensity_ref,
        amrex::Real z_ref,
        amrex::Real exponent,
        std::vector<amrex::Real>& intensity_out)
    {
        int n_heights = heights.size();
        
        if (!use_gpu_) {
            // CPU fallback
            return compute_intensity_powerlaw_cpu(heights, intensity_ref,
                                                 z_ref, exponent, intensity_out);
        }
        
        #ifdef AMREX_USE_GPU
        // GPU version
        return compute_intensity_powerlaw_gpu(heights, intensity_ref,
                                             z_ref, exponent, intensity_out);
        #else
        return compute_intensity_powerlaw_cpu(heights, intensity_ref,
                                             z_ref, exponent, intensity_out);
        #endif
    }
    
    // ====================================================================
    // Intensity Computation (Logarithmic Model)
    // ====================================================================
    
    amrex::Real compute_intensity_logarithmic(
        const std::vector<amrex::Real>& heights,
        amrex::Real intensity_ref,
        amrex::Real z_ref,
        amrex::Real z_0,
        std::vector<amrex::Real>& intensity_out)
    {
        int n_heights = heights.size();
        
        if (!use_gpu_) {
            // CPU fallback
            return compute_intensity_logarithmic_cpu(heights, intensity_ref,
                                                    z_ref, z_0, intensity_out);
        }
        
        #ifdef AMREX_USE_GPU
        // GPU version
        return compute_intensity_logarithmic_gpu(heights, intensity_ref,
                                                z_ref, z_0, intensity_out);
        #else
        return compute_intensity_logarithmic_cpu(heights, intensity_ref,
                                                z_ref, z_0, intensity_out);
        #endif
    }
    
    // ====================================================================
    // Terrain Masking
    // ====================================================================
    
    amrex::Real apply_terrain_mask(
        std::vector<amrex::Real>& velocity_field,
        const std::vector<amrex::Real>& terrain_elevation,
        int nx, int ny, int nz,
        amrex::Real dz,
        amrex::Real z_bottom)
    {
        if (!use_gpu_) {
            // CPU fallback
            return apply_terrain_mask_cpu(velocity_field, terrain_elevation,
                                         nx, ny, nz, dz, z_bottom);
        }
        
        #ifdef AMREX_USE_GPU
        // GPU version
        return apply_terrain_mask_gpu(velocity_field, terrain_elevation,
                                     nx, ny, nz, dz, z_bottom);
        #else
        return apply_terrain_mask_cpu(velocity_field, terrain_elevation,
                                     nx, ny, nz, dz, z_bottom);
        #endif
    }
    
    // ====================================================================
    // Utility Methods
    // ====================================================================
    
    bool is_gpu_enabled() const { return use_gpu_; }
    
    void set_block_size(int block_size) 
    {
        block_size_ = (block_size > 0 && block_size <= 1024) ? block_size : 256;
    }
    
    int get_block_size() const { return block_size_; }

private:
    // ====================================================================
    // CPU Implementations (Fallback)
    // ====================================================================
    
    amrex::Real compute_vonkarman_spectrum_cpu(
        const std::vector<amrex::Real>& frequencies,
        amrex::Real length_scale,
        amrex::Real mean_wind_speed,
        amrex::Real velocity_rms,
        std::vector<amrex::Real>& spectrum_out)
    {
        spectrum_out.resize(frequencies.size());
        
        amrex::Real U_mean = std::max(mean_wind_speed, amrex::Real(0.1));
        amrex::Real u_rms = std::max(velocity_rms, amrex::Real(1e-6));
        amrex::Real L_u = std::max(length_scale, amrex::Real(1.0));
        
        for (size_t i = 0; i < frequencies.size(); ++i) {
            amrex::Real f_hat = frequencies[i] * L_u / U_mean;
            amrex::Real numerator = amrex::Real(4.0) * L_u * u_rms * u_rms;
            amrex::Real denominator_base = amrex::Real(1.0) + amrex::Real(70.8) * f_hat * f_hat;
            amrex::Real denominator = std::pow(denominator_base, amrex::Real(5.0) / amrex::Real(6.0));
            spectrum_out[i] = numerator / denominator;
        }
        
        return amrex::Real(1.0);  // Speedup factor (1x for CPU)
    }
    
    amrex::Real compute_kaimal_spectrum_cpu(
        const std::vector<amrex::Real>& frequencies,
        amrex::Real length_scale,
        amrex::Real mean_wind_speed,
        amrex::Real velocity_rms,
        std::vector<amrex::Real>& spectrum_out)
    {
        spectrum_out.resize(frequencies.size());
        
        amrex::Real U_mean = std::max(mean_wind_speed, amrex::Real(0.1));
        amrex::Real u_rms = std::max(velocity_rms, amrex::Real(1e-6));
        amrex::Real L_u = std::max(length_scale, amrex::Real(1.0));
        
        for (size_t i = 0; i < frequencies.size(); ++i) {
            amrex::Real f_hat = frequencies[i] * L_u / U_mean;
            amrex::Real numerator = amrex::Real(4.0) * L_u * u_rms * u_rms * f_hat;
            amrex::Real denominator_base = amrex::Real(1.0) + amrex::Real(6.0) * f_hat;
            amrex::Real denominator = std::pow(denominator_base, amrex::Real(5.0) / amrex::Real(3.0));
            spectrum_out[i] = numerator / denominator;
        }
        
        return amrex::Real(1.0);  // Speedup factor (1x for CPU)
    }
    
    amrex::Real compute_intensity_powerlaw_cpu(
        const std::vector<amrex::Real>& heights,
        amrex::Real intensity_ref,
        amrex::Real z_ref,
        amrex::Real exponent,
        std::vector<amrex::Real>& intensity_out)
    {
        intensity_out.resize(heights.size());
        
        amrex::Real I_ref = std::min(std::max(intensity_ref, amrex::Real(0.01)), 
                                    amrex::Real(0.30));
        amrex::Real z_r = std::max(z_ref, amrex::Real(1.0));
        
        for (size_t i = 0; i < heights.size(); ++i) {
            amrex::Real z = std::max(heights[i], amrex::Real(0.1));
            amrex::Real intensity = I_ref * std::pow(z / z_r, exponent);
            intensity_out[i] = std::min(std::max(intensity, amrex::Real(0.01)), 
                                       amrex::Real(0.30));
        }
        
        return amrex::Real(1.0);  // Speedup factor (1x for CPU)
    }
    
    amrex::Real compute_intensity_logarithmic_cpu(
        const std::vector<amrex::Real>& heights,
        amrex::Real intensity_ref,
        amrex::Real z_ref,
        amrex::Real z_0,
        std::vector<amrex::Real>& intensity_out)
    {
        intensity_out.resize(heights.size());
        
        amrex::Real I_ref = std::min(std::max(intensity_ref, amrex::Real(0.01)), 
                                    amrex::Real(0.30));
        amrex::Real z_r = std::max(z_ref, amrex::Real(1.0));
        amrex::Real z_roughness = std::max(z_0, amrex::Real(0.001));
        
        for (size_t i = 0; i < heights.size(); ++i) {
            amrex::Real z = std::max(heights[i], z_roughness + amrex::Real(0.1));
            amrex::Real ln_ratio = std::log(z / z_roughness) / 
                                  std::log(z_r / z_roughness);
            amrex::Real intensity = I_ref * ln_ratio;
            intensity_out[i] = std::min(std::max(intensity, amrex::Real(0.01)), 
                                       amrex::Real(0.30));
        }
        
        return amrex::Real(1.0);  // Speedup factor (1x for CPU)
    }
    
    amrex::Real apply_terrain_mask_cpu(
        std::vector<amrex::Real>& velocity_field,
        const std::vector<amrex::Real>& terrain_elevation,
        int nx, int ny, int nz,
        amrex::Real dz,
        amrex::Real z_bottom)
    {
        // Apply terrain masking on CPU
        for (int k = 0; k < nz; ++k) {
            for (int j = 0; j < ny; ++j) {
                for (int i = 0; i < nx; ++i) {
                    int idx = (k * ny + j) * nx + i;
                    int terrain_idx = j * nx + i;
                    
                    amrex::Real z_physical = z_bottom + k * dz;
                    amrex::Real z_agl = z_physical - terrain_elevation[terrain_idx];
                    
                    // Apply masking
                    amrex::Real mask = GPUTurbulence::compute_terrain_mask(z_agl);
                    velocity_field[idx] *= mask;
                }
            }
        }
        
        return amrex::Real(1.0);  // Speedup factor (1x for CPU)
    }
    
    // ====================================================================
    // GPU Implementations (Compiled only with AMREX_USE_GPU)
    // ====================================================================
    
    #ifdef AMREX_USE_GPU
    
    amrex::Real compute_vonkarman_spectrum_gpu(
        const std::vector<amrex::Real>& frequencies,
        amrex::Real length_scale,
        amrex::Real mean_wind_speed,
        amrex::Real velocity_rms,
        std::vector<amrex::Real>& spectrum_out);
    
    amrex::Real compute_kaimal_spectrum_gpu(
        const std::vector<amrex::Real>& frequencies,
        amrex::Real length_scale,
        amrex::Real mean_wind_speed,
        amrex::Real velocity_rms,
        std::vector<amrex::Real>& spectrum_out);
    
    amrex::Real compute_intensity_powerlaw_gpu(
        const std::vector<amrex::Real>& heights,
        amrex::Real intensity_ref,
        amrex::Real z_ref,
        amrex::Real exponent,
        std::vector<amrex::Real>& intensity_out);
    
    amrex::Real compute_intensity_logarithmic_gpu(
        const std::vector<amrex::Real>& heights,
        amrex::Real intensity_ref,
        amrex::Real z_ref,
        amrex::Real z_0,
        std::vector<amrex::Real>& intensity_out);
    
    amrex::Real apply_terrain_mask_gpu(
        std::vector<amrex::Real>& velocity_field,
        const std::vector<amrex::Real>& terrain_elevation,
        int nx, int ny, int nz,
        amrex::Real dz,
        amrex::Real z_bottom);
    
    #endif
    
    // Member variables
    bool use_gpu_;
    int block_size_;
    int n_blocks_;
};

// ============================================================================
// GPU Turbulence Synthesizer (Main Interface)
// ============================================================================
// High-level API for GPU-accelerated turbulence synthesis

class GPUTurbulenceSynthesizer {
public:
    // Constructor
    GPUTurbulenceSynthesizer()
        : spectrum_computer_(std::make_unique<GPUSpectrumComputer>())
    {}
    
    // Destructor
    ~GPUTurbulenceSynthesizer() = default;
    
    // Main synthesis routine with GPU acceleration
    void synthesize_with_gpu(
        std::vector<amrex::Real>& u_fluct,     // Output: u-component fluctuations
        std::vector<amrex::Real>& v_fluct,     // Output: v-component fluctuations
        std::vector<amrex::Real>& w_fluct,     // Output: w-component fluctuations
        const std::vector<amrex::Real>& frequencies,    // Frequency bins
        const std::vector<amrex::Real>& heights,        // Heights for intensity
        amrex::Real length_scale_u,
        amrex::Real length_scale_v,
        amrex::Real length_scale_w,
        amrex::Real intensity_ref,
        amrex::Real z_ref,
        amrex::Real intensity_exponent,
        amrex::Real mean_wind_speed,
        const std::vector<amrex::Real>& terrain_elevation,  // For masking
        int nx, int ny, int nz,
        amrex::Real dz,
        amrex::Real z_bottom)
    {
        // Compute spectral densities on GPU
        std::vector<amrex::Real> spectrum_u, spectrum_v, spectrum_w;
        spectrum_computer_->compute_vonkarman_spectrum(
            frequencies, length_scale_u, mean_wind_speed, amrex::Real(1.0), spectrum_u);
        spectrum_computer_->compute_vonkarman_spectrum(
            frequencies, length_scale_v, mean_wind_speed, amrex::Real(0.8), spectrum_v);
        spectrum_computer_->compute_vonkarman_spectrum(
            frequencies, length_scale_w, mean_wind_speed, amrex::Real(0.5), spectrum_w);
        
        // Compute intensity profile on GPU
        std::vector<amrex::Real> intensity_profile;
        spectrum_computer_->compute_intensity_powerlaw(
            heights, intensity_ref, z_ref, intensity_exponent, intensity_profile);
        
        // Apply terrain masking on GPU
        spectrum_computer_->apply_terrain_mask(
            u_fluct, terrain_elevation, nx, ny, nz, dz, z_bottom);
        spectrum_computer_->apply_terrain_mask(
            v_fluct, terrain_elevation, nx, ny, nz, dz, z_bottom);
        spectrum_computer_->apply_terrain_mask(
            w_fluct, terrain_elevation, nx, ny, nz, dz, z_bottom);
    }
    
    // Check GPU availability
    bool is_gpu_available() const 
    {
        return spectrum_computer_->is_gpu_enabled();
    }
    
    // Set block size for GPU kernels
    void set_gpu_block_size(int block_size)
    {
        spectrum_computer_->set_block_size(block_size);
    }

private:
    std::unique_ptr<GPUSpectrumComputer> spectrum_computer_;
};

#endif // GPU_TURBULENCE_SYNTHESIZER_H
