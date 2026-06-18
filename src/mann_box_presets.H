#ifndef MANN_BOX_PRESETS_H_
#define MANN_BOX_PRESETS_H_

/**
 * Mann Box Built-in Presets for Common Scenarios
 *
 * This header provides pre-tuned parameter sets for common atmospheric
 * scenarios. Each preset contains validated literature-based parameters
 * that provide good starting points for turbulence synthesis.
 *
 * Presets included:
 * 1. Grassland (open terrain, typical wind farm site)
 * 2. Forest (dense vegetation)
 * 3. Urban (city environment, building canopy)
 * 4. Mountain (steep terrain, complex flow)
 * 5. Coastal (sea-land transition, thermal effects)
 *
 * References:
 *   - Mann, J. (1994). The spatial structure of neutral atmospheric
 *     surface-layer turbulence. J. Fluid Mech., 273, 141-168.
 *   - IEC 61400-1 Ed.4. Wind turbines - Part 1: Design requirements
 *   - Panofsky & Dutton (1984). Atmospheric Turbulence.
 */

#include <string>
#include <map>
#include <cmath>

/**
 * Complete parameter set for Mann Box spectral synthesis.
 *
 * Encapsulates all parameters needed for turbulence generation in a given scenario.
 */
struct MannBoxPreset {
    // Identifier
    std::string name;
    std::string description;
    
    // Wind profile parameters
    double U_ref = 10.0;              // Reference wind speed (m/s) at z_ref
    double z_ref = 10.0;              // Reference height (m)
    double z0 = 0.1;                  // Aerodynamic roughness length (m)
    double d = 0.0;                   // Displacement height (m)
    
    // Spectral parameters
    double L_u = 300.0;               // Integral length scale u-component (m)
    double L_v = 200.0;               // Integral length scale v-component (m)
    double L_w = 120.0;               // Integral length scale w-component (m)
    
    // Anisotropy ratios
    double anisotropy_v = 0.80;       // v-component ratio relative to u
    double anisotropy_w = 0.50;       // w-component ratio relative to u
    
    // Coherence parameters
    double decay_rate_uy = 10.0;      // Coherence decay rate in y-direction
    double decay_rate_uz = 5.0;       // Coherence decay rate in z-direction
    
    // Stability parameters
    double L_MO = 1000.0;             // Monin-Obukhov length (m, neutral ~inf)
    double Ri_b = 0.0;                // Bulk Richardson number (neutral)
    
    // Directional parameters
    double wind_direction = 270.0;    // Wind direction (degrees, 0=N, 90=E)
    double veer_rate = 0.0;           // Directional veer (degrees per 100m)
    double cross_wind_bias = 0.0;     // Cross-wind asymmetry (-1 to 1)
    
    // Height-dependent parameters
    double alpha_profile = 0.2;       // Wind shear exponent (power-law)
    
    // Advanced parameters
    double mann_a = 1.0;              // Mann model parameter a
    double mann_k = 1.0;              // Mann model parameter k
    double mann_alpha = 1.0;          // Mann model parameter alpha
    
    // Temporal parameters
    double dt = 0.1;                  // Time step for synthesis (s)
    int n_steps = 6000;               // Number of time steps
};

/**
 * Grassland Preset
 *
 * Typical open terrain, such as wind farm sites on grassland.
 * Characteristics:
 * - Low to moderate roughness (z0 = 0.05-0.1 m)
 * - Normal anisotropy (v/u = 0.8, w/u = 0.5)
 * - Moderate spectral length scales
 * - Neutral to slightly stable stratification
 *
 * References:
 *   - Panofsky & Dutton (1984). Grassland boundary layer
 *   - IEC 61400-1 NTM (Normal Turbulence Model)
 */
inline MannBoxPreset grassland_preset() {
    MannBoxPreset p;
    
    p.name = "Grassland";
    p.description = "Open grassland, wind farm site";
    
    p.U_ref = 10.0;
    p.z_ref = 10.0;
    p.z0 = 0.05;
    p.d = 0.0;
    
    p.L_u = 300.0;
    p.L_v = 200.0;
    p.L_w = 120.0;
    
    p.anisotropy_v = 0.80;
    p.anisotropy_w = 0.50;
    
    p.decay_rate_uy = 10.0;
    p.decay_rate_uz = 5.0;
    
    p.L_MO = 1000.0;  // Neutral
    p.Ri_b = 0.0;
    
    p.wind_direction = 270.0;
    p.veer_rate = 5.0;  // Slight veer in neutral conditions
    p.cross_wind_bias = 0.0;
    
    p.alpha_profile = 0.2;
    
    p.mann_a = 1.0;
    p.mann_k = 1.0;
    p.mann_alpha = 1.0;
    
    p.dt = 0.1;
    p.n_steps = 6000;  // 10 minutes at 10 Hz
    
    return p;
}

