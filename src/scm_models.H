// ============================================================================
// scm_models.H
// Single Column Model (SCM) helper functions for wind profile initialization
//
// This file provides functions for:
// - Time-dependent 1D simulation similar to hrrr_1dsolver_terrain.py
// - Wind equations with Coriolis and turbulent diffusion
// - Temperature profile evolution with lapse rate
// - Turbulence kinetic energy (TKE) prediction
// - Geostrophic wind determination from target wind speed at reference height
// ============================================================================

#ifndef SCM_MODELS_H
#define SCM_MODELS_H

#include <AMReX.H>
#include <cmath>
#include <algorithm>
#include <vector>

namespace SCMModels {

// Physical constants
constexpr amrex::Real EARTH_ROTATION_RATE = 7.27e-5;  // rad/s (Coriolis parameter)
constexpr amrex::Real GRAVITY = 9.81;                 // m/s^2
constexpr amrex::Real VON_KARMAN = 0.41;              // dimensionless

// ============================================================================
// SCMState1D
// Struct to hold 1D SCM state during time-stepping
// ============================================================================
struct SCMState1D {
    int nz;
    amrex::Real z_upper;
    amrex::Real z0;
    amrex::Real dz;
    amrex::Real dt;
    
    std::vector<amrex::Real> z;          // Height grid [m]
    std::vector<amrex::Real> ux;         // U-wind component [m/s]
    std::vector<amrex::Real> uy;         // V-wind component [m/s]
    std::vector<amrex::Real> temperature;// Temperature [K]
    std::vector<amrex::Real> tke;        // Turbulent kinetic energy [m^2/s^2]
    std::vector<amrex::Real> nut;        // Eddy viscosity [m^2/s]
    std::vector<amrex::Real> lscale;     // Mixing length [m]
    
    // Geostrophic wind
    amrex::Real ug;
    amrex::Real vg;
    
    // Surface parameters
    amrex::Real ustar;                   // Friction velocity [m/s]
    amrex::Real Qh;                      // Sensible heat flux [W/m^2]
    amrex::Real mo_length;               // Monin-Obukhov length [m]
    amrex::Real mo_length_prescribed;    // User-specified Monin-Obukhov length (if provided)
    bool use_prescribed_mo_length;       // Whether to use prescribed M-O length
    
