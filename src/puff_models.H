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
#include <AMReX_Array4.H>
#include <AMReX_Box.H>
#include <AMReX_Algorithm.H>
#include "wake_models.H"
#include "canopy_models.H"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

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

// Minimum wind speed for plume rise computation [m/s]
// Below this threshold, plume rise is not computed to avoid division by near-zero
static constexpr amrex::Real MIN_WIND_SPEED_FOR_PLUME_RISE = 0.1;

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
    
    // Height-dependent diffusivity parameters
    bool enable_height_dependent_K = false;  // Enable K(z) profiles
    std::string K_profile = "constant";      // Profile type: constant, power_law, stability
    amrex::Real K_power_law_exponent = 0.5;  // Exponent for power-law: K(z) = K0 * (z/z0)^n
    amrex::Real K_reference_height = 10.0;   // Reference height for power-law [m]
    
    // First-order decay parameters
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
    
    // Advanced building downwash parameters
    bool enable_cavity_trapping = false;     // Enable cavity trapping parameterization
    bool enable_plume_deformation = false;   // Enable plume deformation under shear
    amrex::Real aermod_prime_cavity_factor = 0.67;  // Cavity height factor (fraction of H)
    amrex::Real cavity_recirculation_strength = 0.8;  // Recirculation intensity [0-1]
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
    bool in_cavity = false;         // Whether puff is in building cavity zone
    amrex::Real cavity_age = 0.0;   // Time spent in cavity zone [s]
};

// ============================================================================
// Building Cavity Zone (AERMOD PRIME)
//
// Represents the building-induced cavity zone for advanced downwash modeling.
// The cavity zone is where recirculating flow traps and partially recirculates
// pollutants back toward and above the building.
// ============================================================================
struct CavityZone {
    amrex::Real xmin, xmax;         // Cavity bounds [m] (extends upwind and downwind)
    amrex::Real ymin, ymax;         // Lateral bounds [m]
    amrex::Real z_bottom, z_top;    // Cavity height bounds [m]
    amrex::Real recirculation_strength;  // Fraction of flow that recirculates [0-1]
    amrex::Real residence_time;     // Average residence time in cavity [s]
};

