// ============================================================================
// mann_box_multiscale_adaptation.H
// 
// PHASE 5: Terrain Adaptation & Flow Regimes - Part 3: Multi-Scale Terrain Cascade
// 
// Implements multi-scale terrain analysis and hierarchical anisotropy modifications.
// Combines effects of large-scale, medium-scale, and small-scale terrain features
// to produce physically realistic turbulence modifications.
//
// Key Features:
// - Multi-scale terrain feature extraction (large, medium, small)
// - Hierarchical spectral tensor modifications
// - Cascade combination of effects
// - Height-dependent cascade weighting
// - GPU-ready device-compatible kernels
//
// Mathematical Foundation:
// 
// 1. Multi-Scale Terrain Decomposition
//    h(x,y) = h_large(x,y) + h_medium(x,y) + h_small(x,y)
//    
//    Scales are characterized by their wavelength ranges:
//    - Large-scale:   λ > 1000 m   (ridges, valleys, basins)
//    - Medium-scale:  100 m < λ < 1000 m  (hills, slopes)
//    - Small-scale:   λ < 100 m   (roughness, micro-topography)
//
// 2. Height-Dependent Sensitivity
//    Near surface (z << δ): Sensitive to all scales
//    Mixed layer (z ~ δ):   Sensitive to medium and large
//    Above BL (z > δ):      Only sensitive to large-scale
//
// 3. Cascade Combination
//    Modifications are combined multiplicatively for variance:
//    σ² = σ²_ref × f_large × f_medium × f_small
//    
//    And by RMS combination for intensity:
//    σ = √(σ²_large + σ²_medium + σ²_small - σ²_ref)
//
// Reference:
//   Kaimal, J. C., & Finnigan, J. J. (1994). Atmospheric boundary layer flows:
//   their structure and measurement. Oxford University Press.
//   
//   Dyrbye, C., & Hansen, S. O. (1997). Wind loads on structures.
//   John Wiley & Sons.
//
// Phase 5 Features:
//   1. Multi-scale terrain feature classification
//   2. Hierarchical tensor modifications
//   3. Height-dependent cascade weighting
//   4. Integration with Phase 3 spectral tensor and Phase 5 slope adaptation
// ============================================================================

#ifndef MANN_BOX_MULTISCALE_ADAPTATION_H
#define MANN_BOX_MULTISCALE_ADAPTATION_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <algorithm>
#include <array>
#include "solver_math_constants.H"

// ============================================================================
// Terrain Scale Enumeration
// ============================================================================

enum class TerrainScale : int {
    SMALL_SCALE = 0,   // λ < 100 m (roughness elements)
    MEDIUM_SCALE = 1,  // 100 m < λ < 1000 m (hills, slopes)
    LARGE_SCALE = 2,   // λ > 1000 m (ridges, valleys, basins)
    INVALID = -1
};

// ============================================================================
// Multi-Scale Terrain Information Structure
// ============================================================================

struct MultiScaleTerrainInfo {
    // Terrain elevation components at different scales
    amrex::Real elevation_large;       // Large-scale elevation component [m]
    amrex::Real elevation_medium;      // Medium-scale elevation component [m]
    amrex::Real elevation_small;       // Small-scale elevation component [m]
    
    // Gradient magnitudes at different scales
    amrex::Real gradient_large;        // |∇h_large| [–]
    amrex::Real gradient_medium;       // |∇h_medium| [–]
    amrex::Real gradient_small;        // |∇h_small| [–]
    
    // Slope angles at different scales
    amrex::Real slope_angle_large;     // arctan(gradient_large) [rad]
    amrex::Real slope_angle_medium;    // arctan(gradient_medium) [rad]
    amrex::Real slope_angle_small;     // arctan(gradient_small) [rad]
    
    // Terrain roughness
    amrex::Real roughness_length_z0;   // Aerodynamic roughness [m]
    
