// ==========================================================================
// turbulent_stress.H
// Helper kernels for the double-pass turbulent stress correction.
//
// Implements:
//   1. compute_eddy_viscosity_on_faces() — face-centred ν_t from cell values
//   2. apply_turbulent_stress_term()    — div(ν_t ∇u) for one velocity component
//   3. compute_velocity_gradient_magnitude() — |∂u/∂z| shear helper
//
// All functions are AMREX_GPU_HOST_DEVICE so they can be called inside
// amrex::ParallelFor kernels on both CPU and GPU.
// ==========================================================================

#ifndef TURBULENT_STRESS_H
#define TURBULENT_STRESS_H

#include <AMReX_REAL.H>
#include <AMReX_Array4.H>
#include <cmath>

namespace TurbulentStress {

// --------------------------------------------------------------------------
// compute_velocity_gradient_magnitude
//
// Returns the local vertical-shear magnitude |∂u/∂z| at cell (i,j,k).
// Used as the shear input for mixing-length eddy viscosity:
//   |S| = sqrt( (∂u/∂z)² + (∂v/∂z)² )
// --------------------------------------------------------------------------
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_velocity_gradient_magnitude(
    int i, int j, int k,
    const amrex::Array4<const amrex::Real>& vel,
    amrex::Real inv_dz,
    int nz) noexcept
{
    amrex::Real du_dz = 0.0;
    amrex::Real dv_dz = 0.0;

    if (k < nz - 1) {
        du_dz = (vel(i, j, k + 1, 0) - vel(i, j, k, 0)) * inv_dz;
        dv_dz = (vel(i, j, k + 1, 1) - vel(i, j, k, 1)) * inv_dz;
    } else if (k > 0) {
        du_dz = (vel(i, j, k, 0) - vel(i, j, k - 1, 0)) * inv_dz;
        dv_dz = (vel(i, j, k, 1) - vel(i, j, k - 1, 1)) * inv_dz;
    }

    return std::sqrt(du_dz * du_dz + dv_dz * dv_dz);
}

// --------------------------------------------------------------------------
// compute_mixing_length_nut
//
// Mixing-length eddy viscosity at a single cell:
//   l_m  = κ * (z_agl + z0) * coeff
//   ν_t  = l_m² * |S|
//
// Parameters:
//   z_agl   – height above ground level [m]
//   z0      – aerodynamic roughness [m]
//   kappa   – von Kármán constant
//   coeff   – mixing-length proportionality coefficient
//   shear   – velocity-gradient magnitude |S| [1/s]
// --------------------------------------------------------------------------
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_mixing_length_nut(
    amrex::Real z_agl,
    amrex::Real z0,
    amrex::Real kappa,
    amrex::Real coeff,
    amrex::Real shear) noexcept
{
    // Clamp to 1 mm above ground to avoid a zero/negative mixing length at
    // terrain-coincident cells and to prevent division by zero in l_m.
    amrex::Real z_eff = amrex::max(z_agl, amrex::Real(1.0e-3));
    amrex::Real l_m   = kappa * (z_eff + z0) * coeff;
    return l_m * l_m * shear;
}

// --------------------------------------------------------------------------
// compute_eddy_viscosity_on_faces
//
// Arithmetic average of two neighbouring cell values onto a face.
// Use the horizontal Schmidt number for x/y faces and vertical Schmidt
// number for z faces to allow anisotropic scaling.
//
//   ν_face_h = 0.5 * (ν_t[i] + ν_t[i±1]) / Sc_h
//   ν_face_v = 0.5 * (ν_t[k] + ν_t[k±1]) / Sc_v
// --------------------------------------------------------------------------
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real eddy_viscosity_face_avg(
    amrex::Real nut_a,
    amrex::Real nut_b,
    amrex::Real inv_schmidt) noexcept
{
    return amrex::Real(0.5) * (nut_a + nut_b) * inv_schmidt;
}

// --------------------------------------------------------------------------
// apply_turbulent_stress_term
//
// Discrete div(ν_t ∇u_comp) for one velocity component at cell (i,j,k).
// Uses cell-centred ν_t values and second-order central differences:
//
//   d/dx[ν_t ∂u/∂x] ≈ { ν_t(i+½) [u(i+1)-u(i)]/dx
//                       - ν_t(i-½) [u(i)-u(i-1)]/dx } / dx
// (and similarly for y, z).
//
// Boundary cells use one-sided (first-order) differences.
// Horizontal directions use inv_sc_h scaling; vertical uses inv_sc_v.
//
// Parameters:
//   vel_comp  – velocity component index (0=u, 1=v, 2=w)
//   nut       – cell-centred eddy viscosity field
//   vel       – velocity multifab array
//   bx_lo/hi  – valid box bounds (for boundary handling)
//   inv_dx2 etc. – 1/(dx²)
//   inv_sc_h/v  – 1/Sc_h and 1/Sc_v
// --------------------------------------------------------------------------
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real apply_turbulent_stress_term(
    int i, int j, int k,
    int vel_comp,
    const amrex::Array4<const amrex::Real>& nut,
    const amrex::Array4<const amrex::Real>& vel,
    int ilo, int ihi,
    int jlo, int jhi,
    int klo, int khi,
    amrex::Real inv_dx2,
    amrex::Real inv_dy2,
    amrex::Real inv_dz2,
    amrex::Real inv_sc_h,
    amrex::Real inv_sc_v) noexcept
{
    amrex::Real u_c = vel(i, j, k, vel_comp);
    amrex::Real divterm = amrex::Real(0.0);

    // --- X direction ---
    if (i > ilo && i < ihi) {
        amrex::Real nut_xp = eddy_viscosity_face_avg(nut(i, j, k), nut(i + 1, j, k), inv_sc_h);
        amrex::Real nut_xm = eddy_viscosity_face_avg(nut(i - 1, j, k), nut(i, j, k), inv_sc_h);
        divterm += (nut_xp * (vel(i + 1, j, k, vel_comp) - u_c)
                  - nut_xm * (u_c - vel(i - 1, j, k, vel_comp))) * inv_dx2;
    } else if (i == ilo && i < ihi) {
        amrex::Real nut_xp = eddy_viscosity_face_avg(nut(i, j, k), nut(i + 1, j, k), inv_sc_h);
        divterm += nut_xp * (vel(i + 1, j, k, vel_comp) - u_c) * inv_dx2;
    } else if (i == ihi && i > ilo) {
        amrex::Real nut_xm = eddy_viscosity_face_avg(nut(i - 1, j, k), nut(i, j, k), inv_sc_h);
        divterm += -nut_xm * (u_c - vel(i - 1, j, k, vel_comp)) * inv_dx2;
    }

    // --- Y direction ---
    if (j > jlo && j < jhi) {
        amrex::Real nut_yp = eddy_viscosity_face_avg(nut(i, j, k), nut(i, j + 1, k), inv_sc_h);
        amrex::Real nut_ym = eddy_viscosity_face_avg(nut(i, j - 1, k), nut(i, j, k), inv_sc_h);
        divterm += (nut_yp * (vel(i, j + 1, k, vel_comp) - u_c)
                  - nut_ym * (u_c - vel(i, j - 1, k, vel_comp))) * inv_dy2;
    } else if (j == jlo && j < jhi) {
        amrex::Real nut_yp = eddy_viscosity_face_avg(nut(i, j, k), nut(i, j + 1, k), inv_sc_h);
        divterm += nut_yp * (vel(i, j + 1, k, vel_comp) - u_c) * inv_dy2;
    } else if (j == jhi && j > jlo) {
        amrex::Real nut_ym = eddy_viscosity_face_avg(nut(i, j - 1, k), nut(i, j, k), inv_sc_h);
        divterm += -nut_ym * (u_c - vel(i, j - 1, k, vel_comp)) * inv_dy2;
    }

    // --- Z direction ---
    if (k > klo && k < khi) {
        amrex::Real nut_zp = eddy_viscosity_face_avg(nut(i, j, k), nut(i, j, k + 1), inv_sc_v);
        amrex::Real nut_zm = eddy_viscosity_face_avg(nut(i, j, k - 1), nut(i, j, k), inv_sc_v);
        divterm += (nut_zp * (vel(i, j, k + 1, vel_comp) - u_c)
                  - nut_zm * (u_c - vel(i, j, k - 1, vel_comp))) * inv_dz2;
    } else if (k == klo && k < khi) {
        amrex::Real nut_zp = eddy_viscosity_face_avg(nut(i, j, k), nut(i, j, k + 1), inv_sc_v);
        divterm += nut_zp * (vel(i, j, k + 1, vel_comp) - u_c) * inv_dz2;
    } else if (k == khi && k > klo) {
        amrex::Real nut_zm = eddy_viscosity_face_avg(nut(i, j, k - 1), nut(i, j, k), inv_sc_v);
        divterm += -nut_zm * (u_c - vel(i, j, k - 1, vel_comp)) * inv_dz2;
    }

    return divterm;
}

} // namespace TurbulentStress

#endif // TURBULENT_STRESS_H
