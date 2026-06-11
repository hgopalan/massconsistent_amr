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
#include "dispersion_interpolation.H"
#include "wake_models.H"
#include "canopy_models.H"
#include "stability_models.H"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <limits>

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
    bool enable_capping_lid = false;         // Enable atmospheric inversion capping lid
    amrex::Real capping_lid_height = 1000.0; // Inversion capping lid height [m]
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

    // Gravitational settling parameters
    bool enable_settling = false;            // Enable terminal settling velocity
    amrex::Real particle_density = 1000.0;   // Particle density [kg/m³]
    amrex::Real particle_diameter = 10.0e-6; // Particle diameter [m]
    amrex::Real air_viscosity = 1.8e-5;      // Air dynamic viscosity [Pa·s]
    amrex::Real gravity = 9.81;              // Acceleration of gravity [m/s²]

    // Ground Dry deposition parameter (for both puffs and particles)
    bool enable_puff_deposition = false;     // Enable ground dry deposition
    
    // Wet deposition / Precipitation scavenging parameters
    bool enable_wet_deposition = false;      // Enable wet deposition
    amrex::Real scavenging_coeff_base = 1.0e-4; // Base scavenging coefficient Lambda0 [s^-1]
    amrex::Real precipitation_rate = 1.0;    // Precipitation rate P [mm/hr]
    amrex::Real scavenging_exponent = 0.8;   // Scavenging exponent a

    // Ambient-condition-driven chemical decay parameters
    bool enable_dynamic_decay = false;       // Replace constant decay with ambient-driven decay
    amrex::Real temp_ref = 298.15;           // Reference temperature [K] (25 C)
    amrex::Real temp_coeff = 0.04;           // Temperature sensitivity coefficient (e.g. 0.04 K^-1)
    amrex::Real rh_ref = 50.0;               // Reference relative humidity [%]
    amrex::Real rh_coeff = 0.005;            // Relative humidity sensitivity coefficient
    amrex::Real solar_ref = 500.0;           // Reference solar radiation [W/m^2]
    amrex::Real solar_coeff = 1.0;           // Solar radiation sensitivity coefficient
    amrex::Real ambient_temp = 298.15;       // Ambient temperature [K]
    amrex::Real ambient_rh = 50.0;           // Ambient relative humidity [%]
    amrex::Real ambient_solar = 500.0;       // Ambient solar radiation [W/m^2]
    
    // Wind field input configuration
    std::string wind_field_file = "";        // Path to wind field CSV file (optional)
    std::string wind_field_format = "uniform"; // Format: "uniform", "gridded", "timeseries"
    bool enable_unsteady_wind = false;       // Enable time-varying wind fields
    amrex::Real wind_field_start_time = 0.0; // Start time for wind field [s]
    
    // Multi-source configuration (Phase 2.1)
    std::string sources_file = "";           // Path to CSV with multiple sources (optional)
    std::vector<Source> sources;             // Vector of emission sources (Phase 2.1)
    
    // Meteorological profile configuration (Phase 2.3)
    std::string met_profile_file = "";       // Path to meteorological profile CSV (optional)
    bool enable_spatial_met = false;         // Enable spatially-varying meteorology
    std::vector<MetProfile> met_profiles;    // Vector of meteorological profiles (Phase 2.3)
    
    // Stack aerodynamic modeling parameters (Phase 2.2)
    bool stack_tip_downwash_enabled = false; // Enable Briggs stack tip downwash (STD)
    bool briggs_std_model = true;            // Use Briggs STD model (vs. alternative models)
};

// ============================================================================
// Source Structure (Phase 2.1)
// Represents a single emission source with optional stack parameters
// ============================================================================
struct Source {
    std::string source_id = "source_1";      // Source identifier
    amrex::Real x = 0.0;                     // Source x-coordinate [m]
    amrex::Real y = 0.0;                     // Source y-coordinate [m]
    amrex::Real z = 10.0;                    // Source height [m]
    amrex::Real emission_rate = 1.0;         // Emission rate [units/s]
    amrex::Real emission_duration = 100.0;   // Duration of emission [s]
    std::string type = "point";              // Source type: "point", "volume", "stack"
    
    // Stack aerodynamic parameters (Phase 2.2)
    amrex::Real stack_diameter = 0.0;        // Stack diameter [m] (0 = no stack model)
    amrex::Real stack_exit_velocity = 0.0;   // Stack exit velocity [m/s]
    amrex::Real stack_exit_temperature = 298.15; // Stack exit temperature [K]
};

