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
#include <AMReX_ParmParse.H>
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
constexpr amrex::Real MO_LENGTH_NEUTRAL_SENTINEL = -1.0e30;  // Sentinel value for neutral M-O conditions
constexpr amrex::Real MIN_STABILITY_FACTOR = 0.1;     // Minimum stability correction factor
constexpr amrex::Real MIN_TKE = 1.0e-15;              // Minimum turbulent kinetic energy threshold
constexpr amrex::Real ONE_THIRD = 1.0 / 3.0;          // Cube root exponent

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
    
    // Moisture and extra thermodynamic fields
    std::vector<amrex::Real> qv;         // Water vapor mixing ratio [kg/kg]
    std::vector<amrex::Real> qc;         // Cloud water mixing ratio [kg/kg]
    std::vector<amrex::Real> qr;         // Rain water mixing ratio [kg/kg]
    std::vector<amrex::Real> theta;      // Potential temperature [K]
    std::vector<amrex::Real> pressure;   // Pressure [Pa]
    
    // Turbulence model parameters
    std::string turbulence_model;        // "one_equation", "ysu", "myj"
    bool enable_microphysics;            // Enable saturation adjustment / microphysics
    amrex::Real initial_humidity;        // Initial relative humidity [0.0 - 1.0]
    amrex::Real pbl_height;              // Planetary boundary layer height [m] (computed)
    
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

    // New fields for MYJ and non-canonical/entrainment physics
    std::vector<amrex::Real> nuh;        // Eddy diffusivity for scalars [m^2/s]
    amrex::Real cooling_rate;            // Surface cooling rate [K/hr]
    bool use_charnock;                  // Enable Charnock overwater roughness
    amrex::Real subsidence_rate;         // Subsidence rate [m/s]
    amrex::Real radiation_cooling_rate;  // Radiative cooling rate [K/hr]
    amrex::Real cloud_top_cooling_rate;  // Cloud-top radiative cooling rate [K/hr]
    std::vector<amrex::Real> w_subsidence; // Vertical subsidence velocity [m/s]
    amrex::Real inversion_strength;      // Inversion potential temperature jump [K]
    amrex::Real inversion_thickness;     // Inversion layer thickness [m]
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

// Extra thermodynamic/microphysics physical constants and functions
constexpr amrex::Real Rd = 287.05;                    // J/(kg*K)
constexpr amrex::Real Cp = 1004.0;                    // J/(kg*K)
constexpr amrex::Real Lv = 2.5e6;                     // J/kg (latent heat of vaporization)

inline amrex::Real compute_es(amrex::Real T) {
    amrex::Real Tc = T - 273.15;
    return 610.78 * std::exp(17.27 * Tc / (Tc + 237.3));
}

inline amrex::Real compute_qvs(amrex::Real T, amrex::Real p) {
    amrex::Real es = compute_es(T);
    return 0.622 * es / std::max(p - es, 1.0e-5);
}

inline void apply_saturation_adjustment(SCMState1D& state) {
    constexpr amrex::Real Rv = 461.5;
    for (int i = 0; i < state.nz; ++i) {
        amrex::Real T = state.temperature[i];
        amrex::Real p = state.pressure[i];
        amrex::Real qv = state.qv[i];
        amrex::Real qc = state.qc[i];
        
        amrex::Real qvs = compute_qvs(T, p);
        amrex::Real dq = qv - qvs;
        
        if (dq > 0.0 || (dq < 0.0 && qc > 0.0)) {
            // Clausius-Clapeyron slope
            amrex::Real dqvs_dT = qvs * (Lv / (Rv * T * T));
            amrex::Real denom = 1.0 + (Lv / Cp) * dqvs_dT;
            amrex::Real dqc = dq / denom;
            
            // Cannot evaporate more than available cloud water
            if (dqc < -qc) {
                dqc = -qc;
            }
            
            state.qv[i] -= dqc;
            state.qc[i] += dqc;
            state.temperature[i] += (Lv / Cp) * dqc;
        }
    }
}

