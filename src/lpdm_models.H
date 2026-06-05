// ============================================================================
// lpdm_models.H
// Lagrangian Particle Dispersion Model (LPDM) for passive plume modeling
//
// This header provides structures and functions for computing passive plume
// dispersion using a Lagrangian particle approach. Particles are discrete
// point masses that are emitted from a source, drift with the wind, and
// disperse due to a stochastic random walk corresponding to diffusion.
// ============================================================================

#ifndef LPDM_MODELS_H
#define LPDM_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <AMReX_Array4.H>
#include <AMReX_Box.H>
#include <AMReX_Algorithm.H>
#include <algorithm>
#include <cmath>

// ============================================================================
// Trilinear and Quadrilinear Velocity Interpolation
// ============================================================================
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
    
    i0 = amrex::clamp(i0, imin, imax);
    i1 = amrex::clamp(i1, imin, imax);
    j0 = amrex::clamp(j0, jmin, jmax);
    j1 = amrex::clamp(j1, jmin, jmax);
    k0 = amrex::clamp(k0, kmin, kmax);
    k1 = amrex::clamp(k1, kmin, kmax);
    
    amrex::Real x0 = problo[0] + (i0 + amrex::Real(0.5)) * dx[0];
    amrex::Real y0 = problo[1] + (j0 + amrex::Real(0.5)) * dx[1];
    amrex::Real z0 = problo[2] + (k0 + amrex::Real(0.5)) * dx[2];
    
    amrex::Real x_frac = (i0 == i1) ? amrex::Real(0.0) : (x - x0) / dx[0];
    amrex::Real y_frac = (j0 == j1) ? amrex::Real(0.0) : (y - y0) / dx[1];
    amrex::Real z_frac = (k0 == k1) ? amrex::Real(0.0) : (z - z0) / dx[2];
    
    x_frac = amrex::clamp(x_frac, amrex::Real(0.0), amrex::Real(1.0));
    y_frac = amrex::clamp(y_frac, amrex::Real(0.0), amrex::Real(1.0));
    z_frac = amrex::clamp(z_frac, amrex::Real(0.0), amrex::Real(1.0));
    
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
    t_frac = amrex::clamp(t_frac, amrex::Real(0.0), amrex::Real(1.0));
    return val_t0 * (amrex::Real(1.0) - t_frac) + val_t1 * t_frac;
}

// ============================================================================
// LPD Particle Structure
// Represents a single Lagrangian particle
// ============================================================================
struct LpdParticle {
    amrex::Real x, y, z;            // Particle position [m]
    amrex::Real mass;               // Mass/strength of particle [units]
    amrex::Real time_emitted;       // Time when particle was emitted [s]
    amrex::Real age;                // Current age of particle [s]
    bool active;                    // Whether particle is still modeled
};

// ============================================================================
// Create a new particle at the source
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
LpdParticle create_particle(
    amrex::Real source_x, amrex::Real source_y, amrex::Real source_z,
    amrex::Real mass,
    amrex::Real time)
{
    LpdParticle p;
    p.x = source_x;
    p.y = source_y;
    p.z = source_z;
    p.mass = mass;
    p.time_emitted = time;
    p.age = amrex::Real(0.0);
    p.active = true;
    return p;
}

// ============================================================================
// Advect particle with wind velocity and stochastic random walk
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void advect_particle_with_terrain(
    LpdParticle& p,
    amrex::Real u,
    amrex::Real v,
    amrex::Real w,
    amrex::Real rand_dx,
    amrex::Real rand_dy,
    amrex::Real rand_dz,
    amrex::Real dt,
    amrex::Real terrain_height,
    bool enable_reflection = false)
{
    if (!p.active || dt < amrex::Real(0.0)) {
        return;
    }
    
    // Advect with wind + stochastic random walk
    p.x += u * dt + rand_dx;
    p.y += v * dt + rand_dy;
    p.z += w * dt + rand_dz;
    
    // Ground reflection
    if (enable_reflection && p.z < terrain_height) {
        // Reflect particle above ground: z_new = 2*terrain_height - z_old
        p.z = amrex::Real(2.0) * terrain_height - p.z;
    }
}

// ============================================================================
// Advect particle with wind velocity and stochastic random walk (no terrain)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void advect_particle(
    LpdParticle& p,
    amrex::Real u,
    amrex::Real v,
    amrex::Real w,
    amrex::Real rand_dx,
    amrex::Real rand_dy,
    amrex::Real rand_dz,
    amrex::Real dt)
{
    if (!p.active || dt < amrex::Real(0.0)) {
        return;
    }
    
    p.x += u * dt + rand_dx;
    p.y += v * dt + rand_dy;
    p.z += w * dt + rand_dz;
}

// ============================================================================
// Check if particle is outside domain and deactivate
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void check_particle_bounds_with_terrain(
    LpdParticle& p,
    amrex::Real xmin, amrex::Real xmax,
    amrex::Real ymin, amrex::Real ymax,
    amrex::Real zmin, amrex::Real zmax,
    amrex::Real terrain_height = amrex::Real(0.0),
    bool check_terrain = false)
{
    if (!p.active) {
        return;
    }
    
    // Check horizontal bounds
    if (p.x < xmin || p.x > xmax ||
        p.y < ymin || p.y > ymax) {
        p.active = false;
        return;
    }
    
    // Check vertical bounds
    if (check_terrain) {
        // Deactivate if significantly below terrain
        const amrex::Real tolerance = amrex::Real(1.0);  // 1 meter tolerance
        if (p.z < terrain_height - tolerance || p.z > zmax) {
            p.active = false;
        }
    } else {
        // Standard check against domain bounds
        if (p.z < zmin || p.z > zmax) {
            p.active = false;
        }
    }
}

// ============================================================================
// Check if particle is outside domain and deactivate (no terrain)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void check_particle_bounds(
    LpdParticle& p,
    amrex::Real xmin, amrex::Real xmax,
    amrex::Real ymin, amrex::Real ymax,
    amrex::Real zmin, amrex::Real zmax)
{
    if (!p.active) {
        return;
    }
    
    if (p.x < xmin || p.x > xmax ||
        p.y < ymin || p.y > ymax ||
        p.z < zmin || p.z > zmax) {
        p.active = false;
    }
}

#endif // LPDM_MODELS_H