    // Constructor
    MultiScaleTerrainInfo()
        : elevation_large(0.0), elevation_medium(0.0), elevation_small(0.0),
          gradient_large(0.0), gradient_medium(0.0), gradient_small(0.0),
          slope_angle_large(0.0), slope_angle_medium(0.0), slope_angle_small(0.0),
          roughness_length_z0(0.05) {}
};

// ============================================================================
// Multi-Scale Modification Factors
// ============================================================================

struct MultiScaleFactors {
    // Modification factors for each scale
    amrex::Real factor_large;      // Large-scale terrain effect
    amrex::Real factor_medium;     // Medium-scale terrain effect
    amrex::Real factor_small;      // Small-scale terrain effect (roughness)
    
    // Height-dependent weights
    amrex::Real weight_large;      // How much large-scale contributes
    amrex::Real weight_medium;     // How much medium-scale contributes
    amrex::Real weight_small;      // How much small-scale contributes
    
    // Combined effect
    amrex::Real combined_factor;   // Overall multiplicative factor
    
    // Constructor
    MultiScaleFactors()
        : factor_large(1.0), factor_medium(1.0), factor_small(1.0),
          weight_large(0.5), weight_medium(0.3), weight_small(0.2),
          combined_factor(1.0) {}
};

// ============================================================================
// Namespace for multi-scale terrain operations
// ============================================================================