inline void apply_kessler_microphysics(SCMState1D& state, amrex::Real dt) {
    // Kessler microphysics parameters:
    // qc_threshold: Cloud water mixing ratio threshold for autoconversion to rain [kg/kg] (typical range 1.0e-4 to 1.0e-3)
    // k_auto: Autoconversion rate constant [1/s] (typical value 1.0e-3)
    // k_accr: Accretion rate constant [1/s] (typical value 1.0)
    constexpr amrex::Real qc_threshold = 1.0e-4;
    constexpr amrex::Real k_auto = 1.0e-3;
    constexpr amrex::Real k_accr = 1.0;
    
    for (int i = 0; i < state.nz; ++i) {
        amrex::Real qc = state.qc[i];
        amrex::Real qr = state.qr[i];
        
        amrex::Real auto_conv = 0.0;
        if (qc > qc_threshold) {
            auto_conv = k_auto * (qc - qc_threshold);
        }
        
        amrex::Real accr = k_accr * qc * qr;
        
        amrex::Real total_conversion = (auto_conv + accr) * dt;
        if (total_conversion > qc) {
            total_conversion = qc;
        }
        
        state.qc[i] -= total_conversion;
        state.qr[i] += total_conversion;
    }
}

// ============================================================================
// Stability Analysis Functions
// These functions compute stratification metrics and determine stability class
// ============================================================================

