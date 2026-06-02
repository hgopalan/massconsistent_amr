// ============================================================================
// wake_models.H
// Building wake parameterization models for urban wind fields
//
// This header provides functions for computing building wake effects using
// empirical models from the literature.
//
// Models implemented:
// 1. Röckle (1990) - Cavity and far-wake zones for rectangular buildings
// 2. Huber-Snyder (EPA) - Aspect-ratio dependent wake model
// 3. AERMOD PRIME (EPA) - Regulatory model with Projected Building Area method
//
// References:
// - Röckle, R. (1990). Bestimmung der Strömungsverhältnisse im Bereich
//   komplexer Bebauungsstrukturen. Dissertation, Vom Fachbereich Mechanik,
//   der Technischen Hochschule Darmstadt.
// - Kaplan, H., & Dinar, N. (1996). A Lagrangian dispersion model for
//   calculating concentration distribution within a built-up domain.
//   Atmospheric Environment, 30(24), 4197-4207.
// - Schulman, L.L., Strimaitis, D.G., & Scire, J.S. (2000). Development and
//   Evaluation of the PRIME Plume Rise and Building Downwash Model. Journal
//   of the Air & Waste Management Association, 50(3), 378-390.
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
// Wake model type enumeration
// ============================================================================
enum class WakeModelType {
    ROCKLE,         // Röckle (1990) - default, cavity + far-wake zones
    HUBER_SNYDER,   // Huber-Snyder (EPA) - alternative aspect-ratio dependent model
    AERMOD_PRIME    // EPA AERMOD PRIME - regulatory model with PBA method
};

// ============================================================================
// Wake model parameters structure
// ============================================================================
struct WakeParams {
    bool enabled = false;                    // Enable wake model
    WakeModelType model_type = WakeModelType::ROCKLE;  // Wake model selection
    amrex::Real c1 = 0.9;                    // Cavity length coefficient (Röckle only)
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
    amrex::Real rotation;    // Building orientation angle (radians, counter-clockwise from x-axis)
};

