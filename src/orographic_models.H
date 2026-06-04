// ============================================================================
// orographic_models.H
// Orographic speed-up and flow separation models for terrain effects
//
// This header provides functions for computing wind speed modifications
// due to terrain features (ridges, hills, valleys) based on empirical
// models from the literature.
//
// Models implemented:
// 1. Jackson & Hunt (1975) - Hill speed-up for 2D ridges and 3D hills
// 2. WindNinja-inspired empirical speedup factors
//
// References:
// - Jackson, P.S., & Hunt, J.C.R. (1975). Turbulent wind flow over a low 
//   hill. Quarterly Journal of the Royal Meteorological Society, 101(430), 
//   929-955.
// - Winstral, A., Marks, D., & Gurney, R. (2013). Simulating wind fields 
//   and snow redistribution using terrain-based parameters to model snow 
//   accumulation and melt over a semi-arid mountain catchment. Hydrological 
//   Processes, 27(26), 3973-3998.
// - Forthofer, J. M., Butler, B. W., & Wagenbrenner, N. S. (2014). A 
//   comparison of three approaches for simulating fine-scale surface winds 
//   in support of wildland fire management. Part I. Model formulation and 
//   comparison against measurements. International Journal of Wildland Fire, 
//   23(7), 969-981.
// ============================================================================

#ifndef OROGRAPHIC_MODELS_H
#define OROGRAPHIC_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <algorithm>
#include <cmath>

// ============================================================================
// Orographic parameters structure
// ============================================================================
struct OrographicParams {
    bool enabled = false;                        // Enable orographic speedup
    amrex::Real hill_length_scale = 100.0;       // Characteristic hill half-length L [m]
    amrex::Real speedup_factor_max = 2.0;        // Maximum speedup factor on ridges (dimensionless)
    amrex::Real separation_factor = 0.3;         // Flow separation strength factor (dimensionless)
    amrex::Real smoothing_factor = 0.5;          // Terrain feature smoothing factor (0-1)
};

// Physical constants for orographic model
namespace OrographicConstants {
    // Flow separation typically occurs for slopes > 0.3 (approximately 17 degrees)
    // Based on Jackson & Hunt (1975) and empirical observations of lee-side separation
    constexpr amrex::Real FLOW_SEPARATION_SLOPE_THRESHOLD = amrex::Real(0.3);
    
    // Minimum speedup multiplier to prevent complete flow reversal
    // Wind speed is clamped to 30% of base value in strong separation zones
    constexpr amrex::Real MIN_SEPARATION_SPEEDUP = amrex::Real(0.3);
}

// ============================================================================
// Compute terrain slope magnitude from neighboring elevations
//
// Uses central differences to compute slope in x and y directions, then
// combines to get total slope magnitude.
//
// Inputs:
//   z_xm, z_xp     - Terrain elevations at x-minus and x-plus neighbors [m]
//   z_ym, z_yp     - Terrain elevations at y-minus and y-plus neighbors [m]
//   dx, dy         - Grid spacings [m]
//
// Returns:
//   Slope magnitude (dimensionless, rise/run)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_terrain_slope(
    amrex::Real z_xm, amrex::Real z_xp,
    amrex::Real z_ym, amrex::Real z_yp,
    amrex::Real dx, amrex::Real dy)
{
    // Central difference slopes
    const amrex::Real dz_dx = (z_xp - z_xm) / (amrex::Real(2.0) * dx);
    const amrex::Real dz_dy = (z_yp - z_ym) / (amrex::Real(2.0) * dy);
    
    // Total slope magnitude
    return std::sqrt(dz_dx * dz_dx + dz_dy * dz_dy);
}