// Compute Richardson number for stratification effects
inline amrex::Real compute_richardson_number(amrex::Real shear,
                                              amrex::Real N2) {
    // Ri = N^2 / (dU/dz)^2
    // N^2 = (g/T) * dT/dz (Brunt-Väisälä frequency squared)
    amrex::Real shear_sq = shear * shear + 1.0e-12;  // Avoid division by zero
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
        return MO_LENGTH_NEUTRAL_SENTINEL;
    }
    
    amrex::Real numerator = -RHO_AIR * CP_AIR * t_ref * std::pow(ustar, 3);
    amrex::Real denominator = kappa * GRAVITY * Qh;
    
    if (std::abs(denominator) > 1.0e-12) {
        return numerator / denominator;
    }
    return MO_LENGTH_NEUTRAL_SENTINEL;
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
    amrex::Real lshear = VON_KARMAN * std::max(z - z0, MIN_STABILITY_FACTOR);
    
    if (stability == StabilityClass::STABLE) {
        // Holtslag & Boville (1993) stable mixing length
        // l_s = l_m / (1 + a * Ri_b)
        // For z/L parameterization:
        // l_s = l_m * sqrt(1 + a * z/L)
        constexpr amrex::Real a_stable = 5.0;
        amrex::Real factor = std::sqrt(std::max(1.0 + a_stable * z_over_L, MIN_STABILITY_FACTOR));
        return lshear / factor;
    } else if (stability == StabilityClass::UNSTABLE) {
        // Deardorff (1966) unstable mixing length enhancement
        // For unstable conditions, allow larger mixing lengths
        // l_u = l_m * (1 - c * z/L)^(1/3)
        constexpr amrex::Real c_unstable = 8.0;
        amrex::Real arg = std::max(1.0 - c_unstable * z_over_L, MIN_STABILITY_FACTOR);
        amrex::Real factor = std::pow(arg, ONE_THIRD);
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
                         amrex::Real mo_length_prescribed = -1.0e30,
                         const std::string& turb_model = "one_equation",
                         bool enable_mp = false,
                         amrex::Real init_rh = 0.0)
{
    state.ug = ug;
    state.vg = vg;
    state.t_ref = t_ref;
    state.lapse_rate = lapse_rate;
    state.z0 = z0;
    state.dz = dz;
    state.z_upper = domain_height;
    state.latitude = latitude;
    state.turbulence_model = turb_model;
    state.enable_microphysics = enable_mp;
    state.initial_humidity = init_rh;
    state.pbl_height = 0.0;
    
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
    
    // Hydrostatic pressure profile
    state.pressure.resize(state.nz);
    state.pressure[0] = 101325.0; // Pa
    for (int i = 1; i < state.nz; ++i) {
        amrex::Real T_avg = 0.5 * (state.temperature[i] + state.temperature[i-1]);
        amrex::Real dz_step = state.z[i] - state.z[i-1];
        state.pressure[i] = state.pressure[i-1] * std::exp(-GRAVITY * dz_step / (Rd * T_avg));
    }
    
    // Potential temperature (theta)
    state.theta.resize(state.nz);
    for (int i = 0; i < state.nz; ++i) {
        state.theta[i] = state.temperature[i] * std::pow(101325.0 / state.pressure[i], Rd / Cp);
    }
    
    // Moisture variables
    state.qv.resize(state.nz);
    state.qc.resize(state.nz, 0.0);
    state.qr.resize(state.nz, 0.0);
    for (int i = 0; i < state.nz; ++i) {
        amrex::Real qvs = compute_qvs(state.temperature[i], state.pressure[i]);
        state.qv[i] = state.initial_humidity * qvs;
    }
    
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
        state.mo_length = MO_LENGTH_NEUTRAL_SENTINEL;
    }
    
    // Initialize stability class
    state.stability_class = 0.0;  // Start with neutral

    // Initialize new fields
    state.nuh.resize(state.nz, 1.0e-5);
    state.cooling_rate = 0.0;
    state.use_charnock = false;
    state.subsidence_rate = 0.0;
    state.radiation_cooling_rate = 0.0;
    state.cloud_top_cooling_rate = 0.0;
    state.inversion_strength = 0.0;
    state.inversion_thickness = 0.0;

    // Use ParmParse to query from "scm" namespace if present
    amrex::ParmParse pp("scm");
    pp.query("cooling_rate", state.cooling_rate);
    pp.query("use_charnock", state.use_charnock);
    pp.query("subsidence_rate", state.subsidence_rate);
    pp.query("radiation_cooling_rate", state.radiation_cooling_rate);
    pp.query("cloud_top_cooling_rate", state.cloud_top_cooling_rate);

    state.w_subsidence.resize(state.nz, 0.0);
    if (std::abs(state.subsidence_rate) > 1.0e-10) {
        for (int i = 0; i < state.nz; ++i) {
            // Linear subsidence profile: 0 at surface, max at domain top
            state.w_subsidence[i] = - state.subsidence_rate * (state.z[i] / domain_height);
        }
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
    
    // Apply surface temperature cooling rate if specified
    if (std::abs(state.cooling_rate) > 1.0e-10) {
        state.temperature[state.lower] -= (state.cooling_rate / 3600.0) * state.dt;
    }

    amrex::Real M1 = std::sqrt(state.ux[state.lower + 1] * state.ux[state.lower + 1] +
                               state.uy[state.lower + 1] * state.uy[state.lower + 1]);
    
    // Compute friction velocity and optionally Charnock overwater roughness length
    if (state.use_charnock) {
        for (int iter = 0; iter < 5; ++iter) {
            if (M1 > 0.0) {
                state.ustar = VON_KARMAN * M1 / std::log((state.z[state.lower + 1] + state.z0) / state.z0);
            } else {
                state.ustar = 0.0;
            }
            state.z0 = std::max(1.0e-4, 0.0185 * state.ustar * state.ustar / GRAVITY);
        }
    } else {
        if (M1 > 0.0) {
            state.ustar = VON_KARMAN * M1 / std::log((state.z[state.lower + 1] + state.z0) / state.z0);
        } else {
            state.ustar = 0.0;
        }
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
            // Use neutral Prandtl number (1.0) for initial heat flux estimate
            // since final M-O length hasn't been computed yet (bootstrap)
            if (state.lower < state.nz) {
                amrex::Real K_h = state.nut[state.lower] / std::max(1.0, MIN_STABILITY_FACTOR);
                Qh_estimated = -RHO_AIR * CP_AIR * K_h * dTdz;
            }
        }
        
        state.Qh = Qh_estimated;
        
        // Compute Monin-Obukhov length
        if (state.ustar > 1.0e-10) {
            state.mo_length = compute_monin_obukhov_length(state.ustar, state.Qh, state.t_ref);
        } else {
            state.mo_length = MO_LENGTH_NEUTRAL_SENTINEL;
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
    
    if (state.turbulence_model == "ysu") {
        // 1. Compute PBL height using bulk Richardson number (threshold Ri_cr = 0.25)
        amrex::Real h = 100.0; // Minimum PBL height
        amrex::Real theta_s = state.theta[state.lower];
        amrex::Real u_s = state.ux[state.lower];
        amrex::Real v_s = state.uy[state.lower];
        
        // Compute convective velocity scale ws and thermal perturbation theta_T if Qh > 0
        amrex::Real w_theta_0 = 0.0;
        amrex::Real w_star = 0.0;
        amrex::Real theta_T = 0.0;
        if (state.Qh > 0.0) {
            w_theta_0 = state.Qh / (RHO_AIR * CP_AIR);
            // Estimate a preliminary h for w_star bootstrap (using old h if available, or 500m)
            amrex::Real h_est = (state.pbl_height > 100.0) ? state.pbl_height : 500.0;
            w_star = std::pow(std::max(0.0, (GRAVITY / state.t_ref) * w_theta_0 * h_est), ONE_THIRD);
            theta_T = 7.8 * w_theta_0 / std::max(w_star, 0.01);
        }
        
        for (int i = state.lower + 1; i < state.nz; ++i) {
            amrex::Real du = state.ux[i] - u_s;
            amrex::Real dv = state.uy[i] - v_s;
            amrex::Real wind_sq = std::max(du*du + dv*dv, 0.01);
            // Add thermal perturbation under unstable conditions
            amrex::Real theta_s_eff = theta_s + theta_T;
            amrex::Real dtheta = state.theta[i] - theta_s_eff;
            amrex::Real Ri_b = (GRAVITY / theta_s) * dtheta * (state.z[i] - state.z[state.lower]) / wind_sq;
            
            if (Ri_b >= 0.25) {
                h = state.z[i];
                break;
            }
        }
        state.pbl_height = h;
        
        // 2. Compute velocity scale ws
        amrex::Real ws = state.ustar;
        if (state.use_prescribed_mo_length && std::abs(state.mo_length) > 1.0e-10) {
            amrex::Real zeta = (0.1 * h) / state.mo_length;
            ws = state.ustar / mo_phi_m(zeta);
        } else if (state.Qh > 0.0) {
            // Unstable convective scale
            ws = state.ustar + 0.6 * w_star;
        }
        
        // 3. Compute eddy viscosity profile
        for (int i = state.lower + 1; i < state.nz - 1; ++i) {
            amrex::Real z = state.z[i];
            if (z < h) {
                state.nut[i] = VON_KARMAN * ws * z * std::pow(1.0 - z/h, 2.0);
            } else {
                // Dynamic local diffusion above PBL (e.g., Louis/Richardson-dependent scheme)
                amrex::Real lshear = VON_KARMAN * state.z[i];
                amrex::Real lmax = 100.0;
                amrex::Real lscale = 1.0 / std::sqrt(1.0 / (lshear * lshear) + 1.0 / (lmax * lmax));
                
                amrex::Real dudz = (state.ux[i + 1] - state.ux[i - 1]) / (2.0 * dz);
                amrex::Real dvdz = (state.uy[i + 1] - state.uy[i - 1]) / (2.0 * dz);
                amrex::Real dTdz = (state.temperature[i + 1] - state.temperature[i - 1]) / (2.0 * dz);
                amrex::Real shear = std::sqrt(std::max(dudz * dudz + dvdz * dvdz, 1.0e-8));
                amrex::Real N_sq = (GRAVITY / state.t_ref) * dTdz;
                amrex::Real local_Ri = N_sq / (shear * shear);
                
                amrex::Real stability_func = 1.0;
                if (local_Ri >= 0.0) {
                    stability_func = std::pow(std::max(0.0, 1.0 - local_Ri / 0.25), 2.0);
                } else {
                    stability_func = std::sqrt(1.0 - 16.0 * local_Ri);
                }
                state.nut[i] = lscale * lscale * shear * stability_func;
            }
            state.nut[i] = std::max(state.nut[i], 1.0e-5);
            
            // Set eddy diffusivity for scalars
            amrex::Real z_over_L = (std::abs(state.mo_length) > 1.0e-10) ? 
                                  state.z[i] / state.mo_length : 0.0;
            amrex::Real sigma_t = compute_prandtl_number(z_over_L);
            state.nuh[i] = state.nut[i] / sigma_t;
            state.nuh[i] = std::max(state.nuh[i], 1.0e-5);
        }
    }
    else if (state.turbulence_model == "myj") {
        // Mellor-Yamada-Janjić (Level 2.5) local closure
        constexpr amrex::Real A1 = 0.92;
        constexpr amrex::Real A2 = 0.74;
        constexpr amrex::Real B1 = 16.6;
        constexpr amrex::Real B2 = 10.1;
        constexpr amrex::Real C1 = 0.08;
        
        // Compute Blackadar length scale
        for (int i = state.lower + 1; i < state.nz - 1; ++i) {
            amrex::Real lshear = VON_KARMAN * (state.z[i] - state.z[state.lower]);
            amrex::Real fg = std::sqrt(state.ug * state.ug + state.vg * state.vg);
            amrex::Real lmax = (std::abs(state.coriolis) > 1.0e-8) 
                                 ? 0.00027 * fg / std::abs(state.coriolis)
                                 : 1000.0;
            amrex::Real inv_l_sq = 1.0 / (lshear * lshear) + 1.0 / (lmax * lmax);
            amrex::Real lscale = 1.0 / std::sqrt(inv_l_sq);
            
            // Compute shear and buoyancy parameters Gm and Gh
            amrex::Real dudz = (state.ux[i + 1] - state.ux[i - 1]) / (2.0 * dz);
            amrex::Real dvdz = (state.uy[i + 1] - state.uy[i - 1]) / (2.0 * dz);
            amrex::Real dTdz = (state.temperature[i + 1] - state.temperature[i - 1]) / (2.0 * dz);
            
            amrex::Real shear_sq = dudz * dudz + dvdz * dvdz;
            amrex::Real N_sq = (GRAVITY / state.t_ref) * dTdz;
            
            amrex::Real q = std::sqrt(std::max(2.0 * state.tke[i], 1.0e-5));
            amrex::Real lscale_val = lscale;
            
            // Apply stable buoyancy limit (Janjić 1990)
            if (N_sq > 0.0) {
                amrex::Real N_freq = std::sqrt(N_sq);
                amrex::Real l_buoy = 0.53 * q / N_freq;
                lscale_val = std::min(lscale_val, l_buoy);
            }
            // Background minimum mixing length (prevent entrainment pinch-off)
            lscale_val = std::max(lscale_val, 1.0);
            state.lscale[i] = lscale_val;
            amrex::Real lscale_clamped = lscale_val;
            
            amrex::Real Gm = (lscale_clamped * lscale_clamped / (q * q)) * shear_sq;
            amrex::Real Gh = - (lscale_clamped * lscale_clamped / (q * q)) * N_sq;
            
            // Clamping Gh to avoid Mellor-Yamada singularities
            amrex::Real Gh_clamped = std::max(-0.28, std::min(Gh, 0.028));
            
            // Solve for Sm, Sh
            amrex::Real den = 1.0 - 3.0 * A2 * Gh_clamped * (6.0 * A1 + B2) 
                                  - 9.0 * A1 * A2 * Gh_clamped * (1.0 - 6.0 * A1 / B1);
            amrex::Real Sh = 0.49;
            amrex::Real Sm = 0.39;
            if (std::abs(den) > 1.0e-5) {
                Sh = A2 * (1.0 - 6.0 * A1 / B1) / den;
                Sm = (A1 * (1.0 - 3.0 * C1 - 6.0 * A1 / B1) + 9.0 * A1 * A2 * Gh_clamped * Sh) / den;
            }
            
            Sm = std::max(0.01, std::min(Sm, 1.0));
            Sh = std::max(0.01, std::min(Sh, 1.0));
            
            state.nut[i] = lscale_clamped * q * Sm;
            state.nut[i] = std::max(state.nut[i], 1.0e-5);
            
            state.nuh[i] = lscale_clamped * q * Sh;
            state.nuh[i] = std::max(state.nuh[i], 1.0e-5);
        }
        
        // Compute PBL height for MYJ using TKE threshold (e.g., TKE = 0.01 m^2/s^2)
        amrex::Real h = 100.0;
        for (int i = state.lower + 1; i < state.nz; ++i) {
            if (state.tke[i] < 0.01) {
                h = state.z[i];
                break;
            }
        }
        state.pbl_height = h;
    }
    else {
        // Original "one_equation" model (with advanced stability physics)
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
            state.nut[i] = cmu * std::sqrt(std::max(state.tke[i], MIN_TKE)) * lscale;
            state.nut[i] = std::max(state.nut[i], 1.0e-5);
            
            // Set eddy diffusivity for scalars
            amrex::Real sigma_t = compute_prandtl_number(z_over_L);
            state.nuh[i] = state.nut[i] / sigma_t;
            state.nuh[i] = std::max(state.nuh[i], 1.0e-5);
        }
    }
    
    // Boundary conditions at top
    if (state.nz > 1) {
        state.nut[state.nz - 1] = state.nut[state.nz - 2];
        state.nuh[state.nz - 1] = state.nuh[state.nz - 2];
        state.tke[state.nz - 1] = state.tke[state.nz - 2];
        state.lscale[state.nz - 1] = state.lscale[state.nz - 2];
        state.Ri[state.nz - 1] = state.Ri[state.nz - 2];
    }
    
    // Inversion tracking diagnostics for any turbulence model
    {
        int k_inv = state.lower + 1;
        for (int k = state.lower + 1; k < state.nz; ++k) {
            if (state.z[k] >= state.pbl_height) {
                k_inv = k;
                break;
            }
        }
        int k1 = std::max(state.lower, k_inv - 2);
        int k2 = std::min(state.nz - 1, k_inv + 2);
        state.inversion_strength = state.theta[k2] - state.theta[k1];
        state.inversion_thickness = state.z[k2] - state.z[k1];
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

    // Subsidence vertical advection (upwind since w_subsidence <= 0)
    if (!state.w_subsidence.empty() && state.w_subsidence[i] < 0.0) {
        state.ux[i] -= dt * state.w_subsidence[i] * (state.ux[i + 1] - state.ux[i]) / dz;
    }
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

    // Subsidence vertical advection (upwind since w_subsidence <= 0)
    if (!state.w_subsidence.empty() && state.w_subsidence[i] < 0.0) {
        state.uy[i] -= dt * state.w_subsidence[i] * (state.uy[i + 1] - state.uy[i]) / dz;
    }
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
    
    // Get diffusivities at interfaces
    amrex::Real Kh_ip = 0.5 * (state.nuh[i] + state.nuh[i + 1]);
    amrex::Real Kh_im = 0.5 * (state.nuh[i] + state.nuh[i - 1]);
    
    // YSU non-local counter-gradient term
    amrex::Real gamma_h_ip = 0.0;
    amrex::Real gamma_h_im = 0.0;
    if (state.turbulence_model == "ysu" && state.Qh > 0.0) {
        amrex::Real h = state.pbl_height;
        if (state.z[i] < h) {
            amrex::Real w_theta_0 = state.Qh / (RHO_AIR * CP_AIR);
            amrex::Real w_theta_h = -0.2 * w_theta_0;
            // ws convective scale
            amrex::Real ws = state.ustar;
            if (state.use_prescribed_mo_length && std::abs(state.mo_length) > 1.0e-10) {
                amrex::Real zeta = (0.1 * h) / state.mo_length;
                ws = state.ustar / mo_phi_m(zeta);
            }
            gamma_h_ip = 10.0 * (w_theta_0 + w_theta_h) / (std::max(ws, 0.01) * h);
            gamma_h_im = gamma_h_ip;
        }
    }
    
    // Discretize flux at interfaces (flux = -Kh * (dT/dz - gamma))
    amrex::Real flux_ip = -Kh_ip * ((state.temperature[i + 1] - state.temperature[i]) / dz - gamma_h_ip);
    amrex::Real flux_im = -Kh_im * ((state.temperature[i] - state.temperature[i - 1]) / dz - gamma_h_im);
    
    // If we have prescribed sensible heat flux, use it as boundary condition at lowest active level
    if (i == state.lower + 1 && std::abs(state.Qh) > 1.0e-10) {
        flux_im = - state.Qh / (RHO_AIR * CP_AIR);
    }
    
    // Temperature change due to vertical diffusion
    amrex::Real dT_dt = -(flux_ip - flux_im) / dz;
    
    // Subsidence vertical advection (upwind since w_subsidence <= 0)
    if (!state.w_subsidence.empty() && state.w_subsidence[i] < 0.0) {
        dT_dt -= state.w_subsidence[i] * (state.temperature[i + 1] - state.temperature[i]) / dz;
    }
    
    // Radiative cooling
    if (state.radiation_cooling_rate > 0.0) {
        dT_dt -= state.radiation_cooling_rate / 3600.0;
    }
    
    // Cloud-top radiative cooling (applied dynamically at highest level where qc > 1e-5)
    if (state.cloud_top_cooling_rate > 0.0) {
        int k_ct = -1;
        for (int k = state.nz - 2; k > state.lower; --k) {
            if (state.qc[k] > 1.0e-5) {
                k_ct = k;
                break;
            }
        }
        if (i == k_ct) {
            dT_dt -= state.cloud_top_cooling_rate / 3600.0;
        }
    }
    
    state.temperature[i] += dt * dT_dt;
}

// ============================================================================
// update_moisture
// Time-step the moisture variables
// ============================================================================
void update_moisture(SCMState1D& state, int i, amrex::Real dt)
{
    if (i <= state.lower || i >= state.nz - 1) return;
    
    amrex::Real dz = state.dz;
    amrex::Real Sc = 1.0; // Schmidt number for moisture
    
    // Eddy diffusivity at interfaces
    amrex::Real Kq_ip = 0.5 * (state.nuh[i] / Sc + state.nuh[i + 1] / Sc);
    amrex::Real Kq_im = 0.5 * (state.nuh[i] / Sc + state.nuh[i - 1] / Sc);
    
    // Counter-gradient term for moisture in YSU
    amrex::Real gamma_q_ip = 0.0;
    amrex::Real gamma_q_im = 0.0;
    if (state.turbulence_model == "ysu" && state.Qh > 0.0) {
        amrex::Real h = state.pbl_height;
        if (state.z[i] < h) {
            // Find inversion jump
            int k_inv = state.lower + 1;
            for (int k = state.lower + 1; k < state.nz; ++k) {
                if (state.z[k] >= h) {
                    k_inv = k;
                    break;
                }
            }
            amrex::Real dtheta = state.theta[std::min(k_inv + 1, state.nz - 1)] - state.theta[std::max(k_inv - 1, 0)];
            amrex::Real dqv = state.qv[std::min(k_inv + 1, state.nz - 1)] - state.qv[std::max(k_inv - 1, 0)];
            amrex::Real w_theta_0 = state.Qh / (RHO_AIR * CP_AIR);
            amrex::Real w_e = 0.2 * w_theta_0 / std::max(dtheta, 0.01);
            w_e = std::min(w_e, 0.2); // clamp
            amrex::Real w_qv_h = -w_e * dqv;
            
            amrex::Real ws = state.ustar;
            if (state.use_prescribed_mo_length && std::abs(state.mo_length) > 1.0e-10) {
                amrex::Real zeta = (0.1 * h) / state.mo_length;
                ws = state.ustar / mo_phi_m(zeta);
            }
            gamma_q_ip = 10.0 * w_qv_h / (std::max(ws, 0.01) * h);
            gamma_q_im = gamma_q_ip;
        }
    }
    
    // Update qv
    {
        amrex::Real flux_ip = -Kq_ip * ((state.qv[i + 1] - state.qv[i]) / dz - gamma_q_ip);
        amrex::Real flux_im = -Kq_im * ((state.qv[i] - state.qv[i - 1]) / dz - gamma_q_im);
        amrex::Real dqv_dt = -(flux_ip - flux_im) / dz;
        if (!state.w_subsidence.empty() && state.w_subsidence[i] < 0.0) {
            dqv_dt -= state.w_subsidence[i] * (state.qv[i + 1] - state.qv[i]) / dz;
        }
        state.qv[i] += dt * dqv_dt;
    }
    
    // Update qc
    {
        amrex::Real flux_ip = -Kq_ip * ((state.qc[i + 1] - state.qc[i]) / dz);
        amrex::Real flux_im = -Kq_im * ((state.qc[i] - state.qc[i - 1]) / dz);
        amrex::Real dqc_dt = -(flux_ip - flux_im) / dz;
        if (!state.w_subsidence.empty() && state.w_subsidence[i] < 0.0) {
            dqc_dt -= state.w_subsidence[i] * (state.qc[i + 1] - state.qc[i]) / dz;
        }
        state.qc[i] += dt * dqc_dt;
    }

    // Update qr
    {
        amrex::Real flux_ip = -Kq_ip * ((state.qr[i + 1] - state.qr[i]) / dz);
        amrex::Real flux_im = -Kq_im * ((state.qr[i] - state.qr[i - 1]) / dz);
        amrex::Real dqr_dt = -(flux_ip - flux_im) / dz;
        if (!state.w_subsidence.empty() && state.w_subsidence[i] < 0.0) {
            dqr_dt -= state.w_subsidence[i] * (state.qr[i + 1] - state.qr[i]) / dz;
        }
        state.qr[i] += dt * dqr_dt;
    }
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
    
    // Buoyancy production
    amrex::Real buoyancy = 0.0;
    if (state.turbulence_model == "myj") {
        // MYJ uses standard buoyancy with Central Difference dTdz
        amrex::Real dTdz = (state.temperature[i + 1] - state.temperature[i - 1]) / (2.0 * dz);
        buoyancy = - state.nut[i] * (GRAVITY / state.t_ref) * dTdz;
    } else {
        // "one_equation" and "ysu" use the stability-dependent buoyancy
        if (i > state.lower) {
            amrex::Real dTdz = (state.temperature[i] - state.temperature[i - 1]) / dz;
            amrex::Real z_over_L = (std::abs(state.mo_length) > 1.0e-10) ? 
                                   state.z[i] / state.mo_length : 0.0;
            amrex::Real sigma_t = compute_prandtl_number(z_over_L);
            amrex::Real K_h = state.nut[i] / std::max(sigma_t, MIN_STABILITY_FACTOR);
            buoyancy = -(GRAVITY / state.t_ref) * K_h * dTdz;
        }
    }
    
    // Dissipation
    amrex::Real dissipation = 0.0;
    if (state.turbulence_model == "myj") {
        constexpr amrex::Real B1 = 16.6;
        amrex::Real lscale = std::max(state.lscale[i], 0.01);
        amrex::Real q = std::sqrt(std::max(2.0 * state.tke[i], 1.0e-8));
        dissipation = (q * q * q) / (B1 * lscale);
        // Note: The standard Mellor-Yamada prognostic equation is solved for q^2 (twice the TKE).
        // Since this SCM solver integrates the equation directly for TKE (e = q^2 / 2),
        // we multiply the q^2 dissipation rate epsilon = q^3 / (B1 * l) by 0.5 to convert it
        // explicitly to the correct TKE dissipation rate.
        dissipation *= 0.5;
    } else {
        amrex::Real Ce = 1.92;
        amrex::Real lscale = std::max(state.lscale[i], MIN_STABILITY_FACTOR);
        dissipation = Ce * std::pow(std::max(state.tke[i], MIN_TKE), 1.5) / lscale;
    }
    
    // Update TKE: includes production (shear + buoyancy), diffusion, and dissipation
    state.tke[i] += dt * (production + buoyancy + term1 + term2 - dissipation);
    state.tke[i] = std::max(state.tke[i], MIN_TKE);
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
            if (state.enable_microphysics) {
                update_moisture(state, i, state.dt);
            }
            if (state.turbulence_model != "ysu") {
                update_tke(state, i, state.dt);
            }
        }
        
        // Apply microphysics / saturation adjustment
        if (state.enable_microphysics) {
            apply_saturation_adjustment(state);
            apply_kessler_microphysics(state, state.dt);
        }
        
        // Update potential temperature and pressure hydrostatically
        // (to keep variables in framework consistent as ABL evolves)
        for (int i = 1; i < state.nz; ++i) {
            amrex::Real T_avg = 0.5 * (state.temperature[i] + state.temperature[i-1]);
            amrex::Real dz_step = state.z[i] - state.z[i-1];
            state.pressure[i] = state.pressure[i-1] * std::exp(-GRAVITY * dz_step / (Rd * T_avg));
        }
        for (int i = 0; i < state.nz; ++i) {
            state.theta[i] = state.temperature[i] * std::pow(101325.0 / state.pressure[i], Rd / Cp);
        }
        
        // Apply zero-gradient boundary conditions for moisture at top boundary
        if (state.nz > 1 && state.enable_microphysics) {
            state.qv[state.nz - 1] = state.qv[state.nz - 2];
            state.qc[state.nz - 1] = state.qc[state.nz - 2];
            state.qr[state.nz - 1] = state.qr[state.nz - 2];
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
                             int max_iterations = 20,
                             const std::string& turb_model = "one_equation",
                             bool enable_mp = false,
                             amrex::Real init_rh = 0.0)
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
        initialize_1d_state(state, ug, vg, t_ref, lapse_rate, z0, domain_height, dz, latitude,
                            0.0, -1.0e30, turb_model, enable_mp, init_rh);
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
