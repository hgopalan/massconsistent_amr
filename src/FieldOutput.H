// ============================================================================
// FieldOutput.H
// Unified output module for mass-consistent wind solver diagnostics
//
// This header consolidates all diagnostic field output into a single,
// organized interface for writing solver results to AMReX plotfiles and
// CSV formats. It provides:
//
// - Standardized field naming and organization
// - Multi-format output support (AMReEx plotfile, terrain-aligned CSV)
// - Diagnostic computation helpers
// - Field metadata and documentation
//
// Supported Output Fields (21+ diagnostics):
//
// Wind Components:
//   0  u              U-component of corrected wind [m/s]
//   1  v              V-component of corrected wind [m/s]
//   2  w              W-component of corrected wind [m/s]
//   3  vel_magnitude  Horizontal wind speed |U| = sqrt(u² + v²) [m/s]
//
// Initial Wind Field:
//   4  u0             U-component of initial log-law wind [m/s]
//   5  v0             V-component of initial log-law wind [m/s]
//   6  w0             W-component of initial log-law wind [m/s]
//
// Pressure & Mass Consistency:
//   7  lambda          Lagrange multiplier (normalized pressure-like variable)
//   8  div_before      Divergence of initial field ∇·u₀ [1/s]
//   9  div_after       Divergence of corrected field ∇·u [1/s]
//
// Terrain & Geometry:
//  10  terrain_z      Terrain elevation at each (i,j) location [m MSL]
//
// Surface Flux Diagnostics (Phase 2, Features 5-7):
//  11  heat_flux      Sensible heat flux SHF = ρ Cp u* θ* [W/m²]
//  12  drag_coeff     Drag coefficient Cd = (κ / ln(z/z₀))² [dimensionless]
//  13  tau_x          Shear stress x-component τ_x = ρ u*² (u/|U|) [Pa]
//  14  tau_y          Shear stress y-component τ_y = ρ u*² (v/|U|) [Pa]
//  15  u_star         Friction velocity u* [m/s]
//
// Boundary Layer & Stability (Phase 2, Features 23-24):
//  16  richardson_no  Richardson number Ri_b (bulk) [dimensionless]
//  17  bl_depth       Boundary layer depth [m]
//
// Terrain Analysis (Phase 2, Feature 22):
//  18  terrain_type   Classification of terrain type (0=smooth, 1=rough, etc.)
//  19  terrain_slope  Local terrain slope magnitude [dimensionless]
//  20  adaptive_z0    Adaptive aerodynamic roughness length [m]
//
// Total: 21 components in output MultiFab
//
// Usage Example:
//   FieldOutputManager manager(ba, dm, geom);
//   manager.ComputeAllDiagnostics(vel_corrected, vel_initial, lambda, ...);
//   manager.WriteAMReXPlotfile("plt_output", var_names);
//   manager.WriteTerrainAlignedCSV("wind_extract.csv", extract_agl);
//
// Physical Constants:
//   ρ (air density)     = 1.225 kg/m³ (sea level, 15°C)
//   Cp (specific heat)  = 1005 J/(kg·K) (dry air)
//   κ (von Karman)      = 0.41 (dimensionless)
//   g (gravitational)   = 9.81 m/s²
// ============================================================================

#ifndef FIELD_OUTPUT_H
#define FIELD_OUTPUT_H

#include <AMReX_REAL.H>
#include <AMReX_MultiFab.H>
#include <AMReX_Geometry.H>
#include <AMReX_BoxArray.H>
#include <AMReX_DistributionMapping.H>
#include <AMReX_PlotFileUtil.H>
#include <vector>
#include <string>
#include <cmath>

namespace FieldOutput {

// Physical constants for diagnostic calculations
namespace Constants {
    constexpr amrex::Real rho_air = amrex::Real(1.225);      // Air density [kg/m³]
    constexpr amrex::Real cp_air = amrex::Real(1005.0);      // Specific heat [J/(kg·K)]
    constexpr amrex::Real kappa = amrex::Real(0.41);         // von Karman constant
    constexpr amrex::Real g_accel = amrex::Real(9.81);       // Gravity [m/s²]
    constexpr amrex::Real T_ref = amrex::Real(288.15);       // Reference temp [K]
    constexpr amrex::Real theta_star_default = amrex::Real(0.1); // Default temp scale [K]
}

// ============================================================================
// Field Enumeration — standardized index mapping for output components
// ============================================================================
enum class FieldIndex : int {
    // Wind components
    U = 0,               // U-component [m/s]
    V = 1,               // V-component [m/s]
    W = 2,               // W-component [m/s]
    VEL_MAG = 3,         // Horizontal wind speed [m/s]
    
