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
#include "dispersion_interpolation.H"
#include <algorithm>
#include <cmath>

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
// Advect particle with wind velocity, terminal settling velocity, and stochastic random walk
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
    bool enable_reflection = false,
    amrex::Real v_s = amrex::Real(0.0))
{
    if (!p.active || dt < amrex::Real(0.0)) {
        return;
    }
    
    // Advect with wind, terminal settling velocity, + stochastic random walk
    p.x += u * dt + rand_dx;
    p.y += v * dt + rand_dy;
    p.z += (w - v_s) * dt + rand_dz;
    
    // Ground reflection
    if (enable_reflection && p.z < terrain_height) {
        // Reflect particle above ground: z_new = 2*terrain_height - z_old
        p.z = amrex::Real(2.0) * terrain_height - p.z;
    }
}

// ============================================================================
// Advect particle with wind velocity, terminal settling velocity, and stochastic random walk (no terrain)
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
    amrex::Real dt,
    amrex::Real v_s = amrex::Real(0.0))
{
    if (!p.active || dt < amrex::Real(0.0)) {
        return;
    }
    
    p.x += u * dt + rand_dx;
    p.y += v * dt + rand_dy;
    p.z += (w - v_s) * dt + rand_dz;
}

// ============================================================================
// Apply ground dry deposition to particle
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_particle_deposition(
    LpdParticle& p,
    amrex::Real dt,
    amrex::Real z_agl,
    amrex::Real deposition_velocity,
    amrex::Real height_scale = amrex::Real(1.0))
{
    if (!p.active || p.mass <= amrex::Real(0.0)) {
        return;
    }
    if (deposition_velocity <= amrex::Real(0.0) || dt <= amrex::Real(0.0)) {
        return;
    }
    // Deposition occurs close to the ground (within 2 meters as a representative surface layer)
    if (z_agl >= amrex::Real(0.0) && z_agl < amrex::Real(2.0)) {
        amrex::Real decay_rate = deposition_velocity / std::max(height_scale, amrex::Real(0.1));
        p.mass *= std::exp(-decay_rate * dt);
        
        if (p.mass < amrex::Real(1.0e-12)) {
            p.active = false;
        }
    }
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
