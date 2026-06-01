// ============================================================================
// wall_functions.H
// Wall function models for near-wall boundary treatment
//
// This header provides functions for applying log-law wall functions at
// solid boundaries (terrain, flat surfaces, building walls) instead of
// traditional no-slip (zero velocity) boundary conditions.
//
// Wall functions are appropriate for high Reynolds number turbulent flows
// where the near-wall region cannot be resolved with the grid spacing.
// They provide more realistic velocity profiles near walls using the
// turbulent law-of-the-wall (log-law).
//
// Models implemented:
// 1. Flat surface wall function (simplest case: horizontal surface)
// 2. Terrain wall function (non-horizontal surfaces with normal vectors)
// 3. Building wall function (vertical and horizontal building faces)
//
// References:
// - Launder, B.E., & Spalding, D.B. (1974). The numerical computation of
//   turbulent flows. Computer Methods in Applied Mechanics and Engineering,
//   3(2), 269-289.
// - Pope, S.B. (2000). Turbulent Flows. Cambridge University Press.
// - Blocken, B., et al. (2007). CFD simulation of the atmospheric boundary
//   layer: wall function problems. Atmospheric Environment, 41(2), 238-252.
// ============================================================================

#ifndef WALL_FUNCTIONS_H
#define WALL_FUNCTIONS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include "math_constants.H"
#include <cmath>

// ============================================================================
// Wall function parameters structure
// ============================================================================
struct WallFunctionParams {
    bool enabled = false;                           // Master enable/disable flag
    bool terrain_wall_function = false;             // Enable for terrain surfaces
    bool flat_surface_wall_function = false;        // Enable for flat surfaces
    bool building_wall_function = false;            // Enable for building walls
    
    amrex::Real z0_building = 0.001;                // Building wall roughness [m]
    amrex::Real z0_flat = 0.01;                     // Flat surface roughness [m]
    amrex::Real blend_height = 2.0;                 // Blending layer height [cells]
    amrex::Real max_distance = 3.0;                 // Max distance for wall function [cells]
    
    amrex::Real flat_surface_elevation = 0.0;       // Elevation of flat surface [m]
    bool enable_flat_surface = false;               // Use flat surface instead of terrain
    
    amrex::Real min_wall_distance = 0.1;            // Minimum distance from wall [m]
    amrex::Real kappa = 0.41;                       // von Karman constant
    
    // Stability correction parameters
    bool enable_stability_correction = false;       // Enable Monin-Obukhov stability corrections
    amrex::Real stability_length = 1.0e10;          // Obukhov length L [m] (large = neutral)
    
    // Adaptive activation parameters
    bool enable_adaptive_activation = false;        // Enable automatic on/off based on grid resolution
    amrex::Real adaptive_resolution_threshold = 30.0; // Activate only if dz < threshold*z0 [dimensionless]
    amrex::Real adaptive_min_cells_in_log_layer = 3.0; // Minimum cells in log layer for activation
};

// ============================================================================
// Check if grid resolution is fine enough to activate wall functions
//
// Wall functions are appropriate when the first cell height is within the
// log-layer but not too close to the wall. Typical criteria:
//   - First cell height z > z0 (above roughness)
//   - First cell height z < 0.1*boundary layer height (in log layer)
//   - At least min_cells cells within log layer region
//
// Inputs:
//   dz            - Grid spacing in wall-normal direction [m]
//   z0            - Surface roughness length [m]
//   resolution_threshold - Max ratio dz/z0 for activation (default: 30)
//   min_cells_in_log - Minimum cells in log layer (default: 3)
//
// Returns:
//   true if grid is fine enough for wall functions, false otherwise
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool is_grid_resolution_suitable_for_wall_function(
    amrex::Real dz,
    amrex::Real z0,
    amrex::Real resolution_threshold = amrex::Real(30.0),
    amrex::Real min_cells_in_log = amrex::Real(3.0))
{
    // Check if first cell is above roughness elements
    if (dz < z0) {
        return false;  // Grid too fine - first cell inside roughness
    }
    
    // Check if grid spacing is reasonable relative to roughness
    // dz/z0 < threshold means grid is fine enough to resolve log-layer
    amrex::Real ratio = dz / z0;
    if (ratio > resolution_threshold) {
        return false;  // Grid too coarse - cannot resolve log-layer
    }
    
    // Check if we have enough cells in the log-layer region
    // Log layer extends roughly to z = 0.1*δ where δ ~ 1000*z0 for ABL
    // For practical purposes, check if we have min_cells within first ~100*z0
    amrex::Real log_layer_height = amrex::Real(100.0) * z0;
    amrex::Real cells_in_log = log_layer_height / dz;
    if (cells_in_log < min_cells_in_log) {
        return false;  // Not enough grid points in log layer
    }
    
    return true;  // Grid resolution is suitable
}