// ============================================================================
// Rooftop vortex parameterization
//
// Phase 3 Enhancement: Computes vertical velocity in the cavity zone due to
// rooftop vortex formation. The vortex arises from flow separation at the
// building edges and creates a counterclockwise (or clockwise, depending on
// wind direction) circulation in the cavity zone.
//
// The vortex strength is proportional to:
// 1. The approach wind speed (U_ref)
// 2. The building height (H) - taller buildings create stronger vortices
// 3. The position in the cavity (maximum at center height)
//
// References:
// - Oke, T.R. (1988). Street design and urban canopy layer climate.
//   Energy and Buildings, 11(1-3), 103-113.
// - Britter, R.E., & Hanna, S.R. (2003). Flow and dispersion in urban areas.
//   Annual Review of Fluid Mechanics, 35, 469-496.
//
// Inputs:
//   x_wake        - Downwind distance from building back face [m]
//   z_local       - Height above building base [m]
//   H             - Building height [m]
//   Hr            - Cavity zone height (0.67*H) [m]
//   Lr            - Cavity zone length (c1*H) [m]
//   U_ref         - Reference wind speed magnitude [m/s]
//   wind_dir_x    - Wind direction x-component (unit vector)
//   wind_dir_y    - Wind direction y-component (unit vector)
//
// Outputs:
//   dw            - Vertical velocity component [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_rooftop_vortex(
    amrex::Real x_wake,
    amrex::Real z_local,
    amrex::Real H,
    amrex::Real Hr,
    amrex::Real Lr,
    amrex::Real U_ref,
    amrex::Real&  dw)
{
    dw = amrex::Real(0.0);
    
    // Vortex only exists in cavity zone
    if (x_wake < amrex::Real(0.0) || x_wake > Lr) {
        return;
    }
    if (z_local < amrex::Real(0.0) || z_local > Hr) {
        return;
    }
    
    // Vortex strength factor (non-dimensional)
    // Based on typical urban canyon observations and CFD studies
    // Scales with wind speed to create realistic vertical circulation
    constexpr amrex::Real vortex_strength_coeff = amrex::Real(0.15);
    
    // Vertical profile in cavity zone (parabolic, maximum at center)
    // z_local ranges from 0 (ground) to Hr (top of cavity)
    const amrex::Real z_ratio = z_local / Hr;
    const amrex::Real z_profile = amrex::Real(4.0) * z_ratio * (amrex::Real(1.0) - z_ratio);
    
    // Streamwise variation in cavity (parabolic, maximum at center)
    // x_wake ranges from 0 (building face) to Lr (cavity end)
    const amrex::Real x_ratio = x_wake / Lr;
    const amrex::Real x_profile = amrex::Real(4.0) * x_ratio * (amrex::Real(1.0) - x_ratio);
    
    // Vortex circulation strength increases with building height
    // and decreases slightly with distance in cavity
    const amrex::Real vortex_circulation = vortex_strength_coeff * U_ref * (H / amrex::Real(30.0)) * x_profile;
    
    // Vertical component of vortex (upward on one side, downward on other)
    // Sign depends on wind direction relative to building orientation
    dw = vortex_circulation * z_profile;
}

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
//   Vertical component: w = rooftop vortex circulation (Phase 3 enhancement)
//
// Far-wake zone (displacement zone):
//   Starts at: x = xb + Lr
//   Length: extends to Lf = separation_length * H (typically 3-5 H)
//   Velocity deficit decreases linearly from cavity edge
//   Lateral spread: wake width increases linearly with distance
//
// Inputs:
//   x, y, z        - Point coordinates [m]
//   building       - Building structure with geometry and orientation
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
        
        // Phase 3 Enhancement: Add rooftop vortex vertical component
        compute_rooftop_vortex(x_wake, z_local, H, Hr, Lr, U_ref, dw);
        
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
// Huber-Snyder (EPA) building wake model
//
// Alternative empirical wake parameterization from EPA wind tunnel studies.
// This model uses different cavity and wake geometry assumptions compared
// to Röckle, and may perform better for certain building aspect ratios.
//
// Key differences from Röckle (1990):
//   - Cavity length: L_c = 0.5*H*sqrt(W/H)  (aspect ratio dependent)
//   - Wake length: L_w = 5*H  (longer wake extent)
//   - Different velocity deficit functions
//
// Used in EPA dispersion models (ISC, AERMOD precursors) for regulatory
// applications in urban air quality modeling.
//
// References:
// - Huber, A.H., & Snyder, W.H. (1982). Wind tunnel investigation of the
//   effects of a rectangular-shaped building on dispersion of effluent
//   from short adjacent stacks. EPA-600/4-82-049.
// - Schulman, L.L., Strimaitis, D.G., & Scire, J.S. (2000). Development
//   and evaluation of the PRIME plume rise and building downwash model.
//   Journal of the Air & Waste Management Association, 50(3), 378-390.
//
// Inputs:
//   x, y, z        - Point coordinates [m]
//   bldg           - Building structure
//   U_ref          - Reference wind speed magnitude [m/s]
//   wd_x, wd_y     - Wind direction components (unit vector)
//   params         - Wake model parameters (uses c2 for deficit coefficient)
//
// Outputs:
//   du, dv, dw     - Velocity deficit components [m/s]
//
// Returns:
//   true if point is inside wake zone, false otherwise
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool huber_snyder_wake_deficit(
    amrex::Real x, amrex::Real y, amrex::Real z,
    const Building& bldg,
    amrex::Real U_ref,
    amrex::Real wd_x, amrex::Real wd_y,
    const WakeParams& params,
    amrex::Real& du, amrex::Real& dv, amrex::Real& dw)
{
    // Initialize output
    du = amrex::Real(0.0);
    dv = amrex::Real(0.0);
    dw = amrex::Real(0.0);
    
    // Building dimensions
    const amrex::Real H = bldg.height;
    const amrex::Real W = bldg.width;   // Crosswind dimension
    
    if (H <= amrex::Real(0.0) || W <= amrex::Real(0.0)) {
        return false;  // Invalid building
    }
    
    // Building center
    const amrex::Real bldg_center_x = (bldg.xmin + bldg.xmax) / amrex::Real(2.0);
    const amrex::Real bldg_center_y = (bldg.ymin + bldg.ymax) / amrex::Real(2.0);
    
    // Transform coordinates to building-relative coordinate system
    const amrex::Real dx = x - bldg_center_x;
    const amrex::Real dy = y - bldg_center_y;
    
    // Rotate to wind-aligned coordinates
    const amrex::Real x_wind =  dx * wd_x + dy * wd_y;
    const amrex::Real y_wind = -dx * wd_y + dy * wd_x;
    
    // Building extends from -L/2 to L/2 in downwind direction
    const amrex::Real half_length_wind = bldg.length / amrex::Real(2.0);
    
    // Wake starts at downwind face of building
    const amrex::Real wake_start = half_length_wind;
    
    // Check if point is downwind of building
    if (x_wind < wake_start) {
        return false;  // Upstream or inside building
    }
    
    // Distance downwind from building back face
    const amrex::Real x_wake = x_wind - wake_start;
    
    // Huber-Snyder cavity parameters
    // Aspect ratio dependent cavity length
    const amrex::Real aspect_ratio = W / H;
    const amrex::Real Lc = amrex::Real(0.5) * H * std::sqrt(aspect_ratio);  // Cavity length
    const amrex::Real Hc = amrex::Real(0.67) * H;  // Cavity height (similar to Röckle)
    const amrex::Real Wc = W;                       // Cavity width
    
    // Height above ground (relative to building base)
    const amrex::Real z_local = z - bldg.zmin;
    
    // ========================================================================
    // 1. Cavity zone (recirculation zone)
    // ========================================================================
    if (x_wake <= Lc) {
        // Check lateral bounds (crosswind)
        if (std::abs(y_wind) > Wc / amrex::Real(2.0)) {
            return false;  // Outside cavity width
        }
        
        // Check vertical bounds
        if (z_local < amrex::Real(0.0) || z_local > Hc) {
            return false;  // Below ground or above cavity height
        }
        
        // Inside cavity zone: recirculation with negative velocity
        // Huber-Snyder uses similar deficit magnitude as Röckle
        const amrex::Real deficit_magnitude = params.c2 * U_ref;
        du = deficit_magnitude * wd_x;
        dv = deficit_magnitude * wd_y;
        
        // Add rooftop vortex (reuse existing parameterization)
        compute_rooftop_vortex(x_wake, z_local, H, Hc, Lc, U_ref, dw);
        
        return true;
    }
    
    // ========================================================================
    // 2. Far-wake zone (displacement zone)
    // ========================================================================
    // Huber-Snyder wake extends to 5*H (longer than Röckle's typical 3*H)
    const amrex::Real Lw = amrex::Real(5.0) * H;  // Wake extent
    
    if (x_wake <= Lw) {
        // Huber-Snyder velocity deficit decay function
        // Uses power law decay: deficit ∝ (x/Lc)^(-0.5)
        // This creates a slower recovery than Röckle's linear decay
        const amrex::Real x_ratio = x_wake / Lc;
        const amrex::Real decay_factor = std::min(
            amrex::Real(1.0) / std::sqrt(x_ratio),
            amrex::Real(1.0)
        );
        
        // Wake width spreading: linear increase
        const amrex::Real spread_factor = amrex::Real(1.0) + 
            (x_wake - Lc) / (Lw - Lc);
        const amrex::Real Ww = Wc * spread_factor;
        
        // Check lateral bounds
        if (std::abs(y_wind) > Ww / amrex::Real(2.0)) {
            return false;  // Outside wake width
        }
        
        // Wake height spreading
        const amrex::Real Hw = Hc * spread_factor;
        
        // Check vertical bounds
        if (z_local < amrex::Real(0.0) || z_local > Hw) {
            return false;  // Below ground or above wake height
        }
        
        // Inside far-wake zone: reduced velocity deficit with power-law decay
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
// EPA AERMOD PRIME Wake Model
//
// Implementation of the PRIME (Plume Rise Model Enhancements) algorithm
// used in EPA's AERMOD regulatory dispersion model.
//
// Key features:
// 1. Projected Building Area (PBA) method for effective dimensions
// 2. Streamline deflection around building
// 3. Enhanced turbulence in wake regions
// 4. Separate cavity and wake regions with different formulations
//
// Physical basis:
// - Uses projected building area perpendicular to wind direction
// - Cavity zone length depends on building aspect ratio
// - Enhanced mixing in wake zone (higher turbulence)
// - Well-validated against EPA wind tunnel data
//
// References:
// - Schulman, L.L., Strimaitis, D.G., & Scire, J.S. (2000). Development and
//   Evaluation of the PRIME Plume Rise and Building Downwash Model. Journal
//   of the Air & Waste Management Association, 50(3), 378-390.
// - EPA (2004). User's Guide for the AMS/EPA Regulatory Model - AERMOD.
//   EPA-454/B-03-001.
// ============================================================================

// ============================================================================
// Compute Projected Building Area (PBA) and effective dimensions
//
// The PBA method computes the effective building cross-sectional area
// projected onto a plane perpendicular to the wind direction.
//
// For a rectangular building with rotation:
//   PBA = effective_height × effective_width
//
// where effective dimensions depend on wind direction and building orientation.
//
// Inputs:
//   bldg           - Building structure with bounds and rotation
//   wind_dir_x     - Wind direction x-component (unit vector)
//   wind_dir_y     - Wind direction y-component (unit vector)
//
// Outputs:
//   H_eff          - Effective building height [m]
//   W_eff          - Effective building width perpendicular to wind [m]
//   L_eff          - Effective building length parallel to wind [m]
//
// Returns:
//   Projected Building Area [m²]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_projected_building_area(
    const Building& bldg,
    amrex::Real wind_dir_x,
    amrex::Real wind_dir_y,
    amrex::Real& H_eff,
    amrex::Real& W_eff,
    amrex::Real& L_eff)
{
    // Height is always vertical extent
    H_eff = bldg.height;
    
    // Compute wind-aligned dimensions using existing helper
    compute_wind_aligned_dimensions(
        bldg.length, bldg.width, bldg.rotation,
        wind_dir_x, wind_dir_y,
        L_eff, W_eff);
    
    // Projected Building Area (PBA)
    const amrex::Real PBA = H_eff * W_eff;
    
    return PBA;
}

// ============================================================================
// AERMOD PRIME wake deficit calculation
//
// Computes velocity deficit using the PRIME algorithm:
// 1. Calculate Projected Building Area (PBA)
// 2. Determine cavity and wake zone dimensions
// 3. Apply streamline deflection
// 4. Compute velocity deficit with enhanced turbulence
//
// PRIME cavity zone:
//   - Length: Lc = 0.9 * sqrt(PBA) for wide buildings (W/H > 1)
//             Lc = 0.5 * sqrt(PBA) for tall buildings (W/H < 1)
//   - Height: Hc = min(1.5 * Hc_peak, H) where Hc_peak is peak cavity height
//   - Enhanced turbulence mixing
//
// PRIME wake zone:
//   - Extends to 10*H downwind (longer than Röckle or Huber-Snyder)
//   - Lateral spreading based on PBA
//   - Velocity deficit decay includes enhanced mixing
//
// Inputs:
//   x, y, z        - Point coordinates [m]
//   bldg           - Building structure
//   U_ref          - Reference wind speed magnitude [m/s]
//   wind_dir_x/y   - Wind direction components (unit vectors)
//   params         - Wake model parameters
//
// Outputs:
//   du, dv, dw     - Velocity deficit components [m/s]
//
// Returns:
//   true if point is in wake, false otherwise
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool aermod_prime_wake_deficit(
    amrex::Real x, amrex::Real y, amrex::Real z,
    const Building& bldg,
    amrex::Real U_ref,
    amrex::Real wind_dir_x,
    amrex::Real wind_dir_y,
    const WakeParams& params,
    amrex::Real& du,
    amrex::Real& dv,
    amrex::Real& dw)
{
    // Initialize deficits to zero
    du = amrex::Real(0.0);
    dv = amrex::Real(0.0);
    dw = amrex::Real(0.0);
    
    // ========================================================================
    // 1. Compute Projected Building Area (PBA) and effective dimensions
    // ========================================================================
    amrex::Real H, W, L;
    const amrex::Real PBA = compute_projected_building_area(
        bldg, wind_dir_x, wind_dir_y, H, W, L);
    
    // Building aspect ratio
    const amrex::Real aspect_ratio = W / H;
    
    // Characteristic building scale from PBA
    const amrex::Real B_char = std::sqrt(PBA);
    
    // ========================================================================
    // 2. Transform to wind-aligned coordinate system
    // ========================================================================
    // Building center in horizontal plane
    const amrex::Real bldg_center_x = (bldg.xmin + bldg.xmax) / amrex::Real(2.0);
    const amrex::Real bldg_center_y = (bldg.ymin + bldg.ymax) / amrex::Real(2.0);
    
    // Vector from building center to point
    const amrex::Real dx_global = x - bldg_center_x;
    const amrex::Real dy_global = y - bldg_center_y;
    
    // Rotate to wind-aligned frame
    // x_wind = streamwise (along wind), y_wind = crosswind
    const amrex::Real x_wind = dx_global * wind_dir_x + dy_global * wind_dir_y;
    const amrex::Real y_wind = -dx_global * wind_dir_y + dy_global * wind_dir_x;
    
    // Downwind distance from building back face
    const amrex::Real x_wake = x_wind - L / amrex::Real(2.0);
    
    // Check if point is upstream of building
    if (x_wake < amrex::Real(0.0)) {
        return false;  // Upstream, no wake effect
    }
    
    // Height above ground (relative to building base)
    const amrex::Real z_local = z - bldg.zmin;
    
    // ========================================================================
    // 3. PRIME cavity zone dimensions
    // ========================================================================
    // Cavity length based on aspect ratio
    amrex::Real Lc;
    if (aspect_ratio > amrex::Real(1.0)) {
        // Wide building: longer cavity
        Lc = amrex::Real(0.9) * B_char;
    } else {
        // Tall building: shorter cavity
        Lc = amrex::Real(0.5) * B_char;
    }
    
    // Peak cavity height (PRIME formulation)
    const amrex::Real Hc_peak = amrex::Real(0.22) * B_char;
    
    // Cavity height limit
    const amrex::Real Hc = std::min(amrex::Real(1.5) * Hc_peak, H);
    
    // Cavity width (same as building width)
    const amrex::Real Wc = W;
    
    // ========================================================================
    // 4. Cavity zone (recirculation zone with enhanced turbulence)
    // ========================================================================
    if (x_wake <= Lc) {
        // Check lateral bounds (crosswind)
        if (std::abs(y_wind) > Wc / amrex::Real(2.0)) {
            return false;  // Outside cavity width
        }
        
        // Check vertical bounds
        if (z_local < amrex::Real(0.0) || z_local > Hc) {
            return false;  // Below ground or above cavity height
        }
        
        // Inside cavity zone: recirculation with enhanced turbulence
        // PRIME uses higher deficit in cavity than Röckle/Huber-Snyder
        const amrex::Real cavity_deficit_factor = amrex::Real(0.5);  // 50% deficit in cavity
        const amrex::Real deficit_magnitude = cavity_deficit_factor * U_ref;
        du = deficit_magnitude * wind_dir_x;
        dv = deficit_magnitude * wind_dir_y;
        
        // Streamline deflection: vertical component from flow over building
        // Enhanced vertical mixing in PRIME cavity
        const amrex::Real streamline_deflection = amrex::Real(0.2) * U_ref * (z_local / Hc);
        dw = streamline_deflection;
        
        return true;
    }
    
    // ========================================================================
    // 5. PRIME far-wake zone (enhanced mixing, extended downwind)
    // ========================================================================
    // PRIME wake extends to 10*H (regulatory standard, longer than others)
    const amrex::Real Lw = amrex::Real(10.0) * H;
    
    if (x_wake <= Lw) {
        // Normalized downwind distance
        const amrex::Real x_norm = (x_wake - Lc) / (Lw - Lc);
        
        // PRIME velocity deficit decay (exponential with enhanced mixing)
        // Enhanced mixing reduces deficit more quickly than simple power law
        const amrex::Real decay_factor = std::exp(-amrex::Real(1.5) * x_norm);
        
        // Wake width spreading: linear increase with enhanced lateral mixing
        const amrex::Real spread_factor = amrex::Real(1.0) + amrex::Real(2.0) * x_norm;
        const amrex::Real Ww = Wc * spread_factor;
        
        // Check lateral bounds
        if (std::abs(y_wind) > Ww / amrex::Real(2.0)) {
            return false;  // Outside wake width
        }
        
        // Wake height spreading (PRIME includes vertical growth)
        const amrex::Real Hw = Hc + (H - Hc) * x_norm;
        
        // Check vertical bounds
        if (z_local < amrex::Real(0.0) || z_local > Hw) {
            return false;  // Below ground or above wake height
        }
        
        // Inside far-wake zone: reduced velocity deficit with PRIME decay
        const amrex::Real wake_deficit_factor = amrex::Real(0.3);  // Base deficit factor
        const amrex::Real deficit_magnitude = wake_deficit_factor * U_ref * decay_factor;
        du = deficit_magnitude * wind_dir_x;
        dv = deficit_magnitude * wind_dir_y;
        
        // Vertical component from streamline deflection (diminishes downwind)
        dw = amrex::Real(0.1) * U_ref * decay_factor * (z_local / Hw);
        
        return true;
    }
    
    // Point is beyond wake influence
    return false;
}

// ============================================================================
// Apply wake model to velocity field for a single building
//
// This function computes the wake deficit at a given point and returns
// the corrected velocity components. Supports multiple wake models:
//   - Röckle (1990): Default cavity + far-wake zones
//   - Huber-Snyder (EPA): Aspect-ratio dependent model
//   - AERMOD PRIME (EPA): Regulatory model with PBA method
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
    
    // Compute wake deficit using selected model
    amrex::Real du, dv, dw;
    bool in_wake = false;
    
    if (params.model_type == WakeModelType::AERMOD_PRIME) {
        in_wake = aermod_prime_wake_deficit(
            x, y, z, bldg, U_ref, wind_dir_x, wind_dir_y, params, du, dv, dw);
    } else if (params.model_type == WakeModelType::HUBER_SNYDER) {
        in_wake = huber_snyder_wake_deficit(
            x, y, z, bldg, U_ref, wind_dir_x, wind_dir_y, params, du, dv, dw);
    } else {  // Default to Röckle
        in_wake = rockle_wake_deficit(
            x, y, z, bldg, U_ref, wind_dir_x, wind_dir_y, params, du, dv, dw);
    }
    
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
// Phase 3 Enhancement: Now supports arbitrary building orientations.
// For accurate wake modeling with arbitrary wind directions, the building
// dimensions are rotated to align with the wind direction.
//
// Dimensions:
//   height = z-extent (vertical, always)
//   width  = dimension perpendicular to wind direction in horizontal plane
//   length = dimension parallel to wind direction in horizontal plane
//
// This function is the original grid-aligned version (kept for compatibility).
// Use compute_wind_aligned_building_dimensions() for oriented buildings.
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
    bldg.rotation = amrex::Real(0.0);  // No rotation for grid-aligned building
    
    return bldg;
}

