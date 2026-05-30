// ============================================================================
// wake_models.H
// Building wake parameterization models for urban wind fields
//
// This header provides functions for computing building wake effects using
// empirical models from the literature.
//
// Models implemented:
// 1. Röckle (1990) - Cavity and far-wake zones for rectangular buildings
//
// References:
// - Röckle, R. (1990). Bestimmung der Strömungsverhältnisse im Bereich
//   komplexer Bebauungsstrukturen. Dissertation, Vom Fachbereich Mechanik,
//   der Technischen Hochschule Darmstadt.
// - Kaplan, H., & Dinar, N. (1996). A Lagrangian dispersion model for
//   calculating concentration distribution within a built-up domain.
//   Atmospheric Environment, 30(24), 4197-4207.
// ============================================================================

#ifndef WAKE_MODELS_H
#define WAKE_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <algorithm>
#include <cmath>

namespace {
    using namespace amrex;
}

// ============================================================================
// Wake model parameters structure
// ============================================================================
struct WakeParams {
    bool enabled = false;                    // Enable wake model
    amrex::Real c1 = 0.9;                    // Cavity length coefficient
    amrex::Real c2 = 0.3;                    // Wake deficit coefficient
    amrex::Real separation_length = 3.0;     // Wake separation length factor (× building height)
};

// ============================================================================
// Building structure
// ============================================================================
struct Building {
    amrex::Real xmin, xmax;  // Building bounds [m]
    amrex::Real ymin, ymax;
    amrex::Real zmin, zmax;
    amrex::Real height;      // Effective height H [m]
    amrex::Real width;       // Effective width W [m]
    amrex::Real length;      // Effective length L [m]
};

// ============================================================================
// Röckle (1990) building wake model
//
// Computes velocity deficit behind a rectangular building due to wake effects.
// The model divides the wake into two zones:
// 1. Cavity zone (recirculation zone immediately behind building)
// 2. Far-wake zone (displacement zone downwind of cavity)
//
// Cavity zone:
//   Length: Lr = c1 * H  (where H = building height, c1 ≈ 0.9)
//   Height: Hr = 0.67 * H
//   Width:  Wr = W
//   Velocity deficit: u = -c2 * U_H (recirculation, c2 ≈ 0.3)
//
// Far-wake zone (displacement zone):
//   Starts at: x = xb + Lr
//   Length: extends to Lf = separation_length * H (typically 3-5 H)
//   Velocity deficit decreases linearly from cavity edge
//   Lateral spread: wake width increases linearly with distance
//
// Inputs:
//   x, y, z        - Point coordinates [m]
//   building       - Building structure with geometry
//   U_ref          - Reference velocity magnitude at building height [m/s]
//   wind_dir_x     - Wind direction x-component (unit vector)
//   wind_dir_y     - Wind direction y-component (unit vector)
//   params         - Wake model parameters
//
// Outputs:
//   du, dv, dw     - Velocity deficit components [m/s]
//                    (to be subtracted from initial velocity field)
//
// Returns:
//   true if point is inside wake zone, false otherwise
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool rockle_wake_deficit(
    amrex::Real x, amrex::Real y, amrex::Real z,
    const Building& bldg,
    amrex::Real U_ref,
    amrex::Real wind_dir_x,
    amrex::Real wind_dir_y,
    const WakeParams& params,
    amrex::Real& du, amrex::Real& dv, amrex::Real& dw)
{
    // Initialize outputs
    du = amrex::Real(0.0);
    dv = amrex::Real(0.0);
    dw = amrex::Real(0.0);
    
    if (!params.enabled) {
        return false;
    }
    
    // Building center and dimensions
    const amrex::Real bldg_center_x = (bldg.xmin + bldg.xmax) / amrex::Real(2.0);
    const amrex::Real bldg_center_y = (bldg.ymin + bldg.ymax) / amrex::Real(2.0);
    const amrex::Real H = bldg.height;
    const amrex::Real W = bldg.width;
    
    // Normalize wind direction
    const amrex::Real wind_mag = std::sqrt(wind_dir_x * wind_dir_x + wind_dir_y * wind_dir_y);
    if (wind_mag < amrex::Real(1.0e-10)) {
        return false;  // No wind, no wake
    }
    const amrex::Real wd_x = wind_dir_x / wind_mag;
    const amrex::Real wd_y = wind_dir_y / wind_mag;
    
    // Transform coordinates to building-relative coordinate system
    // Origin at building center, x-axis aligned with wind direction
    const amrex::Real dx = x - bldg_center_x;
    const amrex::Real dy = y - bldg_center_y;
    
    // Rotate to wind-aligned coordinates
    // x_wind: downwind distance from building center
    // y_wind: crosswind distance from building center
    const amrex::Real x_wind =  dx * wd_x + dy * wd_y;
    const amrex::Real y_wind = -dx * wd_y + dy * wd_x;
    
    // Building extends from -L/2 to L/2 in downwind direction
    // Determine building length in wind direction (simplified: use building width)
    const amrex::Real half_length_wind = bldg.length / amrex::Real(2.0);
    
    // Wake starts at downwind face of building
    const amrex::Real wake_start = half_length_wind;
    
    // Check if point is downwind of building
    if (x_wind < wake_start) {
        return false;  // Upstream or inside building
    }
    
    // Distance downwind from building back face
    const amrex::Real x_wake = x_wind - wake_start;
    
    // Röckle cavity parameters
    const amrex::Real Lr = params.c1 * H;  // Cavity length
    const amrex::Real Hr = amrex::Real(0.67) * H;  // Cavity height
    const amrex::Real Wr = W;               // Cavity width
    
    // Height above ground (relative to building base)
    const amrex::Real z_local = z - bldg.zmin;
    
    // ========================================================================
    // 1. Cavity zone (recirculation zone)
    // ========================================================================
    if (x_wake <= Lr) {
        // Check lateral bounds (crosswind)
        if (std::abs(y_wind) > Wr / amrex::Real(2.0)) {
            return false;  // Outside cavity width
        }
        
        // Check vertical bounds
        if (z_local < amrex::Real(0.0) || z_local > Hr) {
            return false;  // Below ground or above cavity height
        }
        
        // Inside cavity zone: recirculation with negative velocity
        const amrex::Real deficit_magnitude = params.c2 * U_ref;
        du = deficit_magnitude * wd_x;
        dv = deficit_magnitude * wd_y;
        dw = amrex::Real(0.0);
        
        return true;
    }
    
    // ========================================================================
    // 2. Far-wake zone (displacement zone)
    // ========================================================================
    const amrex::Real Lf = params.separation_length * H;  // Wake extent
    
    if (x_wake <= Lf) {
        // Linear decay of deficit with downwind distance
        const amrex::Real decay_factor = amrex::Real(1.0) - (x_wake - Lr) / (Lf - Lr);
        
        // Linear lateral spreading of wake
        // Wake width increases from Wr to approximately 2*Wr at end of far-wake
        const amrex::Real spread_factor = amrex::Real(1.0) + (x_wake - Lr) / (Lf - Lr);
        const amrex::Real Wf = Wr * spread_factor;
        
        // Check lateral bounds
        if (std::abs(y_wind) > Wf / amrex::Real(2.0)) {
            return false;  // Outside wake width
        }
        
        // Wake height also increases with distance (simplified: use cavity height)
        const amrex::Real Hf = Hr * spread_factor;
        
        // Check vertical bounds
        if (z_local < amrex::Real(0.0) || z_local > Hf) {
            return false;  // Below ground or above wake height
        }
        
        // Inside far-wake zone: reduced velocity deficit
        // Deficit at cavity edge is c2 * U_ref, decays to zero at Lf
        const amrex::Real deficit_magnitude = params.c2 * U_ref * decay_factor;
        du = deficit_magnitude * wd_x;
        dv = deficit_magnitude * wd_y;
        dw = amrex::Real(0.0);
        
        return true;
    }
    
    // Point is beyond wake influence
    return false;
}