// ============================================================================
// Blending function for smooth transition between wall function and outer flow
// Uses smooth Hermite interpolation (3rd order polynomial)
//
// Returns blend factor in [0, 1]:
//   0 = pure wall function (close to wall)
//   1 = pure outer flow (far from wall)
//
// Inputs:
//   distance      - Distance from wall [cells]
//   blend_start   - Start of blending region [cells]
//   blend_end     - End of blending region [cells]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real wall_function_blend(
    amrex::Real distance,
    amrex::Real blend_start,
    amrex::Real blend_end)
{
    if (distance <= blend_start) {
        return amrex::Real(0.0);  // Pure wall function
    } else if (distance >= blend_end) {
        return amrex::Real(1.0);  // Pure outer flow
    } else {
        // Smooth Hermite interpolation
        amrex::Real t = (distance - blend_start) / (blend_end - blend_start);
        return t * t * (amrex::Real(3.0) - amrex::Real(2.0) * t);
    }
}

// ============================================================================
// Compute friction velocity u* from velocity at a given height using log-law
//
// Given velocity magnitude |U| at height z above a surface with roughness z0,
// compute the friction velocity u* such that:
//   |U| = (u*/κ) * ln((z + z0) / z0)
//
// Rearranging:
//   u* = κ * |U| / ln((z + z0) / z0)
//
// Inputs:
//   speed         - Velocity magnitude at height z [m/s]
//   z             - Height above surface [m]
//   z0            - Surface roughness length [m]
//   kappa         - von Karman constant (default: 0.41)
//
// Returns:
//   u*            - Friction velocity [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_friction_velocity(
    amrex::Real speed,
    amrex::Real z,
    amrex::Real z0,
    amrex::Real kappa = amrex::Real(0.41))
{
    if (speed < amrex::Real(1.0e-10) || z < z0) {
        return amrex::Real(0.0);
    }
    
    amrex::Real log_term = std::log((z + z0) / z0);
    if (log_term < amrex::Real(1.0e-10)) {
        return amrex::Real(0.0);
    }
    
    return kappa * speed / log_term;
}