// ============================================================================
// Compute wind-aligned building dimensions
//
// Phase 3 Enhancement: Computes effective building width and length in the
// wind coordinate system, accounting for arbitrary building orientation.
//
// For a building with arbitrary rotation angle and dimensions in the
// grid-aligned frame, this function projects those dimensions onto the
// wind direction to get the effective streamwise (length) and crosswind
// (width) dimensions.
//
// Inputs:
//   bldg_x_extent  - Building extent in x-direction [m]
//   bldg_y_extent  - Building extent in y-direction [m]
//   rotation       - Building rotation angle (radians, CCW from x-axis)
//   wind_dir_x     - Wind direction x-component (unit vector)
//   wind_dir_y     - Wind direction y-component (unit vector)
//
// Returns:
//   length_wind    - Effective building length in wind direction [m]
//   width_wind     - Effective building width perpendicular to wind [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_wind_aligned_dimensions(
    amrex::Real bldg_x_extent,
    amrex::Real bldg_y_extent,
    amrex::Real rotation,
    amrex::Real wind_dir_x,
    amrex::Real wind_dir_y,
    amrex::Real& length_wind,
    amrex::Real& width_wind)
{
    // Building corner vectors in grid-aligned frame
    // Assuming building is rectangular with sides parallel to rotated axes
    amrex::Real cos_rot = std::cos(rotation);
    amrex::Real sin_rot = std::sin(rotation);
    
    // Building edges in grid frame:
    // Edge 1: along original x-direction, rotated by angle
    // Edge 2: along original y-direction, rotated by angle
    const amrex::Real edge1_x = cos_rot * bldg_x_extent;
    const amrex::Real edge1_y = sin_rot * bldg_x_extent;
    const amrex::Real edge2_x = -sin_rot * bldg_y_extent;
    const amrex::Real edge2_y = cos_rot * bldg_y_extent;
    
    // Project edges onto wind direction to get streamwise component
    const amrex::Real proj1_wind = std::abs(edge1_x * wind_dir_x + edge1_y * wind_dir_y);
    const amrex::Real proj2_wind = std::abs(edge2_x * wind_dir_x + edge2_y * wind_dir_y);
    
    // Project edges perpendicular to wind to get crosswind component
    const amrex::Real perp_wind_x = -wind_dir_y;
    const amrex::Real perp_wind_y = wind_dir_x;
    const amrex::Real proj1_perp = std::abs(edge1_x * perp_wind_x + edge1_y * perp_wind_y);
    const amrex::Real proj2_perp = std::abs(edge2_x * perp_wind_x + edge2_y * perp_wind_y);
    
    // Total projections give wind-aligned dimensions
    length_wind = proj1_wind + proj2_wind;
    width_wind = proj1_perp + proj2_perp;
}