// ============================================================================
// Apply wake model to velocity field for a single building
//
// This function computes the wake deficit at a given point and returns
// the corrected velocity components.
//
// Inputs:
//   x, y, z        - Point coordinates [m]
//   u0, v0, w0     - Initial velocity components [m/s]
//   building       - Building structure
//   params         - Wake model parameters
//
// Returns:
//   Corrected velocity components (u, v, w) with wake deficit applied
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_single_building_wake(
    amrex::Real x, amrex::Real y, amrex::Real z,
    amrex::Real& u, amrex::Real& v, amrex::Real& w,
    const Building& bldg,
    const WakeParams& params)
{
    // Compute reference velocity magnitude at building height
    // (simplified: use horizontal velocity magnitude at this point)
    const amrex::Real U_ref = std::sqrt(u * u + v * v);
    
    if (U_ref < amrex::Real(1.0e-10)) {
        return;  // No flow, no wake
    }
    
    // Wind direction from velocity components
    const amrex::Real wind_dir_x = u / U_ref;
    const amrex::Real wind_dir_y = v / U_ref;
    
    // Compute wake deficit
    amrex::Real du, dv, dw;
    bool in_wake = rockle_wake_deficit(
        x, y, z, bldg, U_ref, wind_dir_x, wind_dir_y, params, du, dv, dw);
    
    if (in_wake) {
        // Apply deficit (reduce velocity in wake region)
        u -= du;
        v -= dv;
        w -= dw;
    }
}

// ============================================================================
// Compute building dimensions from bounds
//
// Helper function to compute effective building dimensions for wake model.
// For Röckle model, we need height, width (perpendicular to wind), and
// length (parallel to wind).
//
// Note: This simplified version uses the x and y extents directly. For
// accurate wake modeling with arbitrary wind directions, the building
// dimensions should be rotated to align with the wind direction.
// Currently assumes wind-aligned coordinate system.
//
// Dimensions:
//   height = z-extent (vertical)
//   width  = y-extent (crosswind for westerly/easterly winds)
//   length = x-extent (streamwise for westerly/easterly winds)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
Building compute_building_dimensions(
    amrex::Real xmin, amrex::Real xmax,
    amrex::Real ymin, amrex::Real ymax,
    amrex::Real zmin, amrex::Real zmax)
{
    Building bldg;
    bldg.xmin = xmin;
    bldg.xmax = xmax;
    bldg.ymin = ymin;
    bldg.ymax = ymax;
    bldg.zmin = zmin;
    bldg.zmax = zmax;
    
    // Compute dimensions
    bldg.height = zmax - zmin;
    bldg.width  = ymax - ymin;  // y-extent (crosswind for x-aligned wind)
    bldg.length = xmax - xmin;  // x-extent (streamwise for x-aligned wind)
    
    return bldg;
}

#endif // WAKE_MODELS_H
