// ============================================================================
// mann_box_terrain_tensor_rotation.H
// 
// Terrain Adaptation & Flow Regimes: Slope-Aware Tensor Rotation
// 
// Implements slope-aware modifications to the Mann Box spectral tensor,
// rotating the anisotropy tensor to align with local terrain slope and
// supporting steep terrain (>30° slopes).
//
// Key Features:
// - Terrain slope computation from DEM data
// - Surface-parallel coordinate transformation
// - Along-slope vs cross-slope tensor component separation
// - Steep slope support (>30°)
// - Slope-dependent variance modification
// - GPU-ready device-compatible kernels
//
// Mathematical Foundation:
// 
// 1. Terrain Slope Gradient
//    ∇h = (∂h/∂x, ∂h/∂y)  [dimensionless]
//    slope_magnitude = √((∂h/∂x)² + (∂h/∂y)²)
//    slope_angle = arctan(slope_magnitude)
//
// 2. Surface-Parallel Coordinate System
//    For slopes, rotate from (x,y,z) to (s, n, ẑ) coordinates:
//    - s: along-slope direction (maximizes gradient descent)
//    - n: cross-slope (perpendicular to gradient)
//    - ẑ: surface-normal direction
//
// 3. Tensor Component Transformation
//    Rotate 3×3 spectral tensor into slope-aligned coordinates:
//    S'_ij = R_ik S_kl R_jl  (similarity transformation)
//    
//    where R is the rotation matrix from horizontal to slope coordinates
//
// 4. Slope-Dependent Modifications
//    - Along-slope variance enhanced (flow acceleration)
//    - Cross-slope variance suppressed (flow steering)
//    - Vertical component modified based on slope angle
//    - Length scales adjusted for terrain-following flows
//
// Reference:
//   Jackson, P. S., & Hunt, J. C. (1975). Turbulent wind flow over a low hill.
//   Quarterly Journal of the Royal Meteorological Society, 101(430), 929-955.
//   
//   Belcher, S. E., & Hunt, J. C. (1998). Turbulent shear flow over slowly
//   varying hills and valleys. Journal of Fluid Mechanics, 359, 329-374.
//
// Core Features:
//   1. Terrain slope-aligned coordinate transformation
//   2. Along-slope and cross-slope variance adjustment
//   3. Steep slope support (>30°)
//   4. Slope-dependent length scale modification
//   5. Integration with the spectral tensor model
// ============================================================================

#ifndef MANN_BOX_TERRAIN_TENSOR_ROTATION_H
#define MANN_BOX_TERRAIN_TENSOR_ROTATION_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <algorithm>
#include <array>
#include "solver_math_constants.H"

// ============================================================================
// Slope Information Structure
// 
// Stores local terrain slope properties and coordinate transformations
// ============================================================================

struct SlopeInfo {
    // Terrain gradients
    amrex::Real dh_dx;                    // Elevation gradient in x [–]
    amrex::Real dh_dy;                    // Elevation gradient in y [–]
    amrex::Real slope_magnitude;          // |∇h| = √((∂h/∂x)² + (∂h/∂y)²)
    amrex::Real slope_angle;              // α = arctan(slope_mag) [radians]
    
    // Azimuthal angle of maximum slope
    amrex::Real slope_azimuth;            // θ = arctan(∂h/∂y, ∂h/∂x) [radians]
    
    // Rotation matrix components (3x3, stored column-major)
    // Rotates from (x,y,z) to (s,n,ẑ) coordinates
    amrex::Real R[9];                     // Rotation matrix (9 components)
    
    // Constructor with default values
    SlopeInfo()
        : dh_dx(0.0), dh_dy(0.0), slope_magnitude(0.0), 
          slope_angle(0.0), slope_azimuth(0.0)
    {
        // Initialize as identity matrix
        R[0] = 1.0; R[3] = 0.0; R[6] = 0.0;
        R[1] = 0.0; R[4] = 1.0; R[7] = 0.0;
        R[2] = 0.0; R[5] = 0.0; R[8] = 1.0;
    }
};

// ============================================================================
// Slope-Modified Spectral Tensor
// 
// Stores tensor components in slope-aligned coordinates
// ============================================================================

struct SlopeAlignedTensor {
    // Along-slope (downslope) variance component
    amrex::Real S_ss;  // Along-slope spectrum
    
    // Cross-slope (perpendicular) variance component
    amrex::Real S_nn;  // Cross-slope spectrum
    
