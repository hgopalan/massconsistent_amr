// ============================================================================
// ageostrophic_models.H
// Ageostrophic wind balance and geostrophic wind computation
//
// This header provides functions for computing geostrophic wind from
// pressure gradients and applying ageostrophic (non-geostrophic) balance
// to boundary conditions.
//
// Physical basis:
// - Geostrophic wind: Balance between pressure gradient and Coriolis force
// - Ageostrophic component: Boundary layer wind that deviates from geostrophic balance
// - Boundary layer typically 10-20% ageostrophic
// - Important for accurate lateral BC specification
//
// Model: U_geo = -(1/ρf)·∂p/∂y  (eastward geostrophic wind)
//        V_geo = +(1/ρf)·∂p/∂x  (northward geostrophic wind)
//
// where:
//   ρ = air density [kg/m³]
//   f = Coriolis parameter [1/s]
//   ∂p/∂x, ∂p/∂y = pressure gradients [Pa/m]
//
// References:
// - Holton, J.R. (2004). An Introduction to Dynamic Meteorology.
// - Gill, A.E. (1982). Atmosphere-Ocean Dynamics.
// ============================================================================

#ifndef AGEOSTROPHIC_MODELS_H
#define AGEOSTROPHIC_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <algorithm>

// ============================================================================
// Ageostrophic wind parameters structure
// ============================================================================
struct AgeostrophicParams {
    bool enabled = false;                    // Enable ageostrophic wind balance
    amrex::Real latitude = 45.0;             // Latitude [degrees] for Coriolis parameter
    amrex::Real pressure_gradient_x = 0.0;   // Pressure gradient ∂p/∂x [Pa/m]
    amrex::Real pressure_gradient_y = 0.0;   // Pressure gradient ∂p/∂y [Pa/m]
    amrex::Real air_density = 1.225;         // Air density ρ [kg/m³]
    amrex::Real ageostrophic_fraction = 0.1; // Ageostrophic component as fraction of geostrophic (0-1)
};

// Physical constants for ageostrophic wind
namespace AgeostrophicConstants {
    // Earth's rotation rate [1/s]
    constexpr amrex::Real EARTH_ROTATION = amrex::Real(7.292115e-5);
    
    // Earth's radius [m]
    constexpr amrex::Real EARTH_RADIUS = amrex::Real(6.371e6);
    
    // Standard air density at sea level, 15°C [kg/m³]
    constexpr amrex::Real STANDARD_AIR_DENSITY = amrex::Real(1.225);
    
    // Typical Coriolis parameter range [1/s]
    // At equator: f ≈ 0
    // At 45° latitude: f ≈ 1.0e-4
    // At pole: f ≈ 1.46e-4
    constexpr amrex::Real CORIOLIS_AT_EQUATOR = amrex::Real(0.0);
    constexpr amrex::Real CORIOLIS_AT_45DEG = amrex::Real(1.0e-4);
    constexpr amrex::Real CORIOLIS_AT_POLE = amrex::Real(1.46e-4);
}

// ============================================================================
// Compute Coriolis parameter from latitude
//
// f = 2·Ω·sin(φ)
//
// where:
//   Ω = Earth's angular rotation rate ≈ 7.29e-5 rad/s
//   φ = latitude [radians]
//
// Inputs:
//   latitude_deg       - Latitude [degrees, -90 to +90]
//                        Positive = Northern hemisphere, Negative = Southern
//
// Returns:
//   Coriolis parameter f [1/s] (positive in Northern hemisphere)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_coriolis_parameter(amrex::Real latitude_deg)
{
    // Convert to radians
    constexpr amrex::Real deg_to_rad = amrex::Real(3.14159265359) / amrex::Real(180.0);
    const amrex::Real latitude_rad = latitude_deg * deg_to_rad;
    
    // f = 2·Ω·sin(φ)
    const amrex::Real f = amrex::Real(2.0) * AgeostrophicConstants::EARTH_ROTATION 
                         * std::sin(latitude_rad);
    
    return f;
}

// ============================================================================
// Compute geostrophic wind components from pressure gradients
//
// Geostrophic balance (in Northern hemisphere):
//   U_geo = -(1/(ρf))·∂p/∂y  (eastward wind)
//   V_geo = +(1/(ρf))·∂p/∂x  (northward wind)
//
// In Southern hemisphere, signs flip (f < 0 changes sense)
//
// Inputs:
//   pressure_grad_x    - Pressure gradient ∂p/∂x [Pa/m] (positive eastward)
//   pressure_grad_y    - Pressure gradient ∂p/∂y [Pa/m] (positive northward)
//   air_density        - Air density ρ [kg/m³]
//   coriolis_param     - Coriolis parameter f [1/s]
//
// Returns tuple: (U_geo, V_geo) - Geostrophic wind components [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_geostrophic_wind(
    amrex::Real pressure_grad_x,
    amrex::Real pressure_grad_y,
    amrex::Real air_density,
    amrex::Real coriolis_param,
    amrex::Real& U_geo,
    amrex::Real& V_geo)
{
    // Protect against invalid Coriolis parameter
    if (std::abs(coriolis_param) < amrex::Real(1.0e-8)) {
        // At equator or very low latitude: geostrophic wind undefined/very large
        U_geo = amrex::Real(0.0);
        V_geo = amrex::Real(0.0);
        return;
    }
    
    // Protect against invalid density
    if (air_density < amrex::Real(0.1)) {
        air_density = AgeostrophicConstants::STANDARD_AIR_DENSITY;
    }
    
    // Compute geostrophic wind
    // U_geo = -(1/(ρf))·∂p/∂y
    // V_geo = +(1/(ρf))·∂p/∂x
    const amrex::Real factor = amrex::Real(1.0) / (air_density * coriolis_param);
    
    U_geo = -factor * pressure_grad_y;
    V_geo = +factor * pressure_grad_x;
    
    // Limit to reasonable values (typically |U_geo| < 50 m/s)
    const amrex::Real max_geo_wind = amrex::Real(50.0);
    U_geo = std::max(-max_geo_wind, std::min(U_geo, max_geo_wind));
    V_geo = std::max(-max_geo_wind, std::min(V_geo, max_geo_wind));
}

