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
#include "solver_math_constants.H"

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
    
    // Enhancement flags
    bool enable_oblique_scaling = true;      // Oblique angle cavity scaling
    bool enable_tall_building_correction = true;  // Tall-building aspect-ratio correction
    bool enable_gaussian_profile = false;    // Gaussian lateral wake profile option
    bool enable_upwind_recirculation = true; // Upwind recirculation zone modeling
    bool enable_reference_correction = false; // Log-law reference velocity extraction
    bool enable_corner_acceleration = true;  // Corner and side acceleration effects
    bool enable_variance_correction = false;  // Height-dependent velocity variance
    bool enable_horseshoe_vortex = true;    // Horseshoe vortex at building base
    bool enable_extended_farwake = true;     // Far-wake extension to 15 building heights
    bool enable_yoshie_two_layer = true;     // Two-layer height-dependent deficit model
    bool enable_rodi_entrainment = true;     // Rodi entrainment-based far-wake decay model
    bool enable_lopes_comfort = true;        // Lopes pedestrian wind comfort assessment
    bool enable_oikonomou_aspect = true;     // Oikonomou aspect-ratio dependent cavity correction
    bool enable_britter_hanna_urban = true;  // Britter-Hanna urban canyon effects model
    
    // Additional parameters for enhancements
    amrex::Real z_ref = amrex::Real(10.0);   // Reference height for log-law (m)
    amrex::Real z0 = amrex::Real(0.1);       // Roughness length (m)
    amrex::Real yoshie_decay_beta = amrex::Real(1.75);  // Decay coefficient for above-roof deficit (1.5-2.0)
    amrex::Real rodi_ce_coefficient = amrex::Real(1.0);  // Entrainment coefficient (0.5-1.5)
    amrex::Real lopes_comfort_threshold = amrex::Real(5.0);  // Discomfort velocity threshold [m/s]
    amrex::Real lopes_assessment_height = amrex::Real(1.5);  // Assessment height [m AGL]
    amrex::Real lopes_reference_frequency = amrex::Real(0.02);  // Reference discomfort frequency for diagnostics
    amrex::Real oikonomou_beta_aspect = amrex::Real(0.25);  // Aspect-ratio correction coefficient
    amrex::Real britter_hanna_alpha = amrex::Real(0.15);  // Urban canyon attenuation coefficient
};

// ============================================================================
// Building shape enumeration
// ============================================================================
enum class BuildingShape : int {
    RECTANGULAR = 0,
    CYLINDRICAL = 1,
    PITCHED_ROOF = 2
};

// ============================================================================
// Building geometry type enumeration (for polygon support)
// ============================================================================
enum class BuildingGeometryType : int {
    RECTANGULAR_BLOCK = 0,  // Standard rectangular building
    POLYGON = 1,            // Arbitrary polygon footprint
    VOID = 2                // Void zone (internal courtyard/exclusion)
};

// ============================================================================
// Building structure (extended for optional polygon support)
// ============================================================================
struct Building {
    amrex::Real xmin, xmax;  // Building bounds [m] (for rectangular only)
    amrex::Real ymin, ymax;
    amrex::Real zmin, zmax;
    amrex::Real height;      // Effective height H [m]
    amrex::Real width;       // Effective width W [m]
    amrex::Real length;      // Effective length L [m]
    amrex::Real rotation;    // Building orientation angle (radians, counter-clockwise from x-axis)
    BuildingShape shape = BuildingShape::RECTANGULAR;
    amrex::Real pitch_or_radius = amrex::Real(0.0); // Cylinder radius [m] or roof pitch angle [degrees]
    amrex::Real pitch_direction = amrex::Real(0.0); // Direction of pitch/ridge (radians, CCW from x-axis)
    
    // Optional: Polygon geometry support (for arbitrary building footprints, L/T/U shapes, courtyards)
    BuildingGeometryType geom_type = BuildingGeometryType::RECTANGULAR_BLOCK;
    std::vector<amrex::Real> polygon_x;  // Polygon vertex x-coordinates [m] (only populated if POLYGON)
    std::vector<amrex::Real> polygon_y;  // Polygon vertex y-coordinates [m] (only populated if POLYGON)
    bool is_void = false;                 // If true, this is a void zone (internal courtyard/exclusion)
    int parent_building_id = -1;          // Optional: reference to parent building (for grouping)
};

// ============================================================================
// Rooftop vortex parameterization
//
// Enhancement: Computes velocity components in the cavity zone due to
// rooftop vortex formation. The vortex arises from flow separation at the
// building edges and creates a counterclockwise (or clockwise, depending on
// wind direction) circulation in the cavity zone.
//
// The vortex circulation has both:
// 1. VERTICAL component: Upward motion on windward side, downward on leeward
// 2. HORIZONTAL component: Crosswind circulation that enhances reverse flow
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
// - Kaplan, H., & Dinar, N. (1996). A Lagrangian dispersion model for
//   calculating concentration distribution within a built-up domain.
//
// Inputs:
//   x_wake        - Downwind distance from building back face [m]
//   z_local       - Height above building base [m]
//   H             - Building height [m]
//   Hr            - Cavity zone height (0.67*H) [m]
//   Lr            - Cavity zone length (c1*H) [m]
//   U_ref         - Reference wind speed magnitude [m/s]
//   y_wind        - Crosswind distance from building center [m]
//   W             - Building width (crosswind dimension) [m]
//   wind_dir_x    - Wind direction x-component (unit vector)
//   wind_dir_y    - Wind direction y-component (unit vector)
//
// Outputs:
//   du            - Horizontal velocity component (downwind) [m/s]
//   dv            - Horizontal velocity component (crosswind) [m/s]
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
    amrex::Real y_wind,
    amrex::Real W,
    amrex::Real wind_dir_x,
    amrex::Real wind_dir_y,
    amrex::Real&  du,
    amrex::Real&  dv,
    amrex::Real&  dw)
{
    du = amrex::Real(0.0);
    dv = amrex::Real(0.0);
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
    // Scales with wind speed to create realistic circulation
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
    
    // VERTICAL COMPONENT: Upward on one side, downward on other
    // Sign depends on wind direction relative to building orientation
    dw = vortex_circulation * z_profile;
    
    // HORIZONTAL COMPONENT: Contribute to reverse flow
    // The vortex circulation creates a crosswind velocity component that
    // enhances the recirculation pattern and creates horizontal flow reversal
    const amrex::Real y_ratio = y_wind / (W / amrex::Real(2.0));
    
    // Horizontal vortex contribution (strongest at building centerline)
    // Creates crosswind flow that drives recirculation
    const amrex::Real vortex_crosswind = vortex_strength_coeff * U_ref * 
                                         std::abs(y_ratio) * (amrex::Real(1.0) - std::abs(y_ratio));
    
    // Project horizontal vortex component onto building-relative axes
    // This adds to the reverse flow enhancement
    const amrex::Real vortex_streamwise = amrex::Real(0.1) * vortex_circulation * z_profile;
    
    // Add horizontal components to create enhanced recirculation
    du += vortex_streamwise * wind_dir_x;
    dv += vortex_crosswind * wind_dir_y;
}


// ============================================================================
// Oblique angle cavity scaling
//
// Scales cavity length based on wind approach angle relative to building normal.
// Formula: Lr(θ) = Lr₀ × cos(θ)
// where θ is angle from building normal.
//
// Inputs:
//   Lr0            - Base cavity length [m]
//   wind_dir_x, wind_dir_y - Wind direction (unit vector)
//   bldg_normal_x, bldg_normal_y - Building normal (unit vector)
//
// Returns:
//   Angle-adjusted cavity length [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_oblique_cavity_scaling(
    amrex::Real Lr0,
    amrex::Real wind_dir_x, amrex::Real wind_dir_y,
    amrex::Real bldg_normal_x, amrex::Real bldg_normal_y)
{
    // Compute dot product to get cos(θ)
    amrex::Real cos_theta = std::abs(wind_dir_x * bldg_normal_x + wind_dir_y * bldg_normal_y);
    // Ensure minimum scaling to avoid zero cavity
    cos_theta = std::max(cos_theta, amrex::Real(0.3));
    return Lr0 * cos_theta;
}


// ============================================================================
// Tall-building aspect-ratio correction
//
// Applies aspect-ratio dependent correction to cavity length.
// Formula: Lr = 0.9H × max(1.0, min(W/H, 1.5))
// where H = building height, W = building width (crosswind).
//
// Inputs:
//   H              - Building height [m]
//   W              - Building width (crosswind) [m]
//
// Returns:
//   Aspect-ratio corrected cavity length [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_tall_building_correction(amrex::Real H, amrex::Real W)
{
    // Aspect ratio scaling
    amrex::Real aspect_ratio = W / H;
    aspect_ratio = std::max(amrex::Real(1.0), std::min(aspect_ratio, amrex::Real(1.5)));
    return amrex::Real(0.9) * H * aspect_ratio;
}


