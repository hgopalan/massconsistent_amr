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

namespace {
    using namespace amrex;
}

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

// ============================================================================
// Compute terrain slope magnitude from neighboring elevations
//
// Uses central differences to compute slope in x and y directions, then
// combines to get total slope magnitude.
//
// Inputs:
//   z_center       - Terrain elevation at center point [m]
//   z_xm, z_xp     - Terrain elevations at x-minus and x-plus neighbors [m]
//   z_ym, z_yp     - Terrain elevations at y-minus and y-plus neighbors [m]
//   dx, dy         - Grid spacings [m]
//
// Returns:
//   Slope magnitude (dimensionless, rise/run)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_terrain_slope(
    amrex::Real z_center,
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
        const amrex::Real curv_strength = std::min(
            std::abs(curvature) * L * L,  // Normalize by length scale
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
        
        // Curvature strength indicator
        const amrex::Real curv_strength = std::min(
            std::abs(curvature) * L * L,
            amrex::Real(1.0)
        );
        
        // Separation factor: reduces wind speed on lee slopes
        // Flow separation typically occurs for slopes > 0.3 (17 degrees)
        const amrex::Real separation_strength = (slope > amrex::Real(0.3)) 
            ? params.separation_factor 
            : params.separation_factor * slope / amrex::Real(0.3);
        
        // Slowdown factor: 1 - ΔS (but keep positive)
        const amrex::Real delta_S = separation_strength 
                                   * curv_strength 
                                   * z_decay
                                   * params.smoothing_factor;
        
        speedup_mult = amrex::Real(1.0) - delta_S;
        
        // Clamp to reasonable range [0.3, 1.0] (don't allow reversal)
        speedup_mult = std::max(amrex::Real(0.3), 
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

#endif // OROGRAPHIC_MODELS_H