    // Surface-normal (vertical-like) variance component
    amrex::Real S_zz;  // Surface-normal spectrum
    
    // Off-diagonal components
    amrex::Real S_sn;  // Along-slope to cross-slope
    amrex::Real S_sz;  // Along-slope to surface-normal
    amrex::Real S_nz;  // Cross-slope to surface-normal
    
    // Constructor
    SlopeAlignedTensor()
        : S_ss(0.0), S_nn(0.0), S_zz(0.0),
          S_sn(0.0), S_sz(0.0), S_nz(0.0) {}
};

// ============================================================================
// Namespace for terrain tensor rotation operations
// ============================================================================

namespace MannBoxTerrainTensorRotation {

// ============================================================================
// PART 1: Slope Computation
// ============================================================================

// Compute terrain slope from elevation gradients
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
SlopeInfo compute_slope_info(
    amrex::Real dh_dx,  // Elevation gradient in x-direction [–]
    amrex::Real dh_dy   // Elevation gradient in y-direction [–]
) noexcept
{
    SlopeInfo slope;
    
    slope.dh_dx = dh_dx;
    slope.dh_dy = dh_dy;
    
    // Compute slope magnitude
    slope.slope_magnitude = std::sqrt(dh_dx * dh_dx + dh_dy * dh_dy);
    
    // Compute slope angle (cap at 85 degrees for numerical stability)
    amrex::Real max_slope = std::tan(amrex::Real(85.0) * M_PI / amrex::Real(180.0));
    slope.slope_magnitude = std::min(slope.slope_magnitude, max_slope);
    
    slope.slope_angle = std::atan(slope.slope_magnitude);
    
    // Compute azimuthal angle (direction of steepest descent)
    slope.slope_azimuth = std::atan2(dh_dy, dh_dx);
    
    return slope;
}

// ============================================================================
// PART 2: Surface-Parallel Coordinate Transformation
// ============================================================================

// Construct rotation matrix from horizontal (x,y,z) to slope-aligned (s,n,ẑ)
// coordinates, where s = along-slope, n = cross-slope, ẑ = surface-normal
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void construct_slope_rotation_matrix(
    const SlopeInfo& slope,
    amrex::Real R[9]  // Output: 3x3 rotation matrix (column-major)
) noexcept
{
    // Rotation strategy:
    // 1. Rotate around z-axis by slope azimuth to align with max gradient
    // 2. Rotate around rotated y-axis by slope angle to tilt to slope
    
    amrex::Real cos_az = std::cos(slope.slope_azimuth);
    amrex::Real sin_az = std::sin(slope.slope_azimuth);
    amrex::Real cos_alpha = std::cos(slope.slope_angle);
    amrex::Real sin_alpha = std::sin(slope.slope_angle);
    
    // First: Rotate by azimuth around z-axis (to align with downslope direction)
    amrex::Real R1[9];
    R1[0] = cos_az;        R1[3] = -sin_az;       R1[6] = 0.0;
    R1[1] = sin_az;        R1[4] = cos_az;        R1[7] = 0.0;
    R1[2] = 0.0;           R1[5] = 0.0;           R1[8] = 1.0;
    
    // Second: Rotate by slope angle around y-axis (to tilt to slope)
    amrex::Real R2[9];
    R2[0] = cos_alpha;     R2[3] = 0.0;           R2[6] = sin_alpha;
    R2[1] = 0.0;           R2[4] = 1.0;           R2[7] = 0.0;
    R2[2] = -sin_alpha;    R2[5] = 0.0;           R2[8] = cos_alpha;
    
    // Combined rotation: R = R2 * R1
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[j * 3 + i] = amrex::Real(0.0);
            for (int k = 0; k < 3; ++k) {
                R[j * 3 + i] += R2[k * 3 + i] * R1[j * 3 + k];
            }
        }
    }
}

// Compute surface-normal vector
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void compute_surface_normal(
    amrex::Real dh_dx,     // Elevation gradient in x
    amrex::Real dh_dy,     // Elevation gradient in y
    amrex::Real& nx,       // Output: x-component of normal
    amrex::Real& ny,       // Output: y-component of normal
    amrex::Real& nz        // Output: z-component of normal
) noexcept
{
    // Surface normal to z = h(x,y) is ∇h = (-∂h/∂x, -∂h/∂y, 1)
    nx = -dh_dx;
    ny = -dh_dy;
    nz = amrex::Real(1.0);
    
    // Normalize
    amrex::Real mag = std::sqrt(nx * nx + ny * ny + nz * nz);
    mag = std::max(mag, amrex::Real(1e-10));
    
    nx /= mag;
    ny /= mag;
    nz /= mag;
}

