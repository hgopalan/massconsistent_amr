// ============================================================================
// datacenter_heat_source.H
// Data center heat source modeling for atmospheric effect studies
//
// This module provides utilities for representing a data center facility
// as a distributed heat source and computing its atmospheric signature.
//
// Physical basis:
// - Data center waste heat modeled as distributed thermal source
// - Plume rise computed using Briggs (1975) parameterization
// - Heat dispersion tracked via 3D scalar transport equation
// - Temperature anomaly (ΔT) relative to ambient profiles
//
// References:
// - Briggs, G.A. (1975). Plume rise predictions. In Lectures on air pollution
//   modeling. American Meteorological Society.
// - Skamarock, W.C., et al. (2008). A description of the Advanced Research
//   WRF version 3. NCAR/TN-475+STR.
// ============================================================================

#ifndef DATACENTER_HEAT_SOURCE_H
#define DATACENTER_HEAT_SOURCE_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>

// ============================================================================
// Data center heat source parameters structure
// ============================================================================
struct DataCenterHeatSourceParams {
    // Enable/disable
    bool enabled = false;
    
    // Heat source location [m]
    amrex::Real x_center = 0.0;        // Easting coordinate
    amrex::Real y_center = 0.0;        // Northing coordinate
    amrex::Real z_center = 10.0;       // Height above ground [m]
    
    // Heat release specifications
    amrex::Real heat_release_rate = 0.0;  // Total power [W]
    amrex::Real source_area = 1000.0;     // Facility footprint [m²]
    amrex::Real effective_radius = 0.0;   // Radius of heat source [m], computed from area if 0
    
    // Gaussian distribution parameters
    amrex::Real sigma_x = 50.0;        // Horizontal spread in X [m]
    amrex::Real sigma_y = 50.0;        // Horizontal spread in Y [m]
    amrex::Real sigma_z = 5.0;         // Vertical spread [m]
    
    // Reference/ambient conditions
    amrex::Real reference_temperature = 300.0;  // T_ref for buoyancy [K]
    amrex::Real cp = 1005.0;                     // Specific heat at constant pressure [J/(kg·K)]
    amrex::Real rho_ref = 1.225;                 // Reference air density [kg/m³]
};

// ============================================================================
// Physical constants for heat source modeling
// ============================================================================
namespace DataCenterConstants {
    // Gravitational acceleration [m/s²]
    constexpr amrex::Real g = amrex::Real(9.81);
    
    // Von Kármán constant
    constexpr amrex::Real von_karman = amrex::Real(0.41);
}

// ============================================================================
// Compute heat source Gaussian distribution
//
// Returns normalized Gaussian distribution centered at (x_c, y_c, z_c).
// The distribution is normalized such that the volume integral equals 1.0,
// allowing the user to scale by the desired temperature increase.
//
// Inputs:
//   x, y, z             - Query coordinates [m]
//   params              - Heat source parameters structure
//
// Returns:
//   Normalized Gaussian value (0 to 1, peak = 1)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real heat_source_gaussian(
    amrex::Real x,
    amrex::Real y,
    amrex::Real z,
    const DataCenterHeatSourceParams& params)
{
    if (!params.enabled) {
        return amrex::Real(0.0);
    }
    
    // Compute horizontal and vertical distances
    amrex::Real dx = x - params.x_center;
    amrex::Real dy = y - params.y_center;
    amrex::Real dz = z - params.z_center;
    
    // Gaussian distribution: exp(-(dx²/sigma_x² + dy²/sigma_y² + dz²/sigma_z²) / 2)
    amrex::Real exponent = -(dx*dx / (2.0 * params.sigma_x * params.sigma_x) +
                             dy*dy / (2.0 * params.sigma_y * params.sigma_y) +
                             dz*dz / (2.0 * params.sigma_z * params.sigma_z));
    
    return std::exp(exponent);
}

// ============================================================================
// Compute effective radius from source area
//
// For a circular facility with given area, compute the equivalent radius.
// This is used to set default sigma_x and sigma_y if not specified.
//
// Inputs:
//   area - Facility footprint [m²]
//
// Returns:
//   Effective radius [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_effective_radius(amrex::Real area)
{
    // A = π * r²  =>  r = sqrt(A / π)
    if (area > amrex::Real(0.0)) {
        return std::sqrt(area / amrex::Real(3.141592653589793));
    }
    return amrex::Real(0.0);
}

