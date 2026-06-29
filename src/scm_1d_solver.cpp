#include "scm_1d_solver.H"
#include <iostream>
#include <iomanip>
#include <cmath>

/**
 * @brief SCM1DSolver Implementation
 *
 * Single Column Model (SCM) 1D solver for atmospheric boundary layer simulation.
 * Based on physics from: https://github.com/hgopalan/onedterrainsolver/blob/main/hrrr_1dsolver_terrain.py
 *
 * Date: 2026-06-29
 */

SCM1DSolver::SCM1DSolver(Real scm_height, Real scm_dz, const SCMSimilarityParams& params)
    : scm_height_(scm_height), scm_dz_(scm_dz), params_(params),
      ug_(10.0), vg_(0.0), t_ref_(params.temperature_reference),
      tke_init_(0.4), pblh_(1000.0), ustar_(0.41), thetastar_(0.0),
      mo_length_(-1e30), Qh_(0.0), Qb_(0.0),
      start_time_(0.0), end_time_(20000.0), lower_(0),
      inversion_height_(468.0), inversion_width_(83.0),
      delta_inversion_(8.0), lapse_rate_(0.003)
{
    // Create grid
    nz_ = static_cast<int>(scm_height / scm_dz) + 1;
    z_.resize(nz_);
    for (int i = 0; i < nz_; ++i) {
        z_[i] = i * scm_dz;
    }

    // Allocate fields
    ux_.resize(nz_, 0.0);
    uy_.resize(nz_, 0.0);
    temperature_.resize(nz_, t_ref_);
    tke_.resize(nz_, 0.1);
    nut_.resize(nz_, 1e-5);
    lscale_.resize(nz_, 0.1);
    nutPrime_.resize(nz_, 0.0);
    sigmaT_.resize(nz_, 1.0);
    Rt_.resize(nz_, 0.0);

    // Compute Coriolis parameter for given latitude
    Real omega = 7.292115e-5; // Earth's rotation rate (rad/s)
    coriolis_f_ = 2.0 * omega * std::sin(params.latitude * M_PI / 180.0);
}

void SCM1DSolver::initialize_fields() {
    // Initialize wind fields with geostrophic values
    for (int i = 0; i < nz_; ++i) {
        ux_[i] = ug_;
        uy_[i] = vg_;

        // Temperature profile with inversion
        if (z_[i] <= inversion_height_) {
            temperature_[i] = t_ref_;
        } else if (z_[i] > inversion_height_ && z_[i] <= inversion_height_ + inversion_width_) {
            temperature_[i] = t_ref_ + (z_[i] - inversion_height_) * 0.08;
        } else {
            temperature_[i] = temperature_[i > 0 ? i - 1 : 0] + lapse_rate_ * scm_dz_;
        }

        // Turbulence initialization
        nut_[i] = 1e-5;
        tke_[i] = 0.1;
        lscale_[i] = 0.1;
        nutPrime_[i] = nut_[i];
        sigmaT_[i] = 1.0;
    }
}

