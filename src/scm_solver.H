// ============================================================================
// scm_solver.H
// Single Column Model (SCM) for atmospheric boundary layer initialization
//
// Implements the PALM 1D model approach (Maronga et al. 2015) on a 1D vertical
// column (0–4000 m, dz = 4 m, N = 1000 levels) integrated in pseudo-time to a
// quasi-steady state. The resulting profiles of wind (u, v), potential temperature
// (theta), and turbulence kinetic energy (e) are then interpolated onto the
// 3D AMReX MultiFab with terrain awareness.
//
// Physics summary:
//   - Monin–Obukhov Similarity Theory (MOST) lower boundary condition
//   - Geostrophic wind estimated via ABL resistance law (Clarke & Hess 1974)
//   - Coriolis forcing: f = 2 Omega sin(phi)
//   - Turbulence closure: 1.5-order E-l scheme (Deardorff 1980)
//   - Blackadar (1962) mixing length
//   - Semi-implicit time integration: implicit diffusion, explicit forcing
//   - Tridiagonal (Thomas algorithm) solve for u, v, theta, e per step
//
// References:
//   - Deardorff, J. W. (1980). Boundary-Layer Meteorol., 18, 495–527.
//   - Maronga et al. (2015). Geosci. Model Dev., 8, 2515–2551.
//   - Clarke, R. H., & Hess, G. D. (1974). Boundary-Layer Meteorol., 6, 353–370.
//   - Blackadar, A. K. (1962). J. Geophys. Res., 67, 3095–3102.
//   - Businger et al. (1971). J. Atmos. Sci., 28, 181–189.
//   - Zilitinkevich, S. S. (1972). Boundary-Layer Meteorol., 3, 141–145.
//
// Date: 2026-07-01
// ============================================================================

#ifndef SCM_SOLVER_H
#define SCM_SOLVER_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <string>

#include <AMReX_REAL.H>
#include <AMReX_Print.H>

#include "stability_models.H"
#include "coriolis_latitude_scaling.H"

