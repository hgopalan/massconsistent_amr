// ============================================================================
// richardson_number_models.H
// Richardson number and boundary layer depth diagnostic
//
// This header provides functions for computing the Richardson number (Ri)
// to diagnose boundary layer depth. The Richardson number characterizes
// the ratio of stratification to shear-driven turbulence.
//
// Ri = g * dθ/dz / (θ * (du²+dv²))
//
// where:
//   g = gravitational acceleration [m/s²]
//   dθ/dz = virtual potential temperature gradient [K/m]
//   θ = virtual potential temperature [K]
//   du/dz, dv/dz = wind shear [1/s]
//
// The boundary layer depth H_mix is found where Ri = Ri_critical (typically 0.25)
//
// References:
// - Troen, I., & Mahrt, L. (1986). A simple model of the atmospheric boundary layer.
// - Stull, R.B. (1988). An Introduction to Boundary Layer Meteorology.
// ============================================================================

#ifndef RICHARDSON_NUMBER_MODELS_H
#define RICHARDSON_NUMBER_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <algorithm>

// ============================================================================
// Richardson number parameters structure
// ============================================================================
struct RichardsonNumberParams {
    bool enabled = false;                          // Enable Richardson number diagnostics
    amrex::Real richardson_critical = 0.25;        // Critical Richardson number (typically 0.25)
    amrex::Real min_wind_shear = 0.001;            // Minimum wind shear to avoid division by zero [1/s]
    amrex::Real reference_potential_temp = 300.0;  // Reference potential temperature [K]
    amrex::Real min_potential_temp_gradient = 0.0001;  // Minimum temperature gradient [K/m]
};

// Physical constants for Richardson number
namespace RichardsonNumberConstants {
    // Gravitational acceleration [m/s²]
    constexpr amrex::Real g = amrex::Real(9.81);
    
    // Default critical Richardson number (boundary layer top)
    constexpr amrex::Real RICHARDSON_CRITICAL = amrex::Real(0.25);
    
    // Minimum BL depth [m] to avoid unrealistic values
    constexpr amrex::Real MIN_BL_DEPTH = amrex::Real(50.0);
    
    // Maximum BL depth [m] to avoid unrealistic values
    constexpr amrex::Real MAX_BL_DEPTH = amrex::Real(5000.0);
}

// ============================================================================
// Compute potential temperature gradient
//
// From vertical temperature profile, compute dθ/dz (potential temperature gradient)
// For simplicity, assumes dry atmosphere where θ ≈ T (Exner function ≈ 1 near surface)
//
// Inputs:
//   theta_below   - Potential temperature at level z [K]
//   theta_above   - Potential temperature at level z + dz [K]
//   dz            - Vertical distance between levels [m]
//
// Returns:
//   Potential temperature gradient dθ/dz [K/m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_potential_temp_gradient(
    amrex::Real theta_below,
    amrex::Real theta_above,
    amrex::Real dz)
{
    if (dz < amrex::Real(1.0e-10)) {
        return amrex::Real(0.0);
    }
    
    // dθ/dz = (θ(z+dz) - θ(z)) / dz
    return (theta_above - theta_below) / dz;
}

// ============================================================================
// Compute wind shear magnitude
//
// Calculate the magnitude of horizontal wind shear from velocity gradients
// Shear² = (du/dz)² + (dv/dz)²
//
// Inputs:
//   du_dz         - Horizontal wind u gradient with respect to height [1/s]
//   dv_dz         - Horizontal wind v gradient with respect to height [1/s]
//
// Returns:
//   Wind shear magnitude [1/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_wind_shear_magnitude(
    amrex::Real du_dz,
    amrex::Real dv_dz)
{
    // Shear magnitude = sqrt((du/dz)² + (dv/dz)²)
    const amrex::Real shear_squared = du_dz * du_dz + dv_dz * dv_dz;
    
    if (shear_squared > amrex::Real(0.0)) {
        return std::sqrt(shear_squared);
    } else {
        return amrex::Real(0.0);
    }
}