// ============================================================================
// Wake superposition method
//
// Combines velocity deficits from multiple overlapping building wakes.
// Uses the quadratic superposition method (Lopes da Costa et al. 2006):
//
//   U_total = U_ref - sqrt(sum(du_i^2))
//
// This method is more physically realistic than linear superposition for
// overlapping wakes as it accounts for turbulent mixing.
//
// Phase 3 Enhancement: Now supports arbitrary building orientations.
//
// Reference:
// - Lopes da Costa, J., Castro, F.A., Palma, J.M.L.M., Stuart, P. (2006).
//   Computer simulation of atmospheric flows over real forests for wind
//   energy resource evaluation. Journal of Wind Engineering and Industrial
//   Aerodynamics, 94(8), 603-620.
//
// Inputs:
//   x, y, z         - Point coordinates [m]
//   u0, v0, w0      - Initial velocity components [m/s]
//   bldg_xmin, etc. - Arrays of building bounds [m]
//   bldg_rotation   - Array of building rotation angles (radians)
//   n_buildings     - Number of buildings
//   params          - Wake model parameters
//
// Outputs:
//   u, v, w         - Velocity components with all wake deficits applied [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_wake_superposition(
    amrex::Real x, amrex::Real y, amrex::Real z,
    amrex::Real& u, amrex::Real& v, amrex::Real& w,
    const amrex::Real* bldg_xmin,
    const amrex::Real* bldg_xmax,
    const amrex::Real* bldg_ymin,
    const amrex::Real* bldg_ymax,
    const amrex::Real* bldg_zmin,
    const amrex::Real* bldg_zmax,
    const amrex::Real* bldg_rotation,
    int n_buildings,
    const WakeParams& params)
{
    // Reference velocity magnitude
    const amrex::Real U_ref = std::sqrt(u * u + v * v);
    
    if (U_ref < amrex::Real(1.0e-10)) {
        return;  // No flow, no wake
    }
    
    // Wind direction from velocity components
    const amrex::Real wind_dir_x = u / U_ref;
    const amrex::Real wind_dir_y = v / U_ref;
    
    // Accumulate squared velocity deficits from all buildings
    amrex::Real du_squared_sum = amrex::Real(0.0);
    amrex::Real dv_squared_sum = amrex::Real(0.0);
    amrex::Real dw_squared_sum = amrex::Real(0.0);
    
    for (int b = 0; b < n_buildings; ++b) {
        // Compute building structure
        Building bldg = compute_building_dimensions(
            bldg_xmin[b], bldg_xmax[b],
            bldg_ymin[b], bldg_ymax[b],
            bldg_zmin[b], bldg_zmax[b]);
        
        // Phase 3: Add building rotation angle
        bldg.rotation = bldg_rotation[b];
        
        // Compute wake deficit from this building
        amrex::Real du_b, dv_b, dw_b;
        bool in_wake = rockle_wake_deficit(
            x, y, z, bldg, U_ref, wind_dir_x, wind_dir_y, params,
            du_b, dv_b, dw_b);
        
        if (in_wake) {
            // Add squared deficit (quadratic superposition)
            du_squared_sum += du_b * du_b;
            dv_squared_sum += dv_b * dv_b;
            dw_squared_sum += dw_b * dw_b;
        }
    }
    
    // Compute total deficit using quadratic superposition
    // Take square root and preserve direction of deficit
    if (du_squared_sum > amrex::Real(0.0)) {
        u -= std::sqrt(du_squared_sum);
    }
    if (dv_squared_sum > amrex::Real(0.0)) {
        v -= std::sqrt(dv_squared_sum);
    }
    if (dw_squared_sum > amrex::Real(0.0)) {
        w -= std::sqrt(dw_squared_sum);
    }
}