// ============================================================================
// Compute terrain curvature from neighboring elevations
//
// Uses second derivatives to compute curvature. Positive curvature indicates
// a convex feature (ridge/hill top), negative indicates concave (valley).
//
// Inputs:
//   z_center       - Terrain elevation at center point [m]
//   z_xm, z_xp     - Terrain elevations at x-minus and x-plus neighbors [m]
//   z_ym, z_yp     - Terrain elevations at y-minus and y-plus neighbors [m]
//   dx, dy         - Grid spacings [m]
//
// Returns:
//   Curvature (1/m) - positive for convex (ridges), negative for concave (valleys)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_terrain_curvature(
    amrex::Real z_center,
    amrex::Real z_xm, amrex::Real z_xp,
    amrex::Real z_ym, amrex::Real z_yp,
    amrex::Real dx, amrex::Real dy)
{
    // Second derivatives (curvature in x and y)
    const amrex::Real d2z_dx2 = (z_xp - amrex::Real(2.0) * z_center + z_xm) / (dx * dx);
    const amrex::Real d2z_dy2 = (z_yp - amrex::Real(2.0) * z_center + z_ym) / (dy * dy);
    
    // Average curvature (Laplacian / 2)
    return amrex::Real(0.5) * (d2z_dx2 + d2z_dy2);
}

// ============================================================================
// Jackson & Hunt (1975) hill speed-up factor
//
// Computes the fractional speedup (ΔS) of wind over a hill or ridge based on
// linear theory for flow over low hills. The theory assumes:
// - Hill height H << boundary layer height
// - Slope H/L << 1 (shallow hill)
// - Neutral atmospheric stability
//
// The speedup at height z above the hill crest is:
//   ΔS(z) = (H/L) × S_factor × exp(-z/L_z)
//
// where:
//   H = hill height above surrounding terrain
//   L = hill half-length (characteristic horizontal scale)
//   S_factor = empirical speedup coefficient (typically 1.5-2.0 for ridges)
//   L_z = vertical decay length scale (typically L/2)
//
// For flow separation on lee slopes (negative curvature), a reduction
// factor is applied instead of speedup.
//
// Inputs:
//   slope          - Terrain slope magnitude (dimensionless)
//   curvature      - Terrain curvature [1/m] (positive=convex, negative=concave)
//   z_agl          - Height above ground level [m]
//   params         - Orographic parameters structure
//
// Returns:
//   Speed multiplier (dimensionless, typically 0.5-2.0)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real jackson_hunt_speedup_factor(
    amrex::Real slope,
    amrex::Real curvature,
    amrex::Real z_agl,
    const OrographicParams& params)
{
    if (!params.enabled) {
        return amrex::Real(1.0);  // No speedup
    }
    
    // Hill characteristic length scale
    const amrex::Real L = params.hill_length_scale;
    
    // Vertical decay length scale (typically L/2 for neutral stability)
    const amrex::Real L_z = L * amrex::Real(0.5);
    
    // Vertical decay factor: exp(-z/L_z)
    // Near ground: full speedup, at height >> L_z: negligible speedup
    const amrex::Real z_decay = (L_z > amrex::Real(1.0e-10)) 
        ? std::exp(-z_agl / L_z) 
        : amrex::Real(0.0);
    
    // Jackson & Hunt base speedup: proportional to slope (H/L)
    // For low hills with slope s, the speedup is approximately:
    //   ΔS ≈ S_factor × slope
    // where S_factor is an empirical coefficient (1.5-2.0 for ridges)
    
    amrex::Real speedup_mult = amrex::Real(1.0);
    
    if (curvature > amrex::Real(1.0e-6)) {
        // Positive curvature = convex feature (ridge, hill crest)
        // Apply speedup proportional to slope and curvature
        
        // Curvature strength indicator (dimensionless)
        // Normalize curvature (units: 1/m) by L² to create dimensionless measure
        // This makes the strength independent of the choice of length scale units
        const amrex::Real curv_strength = std::min(
            std::abs(curvature) * L * L,  // Normalize: curvature × L² is dimensionless
            amrex::Real(1.0)
        );
        
        // Speedup factor: 1 + ΔS
        // ΔS = S_max × slope × curvature_strength × vertical_decay
        const amrex::Real delta_S = (params.speedup_factor_max - amrex::Real(1.0)) 
                                   * slope 
                                   * curv_strength 
                                   * z_decay
                                   * params.smoothing_factor;
        
        speedup_mult = amrex::Real(1.0) + delta_S;
        
        // Clamp to reasonable range [1.0, speedup_factor_max]
        speedup_mult = std::max(amrex::Real(1.0), 
                                std::min(speedup_mult, params.speedup_factor_max));
        
    } else if (curvature < -amrex::Real(1.0e-6)) {
        // Negative curvature = concave feature (valley, lee slope)
        // Apply flow separation / deceleration
        
        // Curvature strength indicator (dimensionless, normalized by L²)
        const amrex::Real curv_strength = std::min(
            std::abs(curvature) * L * L,
            amrex::Real(1.0)
        );
        
        // Separation factor: reduces wind speed on lee slopes
        // Flow separation typically occurs for slopes > FLOW_SEPARATION_SLOPE_THRESHOLD
        const amrex::Real separation_strength = 
            (slope > OrographicConstants::FLOW_SEPARATION_SLOPE_THRESHOLD) 
            ? params.separation_factor 
            : params.separation_factor * slope / OrographicConstants::FLOW_SEPARATION_SLOPE_THRESHOLD;
        
        // Slowdown factor: 1 - ΔS (but keep positive)
        const amrex::Real delta_S = separation_strength 
                                   * curv_strength 
                                   * z_decay
                                   * params.smoothing_factor;
        
        speedup_mult = amrex::Real(1.0) - delta_S;
        
        // Clamp to reasonable range [MIN_SEPARATION_SPEEDUP, 1.0]
        // Prevent complete flow reversal - maintain minimum 30% of base speed
        constexpr amrex::Real min_separation_speedup = amrex::Real(0.3);
        speedup_mult = std::max(min_separation_speedup,
                                std::min(speedup_mult, amrex::Real(1.0)));
    }
    // else: near-zero curvature (flat or uniform slope) → no speedup/slowdown
    
    return speedup_mult;
}

