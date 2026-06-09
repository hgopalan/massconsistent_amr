// ==========================================================================
// divergence_damping.H
// Divergence damping filter for post-solve smoothing
//
// Implements implicit smoothing of the Lagrange multiplier field to reduce
// spurious divergence in the corrected velocity field.
//
// Physics:
//   λ_filtered = λ - ε∇²λ
// where ε is the damping coefficient (typically 0.01-0.1 × grid_spacing²)
//
// This kernel applies a Laplacian diffusion operator to the Lagrange multiplier
// in a post-processing step after the mass-consistency Poisson solve.
//
// ==========================================================================

#ifndef DIVERGENCE_DAMPING_H
#define DIVERGENCE_DAMPING_H

#include <AMReX.H>
#include <AMReX_REAL.H>
#include <AMReX_Box.H>
#include <AMReX_FArrayBox.H>
#include <cmath>

namespace divergence_damping {

// =========================================================================
// Compute Laplacian of a scalar field: ∇²f = ∂²f/∂x² + ∂²f/∂y² + ∂²f/∂z²
// Using 2nd-order centered finite differences
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real laplacian_3d(
    int i, int j, int k,
    const amrex::FArrayBox& fab,
    int comp,
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real dz) noexcept
{
    const amrex::Real inv_dx2 = amrex::Real(1.0) / (dx * dx);
    const amrex::Real inv_dy2 = amrex::Real(1.0) / (dy * dy);
    const amrex::Real inv_dz2 = amrex::Real(1.0) / (dz * dz);

    // Centered differences: f_xx = (f(i+1) - 2*f(i) + f(i-1)) / dx²
    amrex::Real f_xx = (fab(i+1, j, k, comp) - amrex::Real(2.0) * fab(i, j, k, comp) + fab(i-1, j, k, comp)) * inv_dx2;
    amrex::Real f_yy = (fab(i, j+1, k, comp) - amrex::Real(2.0) * fab(i, j, k, comp) + fab(i, j-1, k, comp)) * inv_dy2;
    amrex::Real f_zz = (fab(i, j, k+1, comp) - amrex::Real(2.0) * fab(i, j, k, comp) + fab(i, j, k-1, comp)) * inv_dz2;

    return f_xx + f_yy + f_zz;
}

// =========================================================================
// Compute damped Lagrange multiplier: λ_filtered = λ - ε∇²λ
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real damped_lagrange_multiplier(
    int i, int j, int k,
    const amrex::FArrayBox& lambda_fab,
    int lambda_comp,
    amrex::Real damping_coefficient,
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real dz) noexcept
{
    amrex::Real lambda_val = lambda_fab(i, j, k, lambda_comp);
    amrex::Real laplacian = laplacian_3d(i, j, k, lambda_fab, lambda_comp, dx, dy, dz);
    
    // λ_filtered = λ - ε∇²λ  (negative sign for smoothing - removes high-frequency content)
    return lambda_val - damping_coefficient * laplacian;
}

// =========================================================================
// Compute damped Lagrange multiplier anisotropically: λ_filtered = λ - ε_h∇_h²λ - ε_v∇_v²λ
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real damped_lagrange_multiplier_anisotropic(
    int i, int j, int k,
    const amrex::FArrayBox& lambda_fab,
    int lambda_comp,
    amrex::Real damping_coeff_h,
    amrex::Real damping_coeff_v,
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real dz) noexcept
{
    const amrex::Real inv_dx2 = amrex::Real(1.0) / (dx * dx);
    const amrex::Real inv_dy2 = amrex::Real(1.0) / (dy * dy);
    const amrex::Real inv_dz2 = amrex::Real(1.0) / (dz * dz);

    amrex::Real f_xx = (lambda_fab(i+1, j, k, lambda_comp) - amrex::Real(2.0) * lambda_fab(i, j, k, lambda_comp) + lambda_fab(i-1, j, k, lambda_comp)) * inv_dx2;
    amrex::Real f_yy = (lambda_fab(i, j+1, k, lambda_comp) - amrex::Real(2.0) * lambda_fab(i, j, k, lambda_comp) + lambda_fab(i, j-1, k, lambda_comp)) * inv_dy2;
    amrex::Real f_zz = (lambda_fab(i, j, k+1, lambda_comp) - amrex::Real(2.0) * lambda_fab(i, j, k, lambda_comp) + lambda_fab(i, j, k-1, lambda_comp)) * inv_dz2;

    amrex::Real lambda_val = lambda_fab(i, j, k, lambda_comp);
    return lambda_val - damping_coeff_h * (f_xx + f_yy) - damping_coeff_v * f_zz;
}

// =========================================================================
// Compute velocity correction from damped Lagrange multiplier gradient
// u_corrected = u - α_h² ∇λ_damped
// v_corrected = v - α_h² ∇λ_damped
// w_corrected = w - α_v² ∇λ_damped
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void compute_damped_velocity_correction(
    int i, int j, int k,
    const amrex::FArrayBox& u_fab, int u_comp,
    const amrex::FArrayBox& v_fab, int v_comp,
    const amrex::FArrayBox& w_fab, int w_comp,
    const amrex::FArrayBox& lambda_damped_fab, int lambda_comp,
    amrex::FArrayBox& u_out, int u_out_comp,
    amrex::FArrayBox& v_out, int v_out_comp,
    amrex::FArrayBox& w_out, int w_out_comp,
    amrex::Real alpha_h_sq,
    amrex::Real alpha_v_sq,
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real dz) noexcept
{
    const amrex::Real inv_2dx = amrex::Real(1.0) / (amrex::Real(2.0) * dx);
    const amrex::Real inv_2dy = amrex::Real(1.0) / (amrex::Real(2.0) * dy);
    const amrex::Real inv_2dz = amrex::Real(1.0) / (amrex::Real(2.0) * dz);

    // Gradient of damped Lagrange multiplier (centered differences)
    amrex::Real dlambda_dx = (lambda_damped_fab(i+1, j, k, lambda_comp) - lambda_damped_fab(i-1, j, k, lambda_comp)) * inv_2dx;
    amrex::Real dlambda_dy = (lambda_damped_fab(i, j+1, k, lambda_comp) - lambda_damped_fab(i, j-1, k, lambda_comp)) * inv_2dy;
    amrex::Real dlambda_dz = (lambda_damped_fab(i, j, k+1, lambda_comp) - lambda_damped_fab(i, j, k-1, lambda_comp)) * inv_2dz;

    // Velocity corrections
    u_out(i, j, k, u_out_comp) = u_fab(i, j, k, u_comp) - alpha_h_sq * dlambda_dx;
    v_out(i, j, k, v_out_comp) = v_fab(i, j, k, v_comp) - alpha_h_sq * dlambda_dy;
    w_out(i, j, k, w_out_comp) = w_fab(i, j, k, w_comp) - alpha_v_sq * dlambda_dz;
}

// =========================================================================
// Estimate optimal damping coefficient from grid spacing
// Typical: ε ≈ 0.05 × min(dx², dy², dz²)
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real estimate_damping_coefficient(
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real dz) noexcept
{
    amrex::Real min_spacing = std::min({dx, dy, dz});
    amrex::Real min_spacing_sq = min_spacing * min_spacing;
    
    // Default: ~5% of grid spacing squared
    return amrex::Real(0.05) * min_spacing_sq;
}

// =========================================================================
// Estimate optimal anisotropic damping coefficients from grid spacing
// Horizontal: ε_h ≈ 0.05 × min(dx², dy²)
// Vertical:   ε_v ≈ 0.05 × dz²
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void estimate_damping_coefficients_anisotropic(
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real dz,
    amrex::Real& damping_coeff_h,
    amrex::Real& damping_coeff_v) noexcept
{
    amrex::Real h_spacing = std::min(dx, dy);
    damping_coeff_h = amrex::Real(0.05) * h_spacing * h_spacing;
    damping_coeff_v = amrex::Real(0.05) * dz * dz;
}

} // namespace divergence_damping

#endif // DIVERGENCE_DAMPING_H
