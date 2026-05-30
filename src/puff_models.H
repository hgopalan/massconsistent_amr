// ============================================================================
// puff_models.H
// Gaussian puff dispersion model for passive plume modeling
//
// This header provides functions for computing passive plume dispersion using
// a Gaussian puff approach. Puffs are discrete Gaussian-shaped concentration
// clouds that are emitted from a source, drift with the wind, and grow over time.
//
// Model: Gaussian Puff (similar to CALPUFF, QUIC-PLUME)
// - Each puff: position (x,y,z), σy(t), σz(t), mass
// - Advection: drift with local wind velocity
// - Diffusion: Gaussian growth σ(t) = √(σ0² + 2K·t)
// - Concentration: C(x,y,z) = (mass/(2π)^3/2 · σx · σy · σz) · 
//                             exp(-(dx²/(2σx²) + dy²/(2σy²) + dz²/(2σz²)))
//
// References:
// - Hanna, S.R., et al. (1982). Handbook on atmospheric diffusion. 
//   DOE/TIC-11223, U.S. Department of Energy
// - Pasquill, F., & Gifford, F.A. (1961). The estimation of the dispersion 
//   of wind-borne material from industrial sources. Meteorological Magazine.
// ============================================================================

#ifndef PUFF_MODELS_H
#define PUFF_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
    using namespace amrex;
}

// ============================================================================
// Gaussian Puff Parameters
// ============================================================================
struct PuffParams {
    bool enabled = false;                    // Enable puff model
    amrex::Real source_x = 0.0;              // Source x-coordinate [m]
    amrex::Real source_y = 0.0;              // Source y-coordinate [m]
    amrex::Real source_z = 10.0;             // Source height [m]
    amrex::Real emission_rate = 1.0;         // Emission rate [units/s]
    amrex::Real emission_duration = 100.0;   // Duration of emission [s]
    amrex::Real K_h = 1.0;                   // Horizontal diffusivity [m²/s]
    amrex::Real K_v = 0.5;                   // Vertical diffusivity [m²/s]
    amrex::Real sigma_y0 = 1.0;              // Initial puff width σy [m]
    amrex::Real sigma_z0 = 1.0;              // Initial puff height σz [m]
};

// ============================================================================
// Puff Structure
// Represents a single Gaussian puff cloud
// ============================================================================
struct Puff {
    amrex::Real x, y, z;            // Puff center position [m]
    amrex::Real sigma_y, sigma_z;   // Gaussian standard deviations [m]
    amrex::Real mass;               // Mass/strength of puff [units]
    amrex::Real time_emitted;       // Time when puff was emitted [s]
    amrex::Real age;                // Current age of puff [s]
    bool active;                    // Whether puff should be modeled
};

// ============================================================================
// Compute Gaussian concentration from a single puff
//
// Computes the concentration at point (x, y, z) due to a single puff using
// the 3D Gaussian formula:
//
//   C = (mass / ((2π)^(3/2) * σx * σy * σz)) * 
//       exp(-(dx²/(2σx²) + dy²/(2σy²) + dz²/(2σz²)))
//
// For simplicity, σx = σy (horizontal isotropy) in this implementation.
//
// Inputs:
//   x, y, z          - Point coordinates [m]
//   puff             - Puff structure with position, size, mass
//
// Returns:
//   Concentration at (x, y, z) due to this puff [units/m³]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real gaussian_puff_concentration(
    amrex::Real x, amrex::Real y, amrex::Real z,
    const Puff& puff)
{
    if (!puff.active || puff.mass <= amrex::Real(0.0)) {
        return amrex::Real(0.0);
    }
    
    // Distance from puff center
    const amrex::Real dx = x - puff.x;
    const amrex::Real dy = y - puff.y;
    const amrex::Real dz = z - puff.z;
    
    // Avoid division by zero
    if (puff.sigma_y <= amrex::Real(1.0e-10) || puff.sigma_z <= amrex::Real(1.0e-10)) {
        return amrex::Real(0.0);
    }
    
    // Horizontal standard deviation (use σy for both x and y)
    const amrex::Real sigma_x = puff.sigma_y;
    const amrex::Real sigma_y = puff.sigma_y;
    const amrex::Real sigma_z = puff.sigma_z;
    
    // Compute exponent: -(dx²/(2σx²) + dy²/(2σy²) + dz²/(2σz²))
    const amrex::Real exponent = 
        -amrex::Real(0.5) * (
            dx * dx / (sigma_x * sigma_x) +
            dy * dy / (sigma_y * sigma_y) +
            dz * dz / (sigma_z * sigma_z)
        );
    
    // Avoid overflow/underflow
    if (exponent < amrex::Real(-100.0)) {
        return amrex::Real(0.0);  // Negligibly small
    }
    
    // Normalization constant: 1 / ((2π)^(3/2) * σx * σy * σz)
    const amrex::Real pi = amrex::Real(3.14159265358979323846);
    const amrex::Real normalization = amrex::Real(1.0) / (
        std::pow(amrex::Real(2.0) * pi, amrex::Real(1.5)) * 
        sigma_x * sigma_y * sigma_z
    );
    
    // Gaussian concentration
    const amrex::Real concentration = puff.mass * normalization * std::exp(exponent);
    
    return concentration;
}