// ============================================================================
// Compute Richardson number (Ri)
//
// Gradient Richardson number:
//   Ri = (g/θ) * (dθ/dz) / [(du/dz)² + (dv/dz)²]
//
// Positive Ri (stable stratification) indicates strong suppression of turbulence
// Ri > Ri_c → transition to laminar flow (stratification dominates)
// Ri < Ri_c → turbulent boundary layer (shear dominates)
//
// Inputs:
//   potential_temp      - Potential temperature [K]
//   dtheta_dz           - Vertical potential temperature gradient [K/m]
//   shear_magnitude     - Wind shear magnitude sqrt((du/dz)² + (dv/dz)²) [1/s]
//   min_wind_shear      - Minimum shear to avoid division by zero [1/s]
//
// Returns:
//   Richardson number (dimensionless)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_richardson_number(
    amrex::Real potential_temp,
    amrex::Real dtheta_dz,
    amrex::Real shear_magnitude,
    amrex::Real min_wind_shear)
{
    // Ensure valid input
    if (potential_temp < amrex::Real(1.0)) {
        potential_temp = amrex::Real(300.0);  // Default to 300 K
    }
    
    // Use minimum shear to prevent division by zero
    shear_magnitude = std::max(shear_magnitude, min_wind_shear);
    
    // Ri = (g/θ) * (dθ/dz) / Shear²
    const amrex::Real shear_squared = shear_magnitude * shear_magnitude;
    const amrex::Real ri = (RichardsonNumberConstants::g / potential_temp) 
                          * (dtheta_dz / shear_squared);
    
    // Limit to reasonable values (-2 to 10 is typical range)
    const amrex::Real ri_min = amrex::Real(-2.0);
    const amrex::Real ri_max = amrex::Real(10.0);
    
    return std::max(ri_min, std::min(ri, ri_max));
}

// ============================================================================
// Diagnose boundary layer depth from Richardson number profile
//
// Finds the height where the Richardson number first exceeds the critical value
// Ri_c (typically 0.25). This is the transition to the free atmosphere.
//
// Inputs:
//   ri_profile    - Array of Richardson number values from surface up [dimensionless]
//   nz            - Number of levels in the profile
//   z_array       - Array of heights corresponding to ri_profile [m]
//   ri_critical   - Critical Richardson number (typically 0.25) [dimensionless]
//
// Returns:
//   Boundary layer depth H_mix [m] (or MAX_BL_DEPTH if not found)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real diagnose_bl_depth(
    const amrex::Real* AMREX_RESTRICT ri_profile,
    int nz,
    amrex::Real dz,
    amrex::Real ri_critical)
{
    // Search for first height where Ri > Ri_c
    // Start from surface (k=0) and go up
    for (int k = 0; k < nz - 1; ++k) {
        if (ri_profile[k] > ri_critical) {
            // Found the transition level
            // Interpolate to find exact height
            const amrex::Real z_k = amrex::Real(k) * dz;
            const amrex::Real z_kp1 = amrex::Real(k + 1) * dz;
            const amrex::Real ri_k = ri_profile[k];
            const amrex::Real ri_kp1 = ri_profile[k + 1];
            
            // Linear interpolation
            if (std::abs(ri_kp1 - ri_k) > amrex::Real(1.0e-10)) {
                const amrex::Real frac = (ri_critical - ri_k) / (ri_kp1 - ri_k);
                const amrex::Real h_mix = z_k + frac * (z_kp1 - z_k);
                
                // Clamp to reasonable values
                return std::max(RichardsonNumberConstants::MIN_BL_DEPTH,
                               std::min(h_mix, RichardsonNumberConstants::MAX_BL_DEPTH));
            }
        }
    }
    
    // If Ri never exceeds critical value, BL extends to top of domain
    return RichardsonNumberConstants::MAX_BL_DEPTH;
}

#endif // RICHARDSON_NUMBER_MODELS_H
