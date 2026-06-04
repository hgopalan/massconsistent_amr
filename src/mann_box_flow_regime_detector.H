// ============================================================================
// mann_box_flow_regime_detector.H
// 
// PHASE 5: Terrain Adaptation & Flow Regimes - Part 1: Flow Regime Detection
// 
// Implements automated detection of wind flow regimes over complex terrain,
// including acceleration zones, flow separation, stagnation points, and
// channel flows. Provides automatic tensor modifications based on detected
// regimes.
//
// Key Features:
// - Multi-regime classification (acceleration, separation, stagnation, channeling)
// - Automatic turbulence parameter adjustment per regime
// - Boundary layer height (δ) explicit modeling
// - Surface layer vs mixed layer classification
// - Height-dependent tensor modifications
// - GPU-ready device-compatible kernels
//
// Mathematical Foundation:
// 
// 1. Flow Regime Classification
//    Regimes are detected using local flow properties:
//    - Acceleration: ∇·u > threshold (flow speeding up)
//    - Separation: ∇×u indicates recirculation
//    - Stagnation: |u| < threshold, ∇p > 0
//    - Channeling: Flow aligned with valley axis
//    - Neutral: Mixed layer turbulence
//
// 2. Boundary Layer Height Classification
//    Explicit BL height δ enables profile modeling:
//    - z < δ_surface: Surface layer (constant flux, log-law)
//    - δ_surface < z < δ: Mixed layer (buoyancy-driven)
//    - z > δ: Free atmosphere (external flow)
//
// 3. Relative Height Normalization
//    ζ = z / δ  (normalized height)
//    Different tensor modifications applied per layer
//
// Reference:
//   Jackson, P. S., & Hunt, J. C. (1975). Turbulent wind flow over a low hill.
//   Quarterly Journal of the Royal Meteorological Society, 101(430), 929-955.
//   
//   Stull, R. B. (1988). An Introduction to Boundary Layer Meteorology.
//   Kluwer Academic Publishers.
//
// Phase 5 Features:
//   1. Flow regime detection from velocity field
//   2. Automatic parameter adjustment per regime
//   3. Boundary layer height classification
//   4. Height-dependent tensor modifications
//   5. Integration with Phase 3 spectral tensor
// ============================================================================

#ifndef MANN_BOX_FLOW_REGIME_DETECTOR_H
#define MANN_BOX_FLOW_REGIME_DETECTOR_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <algorithm>
#include <array>
#include "math_constants.H"

// ============================================================================
// Flow Regime Enumeration
// ============================================================================

enum class FlowRegime : int {
    NEUTRAL = 0,        // Mixed layer, well-mixed turbulence
    ACCELERATION = 1,   // Speed-up zone (enhanced streamwise)
    SEPARATION = 2,     // Recirculation zone (high isotropy)
    STAGNATION = 3,     // Near stagnation point (very weak flow)
    CHANNELING = 4,     // Valley flow, aligned channeling
    INVALID = -1
};

// ============================================================================
// Boundary Layer Classification
// ============================================================================

enum class BoundaryLayerRegion : int {
    SURFACE_LAYER = 0,     // z < 0.1*δ, log-law profile
    MIXED_LAYER = 1,       // 0.1*δ < z < δ, well-mixed
    FREE_ATMOSPHERE = 2,   // z > δ, external flow
    INVALID = -1
};

// ============================================================================
// Flow Properties Structure
// 
// Stores local flow properties for regime classification
// ============================================================================

struct FlowProperties {
    // Velocity gradient metrics
    amrex::Real velocity_magnitude;      // |u| [m/s]
    amrex::Real velocity_gradient_mag;   // |∇u| [1/s]
    amrex::Real divergence;              // ∇·u [1/s]
    amrex::Real vorticity_magnitude;     // |∇×u| [1/s]
    
