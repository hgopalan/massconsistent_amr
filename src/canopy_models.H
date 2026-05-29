// ============================================================================
// canopy_models.H
// Canopy parameterization models for vegetation effects on wind profiles
//
// This header provides functions for computing canopy-modified wind profiles
// based on empirical models from the literature.
//
// Models implemented:
// 1. MacDonald et al. (2000) - Effective roughness and displacement height
// 2. Shaw & Pereira (1982) - Exponential decay within canopy
//
// References:
// - MacDonald, R.W., Griffiths, R.F., Hall, D.J. (2000). A comparison of 
//   results from scaled field and wind tunnel modelling of dispersion in 
//   arrays of obstacles. Atmospheric Environment, 34(20), 3845-3862.
// - Shaw, R.H., Pereira, A.R. (1982). Aerodynamic roughness of a plant 
//   canopy: A numerical experiment. Agricultural Meteorology, 26, 51-65.
// - Cionco, R.M. (1965). A mathematical model for air flow in a vegetative 
//   canopy. Journal of Applied Meteorology, 4, 517-522.
// ============================================================================

#ifndef CANOPY_MODELS_H
#define CANOPY_MODELS_H

#include <AMReX.H>
#include <cmath>

// ============================================================================
// Canopy parameters structure
// ============================================================================
struct CanopyParams {
    bool enabled = false;                    // Enable canopy model
    amrex::Real height = 0.0;                // Canopy height [m]
    amrex::Real frontal_area_index = 0.0;    // Frontal area index λf (dimensionless)
    amrex::Real plan_area_index = 0.0;       // Plan area index λp (dimensionless)
    amrex::Real drag_coefficient = 0.2;      // Drag coefficient Cd (dimensionless)
    amrex::Real attenuation_coeff = 2.5;     // Exponential attenuation coefficient α
    bool use_exponential_profile = false;    // Use Shaw-Pereira exponential profile
};

// ============================================================================
// MacDonald et al. (2000) displacement height and roughness length
//
// Computes effective displacement height d and roughness length z0_eff
// based on canopy morphology parameters.
//
// Inputs:
//   canopy_height  - Height of canopy elements [m]
//   lambda_f       - Frontal area index (frontal area / ground area)
//   lambda_p       - Plan area index (plan area / ground area)
//   Cd             - Drag coefficient (typically 0.2-0.3)
//   z0_base        - Base roughness length for ground surface [m]
//
// Outputs:
//   d              - Displacement height [m]
//   z0_eff         - Effective roughness length [m]
//
// MacDonald formulas:
//   d/H = 1 + α^(-λp) * (λp - 1)   where α = 4.43
//   z0/H = (1 - d/H) * exp(-[0.5 * β * Cd * (1 - d/H) * λf]^(-0.5))
//   where β = 1.0
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void macdonald_canopy_parameters(
    amrex::Real canopy_height,
    amrex::Real lambda_f,
    amrex::Real lambda_p,
    amrex::Real Cd,
    amrex::Real z0_base,
    amrex::Real& d,
    amrex::Real& z0_eff)
{
    using amrex::Real;
    
    const Real H = canopy_height;
    
    // Handle zero or very sparse canopy
    if (H <= Real(0.0) || lambda_f <= Real(1.0e-10) || lambda_p <= Real(1.0e-10)) {
        d = Real(0.0);
        z0_eff = z0_base;
        return;
    }
    
    // MacDonald constants
    const Real alpha_mac = Real(4.43);
    const Real beta_mac = Real(1.0);
    
    // Displacement height: d/H = 1 + α^(-λp) * (λp - 1)
    Real d_over_H = Real(1.0) + std::pow(alpha_mac, -lambda_p) * (lambda_p - Real(1.0));
    
    // Clamp d/H to reasonable range [0, 0.9]
    d_over_H = amrex::max(Real(0.0), amrex::min(d_over_H, Real(0.9)));
    
    d = d_over_H * H;
    
    // Roughness length: z0/H = (1 - d/H) * exp(-sqrt(1/(0.5*β*Cd*(1-d/H)*λf)))
    const Real one_minus_d_H = Real(1.0) - d_over_H;
    
    if (one_minus_d_H > Real(1.0e-10)) {
        // Compute argument for exponential
        const Real denom = Real(0.5) * beta_mac * Cd * one_minus_d_H * lambda_f;
        
        if (denom > Real(1.0e-10)) {
            const Real exponent = -std::sqrt(Real(1.0) / denom);
            Real z0_over_H = one_minus_d_H * std::exp(exponent);
            
            // Clamp z0/H to reasonable range [0.001, 0.5]
            z0_over_H = amrex::max(Real(0.001), amrex::min(z0_over_H, Real(0.5)));
            
            z0_eff = z0_over_H * H;
        } else {
            // Very dense canopy - use high roughness
            z0_eff = Real(0.1) * H;
        }
    } else {
        // d ≈ H, use small roughness
        z0_eff = Real(0.01) * H;
    }
    
    // Ensure z0_eff is at least z0_base
    z0_eff = amrex::max(z0_eff, z0_base);
}