// ============================================================================
// Compute ageostrophic wind components (deviation from geostrophic balance)
//
// Ageostrophic wind is the difference between the actual wind and the
// geostrophic wind. In the boundary layer, the ageostrophic component
// typically 10-20% of the geostrophic wind.
//
// U_ageo = U_actual - U_geo
// V_ageo = V_actual - V_geo
//
// Inputs:
//   u_actual, v_actual - Actual wind components [m/s]
//   u_geo, v_geo       - Geostrophic wind components [m/s]
//
// Returns tuple: (U_ageo, V_ageo) - Ageostrophic wind components [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_ageostrophic_wind(
    amrex::Real u_actual,
    amrex::Real v_actual,
    amrex::Real u_geo,
    amrex::Real v_geo,
    amrex::Real& u_ageo,
    amrex::Real& v_ageo)
{
    u_ageo = u_actual - u_geo;
    v_ageo = v_actual - v_geo;
}

// ============================================================================
// Apply ageostrophic wind balance to wind field at lateral boundary
//
// Modifies wind components at the domain boundary to satisfy ageostrophic
// balance with respect to the pressure gradient.
//
// The boundary wind is adjusted to be:
//   U_boundary = U_geo + U_ageo_factor × (U_interior - U_geo)
//
// where U_ageo_factor is the ageostrophic fraction (typically 0.05-0.2).
//
// Inputs:
//   u_interior, v_interior - Wind components in interior domain [m/s]
//   pressure_grad_x, _y    - Pressure gradients [Pa/m]
//   params                 - AgeostrophicParams structure
//
// Returns tuple: (u_boundary, v_boundary) - Boundary wind components [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_ageostrophic_bc(
    amrex::Real u_interior,
    amrex::Real v_interior,
    amrex::Real& u_boundary,
    amrex::Real& v_boundary,
    const AgeostrophicParams& params)
{
    if (!params.enabled) {
        u_boundary = u_interior;
        v_boundary = v_interior;
        return;
    }
    
    // Compute Coriolis parameter
    const amrex::Real f = compute_coriolis_parameter(params.latitude);
    
    // Compute geostrophic wind
    amrex::Real u_geo, v_geo;
    compute_geostrophic_wind(
        params.pressure_gradient_x,
        params.pressure_gradient_y,
        params.air_density,
        f,
        u_geo, v_geo);
    
    // Compute ageostrophic component of interior wind
    amrex::Real u_ageo_interior, v_ageo_interior;
    compute_ageostrophic_wind(
        u_interior, v_interior,
        u_geo, v_geo,
        u_ageo_interior, v_ageo_interior);
    
    // Scale ageostrophic component by specified fraction
    const amrex::Real ageo_scale = params.ageostrophic_fraction;
    
    // Apply boundary condition: U = U_geo + ageo_scale × U_ageo
    u_boundary = u_geo + ageo_scale * u_ageo_interior;
    v_boundary = v_geo + ageo_scale * v_ageo_interior;
}

// ============================================================================
// Compute thermal wind vector
//
// The thermal wind relates the vertical wind shear to the horizontal
// temperature gradient:
//
// ∂U_geo/∂z = -(g/(T·f))·∂T/∂y  (zonal thermal wind)
// ∂V_geo/∂z = +(g/(T·f))·∂T/∂x  (meridional thermal wind)
//
// This is useful for understanding how the geostrophic wind changes with height.
//
// Inputs:
//   temp_grad_x        - Temperature gradient ∂T/∂x [K/m]
//   temp_grad_y        - Temperature gradient ∂T/∂y [K/m]
//   temperature        - Temperature [K]
//   coriolis_param     - Coriolis parameter f [1/s]
//
// Returns tuple: (dU_geo/dz, dV_geo/dz) - Vertical wind shear [m/s/m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_thermal_wind(
    amrex::Real temp_grad_x,
    amrex::Real temp_grad_y,
    amrex::Real temperature,
    amrex::Real coriolis_param,
    amrex::Real& du_geo_dz,
    amrex::Real& dv_geo_dz)
{
    constexpr amrex::Real g = amrex::Real(9.81);
    
    // Protect against invalid inputs
    if (std::abs(coriolis_param) < amrex::Real(1.0e-8) || temperature < amrex::Real(1.0)) {
        du_geo_dz = amrex::Real(0.0);
        dv_geo_dz = amrex::Real(0.0);
        return;
    }
    
    // Compute thermal wind components
    // ∂U_geo/∂z = -(g/(T·f))·∂T/∂y
    // ∂V_geo/∂z = +(g/(T·f))·∂T/∂x
    const amrex::Real factor = -g / (temperature * coriolis_param);
    
    du_geo_dz = factor * temp_grad_y;
    dv_geo_dz = -factor * temp_grad_x;
}

#endif // AGEOSTROPHIC_MODELS_H