// ============================================================================
// Compute buoyant plume rise using Briggs formula
//
// Estimates the rise height of a thermal plume due to buoyancy using the
// Briggs (1975) parameterization. This gives the theoretical rise height
// above the source under stable/neutral conditions.
//
// Inputs:
//   heat_flux     - Buoyant heat flux [W]
//   wind_speed    - Ambient wind speed at source height [m/s]
//   distance      - Distance downwind from source [m]
//   F_parameter   - Buoyancy parameter, typically computed as:
//                   F = g * (dT/T_ref) * v_s² / (u_wind)
//                   where v_s is source velocity scale [m³/s]
//
// Returns:
//   Plume rise height above source [m]
//
// References:
//   Briggs, G.A. (1975). Plume rise predictions. In Lectures on air pollution
//   modeling. American Meteorological Society.
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_briggs_plume_rise(
    amrex::Real heat_flux,
    amrex::Real wind_speed,
    amrex::Real distance,
    amrex::Real temperature_excess,
    amrex::Real temperature_ref)
{
    // Guard against invalid inputs
    if (heat_flux <= amrex::Real(0.0) || wind_speed < amrex::Real(0.1) || 
        distance <= amrex::Real(0.0) || temperature_ref < amrex::Real(100.0)) {
        return amrex::Real(0.0);
    }
    
    // Simplified Briggs formula for unstable/neutral conditions:
    // Δh = 1.6 * (F)^(1/3) * (x)^(2/3) / u
    //
    // where F is the buoyant heat flux parameter.
    // For our case, we use:
    // F = (g / T_ref) * (dT/dt) * distance_scale²
    
    // Compute buoyancy parameter from temperature excess
    // Approximate F from: F ~ (g/T_ref) * dT * velocity_scale * area_scale
    amrex::Real buoyancy = DataCenterConstants::g / temperature_ref;
    amrex::Real F = buoyancy * temperature_excess * amrex::Real(10.0);  // velocity_scale ~ 10 m/s
    
    if (F <= amrex::Real(0.0)) {
        return amrex::Real(0.0);
    }
    
    // Briggs formula with distance downwind
    amrex::Real F_third = std::pow(F, amrex::Real(1.0/3.0));
    amrex::Real x_twothirds = std::pow(std::max(distance, amrex::Real(1.0)), amrex::Real(2.0/3.0));
    
    amrex::Real plume_rise = amrex::Real(1.6) * F_third * x_twothirds / wind_speed;
    
    return plume_rise;
}

// ============================================================================
// Compute temperature excess from heat source
//
// Estimates the temperature increase above ambient caused by the heat source.
// Uses simple heat balance: dT = Q / (rho * cp * volume_scale)
//
// Inputs:
//   heat_release_rate - Total heat output [W]
//   rho_air          - Air density [kg/m³]
//   cp               - Specific heat capacity [J/(kg·K)]
//   volume_scale     - Characteristic volume scale [m³]
//
// Returns:
//   Temperature excess [K]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_temperature_excess(
    amrex::Real heat_release_rate,
    amrex::Real rho_air,
    amrex::Real cp,
    amrex::Real volume_scale)
{
    // Guard against invalid inputs
    if (heat_release_rate <= amrex::Real(0.0) || rho_air <= amrex::Real(0.0) || 
        cp <= amrex::Real(0.0) || volume_scale <= amrex::Real(0.0)) {
        return amrex::Real(0.0);
    }
    
    // Simple energy balance: Q = rho * cp * dT * V_eff
    // where V_eff is the effective volume scale over which heat mixes
    amrex::Real dT = heat_release_rate / (rho_air * cp * volume_scale);
    
    return dT;
}

// ============================================================================
// Compute heat source strength for temperature equation source term
//
// Returns the strength of the heat source term to be added to the temperature
// evolution equation. This is the volume-distributed heat release rate that
// drives the temperature field.
//
// Inputs:
//   Q_total     - Total heat release rate [W]
//   Gaussian    - Normalized Gaussian distribution value (0-1)
//   volume_cell - Cell volume [m³]
//
// Returns:
//   Source strength [K/s] for temperature equation
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real heat_source_strength(
    amrex::Real Q_total,
    amrex::Real gaussian_weight,
    amrex::Real volume_cell,
    amrex::Real rho_air,
    amrex::Real cp)
{
    // Guard against invalid inputs
    if (Q_total <= amrex::Real(0.0) || volume_cell <= amrex::Real(0.0) || 
        rho_air <= amrex::Real(0.0) || cp <= amrex::Real(0.0)) {
        return amrex::Real(0.0);
    }
    
    // Distributed source: dT/dt = (Q * gaussian_weight) / (rho * cp * V_cell)
    return (Q_total * gaussian_weight) / (rho_air * cp * volume_cell);
}

#endif // DATACENTER_HEAT_SOURCE_H