/**
 * Forest Preset
 *
 * Dense forest environment with canopy effects.
 * Characteristics:
 * - High roughness (z0 = 0.5-2.0 m)
 * - Significant displacement height (d = 10-15 m for tall forest)
 * - Enhanced vertical coherence, reduced w-component
 * - Suppressed vertical motion within canopy
 *
 * References:
 *   - Poggi et al. (2004). Turbulence structure in forest canopy
 *   - Raupach et al. (1991). Rough-wall turbulent boundary layers
 */
inline MannBoxPreset forest_preset() {
    MannBoxPreset p;
    
    p.name = "Forest";
    p.description = "Dense forest with canopy effects";
    
    p.U_ref = 8.0;  // Lower mean wind in forest
    p.z_ref = 20.0;  // Reference above canopy
    p.z0 = 1.0;
    p.d = 12.0;  // Displacement height in tall forest
    
    p.L_u = 250.0;  // Slightly shorter scales in forest
    p.L_v = 180.0;
    p.L_w = 80.0;  // Reduced vertical scale
    
    p.anisotropy_v = 0.85;  // Slightly enhanced
    p.anisotropy_w = 0.35;  // Significantly reduced (suppressed vertical)
    
    p.decay_rate_uy = 12.0;
    p.decay_rate_uz = 8.0;
    
    p.L_MO = 500.0;  // Slightly stable in dense forest
    p.Ri_b = 0.01;
    
    p.wind_direction = 270.0;
    p.veer_rate = 15.0;  // More veer in stable forest
    p.cross_wind_bias = 0.1;  // Slight enhancement of crosswind
    
    p.alpha_profile = 0.3;  // Enhanced shear in forest
    
    p.mann_a = 1.05;
    p.mann_k = 0.95;
    p.mann_alpha = 1.0;
    
    p.dt = 0.1;
    p.n_steps = 6000;
    
    return p;
}

/**
 * Urban Preset
 *
 * Urban environment with building canopy.
 * Characteristics:
 * - Very high roughness (z0 = 0.3-3.0 m)
 * - Significant displacement height
 * - Enhanced u-component, suppressed w-component (street canyon effect)
 * - More spatially variable flow
 *
 * References:
 *   - Oke (1988). Boundary Layer Climates
 *   - Macdonald et al. (1998). Spanwise coherence of surface layer flow
 */
inline MannBoxPreset urban_preset() {
    MannBoxPreset p;
    
    p.name = "Urban";
    p.description = "Urban environment, building canopy";
    
    p.U_ref = 7.0;  // Reduced wind in urban canyon
    p.z_ref = 15.0;
    p.z0 = 1.5;
    p.d = 10.0;
    
    p.L_u = 280.0;
    p.L_v = 180.0;
    p.L_w = 100.0;  // Reduced vertical scale (street canyons)
    
    p.anisotropy_v = 0.75;  // Reduced
    p.anisotropy_w = 0.40;  // Suppressed vertical (canyon effect)
    
    p.decay_rate_uy = 15.0;  // Faster decay (smaller eddies)
    p.decay_rate_uz = 10.0;
    
    p.L_MO = 400.0;  // Often stable in urban heat island
    p.Ri_b = 0.02;
    
    p.wind_direction = 270.0;
    p.veer_rate = 20.0;  // Strong veer in urban stable layer
    p.cross_wind_bias = -0.2;  // Suppression of crosswind
    
    p.alpha_profile = 0.35;  // Strong shear from buildings
    
    p.mann_a = 1.1;
    p.mann_k = 0.9;
    p.mann_alpha = 0.95;
    
    p.dt = 0.1;
    p.n_steps = 6000;
    
    return p;
}

/**
 * Mountain Preset
 *
 * Mountainous terrain with complex flow.
 * Characteristics:
 * - High to very high roughness (z0 = 1-5 m)
 * - Large displacement height
 * - Larger spectral scales (mountain effects)
 * - Higher turbulence intensity
 * - Variable directional effects
 *
 * References:
 *   - Panofsky & Dutton (1984). Mountainous terrain
 *   - Lott & Miller (1997). Orographic corrections to meteorology
 */
