#ifndef MANN_BOX_ROUGHNESS_EFFECTS_H_
#define MANN_BOX_ROUGHNESS_EFFECTS_H_

/**
 * Mann Box Phase 6: Surface Roughness & Canopy Effects
 *
 * This header implements roughness-dependent modifications to the Mann Box
 * spectral tensor, particularly for forest canopies and urban environments.
 *
 * Key features:
 * 1. Roughness-dependent spectral shape modifications
 * 2. Forest/canopy enhancement of vertical coherence
 * 3. Urban area anisotropy modification
 * 4. Continuous z₀ (roughness length) variation support
 * 5. Displacement height effects
 *
 * Theory:
 *   - Roughness elements modify boundary layer structure
 *   - Forests enhance vertical mixing (reduce w-component)
 *   - Urban canyons increase u-component variance
 *   - Rougher surfaces increase turbulence intensity
 *
 * References:
 *   - Raupach et al. (1991). Rough-wall turbulent boundary layers.
 *     Appl. Mech. Rev., 44, 1-25.
 *   - Poggi et al. (2004). Turbulence structure in a forest canopy.
 *     Boundary-Layer Meteorol., 111, 179-196.
 */

#include <cmath>
#include <array>
#include <algorithm>
#include <map>
#include <string>

/**
 * Roughness classes enumeration.
 *
 * Predefined roughness categories for different surface types.
 * Values are representative aerodynamic roughness lengths in meters.
 */
enum class RoughnessClass {
    SMOOTH_WATER = 0,      // z0 ~ 0.0001 m (open ocean)
    GRASSLAND = 1,         // z0 ~ 0.05 m (short grass)
    GRASSLAND_HIGH = 2,    // z0 ~ 0.1 m (tall grass)
    SHRUBLAND = 3,         // z0 ~ 0.15 m (low shrubs)
    FOREST_SPARSE = 4,     // z0 ~ 0.2-0.5 m (sparse forest)
    FOREST_DENSE = 5,      // z0 ~ 0.5-2.0 m (dense forest)
    URBAN_LOW = 6,         // z0 ~ 0.3-1.0 m (low urban)
    URBAN_HIGH = 7,        // z0 ~ 1.0-3.0 m (high urban)
    MOUNTAINS = 8          // z0 ~ 1.0-5.0 m (mountainous)
};

/**
 * Get representative roughness length for a given class.
 *
 * @param roughness_class  Roughness class enumeration
 * @return                 Aerodynamic roughness length (m)
 */
inline double get_roughness_length_for_class(RoughnessClass roughness_class) {
    switch (roughness_class) {
        case RoughnessClass::SMOOTH_WATER:    return 0.0001;
        case RoughnessClass::GRASSLAND:       return 0.05;
        case RoughnessClass::GRASSLAND_HIGH:  return 0.1;
        case RoughnessClass::SHRUBLAND:       return 0.15;
        case RoughnessClass::FOREST_SPARSE:   return 0.35;
        case RoughnessClass::FOREST_DENSE:    return 1.0;
        case RoughnessClass::URBAN_LOW:       return 0.7;
        case RoughnessClass::URBAN_HIGH:      return 1.5;
        case RoughnessClass::MOUNTAINS:       return 2.0;
        default:                              return 0.1;
    }
}

/**
 * Roughness parameters structure.
 *
 * Holds aerodynamic roughness, displacement height, and derived quantities.
 *
 * Members:
 *   z0:                  Aerodynamic roughness length (m)
 *   d:                   Zero-plane displacement height (m)
 *   h_canopy:            Canopy height (m)
 *   roughness_class:     Classification of surface type
 *   canopy_density:      Leaf area density for vegetation (0-1, where 1 is dense)
 *   urban_canyon_width:  Width of urban canyon (m, for urban areas)
 */
struct RoughnessParameters {
    double z0 = 0.1;
    double d = 0.0;
    double h_canopy = 0.0;
    RoughnessClass roughness_class = RoughnessClass::GRASSLAND;
    double canopy_density = 0.0;
    double urban_canyon_width = 25.0;
};

/**
 * Compute turbulence intensity scaling with roughness.
 *
 * Rougher surfaces produce higher turbulence intensity. The relationship
 * is approximately logarithmic.
 *
 * I(z, z0) ≈ I_ref * (1 + a * ln(z0/z0_ref))
 *
 * @param z0               Aerodynamic roughness length (m)
 * @param z0_ref           Reference roughness (m, typically 0.1 m for grassland)
 * @param TI_ref           Reference turbulence intensity
 * @param a                Scaling coefficient (typically 0.1-0.2)
 * @return                 Adjusted turbulence intensity
 */