void SCM1DSolver::compute_similarity() {
    // Wind speed at first grid level above surface
    Real M1 = std::sqrt(ux_[1] * ux_[1] + uy_[1] * uy_[1]);

    // Initial friction velocity estimate from log-law
    ustar_ = 0.41 * M1 / std::log(z_[1] / params_.z0);

    // Iteration for similarity theory
    int iter = 0;
    Real error = 25.0;
    Real psi_m = 0.0, psi_h = 0.0;

    if (params_.heat_flux_mode == 1) {
        // Heat flux specified - iterate to find MOL and surface temperature
        Qh_ = params_.heat_flux_value;
        while (iter <= 25 && error > 1e-5) {
            Real utau_iter = ustar_;
            
            // Compute surface temperature from heat flux
            temperature_[lower_] = Qh_ * (std::log(z_[1] / params_.z0) - psi_h) /
                                  (ustar_ * 0.41) + temperature_[lower_ + 1];

            // Compute MOL
            if (std::abs(Qh_) > 1e-5) {
                mo_length_ = -ustar_ * ustar_ * ustar_ * temperature_[lower_ + 1] /
                            (0.41 * 9.81 * Qh_);
                Real zeta = z_[1] / mo_length_;

                if (zeta >= 0.0) {
                    psi_m = -5.0 * zeta;
                    psi_h = -5.0 * zeta;
                } else {
                    Real x = std::sqrt(1.0 - 16.0 * zeta);
                    psi_h = 2.0 * std::log(0.5 * (1.0 + x));
                    x = std::sqrt(std::sqrt(1.0 - 16.0 * zeta));
                    psi_m = std::log(0.5 * (1.0 + x * x) * 0.25 * (1.0 + x) * (1.0 + x)) -
                           2.0 * std::atan(x) + 0.5 * M_PI;
                }
            } else {
                mo_length_ = -1e30;
                zeta = 0.0;
            }

            ustar_ = 0.41 * M1 / (std::log(z_[1] / params_.z0) - psi_m);
            iter++;
            error = std::abs(ustar_ - utau_iter);
        }
    } else if (params_.heat_flux_mode == 2) {
        // Surface temperature specified
        temperature_[lower_] = params_.temperature_surface;
        iter = 0;
        error = 100.0;
        
        while (iter <= 25 && error > 1e-5) {
            Real utau_iter = ustar_;
            
            Qh_ = (temperature_[lower_] - temperature_[lower_ + 1]) * (ustar_ * 0.41) /
                  (std::log(z_[1] / params_.z0) - psi_h);

            if (std::abs(Qh_) > 1e-5) {
                mo_length_ = -ustar_ * ustar_ * ustar_ * temperature_[lower_ + 1] /
                            (0.41 * 9.81 * Qh_);
                Real zeta = z_[1] / mo_length_;

                if (zeta >= 0.0) {
                    psi_m = -5.0 * zeta;
                    psi_h = -5.0 * zeta;
                } else {
                    Real x = std::sqrt(1.0 - 16.0 * zeta);
                    psi_h = 2.0 * std::log(0.5 * (1.0 + x));
                    x = std::sqrt(std::sqrt(1.0 - 16.0 * zeta));
                    psi_m = std::log(0.5 * (1.0 + x * x) * 0.25 * (1.0 + x) * (1.0 + x)) -
                           2.0 * std::atan(x) + 0.5 * M_PI;
                }
            } else {
                mo_length_ = -1e30;
                psi_m = 0.0;
                psi_h = 0.0;
            }

            ustar_ = 0.41 * M1 / (std::log(z_[1] / params_.z0) - psi_m);
            iter++;
            error = std::abs(ustar_ - utau_iter);
        }
    } else if (params_.heat_flux_mode == 4) {
        // MOL specified - compute surface conditions
        mo_length_ = params_.heat_flux_value;
        Real zeta = z_[1] / mo_length_;

        if (zeta >= 0.0) {
            psi_m = -5.0 * zeta;
            psi_h = -5.0 * zeta;
        } else {
            Real x = std::sqrt(1.0 - 16.0 * zeta);
            psi_h = 2.0 * std::log(0.5 * (1.0 + x));
            x = std::sqrt(std::sqrt(1.0 - 16.0 * zeta));
            psi_m = std::log(0.5 * (1.0 + x * x) * 0.25 * (1.0 + x) * (1.0 + x)) -
                   2.0 * std::atan(x) + 0.5 * M_PI;
        }

        ustar_ = 0.41 * M1 / (std::log(z_[1] / params_.z0) - psi_m);
        thetastar_ = ustar_ * ustar_ * temperature_[lower_ + 1] /
                    (0.41 * 9.81 * mo_length_);
        temperature_[lower_] = temperature_[lower_ + 1] -
                              thetastar_ / 0.41 * (std::log(z_[1] / params_.z0) - psi_h);
        Qh_ = -ustar_ * thetastar_;
    }

    // Compute phi_m for surface conditions
    Real phi_m = 1.0;
    if (mo_length_ < 0.0) {
        phi_m = std::pow(1.0 - 16.0 * params_.z0 / mo_length_, -0.25);
    } else {
        phi_m = 1.0 + 5.0 * params_.z0 / mo_length_;
    }

    // Set surface boundary conditions
    nut_[lower_] = ustar_ * 0.41 * params_.z0 / phi_m;
    Real M0 = M1 - ustar_ / 0.41 * phi_m;
    if (M1 > 1e-10) {
        ux_[lower_] = M0 * ux_[lower_ + 1] / M1;
        uy_[lower_] = M0 * uy_[lower_ + 1] / M1;
    }

    Qb_ = 9.81 / t_ref_ * Qh_;
    tke_[lower_] = ustar_ * ustar_ / (0.556 * 0.556) +
                  (std::max(Qb_, Real(0.0)) * 0.41 * z_[1] / (0.556 * 0.556 * 0.556));
    tke_[lower_] = std::pow(tke_[lower_], 2.0 / 3.0);
    lscale_[lower_] = 0.0;

    // Top boundary conditions
    lscale_[nz_ - 1] = lscale_[nz_ - 2];
    tke_[nz_ - 1] = tke_[nz_ - 2];
    nut_[nz_ - 1] = nut_[nz_ - 2];
    ux_[nz_ - 1] = ux_[nz_ - 2];
    uy_[nz_ - 1] = uy_[nz_ - 2];
    temperature_[nz_ - 1] = temperature_[nz_ - 2];
}