// ============================================================================
// Apply orographic speedup to wind velocity components
//
// Multiplies horizontal wind components (u, v) by the speedup factor.
// Vertical component (w) is not modified by this simple model.
// More sophisticated models would adjust w based on flow deflection.
//
// Inputs:
//   u, v           - Horizontal velocity components [m/s] (modified in place)
//   speedup_factor - Speed multiplier from jackson_hunt_speedup_factor
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_orographic_speedup(
    amrex::Real& u,
    amrex::Real& v,
    amrex::Real speedup_factor)
{
    u *= speedup_factor;
    v *= speedup_factor;
}

// ============================================================================
// Terrain-Adaptive Alpha Coefficients
//
// Computes spatially-varying anisotropy coefficients (α_h, α_v) based on
// local terrain characteristics to improve mass-consistent wind field
// adjustment over complex terrain.
//
// Strategy:
//   - Flat areas: isotropic (α_h = α_v = 1.0) - equal adjustment in all directions
//   - Steep slopes: preserve terrain-following (α_v << α_h) - minimal vertical adjustment
//   - Valley bottoms: allow more vertical adjustment (larger α_v)
//   - Ridge tops: constrain vertical motion (smaller α_v)
//
// The vertical coefficient α_v is a function of slope magnitude:
//   α_v = α_v_flat * exp(-slope / slope_scale)
//
// where:
//   α_v_flat: alpha_v for flat terrain (typically 1.0)
//   slope_scale: characteristic slope for decay (typically 0.2-0.3)
//   
// For steep slopes (slope > 0.3):
//   - α_v becomes small (< 0.3)
//   - Wind adjustment preserves terrain-following structure
//
// For flat terrain (slope < 0.05):
//   - α_v ≈ α_v_flat
//   - Wind adjustment is nearly isotropic
//
// Parameters:
//   slope          - Terrain slope magnitude (dimensionless, rise/run)
//   curvature      - Terrain curvature [1/m] (positive=ridge, negative=valley)
//   elevation      - Local terrain elevation [m]
//   alpha_h_base   - Base horizontal coefficient (typically 1.0)
//   alpha_v_flat   - Vertical coefficient for flat terrain (typically 1.0)
//   slope_scale    - Slope decay parameter (typically 0.2-0.3)
//
// Returns:
//   Pair (α_h, α_v) for this location
//
// References:
//   - Ross, D.G., et al. (1988). Diagnostic wind field modeling for complex terrain.
//     J. Appl. Meteor., 27, 785-796.
//   - Sherman, C.A. (1978). A mass-consistent model for wind fields over complex terrain.
//     J. Appl. Meteor., 17, 312-319.
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
std::pair<amrex::Real, amrex::Real> terrain_adaptive_alpha_coefficients(
    amrex::Real slope,
    amrex::Real curvature,
    amrex::Real elevation,
    amrex::Real alpha_h_base = amrex::Real(1.0),
    amrex::Real alpha_v_flat = amrex::Real(1.0),
    amrex::Real slope_scale = amrex::Real(0.25))
{
    (void)elevation;
    // Horizontal coefficient: typically constant
    amrex::Real alpha_h = alpha_h_base;
    
    // Vertical coefficient: depends on terrain slope
    // For steep slopes: α_v << 1 (preserve terrain-following)
    // For flat terrain: α_v ≈ α_v_flat (allow vertical adjustment)
    
    // Exponential decay with slope: α_v = α_v_flat × exp(-slope / slope_scale)
    amrex::Real alpha_v = alpha_v_flat * std::exp(-slope / slope_scale);
    
    // Apply curvature modulation:
    // - Ridge tops (positive curvature): reduce α_v further (constrain vertical motion)
    // - Valley bottoms (negative curvature): increase α_v (allow more vertical adjustment)
    
    if (std::abs(curvature) > amrex::Real(1.0e-6)) {
        // Curvature strength indicator (dimensionless)
        // Normalize by characteristic length scale L = 100 m
        const amrex::Real L_char = amrex::Real(100.0);  // meters
        const amrex::Real curv_strength = std::min(
            std::abs(curvature) * L_char * L_char,
            amrex::Real(1.0)
        );
        
        if (curvature > amrex::Real(0.0)) {
            // Ridge top (positive curvature): reduce α_v by up to 30%
            const amrex::Real ridge_factor = amrex::Real(1.0) - amrex::Real(0.3) * curv_strength;
            alpha_v *= ridge_factor;
        } else {
            // Valley bottom (negative curvature): increase α_v by up to 50%
            const amrex::Real valley_factor = amrex::Real(1.0) + amrex::Real(0.5) * curv_strength;
            alpha_v *= valley_factor;
            
            // Clamp to reasonable maximum
            alpha_v = std::min(alpha_v, amrex::Real(1.5) * alpha_v_flat);
        }
    }
    
    // Clamp α_v to physically reasonable range
    // Minimum: 0.1 × α_v_flat (very steep terrain, strong terrain-following)
    // Maximum: 1.5 × α_v_flat (valley bottoms, enhanced vertical adjustment)
    const amrex::Real alpha_v_min = amrex::Real(0.1) * alpha_v_flat;
    const amrex::Real alpha_v_max = amrex::Real(1.5) * alpha_v_flat;
    alpha_v = std::max(alpha_v_min, std::min(alpha_v, alpha_v_max));
    
    return {alpha_h, alpha_v};
}