// ============================================================================
// Meteorological Profile Structure (Phase 2.3)
// Represents a spatial profile of wind and diffusivity
// ============================================================================
struct MetProfile {
    std::string profile_id = "profile_1";    // Profile identifier
    amrex::Real x_ref = 0.0;                 // Profile reference x-coordinate [m]
    amrex::Real y_ref = 0.0;                 // Profile reference y-coordinate [m]
    std::vector<amrex::Real> z_agl;          // Heights above ground level [m]
    std::vector<amrex::Real> u;              // Wind u-component [m/s]
    std::vector<amrex::Real> v;              // Wind v-component [m/s]
    std::vector<amrex::Real> w;              // Wind w-component [m/s]
    std::vector<amrex::Real> K_h;            // Horizontal diffusivity [m²/s]
    std::vector<amrex::Real> K_v;            // Vertical diffusivity [m²/s]
    std::string stability_class = "D";       // Stability class (A-F or "neutral")
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
    static constexpr int NUM_SPECIES = 5;
    amrex::Real species_mass[NUM_SPECIES] = {0.0, 0.0, 0.0, 0.0, 0.0};
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
AMREX_INLINE
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
    bool use_image_source = true,
    bool enable_capping_lid = false,
    amrex::Real capping_lid_height = 1000.0)
{
    // Concentration from the actual puff
    amrex::Real conc_real = gaussian_puff_concentration(x, y, z, puff);
    
    amrex::Real conc_image = amrex::Real(0.0);
    
    if (use_image_source) {
        // Create image puff (mirror below ground)
        // Image puff position: z_image = 2*terrain_height - z_puff
        Puff image_puff = puff;
        image_puff.z = amrex::Real(2.0) * terrain_height - puff.z;
        
        // Concentration from the image puff
        conc_image += gaussian_puff_concentration(x, y, z, image_puff);
    }
    
    if (enable_capping_lid && use_image_source) {
        // Create image puff (mirror above capping lid)
        // Physical height of capping lid = terrain_height + capping_lid_height
        amrex::Real z_lid = terrain_height + capping_lid_height;
        Puff image_puff_lid = puff;
        image_puff_lid.z = amrex::Real(2.0) * z_lid - puff.z;
        
        // Concentration from the capping lid image puff
        conc_image += gaussian_puff_concentration(x, y, z, image_puff_lid);
    }
    
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
AMREX_INLINE
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
//   v_s                - Terminal settling velocity [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void advect_puff_with_terrain(
    Puff& puff,
    amrex::Real u,
    amrex::Real v,
    amrex::Real w,
    amrex::Real dt,
    amrex::Real terrain_height,
    bool enable_reflection = false,
    amrex::Real v_s = amrex::Real(0.0),
    bool enable_capping_lid = false,
    amrex::Real capping_lid_height = 1000.0)
{
    if (!puff.active || dt < amrex::Real(0.0)) {
        return;
    }
    
    // Advect puff
    puff.x += u * dt;
    puff.y += v * dt;
    puff.z += (w - v_s) * dt;
    
    // Ground reflection
    if (enable_reflection && puff.z < terrain_height) {
        // Reflect puff above ground: z_new = 2*terrain_height - z_old
        puff.z = amrex::Real(2.0) * terrain_height - puff.z;
        
        // Note: In a full implementation, we would also reverse the vertical
        // velocity component. For the simple puff model with prescribed wind,
        // the reflection of position is sufficient.
    }

    // Capping lid reflection
    if (enable_capping_lid) {
        amrex::Real z_lid = terrain_height + capping_lid_height;
        if (puff.z > z_lid) {
            puff.z = amrex::Real(2.0) * z_lid - puff.z;
        }
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
//   v_s              - Terminal settling velocity [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void advect_puff(
    Puff& puff,
    amrex::Real u,
    amrex::Real v,
    amrex::Real w,
    amrex::Real dt,
    amrex::Real v_s = amrex::Real(0.0),
    bool enable_capping_lid = false,
    amrex::Real capping_lid_height = 1000.0)
{
    if (!puff.active || dt < amrex::Real(0.0)) {
        return;
    }
    
    puff.x += u * dt;
    puff.y += v * dt;
    puff.z += (w - v_s) * dt;

    // Capping lid reflection (with terrain_height = 0.0)
    if (enable_capping_lid) {
        amrex::Real z_lid = capping_lid_height;
        if (puff.z > z_lid) {
            puff.z = amrex::Real(2.0) * z_lid - puff.z;
        }
    }
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
// Compute terminal settling velocity via Stokes' Law
//
// Formula: v_s = (rho_p * d_p^2 * g) / (18 * mu)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_settling_velocity(
    amrex::Real particle_density,
    amrex::Real particle_diameter,
    amrex::Real gravity,
    amrex::Real air_viscosity)
{
    if (air_viscosity <= amrex::Real(0.0)) {
        return amrex::Real(0.0);
    }
    return (particle_density * particle_diameter * particle_diameter * gravity) / 
           (amrex::Real(18.0) * air_viscosity);
}

// ============================================================================
// Compute dynamic chemical decay constant
//
// Updates decay constant based on temperature (lapse rate), relative humidity,
// and solar radiation (canopy attenuation).
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_dynamic_decay_constant(
    amrex::Real z_agl,
    amrex::Real base_decay,
    amrex::Real ambient_temp,
    amrex::Real ambient_rh,
    amrex::Real ambient_solar,
    amrex::Real temp_ref,
    amrex::Real temp_coeff,
    amrex::Real rh_ref,
    amrex::Real rh_coeff,
    amrex::Real solar_ref,
    amrex::Real solar_coeff,
    bool enable_canopy_effects = false,
    amrex::Real canopy_height = 0.0,
    amrex::Real frontal_area_index = 0.0)
{
    if (base_decay <= amrex::Real(0.0)) {
        return amrex::Real(0.0);
    }

    // 1. Temperature height adjustment (standard lapse rate 0.0065 K/m)
    amrex::Real T_z = ambient_temp;
    if (z_agl > amrex::Real(0.0)) {
        T_z = std::max(amrex::Real(200.0), ambient_temp - amrex::Real(0.0065) * z_agl);
    }

    // 2. Solar radiation canopy attenuation
    amrex::Real I_z = ambient_solar;
    if (enable_canopy_effects && canopy_height > amrex::Real(0.0) && z_agl < canopy_height) {
        amrex::Real z_safe = std::max(amrex::Real(0.0), z_agl);
        amrex::Real attenuation = std::exp(-amrex::Real(0.5) * frontal_area_index * (amrex::Real(1.0) - z_safe / canopy_height));
        I_z *= attenuation;
    }

    // 3. Compute scaling factors
    // Temperature factor (exponential Arrhenius-like scaling)
    amrex::Real f_T = std::exp(temp_coeff * (T_z - temp_ref));

    // Humidity factor (linear scaling around reference RH, clamped to be positive)
    amrex::Real f_RH = amrex::Real(1.0) + rh_coeff * (ambient_rh - rh_ref);
    if (f_RH < amrex::Real(0.1)) f_RH = amrex::Real(0.1);

    // Solar radiation factor (photolysis increases decay rate, proportional to light intensity)
    amrex::Real f_solar = amrex::Real(1.0) + solar_coeff * (I_z / std::max(solar_ref, amrex::Real(1.0e-3)));
    if (f_solar < amrex::Real(0.0)) f_solar = amrex::Real(0.0);

    return base_decay * f_T * f_RH * f_solar;
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
    for (int i = 0; i < Puff::NUM_SPECIES; ++i) puff.species_mass[i] = amrex::Real(0.0);
    puff.species_mass[0] = mass;
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

// ============================================================================
// Compute analytical dispersion coefficients (Pasquill-Gifford and McElroy-Pooler)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_analytical_dispersion(
    amrex::Real x, // downwind distance in meters
    int pg_class,  // 0: A, 1: B, 2: C, 3: D, 4: E, 5: F
    bool is_urban,
    amrex::Real& sigma_y,
    amrex::Real& sigma_z)
{
    x = std::max(x, amrex::Real(1.0));
    
    if (is_urban) {
        // McElroy-Pooler (urban) formulas (Briggs, 1973)
        if (pg_class <= 1) { // A-B (unstable)
            sigma_y = amrex::Real(0.32) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0004) * x);
            sigma_z = amrex::Real(0.24) * x * std::sqrt(amrex::Real(1.0) + amrex::Real(0.001) * x);
        } else if (pg_class == 2) { // C (slightly unstable)
            sigma_y = amrex::Real(0.22) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0004) * x);
            sigma_z = amrex::Real(0.20) * x;
        } else if (pg_class == 3) { // D (neutral)
            sigma_y = amrex::Real(0.16) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0004) * x);
            sigma_z = amrex::Real(0.14) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0003) * x);
        } else { // E-F (stable)
            sigma_y = amrex::Real(0.11) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0004) * x);
            sigma_z = amrex::Real(0.08) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.00015) * x);
        }
    } else {
        // Pasquill-Gifford (rural) formulas (Briggs, 1973)
        if (pg_class == 0) { // A (very unstable)
            sigma_y = amrex::Real(0.22) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0001) * x);
            sigma_z = amrex::Real(0.20) * x;
        } else if (pg_class == 1) { // B (moderately unstable)
            sigma_y = amrex::Real(0.16) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0001) * x);
            sigma_z = amrex::Real(0.12) * x;
        } else if (pg_class == 2) { // C (slightly unstable)
            sigma_y = amrex::Real(0.11) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0001) * x);
            sigma_z = amrex::Real(0.08) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0002) * x);
        } else if (pg_class == 3) { // D (neutral)
            sigma_y = amrex::Real(0.08) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0001) * x);
            sigma_z = amrex::Real(0.06) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0015) * x);
        } else if (pg_class == 4) { // E (slightly stable)
            sigma_y = amrex::Real(0.06) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0001) * x);
            sigma_z = amrex::Real(0.03) * x / (amrex::Real(1.0) + amrex::Real(0.0003) * x);
        } else { // F (moderately stable)
            sigma_y = amrex::Real(0.04) * x / std::sqrt(amrex::Real(1.0) + amrex::Real(0.0001) * x);
            sigma_z = amrex::Real(0.016) * x / (amrex::Real(1.0) + amrex::Real(0.0003) * x);
        }
    }
}