// ============================================================================
// Compute friction velocity with Monin-Obukhov stability correction
//
// Extends the neutral log-law to non-neutral atmospheric conditions using
// Businger-Dyer stability functions. This version solves:
//   U(z) = (u*/κ) * [ln((z+z0)/z0) - ψ_m(z/L) + ψ_m(z0/L)]
//
// For u* given U(z), requires iterative solution since ψ_m depends on u*.
// We use the neutral solution as first guess and apply one correction step.
//
// Inputs:
//   speed         - Velocity magnitude at height z [m/s]
//   z             - Height above surface [m]
//   z0            - Surface roughness length [m]
//   L_obukhov     - Obukhov length [m] (>0 stable, <0 unstable, large = neutral)
//   kappa         - von Karman constant (default: 0.41)
//
// Returns:
//   u*            - Friction velocity [m/s]
//
// Note: Requires psi_m function from stability_models.H
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_friction_velocity_stability(
    amrex::Real speed,
    amrex::Real z,
    amrex::Real z0,
    amrex::Real L_obukhov,
    amrex::Real kappa = amrex::Real(0.41))
{
    if (speed < amrex::Real(1.0e-10) || z < z0) {
        return amrex::Real(0.0);
    }
    
    // For neutral or nearly neutral conditions, use simple log-law
    if (std::abs(L_obukhov) > amrex::Real(1.0e8)) {
        return compute_friction_velocity(speed, z, z0, kappa);
    }
    
    // Stability parameters
    amrex::Real zeta = z / L_obukhov;
    amrex::Real zeta0 = z0 / L_obukhov;
    
    // Limit zeta to prevent numerical issues
    const amrex::Real zeta_max = amrex::Real(2.0);
    const amrex::Real zeta_min = amrex::Real(-2.0);
    zeta = (zeta > zeta_max) ? zeta_max : zeta;
    zeta = (zeta < zeta_min) ? zeta_min : zeta;
    
    // Compute stability function (requires psi_m from stability_models.H)
    // For stable: psi_m = -5*zeta
    // For unstable: psi_m = 2*ln((1+x)/2) + ln((1+x^2)/2) - 2*atan(x) + pi/2
    //               where x = (1-16*zeta)^0.25
    amrex::Real psi_m_z, psi_m_z0;
    
    if (zeta > amrex::Real(0.0)) {
        // Stable
        psi_m_z = -amrex::Real(5.0) * zeta;
        psi_m_z0 = -amrex::Real(5.0) * zeta0;
    } else if (zeta < amrex::Real(0.0)) {
        // Unstable
        const amrex::Real x = std::pow(amrex::Real(1.0) - amrex::Real(16.0) * zeta, amrex::Real(0.25));
        const amrex::Real x2 = x * x;
        psi_m_z = amrex::Real(2.0) * std::log((amrex::Real(1.0) + x) / amrex::Real(2.0))
                + std::log((amrex::Real(1.0) + x2) / amrex::Real(2.0))
                - amrex::Real(2.0) * std::atan(x)
                + amrex::Real(1.5707963267948966);  // pi/2
        
        const amrex::Real x0 = std::pow(amrex::Real(1.0) - amrex::Real(16.0) * zeta0, amrex::Real(0.25));
        const amrex::Real x02 = x0 * x0;
        psi_m_z0 = amrex::Real(2.0) * std::log((amrex::Real(1.0) + x0) / amrex::Real(2.0))
                 + std::log((amrex::Real(1.0) + x02) / amrex::Real(2.0))
                 - amrex::Real(2.0) * std::atan(x0)
                 + amrex::Real(1.5707963267948966);
    } else {
        // Neutral
        psi_m_z = amrex::Real(0.0);
        psi_m_z0 = amrex::Real(0.0);
    }
    
    // Modified log term with stability correction
    amrex::Real log_term = std::log((z + z0) / z0) - psi_m_z + psi_m_z0;
    if (log_term < amrex::Real(1.0e-10)) {
        return amrex::Real(0.0);
    }
    
    return kappa * speed / log_term;
}

