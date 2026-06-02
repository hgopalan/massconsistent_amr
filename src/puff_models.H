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
// //                             exp(-(dx²/(2σx²) + dy²/(2σy²) + dz²/(2σz²)))
//
// Enhancements:
// - Terrain awareness: ground reflection and image source method
// - Building awareness: collision detection and wake-enhanced diffusivity
// - Canopy integration: enhanced diffusivity and velocity reduction
//
// References:
// - Hanna, S.R., et al. (1982). Handbook on atmospheric diffusion. 
//   DOE/TIC-11223, U.S. Department of Energy
// - Pasquill, F., & Gifford, F.A. (1961). The estimation of the dispersion 
//   of wind-borne material from industrial sources. Meteorological Magazine.
// - Röckle, R. (1990). Building wake parameterization
// ============================================================================

#ifndef PUFF_MODELS_H
#define PUFF_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include "wake_models.H"
#include "canopy_models.H"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {
    using namespace amrex;
    
    // Minimum wind speed for plume rise computation [m/s]
    // Below this threshold, plume rise is not computed to avoid division by near-zero
    constexpr amrex::Real MIN_WIND_SPEED_FOR_PLUME_RISE = 0.1;
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
    amrex::Real K_h = 1.0;                   // Horizontal diffusivity [m²/s] (base value)
    amrex::Real K_v = 0.5;                   // Vertical diffusivity [m²/s] (base value)
    amrex::Real sigma_y0 = 1.0;              // Initial puff width σy [m]
    amrex::Real sigma_z0 = 1.0;              // Initial puff height σz [m]
    
    // Height-dependent diffusivity parameters (Feature 2)
    bool enable_height_dependent_K = false;  // Enable K(z) profiles
    std::string K_profile = "constant";      // Profile type: constant, power_law, stability
    amrex::Real K_power_law_exponent = 0.5;  // Exponent for power-law: K(z) = K0 * (z/z0)^n
    amrex::Real K_reference_height = 10.0;   // Reference height for power-law [m]
    
    // First-order decay parameters (Feature 3)
    bool enable_decay = false;               // Enable exponential decay
    amrex::Real decay_constant = 0.0;        // Decay constant λ [1/s] (e.g., 0.001 = 1e-3/s)
    
    // Plume rise parameters (Briggs buoyancy formula)
    bool enable_plume_rise = false;          // Enable plume rise for buoyant sources
    amrex::Real heat_flux = 0.0;             // Heat flux F [m⁴/s³] (buoyancy flux)
    
    bool enable_terrain_reflection = false;  // Enable ground reflection
    bool use_image_source = true;            // Use image source method for concentration
    bool enable_building_masking = false;    // Enable building collision detection
    bool enable_wake_diffusivity = false;    // Enable wake-enhanced diffusivity
    amrex::Real wake_enhancement_cavity = 3.0;  // Cavity zone K enhancement factor
    amrex::Real wake_enhancement_far = 1.5;     // Far wake K enhancement factor
    bool enable_canopy_effects = false;      // Enable canopy effects on dispersion
    amrex::Real canopy_enhancement_factor = 3.0;  // Canopy vertical diffusivity enhancement
    amrex::Real canopy_sheltering_factor = 0.8;   // Canopy horizontal diffusivity reduction
    bool enable_canopy_deposition = false;   // Enable dry deposition in canopy
    amrex::Real deposition_velocity = 0.01;  // Dry deposition velocity [m/s]
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
// Check if point is inside a building
//
// Inputs:
//   x, y, z          - Point coordinates [m]
//   building         - Building structure
//
// Returns:
//   True if point is inside building volume
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool point_in_building(
    amrex::Real x, amrex::Real y, amrex::Real z,
    const Building& building)
{
    return (x >= building.xmin && x <= building.xmax &&
            y >= building.ymin && y <= building.ymax &&
            z >= building.zmin && z <= building.zmax);
}

