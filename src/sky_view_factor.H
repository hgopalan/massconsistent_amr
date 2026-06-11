// ==========================================================================
// sky_view_factor.H
// Sky View Factor (SVF) and Solar Shading from Combined Terrain+Buildings
//
// Unified approach: terrain and buildings are both represented as height
// features in the elevation field. This module computes:
// 1. Sky View Factor (SVF): fraction of sky visible from each surface
// 2. Solar shading: direct radiation blocking by terrain/building features
//
// Physical Basis:
// - SVF depends on local slope and terrain/building geometry
// - Simple approximation: SVF = (1 + cos(slope))/2
// - Advanced: horizon-based SVF using surrounding elevation
// - Solar shading: ray-casting from solar position through elevation field
//
// Key insight: Because buildings and terrain are both represented as
// elevation features in the combined height field, a single SVF and shading
// algorithm naturally handles both terrain-terrain, terrain-building, and
// building-building interactions.
//
// References:
// - Oke, T.R. (1988). Street design and urban canopy layer climate.
// - Watson, I.D., Johnson, G.T. (1987). Graphical estimation of sky view factors...
// - Grimmond, C.S.B., Oke, T.R. (1999). Aerodynamic properties of urban areas...
// ==========================================================================

#ifndef SKY_VIEW_FACTOR_H
#define SKY_VIEW_FACTOR_H

#include <AMReX.H>
#include <AMReX_REAL.H>
#include <AMReX_Box.H>
#include <AMReX_FArrayBox.H>
#include <AMReX_MultiFab.H>
#include <cmath>
#include "solver_math_constants.H"

