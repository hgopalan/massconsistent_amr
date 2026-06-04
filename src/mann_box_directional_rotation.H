#ifndef MANN_BOX_DIRECTIONAL_ROTATION_H_
#define MANN_BOX_DIRECTIONAL_ROTATION_H_

/**
 * Mann Box Phase 6: Directional Anisotropy & Wind Veer
 *
 * This header implements directional rotation and wind veering for the Mann Box
 * spectral tensor model. Key features:
 *
 * 1. Wind direction tensor rotation (align spectrum to wind direction)
 * 2. Height-dependent wind veer (directional shear with height)
 * 3. Cross-wind component asymmetry
 * 4. Wavenumber vector directional preferences
 *
 * Theory:
 *   - Spectral tensor is computed in neutral reference frame
 *   - Rotation matrix aligns to mean wind direction
 *   - Veer creates helical wind profile with height
 *   - Cross-wind assymetry from directional preferences
 *
 * References:
 *   - Mann, J. (1994). The spatial structure of neutral atmospheric
 *     surface-layer turbulence. J. Fluid Mech., 273, 141-168.
 *   - Kristensen, L., et al. (2005). Turbulence intensity and temperature
 *     fluctuations. Boundary-Layer Meteorol., 116, 167-189.
 */

#include <cmath>
#include <array>
#include <algorithm>

/**
 * Rotation matrix for 3D coordinate transformation.
 *
 * Represents a 3x3 rotation matrix using array-of-arrays layout.
 */
using RotationMatrix = std::array<std::array<double, 3>, 3>;

/**
 * Directional parameters for wind veer and rotation.
 *
 * Members:
 *   wind_direction:  Mean wind direction (degrees, 0=North, 90=East)
 *   veer_rate:       Rate of directional change with height (deg/100m)
 *   veer_amplitude:  Maximum veer angle from ground to reference (degrees)
 *   cross_wind_bias: Asymmetry factor for cross-wind component (-1 to 1)
 *   kappa_directional: Directional coherence attenuation factor (0 to 1)
 */
struct DirectionalParameters {
    double wind_direction = 270.0;      // Default: west wind (from west)
    double veer_rate = 0.0;             // No veer by default (neutral)
    double veer_amplitude = 30.0;       // Typical 20-40 deg veer in stable
    double cross_wind_bias = 0.0;       // Default: no asymmetry
    double kappa_directional = 1.0;     // Default: no directional attenuation
};

/**
 * Create a rotation matrix from wind direction angle.
 *
 * The rotation matrix rotates from North-East-Up coordinates to
 * Streamwise-Crosswind-Vertical coordinates aligned with wind direction.
 *
 * Convention:
 *   wind_direction = 0°   → North wind (u towards south)
 *   wind_direction = 90°  → East wind (u towards west)
 *   wind_direction = 180° → South wind (u towards north)
 *   wind_direction = 270° → West wind (u towards east)
 *
 * @param wind_direction  Wind direction in degrees (0-360)
 * @return                3x3 rotation matrix for coordinate transformation
 */
inline RotationMatrix create_rotation_matrix_from_direction(double wind_direction) {
    // Convert to radians
    double theta = (wind_direction - 180.0) * M_PI / 180.0;  // angle from streamwise
    double cos_t = std::cos(theta);
    double sin_t = std::sin(theta);
    
    // Rotation matrix (active rotation, counterclockwise when viewed from above)
    RotationMatrix R = {{
        {cos_t,  sin_t,  0.0},
        {-sin_t, cos_t,  0.0},
        {0.0,    0.0,    1.0}
    }};
    
    return R;
}

/**
 * Apply rotation matrix to spectral tensor components.
 *
 * Transforms the 9 components of the spectral tensor from reference
 * coordinates to wind-aligned coordinates using:
 *   S'_ij = R_ik * S_kl * R_jl
 *
 * @param S_original       Spectral tensor (9 components) in reference frame
 * @param R                Rotation matrix (wind direction alignment)
 * @return                 Rotated spectral tensor (9 components)
 */