namespace MannBoxMultiScaleTerrain {

// ============================================================================
// PART 1: Terrain Scale Identification
// ============================================================================

// Classify terrain scale based on wavelength or gradient characteristics
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
TerrainScale classify_terrain_scale(
    amrex::Real wavelength,  // Wavelength or spatial extent [m]
    amrex::Real gradient = amrex::Real(0.0)  // Optional gradient for fallback
) noexcept
{
    // Wavelength-based classification
    if (wavelength < amrex::Real(100.0)) {
        return TerrainScale::SMALL_SCALE;
    } else if (wavelength < amrex::Real(1000.0)) {
        return TerrainScale::MEDIUM_SCALE;
    } else {
        return TerrainScale::LARGE_SCALE;
    }
}

// ============================================================================
// PART 2: Scale-Specific Modification Factors
// ============================================================================

// Compute modification factor for large-scale terrain
// Large-scale features (ridges, valleys) create long-wavelength perturbations
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_large_scale_factor(
    amrex::Real gradient_large,      // Large-scale terrain gradient
    amrex::Real height_agl,          // Height above ground [m]
    amrex::Real boundary_layer_height = amrex::Real(500.0)  // BL height [m]
) noexcept
{
    // Large-scale features affect entire BL depth
    // Effect is strongest at intermediate heights (hills/ridges)
    
    // Slope contribution: enhance variance on slopes
    amrex::Real slope_angle = std::atan(gradient_large);
    amrex::Real slope_factor = amrex::Real(1.0) + amrex::Real(0.5) * gradient_large;
    
    // Height weighting: peak effect at mid-BL height
    amrex::Real height_fraction = height_agl / (boundary_layer_height + amrex::Real(1e-8));
    height_fraction = std::min(height_fraction, amrex::Real(1.0));
    
    // Gaussian weighting, peak at z ~ 0.3*δ
    amrex::Real height_weight = std::exp(-std::pow((height_fraction - amrex::Real(0.3)) / 
                                                    amrex::Real(0.3), amrex::Real(2.0)));
    
    // Combined: base enhancement × slope effect × height weighting
    return amrex::Real(1.1) * slope_factor * height_weight;
}

// Compute modification factor for medium-scale terrain
// Medium-scale features (hills, gentle slopes) create moderate-wavelength effects
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_medium_scale_factor(
    amrex::Real gradient_medium,     // Medium-scale terrain gradient
    amrex::Real height_agl,          // Height above ground [m]
    amrex::Real boundary_layer_height = amrex::Real(500.0)
) noexcept
{
    // Medium-scale features primarily affect lower BL (z < 0.5*δ)
    
    // Slope contribution
    amrex::Real slope_factor = amrex::Real(1.0) + amrex::Real(0.7) * gradient_medium;
    
    // Height weighting: strongest near surface, decay with height
    amrex::Real height_fraction = height_agl / (boundary_layer_height + amrex::Real(1e-8));
    height_fraction = std::min(height_fraction, amrex::Real(1.0));
    
    // Exponential decay with height
    amrex::Real height_weight = std::exp(-amrex::Real(3.0) * height_fraction);
    
    return amrex::Real(1.2) * slope_factor * height_weight;
}

// Compute modification factor for small-scale terrain (roughness)
// Small-scale elements (vegetation, buildings) create local drag and roughness
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_small_scale_factor(
    amrex::Real roughness_length,    // Aerodynamic roughness z₀ [m]
    amrex::Real height_agl,          // Height above ground [m]
    amrex::Real reference_height = amrex::Real(10.0)  // Reference height [m]
) noexcept
{
    // Roughness creates drag and dissipation in the surface layer
    // Effect is strongest near surface
    
    // Log-law profile scaling
    // u*(z) ∝ ln(z/z0)
    // Turbulence intensity ∝ 1/ln(z/z0) approximately
    
    amrex::Real z_ratio = height_agl / (roughness_length + amrex::Real(1e-8));
    z_ratio = std::max(z_ratio, amrex::Real(1.0));
    
    // Logarithmic scaling of roughness effect
    // Higher roughness → higher turbulence intensity near surface
    amrex::Real log_factor = std::log(z_ratio) / std::log(reference_height / roughness_length + amrex::Real(1e-8));
    log_factor = std::max(log_factor, amrex::Real(0.1));
    
    // Roughness intensity: based on z₀ magnitude
    // z₀ = 0.01 m (smooth)   → base turbulence
    // z₀ = 0.5 m (forest)    → enhanced turbulence
    // z₀ = 2.0 m (urban)     → strong enhancement
    
    amrex::Real roughness_intensity = amrex::Real(1.0) + 
                                     amrex::Real(2.0) * std::log(roughness_length / amrex::Real(0.05));
    roughness_intensity = std::max(roughness_intensity, amrex::Real(0.5));
    
    return roughness_intensity * log_factor;
}

// ============================================================================
// PART 3: Height-Dependent Cascade Weighting
// ============================================================================

// Compute height-dependent weights for each scale
// Weighting changes with height to account for different scale influences
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
MultiScaleFactors compute_height_dependent_weights(
    amrex::Real height_agl,           // Height above ground [m]
    amrex::Real boundary_layer_height = amrex::Real(500.0)
) noexcept
{
    MultiScaleFactors factors;
    
    // Compute relative height
    amrex::Real height_fraction = height_agl / (boundary_layer_height + amrex::Real(1e-8));
    height_fraction = std::min(height_fraction, amrex::Real(1.0));
    
    // Surface layer (z < 0.1*δ): all scales important
    if (height_fraction < amrex::Real(0.1)) {
        factors.weight_small = amrex::Real(0.5);   // Roughness/drag dominant
        factors.weight_medium = amrex::Real(0.35);
        factors.weight_large = amrex::Real(0.15);
    }
    // Lower mixed layer (0.1*δ < z < 0.5*δ): medium and large scales
    else if (height_fraction < amrex::Real(0.5)) {
        factors.weight_small = amrex::Real(0.2);   // Roughness fading
        factors.weight_medium = amrex::Real(0.5);  // Hills/slopes important
        factors.weight_large = amrex::Real(0.3);
    }
    // Upper mixed layer (0.5*δ < z < δ): primarily large scales
    else if (height_fraction < amrex::Real(1.0)) {
        factors.weight_small = amrex::Real(0.05);
        factors.weight_medium = amrex::Real(0.3);
        factors.weight_large = amrex::Real(0.65);
    }
    // Free atmosphere (z > δ): minimal effect
    else {
        factors.weight_small = amrex::Real(0.0);
        factors.weight_medium = amrex::Real(0.1);
        factors.weight_large = amrex::Real(0.5);
    }
    
    return factors;
}

// ============================================================================
// PART 4: Cascade Combination of Effects
// ============================================================================

// Combine multi-scale factors using weighted product (geometric mean)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real combine_multiscale_factors_product(
    amrex::Real factor_large,
    amrex::Real factor_medium,
    amrex::Real factor_small,
    amrex::Real weight_large,
    amrex::Real weight_medium,
    amrex::Real weight_small
) noexcept
{
    // Normalize weights
    amrex::Real total_weight = weight_large + weight_medium + weight_small;
    if (total_weight < amrex::Real(1e-10)) {
        return amrex::Real(1.0);
    }
    
    weight_large /= total_weight;
    weight_medium /= total_weight;
    weight_small /= total_weight;
    
    // Geometric combination (product of weighted factors)
    // f_combined = f_large^w_large × f_medium^w_medium × f_small^w_small
    
    amrex::Real log_sum = weight_large * std::log(std::max(factor_large, amrex::Real(0.1))) +
                         weight_medium * std::log(std::max(factor_medium, amrex::Real(0.1))) +
                         weight_small * std::log(std::max(factor_small, amrex::Real(0.1)));
    
    return std::exp(log_sum);
}