// ============================================================================
// Simplified terrain-adaptive alpha (slope-only version)
//
// Computes α_v based solely on terrain slope, without curvature effects.
// Simpler interface for basic terrain adaptation.
//
// Parameters:
//   slope          - Terrain slope magnitude (dimensionless)
//   alpha_v_flat   - Vertical coefficient for flat terrain (default 1.0)
//   slope_scale    - Slope decay parameter (default 0.25)
//
// Returns:
//   α_v for this slope
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real terrain_adaptive_alpha_v(
    amrex::Real slope,
    amrex::Real alpha_v_flat = amrex::Real(1.0),
    amrex::Real slope_scale = amrex::Real(0.25))
{
    // Exponential decay with slope
    amrex::Real alpha_v = alpha_v_flat * std::exp(-slope / slope_scale);
    
    // Clamp to reasonable range
    const amrex::Real alpha_v_min = amrex::Real(0.1) * alpha_v_flat;
    alpha_v = std::max(alpha_v_min, std::min(alpha_v, alpha_v_flat));
    
    return alpha_v;
}

// ============================================================================
// Orographic Model Activation Thresholds
//
// Jackson-Hunt model is only activated when both conditions are met:
// 1. Froude number Fr > 0.1 (sufficient wind to overcome terrain blocking)
// 2. Terrain slope > 5% (0.05) (steep enough to generate significant speedup)
//
// These thresholds prevent model application in regimes where it's inappropriate:
// - Low Fr (Fr < 0.1): Strong stratification blocks flow over terrain
// - Low slope: Minimal terrain effects, speedup negligible
//
// References:
// - Jackson, P.S., & Hunt, J.C.R. (1975). Turbulent wind flow over a low hill.
//   Quarterly Journal of the Royal Meteorological Society, 101(430), 929-955.
// - Grubisic, V. (2004). The Morning Glory of the Gulf of Carpentaria.
//   Monthly Weather Review, 132(12), 2830-2841.
// ============================================================================