// ============================================================================
// Wake Recirculation Zone
//
// Represents the far-wake region where plume recovery occurs.
// Models the gradual mixing and dissipation of building-induced perturbations.
// ============================================================================
struct RecirculationZone {
    amrex::Real x_start, x_end;     // Downwind extent of recirculation [m]
    amrex::Real vortex_strength;    // Strength of rooftop vortex pair [m²/s]
    amrex::Real lateral_spread;     // Lateral spreading due to vortices [m]
    amrex::Real vertical_velocity_max;  // Maximum induced vertical velocity [m/s]
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
    (void)U_ref;
    // Building center and dimensions
    const amrex::Real bldg_center_x = amrex::Real(0.5) * (building.xmin + building.xmax);
    const amrex::Real bldg_center_y = amrex::Real(0.5) * (building.ymin + building.ymax);
    const amrex::Real H = building.height;
    const amrex::Real W = building.width;
    const amrex::Real L = building.length;
    (void)L;
    
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
// Compute height-dependent diffusivity
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
// Apply first-order exponential decay to puff mass
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
// Apply deposition/sedimentation for puffs near ground
//
// Removes mass from puffs that touch the ground due to:
//   1. Gravitational settling (particles with terminal velocity v_d)
//   2. Surface uptake (gases deposited by vegetation/surfaces)
//
// When puff center z < 3*σ_z (significant ground contact):
//   - Compute ground-level concentration C_ground
//   - Remove mass: Δm = C_ground × v_d × A_surface × Δt
//   - Update puff: m_new = m_old × (1 - deposition_fraction)
//
// Parameters:
//   puff               - Puff structure (mass will be modified)
//   dt                 - Time step [s]
//   terrain_height     - Ground elevation at puff location [m]
//   deposition_velocity - Deposition velocity v_d [m/s]
//                        Typical values:
//                        - Particles: 0.001-0.1 m/s (diameter-dependent)
//                        - SO2: 0.005-0.01 m/s
//                        - NO2: 0.002-0.006 m/s
//                        - O3: 0.003-0.008 m/s
//
// Method:
//   1. Check if puff touches ground: z_center < terrain + 3*σ_z
//   2. Compute ground concentration using Gaussian formula
//   3. Compute deposition flux: Φ = v_d × C_ground
//   4. Estimate effective surface area from puff footprint: A ≈ π × (2σ_y)²
//   5. Remove mass: Δm = Φ × A × Δt
//   6. Update puff mass: m_new = m_old - Δm
//
// References:
//   - Seinfeld & Pandis (2016). Atmospheric Chemistry and Physics, Ch. 19
//   - Wesely & Hicks (1977). Some factors that affect the deposition rates
//     of sulfur dioxide and similar gases on vegetation. J. Air Pollut. Control Assoc.
//   - CALPUFF modeling system documentation (deposition algorithms)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_puff_deposition(
    Puff& puff,
    amrex::Real dt,
    amrex::Real terrain_height,
    amrex::Real deposition_velocity)
{
    if (!puff.active || puff.mass <= amrex::Real(0.0)) {
        return;
    }
    
    if (deposition_velocity <= amrex::Real(0.0) || dt <= amrex::Real(0.0)) {
        return;  // No deposition
    }
    
    // Check if puff touches ground: z_center < terrain + 3*σ_z
    const amrex::Real threshold_height = terrain_height + amrex::Real(3.0) * puff.sigma_z;
    if (puff.z >= threshold_height) {
        return;  // Puff is too high above ground for deposition
    }
    
    // Compute ground-level concentration at puff center (x, y) location
    // Using Gaussian formula: C(x,y,z_ground) where z_ground = terrain_height
    const amrex::Real dz = terrain_height - puff.z;  // Vertical distance to ground
    
    // Avoid division by zero
    if (puff.sigma_y <= amrex::Real(1.0e-10) || puff.sigma_z <= amrex::Real(1.0e-10)) {
        return;
    }
    
    // Gaussian concentration at ground level directly below puff center
    // C_ground = (mass / ((2π)^(3/2) * σx * σy * σz)) * exp(-dz²/(2σz²))
    const amrex::Real pi = amrex::Real(3.14159265358979323846);
    const amrex::Real sigma_x = puff.sigma_y;  // Horizontal isotropy
    const amrex::Real sigma_y = puff.sigma_y;
    const amrex::Real sigma_z = puff.sigma_z;
    
    const amrex::Real exponent_z = -amrex::Real(0.5) * dz * dz / (sigma_z * sigma_z);
    
    // Avoid underflow
    if (exponent_z < amrex::Real(-100.0)) {
        return;  // Negligible ground concentration
    }
    
    const amrex::Real normalization = amrex::Real(1.0) / (
        std::pow(amrex::Real(2.0) * pi, amrex::Real(1.5)) * 
        sigma_x * sigma_y * sigma_z
    );
    
    const amrex::Real C_ground = puff.mass * normalization * std::exp(exponent_z);
    
    // Estimate effective surface area of puff footprint
    // Use conservative estimate: A ≈ π × (2σ_y)² (covers ~95% of horizontal Gaussian)
    const amrex::Real effective_area = pi * amrex::Real(4.0) * sigma_y * sigma_y;
    
    // Compute deposition flux: Φ = v_d × C_ground [units/m²/s]
    const amrex::Real deposition_flux = deposition_velocity * C_ground;
    
    // Mass removed during time step: Δm = Φ × A × Δt
    const amrex::Real mass_deposited = deposition_flux * effective_area * dt;
    
    // Update puff mass (cannot deposit more mass than exists)
    const amrex::Real mass_deposited_safe = std::min(mass_deposited, puff.mass);
    puff.mass -= mass_deposited_safe;
    
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
    puff.in_cavity = false;
    puff.cavity_age = amrex::Real(0.0);
    return puff;
}

// ============================================================================
// Compute building cavity zone geometry (AERMOD PRIME model)
//
// The cavity zone is a recirculation region downwind of a building where
// pollutants are trapped and partially recirculated. This function computes
// the cavity zone bounds using empirical correlations from AERMOD PRIME.
//
// Cavity zone parameters:
// - Height: H_cav = 0.67 × H (default cavity_factor)
// - Upwind extent: x_up = 0.5 × H
// - Downwind extent: x_down = 2.0 × H (approximately c1 × H where c1 ≈ 0.9)
// - Lateral extent: ±0.5 × W (half building width on each side)
//
// References:
// - Schulman, L.L., et al. (2000). Development and Evaluation of the PRIME
//   Plume Rise and Building Downwash Model. JAWMA, 50(3), 378-390.
// - EPA AERMOD User's Guide
//
// Inputs:
//   building            - Building structure with dimensions and position
//   cavity_factor       - Fraction of building height defining cavity (default 0.67)
//   upwind_factor       - Fraction of H for upwind extent (default 0.5)
//   downwind_factor     - Fraction of H for downwind extent (default 2.0)
//   wind_dir_x          - Wind direction x-component (unit vector)
//   wind_dir_y          - Wind direction y-component (unit vector)
//
// Returns:
//   CavityZone structure with bounds
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
CavityZone compute_cavity_zone(
    const Building& building,
    amrex::Real cavity_factor = 0.67,
    amrex::Real upwind_factor = 0.5,
    amrex::Real downwind_factor = 2.0,
    amrex::Real wind_dir_x = 1.0,
    amrex::Real wind_dir_y = 0.0)
{
    (void)wind_dir_x;
    (void)wind_dir_y;
    CavityZone cavity;
    
    // Cavity dimensions from AERMOD PRIME
    amrex::Real H = building.height;
    amrex::Real W = building.width;
    amrex::Real L = building.length;
    (void)L;
    
    // Cavity height: 0.67 × H above building base
    cavity.z_bottom = building.zmin;
    cavity.z_top = building.zmin + cavity_factor * H;
    
    // Cavity horizontal extent (in building reference frame)
    // For simplicity, use axis-aligned approach
    // Upwind: 0.5×H, Downwind: 2×H from building edges
    cavity.xmin = building.xmin - upwind_factor * H;
    cavity.xmax = building.xmax + downwind_factor * H;
    
    cavity.ymin = building.ymin - 0.5 * W;
    cavity.ymax = building.ymax + 0.5 * W;
    
    // Recirculation strength: fraction of incident flow that recirculates
    cavity.recirculation_strength = amrex::Real(0.8);  // 80% of mass stays in cavity
    
    // Residence time: estimate based on cavity volume and typical wind speed
    // τ_res ≈ L_cavity / U_typical
    // For 1 m/s wind: approximately 2-3 building heights of distance / 1 m/s ≈ 100-300 s
    cavity.residence_time = amrex::Real(150.0);  // Typical value [s]
    
    return cavity;
}