    // Spatial metrics
    amrex::Real terrain_slope;           // Local terrain slope [–] (0 to 1)
    amrex::Real pressure_gradient;       // |∇p| [Pa/m]
    amrex::Real height_agl;              // Height above ground [m]
    
    // Flow characteristics
    amrex::Real u_component;             // u-component velocity [m/s]
    amrex::Real v_component;             // v-component velocity [m/s]
    amrex::Real w_component;             // w-component velocity [m/s]
    
    // Constructor
    FlowProperties()
        : velocity_magnitude(0.0), velocity_gradient_mag(0.0),
          divergence(0.0), vorticity_magnitude(0.0),
          terrain_slope(0.0), pressure_gradient(0.0), height_agl(0.0),
          u_component(0.0), v_component(0.0), w_component(0.0) {}
};

// ============================================================================
// Regime Classification Thresholds Structure
// 
// Configurable thresholds for regime detection
// ============================================================================

struct RegimeThresholds {
    // Velocity thresholds
    amrex::Real min_velocity_for_flow = amrex::Real(0.5);      // Minimum flow speed [m/s]
    amrex::Real stagnation_velocity_threshold = amrex::Real(0.2); // Stagnation speed [m/s]
    
    // Gradient thresholds
    amrex::Real divergence_acceleration_threshold = amrex::Real(0.02);  // ∇·u threshold [1/s]
    amrex::Real vorticity_separation_threshold = amrex::Real(0.01);     // |∇×u| threshold [1/s]
    
    // Terrain thresholds
    amrex::Real steep_slope_threshold = amrex::Real(0.577);    // tan(30°) for steep terrain
    amrex::Real channeling_slope_threshold = amrex::Real(0.2); // Gentle slope for channeling
    
    // Pressure thresholds
    amrex::Real high_pressure_gradient_threshold = amrex::Real(5.0); // |∇p| [Pa/m]
    
    // Boundary layer
    amrex::Real surface_layer_fraction = amrex::Real(0.1);     // z_surface = 0.1*δ
};

// ============================================================================
// Tensor Modification Factors Structure
// 
// Stores regime-specific modifications to spectral tensor
// ============================================================================

struct TensorModificationFactors {
    // Scaling factors for tensor components
    amrex::Real u_variance_factor = amrex::Real(1.0);    // σ_u scaling
    amrex::Real v_variance_factor = amrex::Real(1.0);    // σ_v scaling
    amrex::Real w_variance_factor = amrex::Real(1.0);    // σ_w scaling
    
    // Length scale modifications
    amrex::Real u_length_scale_factor = amrex::Real(1.0);
    amrex::Real v_length_scale_factor = amrex::Real(1.0);
    amrex::Real w_length_scale_factor = amrex::Real(1.0);
    
    // Anisotropy modifications
    amrex::Real isotropy_factor = amrex::Real(1.0);      // Increase for separation
    amrex::Real coherence_reduction = amrex::Real(1.0);  // Reduce for separation
    
    // Constructor
    TensorModificationFactors() = default;
};

// ============================================================================
// Boundary Layer Classification Structure
// ============================================================================

struct BoundaryLayerProfile {
    amrex::Real boundary_layer_height;           // δ [m]
    amrex::Real surface_layer_height;            // ~0.1*δ [m]
    amrex::Real monin_obukhov_length;            // L_MO [m]
    BoundaryLayerRegion current_region;          // Surface layer, mixed layer, or free atm
    amrex::Real relative_height;                 // ζ = z/δ [–]
    
    // Constructor
    BoundaryLayerProfile()
        : boundary_layer_height(500.0), surface_layer_height(50.0),
          monin_obukhov_length(100.0), 
          current_region(BoundaryLayerRegion::MIXED_LAYER),
          relative_height(0.5) {}
};

// ============================================================================
// Namespace for flow regime operations
// ============================================================================