// Combine multi-scale factors using RMS (quadrature) combination
// Used for additive contributions like variances
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real combine_multiscale_factors_rms(
    amrex::Real factor_large,
    amrex::Real factor_medium,
    amrex::Real factor_small,
    amrex::Real weight_large,
    amrex::Real weight_medium,
    amrex::Real weight_small
) noexcept
{
    // Normalize weights
    amrex::Real total_weight = weight_large + weight_medium + weight_small;
    if (total_weight < amrex::Real(1e-10)) {
        return amrex::Real(1.0);
    }
    
    weight_large /= total_weight;
    weight_medium /= total_weight;
    weight_small /= total_weight;
    
    // RMS combination: f_combined = √(w_l*f_l² + w_m*f_m² + w_s*f_s²)
    amrex::Real sum_sq = weight_large * factor_large * factor_large +
                        weight_medium * factor_medium * factor_medium +
                        weight_small * factor_small * factor_small;
    
    return std::sqrt(std::max(sum_sq, amrex::Real(0.01)));
}

// ============================================================================
// PART 5: Complete Multi-Scale Computation
// ============================================================================

// Compute complete multi-scale factor for tensor modification
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_complete_multiscale_factor(
    const MultiScaleTerrainInfo& terrain,
    amrex::Real height_agl,
    amrex::Real boundary_layer_height = amrex::Real(500.0)
) noexcept
{
    // Step 1: Compute factors for each scale
    amrex::Real factor_large = compute_large_scale_factor(
        terrain.gradient_large, height_agl, boundary_layer_height);
    
    amrex::Real factor_medium = compute_medium_scale_factor(
        terrain.gradient_medium, height_agl, boundary_layer_height);
    
    amrex::Real factor_small = compute_small_scale_factor(
        terrain.roughness_length_z0, height_agl);
    
    // Step 2: Compute height-dependent weights
    MultiScaleFactors weights = compute_height_dependent_weights(height_agl, boundary_layer_height);
    
    // Step 3: Combine using geometric mean
    amrex::Real combined = combine_multiscale_factors_product(
        factor_large, factor_medium, factor_small,
        weights.weight_large, weights.weight_medium, weights.weight_small);
    
    return combined;
}

// ============================================================================
// PART 6: Apply Multi-Scale Modifications to Spectral Components
// ============================================================================