// ============================================================================
// Compute Wesely Dry Deposition Velocity
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_wesely_deposition_velocity(
    amrex::Real z_agl,
    amrex::Real z0,
    amrex::Real u_star,
    amrex::Real L_obukhov,
    amrex::Real molecular_diffusivity,
    amrex::Real air_viscosity,
    amrex::Real base_surface_resistance,
    amrex::Real terminal_velocity = amrex::Real(0.0))
{
    if (u_star < amrex::Real(1.0e-5)) return amrex::Real(0.0);
    
    amrex::Real r_a = std::log(std::max(z_agl, amrex::Real(0.1)) / std::max(z0, amrex::Real(1.0e-4))) / (amrex::Real(0.41) * u_star);
    if (std::abs(L_obukhov) > amrex::Real(1.0e-10)) {
        amrex::Real zeta = z_agl / L_obukhov;
        r_a = r_a - psi_m(zeta) / (amrex::Real(0.41) * u_star);
        r_a = std::max(r_a, amrex::Real(1.0));
    }
    
    amrex::Real Sc = amrex::Real(0.7);
    if (molecular_diffusivity > amrex::Real(0.0) && air_viscosity > amrex::Real(0.0)) {
        Sc = air_viscosity / molecular_diffusivity;
    }
    amrex::Real r_b = amrex::Real(5.0) * std::pow(Sc, amrex::Real(2.0)/amrex::Real(3.0)) / u_star;
    
    amrex::Real r_c = base_surface_resistance;
    if (r_c <= amrex::Real(0.0)) r_c = amrex::Real(100.0);
    
    amrex::Real total_resistance = r_a + r_b + r_c;
    if (total_resistance <= amrex::Real(0.0)) return amrex::Real(0.0);
    
    return amrex::Real(1.0) / total_resistance + terminal_velocity;
}