namespace MannBoxFlowRegime {

// ============================================================================
// PART 1: Flow Regime Classification
// ============================================================================

// Classify flow regime based on local flow properties
// Returns the detected regime type
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
FlowRegime classify_flow_regime(
    const FlowProperties& props,
    const RegimeThresholds& thresholds
) noexcept
{
    // Check for stagnation first (weakest flow)
    if (props.velocity_magnitude < thresholds.stagnation_velocity_threshold) {
        return FlowRegime::STAGNATION;
    }
    
    // Check for flow separation (high vorticity)
    if (props.vorticity_magnitude > thresholds.vorticity_separation_threshold) {
        return FlowRegime::SEPARATION;
    }
    
    // Check for acceleration zones (positive divergence)
    if (props.divergence > thresholds.divergence_acceleration_threshold) {
        return FlowRegime::ACCELERATION;
    }
    
    // Check for channeling flow (gentle slopes, high velocity alignment)
    if (props.terrain_slope > thresholds.channeling_slope_threshold &&
        props.terrain_slope < thresholds.steep_slope_threshold) {
        // Check if flow is channeled (high longitudinal component)
        amrex::Real horizontal_mag = std::sqrt(
            props.u_component * props.u_component + 
            props.v_component * props.v_component
        );
        if (horizontal_mag > thresholds.min_velocity_for_flow) {
            // Check alignment with expected channel direction
            amrex::Real u_ratio = std::abs(props.u_component) / 
                                  (horizontal_mag + amrex::Real(1e-10));
            if (u_ratio > amrex::Real(0.7)) {  // Strong alignment
                return FlowRegime::CHANNELING;
            }
        }
    }
    
    // Default to neutral regime
    return FlowRegime::NEUTRAL;
}

// Get string representation of flow regime
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
const char* get_regime_name(FlowRegime regime) noexcept
{
    switch (regime) {
        case FlowRegime::NEUTRAL:      return "Neutral";
        case FlowRegime::ACCELERATION: return "Acceleration";
        case FlowRegime::SEPARATION:   return "Separation";
        case FlowRegime::STAGNATION:   return "Stagnation";
        case FlowRegime::CHANNELING:   return "Channeling";
        default:                       return "Invalid";
    }
}

// ============================================================================
// PART 2: Tensor Modification Factors per Regime
// ============================================================================

// Compute tensor modification factors based on detected regime
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
TensorModificationFactors compute_regime_modification_factors(
    FlowRegime regime,
    const FlowProperties& props,
    amrex::Real height_fraction = amrex::Real(0.5)  // ζ = z/δ
) noexcept
{
    TensorModificationFactors factors;
    
    // Apply regime-specific modifications
    switch (regime) {
        
        // Acceleration zone: enhance streamwise, reduce vertical
        case FlowRegime::ACCELERATION: {
            factors.u_variance_factor = amrex::Real(1.4);      // +40%
            factors.v_variance_factor = amrex::Real(1.1);      // +10%
            factors.w_variance_factor = amrex::Real(0.7);      // -30%
            
            factors.u_length_scale_factor = amrex::Real(1.2);
            factors.v_length_scale_factor = amrex::Real(1.0);
            factors.w_length_scale_factor = amrex::Real(0.8);
            
            factors.isotropy_factor = amrex::Real(0.8);
            factors.coherence_reduction = amrex::Real(1.0);
            break;
        }
        
        // Separation zone: increase isotropy, reduce overall intensity
        case FlowRegime::SEPARATION: {
            factors.u_variance_factor = amrex::Real(0.6);      // -40%
            factors.v_variance_factor = amrex::Real(0.8);      // -20%
            factors.w_variance_factor = amrex::Real(1.0);      // Maintain
            
            factors.u_length_scale_factor = amrex::Real(0.7);
            factors.v_length_scale_factor = amrex::Real(0.7);
            factors.w_length_scale_factor = amrex::Real(0.9);
            
            factors.isotropy_factor = amrex::Real(1.3);        // More isotropic
            factors.coherence_reduction = amrex::Real(0.7);    // Reduced coherence
            break;
        }
        
        // Stagnation: very weak turbulence
        case FlowRegime::STAGNATION: {
            factors.u_variance_factor = amrex::Real(0.3);      // -70%
            factors.v_variance_factor = amrex::Real(0.3);      // -70%
            factors.w_variance_factor = amrex::Real(0.4);      // -60%
            
            factors.u_length_scale_factor = amrex::Real(0.5);
            factors.v_length_scale_factor = amrex::Real(0.5);
            factors.w_length_scale_factor = amrex::Real(0.6);
            
            factors.isotropy_factor = amrex::Real(1.5);        // Very isotropic
            factors.coherence_reduction = amrex::Real(0.4);    // Very weak coherence
            break;
        }
        
        // Channeling: enhanced streamwise, suppressed cross-wind
        case FlowRegime::CHANNELING: {
            factors.u_variance_factor = amrex::Real(1.3);      // +30%
            factors.v_variance_factor = amrex::Real(0.6);      // -40% (cross-wind)
            factors.w_variance_factor = amrex::Real(0.9);      // -10%
            
            factors.u_length_scale_factor = amrex::Real(1.5);  // Stretched along channel
            factors.v_length_scale_factor = amrex::Real(0.7);
            factors.w_length_scale_factor = amrex::Real(0.8);
            
            factors.isotropy_factor = amrex::Real(0.7);        // Less isotropic
            factors.coherence_reduction = amrex::Real(0.9);    // Good coherence
            break;
        }
        
        // Neutral: use default factors
        case FlowRegime::NEUTRAL:
        default: {
            // Keep default values (all 1.0)
            break;
        }
    }
    
    // Height-dependent modifications
    // Near surface: stronger modifications; in mixed layer: weaker
    amrex::Real height_factor = (height_fraction < amrex::Real(0.1)) 
        ? amrex::Real(1.2) * height_fraction / amrex::Real(0.1)
        : amrex::Real(1.0);
    
    // Apply height damping to variance factors
    factors.u_variance_factor = amrex::Real(1.0) + 
        (factors.u_variance_factor - amrex::Real(1.0)) * height_factor;
    factors.v_variance_factor = amrex::Real(1.0) + 
        (factors.v_variance_factor - amrex::Real(1.0)) * height_factor;
    factors.w_variance_factor = amrex::Real(1.0) + 
        (factors.w_variance_factor - amrex::Real(1.0)) * height_factor;
    
    return factors;
}

// ============================================================================
// PART 3: Boundary Layer Height and Classification
// ============================================================================

// Estimate boundary layer height from meteorological parameters
// Uses convective velocity scale (w_*) and friction velocity (u_*)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real estimate_boundary_layer_height(
    amrex::Real friction_velocity,         // u_* [m/s]
    amrex::Real convective_velocity,       // w_* [m/s]
    amrex::Real monin_obukhov_length,      // L_MO [m]
    amrex::Real wind_speed_ref = amrex::Real(10.0)  // Reference wind speed [m/s]
) noexcept
{
    // Guard against invalid inputs
    friction_velocity = std::max(friction_velocity, amrex::Real(0.01));
    convective_velocity = std::max(convective_velocity, amrex::Real(0.0));
    monin_obukhov_length = (std::abs(monin_obukhov_length) < amrex::Real(1e6)) 
        ? monin_obukhov_length 
        : amrex::Real(1e6);
    
    // Simple empirical relationship:
    // δ ≈ 0.2 * u_* * |L_MO| / (1 + w_* / u_*)
    // With minimum bounds for stability
    
    const amrex::Real min_BL_height = amrex::Real(200.0);  // Minimum 200 m
    const amrex::Real max_BL_height = amrex::Real(3000.0); // Maximum 3000 m
    
    amrex::Real abs_L_MO = std::abs(monin_obukhov_length);
    amrex::Real denominator = amrex::Real(1.0) + 
        (convective_velocity / (friction_velocity + amrex::Real(1e-8)));
    
    amrex::Real BL_height = amrex::Real(0.2) * friction_velocity * abs_L_MO / denominator;
    
    // Apply bounds
    BL_height = std::max(BL_height, min_BL_height);
    BL_height = std::min(BL_height, max_BL_height);
    
    return BL_height;
}