namespace SCMSolver {

// ============================================================================
// Physical and model constants
// ============================================================================

/// Von Kármán constant
static constexpr amrex::Real kappa     = amrex::Real(0.41);
/// Gravitational acceleration [m/s²]
static constexpr amrex::Real g_accel   = amrex::Real(9.81);
/// Eddy-viscosity coefficient c_m in K_m = c_m * l * sqrt(e)
static constexpr amrex::Real c_m       = amrex::Real(0.1);
/// Turbulent Prandtl number (ratio K_m / K_h for stable/neutral)
static constexpr amrex::Real Pr_t      = amrex::Real(0.85);
/// Minimum TKE floor [m²/s²] (PALM default)
static constexpr amrex::Real e_min     = amrex::Real(1.0e-5);
/// Scalar roughness length ratio z0h = z0 / 7.4 (Andreas 1987)
static constexpr amrex::Real z0h_ratio = amrex::Real(1.0) / amrex::Real(7.4);

// ============================================================================
// Input parameter struct
// ============================================================================

/// All user-configurable SCM parameters (populated from ParmParse in wind_solver_app)
struct SCMParams {
    amrex::Real U_ref      = amrex::Real(10.0);   ///< Reference wind speed magnitude [m/s]
    amrex::Real dir_ref    = amrex::Real(270.0);  ///< Reference wind direction from North [deg]; 270 = westerly
    amrex::Real z_ref      = amrex::Real(10.0);   ///< Height of reference wind speed measurement [m]
    amrex::Real L_obukhov  = amrex::Real(1.0e6);  ///< Monin–Obukhov length L [m]; +stable, -unstable, large = neutral
    amrex::Real latitude   = amrex::Real(45.0);   ///< Geographic latitude [deg]; positive = Northern Hemisphere
    amrex::Real z0         = amrex::Real(0.1);    ///< Aerodynamic roughness length [m]
    amrex::Real T_ref      = amrex::Real(300.0);  ///< Reference potential temperature [K]
    amrex::Real z_T_ref    = amrex::Real(2.0);    ///< Height at which T_ref is measured [m]
    amrex::Real lapse_rate = amrex::Real(0.003);  ///< Free-troposphere potential temperature lapse rate [K/m]
    amrex::Real dt         = amrex::Real(60.0);   ///< Time step for SCM integration [s]
    amrex::Real max_time   = amrex::Real(172800.0); ///< Maximum spin-up time [s] (default 48 h)
    amrex::Real conv_tol   = amrex::Real(1.0e-5); ///< Convergence tolerance (relative change in u, v, theta)
    amrex::Real min_time   = amrex::Real(86400.0); ///< Minimum spin-up time before checking convergence [s] (24 h)
};

// ============================================================================
// SCM output profiles
// ============================================================================

/// All 1D profiles produced by the SCM (indexed from surface k=0 to top k=N-1)
struct SCMProfiles {
    int    N  = 0;              ///< Number of levels
    amrex::Real dz = amrex::Real(0.0); ///< Vertical grid spacing [m]
    std::vector<amrex::Real> z;     ///< Cell-centre heights above ground [m]
    std::vector<amrex::Real> u;     ///< West-East wind component [m/s]
    std::vector<amrex::Real> v;     ///< South-North wind component [m/s]
    std::vector<amrex::Real> theta; ///< Potential temperature [K]
    std::vector<amrex::Real> tke;   ///< Turbulence kinetic energy [m²/s²]
    std::vector<amrex::Real> Km;    ///< Eddy viscosity [m²/s]
    std::vector<amrex::Real> Kh;    ///< Thermal diffusivity [m²/s]
};

// ============================================================================
// Tridiagonal (Thomas algorithm) solver
// Solves A·x = r where A is tridiagonal:
//   a[i]*x[i-1] + b[i]*x[i] + c[i]*x[i+1] = r[i]
// a[0] and c[N-1] are unused (boundary rows).
// ============================================================================
inline void thomas_solve(const std::vector<amrex::Real>& a,
                         const std::vector<amrex::Real>& b,
                         const std::vector<amrex::Real>& c,
                         const std::vector<amrex::Real>& r,
                         std::vector<amrex::Real>& x)
{
    const int N = static_cast<int>(b.size());
    std::vector<amrex::Real> w(N), g(N);

    // Forward sweep
    w[0] = b[0];
    g[0] = r[0];
    for (int i = 1; i < N; ++i) {
        amrex::Real m = a[i] / w[i-1];
        w[i] = b[i] - m * c[i-1];
        g[i] = r[i] - m * g[i-1];
    }

    // Back substitution
    x[N-1] = g[N-1] / w[N-1];
    for (int i = N-2; i >= 0; --i) {
        x[i] = (g[i] - c[i] * x[i+1]) / w[i];
    }
}

// ============================================================================
// Compute friction velocity u* from MOST
//
//   u* = kappa * U_ref / ( ln(z_ref / z0) - psi_m(z_ref / L) )
//
// Uses the Businger-Dyer stability functions from stability_models.H.
// ============================================================================
inline amrex::Real compute_ustar(amrex::Real U_ref,
                                  amrex::Real z_ref,
                                  amrex::Real z0,
                                  amrex::Real L_obukhov) noexcept
{
    amrex::Real psi_val = amrex::Real(0.0);
    if (std::abs(L_obukhov) > amrex::Real(1.0e-3)) {
        psi_val = psi_m(z_ref / L_obukhov, false);
    }
    amrex::Real denom = std::log(z_ref / z0) - psi_val;
    if (denom < amrex::Real(0.1)) denom = amrex::Real(0.1);
    return kappa * U_ref / denom;
}

// ============================================================================
// Compute temperature scaling theta* from L, u*, and T_ref
//
// PALM / standard MOST convention (Maronga et al. 2015, Stull 1988):
//
//   theta* = +u*^2 * T_ref / ( kappa * g * L )
//
// Sign behaviour:
//   L > 0 (stable)  : theta* > 0  →  wt_s = -u* theta* < 0  (downward, cooling)
//   L < 0 (unstable): theta* < 0  →  wt_s = -u* theta* > 0  (upward, heating)
//   |L| large (neutral): theta* → 0
// ============================================================================
inline amrex::Real compute_theta_star(amrex::Real ustar,
                                       amrex::Real T_ref,
                                       amrex::Real L_obukhov) noexcept
{
    if (std::abs(L_obukhov) < amrex::Real(1.0e-3)) {
        return amrex::Real(0.0);
    }
    return (ustar * ustar * T_ref) / (kappa * g_accel * L_obukhov);
}

// ============================================================================
// Stability heat flux correction function psi_h
//
//   Stable   (zeta > 0): psi_h = -5 * zeta
//   Unstable (zeta < 0): psi_h = 2 * ln( (1 + x^2) / 2 ), x = (1 - 16*zeta)^0.25
// ============================================================================
inline amrex::Real psi_h_func(amrex::Real zeta) noexcept
{
    const amrex::Real zeta_max = amrex::Real(2.0);
    const amrex::Real zeta_min = amrex::Real(-2.0);
    zeta = std::max(zeta_min, std::min(zeta_max, zeta));

    if (zeta > amrex::Real(0.0)) {
        return amrex::Real(-5.0) * zeta;
    } else if (zeta < amrex::Real(0.0)) {
        amrex::Real x = std::pow(amrex::Real(1.0) - amrex::Real(16.0) * zeta, amrex::Real(0.25));
        return amrex::Real(2.0) * std::log((amrex::Real(1.0) + x * x) / amrex::Real(2.0));
    }
    return amrex::Real(0.0);
}

// ============================================================================
// Stability momentum correction function phi_m (dimensionless wind shear)
//
//   Stable   (zeta > 0): phi_m = 1 + 5 * zeta
//   Unstable (zeta < 0): phi_m = (1 - 16 * zeta)^(-0.25)
// ============================================================================
inline amrex::Real phi_m_func(amrex::Real zeta) noexcept
{
    const amrex::Real zeta_max = amrex::Real(2.0);
    const amrex::Real zeta_min = amrex::Real(-2.0);
    zeta = std::max(zeta_min, std::min(zeta_max, zeta));
    if (zeta > amrex::Real(0.0)) {
        return amrex::Real(1.0) + amrex::Real(5.0) * zeta;
    } else if (zeta < amrex::Real(0.0)) {
        return std::pow(amrex::Real(1.0) - amrex::Real(16.0) * zeta, amrex::Real(-0.25));
    }
    return amrex::Real(1.0);
}

// ============================================================================
// Estimate geostrophic wind speed G and cross-isobar angle alpha
// using the ABL resistance law (Clarke & Hess 1974):
//
//   G = (u* / kappa) * sqrt( [ ln(u* / (|f| * z0)) - A ]^2 + B^2 )
//   alpha = atan2(B, ln(u* / (|f| * z0)) - A )    [rad]
//
// Neutral baseline: A = 1.8, B = 4.5 (Zilitinkevich 1972).
// For very small |f| (near equator) a fall-back geostrophic wind is used.
// ============================================================================
inline void compute_geostrophic(amrex::Real ustar,
                                 amrex::Real z0,
                                 amrex::Real f_cor,
                                 amrex::Real& G,
                                 amrex::Real& alpha) noexcept
{
    const amrex::Real A = amrex::Real(1.8);
    const amrex::Real B = amrex::Real(4.5);

    if (std::abs(f_cor) < amrex::Real(1.0e-8)) {
        // Near-equator: no rotation; geostrophic = surface wind, zero veer
        G     = ustar / kappa;  // approximate
        alpha = amrex::Real(0.0);
        return;
    }

    amrex::Real arg = ustar / (std::abs(f_cor) * z0);
    if (arg < amrex::Real(1.0)) arg = amrex::Real(1.0);  // prevent log(0)
    amrex::Real X = std::log(arg) - A;
    G     = (ustar / kappa) * std::sqrt(X * X + B * B);
    alpha = std::atan2(B, X);  // cross-isobar angle [rad]; positive = veers left in NH
}

// ============================================================================
// Build Blackadar mixing length profile
//
//   l(z) = kappa * z / (1 + kappa * z / l_inf)
//   l_inf = 0.00027 * G / |f|
//
// For near-zero |f|, l_inf is capped at a large but finite value.
// ============================================================================
inline void build_mixing_length(const std::vector<amrex::Real>& z,
                                 amrex::Real G,
                                 amrex::Real f_cor,
                                 std::vector<amrex::Real>& lm)
{
    amrex::Real l_inf;
    if (std::abs(f_cor) < amrex::Real(1.0e-8)) {
        l_inf = amrex::Real(500.0);  // cap for near-equator
    } else {
        l_inf = amrex::Real(0.00027) * G / std::abs(f_cor);
        l_inf = std::min(l_inf, amrex::Real(500.0));  // reasonable upper limit [m]
    }

    const int N = static_cast<int>(z.size());
    lm.resize(N);
    for (int k = 0; k < N; ++k) {
        amrex::Real zk = std::max(z[k], amrex::Real(0.01));
        lm[k] = kappa * zk / (amrex::Real(1.0) + kappa * zk / l_inf);
    }
}

// ============================================================================
// Update eddy viscosity K_m and thermal diffusivity K_h
// Deardorff (1980) 1.5-order E-l closure:
//
//   K_m(k) = c_m * l(k) * sqrt(e(k))
//
//   K_h(k) = (1 + 2*l/dz) * K_m   for buoyant (unstable, d theta/dz < 0)
//           = K_m / Pr_t            otherwise
// ============================================================================
inline void update_diffusivities(const std::vector<amrex::Real>& e,
                                  const std::vector<amrex::Real>& lm,
                                  const std::vector<amrex::Real>& theta,
                                  amrex::Real dz,
                                  std::vector<amrex::Real>& Km,
                                  std::vector<amrex::Real>& Kh)
{
    const int N = static_cast<int>(e.size());
    const amrex::Real K_min = amrex::Real(1.0e-4);  // minimum diffusivity [m²/s]

    for (int k = 0; k < N; ++k) {
        amrex::Real ek   = std::max(e[k], e_min);
        amrex::Real Km_k = c_m * lm[k] * std::sqrt(ek);
        Km_k = std::max(Km_k, K_min);
        Km[k] = Km_k;

        // Buoyancy check: unstable if d theta/dz < 0 (at interior levels)
        bool unstable = false;
        if (k > 0 && k < N - 1) {
            amrex::Real dthdz = (theta[k+1] - theta[k-1]) / (amrex::Real(2.0) * dz);
            unstable = (dthdz < amrex::Real(0.0));
        } else if (k == 0) {
            amrex::Real dthdz = (theta[1] - theta[0]) / dz;
            unstable = (dthdz < amrex::Real(0.0));
        }

        if (unstable) {
            Kh[k] = (amrex::Real(1.0) + amrex::Real(2.0) * lm[k] / dz) * Km_k;
        } else {
            Kh[k] = Km_k / Pr_t;
        }
        Kh[k] = std::max(Kh[k], K_min);
    }
}

// ============================================================================
// Semi-implicit tridiagonal solve for a generic diffusion equation:
//
//   dphi/dt = d/dz ( K * dphi/dz ) + rhs_explicit
//
// Lower BC: flux = flux_lo  ( K * dphi/dz |_{z=0} = flux_lo )
// Upper BC: phi[N-1] = phi_top  (Dirichlet)
//
// Discretisation of the diffusion term at face k+1/2:
//   flux_{k+1/2} = K_{k+1/2} * (phi[k+1] - phi[k]) / dz
//   where K_{k+1/2} = 0.5*(K[k] + K[k+1])
// ============================================================================
inline void solve_diffusion(std::vector<amrex::Real>& phi,
                             const std::vector<amrex::Real>& K,
                             const std::vector<amrex::Real>& rhs_expl,
                             amrex::Real flux_lo,
                             amrex::Real phi_top,
                             amrex::Real dz,
                             amrex::Real dt)
{
    const int N = static_cast<int>(phi.size());
    std::vector<amrex::Real> a(N), b(N), c(N), r(N), x(N);

    // Face-interpolated diffusivities (harmonic mean is more stable for large contrasts,
    // but arithmetic mean is PALM's default)
    auto Kface = [&](int k) -> amrex::Real {
        if (k >= N - 1) return K[N-1];
        return amrex::Real(0.5) * (K[k] + K[k+1]);
    };

    for (int k = 0; k < N; ++k) {
        amrex::Real Kp = Kface(k);       // K at face k+1/2
        amrex::Real Km_face = Kface(k-1);  // K at face k-1/2 (use K[0] for k=0)
        if (k == 0) Km_face = K[0];

        amrex::Real r_lower = dt / (dz * dz);
        amrex::Real r_upper = dt / (dz * dz);

        a[k] = -r_lower * Km_face;    // sub-diagonal
        c[k] = -r_upper * Kp;         // super-diagonal
        b[k] = amrex::Real(1.0) - a[k] - c[k];  // diagonal (will be adjusted for BCs)
        r[k] = phi[k] + dt * rhs_expl[k];
    }

    // Lower BC: Neumann flux   K * dphi/dz |_{face 0} = flux_lo
    //   flux_lo acts as a source to the first cell:
    //   r[0] += dt * flux_lo / dz
    a[0] = amrex::Real(0.0);
    b[0] = amrex::Real(1.0) + dt * Kface(0) / (dz * dz);
    c[0] = -dt * Kface(0) / (dz * dz);
    r[0] = phi[0] + dt * rhs_expl[0] + dt * flux_lo / dz;

    // Upper BC: Dirichlet phi[N-1] = phi_top
    a[N-1] = amrex::Real(0.0);
    b[N-1] = amrex::Real(1.0);
    c[N-1] = amrex::Real(0.0);
    r[N-1] = phi_top;

    thomas_solve(a, b, c, r, x);
    phi = x;
}

// ============================================================================
// Interpolate a 1D SCM profile to a target height z_agl
// using linear interpolation.  Clamps to boundary values outside the range.
// ============================================================================
inline amrex::Real interp_scm(const std::vector<amrex::Real>& z_scm,
                                const std::vector<amrex::Real>& f_scm,
                                amrex::Real z_target) noexcept
{
    const int N = static_cast<int>(z_scm.size());
    if (N == 0) return amrex::Real(0.0);
    if (z_target <= z_scm[0])     return f_scm[0];
    if (z_target >= z_scm[N-1])   return f_scm[N-1];

    // Binary search for bracket
    int lo = 0, hi = N - 1;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (z_scm[mid] <= z_target) lo = mid;
        else                         hi = mid;
    }
    amrex::Real w = (z_target - z_scm[lo]) / (z_scm[hi] - z_scm[lo]);
    return (amrex::Real(1.0) - w) * f_scm[lo] + w * f_scm[hi];
}