void SCM1DSolver::update_windspeed_x(int i, Real dt) {
    // Relaxation to geostrophic wind in upper domain
    Real dFull = 100.0;
    Real dRD = 50.0;
    Real coeff = 0.0;
    
    if (scm_height_ - z_[i] > dRD + dFull) {
        coeff = 0.0;
    } else if (scm_height_ - z_[i] > dFull) {
        coeff = 0.5 * std::cos(M_PI * (scm_height_ - dFull - z_[i]) / dRD) + 0.5;
    } else {
        coeff = 1.0;
    }

    // Turbulent diffusion term
    Real term1 = nut_[i] * (ux_[i + 1] - 2.0 * ux_[i] + ux_[i - 1]) / (scm_dz_ * scm_dz_);

    // Gradient of eddy viscosity term
    Real dudz = (ux_[i + 1] - ux_[i]) / scm_dz_;
    Real term2 = 0.5 / scm_dz_ * (nut_[i + 1] - nut_[i - 1]) * dudz;

    // Coriolis term
    Real coriolis = coriolis_f_ * uy_[i];

    // Geostrophic forcing in upper domain
    Real damping = coeff * (ug_ - ux_[i]) / 20.0;

    ux_[i] = ux_[i] + dt * (term1 + term2 + coriolis + damping);
}

void SCM1DSolver::update_windspeed_y(int i, Real dt) {
    // Relaxation to geostrophic wind in upper domain
    Real dFull = 100.0;
    Real dRD = 50.0;
    Real coeff = 0.0;
    
    if (scm_height_ - z_[i] > dRD + dFull) {
        coeff = 0.0;
    } else if (scm_height_ - z_[i] > dFull) {
        coeff = 0.5 * std::cos(M_PI * (scm_height_ - dFull - z_[i]) / dRD) + 0.5;
    } else {
        coeff = 1.0;
    }

    // Turbulent diffusion term
    Real term1 = nut_[i] * (uy_[i + 1] - 2.0 * uy_[i] + uy_[i - 1]) / (scm_dz_ * scm_dz_);

    // Gradient of eddy viscosity term
    Real dvdz = (uy_[i + 1] - uy_[i]) / scm_dz_;
    Real term2 = 0.5 / scm_dz_ * (nut_[i + 1] - nut_[i - 1]) * dvdz;

    // Coriolis term
    Real coriolis = -coriolis_f_ * ux_[i];

    // Geostrophic forcing in upper domain
    Real damping = coeff * (vg_ - uy_[i]) / 20.0;

    uy_[i] = uy_[i] + dt * (term1 + term2 + coriolis + damping);
}

void SCM1DSolver::update_temperature(int i, Real dt) {
    Real term1 = nut_[i] / sigmaT_[i] *
                (temperature_[i + 1] - 2.0 * temperature_[i] + temperature_[i - 1]) /
                (scm_dz_ * scm_dz_);
    
    Real term2 = 1.0 / scm_dz_ * (nut_[i] / sigmaT_[i] - nut_[i - 1] / sigmaT_[i - 1]) *
                1.0 / scm_dz_ * (temperature_[i] - temperature_[i - 1]);

    temperature_[i] = temperature_[i] + dt * (term1 + term2);
}