// ============================================================================
// Apply log-law wall function to velocity components for flat horizontal surface
//
// For a flat surface (normal vector = [0, 0, 1]), the wall-parallel velocity
// is the horizontal velocity (u, v), and the wall-normal velocity is w.
//
// Wall function:
//   - Wall-normal component: w = 0 (impermeability)
//   - Wall-parallel magnitude: sqrt(u² + v²) = (u*/κ) * ln((z + z0) / z0)
//   - Direction preserved: (u, v) scaled by wall function
//
// Supports optional stability corrections via Monin-Obukhov similarity theory.
// Supports optional adaptive activation based on grid resolution.
//
// Inputs:
//   u, v, w       - Velocity components [m/s] (modified in place)
//   z             - Height above flat surface [m]
//   z0            - Surface roughness length [m]
//   u_ref         - Reference velocity magnitude for computing u* [m/s]
//   z_ref         - Reference height for computing u* [m]
//   dz            - Grid spacing (for adaptive activation) [m]
//   kappa         - von Karman constant
//   enable_stability - Enable stability corrections (default: false)
//   L_obukhov     - Obukhov length [m] (only used if enable_stability=true)
//   enable_adaptive - Enable adaptive activation (default: false)
//   adaptive_threshold - Resolution threshold for adaptive activation
//
// Algorithm:
// 1. Check if grid resolution is suitable (if adaptive mode enabled)
// 2. Compute u* from reference conditions (with or without stability)
// 3. Apply log-law to get wall-parallel speed at height z
// 4. Scale horizontal velocity to match log-law while preserving direction
// 5. Set vertical velocity to zero (impermeability)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_flat_surface_wall_function(
    amrex::Real& u,
    amrex::Real& v,
    amrex::Real& w,
    amrex::Real z,
    amrex::Real z0,
    amrex::Real u_ref,
    amrex::Real z_ref,
    amrex::Real dz,
    amrex::Real kappa = amrex::Real(0.41),
    bool enable_stability = false,
    amrex::Real L_obukhov = amrex::Real(1.0e10),
    bool enable_adaptive = false,
    amrex::Real adaptive_threshold = amrex::Real(30.0))
{
    // Ensure height is positive
    if (z <= amrex::Real(0.0)) {
        u = amrex::Real(0.0);
        v = amrex::Real(0.0);
        w = amrex::Real(0.0);
        return;
    }
    
    // Check grid resolution suitability (if adaptive activation enabled)
    if (enable_adaptive) {
        if (!is_grid_resolution_suitable_for_wall_function(dz, z0, adaptive_threshold)) {
            // Grid not suitable - don't modify velocity (use original field)
            return;
        }
    }
    
    // Compute friction velocity from reference conditions
    amrex::Real ustar;
    if (enable_stability && std::abs(L_obukhov) < amrex::Real(1.0e8)) {
        ustar = compute_friction_velocity_stability(u_ref, z_ref, z0, L_obukhov, kappa);
    } else {
        ustar = compute_friction_velocity(u_ref, z_ref, z0, kappa);
    }
    
    if (ustar < amrex::Real(1.0e-10)) {
        u = amrex::Real(0.0);
        v = amrex::Real(0.0);
        w = amrex::Real(0.0);
        return;
    }
    
    // Apply log-law (with optional stability correction) to get speed at height z
    amrex::Real speed_wall;
    if (enable_stability && std::abs(L_obukhov) < amrex::Real(1.0e8)) {
        // Stability-corrected log-law
        amrex::Real zeta = z / L_obukhov;
        amrex::Real zeta0 = z0 / L_obukhov;
        
        // Limit zeta
        const amrex::Real zeta_max = amrex::Real(2.0);
        const amrex::Real zeta_min = amrex::Real(-2.0);
        zeta = (zeta > zeta_max) ? zeta_max : zeta;
        zeta = (zeta < zeta_min) ? zeta_min : zeta;
        
        // Compute psi_m
        amrex::Real psi_m_z, psi_m_z0;
        if (zeta > amrex::Real(0.0)) {
            psi_m_z = -amrex::Real(5.0) * zeta;
            psi_m_z0 = -amrex::Real(5.0) * zeta0;
        } else if (zeta < amrex::Real(0.0)) {
            const amrex::Real x = std::pow(amrex::Real(1.0) - amrex::Real(16.0) * zeta, amrex::Real(0.25));
            const amrex::Real x2 = x * x;
            psi_m_z = amrex::Real(2.0) * std::log((amrex::Real(1.0) + x) / amrex::Real(2.0))
                    + std::log((amrex::Real(1.0) + x2) / amrex::Real(2.0))
                    - amrex::Real(2.0) * std::atan(x)
                    + amrex::Real(1.5707963267948966);
            
            const amrex::Real x0 = std::pow(amrex::Real(1.0) - amrex::Real(16.0) * zeta0, amrex::Real(0.25));
            const amrex::Real x02 = x0 * x0;
            psi_m_z0 = amrex::Real(2.0) * std::log((amrex::Real(1.0) + x0) / amrex::Real(2.0))
                     + std::log((amrex::Real(1.0) + x02) / amrex::Real(2.0))
                     - amrex::Real(2.0) * std::atan(x0)
                     + amrex::Real(1.5707963267948966);
        } else {
            psi_m_z = amrex::Real(0.0);
            psi_m_z0 = amrex::Real(0.0);
        }
        
        amrex::Real log_term = std::log((z + z0) / z0) - psi_m_z + psi_m_z0;
        if (log_term < amrex::Real(1.0e-10)) {
            u = amrex::Real(0.0);
            v = amrex::Real(0.0);
            w = amrex::Real(0.0);
            return;
        }
        speed_wall = (ustar / kappa) * log_term;
    } else {
        // Neutral log-law
        amrex::Real log_term = std::log((z + z0) / z0);
        if (log_term < amrex::Real(1.0e-10)) {
            u = amrex::Real(0.0);
            v = amrex::Real(0.0);
            w = amrex::Real(0.0);
            return;
        }
        speed_wall = (ustar / kappa) * log_term;
    }
    
    // Current horizontal speed
    amrex::Real speed_current = std::sqrt(u * u + v * v);
    
    if (speed_current > amrex::Real(1.0e-10)) {
        // Scale velocity to match wall function while preserving direction
        amrex::Real scale = speed_wall / speed_current;
        u *= scale;
        v *= scale;
    } else {
        // No horizontal velocity - cannot determine direction
        u = amrex::Real(0.0);
        v = amrex::Real(0.0);
    }
    
    // Apply impermeability: no flow through surface
    w = amrex::Real(0.0);
}