// ============================================================================
// Compute Wet Scavenging Coefficient
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_wet_scavenging_coeff(
    int species_idx,
    amrex::Real precipitation_rate,
    bool is_snow = false,
    amrex::Real scavenging_coeff_base = amrex::Real(1.0e-4),
    amrex::Real scavenging_exponent = amrex::Real(0.8))
{
    if (precipitation_rate <= amrex::Real(0.0)) return amrex::Real(0.0);
    
    amrex::Real A = scavenging_coeff_base;
    amrex::Real B = scavenging_exponent;
    
    if (species_idx == 3) {
        A *= amrex::Real(3.0);
    } else if (species_idx == 2) {
        A *= amrex::Real(0.1);
    } else if (species_idx == 1 || species_idx == 4) {
        A *= amrex::Real(1.5);
    }
    
    if (is_snow) {
        if (species_idx == 0 || species_idx == 2 || species_idx == 3) {
            A *= amrex::Real(0.2);
        } else {
            A *= amrex::Real(1.2);
        }
    }
    
    return A * std::pow(precipitation_rate, B);
}

// ============================================================================
// Compute Elevated Inversion Penetration Fraction
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_inversion_penetration_fraction(
    amrex::Real puff_z,
    amrex::Real sigma_z,
    amrex::Real capping_lid_height)
{
    if (capping_lid_height <= amrex::Real(0.0) || sigma_z <= amrex::Real(1.0e-10)) {
        return amrex::Real(0.0);
    }
    
    amrex::Real arg = (capping_lid_height - puff_z) / (sigma_z * std::sqrt(amrex::Real(2.0)));
    amrex::Real fraction_penetrated = amrex::Real(0.5) * std::erfc(arg);
    return std::max(amrex::Real(0.0), std::min(amrex::Real(1.0), fraction_penetrated));
}

// ============================================================================
// Compute Visibility Metrics (IMPROVE algorithm)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_visibility_metrics(
    amrex::Real sulfate_conc,
    amrex::Real nitrate_conc,
    amrex::Real rh,
    amrex::Real& b_ext,
    amrex::Real& visual_range,
    amrex::Real& deciview)
{
    amrex::Real b_ray = amrex::Real(10.0);
    amrex::Real f_rh = amrex::Real(1.0);
    if (rh > amrex::Real(0.0) && rh < amrex::Real(100.0)) {
        f_rh = amrex::Real(1.0) + amrex::Real(0.12) * std::pow(rh / (amrex::Real(100.0) - rh), amrex::Real(0.6));
    }
    
    b_ext = amrex::Real(3.0) * f_rh * sulfate_conc + amrex::Real(3.0) * f_rh * nitrate_conc + b_ray;
    visual_range = amrex::Real(3912.0) / b_ext;
    deciview = amrex::Real(10.0) * std::log(b_ext / amrex::Real(10.0));
}

// ============================================================================
// Compute Steam-Plume Fogging and Icing Probability
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_fog_icing_probability(
    amrex::Real temp,
    amrex::Real rh,
    amrex::Real& fog_prob,
    amrex::Real& icing_prob)
{
    if (rh > amrex::Real(80.0)) {
        fog_prob = (rh - amrex::Real(80.0)) / amrex::Real(20.0);
    } else {
        fog_prob = amrex::Real(0.0);
    }
    fog_prob = std::max(amrex::Real(0.0), std::min(amrex::Real(1.0), fog_prob));
    
    if (temp < amrex::Real(273.15) && rh > amrex::Real(80.0)) {
        icing_prob = (rh - amrex::Real(80.0)) / amrex::Real(20.0);
    } else {
        icing_prob = amrex::Real(0.0);
    }
    icing_prob = std::max(amrex::Real(0.0), std::min(amrex::Real(1.0), icing_prob));
}

// ============================================================================
// Apply first-order multi-species chemical conversion (MESOPUFF II kinetics)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_chemical_transformation(
    amrex::Real* species_mass,
    amrex::Real dt,
    amrex::Real k1 = amrex::Real(0.01 / 3600.0),
    amrex::Real k2 = amrex::Real(0.02 / 3600.0),
    amrex::Real k3 = amrex::Real(0.05 / 3600.0))
{
    amrex::Real d_SO2 = species_mass[0] * (amrex::Real(1.0) - std::exp(-k1 * dt));
    species_mass[0] -= d_SO2;
    species_mass[1] += d_SO2 * amrex::Real(1.5);
    
    amrex::Real d_NOx = species_mass[2] * (amrex::Real(1.0) - std::exp(-k2 * dt));
    species_mass[2] -= d_NOx;
    species_mass[3] += d_NOx * amrex::Real(1.35);
    
    amrex::Real d_HNO3 = species_mass[3] * (amrex::Real(1.0) - std::exp(-k3 * dt));
    species_mass[3] -= d_HNO3;
    species_mass[4] += d_HNO3 * amrex::Real(0.98);
}