inline std::array<double, 9> rotate_spectral_tensor(
    const std::array<double, 9>& S_original,
    const RotationMatrix& R) {
    
    // Unfold tensor into 3x3 matrix:
    // [S_uu, S_uv, S_uw]
    // [S_vu, S_vv, S_vw]
    // [S_wu, S_wv, S_ww]
    std::array<std::array<double, 3>, 3> S_mat = {{
        {S_original[0], S_original[1], S_original[2]},
        {S_original[1], S_original[3], S_original[4]},
        {S_original[2], S_original[4], S_original[5]}
    }};
    
    // Perform rotation: S' = R * S * R^T
    std::array<std::array<double, 3>, 3> S_rotated = {{}};
    
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            S_rotated[i][j] = 0.0;
            for (int k = 0; k < 3; ++k) {
                for (int l = 0; l < 3; ++l) {
                    S_rotated[i][j] += R[i][k] * S_mat[k][l] * R[j][l];
                }
            }
        }
    }
    
    // Fold back into 9-component array
    std::array<double, 9> S_rotated_flat = {{
        S_rotated[0][0],  // S_uu
        S_rotated[0][1],  // S_uv
        S_rotated[0][2],  // S_uw
        S_rotated[1][1],  // S_vv
        S_rotated[1][2],  // S_vw
        S_rotated[2][2],  // S_ww
        0.0, 0.0, 0.0    // Spare slots
    }};
    
    return S_rotated_flat;
}

/**
 * Compute wind direction as function of height (wind veer).
 *
 * Implements the power-law veer model:
 *   θ(z) = θ_ref + Δθ * (z / z_ref)^α
 *
 * Typical veering:
 *   - Neutral: 0-5° over 100m
 *   - Stable: 20-40° over 100m (more shear)
 *   - Unstable: 0-10° over 100m (less shear)
 *
 * @param z                Height above ground (m)
 * @param z_ref            Reference height where wind_direction applies (m)
 * @param wind_dir_ref     Reference wind direction (degrees)
 * @param veer_rate        Veering rate (degrees per 100m height increase)
 * @param veer_power       Power-law exponent for veering profile (typical: 0.2-0.3)
 * @return                 Wind direction at height z (degrees, 0-360)
 */
inline double compute_veered_wind_direction(
    double z, double z_ref, double wind_dir_ref,
    double veer_rate, double veer_power = 0.25) {
    
    // Avoid division by zero or negative heights
    if (z_ref <= 0.0 || z < 0.0) return wind_dir_ref;
    
    // Veer angle (degrees)
    // veer_rate is in degrees per 100m, so normalize to per meter
    double veer_factor = veer_rate / 100.0;
    
    // Power-law veering: Δθ = veer_factor * 100m * (z/z_ref)^α
    double height_ratio = z / z_ref;
    double veer_angle = veer_factor * 100.0 * std::pow(height_ratio, veer_power);
    
    // Total wind direction at height z
    double theta_z = wind_dir_ref + veer_angle;
    
    // Normalize to 0-360 range
    while (theta_z < 0.0) theta_z += 360.0;
    while (theta_z >= 360.0) theta_z -= 360.0;
    
    return theta_z;
}

/**
 * Modify spectral tensor for directional effects.
 *
 * Applies directional attenuation to coherence and cross-correlations
 * based on wind veering and directional preferences.
 *
 * @param S_tensor          Input spectral tensor (9 components)
 * @param veer_angle        Total veer angle at this height (radians)
 * @param cross_wind_bias   Cross-wind asymmetry factor (-1 to 1)
 * @param kappa_dir         Directional coherence factor (0 to 1)
 * @return                  Modified spectral tensor with directional effects
 */