// ============================================================================
// Gaussian lateral wake profile
//
// Computes Gaussian-profile deficit instead of linear profile.
// Provides smoother, more physically realistic deficit distribution.
//
// Inputs:
//   y_wind         - Crosswind distance from wake center [m]
//   Wr             - Reference wake width [m]
//   deficit_max    - Maximum deficit magnitude at center [m/s]
//
// Returns:
//   Gaussian-weighted deficit magnitude [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_gaussian_deficit(
    amrex::Real y_wind,
    amrex::Real Wr,
    amrex::Real deficit_max)
{
    // Gaussian profile: exp(-(y/σ)²) where σ = Wr/2
    const amrex::Real sigma = Wr / amrex::Real(2.0);
    const amrex::Real y_normalized = y_wind / sigma;
    return deficit_max * std::exp(-y_normalized * y_normalized);
}


// ============================================================================
// Upwind recirculation zone
//
// Adds reverse flow region upstream of building (~0.5×min(H,W) upstream).
// Models flow diversion and stagnation effects.
//
// Inputs:
//   x_wake         - Distance from building (negative if upstream) [m]
//   H              - Building height [m]
//   W              - Building width (crosswind) [m]
//   U_ref          - Reference wind speed [m/s]
//   z_local        - Height above ground [m]
//
// Returns:
//   Upwind recirculation deficit [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_upwind_recirculation(
    amrex::Real x_wake,
    amrex::Real H,
    amrex::Real W,
    amrex::Real U_ref,
    amrex::Real z_local)
{
    // Recirculation zone extends ~0.5×min(H,W) upstream
    const amrex::Real recirculation_length = amrex::Real(0.5) * std::min(H, W);
    
    // Only applies upstream (x_wake < 0)
    if (x_wake >= amrex::Real(0.0)) {
        return amrex::Real(0.0);
    }
    
    // Decay with distance from building
    const amrex::Real dist_factor = amrex::Real(1.0) + x_wake / recirculation_length;
    if (dist_factor <= amrex::Real(0.0)) {
        return amrex::Real(0.0);  // Beyond recirculation zone
    }
    
    // Vertical profile: stronger near ground, weaker aloft
    const amrex::Real z_profile = std::max(amrex::Real(0.0), 
        amrex::Real(1.0) - (z_local / H) * (z_local / H));
    
    return -amrex::Real(0.1) * U_ref * dist_factor * z_profile;
}


// ============================================================================
// Log-law reference velocity extraction
//
// Extracts reference velocity from log-law profile rather than local value.
// Provides physically consistent wind profile.
//
// Inputs:
//   z              - Height above ground [m]
//   z_ref          - Reference height [m]
//   U_ref          - Reference wind speed [m/s]
//   z0             - Roughness length [m]
//
// Returns:
//   Log-law corrected velocity at height z [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_loglaw_velocity(
    amrex::Real z,
    amrex::Real z_ref,
    amrex::Real U_ref,
    amrex::Real z0)
{
    // Avoid division by zero and log of zero
    const amrex::Real z_safe = std::max(z, z0 * amrex::Real(1.01));
    const amrex::Real z_ref_safe = std::max(z_ref, z0 * amrex::Real(1.01));
    
    // Log-law wind profile: U(z) = U_ref × ln(z/z0) / ln(z_ref/z0)
    const amrex::Real log_ratio = std::log(z_safe / z0) / std::log(z_ref_safe / z0);
    return U_ref * log_ratio;
}


// ============================================================================
// Corner and side acceleration effects
//
// Computes velocity amplification at building corners and sides.
// Models flow acceleration around sharp edges.
//
// Inputs:
//   y_wind         - Crosswind distance from building center [m]
//   W              - Building width (crosswind) [m]
//   H              - Building height [m]
//   z_local        - Height above ground [m]
//   U_deficit      - Base velocity deficit [m/s]
//
// Returns:
//   Corner acceleration factor (multiplier on deficit)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_corner_acceleration(
    amrex::Real y_wind,
    amrex::Real W,
    amrex::Real H,
    amrex::Real z_local,
    amrex::Real U_deficit)
{
    // Distance from building side (0 at edge, increases away)
    const amrex::Real dist_from_edge = std::abs(y_wind) - W / amrex::Real(2.0);
    
    // Only accelerate near corners (within 0.5×W)
    if (dist_from_edge > amrex::Real(0.5) * W) {
        return amrex::Real(1.0);
    }
    
    // Corner vortex strength peaks near building height
    const amrex::Real height_factor = amrex::Real(1.0) - std::abs(z_local - amrex::Real(0.5) * H) / H;
    const amrex::Real height_profile = std::max(amrex::Real(0.0), height_factor * height_factor);
    
    // Acceleration factor increases near corners
    return amrex::Real(1.0) + amrex::Real(0.2) * height_profile;
}


// ============================================================================
// Height-dependent velocity variance correction
//
// Modifies velocity variance profile to account for building effects.
// Reduces variance in cavity, increases in shear layer.
//
// Inputs:
//   z_local        - Height above ground [m]
//   Hr             - Cavity height [m]
//   z_cavity_ratio - z_local / Hr (normalized height in cavity)
//
// Returns:
//   Variance correction factor (multiplier on ambient variance)
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_variance_correction(
    amrex::Real z_local,
    amrex::Real Hr,
    amrex::Real z_cavity_ratio)
{
    // Inside cavity: reduced variance due to recirculation
    if (z_cavity_ratio < amrex::Real(1.0)) {
        // Minimum variance (0.5×) at center height, increases toward edges
        const amrex::Real cavity_profile = amrex::Real(0.5) + 
            amrex::Real(0.5) * std::abs(z_cavity_ratio - amrex::Real(0.5)) * amrex::Real(2.0);
        return cavity_profile;
    }
    
    // Just above cavity: enhanced variance due to shear layer
    const amrex::Real above_cavity = std::min(amrex::Real(1.5) * Hr - z_local, Hr);
    if (above_cavity > amrex::Real(0.0)) {
        // Peak variance at top of cavity
        return amrex::Real(1.5);
    }
    
    // Far above: return to ambient
    return amrex::Real(1.0);
}


// ============================================================================
// Horseshoe vortex modeling
//
// Computes velocity perturbations from horseshoe vortex at building base.
// Models circulation at the junction between building and ground.
//
// Inputs:
//   x_wake         - Downwind distance from building [m]
//   y_wind         - Crosswind distance from building center [m]
//   z_local        - Height above ground [m]
//   H              - Building height [m]
//   W              - Building width (crosswind) [m]
//   U_ref          - Reference wind speed [m/s]
//
// Outputs:
//   du_horseshoe   - Downwind component [m/s]
//   dv_horseshoe   - Crosswind component [m/s]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_horseshoe_vortex(
    amrex::Real x_wake,
    amrex::Real y_wind,
    amrex::Real z_local,
    amrex::Real H,
    amrex::Real W,
    amrex::Real U_ref,
    amrex::Real& du_horseshoe,
    amrex::Real& dv_horseshoe)
{
    du_horseshoe = amrex::Real(0.0);
    dv_horseshoe = amrex::Real(0.0);
    
    // Horseshoe vortex exists near ground and upstream face
    const amrex::Real vortex_height = amrex::Real(0.2) * H;  // Near-ground region
    if (z_local > vortex_height) {
        return;  // Vortex only exists near ground
    }
    
    // Upwind face region (x_wake ~-0.3 to 0)
    const amrex::Real upwind_zone = amrex::Real(0.3) * W;
    if (x_wake < -upwind_zone || x_wake > amrex::Real(0.0)) {
        return;  // Outside horseshoe zone
    }
    
    // Crosswind legs create lateral velocity
    const amrex::Real y_half_width = W / amrex::Real(2.0);
    const amrex::Real y_normalized = std::abs(y_wind) / y_half_width;
    
    if (y_normalized < amrex::Real(1.2)) {
        // Vortex strength peaks at building corners
        const amrex::Real corner_factor = std::exp(-std::abs(y_normalized - amrex::Real(1.0)) * 
            std::abs(y_normalized - amrex::Real(1.0)));
        
        // Height decay toward ground
        const amrex::Real z_factor = z_local / vortex_height;
        
        // Crosswind circulation (lateral acceleration toward center from corners)
        dv_horseshoe = -amrex::Real(0.15) * U_ref * corner_factor * (amrex::Real(1.0) - z_factor);
        if (y_wind < amrex::Real(0.0)) dv_horseshoe = -dv_horseshoe;  // Opposite sign
    }
}


// ============================================================================
// Far-wake extension to 15 building heights
//
// Computes far-wake zone up to 15 building heights downstream.
// Extends influence zone beyond typical 3H limitation.
//
// Inputs:
//   x_wake         - Distance from building back face [m]
//   H              - Building height [m]
//
// Returns:
//   Far-wake extent in building heights
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_extended_farwake_extent(amrex::Real x_wake, amrex::Real H)
{
    // Extended far-wake: up to 15H (vs. typical 3-5H)
    return x_wake / H;
}