// ============================================================================
// Check if puff is in building cavity zone
//
// Determines if a puff center is within the cavity zone bounds.
//
// Inputs:
//   puff        - Puff to check
//   cavity      - Cavity zone structure
//
// Returns:
//   True if puff center is inside cavity zone
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool puff_in_cavity_zone(
    const Puff& puff,
    const CavityZone& cavity)
{
    return (puff.x >= cavity.xmin && puff.x <= cavity.xmax &&
            puff.y >= cavity.ymin && puff.y <= cavity.ymax &&
            puff.z >= cavity.z_bottom && puff.z <= cavity.z_top);
}

// ============================================================================
// Apply cavity trapping effect to puff
//
// Modifies puff behavior when in a building cavity zone:
// 1. Reduced horizontal diffusion (sheltering effect)
// 2. Enhanced vertical diffusion (vortex mixing)
// 3. Partial recirculation (puff drifts back toward building)
//
// Inputs/Outputs:
//   puff                - Puff to modify (in-place modification)
//   cavity              - Cavity zone structure
//   K_h, K_v            - Base diffusivity values [m²/s]
//   dt                  - Time step [s]
//   building_center_x   - Building center x-coordinate [m]
//
// Effects:
//   - K_h is reduced by factor of 0.5 (lateral sheltering)
//   - K_v is enhanced by factor of 2.0 (vertical vortex mixing)
//   - Puff drifts back toward building with recirculation velocity
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_cavity_trapping(
    Puff& puff,
    const CavityZone& cavity,
    amrex::Real& K_h_modified,
    amrex::Real& K_v_modified,
    amrex::Real dt,
    amrex::Real building_center_x,
    amrex::Real cavity_recirculation_strength = 0.8)
{
    (void)building_center_x;
    if (!puff_in_cavity_zone(puff, cavity)) {
        return;
    }
    
    puff.in_cavity = true;
    puff.cavity_age += dt;
    
    // Modify diffusivity in cavity
    K_h_modified = K_h_modified * amrex::Real(0.5);  // Reduced lateral diffusion
    K_v_modified = K_v_modified * amrex::Real(2.0);  // Enhanced vertical mixing
    
    // Recirculation effect: puff drifts back toward building center with reduced strength
    // This is a simplified model; real recirculation is more complex
    amrex::Real recirculation_velocity = -cavity_recirculation_strength * amrex::Real(0.5);  // m/s (negative = toward building)
    amrex::Real downwind_distance = puff.x - cavity.xmin;
    amrex::Real cavity_length = cavity.xmax - cavity.xmin;
    
    if (cavity_length > amrex::Real(0.0)) {
        // Recirculation decreases with distance from building
        amrex::Real recirculation_factor = 
            amrex::Real(1.0) - (downwind_distance / cavity_length);
        if (recirculation_factor > amrex::Real(0.0)) {
            puff.x += recirculation_velocity * recirculation_factor * dt;
        }
    }
}

