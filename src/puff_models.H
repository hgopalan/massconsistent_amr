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
// Terrain Grid Structure
// Stores interpolated terrain elevation data for puff reflection
// ============================================================================
struct TerrainGrid {
    std::vector<amrex::Real> x_terr;         // Terrain x-coordinates [m]
    std::vector<amrex::Real> y_terr;         // Terrain y-coordinates [m]
    std::vector<amrex::Real> z_terr;         // Terrain elevation [m]
    amrex::Real xmin, xmax;                  // Terrain domain bounds [m]
    amrex::Real ymin, ymax;
    bool enabled = false;                    // Terrain awareness enabled
};

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
    bool enable_terrain_reflection = false;  // Enable ground reflection
    bool use_image_source = true;            // Use image source method for concentration
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
// Bilinear interpolation of terrain height at point (x, y)
//
// Uses inverse distance weighting (IDW) for scattered terrain points.
// For a regular grid, this simplifies to bilinear interpolation.
//
// Inputs:
//   x, y             - Query point coordinates [m]
//   terrain          - Terrain grid structure
//
// Returns:
//   Interpolated terrain elevation [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real interpolate_terrain_height(
    amrex::Real x, amrex::Real y,
    const std::vector<amrex::Real>& x_terr,
    const std::vector<amrex::Real>& y_terr,
    const std::vector<amrex::Real>& z_terr)
{
    if (x_terr.empty() || y_terr.empty() || z_terr.empty()) {
        return amrex::Real(0.0);  // No terrain data, assume flat
    }
    
    // Inverse distance weighting (IDW)
    amrex::Real sum_weights = amrex::Real(0.0);
    amrex::Real sum_weighted_z = amrex::Real(0.0);
    const amrex::Real power = amrex::Real(2.0);  // IDW power parameter
    const amrex::Real epsilon = amrex::Real(1.0e-6);  // Avoid division by zero
    
    const int n = static_cast<int>(x_terr.size());
    for (int i = 0; i < n; ++i) {
        const amrex::Real dx = x - x_terr[i];
        const amrex::Real dy = y - y_terr[i];
        const amrex::Real dist_sq = dx * dx + dy * dy;
        
        // If point is very close to a terrain point, use that elevation
        if (dist_sq < epsilon) {
            return z_terr[i];
        }
        
        const amrex::Real dist = std::sqrt(dist_sq);
        const amrex::Real weight = amrex::Real(1.0) / std::pow(dist, power);
        
        sum_weights += weight;
        sum_weighted_z += weight * z_terr[i];
    }
    
    if (sum_weights > epsilon) {
        return sum_weighted_z / sum_weights;
    }
    
    return amrex::Real(0.0);  // Fallback
}

// ============================================================================
// Compute Gaussian concentration with image source for ground reflection
//
// Implements the image source method to satisfy zero-flux boundary condition
// at the ground surface. For each puff, a mirror puff is placed below ground.
//
// Inputs:
//   x, y, z          - Point coordinates [m]
//   puff             - Real puff structure
//   terrain_height   - Ground elevation at (x, y) [m]
//   use_image_source - Enable image source method
//
// Returns:
//   Total concentration from puff and image puff [units/m³]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real gaussian_puff_concentration_with_reflection(
    amrex::Real x, amrex::Real y, amrex::Real z,
    const Puff& puff,
    amrex::Real terrain_height,
    bool use_image_source = true)
{
    // Concentration from the actual puff
    amrex::Real conc_real = gaussian_puff_concentration(x, y, z, puff);
    
    if (!use_image_source) {
        return conc_real;
    }
    
    // Create image puff (mirror below ground)
    // Image puff position: z_image = 2*terrain_height - z_puff
    Puff image_puff = puff;
    image_puff.z = amrex::Real(2.0) * terrain_height - puff.z;
    
    // Concentration from the image puff
    amrex::Real conc_image = gaussian_puff_concentration(x, y, z, image_puff);
    
    return conc_real + conc_image;
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
// Advect puff with wind velocity and ground reflection
//
// Moves puff center with the local wind velocity using Euler time stepping:
//   x_new = x_old + u·dt
//
// If terrain reflection is enabled and puff goes below ground, the puff is
// reflected vertically and the vertical velocity is reversed.
//
// Inputs:
//   puff               - Puff to advect
//   u, v, w            - Wind velocity components [m/s]
//   dt                 - Time step [s]
//   terrain_height     - Ground elevation at puff location [m]
//   enable_reflection  - Enable ground reflection
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void advect_puff_with_terrain(
    Puff& puff,
    amrex::Real u,
    amrex::Real v,
    amrex::Real w,
    amrex::Real dt,
    amrex::Real terrain_height,
    bool enable_reflection = false)
{
    if (!puff.active || dt < amrex::Real(0.0)) {
        return;
    }
    
    // Advect puff
    puff.x += u * dt;
    puff.y += v * dt;
    puff.z += w * dt;
    
    // Ground reflection
    if (enable_reflection && puff.z < terrain_height) {
        // Reflect puff above ground: z_new = 2*terrain_height - z_old
        puff.z = amrex::Real(2.0) * terrain_height - puff.z;
        
        // Note: In a full implementation, we would also reverse the vertical
        // velocity component. For the simple puff model with prescribed wind,
        // the reflection of position is sufficient.
    }
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
// Modified to check against terrain surface instead of just z=0.
//
// Inputs:
//   puff             - Puff to check
//   xmin, xmax       - Domain bounds [m]
//   ymin, ymax
//   zmin, zmax
//   terrain_height   - Ground elevation at puff location [m] (optional)
//   check_terrain    - Check against terrain instead of zmin
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void check_puff_bounds_with_terrain(
    Puff& puff,
    amrex::Real xmin, amrex::Real xmax,
    amrex::Real ymin, amrex::Real ymax,
    amrex::Real zmin, amrex::Real zmax,
    amrex::Real terrain_height = amrex::Real(0.0),
    bool check_terrain = false)
{
    if (!puff.active) {
        return;
    }
    
    // Check horizontal bounds
    if (puff.x < xmin || puff.x > xmax ||
        puff.y < ymin || puff.y > ymax) {
        puff.active = false;
        return;
    }
    
    // Check vertical bounds
    if (check_terrain) {
        // Deactivate if significantly below terrain (allow small tolerance for numerical issues)
        const amrex::Real tolerance = amrex::Real(1.0);  // 1 meter tolerance
        if (puff.z < terrain_height - tolerance || puff.z > zmax) {
            puff.active = false;
        }
    } else {
        // Standard check against domain bounds
        if (puff.z < zmin || puff.z > zmax) {
            puff.active = false;
        }
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