// Classify which boundary layer region point is in
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
BoundaryLayerRegion classify_bl_region(
    amrex::Real height_agl,                  // Height above ground [m]
    amrex::Real boundary_layer_height,       // Boundary layer depth δ [m]
    amrex::Real surface_layer_fraction = amrex::Real(0.1)  // Fraction of δ for surface layer
) noexcept
{
    const amrex::Real surface_layer_top = boundary_layer_height * surface_layer_fraction;
    
    if (height_agl < surface_layer_top) {
        return BoundaryLayerRegion::SURFACE_LAYER;
    } else if (height_agl < boundary_layer_height) {
        return BoundaryLayerRegion::MIXED_LAYER;
    } else {
        return BoundaryLayerRegion::FREE_ATMOSPHERE;
    }
}

// Get tensor modifications based on boundary layer region
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
TensorModificationFactors get_bl_region_modifications(
    BoundaryLayerRegion region,
    amrex::Real relative_height  // ζ = z/δ [–]
) noexcept
{
    TensorModificationFactors factors;
    
    switch (region) {
        
        // Surface layer: log-law profile, strong shear
        case BoundaryLayerRegion::SURFACE_LAYER: {
            // Height-dependent scaling: stronger at surface
            amrex::Real z_factor = std::max(relative_height, amrex::Real(0.01));
            
            factors.u_variance_factor = amrex::Real(1.0) + 
                amrex::Real(0.5) * (amrex::Real(1.0) - z_factor);
            factors.v_variance_factor = amrex::Real(0.8) + 
                amrex::Real(0.3) * (amrex::Real(1.0) - z_factor);
            factors.w_variance_factor = amrex::Real(0.5) + 
                amrex::Real(0.4) * (amrex::Real(1.0) - z_factor);
            
            factors.u_length_scale_factor = amrex::Real(0.5) * z_factor;
            factors.v_length_scale_factor = amrex::Real(0.4) * z_factor;
            factors.w_length_scale_factor = amrex::Real(0.2) * z_factor;
            
            factors.isotropy_factor = amrex::Real(0.6);
            factors.coherence_reduction = amrex::Real(1.0);
            break;
        }
        
        // Mixed layer: well-mixed, weaker shear
        case BoundaryLayerRegion::MIXED_LAYER: {
            // More uniform profile
            factors.u_variance_factor = amrex::Real(1.0);
            factors.v_variance_factor = amrex::Real(0.9);
            factors.w_variance_factor = amrex::Real(0.7);
            
            factors.u_length_scale_factor = relative_height * amrex::Real(0.5);
            factors.v_length_scale_factor = relative_height * amrex::Real(0.5);
            factors.w_length_scale_factor = relative_height * amrex::Real(0.4);
            
            factors.isotropy_factor = amrex::Real(0.9);
            factors.coherence_reduction = amrex::Real(0.95);
            break;
        }
        
        // Free atmosphere: weak turbulence
        case BoundaryLayerRegion::FREE_ATMOSPHERE: {
            factors.u_variance_factor = amrex::Real(0.3);
            factors.v_variance_factor = amrex::Real(0.3);
            factors.w_variance_factor = amrex::Real(0.2);
            
            factors.u_length_scale_factor = amrex::Real(0.3);
            factors.v_length_scale_factor = amrex::Real(0.3);
            factors.w_length_scale_factor = amrex::Real(0.2);
            
            factors.isotropy_factor = amrex::Real(1.2);
            factors.coherence_reduction = amrex::Real(0.7);
            break;
        }
        
        default:
            break;
    }
    
    return factors;
}