// ============================================================================
// Apply log-law wall function with blending to outer flow
//
// This version blends between the wall function (near wall) and the
// existing velocity field (outer flow) using a smooth blending function.
//
// Supports optional stability corrections and adaptive activation.
//
// Inputs:
//   u, v, w       - Velocity components [m/s] (modified in place)
//   u_outer, v_outer, w_outer - Outer flow velocity [m/s]
//   z             - Height above surface [m]
//   z0            - Surface roughness length [m]
//   u_ref         - Reference velocity magnitude [m/s]
//   z_ref         - Reference height [m]
//   dz            - Grid spacing in z [m]
//   blend_height  - Blending layer height [cells]
//   kappa         - von Karman constant
//   enable_stability - Enable stability corrections (default: false)
//   L_obukhov     - Obukhov length [m] (only used if enable_stability=true)
//   enable_adaptive - Enable adaptive activation (default: false)
//   adaptive_threshold - Resolution threshold for adaptive activation
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_flat_surface_wall_function_blended(
    amrex::Real& u,
    amrex::Real& v,
    amrex::Real& w,
    amrex::Real u_outer,
    amrex::Real v_outer,
    amrex::Real w_outer,
    amrex::Real z,
    amrex::Real z0,
    amrex::Real u_ref,
    amrex::Real z_ref,
    amrex::Real dz,
    amrex::Real blend_height = amrex::Real(2.0),
    amrex::Real kappa = amrex::Real(0.41),
    bool enable_stability = false,
    amrex::Real L_obukhov = amrex::Real(1.0e10),
    bool enable_adaptive = false,
    amrex::Real adaptive_threshold = amrex::Real(30.0))
{
    // Check adaptive activation first
    if (enable_adaptive) {
        if (!is_grid_resolution_suitable_for_wall_function(dz, z0, adaptive_threshold)) {
            // Grid not suitable - use outer flow
            u = u_outer;
            v = v_outer;
            w = w_outer;
            return;
        }
    }
    
    // Compute distance from surface in cells
    amrex::Real distance_cells = z / dz;
    
    // Determine blend factor
    amrex::Real blend_start = amrex::Real(0.5);  // Start blending after 0.5 cells
    amrex::Real blend_end = blend_height;        // Fully blended at blend_height cells
    amrex::Real blend = wall_function_blend(distance_cells, blend_start, blend_end);
    
    if (blend >= amrex::Real(0.999)) {
        // Far from wall - use outer flow
        u = u_outer;
        v = v_outer;
        w = w_outer;
        return;
    }
    
    // Apply wall function
    amrex::Real u_wall = u;
    amrex::Real v_wall = v;
    amrex::Real w_wall = w;
    apply_flat_surface_wall_function(u_wall, v_wall, w_wall, z, z0, u_ref, z_ref, dz, 
                                     kappa, enable_stability, L_obukhov, enable_adaptive, adaptive_threshold);
    
    // Blend wall function with outer flow
    u = (amrex::Real(1.0) - blend) * u_wall + blend * u_outer;
    v = (amrex::Real(1.0) - blend) * v_wall + blend * v_outer;
    w = (amrex::Real(1.0) - blend) * w_wall + blend * w_outer;
}