// ============================================================================
// Wind Field Data Structures
// ============================================================================

// Uniform wind field: single (u, v, w) for entire domain and time
struct UniformWindField {
    amrex::Real u, v, w;    // Wind components [m/s]
};

// Gridded wind field: wind values at discrete grid points
struct GriddedWindPoint {
    amrex::Real x, y, z;    // Grid coordinates [m]
    amrex::Real u, v, w;    // Wind components [m/s]
};

// Time-series wind field: wind values at discrete time steps
struct TimeSeriesWindPoint {
    amrex::Real time;       // Time [s]
    amrex::Real u, v, w;    // Wind components [m/s]
};

// ============================================================================
// Wind Field Input/Output Functions
// ============================================================================

/**
 * Read wind field from CSV file.
 * 
 * Supports three formats:
 *   - uniform: Single wind values for entire domain (u, v, w)
 *   - gridded: Wind at grid points (x, y, z, u, v, w)
 *   - timeseries: Wind at time steps (time, u, v, w)
 * 
 * Parameters:
 *   csv_file - Path to CSV wind field file
 *   wind_uniform - Output for uniform wind format
 *   wind_gridded - Output for gridded format
 *   wind_timeseries - Output for time-series format
 *   format_out - Format detected from file ("uniform", "gridded", "timeseries")
 * 
 * Returns:
 *   true if successfully read, false if file not found or parsing error
 */
inline bool read_wind_field_csv(
    const std::string& csv_file,
    UniformWindField& wind_uniform,
    std::vector<GriddedWindPoint>& wind_gridded,
    std::vector<TimeSeriesWindPoint>& wind_timeseries,
    std::string& format_out)
{
    std::ifstream infile(csv_file);
    if (!infile.is_open()) {
        amrex::Print() << "Warning: Could not open wind field file " << csv_file << "\n";
        return false;
    }
    
    std::string line;
    format_out = "unknown";
    bool found_format = false;
    bool found_data = false;
    int ncols = 0;
    
    while (std::getline(infile, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            // Parse format from comment if present
            if (line.find("Format:") != std::string::npos) {
                if (line.find("uniform") != std::string::npos) {
                    format_out = "uniform";
                    found_format = true;
                } else if (line.find("gridded") != std::string::npos) {
                    format_out = "gridded";
                    found_format = true;
                } else if (line.find("timeseries") != std::string::npos) {
                    format_out = "timeseries";
                    found_format = true;
                }
            }
            continue;
        }
        
        // Count columns from header line
        if (!found_data && (line.find("x") != std::string::npos || 
                           line.find("u") != std::string::npos ||
                           line.find("time") != std::string::npos)) {
            std::istringstream iss(line);
            std::string col;
            ncols = 0;
            while (std::getline(iss, col, ',')) {
                if (!col.empty()) ncols++;
            }
            // Infer format if not yet determined
            if (!found_format) {
                if (ncols == 3) format_out = "uniform";
                else if (ncols == 4) format_out = "timeseries";
                else if (ncols == 6) format_out = "gridded";
            }
            found_data = true;
            continue;
        }
        
        if (!found_data) continue;
        
        // Parse data line
        std::istringstream iss(line);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(iss, field, ',')) {
            if (!field.empty()) fields.push_back(field);
        }
        
        if (fields.size() < 3) continue;
        
        try {
            if (format_out == "uniform" && fields.size() >= 3) {
                wind_uniform.u = std::stod(fields[0]);
                wind_uniform.v = std::stod(fields[1]);
                wind_uniform.w = std::stod(fields[2]);
            }
            else if (format_out == "timeseries" && fields.size() >= 4) {
                TimeSeriesWindPoint pt;
                pt.time = std::stod(fields[0]);
                pt.u = std::stod(fields[1]);
                pt.v = std::stod(fields[2]);
                pt.w = std::stod(fields[3]);
                wind_timeseries.push_back(pt);
            }
            else if (format_out == "gridded" && fields.size() >= 6) {
                GriddedWindPoint pt;
                pt.x = std::stod(fields[0]);
                pt.y = std::stod(fields[1]);
                pt.z = std::stod(fields[2]);
                pt.u = std::stod(fields[3]);
                pt.v = std::stod(fields[4]);
                pt.w = std::stod(fields[5]);
                wind_gridded.push_back(pt);
            }
        } catch (...) {
            // Skip lines with parsing errors
            continue;
        }
    }
    
    infile.close();
    
    if (format_out == "unknown") {
        amrex::Print() << "Warning: Could not determine wind field format from " << csv_file << "\n";
        return false;
    }
    
    if (format_out == "uniform") {
        amrex::Print() << "Read uniform wind field from " << csv_file 
                      << " (u=" << wind_uniform.u << ", v=" << wind_uniform.v 
                      << ", w=" << wind_uniform.w << ")\n";
    } else if (format_out == "gridded") {
        amrex::Print() << "Read gridded wind field from " << csv_file 
                      << " (" << wind_gridded.size() << " grid points)\n";
    } else if (format_out == "timeseries") {
        amrex::Print() << "Read time-series wind field from " << csv_file 
                      << " (" << wind_timeseries.size() << " time steps)\n";
    }
    
    return true;
}

