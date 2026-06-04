// ============================================================================
// porosity_models.H
// Building porosity model for partial flow through structures
//
// This header provides functions for modeling porous buildings (trees, fences,
// permeable structures) where air can flow partially through the obstruction.
//
// Model implemented:
// - Fractional porosity with drag coefficient
// - Drag force: F = -0.5 * rho * Cd * A * |u| * u
// - Porosity parameter beta: fraction of volume open to flow (0 = solid, 1 = open)
//
// References:
// - Santiago, J.L., et al. (2007). CFD simulation of airflow over a regular
//   array of cubes. Part I: Three-dimensional simulation of the flow and
//   validation with wind-tunnel measurements. Boundary-Layer Meteorology,
//   122(3), 609-634.
// - Kanda, M., et al. (2013). A new aerodynamic parametrization for real
//   urban surfaces. Boundary-Layer Meteorology, 148(2), 357-377.
// ============================================================================

#ifndef POROSITY_MODELS_H
#define POROSITY_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>

// ============================================================================
// Porosity parameters structure
// ============================================================================
struct PorosityParams {
    bool enabled = false;                       // Enable porosity model
    amrex::Real default_porosity = 0.0;         // Default porosity (0 = solid, 1 = open)
    amrex::Real drag_coefficient = 0.2;         // Drag coefficient for porous flow
    amrex::Real air_density = 1.225;            // Air density [kg/m³]
};

// ============================================================================
// Porous building structure
// ============================================================================
struct PorousBuilding {
    amrex::Real xmin, xmax;  // Building bounds [m]
    amrex::Real ymin, ymax;
    amrex::Real zmin, zmax;
    amrex::Real porosity;    // Porosity parameter (0 = solid, 1 = fully open)
    amrex::Real rotation;    // Building orientation angle (radians)
};

// ============================================================================
// Check if a point is inside a porous building
// Returns porosity value (0 = outside, 0-1 = inside porous structure)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real point_in_porous_building(
    amrex::Real x,
    amrex::Real y,
    amrex::Real z,
    const PorousBuilding& bldg)
{
    // Handle building rotation if needed
    if (std::abs(bldg.rotation) > amrex::Real(1.0e-10)) {
        // Compute building center
        amrex::Real cx = (bldg.xmin + bldg.xmax) / amrex::Real(2.0);
        amrex::Real cy = (bldg.ymin + bldg.ymax) / amrex::Real(2.0);
        
        // Translate to building center coordinates
        amrex::Real dx = x - cx;
        amrex::Real dy = y - cy;
        
        // Rotate point back to building-aligned coordinates
        amrex::Real cos_theta = std::cos(-bldg.rotation);
        amrex::Real sin_theta = std::sin(-bldg.rotation);
        amrex::Real x_rot = dx * cos_theta - dy * sin_theta + cx;
        amrex::Real y_rot = dx * sin_theta + dy * cos_theta + cy;
        
        // Check if rotated point is inside building bounds
        if (x_rot >= bldg.xmin && x_rot <= bldg.xmax &&
            y_rot >= bldg.ymin && y_rot <= bldg.ymax &&
            z >= bldg.zmin && z <= bldg.zmax) {
            return bldg.porosity;
        }
    } else {
        // No rotation - simple box check
        if (x >= bldg.xmin && x <= bldg.xmax &&
            y >= bldg.ymin && y <= bldg.ymax &&
            z >= bldg.zmin && z <= bldg.zmax) {
            return bldg.porosity;
        }
    }
    
    return amrex::Real(0.0);  // Outside building
}

// ============================================================================
// Apply drag force from porous medium
// Modifies velocity components u, v, w based on porosity and drag
//
// Drag formulation:
//   du/dt = -0.5 * (1 - beta) * (Cd / dx) * |u| * u
//
// where:
//   beta = porosity (0 = solid, 1 = open)
//   Cd = drag coefficient
//   dx = cell size (effective frontal area)
//   |u| = velocity magnitude
//
// For steady-state mass-consistent solver, we apply drag as velocity reduction:
//   u_new = u_old * (1 - drag_factor)
//
// where drag_factor depends on (1 - beta), Cd, and flow characteristics
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_porosity_drag(
    amrex::Real& u,
    amrex::Real& v,
    amrex::Real& w,
    amrex::Real porosity,
    amrex::Real drag_coeff,
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real dz)
{
    // If porosity = 1.0 (fully open), no drag
    if (porosity >= amrex::Real(0.999)) {
        return;
    }
    
    // If porosity = 0.0 (solid building), zero velocity
    if (porosity <= amrex::Real(0.001)) {
        u = amrex::Real(0.0);
        v = amrex::Real(0.0);
        w = amrex::Real(0.0);
        return;
    }
    
    // Partial porosity: apply drag reduction
    // Velocity magnitude
    amrex::Real speed = std::sqrt(u*u + v*v + w*w);
    
    if (speed > amrex::Real(1.0e-10)) {
        // Drag factor based on porosity and drag coefficient
        // Higher porosity (more open) → less drag
        // Lower porosity (more blocked) → more drag
        amrex::Real blockage = amrex::Real(1.0) - porosity;
        
        // Effective drag coefficient scaled by blockage
        // Use characteristic length scale (average cell size)
        amrex::Real dx_avg = (dx + dy + dz) / amrex::Real(3.0);
        amrex::Real drag_factor = blockage * drag_coeff / dx_avg;
        
        // Limit drag factor to prevent instability
        drag_factor = (drag_factor > amrex::Real(10.0)) ? amrex::Real(10.0) : drag_factor;
        
        // Apply drag reduction (exponential form for stability)
        // For small drag_factor: reduction ≈ 1 - drag_factor * dt
        // We use characteristic_time = 0.1 s as time scale for steady-state adjustment
        const amrex::Real characteristic_time = amrex::Real(0.1);  // Empirical time scale for drag application
        amrex::Real reduction = std::exp(-drag_factor * characteristic_time);
        
        u *= reduction;
        v *= reduction;
        w *= reduction;
    }
}