// Optional support for arbitrary polygon building footprints, internal courtyards,
// and complex shapes (L-shaped, T-shaped, U-shaped, etc.).
// This enables more accurate urban modeling beyond simple rectangular buildings.
//
// Key capabilities:
// - Point-in-polygon testing for cavity exclusion
// - Effective dimension computation for wind-aligned analysis
// - Courtyard/void zone support (internal wake exclusion)
// - Works with all three wake models (Röckle, Huber-Snyder, AERMOD PRIME)
//
// Format in buildings.csv:
//   POLYGON: x1 y1 x2 y2 x3 y3 ... xn yn | zmin zmax [height]
//   VOID: x1 y1 x2 y2 ... | zmin zmax  (for internal courtyards)
//
// Backward compatibility: Existing rectangular building format still works
// ============================================================================

// ============================================================================
// Point-in-polygon test using ray-casting algorithm
//
// Determines if a 2D point is inside a polygon using the ray-casting method.
// Casts a ray from the point to infinity and counts polygon edge crossings.
// Odd number of crossings = inside polygon.
//
// Inputs:
//   px, py           - Query point coordinates [m]
//   vx, vy           - Arrays of polygon vertex coordinates [m]
//   n_vertices       - Number of polygon vertices
//
// Returns:
//   true if point is inside polygon, false otherwise
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool point_in_polygon(
    amrex::Real px, amrex::Real py,
    const amrex::Real* vx, const amrex::Real* vy,
    int n_vertices)
{
    if (n_vertices < 3) return false;  // Need at least 3 vertices for a polygon
    
    bool inside = false;
    for (int i = 0, j = n_vertices - 1; i < n_vertices; j = i++) {
        const amrex::Real xi = vx[i], yi = vy[i];
        const amrex::Real xj = vx[j], yj = vy[j];
        
        if ((yi > py) != (yj > py) &&
            px < (xj - xi) * (py - yi) / (yj - yi) + xi) {
            inside = !inside;
        }
    }
    return inside;
}

// ============================================================================
// Compute polygon centroid (center of mass)
//
// Uses the shoelace formula to compute the area-weighted centroid of a polygon.
// Inputs:
//   vx, vy           - Polygon vertex coordinates [m]
//   n_vertices       - Number of vertices
//
// Outputs:
//   centroid_x, centroid_y - Coordinates of centroid [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_polygon_centroid(
    const amrex::Real* vx, const amrex::Real* vy,
    int n_vertices,
    amrex::Real& centroid_x, amrex::Real& centroid_y)
{
    amrex::Real area = amrex::Real(0.0);
    centroid_x = amrex::Real(0.0);
    centroid_y = amrex::Real(0.0);
    
    for (int i = 0; i < n_vertices; i++) {
        const int j = (i + 1) % n_vertices;
        const amrex::Real cross = vx[i] * vy[j] - vx[j] * vy[i];
        
        area += cross;
        centroid_x += (vx[i] + vx[j]) * cross;
        centroid_y += (vy[i] + vy[j]) * cross;
    }
    
    if (std::abs(area) > amrex::Real(1.0e-30)) {
        area = area / amrex::Real(2.0);
        centroid_x = centroid_x / (amrex::Real(6.0) * area);
        centroid_y = centroid_y / (amrex::Real(6.0) * area);
    } else {
        // Degenerate polygon, use simple average
        for (int i = 0; i < n_vertices; i++) {
            centroid_x += vx[i];
            centroid_y += vy[i];
        }
        centroid_x /= n_vertices;
        centroid_y /= n_vertices;
    }
}

// ============================================================================
// Find the most downwind point of a polygon
//
// Projects all polygon vertices onto wind direction and finds the maximum
// projection (most downwind point). This determines where the building wake begins.
//
// Inputs:
//   vx, vy           - Polygon vertex coordinates [m]
//   n_vertices       - Number of vertices
//   wind_dir_x, wind_dir_y - Unit vector in wind direction
//
// Returns:
//   Downwind coordinate of the most downwind vertex [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real find_polygon_wake_start(
    const amrex::Real* vx, const amrex::Real* vy,
    int n_vertices,
    amrex::Real wind_dir_x, amrex::Real wind_dir_y)
{
    amrex::Real max_proj = -1.0e30;
    for (int i = 0; i < n_vertices; i++) {
        amrex::Real proj = vx[i] * wind_dir_x + vy[i] * wind_dir_y;
        max_proj = std::max(max_proj, proj);
    }
    return max_proj;
}

// ============================================================================
// Compute polygon width perpendicular to wind at a given downwind position
//
// For a polygon building, the width (crosswind extent) can vary with downwind
// distance, especially for non-rectangular shapes (L, T, U shapes, etc.).
// This computes the maximum crosswind extent at a specified downwind location.
//
// Inputs:
//   vx, vy           - Polygon vertex coordinates [m]
//   n_vertices       - Number of vertices
//   x_wind_query     - Downwind coordinate to query [m]
//   wind_dir_x, wind_dir_y - Unit vector in wind direction
//
// Returns:
//   Crosswind extent (width) at query position [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real get_polygon_width_at_distance(
    const amrex::Real* vx, const amrex::Real* vy,
    int n_vertices,
    amrex::Real x_wind_query,
    amrex::Real wind_dir_x, amrex::Real wind_dir_y)
{
    // For simplicity, compute bounding box width in crosswind direction
    // More sophisticated approaches would interpolate width at specific x_wind
    amrex::Real min_y = 1.0e30, max_y = -1.0e30;
    
    for (int i = 0; i < n_vertices; i++) {
        // Transform to wind-aligned coordinates
        amrex::Real y_wind = -vx[i] * wind_dir_y + vy[i] * wind_dir_x;
        min_y = std::min(min_y, y_wind);
        max_y = std::max(max_y, y_wind);
    }
    
    amrex::Real width = max_y - min_y;
    return std::max(width, amrex::Real(1.0e-3));  // Avoid zero width
}

// ============================================================================
// Compute effective dimensions for polygon (wind-aligned analysis)
//
// For wind-dependent wake models, compute the effective streamwise (length)
// and crosswind (width) extents of a polygon building.
//
// Inputs:
//   vx, vy           - Polygon vertex coordinates [m]
//   n_vertices       - Number of vertices
//   wind_dir_x, wind_dir_y - Unit vector in wind direction
//   H                - Building height [m]
//
// Outputs:
//   L_eff            - Effective length (streamwise extent) [m]
//   W_eff            - Effective width (crosswind extent) [m]
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
void compute_polygon_effective_dimensions(
    const amrex::Real* vx, const amrex::Real* vy,
    int n_vertices,
    amrex::Real wind_dir_x, amrex::Real wind_dir_y,
    amrex::Real& L_eff, amrex::Real& W_eff)
{
    if (n_vertices < 3) {
        L_eff = amrex::Real(10.0);
        W_eff = amrex::Real(10.0);
        return;
    }
    
    amrex::Real min_x = 1.0e30, max_x = -1.0e30;
    amrex::Real min_y = 1.0e30, max_y = -1.0e30;
    
    for (int i = 0; i < n_vertices; i++) {
        // Project onto wind direction (streamwise)
        amrex::Real x_wind = vx[i] * wind_dir_x + vy[i] * wind_dir_y;
        min_x = std::min(min_x, x_wind);
        max_x = std::max(max_x, x_wind);
        
        // Project perpendicular to wind (crosswind)
        amrex::Real y_wind = -vx[i] * wind_dir_y + vy[i] * wind_dir_x;
        min_y = std::min(min_y, y_wind);
        max_y = std::max(max_y, y_wind);
    }
    
    L_eff = std::max(max_x - min_x, amrex::Real(1.0e-3));
    W_eff = std::max(max_y - min_y, amrex::Real(1.0e-3));
}

// ============================================================================
// Yoshie Two-Layer Height-Dependent Deficit Model
//
// Implements the Yoshie et al. (2007) two-layer wake deficit model which
// separates the cavity zone (z < H) from the above-roof zone (z >= H).
// This model improves prediction of wind speed recovery above building height.
//
// In the cavity zone (z < H), the deficit is unchanged from the standard model.
// In the above-roof zone (z >= H), the deficit decays exponentially with height:
//
//    ΔU(z) = ΔU_cavity * exp(-β * (z - H) / H)
//
// where:
//    ΔU_cavity = deficit at building height (cavity edge)
//    β = decay coefficient (default 1.75, range 1.5-2.0)
//    z = height above ground
//    H = building height
//
// References:
//    Yoshie et al. (2007). "Cooperative project on CFD prediction of
//    pedestrian wind environment in the Tokyo Bay area: mean wind and
//    turbulent quantities." Journal of Wind Engineering and Industrial
//    Aerodynamics, 95(1), 1-21.
//
// Inputs:
//    deficit_magnitude - Deficit value at cavity edge (magnitude)
//    z_local          - Height above building base [m]
//    H                - Building height [m]
//    beta             - Decay coefficient (1.5-2.0)
//
// Output:
//    Modified deficit magnitude accounting for height-dependent decay
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_yoshie_two_layer_deficit(
    amrex::Real deficit_magnitude,
    amrex::Real z_local,
    amrex::Real H,
    amrex::Real beta)
{
    // Below building height: return unchanged deficit
    if (z_local <= H) {
        return deficit_magnitude;
    }
    
    // Above building height: apply exponential decay
    const amrex::Real z_normalized = (z_local - H) / H;
    const amrex::Real decay_factor = std::exp(-beta * z_normalized);
    
    return deficit_magnitude * decay_factor;
}

