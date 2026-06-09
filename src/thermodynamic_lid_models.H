// ============================================================================
// thermodynamic_lid_models.H
// 1D Thermodynamic slab models for convective boundary layer growth
//
// This header provides implementations of Carson's (1973) and Maul's (1980)
// thermodynamic models that predict the dynamic growth of the convective
// boundary layer mixing depth z_i(t) from surface sensible heat flux and
// morning potential temperature sounding gradients.
//
// Models:
// 1. Carson's Model (1973): Zero-order encroachment model
//    z_i(t) = sqrt( z_i(0)^2 + [2*(1 + 2A) / (rho*cp*gamma)] * \int QH(t') dt' )
//
// 2. Maul's Model (1980): First-order jump model with temperature inversion step
//    dz_i/dt = A * H0(t) / \Delta\theta
//    d(\Delta\theta)/dt = gamma * dz_i/dt - (1+A) * H0(t) / z_i
//
// References:
// - Carson, D. J. (1973). The development of a dry inversion-capped ...
// - Maul, P. R. (1980). Atmospheric transport of sulfur compound pollutants.
// ============================================================================

#ifndef THERMODYNAMIC_LID_MODELS_H
#define THERMODYNAMIC_LID_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_Print.H>
#include <AMReX_ParmParse.H>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

// ============================================================================
// Thermodynamic lid parameters structure
// ============================================================================
struct ThermodynamicLidParams {
    bool enabled = false;                           // Enable thermodynamic capping lid model
    std::string model = "carson";                   // Chosen model: "carson" or "maul"
    std::string flux_file = "";                     // File containing time-varying sensible heat flux Q_H(t) [W/m²]
    amrex::Real constant_flux = 0.0;                // Constant fallback surface sensible heat flux [W/m²]
    amrex::Real gamma = 0.005;                      // Morning potential temperature gradient [K/m]
    amrex::Real initial_zi = 1000.0;                // Initial sunrise mixing depth z_i(0) [m]
    amrex::Real entrainment_ratio = 0.2;            // Entrainment ratio A (typically 0.2)
    amrex::Real rho = 1.2;                          // Reference air density [kg/m³]
    amrex::Real cp = 1005.0;                        // Specific heat capacity of dry air [J/(kg K)]
    amrex::Real initial_delta_theta = 1.0;          // Initial inversion jump Δθ(0) [K] (Maul only)
};

// ============================================================================
// Parse input parameters for the thermodynamic lid
// ============================================================================
inline void parse_thermodynamic_lid_inputs(ThermodynamicLidParams& params, const std::string& prefix = "") {
    amrex::ParmParse pp(prefix);
    pp.query("enable_thermodynamic_lid", params.enabled);
    if (!params.enabled) return;

    pp.query("thermodynamic_lid_model", params.model);
    pp.query("thermodynamic_lid_flux_file", params.flux_file);
    pp.query("thermodynamic_lid_constant_flux", params.constant_flux);
    pp.query("thermodynamic_lid_gamma", params.gamma);
    pp.query("thermodynamic_lid_initial_zi", params.initial_zi);
    pp.query("thermodynamic_lid_entrainment_ratio", params.entrainment_ratio);
    pp.query("thermodynamic_lid_rho", params.rho);
    pp.query("thermodynamic_lid_cp", params.cp);
    pp.query("thermodynamic_lid_initial_delta_theta", params.initial_delta_theta);

    if (params.gamma <= 0.0) {
        amrex::Abort("Thermodynamic Lid Error: thermodynamic_lid_gamma must be strictly positive!");
    }
}

// ============================================================================
// Read time-varying flux file
// Format: time flux (whitespace or comma separated; '#' comments)
// ============================================================================
inline void read_thermodynamic_flux_file(const std::string& filename,
                                         std::vector<amrex::Real>& times,
                                         std::vector<amrex::Real>& values) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        amrex::Abort("Thermodynamic Lid Error: cannot open flux file: " + filename);
    }

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        amrex::Real t, val;
        if (ss >> t >> val) {
            times.push_back(t);
            values.push_back(val);
        }
    }
    if (times.empty()) {
        amrex::Abort("Thermodynamic Lid Error: no data read from flux file: " + filename);
    }
    amrex::Print() << "thermodynamic_lid: read " << times.size()
                   << " flux data points from " << filename << "\n";
}

// ============================================================================
// Compute cumulative integrated flux from 0 to t
// ============================================================================
inline amrex::Real get_thermo_integrated_flux(amrex::Real t,
                                              const std::vector<amrex::Real>& flux_times,
                                              const std::vector<amrex::Real>& flux_values,
                                              amrex::Real constant_flux) {
    if (flux_times.empty()) {
        return constant_flux * t;
    }

    amrex::Real integral = 0.0;
    if (t <= flux_times.front()) {
        return flux_values.front() * t;
    }

    amrex::Real t_prev = 0.0;
    if (flux_times.front() > 0.0) {
        amrex::Real t_limit = std::min(t, flux_times.front());
        integral += flux_values.front() * t_limit;
        t_prev = t_limit;
    }

    for (std::size_t i = 0; i < flux_times.size() - 1; ++i) {
        if (t <= t_prev) break;

        amrex::Real t0 = flux_times[i];
        amrex::Real t1 = flux_times[i+1];
        amrex::Real f0 = flux_values[i];
        amrex::Real f1 = flux_values[i+1];

        if (t < t1) {
            amrex::Real dt_interval = t - t0;
            amrex::Real f_t = f0 + (f1 - f0) * (dt_interval / (t1 - t0));
            integral += 0.5 * (f0 + f_t) * dt_interval;
            t_prev = t;
            break;
        } else {
            integral += 0.5 * (f0 + f1) * (t1 - t0);
            t_prev = t1;
        }
    }

    if (t > t_prev) {
        integral += flux_values.back() * (t - t_prev);
    }

    return integral;
}