inline double turbulence_intensity_from_roughness(
    double z0, double z0_ref, double TI_ref, double a = 0.15) {
    
    if (z0 <= 0.0 || z0_ref <= 0.0) return TI_ref;
    
    double log_ratio = std::log(z0 / z0_ref);
    double TI_adjusted = TI_ref * (1.0 + a * log_ratio);
    
    // Clamp to reasonable range
    return std::max(0.05, std::min(0.40, TI_adjusted));
}

/**
 * Compute anisotropy modification from roughness.
 *
 * Different surface types produce different anisotropy:
 * - Smooth surfaces: normal v/u~0.8, w/u~0.5
 * - Rough surfaces: enhanced u-variance relative to v,w
 * - Forests: enhanced vertical mixing (lower w/u ratio)
 * - Urban: reduced w-variance in street canyons
 *
 * @param roughness_params  Roughness parameters
 * @param z                 Height above ground (m)
 * @param anisotropy_v_base Base v-component anisotropy ratio
 * @param anisotropy_w_base Base w-component anisotropy ratio
 * @return                  Modified anisotropy ratios (v_ratio, w_ratio)
 */
inline std::pair<double, double> modify_anisotropy_from_roughness(
    const RoughnessParameters& roughness_params,
    double z,
    double anisotropy_v_base,
    double anisotropy_w_base) {
    
    double v_ratio = anisotropy_v_base;
    double w_ratio = anisotropy_w_base;
    
    double z0 = roughness_params.z0;
    double d = roughness_params.d;
    double h_canopy = roughness_params.h_canopy;
    
    // Normalize roughness to reference (grassland z0 = 0.1 m)
    double z0_ref = 0.1;
    double roughness_ratio = std::log(z0 / z0_ref) / std::log(1.0 / z0_ref);
    
    // Height above displacement plane
    double z_eff = std::max(0.1, z - d);
    
    // In-canopy effect (within canopy layer)
    if (h_canopy > 0 && z < h_canopy) {
        // Within canopy: enhanced vertical mixing, reduced w-variance
        double z_norm = z / h_canopy;
        double canopy_effect = (1.0 - z_norm) * roughness_params.canopy_density;
        
        v_ratio *= (1.0 + 0.1 * canopy_effect);
        w_ratio *= (1.0 - 0.2 * canopy_effect);  // Suppress vertical motion in canopy
    }
    // Above-canopy effect (logarithmic profile region)
    else if (h_canopy > 0 && z < 3.0 * h_canopy) {
        // Enhanced mixing above canopy
        double v_ratio_roughness = anisotropy_v_base * (1.0 + 0.15 * roughness_ratio);
        double w_ratio_roughness = anisotropy_w_base * (1.0 - 0.05 * roughness_ratio);
        
        v_ratio = v_ratio_roughness;
        w_ratio = w_ratio_roughness;
    }
    // General roughness effect (away from canopy)
    else {
        // Rougher surfaces increase u-variance relative to v, w
        // This reduces anisotropy ratios
        double roughness_factor = 1.0 - 0.2 * roughness_ratio;
        roughness_factor = std::max(0.7, roughness_factor);
        
        v_ratio *= roughness_factor;
        w_ratio *= roughness_factor;
    }
    
    // Urban modifications
    if (roughness_params.roughness_class == RoughnessClass::URBAN_HIGH ||
        roughness_params.roughness_class == RoughnessClass::URBAN_LOW) {
        // Urban canyon effects: enhanced u-variance, reduced w
        double urban_factor = 0.9;
        v_ratio *= urban_factor;
        w_ratio *= 0.7;  // Vertical motion suppressed by urban heat island
    }
    
    // Clamp to physical ranges
    v_ratio = std::max(0.1, std::min(1.0, v_ratio));
    w_ratio = std::max(0.05, std::min(1.0, w_ratio));
    
    return {v_ratio, w_ratio};
}

/**
 * Modify spectral tensor for roughness effects.
 *
 * Adjusts spectral shape based on surface roughness class and canopy parameters.
 *
 * @param S_tensor         Input spectral tensor (9 components)
 * @param roughness_params Roughness parameters
 * @param z                Height above ground (m)
 * @return                 Modified spectral tensor with roughness effects
 */