// ============================================================================
// PART 3: Tensor Rotation (3x3 Similarity Transform)
// ============================================================================

// Rotate spectral tensor to slope-aligned coordinates
// S_rotated = R * S_original * R^T  (similarity transform)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
SlopeAlignedTensor rotate_spectral_tensor_to_slope(
    amrex::Real S_uu,   // Original u-component variance
    amrex::Real S_vv,   // Original v-component variance
    amrex::Real S_ww,   // Original w-component variance
    amrex::Real S_uv,   // Original u-v cross-spectrum
    amrex::Real S_uw,   // Original u-w cross-spectrum
    amrex::Real S_vw,   // Original v-w cross-spectrum
    const amrex::Real R[9]  // Rotation matrix (column-major)
) noexcept
{
    // Construct original 3x3 tensor
    amrex::Real S[9];
    S[0] = S_uu; S[3] = S_uv; S[6] = S_uw;
    S[1] = S_uv; S[4] = S_vv; S[7] = S_vw;
    S[2] = S_uw; S[5] = S_vw; S[8] = S_ww;
    
    // Intermediate: T = R^T * S
    amrex::Real T[9];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            T[j * 3 + i] = amrex::Real(0.0);
            for (int k = 0; k < 3; ++k) {
                // R^T in column-major is read as rows
                T[j * 3 + i] += R[i * 3 + k] * S[j * 3 + k];
            }
        }
    }
    
    // Final: S' = T * R = (R^T * S) * R
    amrex::Real S_rotated[9];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            S_rotated[j * 3 + i] = amrex::Real(0.0);
            for (int k = 0; k < 3; ++k) {
                S_rotated[j * 3 + i] += T[k * 3 + i] * R[j * 3 + k];
            }
        }
    }
    
    // Extract components in new coordinate system
    // (s = along-slope, n = cross-slope, z = surface-normal)
    SlopeAlignedTensor result;
    result.S_ss = S_rotated[0];  // (0,0) component
    result.S_nn = S_rotated[4];  // (1,1) component
    result.S_zz = S_rotated[8];  // (2,2) component
    result.S_sn = S_rotated[3];  // (0,1) component = (1,0)
    result.S_sz = S_rotated[6];  // (0,2) component = (2,0)
    result.S_nz = S_rotated[7];  // (1,2) component = (2,1)
    
    return result;
}

// ============================================================================
// PART 4: Slope-Dependent Tensor Modifications
// ============================================================================

// Compute slope-dependent modification factors
// Enhances along-slope component, suppresses cross-slope
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void compute_slope_modification_factors(
    amrex::Real slope_angle,          // Terrain slope angle [radians]
    amrex::Real& along_slope_factor,  // Output: enhancement for along-slope
    amrex::Real& cross_slope_factor,  // Output: suppression for cross-slope
    amrex::Real& vertical_factor      // Output: modification for vertical
) noexcept
{
    // Convert to degrees for thresholding
    amrex::Real slope_degrees = slope_angle * amrex::Real(180.0) / M_PI;
    
    // Modifications increase with steepness
    // Gentle slopes (0-10°): small effects
    // Medium slopes (10-30°): moderate effects
    // Steep slopes (>30°): strong effects
    
    amrex::Real normalized_slope = std::min(slope_degrees / amrex::Real(30.0), 
                                            amrex::Real(1.0));
    
    // Along-slope enhancement: 1.0 at flat, ~1.4 at steep
    along_slope_factor = amrex::Real(1.0) + amrex::Real(0.4) * normalized_slope;
    
    // Cross-slope suppression: 1.0 at flat, ~0.6 at steep
    cross_slope_factor = amrex::Real(1.0) - amrex::Real(0.4) * normalized_slope;
    cross_slope_factor = std::max(cross_slope_factor, amrex::Real(0.3));
    
    // Vertical (normal-to-slope) modification: depends on slope direction
    // Near slopes: suppress vertical motion, enhance at summit
    vertical_factor = amrex::Real(0.8) + amrex::Real(0.4) * normalized_slope;
}