// ============================================================================
// Compute terrain normal vector from elevation gradients
//
// For terrain with elevation z_terrain(x, y), the surface is defined by:
//   F(x, y, z) = z - z_terrain(x, y) = 0
//
// The gradient (unnormalized normal) is:
//   ∇F = [-∂z_terrain/∂x, -∂z_terrain/∂y, 1]
//
// The normalized outward normal is:
//   n = ∇F / |∇F| = [-∂z_terrain/∂x, -∂z_terrain/∂y, 1] / sqrt(...)
//
// Inputs:
//   dz_dx         - Terrain slope in x-direction: ∂z_terrain/∂x
//   dz_dy         - Terrain slope in y-direction: ∂z_terrain/∂y
//   nx, ny, nz    - Normal vector components (output)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_terrain_normal(
    amrex::Real dz_dx,
    amrex::Real dz_dy,
    amrex::Real& nx,
    amrex::Real& ny,
    amrex::Real& nz)
{
    // Unnormalized normal: [-dz/dx, -dz/dy, 1]
    nx = -dz_dx;
    ny = -dz_dy;
    nz = amrex::Real(1.0);
    
    // Normalize
    amrex::Real norm = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (norm > amrex::Real(1.0e-10)) {
        nx /= norm;
        ny /= norm;
        nz /= norm;
    } else {
        // Flat terrain (no slope)
        nx = amrex::Real(0.0);
        ny = amrex::Real(0.0);
        nz = amrex::Real(1.0);
    }
}

// ============================================================================
// Decompose velocity into wall-parallel and wall-normal components
//
// Given velocity vector u = [u, v, w] and wall normal n = [nx, ny, nz]:
//   - Wall-normal component: u_n = (u · n) n
//   - Wall-parallel component: u_p = u - u_n
//
// Inputs:
//   u, v, w       - Velocity components
//   nx, ny, nz    - Wall normal vector (must be normalized)
//   u_par, v_par, w_par - Wall-parallel velocity (output)
//   u_norm        - Wall-normal velocity magnitude (output)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void decompose_velocity(
    amrex::Real u,
    amrex::Real v,
    amrex::Real w,
    amrex::Real nx,
    amrex::Real ny,
    amrex::Real nz,
    amrex::Real& u_par,
    amrex::Real& v_par,
    amrex::Real& w_par,
    amrex::Real& u_norm)
{
    // Wall-normal component magnitude: u · n
    u_norm = u * nx + v * ny + w * nz;
    
    // Wall-normal vector: (u · n) * n
    amrex::Real u_n = u_norm * nx;
    amrex::Real v_n = u_norm * ny;
    amrex::Real w_n = u_norm * nz;
    
    // Wall-parallel vector: u - (u · n) * n
    u_par = u - u_n;
    v_par = v - v_n;
    w_par = w - w_n;
}