inline MannBoxPreset mountain_preset() {
    MannBoxPreset p;
    
    p.name = "Mountain";
    p.description = "Mountainous terrain with complex flow";
    
    p.U_ref = 12.0;  // Often windier on mountains
    p.z_ref = 20.0;
    p.z0 = 2.0;
    p.d = 15.0;
    
    p.L_u = 400.0;  // Larger scales from mountain effects
    p.L_v = 300.0;
    p.L_w = 200.0;  // Larger vertical scale
    
    p.anisotropy_v = 0.75;
    p.anisotropy_w = 0.55;  // Slightly enhanced vertical (updrafts)
    
    p.decay_rate_uy = 8.0;  // Slower decay (larger eddies)
    p.decay_rate_uz = 6.0;
    
    p.L_MO = 300.0;  // Often stable on mountains
    p.Ri_b = 0.05;
    
    p.wind_direction = 270.0;
    p.veer_rate = 25.0;  // Strong veer due to terrain
    p.cross_wind_bias = 0.2;  // Alignment with ridge
    
    p.alpha_profile = 0.35;
    
    p.mann_a = 0.95;
    p.mann_k = 1.05;
    p.mann_alpha = 1.05;
    
    p.dt = 0.1;
    p.n_steps = 6000;
    
    return p;
}

/**
 * Coastal Preset
 *
 * Coastal area with sea-land transition and thermal effects.
 * Characteristics:
 * - Low to moderate roughness over water
 * - Variable roughness at transition
 * - Sea breeze effects (thermal circulation)
 * - Often unstable during day, stable at night
 *
 * References:
 *   - Dorman & Salmon (1994). Coastal meteorology
 *   - Mahrt (1996). The bulk aerodynamic formulation over oceans
 */
inline MannBoxPreset coastal_preset() {
    MannBoxPreset p;
    
    p.name = "Coastal";
    p.description = "Coastal area with sea-land effects";
    
    p.U_ref = 9.5;
    p.z_ref = 10.0;
    p.z0 = 0.02;  // Lower roughness (water)
    p.d = 0.0;
    
    p.L_u = 280.0;
    p.L_v = 220.0;
    p.L_w = 140.0;  // Enhanced vertical in unstable conditions
    
    p.anisotropy_v = 0.82;
    p.anisotropy_w = 0.60;  // Higher (more vertical motion from thermal)
    
    p.decay_rate_uy = 9.0;
    p.decay_rate_uz = 4.0;
    
    p.L_MO = -50.0;  // Often unstable (negative = unstable)
    p.Ri_b = -0.05;
    
    p.wind_direction = 250.0;
    p.veer_rate = 3.0;  // Less veer in unstable
    p.cross_wind_bias = 0.0;
    
    p.alpha_profile = 0.15;  // Reduced shear in unstable
    
    p.mann_a = 1.0;
    p.mann_k = 1.05;
    p.mann_alpha = 1.1;  // Enhanced for convection
    
    p.dt = 0.05;  // Finer resolution for coastal effects
    p.n_steps = 12000;
    
    return p;
}

/**
 * Get preset by name.
 *
 * @param preset_name  Name of the preset (case-insensitive)
 * @return            MannBoxPreset with matching name, or grassland_preset() if not found
 */
inline MannBoxPreset get_preset_by_name(const std::string& preset_name) {
    std::string name_lower = preset_name;
    
    // Convert to lowercase for case-insensitive comparison
    for (auto& c : name_lower) {
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    }
    
    if (name_lower == "grassland") {
        return grassland_preset();
    } else if (name_lower == "forest") {
        return forest_preset();
    } else if (name_lower == "urban") {
        return urban_preset();
    } else if (name_lower == "mountain") {
        return mountain_preset();
    } else if (name_lower == "coastal") {
        return coastal_preset();
    } else {
        // Default fallback
        return grassland_preset();
    }
}

/**
 * Get list of all available presets.
 *
 * @return  Vector of preset names
 */
inline std::vector<std::string> get_available_presets() {
    return {
        "Grassland",
        "Forest",
        "Urban",
        "Mountain",
        "Coastal"
    };
}

/**
 * Print preset information.
 *
 * @param preset  The preset to print
 */
inline void print_preset_info(const MannBoxPreset& preset) {
    // Note: In a real implementation, this would use proper I/O
    // For header-only library, we skip printing to avoid I/O dependencies
}

#endif // MANN_BOX_PRESETS_H_