// ============================================================================
// Oikonomou Aspect-Ratio Dependent Cavity Correction (Oikonomou et al., 2011)
//
// Refines cavity zone length and deficit based on building aspect ratio (L/W).
// Accounts for flow separation patterns that vary significantly with building
// elongation, improving predictions for non-cubic buildings.
//
// Cavity length correction:
//   L_r,corrected = L_r,base × f_aspect(L/W)
// where:
//   f_aspect(α) = 1.0 + β × (α - 1.0) / (α_ref - 1.0) for α > 1.0
//   with α = L/W (downwind length / crosswind width), β ≈ 0.25
//
// The correction applies when L/W > 1 (elongated buildings):
//   - Square buildings (L/W ≈ 1.0): no correction
//   - Moderately elongated (L/W ≈ 2): +5-10% cavity length increase
//   - Highly elongated (L/W ≈ 4): +15-20% cavity length increase
//
// Inputs:
//   length_downwind - Building dimension along wind direction [m]
//   width_crosswind - Building dimension perpendicular to wind [m]
//   beta_aspect     - Aspect ratio correction coefficient (≈0.25)
//
// Returns:
//   Aspect-ratio correction factor (≥1.0), multiplied by base cavity length
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_oikonomou_aspect_ratio_factor(
    const amrex::Real length_downwind,
    const amrex::Real width_crosswind,
    const amrex::Real beta_aspect)
{
    // Avoid division by zero
    if (width_crosswind <= amrex::Real(0.0) || length_downwind <= amrex::Real(0.0)) {
        return amrex::Real(1.0);
    }
    
    // Aspect ratio: L/W (downwind / crosswind)
    const amrex::Real aspect_ratio = length_downwind / width_crosswind;
    
    // No correction for square or near-square buildings (L/W ≤ 1)
    if (aspect_ratio <= amrex::Real(1.0)) {
        return amrex::Real(1.0);
    }
    
    // Reference aspect ratio for normalization
    const amrex::Real aspect_ref = amrex::Real(4.0);
    
    // Aspect-ratio correction factor
    // Increases linearly from 1.0 at L/W=1 to (1+β) at L/W=aspect_ref
    const amrex::Real correction_factor = amrex::Real(1.0) + 
        beta_aspect * (aspect_ratio - amrex::Real(1.0)) / (aspect_ref - amrex::Real(1.0));
    
    // Clamp to reasonable range [1.0, 1.5]
    return amrex::max(amrex::Real(1.0), amrex::min(correction_factor, amrex::Real(1.5)));
}

// ============================================================================
// Britter-Hanna Urban Canyon Effects (Britter and Hanna, 2003)
//
// Models wind speed attenuation in dense urban environments accounting for
// inter-building interactions, street canyon flow, and urban boundary layer
// modification. Applies reduction factor to wind speed based on building
// density and urban canyon geometry.
//
// Street canyon wind speed reduction:
//   U_canyon = U_ref × exp(-α_urban × φ_v)
// where:
//   φ_v = frontal area index = (building height × building projection area) / (area of interest)
//   α_urban = urban canyon attenuation coefficient (typically 0.1-0.3)
//
// Physically represents:
//   - Flow deceleration in urban streets due to building drag
//   - Reduced vertical mixing in canyon environments
//   - Enhanced surface roughness effects in urban areas
//
// Inputs:
//   frontal_area_index - Building frontal area density φ_v [dimensionless]
//   alpha_urban        - Canyon attenuation coefficient (default 0.15)
//
// Returns:
//   Wind speed reduction factor (0-1), multiplied by reference velocity
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_britter_hanna_urban_reduction(
    const amrex::Real frontal_area_index,
    const amrex::Real alpha_urban)
{
    // Clamp frontal area index to physically reasonable range [0, 1]
    const amrex::Real phi_v = amrex::max(amrex::Real(0.0), 
                              amrex::min(frontal_area_index, amrex::Real(1.0)));
    
    // Exponential decay of wind speed with urban density
    // At φ_v = 0 (isolated buildings): U_canyon = U_ref (no reduction)
    // At φ_v = 0.5 (moderate density): U_canyon ≈ 0.93 × U_ref (7% reduction)
    // At φ_v = 1.0 (high density):    U_canyon ≈ 0.86 × U_ref (14% reduction)
    const amrex::Real reduction_factor = std::exp(-alpha_urban * phi_v);
    
    return reduction_factor;
}