// ============================================================================
// Shaw & Pereira (1982) exponential wind profile within canopy
//
// Computes wind speed within and above canopy using exponential decay model.
//
// Profile:
//   For z < h (within canopy):  u(z) = u(h) * exp(-α * (1 - z/h))
//   For z ≥ h (above canopy):   u(z) = log-law profile
//
// Inputs:
//   z_agl          - Height above ground level [m]
//   canopy_height  - Canopy height h [m]
//   u_at_top       - Wind speed at canopy top u(h) [m/s]
//   alpha          - Attenuation coefficient (typically 2-4)
//
// Returns:
//   Wind speed multiplier (ratio of u(z) to u_at_top)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real shaw_pereira_profile(
    amrex::Real z_agl,
    amrex::Real canopy_height,
    amrex::Real alpha)
{
    using amrex::Real;
    
    if (canopy_height <= Real(0.0) || z_agl >= canopy_height) {
        return Real(1.0);  // Above canopy or no canopy
    }
    
    if (z_agl <= Real(0.0)) {
        return Real(0.0);  // Below ground
    }
    
    // Within canopy: u(z)/u(h) = exp(-α * (1 - z/h))
    const Real z_ratio = z_agl / canopy_height;
    const Real exponent = -alpha * (Real(1.0) - z_ratio);
    
    return std::exp(exponent);
}

// ============================================================================
// Modified log-law profile with canopy displacement height
//
// Computes wind speed using log-law with displacement height correction.
//
// Profile:
//   u(z) = (u* / κ) * ln((z - d + z0) / z0)   for z > d
//   u(z) = 0                                   for z ≤ d
//
// Inputs:
//   z_agl          - Height above ground level [m]
//   displacement   - Displacement height d [m]
//   z0             - Roughness length [m]
//   ustar          - Friction velocity [m/s]
//   kappa          - von Karman constant (0.41)
//
// Returns:
//   Wind speed [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real log_law_with_displacement(
    amrex::Real z_agl,
    amrex::Real displacement,
    amrex::Real z0,
    amrex::Real ustar,
    amrex::Real kappa)
{
    using amrex::Real;
    
    // Height above displacement
    const Real z_eff = z_agl - displacement;
    
    if (z_eff <= Real(0.0)) {
        return Real(0.0);  // Below or at displacement height
    }
    
    // Log-law: u = (u*/κ) * ln((z_eff + z0) / z0)
    return (ustar / kappa) * std::log((z_eff + z0) / z0);
}

// ============================================================================
// Combined canopy wind profile
//
// Combines MacDonald displacement with optional Shaw-Pereira exponential
// decay within canopy.
//
// Inputs:
//   z_agl          - Height above ground level [m]
//   params         - Canopy parameters structure
//   z0_base        - Base surface roughness [m]
//   ustar          - Friction velocity [m/s]
//   kappa          - von Karman constant (0.41)
//
// Returns:
//   Wind speed [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real canopy_wind_profile(
    amrex::Real z_agl,
    const CanopyParams& params,
    amrex::Real z0_base,
    amrex::Real ustar,
    amrex::Real kappa)
{
    using amrex::Real;
    
    if (!params.enabled || params.height <= Real(0.0)) {
        // No canopy - use standard log-law
        return (ustar / kappa) * std::log((z_agl + z0_base) / z0_base);
    }
    
    // Compute MacDonald displacement height and effective roughness
    Real displacement, z0_eff;
    macdonald_canopy_parameters(
        params.height,
        params.frontal_area_index,
        params.plan_area_index,
        params.drag_coefficient,
        z0_base,
        displacement,
        z0_eff);
    
    if (params.use_exponential_profile && z_agl < params.height) {
        // Use Shaw-Pereira exponential profile within canopy
        
        // Compute wind speed at canopy top using log-law with displacement
        const Real z_top_eff = params.height - displacement;
        Real u_at_top = Real(0.0);
        
        if (z_top_eff > Real(0.0)) {
            u_at_top = (ustar / kappa) * std::log((z_top_eff + z0_eff) / z0_eff);
        }
        
        // Apply exponential decay within canopy
        const Real decay_factor = shaw_pereira_profile(
            z_agl, params.height, params.attenuation_coeff);
        
        return u_at_top * decay_factor;
    } else {
        // Use log-law with displacement height
        return log_law_with_displacement(
            z_agl, displacement, z0_eff, ustar, kappa);
    }
}

#endif // CANOPY_MODELS_H