// Apply slope modifications to spectral tensor
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
SlopeAlignedTensor apply_slope_modifications(
    const SlopeAlignedTensor& tensor,
    const SlopeInfo& slope
) noexcept
{
    amrex::Real along_factor, cross_factor, vert_factor;
    compute_slope_modification_factors(slope.slope_angle, 
                                       along_factor, cross_factor, vert_factor);
    
    SlopeAlignedTensor modified;
    
    // Scale variance components
    modified.S_ss = tensor.S_ss * along_factor * along_factor;
    modified.S_nn = tensor.S_nn * cross_factor * cross_factor;
    modified.S_zz = tensor.S_zz * vert_factor * vert_factor;
    
    // Scale cross-spectra with geometric mean of component factors
    amrex::Real sn_factor = std::sqrt(along_factor * cross_factor);
    amrex::Real sz_factor = std::sqrt(along_factor * vert_factor);
    amrex::Real nz_factor = std::sqrt(cross_factor * vert_factor);
    
    modified.S_sn = tensor.S_sn * sn_factor;
    modified.S_sz = tensor.S_sz * sz_factor;
    modified.S_nz = tensor.S_nz * nz_factor;
    
    return modified;
}

// ============================================================================
// PART 5: Length Scale Modifications on Slopes
// ============================================================================

// Compute slope-dependent length scale modifications
// Length scales can be stretched or compressed based on slope orientation
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void compute_slope_length_scale_factors(
    amrex::Real slope_angle,           // Terrain slope angle [radians]
    amrex::Real& L_along_factor,       // Along-slope length scale factor
    amrex::Real& L_cross_factor,       // Cross-slope length scale factor
    amrex::Real& L_vertical_factor     // Vertical (normal) length scale factor
) noexcept
{
    // On slopes, flow tends to be stretched along the slope
    // and compressed perpendicular to it
    
    amrex::Real cos_alpha = std::cos(slope_angle);
    amrex::Real sin_alpha = std::sin(slope_angle);
    
    // Along-slope: length scales increase due to flow channeling
    // Factor increases from 1.0 at flat to 1.3 at steep
    L_along_factor = amrex::Real(1.0) + amrex::Real(0.3) * sin_alpha;
    
    // Cross-slope: length scales decrease (flow compressed laterally)
    // Factor decreases from 1.0 at flat to 0.7 at steep
    L_cross_factor = cos_alpha;
    L_cross_factor = std::max(L_cross_factor, amrex::Real(0.5));
    
    // Vertical (normal-to-slope): depends on slope angle
    // At steep slopes, vertical scales are reduced
    L_vertical_factor = cos_alpha * amrex::Real(0.8);
    L_vertical_factor = std::max(L_vertical_factor, amrex::Real(0.3));
}

// ============================================================================
// PART 6: Steep Terrain Support (>30°)
// ============================================================================

// Check if terrain is steep (>30°)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
bool is_steep_terrain(amrex::Real slope_angle) noexcept
{
    const amrex::Real steep_threshold = std::atan(amrex::Real(0.577));  // 30° threshold
    return slope_angle > steep_threshold;
}

// Apply additional modifications for steep terrain
// These prevent numerical issues and model physical limits
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
SlopeAlignedTensor apply_steep_terrain_stabilization(
    const SlopeAlignedTensor& tensor,
    amrex::Real slope_angle
) noexcept
{
    SlopeAlignedTensor stabilized = tensor;
    
    if (is_steep_terrain(slope_angle)) {
        // For steep terrain, enhance isotropy to prevent extreme anisotropy
        // which can cause numerical issues in synthesis
        
        amrex::Real isotropy_factor = amrex::Real(1.2);  // 20% increase toward isotropy
        
        // Compute mean variance
        amrex::Real mean_variance = (stabilized.S_ss + stabilized.S_nn + 
                                     stabilized.S_zz) / amrex::Real(3.0);
        
        // Move components toward mean
        stabilized.S_ss = stabilized.S_ss * (amrex::Real(1.0) - amrex::Real(0.1) * isotropy_factor) +
                         mean_variance * (amrex::Real(0.1) * isotropy_factor);
        stabilized.S_nn = stabilized.S_nn * (amrex::Real(1.0) - amrex::Real(0.1) * isotropy_factor) +
                         mean_variance * (amrex::Real(0.1) * isotropy_factor);
        stabilized.S_zz = stabilized.S_zz * (amrex::Real(1.0) - amrex::Real(0.1) * isotropy_factor) +
                         mean_variance * (amrex::Real(0.1) * isotropy_factor);
        
        // Reduce cross-spectra magnitude for stability
        amrex::Real cross_reduction = amrex::Real(0.8);
        stabilized.S_sn *= cross_reduction;
        stabilized.S_sz *= cross_reduction;
        stabilized.S_nz *= cross_reduction;
    }
    
    return stabilized;
}