// ============================================================================
// Rodi Entrainment Model (Rodi et al., 2003)
//
// Enhances far-wake deficit decay through entrainment-based momentum mixing.
// Modifies the linear far-wake deficit decay to include entrainment effects:
//   ΔU_far = ΔU_cavity × (1 - Ce × x_norm^2)
// where x_norm = (x - x_cavity_end) / (L_f - L_r) is normalized distance in far-wake,
// and Ce is the entrainment coefficient (default 1.0, range [0.5, 1.5]).
//
// When Ce > 0 (entrainment enabled), deficit decays more gradually due to
// ambient fluid entrainment into the wake. Higher Ce values increase entrainment
// strength and accelerate deficit recovery. Ce ≈ 1.0 matches wind tunnel data
// from Rodi et al. (2003) field measurements.
//
// Inputs:
//   deficit_cavity - Cavity zone velocity deficit [m/s]
//   x_normalized   - Normalized far-wake distance (0 at cavity end, 1 at far-wake end)
//   ce_coeff       - Entrainment coefficient (0.0 = disabled, default 1.0)
//
// Returns:
//   Modified far-wake deficit accounting for entrainment
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_rodi_entrainment_deficit(
    const amrex::Real deficit_cavity,
    const amrex::Real x_normalized,
    const amrex::Real ce_coeff)
{
    // Entrainment disabled if ce_coeff <= 0
    if (ce_coeff <= amrex::Real(0.0)) {
        return deficit_cavity;
    }
    
    // Normalize x_normalized to [0, 1] range for safety
    amrex::Real x_norm = amrex::max(amrex::Real(0.0), amrex::min(x_normalized, amrex::Real(1.0)));
    
    // Quadratic entrainment decay: ΔU = ΔU_cavity × (1 - Ce × x_norm^2)
    // At x_norm = 0 (cavity end):     ΔU = ΔU_cavity (full deficit)
    // At x_norm = 1 (far-wake end):   ΔU = ΔU_cavity × (1 - Ce)
    // For Ce = 1.0: deficit reduces to 0 at far-wake end
    // For Ce < 1.0: faster recovery (reduced entrainment)
    // For Ce > 1.0: slower recovery (enhanced entrainment)
    
    amrex::Real x_norm_sq = x_norm * x_norm;
    amrex::Real decay_factor = amrex::Real(1.0) - ce_coeff * x_norm_sq;
    
    // Clamp to ensure deficit doesn't exceed cavity value and doesn't go negative
    decay_factor = amrex::max(amrex::Real(0.0), amrex::min(decay_factor, amrex::Real(1.0)));
    
    return deficit_cavity * decay_factor;
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
//   Vertical component: w = rooftop vortex circulation
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
    
    // Height above ground (used for log-law correction)
    const amrex::Real z_local = z - bldg.zmin;
    
    // Apply reference velocity correction from log-law profile if enabled
    amrex::Real U_ref_corrected = U_ref;
    if (params.enable_reference_correction && z_local > amrex::Real(0.0)) {
        U_ref_corrected = compute_loglaw_velocity(z_local, params.z_ref, U_ref, params.z0);
    }
    
    // Shape-specific parameters
    amrex::Real shape_width = W;
    amrex::Real shape_half_len = bldg.length / amrex::Real(2.0);
    amrex::Real Lr = params.c1 * H;
    amrex::Real Hr = amrex::Real(0.67) * H;
    amrex::Real deficit_scale = amrex::Real(1.0);
    amrex::Real vortex_scale = amrex::Real(1.0);
    amrex::Real separation_scale = amrex::Real(1.0);

    if (bldg.shape == BuildingShape::CYLINDRICAL) {
        const amrex::Real radius = (bldg.pitch_or_radius > amrex::Real(0.0)) ? bldg.pitch_or_radius : amrex::Real(0.5) * W;
        shape_width = amrex::Real(2.0) * radius;
        shape_half_len = radius;
        
        // Check if point is inside building volume (cylinder)
        const amrex::Real dist_to_center = std::sqrt(dx * dx + dy * dy);
        if (dist_to_center <= radius && (z - bldg.zmin) <= H) {
            return false;
        }
        
        Lr = amrex::Real(0.5) * params.c1 * H;
        separation_scale = amrex::Real(0.7);
        deficit_scale = amrex::Real(0.7);
    } else if (bldg.shape == BuildingShape::PITCHED_ROOF) {
        const amrex::Real pitch_rad = (bldg.pitch_or_radius > amrex::Real(0.0)) ? bldg.pitch_or_radius : amrex::Real(25.0) * MathConstants::deg_to_rad;
        const amrex::Real pitch_deg = pitch_rad / MathConstants::deg_to_rad;
        const amrex::Real pitch_factor = std::max(amrex::Real(0.4), amrex::Real(1.0) - amrex::Real(0.01) * pitch_deg);
        
        Lr = params.c1 * H * pitch_factor;
        Hr = amrex::Real(0.67) * H * (amrex::Real(0.8) + amrex::Real(0.2) * std::cos(pitch_rad));
        separation_scale = pitch_factor;
        vortex_scale = amrex::Real(1.0) + amrex::Real(0.5) * std::sin(pitch_rad);
    }
    
    // Tall-building aspect-ratio correction
    if (params.enable_tall_building_correction) {
        Lr = compute_tall_building_correction(H, W);
    }
    
    // Oblique angle cavity scaling
    if (params.enable_oblique_scaling && bldg.shape == BuildingShape::RECTANGULAR) {
        // Building normal points in direction perpendicular to long axis
        const amrex::Real bldg_normal_x = -std::sin(bldg.rotation);
        const amrex::Real bldg_normal_y = std::cos(bldg.rotation);
        Lr = compute_oblique_cavity_scaling(Lr, wd_x, wd_y, bldg_normal_x, bldg_normal_y);
    }
    
    // Oikonomou aspect-ratio dependent cavity correction
    // Refines cavity length for elongated buildings based on L/W ratio
    if (params.enable_oikonomou_aspect && bldg.shape == BuildingShape::RECTANGULAR) {
        const amrex::Real length_downwind = amrex::Real(2.0) * shape_half_len;
        const amrex::Real aspect_factor = compute_oikonomou_aspect_ratio_factor(
            length_downwind, shape_width, params.oikonomou_beta_aspect);
        Lr *= aspect_factor;
    }

    // Distance relative to upwind face
    const amrex::Real x_upstream = x_wind + shape_half_len;
    const amrex::Real Wr = shape_width;               // Cavity width

    // ========================================================================
    // Upwind recirculation zone (before building)
    // ========================================================================
    if (params.enable_upwind_recirculation && x_upstream < amrex::Real(0.0)) {
        const amrex::Real recirculation_extent = amrex::Real(0.5) * std::min(H, W);
        if (x_upstream > -recirculation_extent) {
            // Within upwind recirculation zone
            if (std::abs(y_wind) <= Wr / amrex::Real(2.0)) {
                if (z_local >= amrex::Real(0.0) && z_local <= Hr) {
                    const amrex::Real du_upwind = compute_upwind_recirculation(
                        x_upstream, H, W, U_ref_corrected, z_local);
                    du = du_upwind * wd_x;  // Apply in wind direction
                    dv = du_upwind * wd_y;  // Apply in wind direction
                    return true;
                }
            }
        }
        return false;
    }

    // Wake starts at downwind face of building
    const amrex::Real wake_start = shape_half_len;
    
    // Check if point is downwind of building
    if (x_wind < wake_start) {
        return false;  // Upstream or inside building
    }
    
    // Distance downwind from building back face
    const amrex::Real x_wake = x_wind - wake_start;
    
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
        const amrex::Real deficit_magnitude = params.c2 * U_ref_corrected * deficit_scale;
        du = -deficit_magnitude * wd_x;  // Negate to create reverse flow
        dv = -deficit_magnitude * wd_y;  // Negate to create reverse flow
         
        // Add rooftop vortex with horizontal components
        amrex::Real dw_vortex = amrex::Real(0.0);
        amrex::Real du_vortex = amrex::Real(0.0);
        amrex::Real dv_vortex = amrex::Real(0.0);
        compute_rooftop_vortex(x_wake, z_local, H, Hr, Lr, U_ref_corrected, 
                               y_wind, Wr, wd_x, wd_y,
                               du_vortex, dv_vortex, dw_vortex);
        du += du_vortex * vortex_scale;
        dv += dv_vortex * vortex_scale;
        dw = dw_vortex * vortex_scale;
        
        // Horseshoe vortex at building base
        if (params.enable_horseshoe_vortex) {
            amrex::Real du_hs = amrex::Real(0.0);
            amrex::Real dv_hs = amrex::Real(0.0);
            compute_horseshoe_vortex(x_wake, y_wind, z_local, H, W, U_ref_corrected, du_hs, dv_hs);
            du += du_hs;
            dv += dv_hs;
        }
         
        return true;
    }
    
    // ========================================================================
    // 2. Far-wake zone (displacement zone)
    // ========================================================================
    // Far-wake extension to 15 building heights
    amrex::Real Lf = params.separation_length * H * separation_scale;
    if (params.enable_extended_farwake) {
        Lf = amrex::Real(15.0) * H;  // Extend to 15H instead of 3H
    }
    
    if (x_wake <= Lf) {
        // Compute far-wake decay factor with Rodi entrainment option
        amrex::Real x_normalized = (x_wake - Lr) / (Lf - Lr);  // [0, 1] in far-wake
        
        // Default linear decay
        amrex::Real decay_factor = amrex::Real(1.0) - x_normalized;
        
        // Apply Rodi entrainment model if enabled
        // Modifies decay to account for ambient fluid entrainment
        if (params.enable_rodi_entrainment) {
            decay_factor = compute_rodi_entrainment_deficit(
                amrex::Real(1.0), x_normalized, params.rodi_ce_coefficient);
        }
        
        // Linear lateral spreading of wake
        // Wake width increases from Wr to approximately 2*Wr at end of far-wake
        const amrex::Real spread_factor = amrex::Real(1.0) + x_normalized;
        amrex::Real Wf = Wr * spread_factor;
        
        // Gaussian lateral profile option
        amrex::Real lateral_deficit = decay_factor * deficit_scale;
        if (params.enable_gaussian_profile) {
            lateral_deficit = compute_gaussian_deficit(y_wind, Wf, 
                              params.c2 * U_ref_corrected * decay_factor * deficit_scale);
        } else {
            // Check lateral bounds for linear profile
            if (std::abs(y_wind) > Wf / amrex::Real(2.0)) {
                return false;  // Outside wake width
            }
        }
        
        // Wake height also increases with distance (simplified: use cavity height)
        const amrex::Real Hf = Hr * spread_factor;
        
        // Check vertical bounds
        if (z_local < amrex::Real(0.0) || z_local > Hf) {
            return false;  // Below ground or above wake height
        }
        
        // Inside far-wake zone: reduced velocity deficit
        // Deficit at cavity edge is c2 * U_ref, decays to zero at Lf
        amrex::Real deficit_magnitude = params.c2 * U_ref_corrected * lateral_deficit;
        
        // Apply Yoshie two-layer height correction if enabled
        if (params.enable_yoshie_two_layer) {
            deficit_magnitude = compute_yoshie_two_layer_deficit(
                deficit_magnitude, z_local, H, params.yoshie_decay_beta);
        }
        
        // Corner and side acceleration effects
        if (params.enable_corner_acceleration) {
            amrex::Real corner_factor = compute_corner_acceleration(
                y_wind, W, H, z_local, deficit_magnitude);
            deficit_magnitude *= corner_factor;
        }
        
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
    
    // Shape-specific parameters
    amrex::Real shape_width = W;
    amrex::Real shape_half_len = bldg.length / amrex::Real(2.0);
    const amrex::Real aspect_ratio = shape_width / H;
    amrex::Real Lc = amrex::Real(0.5) * H * std::sqrt(aspect_ratio);  // Cavity length
    amrex::Real Hc = amrex::Real(0.67) * H;  // Cavity height
    amrex::Real deficit_scale = amrex::Real(1.0);
    amrex::Real vortex_scale = amrex::Real(1.0);
    amrex::Real separation_scale = amrex::Real(1.0);

    if (bldg.shape == BuildingShape::CYLINDRICAL) {
        const amrex::Real radius = (bldg.pitch_or_radius > amrex::Real(0.0)) ? bldg.pitch_or_radius : amrex::Real(0.5) * W;
        shape_width = amrex::Real(2.0) * radius;
        shape_half_len = radius;
        
        // Check if point is inside building volume (cylinder)
        const amrex::Real dist_to_center = std::sqrt(dx * dx + dy * dy);
        if (dist_to_center <= radius && (z - bldg.zmin) <= H) {
            return false;
        }
        
        const amrex::Real cyl_aspect_ratio = shape_width / H;
        Lc = amrex::Real(0.5) * H * std::sqrt(cyl_aspect_ratio) * amrex::Real(0.5);
        deficit_scale = amrex::Real(0.7);
        separation_scale = amrex::Real(0.7);
    } else if (bldg.shape == BuildingShape::PITCHED_ROOF) {
        const amrex::Real pitch_rad = (bldg.pitch_or_radius > amrex::Real(0.0)) ? bldg.pitch_or_radius : amrex::Real(25.0) * MathConstants::deg_to_rad;
        const amrex::Real pitch_deg = pitch_rad / MathConstants::deg_to_rad;
        const amrex::Real pitch_factor = std::max(amrex::Real(0.4), amrex::Real(1.0) - amrex::Real(0.01) * pitch_deg);
        
        Lc = Lc * pitch_factor;
        Hc = amrex::Real(0.67) * H * (amrex::Real(0.8) + amrex::Real(0.2) * std::cos(pitch_rad));
        separation_scale = pitch_factor;
        vortex_scale = amrex::Real(1.0) + amrex::Real(0.5) * std::sin(pitch_rad);
    }

    // Wake starts at downwind face of building
    const amrex::Real wake_start = shape_half_len;
    
    // Check if point is downwind of building
    if (x_wind < wake_start) {
        return false;  // Upstream or inside building
    }
    
    // Distance downwind from building back face
    const amrex::Real x_wake = x_wind - wake_start;
    const amrex::Real Wc = shape_width;                       // Cavity width
    
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
        const amrex::Real deficit_magnitude = params.c2 * U_ref * deficit_scale;
        du = -deficit_magnitude * wd_x;  // Negate to create reverse flow
        dv = -deficit_magnitude * wd_y;  // Negate to create reverse flow
         
        // Add rooftop vortex with horizontal components
        amrex::Real dw_vortex = amrex::Real(0.0);
        amrex::Real du_vortex = amrex::Real(0.0);
        amrex::Real dv_vortex = amrex::Real(0.0);
        compute_rooftop_vortex(x_wake, z_local, H, Hc, Lc, U_ref,
                               y_wind, Wc, wd_x, wd_y,
                               du_vortex, dv_vortex, dw_vortex);
        du += du_vortex * vortex_scale;
        dv += dv_vortex * vortex_scale;
        dw = dw_vortex * vortex_scale;
         
        return true;
    }
    
    // ========================================================================
    // 2. Far-wake zone (displacement zone)
    // ========================================================================
    // Huber-Snyder wake extends to 5*H (longer than Röckle's typical 3*H)
    const amrex::Real Lw = amrex::Real(5.0) * H * separation_scale;  // Wake extent
    
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
        const amrex::Real deficit_magnitude = params.c2 * U_ref * decay_factor * deficit_scale;
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
// Compute Wind-Aligned Building Dimensions
//
// For rotated buildings, we need to project the building's dimensions onto the
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
    (void)params;
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
    
    // Shape-specific parameters
    amrex::Real shape_width = W;
    amrex::Real shape_half_len = L / amrex::Real(2.0);
    
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
    amrex::Real Hc = std::min(amrex::Real(1.5) * Hc_peak, H);
    
    amrex::Real deficit_scale = amrex::Real(1.0);
    amrex::Real vortex_scale = amrex::Real(1.0);
    amrex::Real separation_scale = amrex::Real(1.0);

    if (bldg.shape == BuildingShape::CYLINDRICAL) {
        const amrex::Real radius = (bldg.pitch_or_radius > amrex::Real(0.0)) ? bldg.pitch_or_radius : amrex::Real(0.5) * W;
        shape_width = amrex::Real(2.0) * radius;
        shape_half_len = radius;
        
        // Check if point is inside building volume (cylinder)
        const amrex::Real dist_to_center = std::sqrt(dx_global * dx_global + dy_global * dy_global);
        if (dist_to_center <= radius && (z - bldg.zmin) <= H) {
            return false;
        }
        
        Lc = amrex::Real(0.5) * Lc;
        deficit_scale = amrex::Real(0.7);
        separation_scale = amrex::Real(0.7);
    } else if (bldg.shape == BuildingShape::PITCHED_ROOF) {
        const amrex::Real pitch_rad = (bldg.pitch_or_radius > amrex::Real(0.0)) ? bldg.pitch_or_radius : amrex::Real(25.0) * MathConstants::deg_to_rad;
        const amrex::Real pitch_deg = pitch_rad / MathConstants::deg_to_rad;
        const amrex::Real pitch_factor = std::max(amrex::Real(0.4), amrex::Real(1.0) - amrex::Real(0.01) * pitch_deg);
        
        Lc = Lc * pitch_factor;
        Hc = Hc * (amrex::Real(0.8) + amrex::Real(0.2) * std::cos(pitch_rad));
        separation_scale = pitch_factor;
        vortex_scale = amrex::Real(1.0) + amrex::Real(0.5) * std::sin(pitch_rad);
    }

    // Downwind distance from building back face
    const amrex::Real x_wake = x_wind - shape_half_len;
    
    // Check if point is upstream of building
    if (x_wake < amrex::Real(0.0)) {
        return false;  // Upstream, no wake effect
    }
    
    // Height above ground (relative to building base)
    const amrex::Real z_local = z - bldg.zmin;
    
    // Cavity width (same as building width)
    const amrex::Real Wc = shape_width;
    
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
        const amrex::Real deficit_magnitude = cavity_deficit_factor * U_ref * deficit_scale;
        du = -deficit_magnitude * wind_dir_x;  // Negate to create reverse flow
        dv = -deficit_magnitude * wind_dir_y;  // Negate to create reverse flow
        
        // Streamline deflection: vertical component from flow over building
        // Enhanced vertical mixing in PRIME cavity
        const amrex::Real streamline_deflection = amrex::Real(0.2) * U_ref * (z_local / Hc);
        dw = streamline_deflection * vortex_scale;
        
        return true;
    }
    
    // ========================================================================
    // 5. PRIME far-wake zone (enhanced mixing, extended downwind)
    // ========================================================================
    // PRIME wake extends to 10*H (regulatory standard, longer than others)
    const amrex::Real Lw = amrex::Real(10.0) * H * separation_scale;
    
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
        const amrex::Real deficit_magnitude = wake_deficit_factor * U_ref * decay_factor * deficit_scale;
        du = deficit_magnitude * wind_dir_x;
        dv = deficit_magnitude * wind_dir_y;
        
        // Vertical component from streamline deflection (diminishes downwind)
        dw = amrex::Real(0.1) * U_ref * decay_factor * (z_local / Hw) * vortex_scale;
        
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
        
        // Apply Britter-Hanna urban canyon attenuation
        // Reduces wind speed in dense urban environments based on building density
        if (params.enable_britter_hanna_urban) {
            // Compute frontal area index: φ_v = (H × area_projection) / area_ref
            // Simplified approach: use building frontal projection
            const amrex::Real bldg_height = bldg.zmax - bldg.zmin;
            const amrex::Real bldg_width = std::abs(bldg.ymax - bldg.ymin);
            const amrex::Real bldg_depth = std::abs(bldg.xmax - bldg.xmin);
            
            // Estimate frontal area index as fraction of building volume per unit volume
            // φ_v ranges from 0 (isolated) to 1.0 (dense urban)
            const amrex::Real frontal_area = bldg_height * bldg_width;
            
            // Estimate reference area as ~100x product of building dimensions
            const amrex::Real ref_area = amrex::Real(100.0) * bldg_width * bldg_depth;
            amrex::Real phi_v = frontal_area / ref_area;
            
            // Clamp to reasonable range
            phi_v = amrex::max(amrex::Real(0.0), amrex::min(phi_v, amrex::Real(1.0)));
            
            // Apply urban canyon reduction factor
            const amrex::Real urban_reduction = compute_britter_hanna_urban_reduction(
                phi_v, params.britter_hanna_alpha);
            
            // Attenuate velocity components
            u *= urban_reduction;
            v *= urban_reduction;
            w *= urban_reduction;
        }
    }
}

