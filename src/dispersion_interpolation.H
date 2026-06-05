#ifndef DISPERSION_INTERPOLATION_H
#define DISPERSION_INTERPOLATION_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <AMReX_Array4.H>
#include <AMReX_Box.H>
#include <AMReX_Algorithm.H>
#include <cmath>

// ============================================================================
// Shared Trilinear and Quadrilinear Velocity Interpolation for Puff/LPDM
// ============================================================================

template<typename T>
AMREX_GPU_HOST_DEVICE AMREX_INLINE
T clamp_val(T val, T low, T high) {
    return (val < low) ? low : ((val > high) ? high : val);
}

AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real interpolate_trilinear_3d(
    amrex::Real x, amrex::Real y, amrex::Real z,
    amrex::Array4<const amrex::Real> const& vel_arr,
    amrex::Real const* problo,
    amrex::Real const* dx,
    amrex::Box const& domain,
    int component)
{
    int i0 = static_cast<int>(amrex::Math::floor((x - problo[0] - amrex::Real(0.5) * dx[0]) / dx[0]));
    int j0 = static_cast<int>(amrex::Math::floor((y - problo[1] - amrex::Real(0.5) * dx[1]) / dx[1]));
    int k0 = static_cast<int>(amrex::Math::floor((z - problo[2] - amrex::Real(0.5) * dx[2]) / dx[2]));
    
    int i1 = i0 + 1;
    int j1 = j0 + 1;
    int k1 = k0 + 1;
    
    // Clamp to domain limits
    int imin = domain.smallEnd(0);
    int imax = domain.bigEnd(0) - 1;
    int jmin = domain.smallEnd(1);
    int jmax = domain.bigEnd(1) - 1;
    int kmin = domain.smallEnd(2);
    int kmax = domain.bigEnd(2) - 1;
    
    i0 = clamp_val(i0, imin, imax);
    i1 = clamp_val(i1, imin, imax);
    j0 = clamp_val(j0, jmin, jmax);
    j1 = clamp_val(j1, jmin, jmax);
    k0 = clamp_val(k0, kmin, kmax);
    k1 = clamp_val(k1, kmin, kmax);
    
    amrex::Real x0 = problo[0] + (i0 + amrex::Real(0.5)) * dx[0];
    amrex::Real y0 = problo[1] + (j0 + amrex::Real(0.5)) * dx[1];
    amrex::Real z0 = problo[2] + (k0 + amrex::Real(0.5)) * dx[2];
    
    amrex::Real x_frac = (i0 == i1) ? amrex::Real(0.0) : (x - x0) / dx[0];
    amrex::Real y_frac = (j0 == j1) ? amrex::Real(0.0) : (y - y0) / dx[1];
    amrex::Real z_frac = (k0 == k1) ? amrex::Real(0.0) : (z - z0) / dx[2];
    
    x_frac = clamp_val(x_frac, amrex::Real(0.0), amrex::Real(1.0));
    y_frac = clamp_val(y_frac, amrex::Real(0.0), amrex::Real(1.0));
    z_frac = clamp_val(z_frac, amrex::Real(0.0), amrex::Real(1.0));
    
    amrex::Real c000 = vel_arr(i0, j0, k0, component);
    amrex::Real c100 = vel_arr(i1, j0, k0, component);
    amrex::Real c010 = vel_arr(i0, j1, k0, component);
    amrex::Real c110 = vel_arr(i1, j1, k0, component);
    amrex::Real c001 = vel_arr(i0, j0, k1, component);
    amrex::Real c101 = vel_arr(i1, j0, k1, component);
    amrex::Real c011 = vel_arr(i0, j1, k1, component);
    amrex::Real c111 = vel_arr(i1, j1, k1, component);
    
    amrex::Real c00 = c000 * (amrex::Real(1.0) - x_frac) + c100 * x_frac;
    amrex::Real c10 = c010 * (amrex::Real(1.0) - x_frac) + c110 * x_frac;
    amrex::Real c01 = c001 * (amrex::Real(1.0) - x_frac) + c101 * x_frac;
    amrex::Real c11 = c011 * (amrex::Real(1.0) - x_frac) + c111 * x_frac;
    
    amrex::Real c0 = c00 * (amrex::Real(1.0) - y_frac) + c10 * y_frac;
    amrex::Real c1 = c01 * (amrex::Real(1.0) - y_frac) + c11 * y_frac;
    
    return c0 * (amrex::Real(1.0) - z_frac) + c1 * z_frac;
}

AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real interpolate_quadrilinear_4d(
    amrex::Real x, amrex::Real y, amrex::Real z, amrex::Real t,
    amrex::Array4<const amrex::Real> const& vel_arr_t0,
    amrex::Array4<const amrex::Real> const& vel_arr_t1,
    amrex::Real t0, amrex::Real t1,
    amrex::Real const* problo,
    amrex::Real const* dx,
    amrex::Box const& domain,
    int component)
{
    amrex::Real val_t0 = interpolate_trilinear_3d(x, y, z, vel_arr_t0, problo, dx, domain, component);
    amrex::Real val_t1 = interpolate_trilinear_3d(x, y, z, vel_arr_t1, problo, dx, domain, component);
    if (std::abs(t1 - t0) < amrex::Real(1.0e-12)) {
        return val_t0;
    }
    amrex::Real t_frac = (t - t0) / (t1 - t0);
    t_frac = clamp_val(t_frac, amrex::Real(0.0), amrex::Real(1.0));
    return val_t0 * (amrex::Real(1.0) - t_frac) + val_t1 * t_frac;
}

#endif // DISPERSION_INTERPOLATION_H