/**
 * Interpolate wind vector at point (x, y, z) from gridded wind field.
 * Uses nearest-neighbor or linear interpolation.
 * 
 * Parameters:
 *   x, y, z - Query point coordinates [m]
 *   wind_gridded - Vector of gridded wind points
 *   u_out, v_out, w_out - Output wind components [m/s]
 *   method - Interpolation method ("nearest" or "linear")
 * 
 * Returns:
 *   true if valid interpolation found, false otherwise
 */
inline bool interpolate_gridded_wind(
    amrex::Real x, amrex::Real y, amrex::Real z,
    const std::vector<GriddedWindPoint>& wind_gridded,
    amrex::Real& u_out, amrex::Real& v_out, amrex::Real& w_out,
    const std::string& method = "nearest")
{
    if (wind_gridded.empty()) {
        return false;
    }
    
    if (method == "nearest") {
        // Find nearest grid point
        amrex::Real min_dist = std::numeric_limits<amrex::Real>::max();
        int nearest_idx = 0;
        
        for (size_t i = 0; i < wind_gridded.size(); ++i) {
            amrex::Real dx = x - wind_gridded[i].x;
            amrex::Real dy = y - wind_gridded[i].y;
            amrex::Real dz = z - wind_gridded[i].z;
            amrex::Real dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            if (dist < min_dist) {
                min_dist = dist;
                nearest_idx = i;
            }
        }
        
        u_out = wind_gridded[nearest_idx].u;
        v_out = wind_gridded[nearest_idx].v;
        w_out = wind_gridded[nearest_idx].w;
        return true;
    }
    
    // Default to nearest if method not recognized
    return interpolate_gridded_wind(x, y, z, wind_gridded, u_out, v_out, w_out, "nearest");
}

/**
 * Interpolate wind vector at time t from time-series wind field.
 * Uses linear interpolation between adjacent time steps.
 * 
 * Parameters:
 *   t - Query time [s]
 *   wind_timeseries - Vector of time-series wind points
 *   u_out, v_out, w_out - Output wind components [m/s]
 * 
 * Returns:
 *   true if valid interpolation found, false otherwise
 */