// ============================================================================
// Apply log-law wall function to velocity for non-horizontal terrain
//
// For terrain with arbitrary slope, decompose velocity into wall-parallel
// and wall-normal components, apply log-law to parallel component, and
// enforce impermeability (zero normal component).
//
// Supports optional stability corrections and adaptive activation.
//
// Inputs:
//   u, v, w       - Velocity components [m/s] (modified in place)
//   nx, ny, nz    - Terrain normal vector (must be normalized)
//   z_agl         - Height above ground level (normal distance) [m]
//   z0            - Surface roughness length [m]
//   u_ref         - Reference velocity magnitude [m/s]
//   z_ref         - Reference height [m]
//   dz            - Grid spacing (for adaptive activation) [m]
//   kappa         - von Karman constant
//   enable_stability - Enable stability corrections (default: false)
//   L_obukhov     - Obukhov length [m] (only used if enable_stability=true)
//   enable_adaptive - Enable adaptive activation (default: false)
//   adaptive_threshold - Resolution threshold for adaptive activation
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_terrain_wall_function(
    amrex::Real& u,
    amrex::Real& v,
    amrex::Real& w,
    amrex::Real nx,
    amrex::Real ny,
    amrex::Real nz,
    amrex::Real z_agl,
    amrex::Real z0,
    amrex::Real u_ref,
    amrex::Real z_ref,
    amrex::Real dz,
    amrex::Real kappa = amrex::Real(0.41),
    bool enable_stability = false,
    amrex::Real L_obukhov = amrex::Real(1.0e10),
    bool enable_adaptive = false,
    amrex::Real adaptive_threshold = amrex::Real(30.0))
{
    // Ensure height is positive
    if (z_agl <= amrex::Real(0.0)) {
        u = amrex::Real(0.0);
        v = amrex::Real(0.0);
        w = amrex::Real(0.0);
        return;
    }
    
    // Check grid resolution suitability (if adaptive activation enabled)
    if (enable_adaptive) {
        if (!is_grid_resolution_suitable_for_wall_function(dz, z0, adaptive_threshold)) {
            // Grid not suitable - don't modify velocity (use original field)
            return;
        }
    }
    
    // Decompose velocity into parallel and normal components
    amrex::Real u_par, v_par, w_par, u_norm;
    decompose_velocity(u, v, w, nx, ny, nz, u_par, v_par, w_par, u_norm);
    
    // Compute friction velocity from reference conditions
    amrex::Real ustar;
    if (enable_stability && std::abs(L_obukhov) < amrex::Real(1.0e8)) {
        ustar = compute_friction_velocity_stability(u_ref, z_ref, z0, L_obukhov, kappa);
    } else {
        ustar = compute_friction_velocity(u_ref, z_ref, z0, kappa);
    }
    
    if (ustar < amrex::Real(1.0e-10)) {
        u = amrex::Real(0.0);
        v = amrex::Real(0.0);
        w = amrex::Real(0.0);
        return;
    }
    
    // Apply log-law (with optional stability correction) to parallel component magnitude
    amrex::Real speed_par = std::sqrt(u_par * u_par + v_par * v_par + w_par * w_par);
    amrex::Real speed_wall;
    
    if (enable_stability && std::abs(L_obukhov) < amrex::Real(1.0e8)) {
        // Stability-corrected log-law
        amrex::Real zeta = z_agl / L_obukhov;
        amrex::Real zeta0 = z0 / L_obukhov;
        
        // Limit zeta
        const amrex::Real zeta_max = amrex::Real(2.0);
        const amrex::Real zeta_min = amrex::Real(-2.0);
        zeta = (zeta > zeta_max) ? zeta_max : zeta;
        zeta = (zeta < zeta_min) ? zeta_min : zeta;
        
        // Compute psi_m
        amrex::Real psi_m_z, psi_m_z0;
        if (zeta > amrex::Real(0.0)) {
            psi_m_z = -amrex::Real(5.0) * zeta;
            psi_m_z0 = -amrex::Real(5.0) * zeta0;
        } else if (zeta < amrex::Real(0.0)) {
            const amrex::Real x = std::pow(amrex::Real(1.0) - amrex::Real(16.0) * zeta, amrex::Real(0.25));
            const amrex::Real x2 = x * x;
            psi_m_z = amrex::Real(2.0) * std::log((amrex::Real(1.0) + x) / amrex::Real(2.0))
                    + std::log((amrex::Real(1.0) + x2) / amrex::Real(2.0))
                    - amrex::Real(2.0) * std::atan(x)
                    + amrex::Real(1.5707963267948966);
            
            const amrex::Real x0 = std::pow(amrex::Real(1.0) - amrex::Real(16.0) * zeta0, amrex::Real(0.25));
            const amrex::Real x02 = x0 * x0;
            psi_m_z0 = amrex::Real(2.0) * std::log((amrex::Real(1.0) + x0) / amrex::Real(2.0))
                     + std::log((amrex::Real(1.0) + x02) / amrex::Real(2.0))
                     - amrex::Real(2.0) * std::atan(x0)
                     + amrex::Real(1.5707963267948966);
        } else {
            psi_m_z = amrex::Real(0.0);
            psi_m_z0 = amrex::Real(0.0);
        }
        
        amrex::Real log_term = std::log((z_agl + z0) / z0) - psi_m_z + psi_m_z0;
        if (log_term < amrex::Real(1.0e-10)) {
            u = amrex::Real(0.0);
            v = amrex::Real(0.0);
            w = amrex::Real(0.0);
            return;
        }
        speed_wall = (ustar / kappa) * log_term;
    } else {
        // Neutral log-law
        amrex::Real log_term = std::log((z_agl + z0) / z0);
        if (log_term < amrex::Real(1.0e-10)) {
            u = amrex::Real(0.0);
            v = amrex::Real(0.0);
            w = amrex::Real(0.0);
            return;
        }
        speed_wall = (ustar / kappa) * log_term;
    }
    
    // Scale parallel component to match wall function
    if (speed_par > amrex::Real(1.0e-10)) {
        amrex::Real scale = speed_wall / speed_par;
        u_par *= scale;
        v_par *= scale;
        w_par *= scale;
    } else {
        u_par = amrex::Real(0.0);
        v_par = amrex::Real(0.0);
        w_par = amrex::Real(0.0);
    }
    
    // Reconstruct velocity: parallel component only (normal = 0)
    u = u_par;
    v = v_par;
    w = w_par;
}