inline std::array<double, 9> apply_directional_modifications(
    const std::array<double, 9>& S_tensor,
    double veer_angle,
    double cross_wind_bias,
    double kappa_dir) {
    
    std::array<double, 9> S_modified = S_tensor;
    
    // Apply directional coherence attenuation
    // Cross-correlations are reduced by directional effects
    double coherence_factor = 1.0 - (1.0 - kappa_dir) * std::abs(veer_angle);
    coherence_factor = std::max(0.0, std::min(1.0, coherence_factor));
    
    // Reduce off-diagonal terms (cross-correlations) by coherence factor
    S_modified[1] *= coherence_factor;  // S_uv
    S_modified[2] *= coherence_factor;  // S_uw
    S_modified[4] *= coherence_factor;  // S_vw
    
    // Apply cross-wind asymmetry
    if (std::abs(cross_wind_bias) > 1e-6) {
        // Bias increases v-component variance, decreases w slightly
        double v_scale = 1.0 + 0.2 * cross_wind_bias;
        double w_scale = 1.0 - 0.1 * cross_wind_bias;
        
        S_modified[3] *= v_scale * v_scale;  // S_vv
        S_modified[5] *= w_scale * w_scale;  // S_ww
    }
    
    return S_modified;
}

/**
 * Compute anisotropy modification from directional bias.
 *
 * In situations with preferred flow directions (e.g., valley channeling,
 * coastal sea breeze), the anisotropy ratios can change.
 *
 * @param directional_param  Directional parameters struct
 * @param anisotropy_v_base  Base v-component anisotropy ratio
 * @param anisotropy_w_base  Base w-component anisotropy ratio
 * @return                   Modified anisotropy ratios (v_ratio, w_ratio)
 */
inline std::pair<double, double> modify_anisotropy_from_direction(
    const DirectionalParameters& directional_param,
    double anisotropy_v_base,
    double anisotropy_w_base) {
    
    // Cross-wind bias affects anisotropy
    double v_ratio = anisotropy_v_base;
    double w_ratio = anisotropy_w_base;
    
    // Positive cross_wind_bias enhances v-component (crosswind)
    if (directional_param.cross_wind_bias > 0.0) {
        v_ratio *= (1.0 + 0.15 * directional_param.cross_wind_bias);
        w_ratio *= (1.0 - 0.05 * directional_param.cross_wind_bias);
    } else if (directional_param.cross_wind_bias < 0.0) {
        v_ratio *= (1.0 + 0.05 * directional_param.cross_wind_bias);
        w_ratio *= (1.0 - 0.10 * directional_param.cross_wind_bias);
    }
    
    // Clamp to physical ranges
    v_ratio = std::max(0.1, std::min(1.0, v_ratio));
    w_ratio = std::max(0.05, std::min(1.0, w_ratio));
    
    return {v_ratio, w_ratio};
}

/**
 * Create full directional transformation for a given height.
 *
 * Combines rotation matrix and directional modifications into a complete
 * transformation for use in spectral tensor synthesis.
 *
 * @param z                Height above ground (m)
 * @param z_ref            Reference height (m)
 * @param directional_params  Directional parameters
 * @param S_tensor         Input spectral tensor (will be modified)
 * @return                 Rotation matrix to apply at this height
 */
inline RotationMatrix create_height_dependent_rotation(
    double z, double z_ref,
    const DirectionalParameters& directional_params,
    std::array<double, 9>& S_tensor) {
    
    // Compute veered wind direction at height z
    double veer_dir = compute_veered_wind_direction(
        z, z_ref,
        directional_params.wind_direction,
        directional_params.veer_rate
    );
    
    // Create rotation matrix for this direction
    RotationMatrix R = create_rotation_matrix_from_direction(veer_dir);
    
    // Apply directional modifications to spectral tensor
    double veer_angle = (directional_params.veer_rate / 100.0) * 
                       std::pow(z / z_ref, 0.25) * M_PI / 180.0;
    
    S_tensor = apply_directional_modifications(
        S_tensor,
        veer_angle,
        directional_params.cross_wind_bias,
        directional_params.kappa_directional
    );
    
    return R;
}

#endif // MANN_BOX_DIRECTIONAL_ROTATION_H_