namespace sky_view_factor {

// =========================================================================
// Simple SVF: Based on local slope angle
//
// Physical basis: A steeper slope "sees" less sky.
// - Horizontal surface (slope=0°): SVF = 1.0 (sees entire hemisphere)
// - Vertical cliff (slope=90°): SVF = 0.5 (sees half hemisphere)
// - Overhang (slope=180°): SVF = 0.0 (sees no sky)
//
// Formula: SVF_slope = (1 + cos(slope_angle)) / 2
//
// Parameters:
//   dh_dx, dh_dy: terrain/building elevation gradients
//   dx, dy: grid spacing [m]
//
// Returns: SVF in range [0, 1]
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_svf_from_slope(
    amrex::Real dh_dx,
    amrex::Real dh_dy) noexcept
{
    // Compute slope angle from gradients
    // slope = atan(|∇h|)
    amrex::Real grad_mag = std::sqrt(dh_dx * dh_dx + dh_dy * dh_dy);
    amrex::Real slope_angle = std::atan(grad_mag);  // radians, 0 to π/4
    
    // SVF = (1 + cos(slope)) / 2
    // cos(slope) ranges from 1 (horizontal) to 0 (vertical cliff)
    amrex::Real svf = (amrex::Real(1.0) + std::cos(slope_angle)) / amrex::Real(2.0);
    
    // Clamp to valid range
    svf = std::max(amrex::Real(0.0), std::min(svf, amrex::Real(1.0)));
    
    return svf;
}

// =========================================================================
// Compute terrain/building elevation gradient at cell (i,j)
// using centered finite differences
// =========================================================================
template<typename Array4>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void compute_elevation_gradient(
    int i, int j,
    const Array4& elevation_arr,  // Combined terrain + building height
    int lo_i, int hi_i, int lo_j, int hi_j,  // Box bounds
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real& dh_dx,  // Output
    amrex::Real& dh_dy) noexcept  // Output
{
    const amrex::Real inv_2dx = amrex::Real(1.0) / (amrex::Real(2.0) * dx);
    const amrex::Real inv_2dy = amrex::Real(1.0) / (amrex::Real(2.0) * dy);

    // Centered differences
    // Check bounds to avoid accessing invalid cells
    if (i > lo_i && i < hi_i) {
        dh_dx = (elevation_arr(i+1, j, 0) - elevation_arr(i-1, j, 0)) * inv_2dx;
    } else {
        dh_dx = amrex::Real(0.0);
    }
    
    if (j > lo_j && j < hi_j) {
        dh_dy = (elevation_arr(i, j+1, 0) - elevation_arr(i, j-1, 0)) * inv_2dy;
    } else {
        dh_dy = amrex::Real(0.0);
    }
}

// =========================================================================
// Check if a point is shaded by terrain/building at given solar position
//
// Solar shading uses ray-casting: for each surface point, check if a ray
// cast upward toward the sun intersects terrain/building features between
// the point and the sun.
//
// Parameters:
//   z_surface: surface elevation at query point [m]
//   sun_altitude: solar altitude angle [radians, 0 to π/2]
//   sun_azimuth: solar azimuth direction [radians, 0 to 2π]
//   max_horizon_distance: maximum distance to check for horizon blocking [m]
//   dx, dy: grid spacing [m]
//
// Returns: shading factor in [0,1]
//   0.0 = fully shaded (no direct sun)
//   1.0 = fully sunlit (direct sun)
// =========================================================================
template<typename Array4>
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real check_solar_shading(
    int i, int j,
    const Array4& elevation_arr,
    int lo_i, int hi_i, int lo_j, int hi_j,  // Box bounds
    amrex::Real sun_altitude,      // Solar altitude [rad]
    amrex::Real sun_azimuth,       // Solar azimuth [rad]
    amrex::Real max_horizon_distance,
    amrex::Real dx,
    amrex::Real dy) noexcept
{
    // Sun below horizon → fully shaded
    if (sun_altitude < amrex::Real(0.0)) {
        return amrex::Real(0.0);
    }
    
    // Get surface elevation at query point
    amrex::Real z_surface = elevation_arr(i, j, 0);
    
    // Ray-cast in solar direction to check for blocking
    amrex::Real tan_alt = std::tan(sun_altitude);  // Rise/run for sun rays
    
    // Check several points along ray toward sun
    // Number of ray steps (typically 5-10 is sufficient)
    int n_steps = 10;
    amrex::Real shading = amrex::Real(0.0);  // Accumulate shading from blocking features
    
    for (int step = 1; step < n_steps; step++) {
        amrex::Real frac = amrex::Real(step) / amrex::Real(n_steps);
        amrex::Real dist = frac * max_horizon_distance;
        
        // Position along ray in horizontal direction
        amrex::Real x_ray = dist * std::cos(sun_azimuth);
        amrex::Real y_ray = dist * std::sin(sun_azimuth);
        
        // Convert to grid indices (approximate)
        int i_ray = i + static_cast<int>(x_ray / dx);
        int j_ray = j + static_cast<int>(y_ray / dy);
        
        // Check bounds
        if (i_ray > lo_i && i_ray < hi_i &&
            j_ray > lo_j && j_ray < hi_j) {
            
            // Elevation at this ray position
            amrex::Real z_ray = elevation_arr(i_ray, j_ray, 0);
            
            // Height of sun ray at this horizontal distance
            amrex::Real z_sun_ray = z_surface + dist * tan_alt;
            
            // If terrain/building is higher than sun ray, it blocks
            if (z_ray > z_sun_ray) {
                shading += amrex::Real(1.0) / amrex::Real(n_steps);
            }
        }
    }
    
    // Return sunlit fraction (1 - shading)
    return std::max(amrex::Real(0.0), amrex::Real(1.0) - shading);
}

// =========================================================================
// Compute simple solar position (altitude and azimuth)
//
// Simplified solar geometry without external libraries.
// Returns approximate solar position for a given time of day.
//
// Parameters:
//   hour_of_day: time [0-24 hours]
//   day_of_year: day [1-365]
//   latitude: latitude [degrees, -90 to +90]
//
// Returns:
//   altitude_rad: solar altitude angle [radians, π/2 = zenith]
//   azimuth_rad: solar azimuth [radians, 0 = north, π/2 = east, π = south, 3π/2 = west]
// =========================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void compute_solar_position(
    amrex::Real hour_of_day,
    amrex::Real day_of_year,
    amrex::Real latitude,
    amrex::Real& altitude_rad,  // Output
    amrex::Real& azimuth_rad) noexcept  // Output
{
    // Convert to radians
    const amrex::Real lat_rad = latitude * MathConstants::pi / amrex::Real(180.0);
    
    // Fractional day of year (0-1)
    amrex::Real day_frac = day_of_year / amrex::Real(365.25);
    
    // Earth's axial tilt (23.44°)
    amrex::Real declination = amrex::Real(23.44) * MathConstants::pi / amrex::Real(180.0)
                             * std::sin(amrex::Real(2.0) * MathConstants::pi * day_frac);
    
    // Hour angle (15° per hour = π/12 rad/hour)
    // Assume solar noon at 12:00 local time
    amrex::Real hour_angle = (hour_of_day - amrex::Real(12.0)) * MathConstants::pi / amrex::Real(12.0);
    
    // Solar altitude angle
    // sin(h) = sin(lat) * sin(dec) + cos(lat) * cos(dec) * cos(h_angle)
    amrex::Real sin_alt = std::sin(lat_rad) * std::sin(declination)
                        + std::cos(lat_rad) * std::cos(declination) * std::cos(hour_angle);
    
    altitude_rad = std::asin(sin_alt);
    
    // Solar azimuth angle (simplified)
    // tan(A) = sin(h_angle) / (cos(lat) * tan(dec) - sin(lat) * cos(h_angle))
    amrex::Real numerator = std::sin(hour_angle);
    amrex::Real denominator = std::cos(lat_rad) * std::tan(declination) 
                            - std::sin(lat_rad) * std::cos(hour_angle);
    
    azimuth_rad = std::atan2(numerator, denominator);
    
    // Normalize azimuth to [0, 2π]
    // Convention: 0 = North, π/2 = East, π = South, 3π/2 = West
    if (azimuth_rad < amrex::Real(0.0)) {
        azimuth_rad += amrex::Real(2.0) * MathConstants::pi;
    }
}