// ============================================================================
// Check if point is inside any building from a list
//
// Inputs:
//   x, y, z          - Point coordinates [m]
//   buildings        - Vector of Building structures
//
// Returns:
//   True if point is inside any building volume
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool point_in_any_building(
    amrex::Real x, amrex::Real y, amrex::Real z,
    const std::vector<Building>& buildings)
{
    for (const auto& building : buildings) {
        if (point_in_building(x, y, z, building)) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Determine if puff is in building wake zone and compute enhancement factor
//
// Uses simplified Röckle model to identify cavity and far wake zones.
// Returns diffusivity enhancement factor based on wake zone.
//
// Inputs:
//   puff_x, puff_y, puff_z - Puff position [m]
//   building               - Building structure
//   U_ref                  - Reference wind speed [m/s]
//   wind_dir_x, wind_dir_y - Wind direction components (unit vector)
//   wake_params            - Wake model parameters
//   K_enhancement_cavity   - Enhancement factor for cavity zone
//   K_enhancement_far      - Enhancement factor for far wake
//
// Returns:
//   Diffusivity enhancement factor (1.0 = no enhancement)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_wake_enhancement_factor(
    amrex::Real puff_x, amrex::Real puff_y, amrex::Real puff_z,
    const Building& building,
    amrex::Real U_ref,
    amrex::Real wind_dir_x, amrex::Real wind_dir_y,
    const WakeParams& wake_params,
    amrex::Real K_enhancement_cavity = amrex::Real(3.0),
    amrex::Real K_enhancement_far = amrex::Real(1.5))
{
    // Building center and dimensions
    const amrex::Real bldg_center_x = amrex::Real(0.5) * (building.xmin + building.xmax);
    const amrex::Real bldg_center_y = amrex::Real(0.5) * (building.ymin + building.ymax);
    const amrex::Real H = building.height;
    const amrex::Real W = building.width;
    const amrex::Real L = building.length;
    
    // Vector from building center to puff
    const amrex::Real dx = puff_x - bldg_center_x;
    const amrex::Real dy = puff_y - bldg_center_y;
    
    // Project onto wind direction to get downwind distance
    const amrex::Real x_wake = dx * wind_dir_x + dy * wind_dir_y;
    
    // Cross-wind distance
    const amrex::Real y_wake = -dx * wind_dir_y + dy * wind_dir_x;
    
    // Height above building base
    const amrex::Real z_local = puff_z - building.zmin;
    
    // Röckle cavity and far wake dimensions
    const amrex::Real Lr = wake_params.c1 * H;  // Cavity length
    const amrex::Real Hr = amrex::Real(0.67) * H;  // Cavity height
    const amrex::Real Lf = wake_params.separation_length * H;  // Far wake length
    
    // Check if in cavity zone (immediately behind building)
    if (x_wake > amrex::Real(0.0) && x_wake < Lr &&
        std::abs(y_wake) < amrex::Real(0.5) * W &&
        z_local > amrex::Real(0.0) && z_local < Hr) {
        return K_enhancement_cavity;  // Strong enhancement in cavity
    }
    
    // Check if in far wake zone
    if (x_wake > Lr && x_wake < Lf &&
        std::abs(y_wake) < amrex::Real(0.5) * W * (amrex::Real(1.0) + x_wake / Lf) &&
        z_local > amrex::Real(0.0) && z_local < amrex::Real(2.0) * H) {
        // Gradual enhancement that decreases with distance
        const amrex::Real decay_factor = amrex::Real(1.0) - (x_wake - Lr) / (Lf - Lr);
        return amrex::Real(1.0) + (K_enhancement_far - amrex::Real(1.0)) * decay_factor;
    }
    
    return amrex::Real(1.0);  // No enhancement outside wake
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
// Compute puff growth with wake-enhanced diffusivity
//
// Updates puff standard deviations with wake enhancement factor applied.
// Useful for modeling enhanced turbulent mixing in building wakes.
//
// Inputs:
//   puff                 - Puff to update
//   K_h                  - Base horizontal diffusivity [m²/s]
//   K_v                  - Base vertical diffusivity [m²/s]
//   wake_enhancement     - Enhancement factor (>1.0 in wakes)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void update_puff_growth_with_wake(
    Puff& puff,
    amrex::Real K_h,
    amrex::Real K_v,
    amrex::Real wake_enhancement = amrex::Real(1.0))
{
    // Apply wake enhancement to diffusivities
    const amrex::Real K_h_enhanced = K_h * wake_enhancement;
    const amrex::Real K_v_enhanced = K_v * wake_enhancement;
    
    // Call standard growth function with enhanced diffusivities
    update_puff_growth(puff, K_h_enhanced, K_v_enhanced);
}

// ============================================================================
// Compute canopy-enhanced diffusivity
//
// Canopy generates turbulence that enhances vertical mixing while providing
// sheltering that reduces horizontal diffusion.
//
// Inputs:
//   z_agl                      - Height above ground level [m]
//   canopy_height              - Canopy height [m]
//   K_h_base, K_v_base         - Base diffusivities [m²/s]
//   vertical_enhancement       - Vertical enhancement factor (2-5)
//   horizontal_sheltering      - Horizontal sheltering factor (0.5-1.0)
//
// Outputs:
//   K_h_eff, K_v_eff           - Effective diffusivities [m²/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_canopy_diffusivity(
    amrex::Real z_agl,
    amrex::Real canopy_height,
    amrex::Real K_h_base,
    amrex::Real K_v_base,
    amrex::Real vertical_enhancement,
    amrex::Real horizontal_sheltering,
    amrex::Real& K_h_eff,
    amrex::Real& K_v_eff)
{
    if (canopy_height <= amrex::Real(0.0) || z_agl >= canopy_height) {
        // Above canopy: use base diffusivities
        K_h_eff = K_h_base;
        K_v_eff = K_v_base;
        return;
    }
    
    // Within canopy: enhance vertical, reduce horizontal
    K_h_eff = K_h_base * horizontal_sheltering;
    K_v_eff = K_v_base * vertical_enhancement;
}

