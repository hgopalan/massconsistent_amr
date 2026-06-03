// ==========================================================================
// pressure_poisson_solver.H
// Perturbation pressure gradient solver
//
// Solves the pressure-Poisson equation for velocity correction:
//   ∇²p' = -∇·(u·∇u)
// where p' is the perturbation pressure deviation
//
// The velocity correction is then applied:
//   u_corrected = u_uncorrected - (1/ρ) ∇p'
//
// This provides an additional pressure-velocity coupling mechanism
// independent of the mass-consistency Lagrange multiplier approach.
//
// ==========================================================================

#ifndef PRESSURE_POISSON_SOLVER_H
#define PRESSURE_POISSON_SOLVER_H

#include <AMReX.H>
#include <AMReX_REAL.H>
#include <AMReX_Box.H>
#include <AMReX_FArrayBox.H>
#include <AMReX_Array.H>
#include <cmath>

namespace pressure_poisson {

// =========================================================================
// Compute right-hand side: -∇·(u·∇u)
// = -(u·∂u/∂x + v·∂u/∂y + w·∂u/∂z) + same for v and w components
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_convective_divergence(
    int i, int j, int k,
    const amrex::FArrayBox& u_fab,
    const amrex::FArrayBox& v_fab,
    const amrex::FArrayBox& w_fab,
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real dz) noexcept
{
    const amrex::Real inv_2dx = amrex::Real(1.0) / (amrex::Real(2.0) * dx);
    const amrex::Real inv_2dy = amrex::Real(1.0) / (amrex::Real(2.0) * dy);
    const amrex::Real inv_2dz = amrex::Real(1.0) / (amrex::Real(2.0) * dz);

    // Cell-center velocities
    amrex::Real u_c = u_fab(i, j, k, 0);
    amrex::Real v_c = v_fab(i, j, k, 0);
    amrex::Real w_c = w_fab(i, j, k, 0);

    // Gradients of u-component
    amrex::Real du_dx = (u_fab(i+1, j, k, 0) - u_fab(i-1, j, k, 0)) * inv_2dx;
    amrex::Real du_dy = (u_fab(i, j+1, k, 0) - u_fab(i, j-1, k, 0)) * inv_2dy;
    amrex::Real du_dz = (u_fab(i, j, k+1, 0) - u_fab(i, j, k-1, 0)) * inv_2dz;

    // Gradients of v-component
    amrex::Real dv_dx = (v_fab(i+1, j, k, 0) - v_fab(i-1, j, k, 0)) * inv_2dx;
    amrex::Real dv_dy = (v_fab(i, j+1, k, 0) - v_fab(i, j-1, k, 0)) * inv_2dy;
    amrex::Real dv_dz = (v_fab(i, j, k+1, 0) - v_fab(i, j, k-1, 0)) * inv_2dz;

    // Gradients of w-component
    amrex::Real dw_dx = (w_fab(i+1, j, k, 0) - w_fab(i-1, j, k, 0)) * inv_2dx;
    amrex::Real dw_dy = (w_fab(i, j+1, k, 0) - w_fab(i, j-1, k, 0)) * inv_2dy;
    amrex::Real dw_dz = (w_fab(i, j, k+1, 0) - w_fab(i, j, k-1, 0)) * inv_2dz;

    // ∇·(u·∇u) = ∇·(u(∂u/∂x)) + ∇·(v(∂u/∂y)) + ∇·(w(∂u/∂z))
    //           + ∇·(u(∂v/∂x)) + ∇·(v(∂v/∂y)) + ∇·(w(∂v/∂z))
    //           + ∇·(u(∂w/∂x)) + ∇·(v(∂w/∂y)) + ∇·(w(∂w/∂z))
    // Using product rule: ∇·(u∇u) ≈ u·∇(∇·u) + ∇u:∇u (neglecting second term, which is small)
    // Simpler approximation: sum of convective terms
    
    amrex::Real conv_term = u_c * du_dx + v_c * du_dy + w_c * du_dz +
                            u_c * dv_dx + v_c * dv_dy + w_c * dv_dz +
                            u_c * dw_dx + v_c * dw_dy + w_c * dw_dz;

    // RHS: -∇·(u·∇u)
    return -conv_term;
}

// =========================================================================
// Compute Laplacian of pressure: ∇²p' = ∂²p'/∂x² + ∂²p'/∂y² + ∂²p'/∂z²
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real laplacian_pressure_3d(
    int i, int j, int k,
    const amrex::FArrayBox& p_fab,
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real dz) noexcept
{
    const amrex::Real inv_dx2 = amrex::Real(1.0) / (dx * dx);
    const amrex::Real inv_dy2 = amrex::Real(1.0) / (dy * dy);
    const amrex::Real inv_dz2 = amrex::Real(1.0) / (dz * dz);

    // Centered differences
    amrex::Real p_xx = (p_fab(i+1, j, k, 0) - amrex::Real(2.0) * p_fab(i, j, k, 0) + p_fab(i-1, j, k, 0)) * inv_dx2;
    amrex::Real p_yy = (p_fab(i, j+1, k, 0) - amrex::Real(2.0) * p_fab(i, j, k, 0) + p_fab(i, j-1, k, 0)) * inv_dy2;
    amrex::Real p_zz = (p_fab(i, j, k+1, 0) - amrex::Real(2.0) * p_fab(i, j, k, 0) + p_fab(i, j, k-1, 0)) * inv_dz2;

    return p_xx + p_yy + p_zz;
}

// =========================================================================
// Compute gradient of pressure for velocity correction
// ∇p' = (∂p'/∂x, ∂p'/∂y, ∂p'/∂z)
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void compute_pressure_gradient(
    int i, int j, int k,
    const amrex::FArrayBox& p_fab,
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real dz,
    amrex::Real& dp_dx,
    amrex::Real& dp_dy,
    amrex::Real& dp_dz) noexcept
{
    const amrex::Real inv_2dx = amrex::Real(1.0) / (amrex::Real(2.0) * dx);
    const amrex::Real inv_2dy = amrex::Real(1.0) / (amrex::Real(2.0) * dy);
    const amrex::Real inv_2dz = amrex::Real(1.0) / (amrex::Real(2.0) * dz);

    dp_dx = (p_fab(i+1, j, k, 0) - p_fab(i-1, j, k, 0)) * inv_2dx;
    dp_dy = (p_fab(i, j+1, k, 0) - p_fab(i, j-1, k, 0)) * inv_2dy;
    dp_dz = (p_fab(i, j, k+1, 0) - p_fab(i, j, k-1, 0)) * inv_2dz;
}

// =========================================================================
// Apply pressure gradient correction to velocities
// u_corrected = u - (1/ρ) * ∂p'/∂x
// v_corrected = v - (1/ρ) * ∂p'/∂y
// w_corrected = w - (1/ρ) * ∂p'/∂z
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void apply_pressure_correction(
    int i, int j, int k,
    const amrex::FArrayBox& u_fab, int u_comp,
    const amrex::FArrayBox& v_fab, int v_comp,
    const amrex::FArrayBox& w_fab, int w_comp,
    const amrex::FArrayBox& p_fab,
    amrex::FArrayBox& u_out, int u_out_comp,
    amrex::FArrayBox& v_out, int v_out_comp,
    amrex::FArrayBox& w_out, int w_out_comp,
    amrex::Real inv_rho,
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real dz,
    amrex::Real pressure_scale) noexcept
{
    amrex::Real dp_dx, dp_dy, dp_dz;
    compute_pressure_gradient(i, j, k, p_fab, dx, dy, dz, dp_dx, dp_dy, dp_dz);

    // Apply correction with scaling factor (typically 0.0-1.0)
    u_out(i, j, k, u_out_comp) = u_fab(i, j, k, u_comp) - pressure_scale * inv_rho * dp_dx;
    v_out(i, j, k, v_out_comp) = v_fab(i, j, k, v_comp) - pressure_scale * inv_rho * dp_dy;
    w_out(i, j, k, w_out_comp) = w_fab(i, j, k, w_comp) - pressure_scale * inv_rho * dp_dz;
}

// =========================================================================
// Jacobi iteration for pressure Poisson solver
// p_{n+1} = (RHS + α_h²(p_E + p_W) + α_h²(p_N + p_S) + α_v²(p_T + p_B)) / (2α_h² + 2α_h² + 2α_v²)
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real jacobi_step(
    int i, int j, int k,
    const amrex::FArrayBox& rhs_fab,
    const amrex::FArrayBox& p_old_fab,
    amrex::Real alpha_h_sq,
    amrex::Real alpha_v_sq,
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real dz) noexcept
{
    const amrex::Real inv_dx2 = amrex::Real(1.0) / (dx * dx);
    const amrex::Real inv_dy2 = amrex::Real(1.0) / (dy * dy);
    const amrex::Real inv_dz2 = amrex::Real(1.0) / (dz * dz);

    // Laplacian stencil coefficients
    amrex::Real coeff_x = alpha_h_sq * inv_dx2;
    amrex::Real coeff_y = alpha_h_sq * inv_dy2;
    amrex::Real coeff_z = alpha_v_sq * inv_dz2;
    amrex::Real coeff_center = amrex::Real(2.0) * (coeff_x + coeff_y + coeff_z);

    // Neighbors
    amrex::Real p_e = p_old_fab(i+1, j, k, 0);
    amrex::Real p_w = p_old_fab(i-1, j, k, 0);
    amrex::Real p_n = p_old_fab(i, j+1, k, 0);
    amrex::Real p_s = p_old_fab(i, j-1, k, 0);
    amrex::Real p_t = p_old_fab(i, j, k+1, 0);
    amrex::Real p_b = p_old_fab(i, j, k-1, 0);

    amrex::Real p_new = (rhs_fab(i, j, k, 0) + coeff_x * (p_e + p_w) + 
                         coeff_y * (p_n + p_s) + coeff_z * (p_t + p_b)) / coeff_center;

    return p_new;
}

} // namespace pressure_poisson

#endif // PRESSURE_POISSON_SOLVER_H