// ============================================================================
// Compute distance from cell center to nearest building face
//
// For an axis-aligned building box [xmin, xmax] × [ymin, ymax] × [zmin, zmax],
// compute the minimum distance from point (x, y, z) to any face.
//
// Returns:
//   distance      - Signed distance (positive outside, negative inside) [m]
//   nx, ny, nz    - Normal vector of nearest face (pointing outward)
//   is_near       - True if point is near a building face
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool compute_building_distance_and_normal(
    amrex::Real x,
    amrex::Real y,
    amrex::Real z,
    amrex::Real xmin,
    amrex::Real xmax,
    amrex::Real ymin,
    amrex::Real ymax,
    amrex::Real zmin,
    amrex::Real zmax,
    amrex::Real& distance,
    amrex::Real& nx,
    amrex::Real& ny,
    amrex::Real& nz,
    amrex::Real max_distance = amrex::Real(3.0))
{
    // Check if point is inside building
    bool inside = (x >= xmin && x <= xmax &&
                   y >= ymin && y <= ymax &&
                   z >= zmin && z <= zmax);
    
    // Compute distance to each face
    amrex::Real dx_min = x - xmin;
    amrex::Real dx_max = xmax - x;
    amrex::Real dy_min = y - ymin;
    amrex::Real dy_max = ymax - y;
    amrex::Real dz_min = z - zmin;
    amrex::Real dz_max = zmax - z;
    
    // Find minimum distance and corresponding normal
    amrex::Real min_dist = dx_min;
    nx = -amrex::Real(1.0);
    ny = amrex::Real(0.0);
    nz = amrex::Real(0.0);
    
    if (dx_max < min_dist) {
        min_dist = dx_max;
        nx = amrex::Real(1.0);
        ny = amrex::Real(0.0);
        nz = amrex::Real(0.0);
    }
    if (dy_min < min_dist) {
        min_dist = dy_min;
        nx = amrex::Real(0.0);
        ny = -amrex::Real(1.0);
        nz = amrex::Real(0.0);
    }
    if (dy_max < min_dist) {
        min_dist = dy_max;
        nx = amrex::Real(0.0);
        ny = amrex::Real(1.0);
        nz = amrex::Real(0.0);
    }
    if (dz_min < min_dist) {
        min_dist = dz_min;
        nx = amrex::Real(0.0);
        ny = amrex::Real(0.0);
        nz = -amrex::Real(1.0);
    }
    if (dz_max < min_dist) {
        min_dist = dz_max;
        nx = amrex::Real(0.0);
        ny = amrex::Real(0.0);
        nz = amrex::Real(1.0);
    }
    
    distance = inside ? -min_dist : min_dist;
    
    // Return true if within max_distance of a wall
    return (inside || min_dist <= max_distance);
}

#endif // WALL_FUNCTIONS_H