// ============================================================================
// Carson's Model (1973) Integrated Analytical Solution
// ============================================================================
inline amrex::Real compute_carson_zi(amrex::Real t,
                                     const ThermodynamicLidParams& params,
                                     const std::vector<amrex::Real>& flux_times,
                                     const std::vector<amrex::Real>& flux_values) {
    if (t <= 0.0) {
        return params.initial_zi;
    }
    amrex::Real integrated_flux = get_thermo_integrated_flux(t, flux_times, flux_values, params.constant_flux);

    // Carson's equation: z_i(t) = sqrt( z_i(0)^2 + 2*(1 + 2A)*\int QH dt' / (rho * cp * gamma) )
    amrex::Real coeff = 2.0 * (1.0 + 2.0 * params.entrainment_ratio) / (params.rho * params.cp * params.gamma);
    amrex::Real val = params.initial_zi * params.initial_zi + coeff * integrated_flux;

    if (val < 0.0) {
        return 0.0;
    }
    return std::sqrt(val);
}

// ============================================================================
// Maul's Model (1980) Numerical ODE Integrator
// ============================================================================
inline amrex::Real compute_maul_zi(amrex::Real t_target,
                                   const ThermodynamicLidParams& params,
                                   const std::vector<amrex::Real>& flux_times,
                                   const std::vector<amrex::Real>& flux_values) {
    if (t_target <= 0.0) {
        return params.initial_zi;
    }

    amrex::Real zi = params.initial_zi;
    amrex::Real dt_val = params.initial_delta_theta;

    amrex::Real t_curr = 0.0;
    amrex::Real dt_sub = 1.0; // 1.0 second sub-step integration

    auto derivatives = [&](amrex::Real t_loc, amrex::Real zi_curr, amrex::Real dt_curr,
                           amrex::Real& dzi_dt, amrex::Real& ddt_dt) {
        amrex::Real QH = params.constant_flux;
        if (!flux_times.empty()) {
            if (t_loc <= flux_times.front()) {
                QH = flux_values.front();
            } else if (t_loc >= flux_times.back()) {
                QH = flux_values.back();
            } else {
                for (std::size_t i = 0; i < flux_times.size() - 1; ++i) {
                    if (t_loc >= flux_times[i] && t_loc <= flux_times[i+1]) {
                        amrex::Real t0 = flux_times[i];
                        amrex::Real t1 = flux_times[i+1];
                        amrex::Real f0 = flux_values[i];
                        amrex::Real f1 = flux_values[i+1];
                        QH = f0 + (t_loc - t0) * (f1 - f0) / (t1 - t0);
                        break;
                    }
                }
            }
        }

        amrex::Real H0 = QH / (params.rho * params.cp);
        if (H0 <= 0.0) {
            // No growth if surface heat flux is negative or zero (e.g. stable boundary layer)
            dzi_dt = 0.0;
            ddt_dt = 0.0;
            return;
        }

        amrex::Real zi_safe = std::max(zi_curr, amrex::Real(1.0));
        amrex::Real dt_safe = std::max(dt_curr, amrex::Real(1e-4));

        dzi_dt = params.entrainment_ratio * H0 / dt_safe;
        ddt_dt = params.gamma * dzi_dt - (1.0 + params.entrainment_ratio) * H0 / zi_safe;
    };

    while (t_curr < t_target) {
        amrex::Real step = std::min(dt_sub, t_target - t_curr);

        // Runge-Kutta 4th Order (RK4)
        amrex::Real dzi1, ddt1;
        derivatives(t_curr, zi, dt_val, dzi1, ddt1);

        amrex::Real dzi2, ddt2;
        derivatives(t_curr + 0.5 * step, zi + 0.5 * step * dzi1, dt_val + 0.5 * step * ddt1, dzi2, ddt2);

        amrex::Real dzi3, ddt3;
        derivatives(t_curr + 0.5 * step, zi + 0.5 * step * dzi2, dt_val + 0.5 * step * ddt2, dzi3, ddt3);

        amrex::Real dzi4, ddt4;
        derivatives(t_curr + step, zi + step * dzi3, dt_val + step * ddt3, dzi4, ddt4);

        zi += (step / 6.0) * (dzi1 + 2.0 * dzi2 + 2.0 * dzi3 + dzi4);
        dt_val += (step / 6.0) * (ddt1 + 2.0 * ddt2 + 2.0 * ddt3 + ddt4);

        zi = std::max(zi, amrex::Real(1.0));
        dt_val = std::max(dt_val, amrex::Real(1e-4));

        t_curr += step;
    }

    return zi;
}

// ============================================================================
// Unified Thermodynamic Lid Model zi(t) Calculation
// ============================================================================
inline amrex::Real compute_thermodynamic_zi(amrex::Real t,
                                            const ThermodynamicLidParams& params,
                                            const std::vector<amrex::Real>& flux_times,
                                            const std::vector<amrex::Real>& flux_values) {
    if (!params.enabled) {
        return params.initial_zi;
    }
    if (params.model == "maul") {
        return compute_maul_zi(t, params, flux_times, flux_values);
    } else {
        return compute_carson_zi(t, params, flux_times, flux_values);
    }
}

#endif // THERMODYNAMIC_LID_MODELS_H