// ============================================================================
// Compute building dimensions from bounds
//
// Supports arbitrary building orientations.
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
// Wake Deficit Superposition Refinement
//
// Enhanced wake superposition with weighted blending of overlapping wake zones.
// Instead of exclusive assignment (point belongs to only one building's wake),
// uses distance-weighted blending to smoothly merge adjacent wake effects.
//
// Key improvements:
// 1. Smooth velocity field transitions at wake boundaries
// 2. Physically realistic blending at wake intersections
// 3. Prevents discontinuities from exclusive zone assignment
//
// Weighted blending formula:
//   du_blended = sum(w_i * du_i) / sum(w_i)
//
// where:
//   w_i = exp(-distance_to_wake_i / blend_scale)
//   blend_scale ≈ building height (typical: 10-30 m)
//
// This creates smooth transitions while preserving deficit magnitudes.
//
// References:
// - Ochieng, R., Bartha, D., Sinn, F., Greschow, B., Emeis, S. (2005).
//   Near-wake effects on wind farm performance - impact of multiple buildings.
//   Wind Energy, 8(1), 47-60.
// ============================================================================

// Compute distance to nearest wake zone boundary
//
// Estimates how far a point is from any building's wake zone.
// Used for weighting in deficit blending.
//
// Inputs:
//   x, y, z         - Point coordinates [m]
//   bldg_center_x   - Building center x-coordinate [m]
//   bldg_center_y   - Building center y-coordinate [m]
//   bldg_half_len   - Half-length of building in wind direction [m]
//   bldg_width      - Building width (perpendicular to wind) [m]
//   bldg_height     - Building height [m]
//   max_wake_length - Maximum extent of wake [m]
//   wind_dir_x      - Wind direction x-component (unit vector)
//   wind_dir_y      - Wind direction y-component (unit vector)
//
// Returns:
//   Distance to wake zone boundary [m]
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_distance_to_wake_zone(
    amrex::Real x, amrex::Real y, amrex::Real z,
    amrex::Real bldg_center_x, amrex::Real bldg_center_y,
    amrex::Real bldg_half_len, amrex::Real bldg_width,
    amrex::Real bldg_height,
    amrex::Real max_wake_length,
    amrex::Real wind_dir_x, amrex::Real wind_dir_y)
{
    // Transform to wind-aligned coordinates
    const amrex::Real dx = x - bldg_center_x;
    const amrex::Real dy = y - bldg_center_y;
    
    const amrex::Real x_wind = dx * wind_dir_x + dy * wind_dir_y;
    const amrex::Real y_wind = -dx * wind_dir_y + dy * wind_dir_x;
    
    // Distance to wake zone in downwind direction
    const amrex::Real wake_start = bldg_half_len;
    const amrex::Real wake_end = bldg_half_len + max_wake_length;
    
    amrex::Real dist_wind;
    if (x_wind < wake_start) {
        dist_wind = wake_start - x_wind;  // Upstream of wake
    } else if (x_wind > wake_end) {
        dist_wind = x_wind - wake_end;    // Downstream of wake
    } else {
        dist_wind = amrex::Real(0.0);     // Inside wake
    }
    
    // Distance to wake zone in lateral direction
    const amrex::Real half_width = bldg_width / amrex::Real(2.0);
    const amrex::Real dist_lateral = std::max(amrex::Real(0.0),
                                              std::abs(y_wind) - half_width);
    
    // Distance to wake zone in vertical direction
    const amrex::Real dist_vertical = std::max(amrex::Real(0.0),
                                               std::abs(z - (bldg_center_y + bldg_height / amrex::Real(2.0))) - bldg_height / amrex::Real(2.0));
    
    // Total distance as 3D Euclidean distance to wake zone
    return std::sqrt(dist_wind * dist_wind + dist_lateral * dist_lateral + 
                     dist_vertical * dist_vertical);
}