// Compute Froude number for orographic flow regime determination
//
// The Froude number characterizes the ratio of inertial forces (wind speed)
// to gravitational/buoyancy forces (stratification):
//   Fr = U / (N * H)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_orographic_froude_number(
    amrex::Real wind_speed,
    amrex::Real brunt_vaisala_freq,
    amrex::Real terrain_height) noexcept
{
    // Ensure valid inputs
    const amrex::Real min_wind_speed = amrex::Real(0.1);
    const amrex::Real min_height = amrex::Real(1.0);
    const amrex::Real min_frequency = amrex::Real(0.001);
    
    wind_speed = std::max(wind_speed, min_wind_speed);
    terrain_height = std::max(terrain_height, min_height);
    brunt_vaisala_freq = std::max(brunt_vaisala_freq, min_frequency);
    
    // Fr = U / (N * H)
    return wind_speed / (brunt_vaisala_freq * terrain_height);
}

// Check if Jackson-Hunt orographic model should be activated
//
// Model is only applied when BOTH conditions are satisfied:
// 1. Fr > Fr_min (typically 0.1): Sufficient wind relative to stratification
// 2. slope > slope_min (typically 0.05 = 5%): Steep enough terrain
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
bool is_orographic_model_active(
    amrex::Real froude_number,
    amrex::Real slope,
    amrex::Real froude_threshold = amrex::Real(0.1),
    amrex::Real slope_threshold = amrex::Real(0.05)) noexcept
{
    // Both conditions must be satisfied for model activation
    const bool fr_condition = (froude_number > froude_threshold);
    const bool slope_condition = (slope > slope_threshold);
    
    return (fr_condition && slope_condition);
}

// Jackson-Hunt speedup factor with adaptive activation thresholds
//
// Checks Froude number and slope thresholds before applying orographic
// speedup. Prevents inappropriate application in low-wind or low-slope
// regimes where the model is not valid.
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real jackson_hunt_speedup_factor_adaptive(
    amrex::Real slope,
    amrex::Real curvature,
    amrex::Real z_agl,
    amrex::Real froude_number,
    const OrographicParams& params)
{
    // Check activation thresholds: Fr > 0.1 AND slope > 5%
    constexpr amrex::Real froude_threshold = amrex::Real(0.1);
    constexpr amrex::Real slope_threshold = amrex::Real(0.05);
    
    if (!is_orographic_model_active(froude_number, slope, froude_threshold, slope_threshold)) {
        return amrex::Real(1.0);  // Model inactive, no speedup
    }
    
    // If thresholds are satisfied, apply standard Jackson-Hunt model
    return jackson_hunt_speedup_factor(slope, curvature, z_agl, params);
}

#endif // OROGRAPHIC_MODELS_H
