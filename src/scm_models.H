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
constexpr amrex::Real RHO_AIR = 1.225;                // kg/m^3 (air density at sea level)
constexpr amrex::Real CP_AIR = 1005.0;                // J/(kg*K) (specific heat of air)

// Stability enumeration
enum class StabilityClass {
    UNSTABLE = -1,     // Unstable (L < 0, free convection dominates)
    NEUTRAL = 0,       // Neutral (|L| > 1000 m or |z/L| < 0.01)
    STABLE = 1         // Stable (L > 0, static stability dominates)
};

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
    std::vector<amrex::Real> Ri;         // Richardson number (stratification indicator)
    
    // Geostrophic wind
    amrex::Real ug;
    amrex::Real vg;
    
    // Surface parameters
    amrex::Real ustar;                   // Friction velocity [m/s]
    amrex::Real Qh;                      // Sensible heat flux [W/m^2]
    amrex::Real mo_length;               // Monin-Obukhov length [m]
    amrex::Real mo_length_prescribed;    // User-specified Monin-Obukhov length (if provided)
    bool use_prescribed_mo_length;       // Whether to use prescribed M-O length
    amrex::Real stability_class;         // Current stability class indicator (-1=unstable, 0=neutral, +1=stable)
    
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
// Stability Analysis Functions
// These functions compute stratification metrics and determine stability class
// ============================================================================

// Compute Richardson number for stratification effects
inline amrex::Real compute_richardson_number(amrex::Real duz_dz,
                                              amrex::Real N2) {
    // Ri = N^2 / (dU/dz)^2
    // N^2 = (g/T) * dT/dz (Brunt-Väisälä frequency squared)
    amrex::Real shear_sq = duz_dz * duz_dz + 1.0e-12;  // Avoid division by zero
    if (shear_sq > 0.0) {
        return N2 / shear_sq;
    }
    return 0.0;
}

// Compute stability class from Monin-Obukhov length
inline StabilityClass get_stability_class(amrex::Real z_over_L) {
    if (z_over_L < -0.01) {
        return StabilityClass::UNSTABLE;
    } else if (z_over_L > 0.01) {
        return StabilityClass::STABLE;
    } else {
        return StabilityClass::NEUTRAL;
    }
}

// Compute stability-dependent Prandtl number
inline amrex::Real compute_prandtl_number(amrex::Real z_over_L) {
    // Based on Högström (1988) and Beljaars & Holtslag (1989)
    if (z_over_L > 0.0) {
        // Stable: Prandtl number increases
        // Pr_t = Pr_t0 * (1 + a * z/L)
        constexpr amrex::Real Pr_t0 = 1.0;  // Neutral value
        constexpr amrex::Real a_pr = 2.0;   // Stability parameter
        return Pr_t0 * (1.0 + a_pr * z_over_L);
    } else if (z_over_L < -0.001) {
        // Unstable: Prandtl number decreases
        // Pr_t = Pr_t0 / (1 + b * |z/L|)
        constexpr amrex::Real Pr_t0 = 1.0;
        constexpr amrex::Real b_pr = 2.0;   // Stability parameter
        return Pr_t0 / (1.0 + b_pr * std::abs(z_over_L));
    } else {
        // Neutral
        return 1.0;
    }
}

// Compute stability-dependent cmu coefficient for TKE model
inline amrex::Real compute_cmu(amrex::Real Ri) {
    // Stability-dependent turbulent kinetic energy coefficient
    // Based on k-epsilon model modifications
    constexpr amrex::Real cmu_neutral = 0.1;
    
    if (Ri > 0.0) {
        // Stable: reduce turbulence production
        amrex::Real dampening = 1.0 / (1.0 + 10.0 * Ri);
        return cmu_neutral * dampening;
    } else if (Ri < 0.0) {
        // Unstable: enhance turbulence production
        amrex::Real enhancement = std::sqrt(1.0 - 5.0 * Ri);
        return cmu_neutral * enhancement;
    } else {
        // Neutral
        return cmu_neutral;
    }
}

// Compute dynamic Monin-Obukhov length from surface fluxes
inline amrex::Real compute_monin_obukhov_length(amrex::Real ustar,
                                                 amrex::Real Qh,
                                                 amrex::Real t_ref,
                                                 amrex::Real kappa = VON_KARMAN) {
    // L = -ρ*cp*T*u*^3 / (κ*g*Qh)
    if (std::abs(Qh) < 1.0e-10) {
        // Neutral condition
        return -1.0e30;
    }
    
    amrex::Real numerator = -RHO_AIR * CP_AIR * t_ref * ustar * ustar * ustar;
    amrex::Real denominator = kappa * GRAVITY * Qh;
    
    if (std::abs(denominator) > 1.0e-12) {
        return numerator / denominator;
    }
    return -1.0e30;
}

// ============================================================================
// Enhanced Mixing Length Models for Different Stability Classes
// ============================================================================