// Compute blending weight for wake deficit at a point
//
// Determines how strongly a point is influenced by a building's wake.
// Based on distance to wake zone and building geometry.
//
// Inputs:
//   distance_to_wake    - Distance to nearest wake boundary [m]
//   building_height     - Building height [m]
//   blend_scale_factor  - Blending scale (relative to building height, default 0.5)
//
// Returns:
//   Blending weight [0, 1] where:
//   - 1.0 = point is in the wake zone (strong influence)
//   - 0.5-1.0 = near-wake region (moderate blending)
//   - 0.0-0.5 = far field (minimal influence)
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real compute_wake_blending_weight(
    amrex::Real distance_to_wake,
    amrex::Real building_height,
    amrex::Real blend_scale_factor = amrex::Real(0.5)) noexcept
{
    if (distance_to_wake < amrex::Real(1.0e-6)) {
        return amrex::Real(1.0);  // Inside wake zone
    }
    
    // Exponential decay of influence with distance
    // blend_scale = blend_scale_factor * H (typically 0.5H)
    const amrex::Real blend_scale = blend_scale_factor * building_height;
    
    if (blend_scale < amrex::Real(1.0e-6)) {
        return amrex::Real(0.0);
    }
    
    // Weight = exp(-distance / blend_scale)
    // This provides smooth decay from 1.0 (in wake) to ~0.0 (far field)
    const amrex::Real weight = std::exp(-distance_to_wake / blend_scale);
    
    // Clamp to [0, 1]
    return std::max(amrex::Real(0.0), std::min(weight, amrex::Real(1.0)));
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
// Supports arbitrary building orientations.
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
    const int* bldg_shape,
    const amrex::Real* bldg_pitch_or_radius,
    const amrex::Real* bldg_pitch_direction,
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
        
        // Apply building rotation angle
        bldg.rotation = bldg_rotation[b];
        bldg.shape = static_cast<BuildingShape>(bldg_shape[b]);
        bldg.pitch_or_radius = bldg_pitch_or_radius[b];
        bldg.pitch_direction = bldg_pitch_direction[b];
        
        // Compute wake deficit from this building
        amrex::Real du_b, dv_b, dw_b;
        bool in_wake = false;
        if (params.model_type == WakeModelType::AERMOD_PRIME) {
            in_wake = aermod_prime_wake_deficit(
                x, y, z, bldg, U_ref, wind_dir_x, wind_dir_y, params, du_b, dv_b, dw_b);
        } else if (params.model_type == WakeModelType::HUBER_SNYDER) {
            in_wake = huber_snyder_wake_deficit(
                x, y, z, bldg, U_ref, wind_dir_x, wind_dir_y, params, du_b, dv_b, dw_b);
        } else {
            in_wake = rockle_wake_deficit(
                x, y, z, bldg, U_ref, wind_dir_x, wind_dir_y, params, du_b, dv_b, dw_b);
        }
        
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
// Supports orientation-aware vortex modeling.
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
        
        // Orientation-aware vortex strength
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

// ============================================================================
// POLYGON WAKE MODELS
//
// Adaptations of the three wake models (Röckle, Huber-Snyder, AERMOD PRIME)
// for arbitrary polygon building footprints.
// ============================================================================

// ============================================================================
// Polygon Röckle (1990) building wake model
//
// Applies Röckle wake physics to polygon-shaped buildings by computing
// effective dimensions and width-varying cavity/far-wake zones.
//
// Inputs:
//   x, y, z          - Query point coordinates [m]
//   poly_vx, poly_vy - Polygon vertex arrays [m]
//   n_vertices       - Number of polygon vertices
//   poly_zmin        - Building base elevation [m]
//   poly_height      - Building height [m]
//   U_ref            - Reference wind speed [m/s]
//   wd_x, wd_y       - Wind direction unit vector
//   params           - Wake model parameters
//
// Outputs:
//   du, dv, dw       - Velocity deficit components [m/s]
//
// Returns:
//   true if point is in wake zone, false otherwise
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool polygon_rockle_wake_deficit(
    amrex::Real x, amrex::Real y, amrex::Real z,
    const amrex::Real* poly_vx, const amrex::Real* poly_vy,
    int n_vertices,
    amrex::Real poly_zmin, amrex::Real poly_height,
    amrex::Real U_ref,
    amrex::Real wd_x, amrex::Real wd_y,
    const WakeParams& params,
    amrex::Real& du, amrex::Real& dv, amrex::Real& dw)
{
    du = amrex::Real(0.0);
    dv = amrex::Real(0.0);
    dw = amrex::Real(0.0);
    
    if (!params.enabled || n_vertices < 3) {
        return false;
    }
    
    // 1. Check if point is inside polygon (inside building = no wake)
    if (point_in_polygon(x, y, poly_vx, poly_vy, n_vertices)) {
        return false;
    }
    
    // 2. Compute polygon centroid and transform to wind-aligned coordinates
    amrex::Real centroid_x, centroid_y;
    compute_polygon_centroid(poly_vx, poly_vy, n_vertices, centroid_x, centroid_y);
    
    amrex::Real dx = x - centroid_x;
    amrex::Real dy = y - centroid_y;
    amrex::Real x_wind = dx * wd_x + dy * wd_y;
    amrex::Real y_wind = -dx * wd_y + dy * wd_x;
    
    // 3. Find wake start (most downwind vertex)
    amrex::Real wake_start = find_polygon_wake_start(poly_vx, poly_vy, n_vertices, wd_x, wd_y);
    if (x_wind < wake_start) {
        return false;  // Upstream of building
    }
    
    amrex::Real x_wake = x_wind - wake_start;
    
    // 4. Get polygon width at this position
    amrex::Real W_poly = get_polygon_width_at_distance(poly_vx, poly_vy, n_vertices, x_wind, wd_x, wd_y);
    amrex::Real H = poly_height;
    amrex::Real z_local = z - poly_zmin;
     
    // 5. Apply Röckle cavity and far-wake zones
    amrex::Real Lr = params.c1 * H;
    amrex::Real Hr = amrex::Real(0.67) * H;
     
    // Cavity zone
    if (x_wake <= Lr) {
        if (std::abs(y_wind) > W_poly / amrex::Real(2.0)) {
            return false;
        }
        if (z_local < amrex::Real(0.0) || z_local > Hr) {
            return false;
        }
         
        amrex::Real deficit_magnitude = params.c2 * U_ref;
        du = -deficit_magnitude * wd_x;  // Negate to create reverse flow
        dv = -deficit_magnitude * wd_y;  // Negate to create reverse flow
         
        amrex::Real dw_vortex = amrex::Real(0.0);
        amrex::Real du_vortex = amrex::Real(0.0);
        amrex::Real dv_vortex = amrex::Real(0.0);
        compute_rooftop_vortex(x_wake, z_local, H, Hr, Lr, U_ref,
                               y_wind, W_poly, wd_x, wd_y,
                               du_vortex, dv_vortex, dw_vortex);
        du += du_vortex;
        dv += dv_vortex;
        dw = dw_vortex;
        return true;
    }
    
    // Far-wake zone
    amrex::Real Lf = params.separation_length * H;
    if (x_wake <= Lf) {
        amrex::Real decay_factor = amrex::Real(1.0) - (x_wake - Lr) / (Lf - Lr);
        amrex::Real spread_factor = amrex::Real(1.0) + (x_wake - Lr) / (Lf - Lr);
        amrex::Real Wf = W_poly * spread_factor;
        
        if (std::abs(y_wind) > Wf / amrex::Real(2.0)) {
            return false;
        }
        
        amrex::Real Hf = Hr * spread_factor;
        if (z_local < amrex::Real(0.0) || z_local > Hf) {
            return false;
        }
        
        amrex::Real deficit_magnitude = params.c2 * U_ref * decay_factor;
        du = deficit_magnitude * wd_x;
        dv = deficit_magnitude * wd_y;
        dw = amrex::Real(0.0);
        
        return true;
    }
    
    return false;
}

// ============================================================================
// Polygon Huber-Snyder (EPA) building wake model
//
// Applies Huber-Snyder wake physics to polygon buildings using effective dimensions.
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool polygon_huber_snyder_wake_deficit(
    amrex::Real x, amrex::Real y, amrex::Real z,
    const amrex::Real* poly_vx, const amrex::Real* poly_vy,
    int n_vertices,
    amrex::Real poly_zmin, amrex::Real poly_height,
    amrex::Real U_ref,
    amrex::Real wd_x, amrex::Real wd_y,
    const WakeParams& params,
    amrex::Real& du, amrex::Real& dv, amrex::Real& dw)
{
    du = amrex::Real(0.0);
    dv = amrex::Real(0.0);
    dw = amrex::Real(0.0);
    
    if (!params.enabled || n_vertices < 3) {
        return false;
    }
    
    if (point_in_polygon(x, y, poly_vx, poly_vy, n_vertices)) {
        return false;
    }
    
    amrex::Real centroid_x, centroid_y;
    compute_polygon_centroid(poly_vx, poly_vy, n_vertices, centroid_x, centroid_y);
    
    amrex::Real dx = x - centroid_x;
    amrex::Real dy = y - centroid_y;
    amrex::Real x_wind = dx * wd_x + dy * wd_y;
    amrex::Real y_wind = -dx * wd_y + dy * wd_x;
    
    amrex::Real wake_start = find_polygon_wake_start(poly_vx, poly_vy, n_vertices, wd_x, wd_y);
    if (x_wind < wake_start) return false;
    
    amrex::Real x_wake = x_wind - wake_start;
    amrex::Real W_poly = get_polygon_width_at_distance(poly_vx, poly_vy, n_vertices, x_wind, wd_x, wd_y);
    amrex::Real H = poly_height;
    amrex::Real z_local = z - poly_zmin;
    
    // Huber-Snyder uses aspect-ratio dependent cavity length
    amrex::Real aspect_ratio = W_poly / H;
    amrex::Real Lc = amrex::Real(0.5) * H * std::sqrt(aspect_ratio);
    amrex::Real Hc = amrex::Real(0.67) * H;
    
    // Cavity zone
    if (x_wake <= Lc) {
        if (std::abs(y_wind) > W_poly / amrex::Real(2.0)) return false;
        if (z_local < amrex::Real(0.0) || z_local > Hc) return false;
         
        amrex::Real deficit_magnitude = params.c2 * U_ref;
        du = -deficit_magnitude * wd_x;  // Negate to create reverse flow
        dv = -deficit_magnitude * wd_y;  // Negate to create reverse flow
        amrex::Real dw_vortex = amrex::Real(0.0);
        amrex::Real du_vortex = amrex::Real(0.0);
        amrex::Real dv_vortex = amrex::Real(0.0);
        compute_rooftop_vortex(x_wake, z_local, H, Hc, Lc, U_ref,
                               y_wind, W_poly, wd_x, wd_y,
                               du_vortex, dv_vortex, dw_vortex);
        du += du_vortex;
        dv += dv_vortex;
        dw = dw_vortex;
        return true;
    }
    
    // Far-wake zone (Huber-Snyder: 5*H)
    amrex::Real Lw = amrex::Real(5.0) * H;
    if (x_wake <= Lw) {
        amrex::Real x_ratio = x_wake / Lc;
        amrex::Real decay_factor = std::min(
            amrex::Real(1.0) / std::sqrt(x_ratio),
            amrex::Real(1.0));
        
        amrex::Real spread_factor = amrex::Real(1.0) + (x_wake - Lc) / (Lw - Lc);
        amrex::Real Ww = W_poly * spread_factor;
        
        if (std::abs(y_wind) > Ww / amrex::Real(2.0)) return false;
        
        amrex::Real Hw = Hc * spread_factor;
        if (z_local < amrex::Real(0.0) || z_local > Hw) return false;
        
        amrex::Real deficit_magnitude = params.c2 * U_ref * decay_factor;
        du = deficit_magnitude * wd_x;
        dv = deficit_magnitude * wd_y;
        dw = amrex::Real(0.0);
        return true;
    }
    
    return false;
}

// ============================================================================
// Polygon AERMOD PRIME building wake model
//
// Applies AERMOD PRIME wake physics to polygon buildings using effective
// projected building area and enhanced turbulence modeling.
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
bool polygon_aermod_prime_wake_deficit(
    amrex::Real x, amrex::Real y, amrex::Real z,
    const amrex::Real* poly_vx, const amrex::Real* poly_vy,
    int n_vertices,
    amrex::Real poly_zmin, amrex::Real poly_height,
    amrex::Real U_ref,
    amrex::Real wd_x, amrex::Real wd_y,
    const WakeParams& params,
    amrex::Real& du, amrex::Real& dv, amrex::Real& dw)
{
    du = amrex::Real(0.0);
    dv = amrex::Real(0.0);
    dw = amrex::Real(0.0);
    
    if (!params.enabled || n_vertices < 3) {
        return false;
    }
    
    if (point_in_polygon(x, y, poly_vx, poly_vy, n_vertices)) {
        return false;
    }
    
    // Compute effective dimensions for PBA method
    amrex::Real L_eff, W_eff;
    compute_polygon_effective_dimensions(poly_vx, poly_vy, n_vertices, wd_x, wd_y, L_eff, W_eff);
    
    amrex::Real centroid_x, centroid_y;
    compute_polygon_centroid(poly_vx, poly_vy, n_vertices, centroid_x, centroid_y);
    
    amrex::Real dx = x - centroid_x;
    amrex::Real dy = y - centroid_y;
    amrex::Real x_wind = dx * wd_x + dy * wd_y;
    amrex::Real y_wind = -dx * wd_y + dy * wd_x;
    
    amrex::Real wake_start = find_polygon_wake_start(poly_vx, poly_vy, n_vertices, wd_x, wd_y);
    if (x_wind < wake_start) return false;
    
    amrex::Real x_wake = x_wind - wake_start;
    amrex::Real H = poly_height;
    amrex::Real z_local = z - poly_zmin;
    
    // AERMOD PRIME cavity zone
    amrex::Real B_char = std::sqrt(H * W_eff);  // Characteristic dimension
    amrex::Real aspect_ratio = W_eff / H;
    
    amrex::Real Lc = (aspect_ratio > amrex::Real(1.0)) ?
        amrex::Real(0.9) * B_char :
        amrex::Real(0.5) * B_char;
     
    amrex::Real Hc_peak = (aspect_ratio > amrex::Real(1.0)) ?
        amrex::Real(0.22) * B_char :
        amrex::Real(0.38) * B_char;
     
    amrex::Real Hc = std::min(amrex::Real(1.5) * Hc_peak, H);
     
    // Cavity zone
    if (x_wake <= Lc) {
        if (std::abs(y_wind) > W_eff / amrex::Real(2.0)) return false;
        if (z_local < amrex::Real(0.0) || z_local > Hc) return false;
         
        amrex::Real cavity_deficit_factor = amrex::Real(0.5);
        amrex::Real deficit_magnitude = cavity_deficit_factor * U_ref;
        du = -deficit_magnitude * wd_x;  // Negate to create reverse flow
        dv = -deficit_magnitude * wd_y;  // Negate to create reverse flow
        amrex::Real dw_vortex = amrex::Real(0.0);
        amrex::Real du_vortex = amrex::Real(0.0);
        amrex::Real dv_vortex = amrex::Real(0.0);
        compute_rooftop_vortex(x_wake, z_local, H, Hc, Lc, U_ref,
                               y_wind, W_eff, wd_x, wd_y,
                               du_vortex, dv_vortex, dw_vortex);
        du += du_vortex;
        dv += dv_vortex;
        dw = dw_vortex;
        return true;
    }
     
    // Far-wake zone (AERMOD PRIME: 10*H with enhanced mixing)
    amrex::Real Lf = amrex::Real(10.0) * H;
    if (x_wake <= Lf) {
        amrex::Real decay_factor = amrex::Real(1.0) - (x_wake - Lc) / (Lf - Lc);
        amrex::Real spread_factor = amrex::Real(1.0) + amrex::Real(0.5) * (x_wake - Lc) / Lc;
        amrex::Real Wf = W_eff * spread_factor;
        
        if (std::abs(y_wind) > Wf / amrex::Real(2.0)) return false;
        
        amrex::Real Hf = Hc * spread_factor;
        if (z_local < amrex::Real(0.0) || z_local > Hf) return false;
        
        amrex::Real deficit_magnitude = amrex::Real(0.4) * U_ref * decay_factor;
        du = deficit_magnitude * wd_x;
        dv = deficit_magnitude * wd_y;
        dw = amrex::Real(0.0);
        return true;
    }
    
    return false;
}

#endif // WAKE_MODELS_H