// ============================================================================
// Compute puff growth over time
//
// Updates puff standard deviations based on diffusion and age using:
//   σ(t) = √(σ₀² + 2K·t)
//
// This represents Gaussian growth of the puff cloud as it diffuses.
//
// Inputs:
//   puff             - Puff to update
//   K_h              - Horizontal diffusivity [m²/s]
//   K_v              - Vertical diffusivity [m²/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void update_puff_growth(
    Puff& puff,
    amrex::Real K_h,
    amrex::Real K_v)
{
    if (!puff.active || puff.age < amrex::Real(0.0)) {
        return;
    }
    
    // Avoid negative diffusivity
    const amrex::Real K_h_safe = std::max(K_h, amrex::Real(0.0));
    const amrex::Real K_v_safe = std::max(K_v, amrex::Real(0.0));
    
    // Growth formula: σ(t) = √(σ₀² + 2K·t)
    const amrex::Real sigma_y_squared = 
        puff.sigma_y * puff.sigma_y + amrex::Real(2.0) * K_h_safe * puff.age;
    const amrex::Real sigma_z_squared = 
        puff.sigma_z * puff.sigma_z + amrex::Real(2.0) * K_v_safe * puff.age;
    
    puff.sigma_y = std::sqrt(sigma_y_squared);
    puff.sigma_z = std::sqrt(sigma_z_squared);
}

// ============================================================================
// Advect puff with wind velocity
//
// Moves puff center with the local wind velocity using Euler time stepping:
//   x_new = x_old + u·dt
//
// Inputs:
//   puff             - Puff to advect
//   u, v, w          - Wind velocity components [m/s]
//   dt               - Time step [s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void advect_puff(
    Puff& puff,
    amrex::Real u,
    amrex::Real v,
    amrex::Real w,
    amrex::Real dt)
{
    if (!puff.active || dt < amrex::Real(0.0)) {
        return;
    }
    
    puff.x += u * dt;
    puff.y += v * dt;
    puff.z += w * dt;
}

// ============================================================================
// Update puff age and deactivate if necessary
//
// Increments puff age and deactivates puff if it has drifted far from source
// or age exceeds reasonable limit (optional).
//
// Inputs:
//   puff             - Puff to update
//   dt               - Time step [s]
//   max_age          - Maximum puff age before deactivation [s] (optional, ≤0 = no limit)
//   domain_xmin/max  - Domain boundaries to check if puff left domain
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void update_puff_age(
    Puff& puff,
    amrex::Real dt,
    amrex::Real max_age = amrex::Real(-1.0))
{
    if (!puff.active) {
        return;
    }
    
    puff.age += dt;
    
    // Deactivate if exceeded max age
    if (max_age > amrex::Real(0.0) && puff.age > max_age) {
        puff.active = false;
    }
}

// ============================================================================
// Check if puff is outside domain and deactivate
//
// Inputs:
//   puff             - Puff to check
//   xmin, xmax       - Domain bounds [m]
//   ymin, ymax
//   zmin, zmax
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void check_puff_bounds(
    Puff& puff,
    amrex::Real xmin, amrex::Real xmax,
    amrex::Real ymin, amrex::Real ymax,
    amrex::Real zmin, amrex::Real zmax)
{
    if (!puff.active) {
        return;
    }
    
    // Check if puff center is outside domain (with margin)
    if (puff.x < xmin || puff.x > xmax ||
        puff.y < ymin || puff.y > ymax ||
        puff.z < zmin || puff.z > zmax) {
        puff.active = false;
    }
}

// ============================================================================
// Create a new puff at the source
//
// Inputs:
//   source_x, source_y, source_z - Source position [m]
//   mass                          - Mass of puff [units]
//   sigma_y0, sigma_z0            - Initial puff standard deviations [m]
//   time                          - Current simulation time [s]
//
// Returns:
//   New Puff structure
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Puff create_puff(
    amrex::Real source_x, amrex::Real source_y, amrex::Real source_z,
    amrex::Real mass,
    amrex::Real sigma_y0, amrex::Real sigma_z0,
    amrex::Real time)
{
    Puff puff;
    puff.x = source_x;
    puff.y = source_y;
    puff.z = source_z;
    puff.sigma_y = sigma_y0;
    puff.sigma_z = sigma_z0;
    puff.mass = mass;
    puff.time_emitted = time;
    puff.age = amrex::Real(0.0);
    puff.active = true;
    return puff;
}

#endif // PUFF_MODELS_H