    // Initial wind field
    U0 = 4,              // Initial U [m/s]
    V0 = 5,              // Initial V [m/s]
    W0 = 6,              // Initial W [m/s]
    
    // Pressure & continuity
    LAMBDA = 7,          // Lagrange multiplier
    DIV_BEFORE = 8,      // Divergence before correction [1/s]
    DIV_AFTER = 9,       // Divergence after correction [1/s]
    
    // Terrain
    TERRAIN_Z = 10,      // Terrain elevation [m MSL]
    
    // Surface diagnostics
    HEAT_FLUX = 11,      // Sensible heat flux [W/m²]
    DRAG_COEFF = 12,     // Drag coefficient [dimensionless]
    TAU_X = 13,          // Shear stress x [Pa]
    TAU_Y = 14,          // Shear stress y [Pa]
    USTAR = 15,          // Friction velocity [m/s]
    
    // Boundary layer & stability
    RICHARDSON_NO = 16,  // Richardson number [dimensionless]
    BL_DEPTH = 17,       // Boundary layer depth [m]
    
    // Terrain analysis
    TERRAIN_TYPE = 18,   // Terrain type classification
    TERRAIN_SLOPE = 19,  // Terrain slope [dimensionless]
    ADAPTIVE_Z0 = 20,    // Adaptive roughness [m]
    
    // Total count
    NFIELDS = 21
};

// ============================================================================
// Get standardized field name from enumeration
// ============================================================================
AMREX_FORCE_INLINE
std::string GetFieldName(FieldIndex idx) {
    switch (idx) {
        case FieldIndex::U: return "u";
        case FieldIndex::V: return "v";
        case FieldIndex::W: return "w";
        case FieldIndex::VEL_MAG: return "vel_magnitude";
        case FieldIndex::U0: return "u0";
        case FieldIndex::V0: return "v0";
        case FieldIndex::W0: return "w0";
        case FieldIndex::LAMBDA: return "lambda";
        case FieldIndex::DIV_BEFORE: return "div_before";
        case FieldIndex::DIV_AFTER: return "div_after";
        case FieldIndex::TERRAIN_Z: return "terrain_z";
        case FieldIndex::HEAT_FLUX: return "heat_flux";
        case FieldIndex::DRAG_COEFF: return "drag_coeff";
        case FieldIndex::TAU_X: return "tau_x";
        case FieldIndex::TAU_Y: return "tau_y";
        case FieldIndex::USTAR: return "u_star";
        case FieldIndex::RICHARDSON_NO: return "richardson_no";
        case FieldIndex::BL_DEPTH: return "bl_depth";
        case FieldIndex::TERRAIN_TYPE: return "terrain_type";
        case FieldIndex::TERRAIN_SLOPE: return "terrain_slope";
        case FieldIndex::ADAPTIVE_Z0: return "adaptive_z0";
        default: return "unknown";
    }
}

// ============================================================================
// Get standardized field description for documentation
// ============================================================================
AMREX_FORCE_INLINE
std::string GetFieldDescription(FieldIndex idx) {
    switch (idx) {
        case FieldIndex::U: return "U-component of corrected wind [m/s]";
        case FieldIndex::V: return "V-component of corrected wind [m/s]";
        case FieldIndex::W: return "W-component of corrected wind [m/s]";
        case FieldIndex::VEL_MAG: return "Horizontal wind speed |U| = sqrt(u^2 + v^2) [m/s]";
        case FieldIndex::U0: return "U-component of initial log-law wind [m/s]";
        case FieldIndex::V0: return "V-component of initial log-law wind [m/s]";
        case FieldIndex::W0: return "W-component of initial log-law wind [m/s]";
        case FieldIndex::LAMBDA: return "Lagrange multiplier for mass consistency";
        case FieldIndex::DIV_BEFORE: return "Divergence of initial field [1/s]";
        case FieldIndex::DIV_AFTER: return "Divergence of corrected field [1/s]";
        case FieldIndex::TERRAIN_Z: return "Terrain elevation at (i,j) [m MSL]";
        case FieldIndex::HEAT_FLUX: return "Sensible heat flux SHF = rho Cp u* theta* [W/m^2]";
        case FieldIndex::DRAG_COEFF: return "Drag coefficient Cd = (kappa/ln(z/z0))^2";
        case FieldIndex::TAU_X: return "Shear stress x-component [Pa]";
        case FieldIndex::TAU_Y: return "Shear stress y-component [Pa]";
        case FieldIndex::USTAR: return "Friction velocity u* [m/s]";
        case FieldIndex::RICHARDSON_NO: return "Richardson number Ri_b (bulk)";
        case FieldIndex::BL_DEPTH: return "Boundary layer depth [m]";
        case FieldIndex::TERRAIN_TYPE: return "Terrain type classification";
        case FieldIndex::TERRAIN_SLOPE: return "Local terrain slope magnitude";
        case FieldIndex::ADAPTIVE_Z0: return "Adaptive aerodynamic roughness length [m]";
        default: return "unknown field";
    }
}

// ============================================================================
// Build standard variable name vector
// ============================================================================
AMREX_FORCE_INLINE
amrex::Vector<std::string> GetStandardVarNames() {
    amrex::Vector<std::string> names;
    for (int i = 0; i < static_cast<int>(FieldIndex::NFIELDS); ++i) {
        names.push_back(GetFieldName(static_cast<FieldIndex>(i)));
    }
    return names;
}

// ============================================================================
// Compute friction velocity from log-law profile
//
// u* = κ u_mag / ln((z_agl + z0) / z0)
//
// Inputs:
//   u_mag   - Horizontal wind magnitude [m/s]
//   z_agl   - Height above ground level [m]
//   z0      - Aerodynamic roughness length [m]
//
// Returns:
//   Friction velocity u* [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real ComputeFrictionVelocity(
    amrex::Real u_mag,
    amrex::Real z_agl,
    amrex::Real z0)
{
    if (z_agl <= 0.0 || u_mag < 1.0e-6 || z0 < 1.0e-6) {
        return 0.0;
    }
    
    amrex::Real log_term = std::log((z_agl + z0) / z0);
    if (log_term < 0.1) {
        return 0.0;
    }
    
    return Constants::kappa * u_mag / log_term;
}