    // Simulation parameters
    amrex::Real t_ref;                   // Reference surface temperature [K]
    amrex::Real lapse_rate;              // Adiabatic lapse rate [K/m]
    amrex::Real coriolis;                // Coriolis parameter [1/s]
    amrex::Real latitude;                // Latitude [degrees]
    int lower;                           // Index of lowest active level
};

// ============================================================================
// Monin-Obukhov Stability Functions
// These functions compute stability corrections based on z/L (height/Obukhov length)
// ============================================================================

// Stable correction function (Högström 1988)
inline amrex::Real mo_stable_correction(amrex::Real z_over_L) {
    // phi_m = 1 + a*z/L
    constexpr amrex::Real a_stable = 5.0;
    return 1.0 + a_stable * z_over_L;
}

// Unstable correction function (Businger et al. 1971)
inline amrex::Real mo_unstable_correction(amrex::Real z_over_L) {
    // phi_m = (1 - b*z/L)^(-1/4)
    // This is only valid for z/L < 0
    constexpr amrex::Real b_unstable = 16.0;
    amrex::Real x = 1.0 + b_unstable * std::abs(z_over_L);
    return std::pow(x, -0.25);
}

// Combined stability function
inline amrex::Real mo_phi_m(amrex::Real z_over_L) {
    if (z_over_L > 0.0) {
        // Stable
        return mo_stable_correction(z_over_L);
    } else if (z_over_L < -0.001) {
        // Unstable
        return mo_unstable_correction(z_over_L);
    } else {
        // Neutral (approximately)
        return 1.0;
    }
}

// Heat transfer stability function
inline amrex::Real mo_phi_h(amrex::Real z_over_L) {
    if (z_over_L > 0.0) {
        // Stable - similar to momentum with slightly different coefficient
        constexpr amrex::Real a_stable_h = 5.0;
        return 1.0 + a_stable_h * z_over_L;
    } else if (z_over_L < -0.001) {
        // Unstable - different exponent for heat
        constexpr amrex::Real b_unstable_h = 16.0;
        amrex::Real x = 1.0 + b_unstable_h * std::abs(z_over_L);
        return std::pow(x, -0.5);  // Note: -0.5 exponent for heat
    } else {
        // Neutral
        return 1.0;
    }
}

// ============================================================================
// initialize_1d_state
// Initialize 1D SCM state with uniform geostrophic wind and linear temperature profile
// Supports optional heat flux and Monin-Obukhov length specification
// ============================================================================
void initialize_1d_state(SCMState1D& state,
                         amrex::Real ug,
                         amrex::Real vg,
                         amrex::Real t_ref,
                         amrex::Real lapse_rate,
                         amrex::Real z0,
                         amrex::Real domain_height,
                         amrex::Real dz,
                         amrex::Real latitude,
                         amrex::Real heat_flux = 0.0,
                         amrex::Real mo_length_prescribed = -1.0e30)
{
    state.ug = ug;
    state.vg = vg;
    state.t_ref = t_ref;
    state.lapse_rate = lapse_rate;
    state.z0 = z0;
    state.dz = dz;
    state.z_upper = domain_height;
    state.latitude = latitude;
    
    // Build height grid
    state.nz = static_cast<int>(std::ceil(domain_height / dz)) + 1;
    state.z.resize(state.nz);
    for (int i = 0; i < state.nz; ++i) {
        state.z[i] = i * dz;
    }
    
    // Initialize velocity
    state.ux.resize(state.nz, ug);
    state.uy.resize(state.nz, vg);
    
    // Initialize temperature with lapse rate
    state.temperature.resize(state.nz);
    for (int i = 0; i < state.nz; ++i) {
        state.temperature[i] = t_ref - lapse_rate * state.z[i];
    }
    
    // Initialize turbulence (small initial values)
    state.tke.resize(state.nz, 0.1);
    state.nut.resize(state.nz, 1.0e-5);
    state.lscale.resize(state.nz, 0.1);
    
    // Find lowest active level above z0
    state.lower = 0;
    for (int i = 0; i < state.nz; ++i) {
        if (state.z[i] >= z0) {
            state.lower = i;
            break;
        }
    }
    
    // Initialize Coriolis
    amrex::Real pi_val = amrex::Real(3.14159265358979323846);
    state.coriolis = 2.0 * EARTH_ROTATION_RATE * std::sin(latitude * pi_val / 180.0);
    
    // Initialize surface friction velocity
    if (state.lower + 1 < state.nz) {
        amrex::Real M1 = std::sqrt(state.ux[state.lower + 1] * state.ux[state.lower + 1] +
                                   state.uy[state.lower + 1] * state.uy[state.lower + 1]);
        state.ustar = (M1 > 0.0) ? VON_KARMAN * M1 / std::log((state.z[state.lower + 1] + z0) / z0)
                                 : 0.0;
    } else {
        state.ustar = 0.0;
    }
    
    // Set heat flux
    state.Qh = heat_flux;
    
    // Set Monin-Obukhov length
    state.mo_length_prescribed = mo_length_prescribed;
    state.use_prescribed_mo_length = (std::abs(mo_length_prescribed) < 1.0e29);  // If reasonable value provided
    
    // Initialize mo_length to neutral (will be updated in compute_similarity_surface)
    if (state.use_prescribed_mo_length) {
        state.mo_length = state.mo_length_prescribed;
    } else {
        state.mo_length = -1.0e30;  // Neutral
    }
}

// ============================================================================
// compute_similarity_surface
// Compute surface layer parameters (ustar, heat flux, etc.)
// Uses Monin-Obukhov similarity theory with optional stability corrections
// ============================================================================
void compute_similarity_surface(SCMState1D& state)
{
    if (state.lower + 1 >= state.nz) return;
    
    amrex::Real M1 = std::sqrt(state.ux[state.lower + 1] * state.ux[state.lower + 1] +
                               state.uy[state.lower + 1] * state.uy[state.lower + 1]);
    
    // Compute friction velocity (log-law, neutral approximation)
    if (M1 > 0.0) {
        state.ustar = VON_KARMAN * M1 / std::log((state.z[state.lower + 1] + state.z0) / state.z0);
    } else {
        state.ustar = 0.0;
    }
    
    // Use prescribed Monin-Obukhov length if provided, otherwise neutral
    if (!state.use_prescribed_mo_length) {
        // Neutral condition
        state.mo_length = -1.0e30;
    }
    
    // Compute eddy viscosity in surface layer with stability correction
    if (state.lower < state.nz) {
        amrex::Real z0_phi = 1.0;  // Default neutral
        
        // Apply Monin-Obukhov stability correction if prescribed M-O length is available
        if (state.use_prescribed_mo_length && std::abs(state.mo_length) > 1.0e-10) {
            amrex::Real z_over_L_z0 = state.z0 / state.mo_length;
            z0_phi = mo_phi_m(z_over_L_z0);
        }
        
        state.nut[state.lower] = state.ustar * VON_KARMAN * state.z0 / z0_phi;
    }
    
    // Update velocities at lowest level using stability-corrected log-law
    if (state.lower < state.nz && state.lower + 1 < state.nz) {
        amrex::Real z0_phi = 1.0;  // Default neutral
        
        // Apply stability correction if prescribed M-O length is available
        if (state.use_prescribed_mo_length && std::abs(state.mo_length) > 1.0e-10) {
            amrex::Real z_over_L_z0 = state.z0 / state.mo_length;
            z0_phi = mo_phi_m(z_over_L_z0);
        }
        
        amrex::Real M0 = M1 - state.ustar / VON_KARMAN * z0_phi;
        if (M1 > 0.0) {
            state.ux[state.lower] = M0 * state.ux[state.lower + 1] / M1;
            state.uy[state.lower] = M0 * state.uy[state.lower + 1] / M1;
        }
    }
}

// ============================================================================
// compute_eddy_viscosity
// Compute eddy viscosity using mixing length and TKE
// Updates lscale and nut based on TKE and stratification
// ============================================================================
void compute_eddy_viscosity(SCMState1D& state)
{
    amrex::Real dz = state.dz;
    
    for (int i = state.lower + 1; i < state.nz - 1; ++i) {
        // Mixing length models
        amrex::Real lshear = VON_KARMAN * (state.z[i] - state.z[state.lower]);
        
        // Maximum length scale (Blackadar length scale)
        amrex::Real fg = std::sqrt(state.ug * state.ug + state.vg * state.vg);
        amrex::Real lmax = (std::abs(state.coriolis) > 1.0e-8) 
                             ? 0.00027 * fg / std::abs(state.coriolis)
                             : 1000.0;
        
        // Buoyancy/stratification effects
        amrex::Real stratification = (GRAVITY / (dz * state.t_ref)) * 
                                     (state.temperature[i] - state.temperature[i - 1]);
        
        // Richardson number and stability corrections (simplified)
        amrex::Real Rt = 0.0;
        amrex::Real lscale = lshear;
        
        if (stratification > 0.0) {
            // Stable stratification
            lscale = lshear * std::sqrt(1.0 - std::max(stratification * lshear * lshear / state.tke[i], 0.3));
        } else {
            // Unstable/neutral
            amrex::Real inv_l_sq = 1.0 / (lshear * lshear) + 1.0 / (lmax * lmax);
            lscale = 1.0 / std::sqrt(inv_l_sq);
        }
        
        state.lscale[i] = lscale;
        
        // Eddy viscosity: nut = cmu * sqrt(tke) * lscale
        // cmu depends on Richardson number (simplified)
        amrex::Real cmu = 0.1;  // Simplified coefficient
        state.nut[i] = cmu * std::sqrt(state.tke[i]) * lscale;
        state.nut[i] = std::max(state.nut[i], 1.0e-5);
    }
    
    // Boundary conditions at top
    if (state.nz > 1) {
        state.nut[state.nz - 1] = state.nut[state.nz - 2];
        state.tke[state.nz - 1] = state.tke[state.nz - 2];
        state.lscale[state.nz - 1] = state.lscale[state.nz - 2];
    }
}

// ============================================================================
// update_wind_x
// Time-step the u-velocity component using momentum equation
// Matches update_windspeed_x from hrrr_1dsolver_terrain.py exactly
// ============================================================================
void update_wind_x(SCMState1D& state, int i, amrex::Real dt)
{
    if (i <= state.lower || i >= state.nz - 1) return;
    
    amrex::Real dz = state.dz;
    amrex::Real z_upper = state.z_upper;
    
    // Damping coefficient that varies with height (relaxation near domain top)
    amrex::Real dFull = 100.0;   // Height scale for full damping
    amrex::Real dRD = 50.0;      // Relaxation damping height scale
    amrex::Real coeff;
    
    if (z_upper - state.z[i] > dRD + dFull) {
        coeff = 0.0;
    } else if (z_upper - state.z[i] > dFull) {
        amrex::Real pi_val = amrex::Real(3.14159265358979323846);
        coeff = 0.5 * std::cos(pi_val * (z_upper - dFull - state.z[i]) / dRD) + 0.5;
    } else {
        coeff = 1.0;
    }
    
    // term1: diffusion with varying eddy viscosity nut[i] * d²u/dz²
    amrex::Real term1 = state.nut[i] * (state.ux[i + 1] - 2.0 * state.ux[i] + state.ux[i - 1]) / (dz * dz);
    
    // term2: gradient of eddy viscosity times velocity gradient
    // dudz computed as forward difference (not centered)
    amrex::Real dudz = (state.ux[i + 1] - state.ux[i]) / dz;
    amrex::Real term2 = 0.5 / dz * (state.nut[i + 1] - state.nut[i - 1]) * dudz;
    
    // Coriolis force: f * v
    amrex::Real coriolis = state.coriolis * state.uy[i];
    
    // Geostrophic balance: -f * vg
    amrex::Real geostrophic = -state.coriolis * state.vg;
    
    // Relaxation/damping toward geostrophic wind
    amrex::Real damping = coeff * (state.ug - state.ux[i]) / 20.0;
    
    // Update u-velocity
    state.ux[i] += dt * (term1 + term2 + coriolis + geostrophic + damping);
}

// ============================================================================
// update_wind_y
// Time-step the v-velocity component using momentum equation
// Matches update_windspeed_y from hrrr_1dsolver_terrain.py exactly
// ============================================================================
void update_wind_y(SCMState1D& state, int i, amrex::Real dt)
{
    if (i <= state.lower || i >= state.nz - 1) return;
    
    amrex::Real dz = state.dz;
    amrex::Real z_upper = state.z_upper;
    
    // Damping coefficient that varies with height
    amrex::Real dFull = 100.0;
    amrex::Real dRD = 50.0;
    amrex::Real coeff;
    
    if (z_upper - state.z[i] > dRD + dFull) {
        coeff = 0.0;
    } else if (z_upper - state.z[i] > dFull) {
        amrex::Real pi_val = amrex::Real(3.14159265358979323846);
        coeff = 0.5 * std::cos(pi_val * (z_upper - dFull - state.z[i]) / dRD) + 0.5;
    } else {
        coeff = 1.0;
    }
    
    // term1: diffusion with varying eddy viscosity
    amrex::Real term1 = state.nut[i] * (state.uy[i + 1] - 2.0 * state.uy[i] + state.uy[i - 1]) / (dz * dz);
    
    // term2: gradient of eddy viscosity times velocity gradient
    amrex::Real dvdz = (state.uy[i + 1] - state.uy[i]) / dz;
    amrex::Real term2 = 0.5 / dz * (state.nut[i + 1] - state.nut[i - 1]) * dvdz;
    
    // Coriolis force: -f * u
    amrex::Real coriolis = -state.coriolis * state.ux[i];
    
    // Geostrophic balance: f * ug
    amrex::Real geostrophic = state.coriolis * state.ug;
    
    // Relaxation/damping toward geostrophic wind
    amrex::Real damping = coeff * (state.vg - state.uy[i]) / 20.0;
    
    // Update v-velocity
    state.uy[i] += dt * (term1 + term2 + coriolis + geostrophic + damping);
}

// ============================================================================
// update_temperature
// Time-step the temperature field
// Matches update_temperature from hrrr_1dsolver_terrain.py exactly
// ============================================================================
void update_temperature(SCMState1D& state, int i, amrex::Real dt)
{
    if (i <= state.lower || i >= state.nz - 1) return;
    
    amrex::Real dz = state.dz;
    amrex::Real sigma_t = 1.0;  // Prandtl number for heat (sigmaT)
    
    // term1: nut/sigmaT * d²T/dz²
    amrex::Real term1 = (state.nut[i] / sigma_t) * 
                        (state.temperature[i + 1] - 2.0 * state.temperature[i] + state.temperature[i - 1]) / (dz * dz);
    
    // term2: d/dz(nut/sigmaT) * dT/dz
    amrex::Real dTdz = (state.temperature[i] - state.temperature[i - 1]) / dz;
    amrex::Real term2 = (1.0 / dz) * (state.nut[i] / sigma_t - state.nut[i - 1] / sigma_t) * dTdz;
    
    state.temperature[i] += dt * (term1 + term2);
}

// ============================================================================
// update_tke
// Time-step the turbulent kinetic energy
// Simplified 1-equation TKE model matching hrrr_1dsolver_terrain.py
// ============================================================================
void update_tke(SCMState1D& state, int i, amrex::Real dt)
{
    if (i <= state.lower || i >= state.nz - 1) return;
    
    amrex::Real dz = state.dz;
    
    // Diffusion term 1: nut * d²tke/dz²
    amrex::Real term1 = state.nut[i] * (state.tke[i + 1] - 2.0 * state.tke[i] + state.tke[i - 1]) / (dz * dz);
    
    // Diffusion term 2: d(nut)/dz * dtke/dz
    amrex::Real dtkezdz = (state.tke[i] - state.tke[i - 1]) / dz;
    amrex::Real term2 = (1.0 / dz) * (state.nut[i] - state.nut[i - 1]) * dtkezdz;
    
    // Shear production: nut * (du/dz)² + (dv/dz)²
    amrex::Real dudz = (state.ux[i] - state.ux[i - 1]) / dz;
    amrex::Real dvdz = (state.uy[i] - state.uy[i - 1]) / dz;
    amrex::Real production = state.nut[i] * (dudz * dudz + dvdz * dvdz);
    
    // Buoyancy production (simplified, currently zero)
    amrex::Real buoyancy = 0.0;
    
    // Dissipation: Ce * (tke^3/2) / l
    // Simplified with Ce = 1.92 and dimensional analysis
    amrex::Real Ce = 1.92;
    amrex::Real lscale = std::max(state.lscale[i], 0.01);
    amrex::Real dissipation = Ce * std::pow(state.tke[i], 1.5) / lscale;
    
    // Update TKE
    state.tke[i] += dt * (production + buoyancy + term1 + term2 - dissipation);
    state.tke[i] = std::max(state.tke[i], 1.0e-15);
}

// ============================================================================
// run_1d_scm
// Main driver for time-dependent 1D SCM simulation
// Returns final wind speed at reference height
// ============================================================================
amrex::Real run_1d_scm(SCMState1D& state,
                       amrex::Real ref_height,
                       int max_iterations = 10000,
                       amrex::Real convergence_tolerance = 0.01)
{
    // Track convergence
    amrex::Real wind_speed_old = 0.0;
    int iter = 0;
    amrex::Real time_elapsed = 0.0;
    
    while (iter < max_iterations) {
        // Compute surface layer similarity
        compute_similarity_surface(state);
        
        // Update wind field
        for (int i = state.lower + 1; i < state.nz - 1; ++i) {
            update_wind_x(state, i, state.dt);
            update_wind_y(state, i, state.dt);
            update_temperature(state, i, state.dt);
            update_tke(state, i, state.dt);
        }
        
        // Update eddy viscosity
        compute_eddy_viscosity(state);
        
        // Adaptive time-stepping
        amrex::Real max_wind = 0.01;
        for (int i = 0; i < state.nz; ++i) {
            amrex::Real wind = std::sqrt(state.ux[i] * state.ux[i] + state.uy[i] * state.uy[i]);
            max_wind = std::max(max_wind, wind);
        }
        
        state.dt = 0.8 * state.dz / max_wind;
        state.dt = std::min(state.dt, 2.0);  // Max time step
        state.dt = std::max(state.dt, 0.01); // Min time step
        
        time_elapsed += state.dt;
        
        // Check convergence at reference height
        int ref_idx = 0;
        amrex::Real min_dist = std::abs(state.z[0] - ref_height);
        for (int i = 1; i < state.nz; ++i) {
            amrex::Real dist = std::abs(state.z[i] - ref_height);
            if (dist < min_dist) {
                min_dist = dist;
                ref_idx = i;
            }
        }
        
        amrex::Real wind_speed_ref = std::sqrt(state.ux[ref_idx] * state.ux[ref_idx] +
                                               state.uy[ref_idx] * state.uy[ref_idx]);
        
        if (iter > 100) {
            amrex::Real error = std::abs(wind_speed_ref - wind_speed_old);
            if (error < convergence_tolerance) {
                break;
            }
        }
        
        wind_speed_old = wind_speed_ref;
        iter++;
        
        // Safety check: divergence detection
        if (state.dt < 0.001) {
            break;  // Diverged
        }
    }
    
    // Return wind speed at reference height
    int ref_idx = 0;
    amrex::Real min_dist = std::abs(state.z[0] - ref_height);
    for (int i = 1; i < state.nz; ++i) {
        amrex::Real dist = std::abs(state.z[i] - ref_height);
        if (dist < min_dist) {
            min_dist = dist;
            ref_idx = i;
        }
    }
    
    return std::sqrt(state.ux[ref_idx] * state.ux[ref_idx] + state.uy[ref_idx] * state.uy[ref_idx]);
}

// ============================================================================
// find_geostrophic_wind_1d_scm
// Iteratively find geostrophic wind (Ug, Vg) that produces desired wind speed
// at reference height using the 1D SCM
// ============================================================================
std::pair<amrex::Real, amrex::Real>
find_geostrophic_wind_1d_scm(amrex::Real target_wind_speed,
                             amrex::Real wind_direction,
                             amrex::Real ref_height,
                             amrex::Real latitude,
                             amrex::Real z0,
                             amrex::Real domain_height,
                             amrex::Real dz,
                             amrex::Real t_ref,
                             amrex::Real lapse_rate,
                             amrex::Real tolerance = 0.05,
                             int max_iterations = 20)
{
    // Convert direction to components for initial guess
    amrex::Real pi_val = amrex::Real(3.14159265358979323846);
    amrex::Real angle_rad = wind_direction * pi_val / 180.0;
    amrex::Real ug_init = target_wind_speed * std::sin(angle_rad);
    amrex::Real vg_init = target_wind_speed * std::cos(angle_rad);
    
    amrex::Real ug = ug_init;
    amrex::Real vg = vg_init;
    
    for (int iter = 0; iter < max_iterations; ++iter) {
        // Run 1D SCM with current geostrophic wind
        SCMState1D state;
        initialize_1d_state(state, ug, vg, t_ref, lapse_rate, z0, domain_height, dz, latitude);
        state.dt = 0.8 * dz / std::sqrt(ug * ug + vg * vg);
        
        amrex::Real current_speed = run_1d_scm(state, ref_height);
        amrex::Real error = std::abs(current_speed - target_wind_speed);
        
        if (error < tolerance) {
            return std::make_pair(ug, vg);
        }
        
        // Adjust geostrophic wind iteratively
        if (current_speed > 1.0e-10) {
            amrex::Real scale = target_wind_speed / current_speed;
            amrex::Real scale_increment = 0.5 * (scale - 1.0);  // Damped update
            ug = ug * (1.0 + scale_increment);
            vg = vg * (1.0 + scale_increment);
        }
    }
    
    return std::make_pair(ug, vg);
}

} // namespace SCMModels

#endif // SCM_MODELS_H