// ============================================================================
// Compute combined porosity from multiple buildings
// Uses minimum porosity (maximum blockage) when buildings overlap
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_combined_porosity(
    amrex::Real x,
    amrex::Real y,
    amrex::Real z,
    const PorousBuilding* buildings,
    int n_buildings)
{
    amrex::Real min_porosity = amrex::Real(1.0);  // Start with fully open
    
    for (int i = 0; i < n_buildings; ++i) {
        amrex::Real p = point_in_porous_building(x, y, z, buildings[i]);
        if (p < min_porosity) {
            min_porosity = p;
        }
    }
    
    return min_porosity;
}

// ============================================================================
// Sub-grid Windbreak structure
// ============================================================================
struct Windbreak {
    amrex::Real x1, y1;      // Start point [m]
    amrex::Real x2, y2;      // End point [m]
    amrex::Real height;      // Barrier height [m]
    amrex::Real blockage;    // Blockage factor (1 - porosity, 0 to 1)
    amrex::Real drag_coeff;  // Drag coefficient (typically ~1.0-2.0 for fences/hedgerows)
};

// ============================================================================
// Compute distance from point P to line segment AB
// Inputs:
//   px, py       - Point coordinates (cell center)
//   ax, ay       - Segment start coordinates
//   bx, by       - Segment end coordinates
//   t_clamped    - (Output) Parameter t clamped to [0, 1] for closest point
// Returns:
//   Distance to segment [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real distance_to_segment(
    amrex::Real px, amrex::Real py,
    amrex::Real ax, amrex::Real ay,
    amrex::Real bx, amrex::Real by,
    amrex::Real& t_clamped)
{
    const amrex::Real vx = bx - ax;
    const amrex::Real vy = by - ay;
    
    const amrex::Real wx = px - ax;
    const amrex::Real wy = py - ay;
    
    const amrex::Real v2 = vx*vx + vy*vy;
    if (v2 <= amrex::Real(1.0e-10)) {
        t_clamped = amrex::Real(0.0);
        return std::sqrt(wx*wx + wy*wy);
    }
    
    const amrex::Real t = (wx*vx + vy*vy) / v2;
    t_clamped = std::max(amrex::Real(0.0), std::min(amrex::Real(1.0), t));
    
    const amrex::Real closest_x = ax + t_clamped * vx;
    const amrex::Real closest_y = ay + t_clamped * vy;
    
    const amrex::Real dx = px - closest_x;
    const amrex::Real dy = py - closest_y;
    
    return std::sqrt(dx*dx + dy*dy);
}

// ============================================================================
// Apply directional drag from an unresolved windbreak / linear barrier
// Inputs:
//   u, v         - Velocity components [m/s] (modified in place)
//   blockage     - Blockage factor (1 - porosity, 0 = open, 1 = solid)
//   drag_coeff   - Drag coefficient
//   ax, ay       - Segment start coordinates
//   bx, by       - Segment end coordinates
//   dx, dy       - Grid spacing [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_windbreak_drag(
    amrex::Real& u,
    amrex::Real& v,
    amrex::Real blockage,
    amrex::Real drag_coeff,
    amrex::Real ax, amrex::Real ay,
    amrex::Real bx, amrex::Real by,
    amrex::Real dx, amrex::Real dy)
{
    if (blockage <= amrex::Real(0.001)) {
        return; // No blockage
    }
    
    const amrex::Real vx = bx - ax;
    const amrex::Real vy = by - ay;
    const amrex::Real seg_len = std::sqrt(vx*vx + vy*vy);
    
    if (seg_len <= amrex::Real(1.0e-10)) {
        return;
    }
    
    // Segment direction (unit vector)
    const amrex::Real sx = vx / seg_len;
    const amrex::Real sy = vy / seg_len;
    
    // Unit normal vector (pointing perpendicular to segment)
    const amrex::Real nx = -sy;
    const amrex::Real ny = sx;
    
    // Project wind velocity onto normal and parallel directions
    const amrex::Real Un = u * nx + v * ny;
    const amrex::Real Up = u * sx + v * sy;
    
    if (std::abs(Un) > amrex::Real(1.0e-10)) {
        // Drag factor for the unresolved barrier
        // drag_factor = blockage * Cd * |Un| / dx_avg
        const amrex::Real dx_avg = amrex::Real(0.5) * (dx + dy);
        amrex::Real drag_factor = blockage * drag_coeff * std::abs(Un) / dx_avg;
        
        // Clamp drag factor for stability
        drag_factor = (drag_factor > amrex::Real(10.0)) ? amrex::Real(10.0) : drag_factor;
        
        // Apply directional reduction to normal component only
        const amrex::Real characteristic_time = amrex::Real(0.1);
        const amrex::Real reduction = std::exp(-drag_factor * characteristic_time);
        
        const amrex::Real Un_new = Un * reduction;
        
        // Reconstruct horizontal velocity components
        u = Un_new * nx + Up * sx;
        v = Un_new * ny + Up * sy;
    }
}

#endif // POROSITY_MODELS_H