// =========================================================================
// Main routine: Compute SVF for entire 2D domain
//
// Computes sky view factor at each (i,j) position in the domain,
// based on combined terrain+building elevation field.
//
// Usage:
//   multifab svf_field;
//   compute_sky_view_factor(combined_elevation_fab, dx, dy, svf_field);
// =========================================================================
void compute_sky_view_factor(
    const amrex::MultiFab& elevation_mf,
    amrex::Real dx,
    amrex::Real dy,
    amrex::MultiFab& svf_mf) noexcept
{
    // Loop over boxes in MultiFab
    for (amrex::MFIter mfi(elevation_mf); mfi.isValid(); ++mfi) {
        const amrex::Box& bx = mfi.validbox();
        const auto& elev_fab = elevation_mf[mfi];
        auto& svf_fab = svf_mf[mfi];
        
        // Get Array4 accessors and box bounds
        auto const& elev_arr = elev_fab.const_array();
        auto svf_arr = svf_fab.array();
        int lo_i = bx.loVect()[0];
        int hi_i = bx.hiVect()[0];
        int lo_j = bx.loVect()[1];
        int hi_j = bx.hiVect()[1];
        
        // Compute SVF for each cell using direct 3-argument form
        amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
            // Compute elevation gradient
            amrex::Real dh_dx, dh_dy;
            compute_elevation_gradient(i, j, elev_arr, lo_i, hi_i, lo_j, hi_j, dx, dy, dh_dx, dh_dy);
            
            // Compute SVF from slope
            amrex::Real svf = compute_svf_from_slope(dh_dx, dh_dy);
            
            svf_arr(i, j, k) = svf;
        });
    }
}

// =========================================================================
// Main routine: Compute solar shading for entire 2D domain
//
// Computes shading factor at each (i,j) position based on solar position.
//
// Usage:
//   multifab shading_field;
//   compute_solar_shading(combined_elevation_fab, dx, dy,
//                         hour_of_day, day_of_year, latitude,
//                         shading_field);
// =========================================================================
void compute_solar_shading(
    const amrex::MultiFab& elevation_mf,
    amrex::Real dx,
    amrex::Real dy,
    amrex::Real hour_of_day,
    amrex::Real day_of_year,
    amrex::Real latitude,
    amrex::MultiFab& shading_mf) noexcept
{
    // Compute solar position
    amrex::Real sun_altitude, sun_azimuth;
    compute_solar_position(hour_of_day, day_of_year, latitude, sun_altitude, sun_azimuth);
    
    // Maximum distance for horizon checking (5x max grid dimension)
    amrex::Real max_dist = amrex::Real(5.0) * std::max(dx, dy) * amrex::Real(100.0);
    
    // Loop over boxes
    for (amrex::MFIter mfi(elevation_mf); mfi.isValid(); ++mfi) {
        const amrex::Box& bx = mfi.validbox();
        const auto& elev_fab = elevation_mf[mfi];
        auto& shading_fab = shading_mf[mfi];
        
        // Get Array4 accessors and box bounds
        auto const& elev_arr = elev_fab.const_array();
        auto shading_arr = shading_fab.array();
        int lo_i = bx.loVect()[0];
        int hi_i = bx.hiVect()[0];
        int lo_j = bx.loVect()[1];
        int hi_j = bx.hiVect()[1];
        
        // Compute shading for each cell using direct 3-argument form
        amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
            // Compute solar shading
            amrex::Real shade_factor = check_solar_shading(
                i, j, elev_arr, lo_i, hi_i, lo_j, hi_j, sun_altitude, sun_azimuth, max_dist, dx, dy
            );
            
            shading_arr(i, j, k) = shade_factor;
        });
    }
}

}  // namespace sky_view_factor

#endif // SKY_VIEW_FACTOR_H