void SCM1DSolver::update_turbulence(int i, Real dt) {
    // TKE diffusion
    Real term1 = nut_[i] * (tke_[i + 1] - 2.0 * tke_[i] + tke_[i - 1]) / (scm_dz_ * scm_dz_);
    Real term2 = 1.0 / scm_dz_ * (nut_[i] - nut_[i - 1]) *
                1.0 / scm_dz_ * (tke_[i] - tke_[i - 1]);

    // Production
    Real production = nut_[i] * (1.0 / (scm_dz_ * scm_dz_)) *
                    ((ux_[i] - ux_[i - 1]) * (ux_[i] - ux_[i - 1]) +
                     (uy_[i] - uy_[i - 1]) * (uy_[i] - uy_[i - 1]));

    // Length scale calculation
    Real lturb = 0.41 * (z_[i] - z_[lower_]);
    Real lmax = 0.00027 * std::sqrt(ug_ * ug_ + vg_ * vg_) / std::max(coriolis_f_, 1e-6);
    Real invLshear = 1.0 / (lturb * lturb) + 1.0 / (lmax * lmax);
    Real lshear = std::sqrt(1.0 / invLshear);

    // Stratification
    Real stratification = 9.81 * (1.0 / (scm_dz_ * t_ref_)) *
                        (temperature_[i] - temperature_[i - 1]);

    // Dissipation
    Real dissipation = 0.556 * 0.556 * 0.556 * std::pow(tke_[i], 1.5) /
                      std::max(lscale_[i], 1e-10);

    Rt_[i] = (tke_[i] / std::max(dissipation, 1e-15)) * (tke_[i] / std::max(dissipation, 1e-15)) *
            stratification;

    if (Rt_[i] < -1.0) {
        Rt_[i] = std::max(Rt_[i], Rt_[i] - (1.0 + Rt_[i]) * (1.0 + Rt_[i]) / (Rt_[i] - 1.0));
    }

    // Buoyancy term
    Real buoyancy = -nutPrime_[i] * stratification;

    // Length scale with buoyancy effects
    Real lscale = lshear;
    if (Rt_[i] > 0.0) {
        Real lbuoyancy = 0.25 * std::sqrt(tke_[i]) / std::sqrt(std::max(stratification, 1e-15));
        Real invLscale = 1.0 / (lshear * lshear) + 1.0 / (lbuoyancy * lbuoyancy);
        lscale = std::sqrt(1.0 / invLscale);
    } else {
        lscale = lshear * std::sqrt(1.0 - (0.556 * 0.556 * 0.556 * 0.556 * 0.556 * 0.556) /
                                         (0.35 * 0.35) * Rt_[i]);
    }

    // Update TKE
    Real diffusion = term1 + term2;
    tke_[i] = tke_[i] + dt * (production + buoyancy - dissipation + diffusion);
    tke_[i] = std::max(tke_[i], 1e-15);

    // Update eddy viscosity and mixing length
    Real cmu = (0.556 + 0.108 * Rt_[i]) / (1.0 + 0.308 * Rt_[i] + 0.00837 * Rt_[i] * Rt_[i]);
    nut_[i] = cmu * std::sqrt(tke_[i]) * lscale;

    Real cmuprime = 0.556 / (1.0 + 0.277 * Rt_[i]);
    sigmaT_[i] = (1.0 + 0.193 * Rt_[i]) / (1.0 + 0.0302 * Rt_[i]);
    nutPrime_[i] = cmuprime * std::sqrt(tke_[i]) * lscale;
    lscale_[i] = lscale;
}

Real SCM1DSolver::interpolate_field(Real z, const std::vector<Real>& field) const {
    // Linear interpolation
    if (z <= z_[0]) return field[0];
    if (z >= z_[nz_ - 1]) return field[nz_ - 1];

    for (int i = 0; i < nz_ - 1; ++i) {
        if (z >= z_[i] && z <= z_[i + 1]) {
            Real frac = (z - z_[i]) / (z_[i + 1] - z_[i]);
            return field[i] * (1.0 - frac) + field[i + 1] * frac;
        }
    }
    return field[nz_ - 1];
}

bool SCM1DSolver::check_convergence(Real u_current, Real v_current,
                                    Real u_target, Real v_target, Real tol) const {
    return std::abs(u_current - u_target) < tol && std::abs(v_current - v_target) < tol;
}