// ============================================================================
// Apply canopy deposition to puff mass
//
// Models dry deposition as first-order removal: dm/dt = -v_d * A * C
// where v_d is deposition velocity and A is canopy frontal area index.
//
// Inputs:
//   puff                 - Puff to update
//   dt                   - Time step [s]
//   z_agl                - Height above ground level [m]
//   canopy_height        - Canopy height [m]
//   frontal_area_index   - Canopy frontal area index (dimensionless)
//   deposition_velocity  - Dry deposition velocity [m/s]
//
// Note: This is a simplified model. Full implementation would compute
//       concentration-weighted deposition.
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_canopy_deposition(
    Puff& puff,
    amrex::Real dt,
    amrex::Real z_agl,
    amrex::Real canopy_height,
    amrex::Real frontal_area_index,
    amrex::Real deposition_velocity)
{
    if (!puff.active || puff.mass <= amrex::Real(0.0)) {
        return;
    }
    
    // Deposition only occurs within canopy
    if (z_agl < amrex::Real(0.0) || z_agl >= canopy_height) {
        return;
    }
    
    if (canopy_height <= amrex::Real(0.0) || frontal_area_index <= amrex::Real(0.0)) {
        return;
    }
    
    // Simplified deposition: exponential decay
    // Fraction deposited = 1 - exp(-v_d * A * dt / h_canopy)
    const amrex::Real decay_rate = deposition_velocity * frontal_area_index / canopy_height;
    const amrex::Real fraction_remaining = std::exp(-decay_rate * dt);
    
    puff.mass *= fraction_remaining;
    
    // Deactivate if mass becomes negligible
    if (puff.mass < amrex::Real(1.0e-10)) {
        puff.active = false;
    }
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
// Compute height-dependent diffusivity (Feature 2)
//
// Computes effective diffusivity as a function of height above ground.
//
// Profiles:
//   - constant: K(z) = K0 (default, no height dependence)
//   - power_law: K(z) = K0 * (z / z_ref)^n  (common in ABL, n ~ 0.5-1.0)
//   - stability: K(z) = K0 * (z / 10m)^0.5  (neutral conditions approximation)
//
// Inputs:
//   z_agl          - Height above ground level [m]
//   K_base         - Base diffusivity value [m²/s]
//   params         - Puff parameters with K_profile settings
//
// Returns:
//   Effective diffusivity at height z [m²/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_K_height_dependent(
    amrex::Real z_agl,
    amrex::Real K_base,
    const PuffParams& params)
{
    if (!params.enable_height_dependent_K) {
        return K_base;  // Constant diffusivity
    }
    
    // Ensure minimum height
    const amrex::Real z_safe = std::max(z_agl, amrex::Real(0.1));
    
    // Power-law profile: K(z) = K0 * (z / z_ref)^n
    const amrex::Real z_ref = std::max(params.K_reference_height, amrex::Real(1.0));
    const amrex::Real exponent = params.K_power_law_exponent;
    const amrex::Real ratio = z_safe / z_ref;
    const amrex::Real K_height = K_base * std::pow(ratio, exponent);
    
    return K_height;
}