// ============================================================================
// Compute drag coefficient
//
// Cd = (κ / ln((z_agl + z0) / z0))²
//
// Inputs:
//   z_agl   - Height above ground level [m]
//   z0      - Aerodynamic roughness length [m]
//
// Returns:
//   Dimensionless drag coefficient Cd
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real ComputeDragCoefficient(
    amrex::Real z_agl,
    amrex::Real z0)
{
    if (z_agl <= 0.0 || z0 < 1.0e-6) {
        return 0.0;
    }
    
    amrex::Real log_term = std::log((z_agl + z0) / z0);
    if (log_term < 0.1) {
        return 0.0;
    }
    
    amrex::Real cd_term = Constants::kappa / log_term;
    return cd_term * cd_term;
}

// ============================================================================
// Compute sensible heat flux
//
// SHF = ρ Cp u* θ*
//
// Inputs:
//   u_star      - Friction velocity [m/s]
//   theta_star  - Temperature scale [K]
//   rho         - Air density [kg/m³]
//   cp          - Specific heat [J/(kg·K)]
//
// Returns:
//   Sensible heat flux [W/m²]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real ComputeSensibleHeatFlux(
    amrex::Real u_star,
    amrex::Real theta_star,
    amrex::Real rho = Constants::rho_air,
    amrex::Real cp = Constants::cp_air)
{
    return rho * cp * u_star * theta_star;
}

// ============================================================================
// Compute momentum flux (shear stress)
//
// τ = ρ u*²
// τ_x = τ × (u / |U|)
// τ_y = τ × (v / |U|)
//
// Inputs:
//   u_star      - Friction velocity [m/s]
//   u, v        - Wind components [m/s]
//   u_mag       - Horizontal wind magnitude [m/s]
//   rho         - Air density [kg/m³]
//
// Returns:
//   Component-wise momentum fluxes (tau_x, tau_y) [Pa]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
void ComputeMomentumFlux(
    amrex::Real u_star,
    amrex::Real u,
    amrex::Real v,
    amrex::Real u_mag,
    amrex::Real rho,
    amrex::Real& tau_x,
    amrex::Real& tau_y)
{
    if (u_mag < 1.0e-6) {
        tau_x = 0.0;
        tau_y = 0.0;
        return;
    }
    
    amrex::Real tau_mag = rho * u_star * u_star;
    tau_x = tau_mag * (u / u_mag);
    tau_y = tau_mag * (v / u_mag);
}

} // namespace FieldOutput

#endif // FIELD_OUTPUT_H