// ============================================================================
// Compute plume deformation under vertical wind shear
//
// Implements plume tilting and deformation due to vertical variation in
// horizontal wind speed. When lower layers move slower than upper layers,
// the plume tilts downwind.
//
// Deformation model:
// 1. Compute local wind shear: dU/dz
// 2. Calculate tilt angle: θ = arctan(dU/dz × Δt)
// 3. Deform puff: stretch in downwind direction, compress in upwind
//
// References:
// - Anfossi, D., et al. (2005). A new Eulerian model of dispersion in
//   complex terrain: the role of wind deformation.
// - Venkatram, A. (1993). Revisiting the Gaussian puff model.
//
// Inputs:
//   puff            - Puff to deform
//   wind_speed_z    - Wind speed at puff height [m/s]
//   wind_speed_z_ref - Wind speed at reference height [m/s]
//   puff_z          - Puff vertical position [m]
//   z_ref           - Reference height [m]
//   z_exponent      - Wind profile exponent (power law) [0.1-0.5]
//   dt              - Time step [s]
//   max_deformation - Maximum deformation factor [dimensionless]
//
// Outputs:
//   puff.sigma_y    - Modified horizontal standard deviation
//   puff.sigma_z    - Modified vertical standard deviation
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_plume_deformation_shear(
    Puff& puff,
    amrex::Real wind_speed_z,
    amrex::Real wind_speed_z_ref,
    amrex::Real puff_z,
    amrex::Real z_ref,
    amrex::Real z_exponent,
    amrex::Real dt,
    amrex::Real max_deformation = 2.0)
{
    (void)wind_speed_z;
    if (puff_z <= amrex::Real(0.0) || z_ref <= amrex::Real(0.0)) {
        return;
    }
    
    // Compute wind shear: dU/dz using power law profile
    // U(z) = U_ref × (z/z_ref)^p  ⟹  dU/dz = U_ref × p × (z/z_ref)^(p-1) / z_ref
    amrex::Real wind_shear = amrex::Real(0.0);
    if (wind_speed_z_ref > MIN_WIND_SPEED_FOR_PLUME_RISE) {
        amrex::Real z_ratio = puff_z / z_ref;
        wind_shear = wind_speed_z_ref * z_exponent * 
                     std::pow(z_ratio, z_exponent - amrex::Real(1.0)) / z_ref;
    }
    
    // Tilt angle: θ = arctan(wind_shear × dt)
    amrex::Real tilt_angle = std::atan(wind_shear * dt);
    
    // Deformation factor based on tilt
    amrex::Real deformation_factor = 
        amrex::Real(1.0) + std::abs(tilt_angle) * amrex::Real(0.5);
    
    // Clamp deformation to avoid unrealistic values
    if (deformation_factor > max_deformation) {
        deformation_factor = max_deformation;
    }
    
    // Apply deformation: increase horizontal spread, decrease vertical spread
    // (plume stretches horizontally and compresses vertically in strong shear)
    puff.sigma_y = puff.sigma_y * deformation_factor;
    puff.sigma_z = puff.sigma_z / (amrex::Real(1.0) + amrex::Real(0.1) * tilt_angle);
}

// ============================================================================
// Compute wake recirculation zone for far-wake mixing
//
// Identifies the far-wake region where rooftop vortex pairs create complex
// mixing patterns. This function characterizes the vortex strength and
// induced velocities.
//
// Far-wake characteristics:
// - Extends from ~2H to ~7H downwind of building
// - Two counter-rotating vortex pair (CVP) induced by flow separation
// - Vortex strength proportional to building height and wind speed
// - Creates lateral and vertical mixing enhancement
//
// Inputs:
//   building        - Building structure
//   wind_speed      - Approach wind speed at building height [m/s]
//
// Returns:
//   RecirculationZone structure with vortex characteristics
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
RecirculationZone compute_recirculation_zone(
    const Building& building,
    amrex::Real wind_speed)
{
    RecirculationZone rz;
    
    amrex::Real H = building.height;
    amrex::Real W = building.width;
    
    // Far-wake extent: starts at ~2H, ends at ~7H downwind
    rz.x_start = building.xmax + amrex::Real(2.0) * H;
    rz.x_end = building.xmax + amrex::Real(7.0) * H;
    
    // Vortex strength (Γ) proportional to wind speed and building size
    // Γ = Γ₀ × U × H, where Γ₀ is a dimensionless coefficient (~0.1-0.3)
    rz.vortex_strength = amrex::Real(0.15) * wind_speed * H;
    
    // Lateral spreading in wake: grows with distance from building
    // Maximum spread approximately ±0.5W at far edge
    rz.lateral_spread = amrex::Real(0.25) * W;
    
    // Maximum induced vertical velocity in vortex core
    // w_max ≈ Γ / (2π × r_core), where r_core ≈ 0.1W (vortex core radius)
    amrex::Real r_core = amrex::Real(0.1) * W;
    if (r_core > amrex::Real(0.01)) {
        rz.vertical_velocity_max = rz.vortex_strength / (amrex::Real(2.0) * amrex::Real(3.14159265) * r_core);
    } else {
        rz.vertical_velocity_max = amrex::Real(0.0);
    }
    
    return rz;
}

#endif // PUFF_MODELS_H