// Apply multi-scale modifications to spectral tensor components
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void apply_multiscale_variance_modifications(
    const MultiScaleTerrainInfo& terrain,
    amrex::Real height_agl,
    amrex::Real boundary_layer_height,
    amrex::Real& S_uu,  // In/Out: u-component variance
    amrex::Real& S_vv,  // In/Out: v-component variance
    amrex::Real& S_ww   // In/Out: w-component variance
) noexcept
{
    // Compute base multi-scale factor
    amrex::Real base_factor = compute_complete_multiscale_factor(
        terrain, height_agl, boundary_layer_height);
    
    // Apply different modifications based on scale and flow direction
    
    // Large-scale primarily affects horizontal components
    amrex::Real factor_u = amrex::Real(0.8) + amrex::Real(0.2) * base_factor;
    amrex::Real factor_v = amrex::Real(0.8) + amrex::Real(0.2) * base_factor;
    
    // Medium and small scales affect all components, especially vertical
    amrex::Real factor_w = base_factor;  // Full factor for vertical
    
    // Apply modifications
    S_uu *= factor_u * factor_u;
    S_vv *= factor_v * factor_v;
    S_ww *= factor_w * factor_w;
}

// Apply multi-scale modifications to length scales
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void apply_multiscale_lengthscale_modifications(
    const MultiScaleTerrainInfo& terrain,
    amrex::Real height_agl,
    amrex::Real boundary_layer_height,
    amrex::Real& L_u,  // In/Out: u-component length scale
    amrex::Real& L_v,  // In/Out: v-component length scale
    amrex::Real& L_w   // In/Out: w-component length scale
) noexcept
{
    // Multi-scale terrain increases effective length scales
    // by creating larger coherent structures
    
    amrex::Real height_fraction = height_agl / (boundary_layer_height + amrex::Real(1e-8));
    height_fraction = std::min(height_fraction, amrex::Real(1.0));
    
    // Large-scale terrain: longer length scales
    // Effect diminishes with height above terrain
    amrex::Real L_factor_large = amrex::Real(1.0) + 
                                 amrex::Real(0.5) * terrain.gradient_large * 
                                 std::exp(-height_fraction);
    
    // Medium-scale terrain: moderate enhancement
    amrex::Real L_factor_medium = amrex::Real(1.0) + 
                                  amrex::Real(0.3) * terrain.gradient_medium * 
                                  std::exp(-amrex::Real(2.0) * height_fraction);
    
    // Small-scale roughness: slight enhancement near surface
    amrex::Real L_factor_small = amrex::Real(1.0) + 
                                 amrex::Real(0.2) * std::log(terrain.roughness_length_z0 / amrex::Real(0.05)) * 
                                 std::exp(-amrex::Real(5.0) * height_fraction);
    
    // Combine factors
    amrex::Real combined = L_factor_large * L_factor_medium * L_factor_small;
    
    // Apply to length scales (horizontal more affected than vertical)
    L_u *= amrex::Real(0.9) * combined;
    L_v *= amrex::Real(0.9) * combined;
    L_w *= amrex::Real(1.1) * combined;
}

// ============================================================================
// PART 7: Multi-Scale Analysis Diagnostics
// ============================================================================

// Compute dominance of each scale (for diagnostics/logging)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void compute_scale_dominance(
    amrex::Real factor_large,
    amrex::Real factor_medium,
    amrex::Real factor_small,
    amrex::Real& dominance_large,    // Output: 0-1
    amrex::Real& dominance_medium,   // Output: 0-1
    amrex::Real& dominance_small     // Output: 0-1
) noexcept
{
    // Compute contributions (deviations from baseline 1.0)
    amrex::Real contrib_large = std::max(amrex::Real(0.0), factor_large - amrex::Real(1.0));
    amrex::Real contrib_medium = std::max(amrex::Real(0.0), factor_medium - amrex::Real(1.0));
    amrex::Real contrib_small = std::max(amrex::Real(0.0), factor_small - amrex::Real(1.0));
    
    // Normalize
    amrex::Real total = contrib_large + contrib_medium + contrib_small + amrex::Real(1e-10);
    
    dominance_large = contrib_large / total;
    dominance_medium = contrib_medium / total;
    dominance_small = contrib_small / total;
}

} // namespace MannBoxMultiScaleTerrain

#endif // MANN_BOX_MULTISCALE_ADAPTATION_H
