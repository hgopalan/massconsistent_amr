#ifndef ROUGHNESS_BLOCKING_METHOD_H
#define ROUGHNESS_BLOCKING_METHOD_H

#include <AMReX_REAL.H>
#include <cmath>

namespace {
    // Roughness modification due to building blockage effects
    // Physical basis: Buildings increase surface roughness by acting as additional
    // obstacles to flow (Grimmond & Oke 1999, Roth 2000)
    
    struct RoughnessBlockingParams {
        // Default parameters
        amrex::Real roughness_increase_factor = 0.04;  // Additional z0 as fraction of building height
        amrex::Real building_height_threshold = 2.0;   // Minimum building height to affect z0 [m]
        bool enable_roughness_blocking = false;
    };
}

// Compute additional roughness from building presence
// Used in wind profile initialization to account for urban canopy effects
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_roughness_from_buildings(
    amrex::Real z_agl,              // Height above ground level [m]
    amrex::Real z_building,         // Local building height [m]
    amrex::Real factor = 0.04)      // Roughness factor (z0_increase = factor * h_building)
{
    // Roughness increase from building when z_agl < z_building
    // Δz0 = factor × z_building (typically factor ≈ 0.04-0.1)
    // This accounts for enhanced surface roughness in urban areas
    
    if (z_agl < z_building) {
        return factor * z_building;
    }
    return 0.0;
}

// Modify roughness length based on building context
// This is applied during wind initialization to modulate z0
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real apply_building_roughness_correction(
    amrex::Real z0_base,        // Base roughness length from land-use [m]
    amrex::Real z_agl,          // Height above ground level [m]
    amrex::Real z_building,     // Local building height [m]
    amrex::Real factor = 0.04)
{
    // Total roughness = base roughness + building-induced roughness
    // Applied only if z_agl < building height
    
    amrex::Real z0_building_contribution = compute_roughness_from_buildings(z_agl, z_building, factor);
    return z0_base + z0_building_contribution;
}

// Compute cumulative roughness in mixed urban-rural domain
// For heterogeneous terrain with scattered buildings
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_effective_roughness_urban_canopy(
    amrex::Real z0_ground,          // Open terrain roughness [m]
    amrex::Real z0_urban_base,      // Urban baseline roughness [m]
    amrex::Real building_density,   // Fraction of domain occupied by buildings [0-1]
    amrex::Real z_agl,              // Height above ground level [m]
    amrex::Real H_avg)              // Average building height [m]
{
    // Morphometric approach: z0_eff = z0_ground × (1 - building_density)
    //                              + z0_urban × building_density
    // where z0_urban incorporates H_avg
    
    amrex::Real z0_urban_local = z0_urban_base;
    if (z_agl < H_avg) {
        // Within building layer: increase z0
        z0_urban_local = std::max(z0_urban_base, 0.04 * H_avg);
    }
    
    return z0_ground * (1.0 - building_density) + z0_urban_local * building_density;
}

#endif