// ============================================================================
// PART 4: Combined Modifications (Regime + BL Height)
// ============================================================================

// Combine regime and BL region modifications into final tensor factors
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
TensorModificationFactors combine_modifications(
    const TensorModificationFactors& regime_factors,
    const TensorModificationFactors& bl_factors
) noexcept
{
    TensorModificationFactors combined;
    
    // Multiply factors (geometric combination)
    combined.u_variance_factor = regime_factors.u_variance_factor * 
                                 bl_factors.u_variance_factor;
    combined.v_variance_factor = regime_factors.v_variance_factor * 
                                 bl_factors.v_variance_factor;
    combined.w_variance_factor = regime_factors.w_variance_factor * 
                                 bl_factors.w_variance_factor;
    
    combined.u_length_scale_factor = regime_factors.u_length_scale_factor * 
                                     bl_factors.u_length_scale_factor;
    combined.v_length_scale_factor = regime_factors.v_length_scale_factor * 
                                     bl_factors.v_length_scale_factor;
    combined.w_length_scale_factor = regime_factors.w_length_scale_factor * 
                                     bl_factors.w_length_scale_factor;
    
    // For isotropy and coherence, use average of factors
    combined.isotropy_factor = amrex::Real(0.5) * (regime_factors.isotropy_factor + 
                                                     bl_factors.isotropy_factor);
    combined.coherence_reduction = amrex::Real(0.5) * (regime_factors.coherence_reduction + 
                                                        bl_factors.coherence_reduction);
    
    return combined;
}