inline bool interpolate_timeseries_wind(
    amrex::Real t,
    const std::vector<TimeSeriesWindPoint>& wind_timeseries,
    amrex::Real& u_out, amrex::Real& v_out, amrex::Real& w_out)
{
    if (wind_timeseries.empty()) {
        return false;
    }
    
    // Find bounding time steps
    if (t <= wind_timeseries.front().time) {
        u_out = wind_timeseries.front().u;
        v_out = wind_timeseries.front().v;
        w_out = wind_timeseries.front().w;
        return true;
    }
    
    if (t >= wind_timeseries.back().time) {
        u_out = wind_timeseries.back().u;
        v_out = wind_timeseries.back().v;
        w_out = wind_timeseries.back().w;
        return true;
    }
    
    // Linear interpolation between adjacent points
    for (size_t i = 0; i < wind_timeseries.size() - 1; ++i) {
        if (t >= wind_timeseries[i].time && t <= wind_timeseries[i+1].time) {
            amrex::Real t0 = wind_timeseries[i].time;
            amrex::Real t1 = wind_timeseries[i+1].time;
            amrex::Real alpha = (t - t0) / (t1 - t0);
            
            u_out = wind_timeseries[i].u * (1.0 - alpha) + wind_timeseries[i+1].u * alpha;
            v_out = wind_timeseries[i].v * (1.0 - alpha) + wind_timeseries[i+1].v * alpha;
            w_out = wind_timeseries[i].w * (1.0 - alpha) + wind_timeseries[i+1].w * alpha;
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// Phase 2.1: Multiple Sources CSV Reader
// ============================================================================

/**
 * Read multiple sources from CSV file.
 * 
 * CSV Format:
 *   source_id, x, y, z, emission_rate, emission_duration, type, [stack_diameter, stack_exit_velocity, stack_exit_temperature]
 * 
 * Example:
 *   source_id,x,y,z,emission_rate,emission_duration,type,stack_diameter,stack_exit_velocity,stack_exit_temperature
 *   source_1,150.0,150.0,10.0,1.0,100.0,point,0.0,0.0,298.15
 *   source_2,250.0,150.0,15.0,2.0,200.0,stack,1.5,8.0,350.0
 */
inline bool read_sources_csv(
    const std::string& csv_file,
    std::vector<Source>& sources)
{
    sources.clear();
    
    std::ifstream infile(csv_file);
    if (!infile.is_open()) {
        amrex::Print() << "Warning: Could not open sources file " << csv_file << "\n";
        return false;
    }
    
    std::string line;
    bool found_header = false;
    int source_count = 0;
    
    while (std::getline(infile, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Parse header to determine column order
        if (!found_header && (line.find("source_id") != std::string::npos || 
                             line.find("x") != std::string::npos)) {
            found_header = true;
            continue;
        }
        
        if (!found_header) continue;
        
        // Parse data line
        std::istringstream iss(line);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(iss, field, ',')) {
            // Trim whitespace
            field.erase(0, field.find_first_not_of(" \t\r\n"));
            field.erase(field.find_last_not_of(" \t\r\n") + 1);
            if (!field.empty()) fields.push_back(field);
        }
        
        if (fields.size() < 7) continue;  // Minimum: source_id, x, y, z, emission_rate, emission_duration, type
        
        try {
            Source src;
            src.source_id = fields[0];
            src.x = std::stod(fields[1]);
            src.y = std::stod(fields[2]);
            src.z = std::stod(fields[3]);
            src.emission_rate = std::stod(fields[4]);
            src.emission_duration = std::stod(fields[5]);
            src.type = fields[6];
            
            // Optional stack parameters
            if (fields.size() >= 10) {
                src.stack_diameter = std::stod(fields[7]);
                src.stack_exit_velocity = std::stod(fields[8]);
                src.stack_exit_temperature = std::stod(fields[9]);
            }
            
            sources.push_back(src);
            source_count++;
        } catch (...) {
            amrex::Print() << "Warning: Parsing error in sources CSV at line: " << line << "\n";
            continue;
        }
    }
    
    infile.close();
    
    if (source_count > 0) {
        amrex::Print() << "Read " << source_count << " sources from " << csv_file << "\n";
    }
    
    return source_count > 0;
}

// ============================================================================
// Phase 2.2: Briggs Stack Tip Downwash (STD) Model
// ============================================================================

/**
 * Compute stack tip downwash velocity deficit using Briggs model.
 * 
 * Reference:
 *   Briggs, G. A. (1975). Plume rise predictions. In Lectures on air pollution and its effects, 
 *   ed. D. A. Haugen, U.S. Dept. of Commerce, Washington, DC.
 * 
 * Parameters:
 *   stack_diameter - Stack outer diameter [m]
 *   stack_exit_velocity - Exit velocity from stack [m/s]
 *   wind_speed - Ambient wind speed at stack height [m/s]
 *   stability_class - Pasquill stability class (0=A, 1=B, 2=C, 3=D, 4=E, 5=F)
 * 
 * Returns:
 *   Downwash velocity deficit [m/s] (positive value to subtract from exit velocity)
 */
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_briggs_stack_downwash(
    amrex::Real stack_diameter,
    amrex::Real stack_exit_velocity,
    amrex::Real wind_speed,
    int stability_class = 3)  // Default to neutral (D)
{
    if (stack_diameter <= amrex::Real(0.0) || wind_speed <= amrex::Real(0.0)) {
        return amrex::Real(0.0);  // No downwash for point sources or calm conditions
    }
    
    // Stack Froude number: Fr = Vs² / (g * D)
    const amrex::Real g = amrex::Real(9.81);
    const amrex::Real Fr = stack_exit_velocity * stack_exit_velocity / (g * stack_diameter);
    
    if (Fr < amrex::Real(1.0)) {
        // Buoyant plume (unstable) - minimal downwash
        return amrex::Real(0.0);
    }
    
    // Downwash velocity ratio: Vs_downwash / Vs
    // Simplified Briggs formula: depends on stack geometry and wind conditions
    // For typical industrial stacks: downwash ≈ 0.5 * U (ambient wind speed)
    amrex::Real downwash_factor = amrex::Real(0.5);
    
    // Adjust based on stability (more downwash in stable conditions)
    if (stability_class >= 4) {  // E or F (stable)
        downwash_factor = amrex::Real(0.7);
    } else if (stability_class <= 1) {  // A or B (unstable)
        downwash_factor = amrex::Real(0.3);
    }
    
    amrex::Real downwash_velocity = downwash_factor * wind_speed;
    
    // Limit downwash to not exceed exit velocity
    downwash_velocity = std::min(downwash_velocity, stack_exit_velocity * amrex::Real(0.95));
    
    return downwash_velocity;
}

// ============================================================================
// Phase 2.3: Meteorological Profile CSV Reader
// ============================================================================

/**
 * Read meteorological profiles from CSV file.
 * 
 * CSV Format (long format):
 *   profile_id, x_ref, y_ref, z_agl, u, v, w, K_h, K_v, stability_class
 * 
 * Example:
 *   profile_id,x_ref,y_ref,z_agl,u,v,w,K_h,K_v,stability_class
 *   profile_1,100.0,100.0,0.0,0.0,0.0,0.0,0.5,0.1,D
 *   profile_1,100.0,100.0,10.0,9.8,0.0,0.0,1.0,0.5,D
 *   profile_1,100.0,100.0,50.0,10.5,0.0,0.0,2.0,1.0,D
 *   profile_2,200.0,100.0,0.0,0.0,0.0,0.0,0.3,0.05,F
 *   ...
 */
inline bool read_met_profiles_csv(
    const std::string& csv_file,
    std::vector<MetProfile>& profiles)
{
    profiles.clear();
    
    std::ifstream infile(csv_file);
    if (!infile.is_open()) {
        amrex::Print() << "Warning: Could not open meteorological profiles file " << csv_file << "\n";
        return false;
    }
    
    std::string line;
    bool found_header = false;
    std::map<std::string, MetProfile> profile_map;  // Temporary storage to aggregate levels
    int profile_count = 0;
    
    while (std::getline(infile, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Parse header
        if (!found_header && (line.find("profile_id") != std::string::npos || 
                             line.find("z_agl") != std::string::npos)) {
            found_header = true;
            continue;
        }
        
        if (!found_header) continue;
        
        // Parse data line
        std::istringstream iss(line);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(iss, field, ',')) {
            // Trim whitespace
            field.erase(0, field.find_first_not_of(" \t\r\n"));
            field.erase(field.find_last_not_of(" \t\r\n") + 1);
            if (!field.empty()) fields.push_back(field);
        }
        
        if (fields.size() < 10) continue;  // Need all columns
        
        try {
            std::string profile_id = fields[0];
            amrex::Real x_ref = std::stod(fields[1]);
            amrex::Real y_ref = std::stod(fields[2]);
            amrex::Real z_agl = std::stod(fields[3]);
            amrex::Real u = std::stod(fields[4]);
            amrex::Real v = std::stod(fields[5]);
            amrex::Real w = std::stod(fields[6]);
            amrex::Real K_h = std::stod(fields[7]);
            amrex::Real K_v = std::stod(fields[8]);
            std::string stability = fields[9];
            
            // Aggregate levels for this profile
            if (profile_map.find(profile_id) == profile_map.end()) {
                MetProfile prof;
                prof.profile_id = profile_id;
                prof.x_ref = x_ref;
                prof.y_ref = y_ref;
                prof.stability_class = stability;
                profile_map[profile_id] = prof;
            }
            
            profile_map[profile_id].z_agl.push_back(z_agl);
            profile_map[profile_id].u.push_back(u);
            profile_map[profile_id].v.push_back(v);
            profile_map[profile_id].w.push_back(w);
            profile_map[profile_id].K_h.push_back(K_h);
            profile_map[profile_id].K_v.push_back(K_v);
        } catch (...) {
            amrex::Print() << "Warning: Parsing error in met profiles CSV at line: " << line << "\n";
            continue;
        }
    }
    
    infile.close();
    
    // Convert map to vector
    for (auto& p : profile_map) {
        profiles.push_back(p.second);
        profile_count++;
    }
    
    if (profile_count > 0) {
        amrex::Print() << "Read " << profile_count << " meteorological profiles from " << csv_file << "\n";
    }
    
    return profile_count > 0;
}

/**
 * Select meteorological profile by puff position (spatial interpolation).
 * 
 * Uses nearest-neighbor selection based on horizontal distance.
 * 
 * Parameters:
 *   x, y - Puff position [m]
 *   profiles - Vector of available meteorological profiles
 *   prof_out - Output selected profile
 * 
 * Returns:
 *   true if profile found, false if no profiles available
 */
inline bool select_met_profile_by_position(
    amrex::Real x, amrex::Real y,
    const std::vector<MetProfile>& profiles,
    const MetProfile*& prof_out)
{
    if (profiles.empty()) {
        return false;
    }
    
    if (profiles.size() == 1) {
        prof_out = &profiles[0];
        return true;
    }
    
    // Find nearest profile by horizontal distance
    amrex::Real min_dist = std::numeric_limits<amrex::Real>::max();
    int nearest_idx = 0;
    
    for (size_t i = 0; i < profiles.size(); ++i) {
        amrex::Real dx = x - profiles[i].x_ref;
        amrex::Real dy = y - profiles[i].y_ref;
        amrex::Real dist = std::sqrt(dx*dx + dy*dy);
        
        if (dist < min_dist) {
            min_dist = dist;
            nearest_idx = i;
        }
    }
    
    prof_out = &profiles[nearest_idx];
    return true;
}

/**
 * Interpolate wind and diffusivity at height z from a meteorological profile.
 * 
 * Uses linear interpolation for z values between profile levels.
 * Extrapolates constant values beyond profile bounds.
 * 
 * Parameters:
 *   z_agl - Height above ground level [m]
 *   profile - Meteorological profile to interpolate
 *   u_out, v_out, w_out - Output wind components [m/s]
 *   K_h_out, K_v_out - Output diffusivities [m²/s]
 * 
 * Returns:
 *   true if valid interpolation found, false otherwise
 */
inline bool interpolate_met_profile(
    amrex::Real z_agl,
    const MetProfile& profile,
    amrex::Real& u_out, amrex::Real& v_out, amrex::Real& w_out,
    amrex::Real& K_h_out, amrex::Real& K_v_out)
{
    if (profile.z_agl.empty()) {
        return false;
    }
    
    // Find bounding levels
    if (z_agl <= profile.z_agl.front()) {
        u_out = profile.u.front();
        v_out = profile.v.front();
        w_out = profile.w.front();
        K_h_out = profile.K_h.front();
        K_v_out = profile.K_v.front();
        return true;
    }
    
    if (z_agl >= profile.z_agl.back()) {
        u_out = profile.u.back();
        v_out = profile.v.back();
        w_out = profile.w.back();
        K_h_out = profile.K_h.back();
        K_v_out = profile.K_v.back();
        return true;
    }
    
    // Linear interpolation between adjacent levels
    for (size_t i = 0; i < profile.z_agl.size() - 1; ++i) {
        if (z_agl >= profile.z_agl[i] && z_agl <= profile.z_agl[i+1]) {
            amrex::Real z0 = profile.z_agl[i];
            amrex::Real z1 = profile.z_agl[i+1];
            amrex::Real alpha = (z_agl - z0) / (z1 - z0);
            
            u_out = profile.u[i] * (amrex::Real(1.0) - alpha) + profile.u[i+1] * alpha;
            v_out = profile.v[i] * (amrex::Real(1.0) - alpha) + profile.v[i+1] * alpha;
            w_out = profile.w[i] * (amrex::Real(1.0) - alpha) + profile.w[i+1] * alpha;
            K_h_out = profile.K_h[i] * (amrex::Real(1.0) - alpha) + profile.K_h[i+1] * alpha;
            K_v_out = profile.K_v[i] * (amrex::Real(1.0) - alpha) + profile.K_v[i+1] * alpha;
            return true;
        }
    }
    
    return false;
}

#endif // PUFF_MODELS_H