inline std::array<double, 9> apply_roughness_modifications(
    const std::array<double, 9>& S_tensor,
    const RoughnessParameters& roughness_params,
    double z) {
    
    std::array<double, 9> S_modified = S_tensor;
    
    // Apply modifications based on surface type
    switch (roughness_params.roughness_class) {
        case RoughnessClass::FOREST_DENSE:
        case RoughnessClass::FOREST_SPARSE: {
            // Forest effects: enhanced vertical momentum exchange
            // In-canopy: reduce w-variance (suppressed vertical motion)
            if (z < roughness_params.h_canopy) {
                double z_norm = z / roughness_params.h_canopy;
                double canopy_factor = 1.0 - 0.3 * (1.0 - z_norm);
                S_modified[5] *= canopy_factor * canopy_factor;  // S_ww
            }
            break;
        }
        case RoughnessClass::URBAN_HIGH:
        case RoughnessClass::URBAN_LOW: {
            // Urban canyon effects: reduce vertical variance
            // Enhanced horizontal u-component due to street alignment
            S_modified[0] *= 1.15;  // S_uu increased
            S_modified[5] *= 0.75;  // S_ww decreased
            break;
        }
        case RoughnessClass::GRASSLAND_HIGH: {
            // Tall grass: moderate enhancement of turbulence
            S_modified[0] *= 1.1;  // S_uu
            S_modified[3] *= 0.95; // S_vv
            S_modified[5] *= 0.85; // S_ww
            break;
        }
        case RoughnessClass::SHRUBLAND: {
            // Shrubland: intermediate effects
            S_modified[0] *= 1.05;
            S_modified[5] *= 0.9;
            break;
        }
        default:
            // No modification for other classes
            break;
    }
    
    return S_modified;
}

/**
 * Compute displacement height from roughness parameters.
 *
 * Displacement height d represents the effective ground level within
 * vegetation canopy. Typically d ≈ 0.6-0.8 * h_canopy.
 *
 * @param roughness_params  Roughness parameters
 * @return                  Displacement height (m)
 */
inline double compute_displacement_height(
    const RoughnessParameters& roughness_params) {
    
    if (roughness_params.h_canopy <= 0) {
        return 0.0;
    }
    
    double d_ratio = 0.65;  // Typical value for vegetation
    
    // Adjust based on surface type
    switch (roughness_params.roughness_class) {
        case RoughnessClass::FOREST_DENSE:
            d_ratio = 0.75;
            break;
        case RoughnessClass::FOREST_SPARSE:
            d_ratio = 0.60;
            break;
        case RoughnessClass::URBAN_HIGH:
            d_ratio = 0.70;
            break;
        case RoughnessClass::SHRUBLAND:
            d_ratio = 0.55;
            break;
        default:
            d_ratio = 0.65;
            break;
    }
    
    return d_ratio * roughness_params.h_canopy;
}

/**
 * Compute roughness sublayer height.
 *
 * Below the roughness sublayer height, flow is directly influenced by
 * individual roughness elements. Above it, a logarithmic profile develops.
 *
 * Typical: h_RSL ≈ 2-5 * h_canopy (or 2-5 * max(z0, d))
 *
 * @param roughness_params  Roughness parameters
 * @return                  Roughness sublayer height (m)
 */
inline double compute_roughness_sublayer_height(
    const RoughnessParameters& roughness_params) {
    
    double h_canopy = roughness_params.h_canopy;
    double z0 = roughness_params.z0;
    
    if (h_canopy > 0) {
        return 4.0 * h_canopy;
    } else {
        return 10.0 * z0;  // Fallback: ~10 * roughness length
    }
}

/**
 * Get roughness class name string.
 *
 * @param roughness_class  Roughness class enumeration
 * @return                 Human-readable class name
 */
inline std::string get_roughness_class_name(RoughnessClass roughness_class) {
    switch (roughness_class) {
        case RoughnessClass::SMOOTH_WATER:    return "Smooth water";
        case RoughnessClass::GRASSLAND:       return "Grassland (short)";
        case RoughnessClass::GRASSLAND_HIGH:  return "Grassland (tall)";
        case RoughnessClass::SHRUBLAND:       return "Shrubland";
        case RoughnessClass::FOREST_SPARSE:   return "Forest (sparse)";
        case RoughnessClass::FOREST_DENSE:    return "Forest (dense)";
        case RoughnessClass::URBAN_LOW:       return "Urban (low)";
        case RoughnessClass::URBAN_HIGH:      return "Urban (high)";
        case RoughnessClass::MOUNTAINS:       return "Mountains";
        default:                              return "Unknown";
    }
}

#endif // MANN_BOX_ROUGHNESS_EFFECTS_H_