// ============================================================================
// PART 5: Utility Functions
// ============================================================================

// Compute terrain slope from elevation gradient
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_terrain_slope(
    amrex::Real dz_dx,  // Terrain elevation gradient in x-direction [–]
    amrex::Real dz_dy   // Terrain elevation gradient in y-direction [–]
) noexcept
{
    // Slope magnitude: slope = sqrt((dz/dx)² + (dz/dy)²)
    return std::sqrt(dz_dx * dz_dx + dz_dy * dz_dy);
}

// Compute velocity magnitude
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_velocity_magnitude(
    amrex::Real u,  // u-component [m/s]
    amrex::Real v,  // v-component [m/s]
    amrex::Real w   // w-component [m/s]
) noexcept
{
    return std::sqrt(u * u + v * v + w * w);
}

// Clamp tensor modification factors to physical bounds
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
TensorModificationFactors clamp_modification_factors(
    const TensorModificationFactors& factors,
    amrex::Real min_factor = amrex::Real(0.1),
    amrex::Real max_factor = amrex::Real(3.0)
) noexcept
{
    TensorModificationFactors clamped;
    
    clamped.u_variance_factor = std::max(min_factor, 
                                         std::min(max_factor, factors.u_variance_factor));
    clamped.v_variance_factor = std::max(min_factor, 
                                         std::min(max_factor, factors.v_variance_factor));
    clamped.w_variance_factor = std::max(min_factor, 
                                         std::min(max_factor, factors.w_variance_factor));
    
    clamped.u_length_scale_factor = std::max(min_factor, 
                                             std::min(max_factor, factors.u_length_scale_factor));
    clamped.v_length_scale_factor = std::max(min_factor, 
                                             std::min(max_factor, factors.v_length_scale_factor));
    clamped.w_length_scale_factor = std::max(min_factor, 
                                             std::min(max_factor, factors.w_length_scale_factor));
    
    clamped.isotropy_factor = std::max(amrex::Real(0.5), 
                                       std::min(amrex::Real(2.0), factors.isotropy_factor));
    clamped.coherence_reduction = std::max(amrex::Real(0.3), 
                                           std::min(amrex::Real(1.2), factors.coherence_reduction));
    
    return clamped;
}

} // namespace MannBoxFlowRegime

#endif // MANN_BOX_FLOW_REGIME_DETECTOR_H