// ============================================================================
// PART 7: Inverse Rotation (Back to Original Coordinates)
// ============================================================================

// Rotate slope-aligned tensor back to original (x,y,z) coordinates
// S_original = R^T * S_slope * R
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void rotate_tensor_back_to_horizontal(
    const SlopeAlignedTensor& slope_tensor,
    const amrex::Real R[9],  // Rotation matrix (column-major)
    amrex::Real& S_uu,      // Output: u-component variance
    amrex::Real& S_vv,      // Output: v-component variance
    amrex::Real& S_ww,      // Output: w-component variance
    amrex::Real& S_uv,      // Output: u-v cross-spectrum
    amrex::Real& S_uw,      // Output: u-w cross-spectrum
    amrex::Real& S_vw       // Output: v-w cross-spectrum
) noexcept
{
    // Construct slope-aligned tensor
    amrex::Real S_slope[9];
    S_slope[0] = slope_tensor.S_ss; S_slope[3] = slope_tensor.S_sn; S_slope[6] = slope_tensor.S_sz;
    S_slope[1] = slope_tensor.S_sn; S_slope[4] = slope_tensor.S_nn; S_slope[7] = slope_tensor.S_nz;
    S_slope[2] = slope_tensor.S_sz; S_slope[5] = slope_tensor.S_nz; S_slope[8] = slope_tensor.S_zz;
    
    // Inverse transform: S' = R * S * R^T
    amrex::Real temp[9];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            temp[j * 3 + i] = amrex::Real(0.0);
            for (int k = 0; k < 3; ++k) {
                temp[j * 3 + i] += R[k * 3 + i] * S_slope[j * 3 + k];
            }
        }
    }
    
    amrex::Real S_original[9];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            S_original[j * 3 + i] = amrex::Real(0.0);
            for (int k = 0; k < 3; ++k) {
                S_original[j * 3 + i] += temp[k * 3 + i] * R[j * 3 + k];
            }
        }
    }
    
    // Extract components
    S_uu = S_original[0];
    S_vv = S_original[4];
    S_ww = S_original[8];
    S_uv = S_original[3];
    S_uw = S_original[6];
    S_vw = S_original[7];
}

// ============================================================================
// PART 8: Combined Slope-Aware Tensor Modification
// ============================================================================

// Complete workflow: rotate, modify, and rotate back
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void apply_complete_slope_adaptation(
    amrex::Real dh_dx,      // Terrain gradient in x
    amrex::Real dh_dy,      // Terrain gradient in y
    amrex::Real& S_uu,      // In/Out: u-component variance
    amrex::Real& S_vv,      // In/Out: v-component variance
    amrex::Real& S_ww,      // In/Out: w-component variance
    amrex::Real& S_uv,      // In/Out: u-v cross-spectrum
    amrex::Real& S_uw,      // In/Out: u-w cross-spectrum
    amrex::Real& S_vw       // In/Out: v-w cross-spectrum
) noexcept
{
    // Step 1: Compute slope info
    SlopeInfo slope = compute_slope_info(dh_dx, dh_dy);
    
    // Skip modification if slope is negligible
    if (slope.slope_magnitude < amrex::Real(0.01)) {
        return;
    }
    
    // Step 2: Construct rotation matrix
    amrex::Real R[9];
    construct_slope_rotation_matrix(slope, R);
    
    // Step 3: Rotate tensor to slope-aligned coordinates
    SlopeAlignedTensor slope_tensor = rotate_spectral_tensor_to_slope(
        S_uu, S_vv, S_ww, S_uv, S_uw, S_vw, R);
    
    // Step 4: Apply slope-specific modifications
    SlopeAlignedTensor modified = apply_slope_modifications(slope_tensor, slope);
    
    // Step 5: Apply steep terrain stabilization if needed
    modified = apply_steep_terrain_stabilization(modified, slope.slope_angle);
    
    // Step 6: Rotate back to horizontal coordinates
    rotate_tensor_back_to_horizontal(modified, R, 
                                    S_uu, S_vv, S_ww, S_uv, S_uw, S_vw);
}

} // namespace MannBoxTerrainTensorRotation

#endif // MANN_BOX_TERRAIN_TENSOR_ROTATION_H