void SCM1DSolver::adjust_geostrophic_wind(Real u_current, Real v_current,
                                         Real u_target, Real v_target) {
    // Simple adjustment: reduce error by half of current residual
    Real du_error = u_current - u_target;
    Real dv_error = v_current - v_target;

    if (std::abs(du_error) > std::abs(dv_error)) {
        if (u_target > 0.0) {
            ug_ = (u_current > u_target) ? ug_ - 0.5 * std::abs(du_error) : ug_ + 0.5 * std::abs(du_error);
        } else {
            ug_ = (u_current < u_target) ? ug_ + 0.5 * std::abs(du_error) : ug_ - 0.5 * std::abs(du_error);
        }
    } else {
        if (v_target > 0.0) {
            vg_ = (v_current > v_target) ? vg_ - 0.5 * std::abs(dv_error) : vg_ + 0.5 * std::abs(dv_error);
        } else {
            vg_ = (v_current < v_target) ? vg_ + 0.5 * std::abs(dv_error) : vg_ - 0.5 * std::abs(dv_error);
        }
    }
}

void SCM1DSolver::run_to_convergence(Real target_u_ref, Real target_v_ref, Real z_ref,
                                     Real tolerance, int max_iterations, Real end_time) {
    end_time_ = end_time;

    int outer_iter = 0;
    Real residual_u = 100.0, residual_v = 100.0;

    while ((residual_u > tolerance || residual_v > tolerance) && outer_iter < max_iterations) {
        // Initialize simulation for this geostrophic wind iteration
        initialize_fields();
        start_time_ = 0.0;
        dt_ = 0.8 * scm_dz_ / std::max(std::sqrt(ug_ * ug_ + vg_ * vg_), 1.0);

        // Run simulation until convergence
        int counter = 0;
        Real converge_tol = 1e-3;
        bool converged = false;

        while (start_time_ <= end_time_ && !converged) {
            start_time_ += dt_;
            compute_similarity();

            // Update fields
            for (int i = lower_ + 1; i < nz_ - 1; ++i) {
                update_windspeed_x(i, dt_);
                update_windspeed_y(i, dt_);
                update_temperature(i, dt_);
                update_turbulence(i, dt_);
            }

            // Adaptive time stepping
            Real max_u = 0.0;
            for (int i = 0; i < nz_; ++i) {
                max_u = std::max(max_u, std::sqrt(ux_[i] * ux_[i] + uy_[i] * uy_[i]));
            }
            dt_ = 0.8 * scm_dz_ / std::max(max_u, 1.0);

            counter++;
            if (counter % 5000 == 0) {
                Real err_u = 0.0, err_v = 0.0;
                for (int i = 0; i < nz_; ++i) {
                    err_u += ux_[i];
                    err_v += uy_[i];
                }
                if (counter % 10000 == 0) {
                    std::cout << "SCM iter " << counter << ": sum(u)=" << err_u
                             << " sum(v)=" << err_v << std::endl;
                }
            }

            if (counter > 100) converged = true; // Simple stopping criterion
        }

        // Evaluate wind at reference height
        Real u_at_ref = interpolate_field(z_ref, ux_);
        Real v_at_ref = interpolate_field(z_ref, uy_);

        residual_u = std::abs(u_at_ref - target_u_ref);
        residual_v = std::abs(v_at_ref - target_v_ref);

        std::cout << "SCM outer iteration " << outer_iter
                 << ": ug=" << ug_ << " vg=" << vg_
                 << " u_ref=" << u_at_ref << " v_ref=" << v_at_ref
                 << " error_u=" << residual_u << " error_v=" << residual_v << std::endl;

        if (residual_u < tolerance && residual_v < tolerance) {
            std::cout << "SCM converged!" << std::endl;
            break;
        }

        adjust_geostrophic_wind(u_at_ref, v_at_ref, target_u_ref, target_v_ref);
        outer_iter++;
    }
}

void SCM1DSolver::get_wind_at_height(Real z, Real& u, Real& v) const {
    u = interpolate_field(z, ux_);
    v = interpolate_field(z, uy_);
}

Real SCM1DSolver::get_temperature_at_height(Real z) const {
    return interpolate_field(z, temperature_);
}

Real SCM1DSolver::get_eddy_viscosity_at_height(Real z) const {
    return interpolate_field(z, nut_);
}

void SCM1DSolver::get_profile(std::vector<Real>& z, std::vector<Real>& u,
                              std::vector<Real>& v, std::vector<Real>& temp,
                              std::vector<Real>& tke, std::vector<Real>& nut) const {
    z = z_;
    u = ux_;
    v = uy_;
    temp = temperature_;
    tke = tke_;
    nut = nut_;
}