// Compute stability-dependent mixing length
inline amrex::Real compute_mixing_length(amrex::Real z,
                                          amrex::Real z0,
                                          amrex::Real z_over_L,
                                          amrex::Real lmax,
                                          StabilityClass stability) {
    // Shear-based mixing length
    amrex::Real lshear = VON_KARMAN * std::max(z - z0, 0.1);
    
    if (stability == StabilityClass::STABLE) {
        // Holtslag & Boville (1993) stable mixing length
        // l_s = l_m / (1 + a * Ri_b)
        // For z/L parameterization:
        // l_s = l_m * sqrt(1 + a * z/L)
        constexpr amrex::Real a_stable = 5.0;
        amrex::Real factor = std::sqrt(std::max(1.0 + a_stable * z_over_L, 0.1));
        return lshear / factor;
    } else if (stability == StabilityClass::UNSTABLE) {
        // Deardorff (1966) unstable mixing length enhancement
        // For unstable conditions, allow larger mixing lengths
        // l_u = l_m * (1 - c * z/L)^(1/3)
        constexpr amrex::Real c_unstable = 8.0;
        amrex::Real arg = std::max(1.0 - c_unstable * z_over_L, 0.1);
        amrex::Real factor = std::pow(arg, 0.333333);
        return lshear * factor;
    } else {
        // Neutral: standard Blackadar length scale
        amrex::Real inv_l_sq = 1.0 / (lshear * lshear) + 1.0 / (lmax * lmax);
        return 1.0 / std::sqrt(inv_l_sq);
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
    state.Ri.resize(state.nz, 0.0);  // Initialize Richardson number
    
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
    
    // Initialize stability class
    state.stability_class = 0.0;  // Start with neutral
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
    
    // Compute or use prescribed Monin-Obukhov length
    if (!state.use_prescribed_mo_length) {
        // Compute L from dynamic heat flux
        // For now, estimate heat flux from TKE and stability
        // More sophisticated approaches would compute from temperature gradient
        amrex::Real Qh_estimated = 0.0;
        
        if (state.lower + 1 < state.nz) {
            amrex::Real dTdz = (state.temperature[state.lower + 1] - state.temperature[state.lower]) / 
                               std::max(state.z[state.lower + 1] - state.z[state.lower], 0.01);
            
            // Heat flux proportional to vertical temperature gradient and TKE
            // Qh = -ρ*cp*w'*T' ≈ -ρ*cp * K_h * dT/dz
            // where K_h is the thermal diffusivity
            if (state.lower < state.nz) {
                amrex::Real K_h = state.nut[state.lower] / std::max(compute_prandtl_number(0.0), 0.1);
                Qh_estimated = -RHO_AIR * CP_AIR * K_h * dTdz;
            }
        }
        
        state.Qh = Qh_estimated;
        
        // Compute Monin-Obukhov length
        if (state.ustar > 1.0e-10) {
            state.mo_length = compute_monin_obukhov_length(state.ustar, state.Qh, state.t_ref);
        } else {
            state.mo_length = -1.0e30;  // Neutral
        }
    }
    
    // Determine stability class
    amrex::Real z_over_L = 0.0;
    if (state.lower + 1 < state.nz && std::abs(state.mo_length) > 1.0e-10) {
        z_over_L = state.z[state.lower + 1] / state.mo_length;
    }
    StabilityClass stability = get_stability_class(z_over_L);
    state.stability_class = static_cast<amrex::Real>(stability);
    
    // Compute eddy viscosity in surface layer with stability correction
    if (state.lower < state.nz) {
        amrex::Real z0_phi = 1.0;  // Default neutral
        
        // Apply Monin-Obukhov stability correction
        if (std::abs(state.mo_length) > 1.0e-10) {
            amrex::Real z_over_L_z0 = state.z0 / state.mo_length;
            z0_phi = mo_phi_m(z_over_L_z0);
        }
        
        state.nut[state.lower] = state.ustar * VON_KARMAN * state.z0 / z0_phi;
    }
    
    // Update velocities at lowest level using stability-corrected log-law
    if (state.lower < state.nz && state.lower + 1 < state.nz) {
        amrex::Real z0_phi = 1.0;  // Default neutral
        
        // Apply stability correction
        if (std::abs(state.mo_length) > 1.0e-10) {
            amrex::Real z_over_L_z0 = state.z0 / state.mo_length;
            amrex::Real z_over_L_z1 = state.z[state.lower + 1] / state.mo_length;
            z0_phi = mo_phi_m(z_over_L_z0);
            amrex::Real z1_phi = mo_phi_m(z_over_L_z1);
            
            // Stability-corrected log-law: u = (u*/κ) * [ln((z+z0)/z0) - ψ_m(z/L) + ψ_m(z0/L)]
            amrex::Real psi_diff = z0_phi - z1_phi;
            amrex::Real log_term = std::log((state.z[state.lower + 1] + state.z0) / state.z0);
            amrex::Real M0 = state.ustar / VON_KARMAN * (log_term - psi_diff);
            
            if (M1 > 0.0) {
                state.ux[state.lower] = M0 * state.ux[state.lower + 1] / M1;
                state.uy[state.lower] = M0 * state.uy[state.lower + 1] / M1;
            }
        } else {
            // Neutral case (backward compatibility)
            amrex::Real M0 = M1 - state.ustar / VON_KARMAN * z0_phi;
            if (M1 > 0.0) {
                state.ux[state.lower] = M0 * state.ux[state.lower + 1] / M1;
                state.uy[state.lower] = M0 * state.uy[state.lower + 1] / M1;
            }
        }
    }
}

// ============================================================================
// compute_eddy_viscosity
// Compute eddy viscosity using mixing length and TKE
// Updates lscale and nut based on TKE, stratification, and stability class
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
        
        // Compute Brunt-Väisälä frequency squared (N^2)
        amrex::Real dTdz = (i > state.lower) ? 
                           (state.temperature[i] - state.temperature[i - 1]) / dz :
                           0.0;
        amrex::Real N2 = (GRAVITY / state.t_ref) * dTdz;  // Brunt-Väisälä frequency squared
        
        // Compute wind shear
        amrex::Real dudz = (i > state.lower) ?
                           std::sqrt((state.ux[i] - state.ux[i-1])*(state.ux[i] - state.ux[i-1]) +
                                     (state.uy[i] - state.uy[i-1])*(state.uy[i] - state.uy[i-1])) / dz :
                           0.0;
        
        // Compute Richardson number
        state.Ri[i] = compute_richardson_number(dudz, N2);
        
        // Determine local stability
        amrex::Real z_over_L = (std::abs(state.mo_length) > 1.0e-10) ? 
                               state.z[i] / state.mo_length : 0.0;
        StabilityClass stability = get_stability_class(z_over_L);
        
        // Compute stability-dependent mixing length
        amrex::Real lscale = compute_mixing_length(state.z[i], state.z[state.lower],
                                                    z_over_L, lmax, stability);
        
        state.lscale[i] = lscale;
        
        // Compute stability-dependent cmu coefficient
        amrex::Real cmu = compute_cmu(state.Ri[i]);
        
        // Eddy viscosity: nut = cmu * sqrt(tke) * lscale
        state.nut[i] = cmu * std::sqrt(std::max(state.tke[i], 1.0e-15)) * lscale;
        state.nut[i] = std::max(state.nut[i], 1.0e-5);
    }
    
    // Boundary conditions at top
    if (state.nz > 1) {
        state.nut[state.nz - 1] = state.nut[state.nz - 2];
        state.tke[state.nz - 1] = state.tke[state.nz - 2];
        state.lscale[state.nz - 1] = state.lscale[state.nz - 2];
        if (state.Ri.size() > state.nz - 1) {
            state.Ri[state.nz - 1] = state.Ri[state.nz - 2];
        }
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
// Includes stability-dependent Prandtl number for better physics
// ============================================================================
void update_temperature(SCMState1D& state, int i, amrex::Real dt)
{
    if (i <= state.lower || i >= state.nz - 1) return;
    
    amrex::Real dz = state.dz;
    
    // Compute stability-dependent Prandtl number
    amrex::Real z_over_L = (std::abs(state.mo_length) > 1.0e-10) ? 
                           state.z[i] / state.mo_length : 0.0;
    amrex::Real sigma_t = compute_prandtl_number(z_over_L);
    
    // term1: (nut/sigma_t) * d²T/dz²
    amrex::Real term1 = (state.nut[i] / sigma_t) * 
                        (state.temperature[i + 1] - 2.0 * state.temperature[i] + state.temperature[i - 1]) / (dz * dz);
    
    // term2: d/dz(nut/sigma_t) * dT/dz
    amrex::Real dTdz = (state.temperature[i] - state.temperature[i - 1]) / dz;
    amrex::Real term2 = (1.0 / dz) * (state.nut[i] / sigma_t - state.nut[i - 1] / compute_prandtl_number(z_over_L - dz / state.mo_length)) * dTdz;
    
    state.temperature[i] += dt * (term1 + term2);
}

// ============================================================================
// update_tke
// Time-step the turbulent kinetic energy
// 1-equation TKE model with stability-dependent buoyancy production
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
    
    // Buoyancy production: -g/T * (nut/sigma_t) * dT/dz
    // This term can be positive (destabilizing in unstable) or negative (stabilizing in stable)
    amrex::Real buoyancy = 0.0;
    if (i > state.lower) {
        amrex::Real dTdz = (state.temperature[i] - state.temperature[i - 1]) / dz;
        amrex::Real z_over_L = (std::abs(state.mo_length) > 1.0e-10) ? 
                               state.z[i] / state.mo_length : 0.0;
        amrex::Real sigma_t = compute_prandtl_number(z_over_L);
        amrex::Real K_h = state.nut[i] / std::max(sigma_t, 0.1);
        buoyancy = -(GRAVITY / state.t_ref) * K_h * dTdz;
    }
    
    // Dissipation: Ce * (tke^3/2) / l
    // Stability-dependent dissipation rate
    amrex::Real Ce = 1.92;
    amrex::Real lscale = std::max(state.lscale[i], 0.01);
    amrex::Real dissipation = Ce * std::pow(std::max(state.tke[i], 1.0e-15), 1.5) / lscale;
    
    // Update TKE: includes production (shear + buoyancy), diffusion, and dissipation
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