// ============================================================================
// Main SCM run function
//
// Integrates the PALM-style 1D equations in pseudo-time until the profiles
// reach quasi-steady state or max_time is reached.
//
// Returns SCMProfiles containing z, u, v, theta, tke, Km, Kh.
// ============================================================================
inline SCMProfiles run_scm(const SCMParams& p)
{
    // ------------------------------------------------------------------
    // 1. Grid setup: N = 1000 levels, dz = 4 m, cell centres at z = 2, 6, ..., 3998 m
    // ------------------------------------------------------------------
    const int    N  = 1000;
    const amrex::Real dz = amrex::Real(4.0);

    std::vector<amrex::Real> z(N);
    for (int k = 0; k < N; ++k) {
        z[k] = (k + amrex::Real(0.5)) * dz;  // cell centres [m]
    }

    // ------------------------------------------------------------------
    // 2. Coriolis parameter
    // ------------------------------------------------------------------
    const amrex::Real f_cor = compute_latitude_coriolis_parameter(p.latitude);

    // ------------------------------------------------------------------
    // 3. Friction velocity from MOST
    //    u* = kappa * U_ref / ( ln(z_ref/z0) - psi_m(z_ref/L) )
    // ------------------------------------------------------------------
    amrex::Real ustar = compute_ustar(p.U_ref, p.z_ref, p.z0, p.L_obukhov);
    const amrex::Real theta_star = compute_theta_star(ustar, p.T_ref, p.L_obukhov);

    // Surface heat flux: wt_s = -u* * theta*
    const amrex::Real wt_s = -ustar * theta_star;

    // ------------------------------------------------------------------
    // 4. Geostrophic wind estimation (resistance law)
    // ------------------------------------------------------------------
    amrex::Real G_geo, alpha_geo;
    compute_geostrophic(ustar, p.z0, f_cor, G_geo, alpha_geo);

    // ------------------------------------------------------------------
    // STEP 1 & 2: Pre-correction of geostrophic wind
    // --
    // Compute predicted wind speed at z = dz/2 (first cell centre at z = 2 m)
    // using the MOST log-law. Compare against U_ref to compute alpha_pred.
    // If alpha_pred deviates by >30% from 1.0, warn the user before spin-up.
    // ------------------------------------------------------------------
    const amrex::Real z_first_cell = amrex::Real(2.0);  // dz/2 where dz=4 m
    
    // Warn if z0 is too large for the grid resolution
    if (p.z0 >= z_first_cell) {
        amrex::Print() << "========== WARNING: SCM ROUGHNESS LENGTH TOO LARGE ==========\n"
                       << "  Aerodynamic roughness z0 = " << p.z0 << " m\n"
                       << "  is >= first SCM cell height = " << z_first_cell << " m\n"
                       << "  This may violate MOST assumptions and give inaccurate results.\n"
                       << "  Consider reducing scm.z0 or using a coarser velocity grid.\n"
                       << "========================================================\n";
    }
    
    amrex::Real psi_m_first = amrex::Real(0.0);
    if (std::abs(p.L_obukhov) > amrex::Real(1.0e-3)) {
        psi_m_first = psi_m(z_first_cell / p.L_obukhov, false);
    }
    amrex::Real log_arg_first = std::log(z_first_cell / p.z0);
    if (log_arg_first < amrex::Real(0.0)) {
        log_arg_first = amrex::Real(0.0);
        amrex::Print() << "========== WARNING: SCM LOG-LAW ARGUMENT CLAMPED ==========\n"
                       << "  log(z_first_cell / z0) = log(" << z_first_cell << " / " << p.z0 << ") < 0\n"
                       << "  Clamping to 0. This indicates z0 is too large relative to grid height.\n"
                       << "  Consider reviewing scm.z0 and grid resolution.\n"
                       << "========================================================\n";
    }
    amrex::Real U_sfc_pred = (ustar / kappa) * std::max(amrex::Real(0.0), log_arg_first - psi_m_first);
    
    // Avoid division by zero
    if (U_sfc_pred < amrex::Real(1.0e-6)) U_sfc_pred = amrex::Real(1.0e-6);
    
    // Pre-correction factor: scale G_geo so converged profile matches U_ref at z_ref
    amrex::Real alpha_pred = p.U_ref / U_sfc_pred;
    
    // Step 2: Warning if pre-correction factor is too far from 1.0 (±30%)
    const amrex::Real alpha_warning_min = amrex::Real(0.77);   // 1.0 - 0.30
    const amrex::Real alpha_warning_max = amrex::Real(1.3);    // 1.0 + 0.30
    if (alpha_pred < alpha_warning_min || alpha_pred > alpha_warning_max) {
        amrex::Print() << "========== WARNING: SCM PRE-CORRECTION FACTOR OUT OF BOUNDS ==========\n"
                       << "  Computed pre-correction factor alpha_pred = " << alpha_pred << "\n"
                       << "  This indicates the MOST resistance law is inaccurate for the\n"
                       << "  specified atmospheric stability.\n"
                       << "  Suggested actions:\n"
                       << "    - Review scm.L_obukhov (Monin-Obukhov length): " << p.L_obukhov << " m\n"
                       << "    - Review scm.U_ref (reference wind speed): " << p.U_ref << " m/s\n"
                       << "    - Review scm.z_ref (reference height): " << p.z_ref << " m\n"
                       << "    - Review scm.z0 (roughness length): " << p.z0 << " m\n"
                       << "=====================================================================\n";
    }
    
    // Scale geostrophic wind by pre-correction factor
    G_geo = G_geo * alpha_pred;

    // Wind direction: meteorological convention (from North, clockwise)
    // dir_ref = 270 deg means wind blows from west (positive u, zero v in standard coords)
    // Standard math angle from East CCW: math_angle = 270 - dir_ref
    // u_ref_dir = cos(270-dir_ref)*U_ref, v_ref_dir = sin(270-dir_ref)*U_ref
    const amrex::Real deg2rad = MathConstants::pi / amrex::Real(180.0);
    // Convert met direction to math angle: theta_math = (270 - dir_ref) in deg
    amrex::Real theta_math = (amrex::Real(270.0) - p.dir_ref) * deg2rad;
    amrex::Real ref_dir_x  = std::cos(theta_math);  // unit vector, East component
    amrex::Real ref_dir_y  = std::sin(theta_math);  // unit vector, North component

    // Geostrophic components: G at angle (dir_ref + alpha_geo) from reference direction
    // alpha_geo rotates geostrophic wind relative to surface wind (veers to the right in NH)
    amrex::Real geo_angle = theta_math + alpha_geo;
    amrex::Real ug = G_geo * std::cos(geo_angle);
    amrex::Real vg = G_geo * std::sin(geo_angle);

    amrex::Print() << "wind_solver: SCM - u* = " << ustar
                   << " m/s, theta* = " << theta_star
                   << " K, G = " << G_geo
                   << " m/s, f = " << f_cor << " rad/s\n";
    amrex::Print() << "wind_solver: SCM - ug = " << ug << " m/s, vg = " << vg << " m/s\n";
    amrex::Print() << "wind_solver: SCM - pre-correction factor alpha_pred = " << alpha_pred << "\n";

    // ------------------------------------------------------------------
    // 5. Initial profiles
    //    u, v: log-law in direction of ref wind, rotated to geostrophic aloft
    //    theta: MOST near-surface, free lapse rate aloft
    //    tke: diagnostic from u*
    // ------------------------------------------------------------------
    std::vector<amrex::Real> u(N), v(N), theta(N), tke(N), Km(N), Kh(N), lm(N);

    // Scalar roughness length
    const amrex::Real z0h = p.z0 * z0h_ratio;

    for (int k = 0; k < N; ++k) {
        amrex::Real zk = z[k];

        // Initialise wind with log-law profile in reference wind direction
        amrex::Real psi_m_val = amrex::Real(0.0);
        if (std::abs(p.L_obukhov) > amrex::Real(1.0e-3)) {
            psi_m_val = psi_m(zk / p.L_obukhov, false);
        }
        amrex::Real log_arg = std::log(zk / p.z0);
        if (log_arg < amrex::Real(0.0)) log_arg = amrex::Real(0.0);
        amrex::Real speed_k = (ustar / kappa) * std::max(amrex::Real(0.0), log_arg - psi_m_val);

        u[k] = speed_k * ref_dir_x;
        v[k] = speed_k * ref_dir_y;

        // Initialise theta from MOST near-surface, then free-troposphere lapse rate
        amrex::Real psi_h_val = amrex::Real(0.0);
        amrex::Real z0h_safe  = std::max(z0h, amrex::Real(1.0e-4));
        if (std::abs(p.L_obukhov) > amrex::Real(1.0e-3)) {
            amrex::Real zeta   = zk / p.L_obukhov;
            amrex::Real zeta_h = z0h_safe / p.L_obukhov;
            psi_h_val = psi_h_func(zeta) - psi_h_func(zeta_h);
        }

        // MOST theta profile (valid in surface layer, ~z < 0.1 * h_ABL)
        amrex::Real log_arg_h = std::log(zk / z0h_safe);
        if (log_arg_h < amrex::Real(0.0)) log_arg_h = amrex::Real(0.0);
        amrex::Real theta_MOST = p.T_ref + (theta_star / kappa) * (log_arg_h - psi_h_val);

        // Free-troposphere lapse rate extrapolated from T_ref at z_T_ref
        amrex::Real theta_FT = p.T_ref + p.lapse_rate * (zk - p.z_T_ref);

        // Blend: use MOST below 200 m, free-troposphere above 400 m
        amrex::Real h_blend_lo = amrex::Real(200.0);
        amrex::Real h_blend_hi = amrex::Real(400.0);
        if (zk <= h_blend_lo) {
            theta[k] = theta_MOST;
        } else if (zk >= h_blend_hi) {
            theta[k] = theta_FT;
        } else {
            amrex::Real w = (zk - h_blend_lo) / (h_blend_hi - h_blend_lo);
            theta[k] = (amrex::Real(1.0) - w) * theta_MOST + w * theta_FT;
        }

        // Initial TKE: decays exponentially from surface
        amrex::Real h_abl = (std::abs(f_cor) > amrex::Real(1.0e-8))
                            ? (amrex::Real(0.3) * ustar / std::abs(f_cor))
                            : amrex::Real(1000.0);
        tke[k] = std::max((ustar * ustar) / std::sqrt(c_m) * std::exp(-zk / h_abl), e_min);
    }

    // ------------------------------------------------------------------
    // 6. Initial mixing length and diffusivities
    // ------------------------------------------------------------------
    build_mixing_length(z, G_geo, f_cor, lm);
    update_diffusivities(tke, lm, theta, dz, Km, Kh);

    // ------------------------------------------------------------------
    // 7. Geostrophic wind at upper boundary (constant geostrophic wind)
    //    Temperature at top: Dirichlet from free-troposphere lapse rate
    // ------------------------------------------------------------------
    const amrex::Real theta_top = p.T_ref + p.lapse_rate * (z[N-1] - p.z_T_ref);

    // ------------------------------------------------------------------
    // 8. Time integration loop
    // ------------------------------------------------------------------
    const int    nsteps   = static_cast<int>(std::ceil(p.max_time / p.dt));
    const int    nsteps_min = static_cast<int>(std::ceil(p.min_time / p.dt));
    const amrex::Real dt_val = p.dt;

    std::vector<amrex::Real> u_old(N), v_old(N), theta_old(N);
    std::vector<amrex::Real> rhs_u(N), rhs_v(N), rhs_theta(N), rhs_tke(N);

    int converged_step = -1;

    for (int step = 0; step < nsteps; ++step) {
        // Save previous profiles for convergence check
        u_old     = u;
        v_old     = v;
        theta_old = theta;

        // --- Re-evaluate u* from MOST using current surface wind speed ---
        amrex::Real U1 = std::sqrt(u[0]*u[0] + v[0]*v[0]);
        if (U1 < amrex::Real(0.01)) U1 = amrex::Real(0.01);
        ustar = compute_ustar(U1, z[0], p.z0, p.L_obukhov);
        ustar = std::max(ustar, amrex::Real(1.0e-4));

        // --- Update diffusivities ---
        update_diffusivities(tke, lm, theta, dz, Km, Kh);

        // -------------------------------------------------------------------
        // Momentum: solve u with implicit diffusion, explicit Coriolis
        // du/dt = f(v - vg) + d/dz(Km * du/dz)
        // Lower BC: Km * du/dz|0 = -u*^2 * u1 / |U1|   (friction flux)
        // Upper BC: u[N-1] = ug
        // -------------------------------------------------------------------
        {
            amrex::Real U1_safe = std::max(U1, amrex::Real(1.0e-6));
            amrex::Real flux_u_lo = -ustar * ustar * u[0] / U1_safe;
            for (int k = 0; k < N; ++k) {
                rhs_u[k] = f_cor * (v[k] - vg);  // explicit Coriolis
            }
            // Upper BC: Dirichlet u[N-1] = ug
            solve_diffusion(u, Km, rhs_u, flux_u_lo, ug, dz, dt_val);
        }

        // -------------------------------------------------------------------
        // Momentum: solve v with implicit diffusion, explicit Coriolis
        // dv/dt = -f(u - ug) + d/dz(Km * dv/dz)
        // Lower BC: Km * dv/dz|0 = -u*^2 * v1 / |U1|
        // Upper BC: v[N-1] = vg
        // -------------------------------------------------------------------
        {
            amrex::Real U1_safe = std::max(U1, amrex::Real(1.0e-6));
            amrex::Real flux_v_lo = -ustar * ustar * v[0] / U1_safe;
            for (int k = 0; k < N; ++k) {
                rhs_v[k] = -f_cor * (u[k] - ug);  // explicit Coriolis
            }
            solve_diffusion(v, Km, rhs_v, flux_v_lo, vg, dz, dt_val);
        }

        // -------------------------------------------------------------------
        // Potential temperature: implicit diffusion
        // d theta/dt = d/dz(Kh * d theta/dz)
        // Lower BC: Kh * dtheta/dz|0 = wt_s  (surface heat flux)
        // Upper BC: theta[N-1] = theta_top
        // -------------------------------------------------------------------
        {
            for (int k = 0; k < N; ++k) {
                rhs_theta[k] = amrex::Real(0.0);
            }
            solve_diffusion(theta, Kh, rhs_theta, wt_s, theta_top, dz, dt_val);
        }

        // -------------------------------------------------------------------
        // TKE: semi-implicit solve
        // de/dt = Ps - Pb - eps + d/dz(Km * de/dz)
        //
        // Shear production:  Ps = Km * [ (du/dz)^2 + (dv/dz)^2 ]
        // Buoyancy:          Pb = (g/theta_v) * Kh * d theta/dz
        // Dissipation:       eps = Ce * e^(3/2) / l   (Ce = 0.19 + 0.74 * l/dz)
        //   Linearised for implicit:  eps^(n+1) ~ Ce * e^n^(1/2) / l * e^(n+1)
        //                             i.e. add Ce * sqrt(e^n) / l to diagonal
        // -------------------------------------------------------------------
        {
            // Compute shear and buoyancy (explicit)
            std::vector<amrex::Real> a_e(N), b_e(N), c_e(N), r_e(N), x_e(N);

            // Face-interpolated K for TKE diffusion (use Km as K_e = Km)
            auto Keface = [&](int k) -> amrex::Real {
                if (k >= N - 1) return Km[N-1];
                return amrex::Real(0.5) * (Km[k] + Km[k+1]);
            };

            for (int k = 0; k < N; ++k) {
                // Wind shear (central differences)
                amrex::Real dudz, dvdz;
                if (k == 0) {
                    // Use surface BC flux to get near-surface shear
                    amrex::Real U1_safe = std::max(U1, amrex::Real(1.0e-6));
                    dudz = ustar * ustar * u[0] / (U1_safe * Km[0]);
                    dvdz = ustar * ustar * v[0] / (U1_safe * Km[0]);
                } else if (k == N-1) {
                    dudz = (u[k] - u[k-1]) / dz;
                    dvdz = (v[k] - v[k-1]) / dz;
                } else {
                    dudz = (u[k+1] - u[k-1]) / (amrex::Real(2.0) * dz);
                    dvdz = (v[k+1] - v[k-1]) / (amrex::Real(2.0) * dz);
                }
                amrex::Real Ps = Km[k] * (dudz*dudz + dvdz*dvdz);

                // Buoyancy production / destruction
                amrex::Real dthdz;
                if (k == 0) {
                    dthdz = (theta[1] - theta[0]) / dz;
                } else if (k == N-1) {
                    dthdz = (theta[k] - theta[k-1]) / dz;
                } else {
                    dthdz = (theta[k+1] - theta[k-1]) / (amrex::Real(2.0) * dz);
                }
                amrex::Real theta_v = std::max(theta[k], amrex::Real(200.0));
                amrex::Real Pb = (g_accel / theta_v) * Kh[k] * dthdz;

                // Dissipation coefficient (Deardorff 1980)
                amrex::Real Ce = amrex::Real(0.19) + amrex::Real(0.74) * lm[k] / dz;
                amrex::Real eps_coeff = (lm[k] > amrex::Real(1.0e-6))
                                       ? Ce * std::sqrt(std::max(tke[k], e_min)) / lm[k]
                                       : amrex::Real(0.0);

                // Diffusion stencil (implicit)
                amrex::Real Kep   = Keface(k);
                amrex::Real Kem_f = (k > 0) ? Keface(k-1) : Km[0];
                amrex::Real r_lo  = dt_val / (dz * dz);

                a_e[k] = -r_lo * Kem_f;
                c_e[k] = -r_lo * Kep;
                b_e[k] = amrex::Real(1.0) + r_lo * (Kem_f + Kep) + dt_val * eps_coeff;
                r_e[k] = tke[k] + dt_val * (Ps - Pb);
            }

            // Lower BC: TKE from Deardorff surface value e_s = u*^2 / sqrt(c_m)
            a_e[0] = amrex::Real(0.0);
            c_e[0] = amrex::Real(0.0);
            b_e[0] = amrex::Real(1.0);
            r_e[0] = ustar * ustar / std::sqrt(c_m);
            r_e[0] = std::max(r_e[0], e_min);

            // Upper BC: zero gradient (Neumann) for TKE
            a_e[N-1] = amrex::Real(0.0);
            b_e[N-1] = amrex::Real(1.0);
            c_e[N-1] = amrex::Real(0.0);
            r_e[N-1] = e_min;  // trivial top BC

            thomas_solve(a_e, b_e, c_e, r_e, x_e);
            for (int k = 0; k < N; ++k) {
                tke[k] = std::max(x_e[k], e_min);
            }
        }

        // --- Rebuild mixing length and diffusivities ---
        build_mixing_length(z, G_geo, f_cor, lm);
        update_diffusivities(tke, lm, theta, dz, Km, Kh);

        // --- Convergence check (only after min_time) ---
        if (step >= nsteps_min) {
            amrex::Real norm_u     = amrex::Real(0.0), diff_u     = amrex::Real(0.0);
            amrex::Real norm_v     = amrex::Real(0.0), diff_v     = amrex::Real(0.0);
            amrex::Real norm_theta = amrex::Real(0.0), diff_theta = amrex::Real(0.0);
            for (int k = 0; k < N; ++k) {
                diff_u     += (u[k]-u_old[k])*(u[k]-u_old[k]);
                norm_u     += u[k]*u[k];
                diff_v     += (v[k]-v_old[k])*(v[k]-v_old[k]);
                norm_v     += v[k]*v[k];
                diff_theta += (theta[k]-theta_old[k])*(theta[k]-theta_old[k]);
                norm_theta += theta[k]*theta[k];
            }
            auto rel_err = [](amrex::Real d, amrex::Real n) -> amrex::Real {
                return (n > amrex::Real(1.0e-12)) ? std::sqrt(d/n) : std::sqrt(d);
            };
            amrex::Real err_u     = rel_err(diff_u,     norm_u);
            amrex::Real err_v     = rel_err(diff_v,     norm_v);
            amrex::Real err_theta = rel_err(diff_theta, norm_theta);
            amrex::Real max_err   = std::max({err_u, err_v, err_theta});

            if (max_err < p.conv_tol) {
                converged_step = step;
                amrex::Print() << "wind_solver: SCM converged at step " << step
                               << " (t = " << (step+1)*dt_val << " s), max_err = " << max_err << "\n";
                break;
            }
        }

        // Progress report every 1000 steps
        if ((step+1) % 1000 == 0) {
            amrex::Real t_sim = (step+1) * dt_val;
            amrex::Real U_sfc = std::sqrt(u[0]*u[0] + v[0]*v[0]);
            amrex::Print() << "wind_solver: SCM step " << (step+1)
                           << " / " << nsteps
                           << "  t = " << t_sim << " s"
                           << "  U_sfc = " << U_sfc << " m/s"
                           << "  u* = " << ustar << " m/s\n";
        }
    }

    if (converged_step < 0) {
        amrex::Print() << "wind_solver: SCM did not converge within " << p.max_time
                       << " s; using final state.\n";
    }

    // ------------------------------------------------------------------
    // 9. Package output profiles
    // ------------------------------------------------------------------
    SCMProfiles out;
    out.N  = N;
    out.dz = dz;
    out.z      = z;
    out.u      = u;
    out.v      = v;
    out.theta  = theta;
    out.tke    = tke;
    out.Km     = Km;
    out.Kh     = Kh;

    // ------------------------------------------------------------------
    // STEPS 3 & 4: Post-spin-up rescaling of converged profiles
    // --
    // Interpolate u and v to z_ref, compute actual speed, and compute residual
    // correction factor alpha. Clamp alpha to [0.5, 2.0] and print diagnostics.
    // Apply alpha uniformly to u,v; scale tke by alpha^2; scale Km,Kh by alpha.
    // Leave theta untouched.
    // ------------------------------------------------------------------
    {
        // Interpolate u and v to z_ref
        amrex::Real u_at_zref = interp_scm(z, u, p.z_ref);
        amrex::Real v_at_zref = interp_scm(z, v, p.z_ref);
        
        // Compute actual speed at z_ref
        amrex::Real speed_at_zref = std::sqrt(u_at_zref * u_at_zref + v_at_zref * v_at_zref);
        
        // Avoid division by zero
        if (speed_at_zref < amrex::Real(1.0e-6)) speed_at_zref = amrex::Real(1.0e-6);
        
        // Residual correction factor
        amrex::Real alpha = p.U_ref / speed_at_zref;
        
        // Step 4: Hard clamp alpha to [0.5, 2.0]
        const amrex::Real alpha_min = amrex::Real(0.5);
        const amrex::Real alpha_max = amrex::Real(2.0);
        bool alpha_clamped = false;
        
        if (alpha < alpha_min) {
            amrex::Print() << "========== ERROR: SCM POST-CORRECTION FACTOR OUT OF BOUNDS ==========\n"
                           << "  Computed post-correction factor alpha = " << alpha << " (clamping to " << alpha_min << ")\n"
                           << "  This may indicate severe incompatibility between the input parameters.\n"
                           << "  Results may NOT satisfy U_ref at z_ref.\n"
                           << "  Parameters to review:\n"
                           << "    - scm.L_obukhov (Monin-Obukhov length): " << p.L_obukhov << " m\n"
                           << "    - scm.U_ref (reference wind speed): " << p.U_ref << " m/s\n"
                           << "    - scm.z_ref (reference height): " << p.z_ref << " m\n"
                           << "    - scm.z0 (roughness length): " << p.z0 << " m\n"
                           << "====================================================================\n";
            alpha = alpha_min;
            alpha_clamped = true;
        } else if (alpha > alpha_max) {
            amrex::Print() << "========== ERROR: SCM POST-CORRECTION FACTOR OUT OF BOUNDS ==========\n"
                           << "  Computed post-correction factor alpha = " << alpha << " (clamping to " << alpha_max << ")\n"
                           << "  This may indicate severe incompatibility between the input parameters.\n"
                           << "  Results may NOT satisfy U_ref at z_ref.\n"
                           << "  Parameters to review:\n"
                           << "    - scm.L_obukhov (Monin-Obukhov length): " << p.L_obukhov << " m\n"
                           << "    - scm.U_ref (reference wind speed): " << p.U_ref << " m/s\n"
                           << "    - scm.z_ref (reference height): " << p.z_ref << " m\n"
                           << "    - scm.z0 (roughness length): " << p.z0 << " m\n"
                           << "====================================================================\n";
            alpha = alpha_max;
            alpha_clamped = true;
        }
        
        // Step 3: Apply rescaling to profiles
        for (int k = 0; k < N; ++k) {
            out.u[k]  = out.u[k] * alpha;
            out.v[k]  = out.v[k] * alpha;
            out.tke[k] = out.tke[k] * alpha * alpha;  // TKE scales with alpha^2
            out.Km[k] = out.Km[k] * alpha;            // Km scales with alpha
            out.Kh[k] = out.Kh[k] * alpha;            // Kh scales with alpha
            // out.theta[k] left untouched
        }
        
        // Print diagnostic line (compute actual speed after rescaling)
        amrex::Real u_after = u_at_zref * alpha;
        amrex::Real v_after = v_at_zref * alpha;
        amrex::Real speed_after = std::sqrt(u_after * u_after + v_after * v_after);
        amrex::Print() << "wind_solver: SCM post-spin-up rescaling: U_before = " << speed_at_zref
                       << " m/s, U_after = " << speed_after << " m/s, alpha = " << alpha;
        if (alpha_clamped) {
            amrex::Print() << " (CLAMPED)";
        }
        amrex::Print() << "\n";
    }

    return out;
}

// ============================================================================
// Interpolate a 1D SCM profile to a target height z_agl
// using linear interpolation.  Clamps to boundary values outside the range.
// ============================================================================

} // namespace SCMSolver

#endif // SCM_SOLVER_H