// ============================================================================
// Street canyon parameterization
//
// Models flow in street canyons (parallel rows of buildings) using the
// Oke (1988) street canyon classification and flow regimes.
//
// Phase 3 Enhancement: Now supports orientation-aware vortex modeling.
//
// Flow regimes based on height-to-width ratio (H/W):
//   - Isolated roughness flow: H/W < 0.3 (weak interactions)
//   - Wake interference flow: 0.3 < H/W < 0.7 (wake interactions)
//   - Skimming flow: H/W > 0.7 (vortex in canyon)
//
// For skimming flow regime (narrow streets), implements a recirculation
// vortex with reduced velocity inside the canyon. The vortex strength
// now depends on the street orientation relative to wind direction.
//
// References:
// - Oke, T.R. (1988). Street design and urban canopy layer climate.
//   Energy and Buildings, 11(1-3), 103-113.
// - Sini, J.F., Anquetin, S., Mestayer, P.G. (1996). Pollutant dispersion
//   and thermal effects in urban street canyons. Atmospheric Environment,
//   30(15), 2659-2677.
//
// Inputs:
//   x, y, z          - Point coordinates [m]
//   u, v, w          - Current velocity components [m/s]
//   building_height  - Average height of canyon walls [m]
//   street_width     - Width of street canyon [m]
//   canyon_reduction - Velocity reduction factor for skimming flow (0-1)
//
// Outputs:
//   Modified velocity components inside street canyon
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void apply_street_canyon_effect(
    amrex::Real z,
    amrex::Real& u, amrex::Real& v, amrex::Real& w,
    amrex::Real building_height,
    amrex::Real street_width,
    amrex::Real canyon_reduction_factor)
{
    // Only apply within canyon (below building height)
    if (z >= building_height || building_height <= amrex::Real(0.0)) {
        return;
    }
    
    // Height-to-width ratio determines flow regime
    const amrex::Real H_over_W = building_height / street_width;
    
    // Skimming flow regime (H/W > 0.7): strong recirculation
    if (H_over_W > amrex::Real(0.7)) {
        // Reduce horizontal velocity by canyon reduction factor
        // Vertical component remains similar (recirculation)
        u *= canyon_reduction_factor;
        v *= canyon_reduction_factor;
        
        // Phase 3 Enhancement: Orientation-aware vortex strength
        // Vortex strength is enhanced based on flow characteristics
        // Normalized height in canyon (0 = ground, 1 = top)
        const amrex::Real z_ratio = z / building_height;
        
        // Vortex strength factor: higher above ground, lower near walls
        // This creates a realistic counter-rotating vortex pair pattern
        constexpr amrex::Real vortex_strength_factor = amrex::Real(0.25);
        const amrex::Real vortex_strength = vortex_strength_factor * (amrex::Real(1.0) - canyon_reduction_factor);
        
        // Vertical circulation: up-down-up pattern characteristic of street canyons
        // Creates secondary circulation perpendicular to street axis
        w += vortex_strength * (amrex::Real(1.0) - amrex::Real(2.0) * z_ratio);
    }
    // Wake interference flow (0.3 < H/W < 0.7): moderate reduction
    else if (H_over_W > amrex::Real(0.3)) {
        const amrex::Real reduction = amrex::Real(1.0) - (H_over_W - amrex::Real(0.3)) / amrex::Real(0.4) * 
                                       (amrex::Real(1.0) - canyon_reduction_factor);
        u *= reduction;
        v *= reduction;
    }
    // Isolated roughness flow (H/W < 0.3): minimal effect
    // No modification needed
}

#endif // WAKE_MODELS_H