// ============================================================================
// Apply first-order exponential decay to puff mass (Feature 3)
//
// Implements radioactive or chemical decay: dm/dt = -λ * m
// Solution: m(t) = m0 * exp(-λ * t)
//
// Inputs:
//   puff           - Puff structure (mass will be modified)
//   dt             - Time step [s]
//   decay_constant - Decay constant λ [1/s]
//
// Examples:
//   - λ = 0.001 /s  → half-life ~ 693 s (11.6 min)
//   - λ = 0.0001 /s → half-life ~ 6930 s (115.5 min)
//   - λ = ln(2)/T_half  for known half-life T_half
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_puff_decay(
    Puff& puff,
    amrex::Real dt,
    amrex::Real decay_constant)
{
    if (decay_constant <= amrex::Real(0.0)) {
        return;  // No decay
    }
    
    // Exponential decay: m(t+dt) = m(t) * exp(-λ * dt)
    const amrex::Real decay_factor = std::exp(-decay_constant * dt);
    puff.mass *= decay_factor;
    
    // Deactivate puff if mass becomes negligible
    if (puff.mass < amrex::Real(1.0e-12)) {
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
// Compute Briggs plume rise for buoyant sources
//
// Uses the Briggs (1975) formula for plume rise from buoyant sources.
// Similar to the approach used in WindNinja and QUIC-PLUME.
//
// Formula: Δh = (1.6 × F^1/3 × x^2/3) / u
//
// where:
//   F = buoyancy flux [m⁴/s³] = g × Q_H / (ρ × c_p × T)
//   x = downwind distance from source [m]
//   u = wind speed at source height [m/s]
//   Δh = plume rise height [m]
//
// For heat flux Q_H [W]:
//   F = g × Q_H / (ρ × c_p × T)
//   where g = 9.81 m/s², ρ ≈ 1.2 kg/m³, c_p ≈ 1005 J/(kg·K), T ≈ 288 K
//   Typical values: F = Q_H / (3.5 × 10⁵) for ambient conditions
//
// Inputs:
//   buoyancy_flux    - Buoyancy flux F [m⁴/s³]
//   downwind_dist    - Downwind distance x from source [m]
//   wind_speed       - Wind speed u at source height [m/s]
//
// Returns:
//   Plume rise height Δh [m]
//
// References:
//   - Briggs, G.A. (1975). Plume rise predictions. 
//     AMS Workshop on Meteorology and Environmental Assessment
//   - WindNinja and QUIC-PLUME implementation
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_plume_rise(
    amrex::Real buoyancy_flux,
    amrex::Real downwind_dist,
    amrex::Real wind_speed)
{
    // Avoid division by zero or negative values
    if (buoyancy_flux <= amrex::Real(0.0) || 
        downwind_dist <= amrex::Real(0.0) || 
        wind_speed <= MIN_WIND_SPEED_FOR_PLUME_RISE) {
        return amrex::Real(0.0);
    }
    
    // Briggs formula: Δh = 1.6 × F^(1/3) × x^(2/3) / u
    const amrex::Real F_third = std::pow(buoyancy_flux, amrex::Real(1.0) / amrex::Real(3.0));
    const amrex::Real x_twothirds = std::pow(downwind_dist, amrex::Real(2.0) / amrex::Real(3.0));
    const amrex::Real plume_rise = amrex::Real(1.6) * F_third * x_twothirds / wind_speed;
    
    return plume_rise;
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
