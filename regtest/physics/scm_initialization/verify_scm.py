#!/usr/bin/env python3
"""
Regression test for SCM (Single Column Model) wind profile initialization.

Implements the 1D SCM algorithm from ``scm_models.H`` in Python and validates
that the geostrophic-wind search reproduces the reference output from
hrrr_1dsolver_terrain.py for the following case:

Reference case parameters
--------------------------
  Stability            : MOL = 500 m (converges to effectively neutral)
  Met mast height      : 150 m
  Target wind at mast  : [10, 0] m/s  (u_e = 10, v_n = 0)
  Roughness length     : z0 = 0.1 m
  Latitude             : 45 °N
  Lapse rate           : 0.01 K/m
  Domain               : 1000 m height, nz = 101, dz = 10 m
  Initial Ug/Vg guess  : [10, -12] m/s
  Max inner steps      : 50 000 (stable case)
  Allowed error        : 0.25 m/s (per component)

Reference outputs (from hrrr_1dsolver_terrain.py)
--------------------------------------------------
  Monin-Obukhov Length  : -1e+30  (neutral sentinel)
  Richardson BL Height  : 800.0 m
  Friction Velocity     : 0.6337 m/s
  Geostrophic Wind      : [13.9206, -10.3659] m/s
  CFD Met Mast Wind     : [ 9.83213,   0.110555] m/s
  Wind Error            : [-0.167866,  0.110555] m/s  (both < 0.25 m/s)

Usage
-----
  python verify_scm.py <input_file> <work_dir>
"""

import argparse
import math
import os
import sys

# ---------------------------------------------------------------------------
# Physical constants (matches scm_models.H)
# ---------------------------------------------------------------------------
KAPPA         = 0.41       # von Kármán constant
GRAVITY       = 9.81       # m/s²
RHO_AIR       = 1.225      # kg/m³
CP_AIR        = 1005.0     # J/(kg·K)
EARTH_ROT     = 7.27e-5    # rad/s  (Earth's rotation rate)
MIN_TKE       = 1.0e-15    # m²/s²
MIN_NUT       = 1.0e-5     # m²/s
MIN_STAB      = 0.1        # minimum stability factor

# ---------------------------------------------------------------------------
# Helper: mixing length (Blackadar + stability)
# ---------------------------------------------------------------------------
def _mixing_length(z_i, z_lower, fg, f_coriolis, lapse_rate_idx_dz):
    """Blackadar mixing length between z_i and z_lower [m].

    Parameters
    ----------
    z_i         : float  – height of level i  [m]
    z_lower     : float  – height of surface level [m]
    fg          : float  – geostrophic wind speed |Ug| [m/s]
    f_coriolis  : float  – Coriolis parameter [1/s]
    lapse_rate_idx_dz : unused placeholder for future stability extension
    """
    lshear = KAPPA * max(z_i - z_lower, MIN_STAB)
    f_abs  = abs(f_coriolis)
    lmax   = 0.00027 * fg / f_abs if f_abs > 1.0e-8 else 1000.0
    inv_l_sq = 1.0 / (lshear * lshear) + 1.0 / (lmax * lmax)
    return 1.0 / math.sqrt(inv_l_sq)

# ---------------------------------------------------------------------------
# cmu coefficient (stability-dependent, matches compute_cmu in scm_models.H)
# ---------------------------------------------------------------------------
def _cmu(Ri):
    cmu0 = 0.1
    if Ri > 0.0:
        return cmu0 / (1.0 + 10.0 * Ri)
    elif Ri < 0.0:
        return cmu0 * math.sqrt(1.0 - 5.0 * Ri)
    return cmu0

# ---------------------------------------------------------------------------
# Damping coefficient (matches update_wind_x/y in scm_models.H)
# ---------------------------------------------------------------------------
def _damping_coeff(z_i, z_upper, d_full=100.0, d_rd=50.0):
    dist = z_upper - z_i
    if dist > d_rd + d_full:
        return 0.0
    elif dist > d_full:
        return 0.5 * math.cos(math.pi * (z_upper - d_full - z_i) / d_rd) + 0.5
    return 1.0

# ---------------------------------------------------------------------------
# run_scm_1d
# Core 1D time-stepping SCM matching scm_models.H (one_equation turbulence)
# ---------------------------------------------------------------------------
def run_scm_1d(ug, vg, params, num_steps=50000, conv_tol=0.01):
    """Run 1D SCM and return wind components at met mast height.

    Implements the same algorithm as:
      initialize_1d_state / compute_similarity_surface /
      compute_eddy_viscosity / update_wind_x / update_wind_y /
      update_temperature / update_tke / run_1d_scm
    in scm_models.H.

    Parameters
    ----------
    ug, vg      : float  – geostrophic wind components [m/s]
    params      : dict   – simulation parameters
    num_steps   : int    – maximum number of time steps
    conv_tol    : float  – convergence tolerance on |Δspeed| [m/s]

    Returns
    -------
    dict with keys: ux_mast, uy_mast, ustar, bl_height_ri, profile_z, profile_ux, profile_uy
    """
    nz          = params['nz']          # number of grid levels (101)
    dz          = params['dz']          # vertical spacing [m]  (10)
    z0          = params['z0']          # roughness length [m]  (0.1)
    T_ref       = params['T_ref']       # surface temperature [K]  (288.15)
    lapse_rate  = params['lapse_rate']  # K/m  (0.01)
    lat_deg     = params['latitude']    # degrees  (45)
    mast_h      = params['mast_height'] # met mast height [m]  (150)

    # Height grid: z[0]=0, z[1]=dz, ..., z[nz-1]=(nz-1)*dz
    z = [i * dz for i in range(nz)]
    z_upper = z[-1]                     # domain top [m]

    # Coriolis parameter
    f = 2.0 * EARTH_ROT * math.sin(math.radians(lat_deg))

    # Geostrophic wind speed (used for mixing length lmax)
    fg = math.sqrt(ug * ug + vg * vg)

    # Find surface (lower) index: first i where z[i] >= z0
    lower = 0
    for i in range(nz):
        if z[i] >= z0:
            lower = i
            break

    # ---- Initialise state (matches initialize_1d_state) ----
    ux  = [ug] * nz                                  # u-wind [m/s]
    uy  = [vg] * nz                                  # v-wind [m/s]
    T   = [T_ref - lapse_rate * z[i] for i in range(nz)]  # temperature [K]
    tke = [0.1] * nz                                 # TKE [m²/s²]
    nut = [MIN_NUT] * nz                             # eddy viscosity [m²/s]
    lsc = [0.1] * nz                                 # mixing length [m]

    # Initial dt
    fg_safe = max(fg, 0.1)
    dt = min(max(0.8 * dz / fg_safe, 0.01), 2.0)

    # ---- Time integration ----
    speed_old = 0.0
    for step in range(num_steps):

        # -- Surface layer (neutral, matches compute_similarity_surface) --
        M1 = math.sqrt(ux[lower + 1] ** 2 + uy[lower + 1] ** 2)
        if M1 > 0.0:
            ustar = KAPPA * M1 / math.log((z[lower + 1] + z0) / z0)
        else:
            ustar = 0.0

        # Surface eddy viscosity and wind (log-law BC)
        nut[lower] = ustar * KAPPA * z0
        if M1 > 0.0:
            log_z0   = math.log((z[lower + 1] + z0) / z0)
            M0 = ustar / KAPPA * math.log((z[lower] + z0) / z0) if z[lower] > 0.0 \
                 else ustar / KAPPA * math.log((z0 + z0) / z0)
            # Neutral log-law: M0 = M1 * log((z0+z0)/z0) / log((z1+z0)/z0)
            # Use M0 = ustar/kappa * ln((z+z0)/z0) at z=z[lower+1]→ re-derive
            # (matches C++ "Neutral case" branch in compute_similarity_surface)
            z0_phi   = 1.0
            psi_diff = z0_phi - 1.0   # both neutral → 0
            log_term = math.log((z[lower + 1] + z0) / z0)
            M0_val   = ustar / KAPPA * (log_term - psi_diff)
            ux[lower] = M0_val * ux[lower + 1] / M1
            uy[lower] = M0_val * uy[lower + 1] / M1

        # -- Eddy viscosity (one_equation model, matches compute_eddy_viscosity) --
        for i in range(lower + 1, nz - 1):
            ls = _mixing_length(z[i], z[lower], fg, f, None)
            lsc[i] = ls

            # Backward-difference shear (matches C++ dTdz/dudz convention)
            dTdz  = (T[i] - T[i - 1]) / dz
            N2    = (GRAVITY / T_ref) * dTdz
            shear_mag = math.sqrt((ux[i] - ux[i - 1]) ** 2 +
                                  (uy[i] - uy[i - 1]) ** 2) / dz
            shear_sq = shear_mag * shear_mag + 1.0e-12
            Ri = N2 / shear_sq

            cmu = _cmu(Ri)
            nut[i] = max(cmu * math.sqrt(max(tke[i], MIN_TKE)) * ls, MIN_NUT)

        # Top BC for eddy viscosity
        nut[nz - 1] = nut[nz - 2]
        lsc[nz - 1] = lsc[nz - 2]
        tke[nz - 1] = tke[nz - 2]

        # -- Wind, temperature and TKE update (in-place, Gauss-Seidel order) --
        # Matches the sequential update in run_1d_scm / update_wind_x/y/tke
        for i in range(lower + 1, nz - 1):
            coeff = _damping_coeff(z[i], z_upper)

            # ux update (matches update_wind_x)
            term1_x  = nut[i] * (ux[i + 1] - 2.0 * ux[i] + ux[i - 1]) / (dz * dz)
            dudz_fwd = (ux[i + 1] - ux[i]) / dz          # forward difference for term2
            term2_x  = 0.5 / dz * (nut[i + 1] - nut[i - 1]) * dudz_fwd
            cor_x    = f * uy[i]
            geo_x    = -f * vg
            damp_x   = coeff * (ug - ux[i]) / 20.0
            ux[i]   += dt * (term1_x + term2_x + cor_x + geo_x + damp_x)

            # uy update (matches update_wind_y)
            term1_y  = nut[i] * (uy[i + 1] - 2.0 * uy[i] + uy[i - 1]) / (dz * dz)
            dvdz_fwd = (uy[i + 1] - uy[i]) / dz
            term2_y  = 0.5 / dz * (nut[i + 1] - nut[i - 1]) * dvdz_fwd
            cor_y    = -f * ux[i]
            geo_y    = f * ug
            damp_y   = coeff * (vg - uy[i]) / 20.0
            uy[i]   += dt * (term1_y + term2_y + cor_y + geo_y + damp_y)

            # Temperature update (matches update_temperature, neutral Qh=0)
            Kh_ip  = 0.5 * (nut[i] + nut[i + 1])
            Kh_im  = 0.5 * (nut[i] + nut[i - 1])
            fl_ip  = -Kh_ip * (T[i + 1] - T[i]) / dz
            fl_im  = -Kh_im * (T[i] - T[i - 1]) / dz
            T[i]  += dt * (-(fl_ip - fl_im) / dz)

            # TKE update (matches update_tke, one_equation)
            t1_k   = nut[i] * (tke[i + 1] - 2.0 * tke[i] + tke[i - 1]) / (dz * dz)
            dtkedz = (tke[i] - tke[i - 1]) / dz
            t2_k   = (1.0 / dz) * (nut[i] - nut[i - 1]) * dtkedz
            du_k   = (ux[i] - ux[i - 1]) / dz
            dv_k   = (uy[i] - uy[i - 1]) / dz
            prod   = nut[i] * (du_k * du_k + dv_k * dv_k)
            dTdz_k = (T[i] - T[i - 1]) / dz
            N2_k   = (GRAVITY / T_ref) * dTdz_k
            buoy   = -nut[i] * N2_k
            ls_eps = max(lsc[i], MIN_STAB)
            diss   = 1.92 * max(tke[i], MIN_TKE) ** 1.5 / ls_eps
            tke[i] = max(tke[i] + dt * (prod + buoy + t1_k + t2_k - diss), MIN_TKE)

        # -- Adaptive dt (matches run_1d_scm) --
        max_wind = 0.01
        for i in range(nz):
            w = math.sqrt(ux[i] * ux[i] + uy[i] * uy[i])
            max_wind = max(max_wind, w)
        dt = min(max(0.8 * dz / max_wind, 0.01), 2.0)

        # -- Convergence check (skip first 100 steps) --
        if step > 100:
            # Find mast grid point
            mast_idx, min_d = 0, abs(z[0] - mast_h)
            for i in range(1, nz):
                d = abs(z[i] - mast_h)
                if d < min_d:
                    min_d, mast_idx = d, i
            speed_now = math.sqrt(ux[mast_idx] ** 2 + uy[mast_idx] ** 2)
            if abs(speed_now - speed_old) < conv_tol:
                break
            speed_old = speed_now

    # ---- Post-processing diagnostics ----
    # Final ustar from converged profile
    M1_fin = math.sqrt(ux[lower + 1] ** 2 + uy[lower + 1] ** 2)
    ustar_fin = KAPPA * M1_fin / math.log((z[lower + 1] + z0) / z0) if M1_fin > 0.0 else 0.0

    # Bulk Richardson number BL height (Richardson criterion, matches Python reference)
    # Ri_b = (g/theta_s) * (theta(z) - theta_s) * (z - z_s) / |U - U_s|^2
    # BL height = z where Ri_b first exceeds critical value 0.25
    Ri_cr = 0.25
    bl_height = z_upper          # default: no stable layer found
    theta_s = T[lower]           # simplified: theta ≈ T for small z
    u_s, v_s = ux[lower], uy[lower]
    for i in range(lower + 1, nz):
        du  = ux[i] - u_s
        dv  = uy[i] - v_s
        wsq = du * du + dv * dv + 0.01    # small floor to prevent /0
        dth = T[i] - theta_s              # approx. potential temperature jump
        dz_blh = z[i] - z[lower]
        Ri_b = (GRAVITY / theta_s) * dth * dz_blh / wsq
        if Ri_b >= Ri_cr:
            bl_height = z[i]
            break

    # Extract wind at met mast height
    mast_idx, min_d = 0, abs(z[0] - mast_h)
    for i in range(1, nz):
        d = abs(z[i] - mast_h)
        if d < min_d:
            min_d, mast_idx = d, i

    return {
        'ux_mast'   : ux[mast_idx],
        'uy_mast'   : uy[mast_idx],
        'ustar'     : ustar_fin,
        'bl_height' : bl_height,
        'profile_z' : z,
        'profile_ux': ux,
        'profile_uy': uy,
    }


# ---------------------------------------------------------------------------
# find_geostrophic_wind
# Outer iteration: matches find_geostrophic_wind_1d_scm in scm_models.H
# ---------------------------------------------------------------------------
def find_geostrophic_wind(target_ux, target_uy, params,
                          initial_ug=10.0, initial_vg=-12.0,
                          tolerance=0.05, max_outer=50, num_steps=50000):
    """Iteratively adjust Ug, Vg until wind at mast matches target.

    Algorithm (matches updated find_geostrophic_wind_1d_scm in scm_models.H):
    - Adjust BOTH components each outer iteration when they exceed tolerance
    - Step size: max(0.5 * |error|, tolerance)
    """
    ug, vg = initial_ug, initial_vg
    result = None

    for outer in range(max_outer):
        result = run_scm_1d(ug, vg, params, num_steps=num_steps)
        err_u  = result['ux_mast'] - target_ux
        err_v  = result['uy_mast'] - target_uy

        if abs(err_u) < tolerance and abs(err_v) < tolerance:
            break

        if abs(err_u) > tolerance:
            step_u = max(0.5 * abs(err_u), tolerance)
            ug -= math.copysign(step_u, err_u)
        if abs(err_v) > tolerance:
            step_v = max(0.5 * abs(err_v), tolerance)
            vg -= math.copysign(step_v, err_v)

    return ug, vg, result


# ---------------------------------------------------------------------------
# verify_scm_initialization
# ---------------------------------------------------------------------------
def verify_scm_initialization(input_file, work_dir):
    """Run the Python 1D SCM with reference parameters and validate results."""

    print("=" * 70)
    print("SCM Initialization Regression Test — Python 1D SCM Verification")
    print("=" * 70)

    # ---- Check inputs.i exists and contains required keys ----
    try:
        with open(input_file, 'r') as fh:
            content = fh.read()

        required = ['init_mode = scm', 'scm_wind_speed', 'scm_ref_height',
                    'scm_wind_direction', 'scm_domain_height', 'scm_dz',
                    'scm_lapse_rate', 'scm_initial_ug', 'scm_initial_vg']
        missing = [k for k in required if k not in content]
        if missing:
            print(f"✗ FAIL: Missing SCM parameters in inputs.i: {missing}")
            return False
        print("✓ PASS: All required SCM parameters present in inputs.i")

        # Parse key parameter values from inputs.i for the SCM run
        def _parse(key, default):
            for line in content.splitlines():
                line = line.strip()
                if line.startswith('#'):
                    continue
                if key in line and '=' in line:
                    try:
                        return float(line.split('=')[1].split('#')[0].strip())
                    except ValueError:
                        pass
            return default

        wind_speed    = _parse('scm_wind_speed',      10.0)
        wind_dir_deg  = _parse('scm_wind_direction',   0.0)
        ref_height    = _parse('scm_ref_height',      150.0)
        T_ref         = _parse('scm_ref_temperature', 288.15)
        lapse_rate    = _parse('scm_lapse_rate',        0.01)
        domain_height = _parse('scm_domain_height',  1000.0)
        dz            = _parse('scm_dz',               10.0)
        z0            = _parse('z0',                    0.1)
        latitude      = _parse('latitude',             45.0)
        initial_ug    = _parse('scm_initial_ug',       10.0)
        initial_vg    = _parse('scm_initial_vg',      -12.0)

    except Exception as exc:
        print(f"⚠ WARNING: Could not parse inputs.i: {exc}")
        # Fall back to reference case defaults
        wind_speed, wind_dir_deg, ref_height = 10.0, 0.0, 150.0
        T_ref, lapse_rate = 288.15, 0.01
        domain_height, dz, z0, latitude = 1000.0, 10.0, 0.1, 45.0
        initial_ug, initial_vg = 10.0, -12.0

    # ---- Compute target u/v components from speed + direction ----
    angle_rad = math.radians(wind_dir_deg)
    target_ux = wind_speed * math.cos(angle_rad)
    target_uy = wind_speed * math.sin(angle_rad)

    nz = int(round(domain_height / dz)) + 1   # 101 for domain=1000, dz=10

    params = {
        'nz'         : nz,
        'dz'         : dz,
        'z0'         : z0,
        'T_ref'      : T_ref,
        'lapse_rate' : lapse_rate,
        'latitude'   : latitude,
        'mast_height': ref_height,
    }

    print(f"\nRunning 1D SCM with parameters:")
    print(f"  nz={nz}, dz={dz} m, z0={z0} m, latitude={latitude}°")
    print(f"  lapse_rate={lapse_rate} K/m, T_ref={T_ref} K")
    print(f"  target wind at {ref_height} m: [{target_ux:.4f}, {target_uy:.4f}] m/s")
    print(f"  initial Ug/Vg guess: [{initial_ug}, {initial_vg}] m/s")

    # ---- Run geostrophic wind search ----
    # Use 50 000 inner steps for the stable reference case (matches Python MOL=500 setting)
    ug, vg, result = find_geostrophic_wind(
        target_ux, target_uy, params,
        initial_ug=initial_ug, initial_vg=initial_vg,
        tolerance=0.05, max_outer=50, num_steps=50000)

    ux_mast  = result['ux_mast']
    uy_mast  = result['uy_mast']
    ustar    = result['ustar']
    bl_h     = result['bl_height']

    err_u = ux_mast - target_ux
    err_v = uy_mast - target_uy

    # ---- Print summary ----
    print(f"\nResults:")
    print(f"  Specified Geostrophic Wind  : [{ug:.4f}, {vg:.4f}] m/s")
    print(f"  CFD Met Mast Wind           : [{ux_mast:.5f}, {uy_mast:.6f}] m/s")
    print(f"  Wind Error (CFD - target)   : [{err_u:.6f}, {err_v:.6f}] m/s")
    print(f"  Friction Velocity ustar     : {ustar:.6f} m/s")
    print(f"  Richardson BL Height        : {bl_h:.1f} m")

    # Reference values from hrrr_1dsolver_terrain.py
    ref_ug, ref_vg        = 13.9206,  -10.3659
    ref_ustar             = 0.633748
    ref_bl_h              = 800.0
    ref_ux_mast           = 9.83213
    ref_uy_mast           = 0.110555

    print(f"\nReference (hrrr_1dsolver_terrain.py):")
    print(f"  Geostrophic Wind  : [{ref_ug}, {ref_vg}] m/s")
    print(f"  CFD Met Mast Wind : [{ref_ux_mast}, {ref_uy_mast}] m/s")
    print(f"  Friction Velocity : {ref_ustar} m/s")
    print(f"  Richardson BL H   : {ref_bl_h} m")

    # ---- Validation checks ----
    all_pass = True
    allowed_wind_error   = 0.25   # m/s  (matches Python allowed_error)
    allowed_ug_error     = 2.0    # m/s  geostrophic may differ slightly
    allowed_ustar_error  = 0.15   # m/s
    allowed_bl_error     = 300.0  # m    BL height is approximate

    def _check(name, got, ref, allowed):
        nonlocal all_pass
        err = abs(got - ref)
        ok  = err <= allowed
        mark = "✓ PASS" if ok else "✗ FAIL"
        print(f"  {mark}: {name}: got {got:.4f}, ref {ref:.4f}, |err|={err:.4f} (tol {allowed})")
        if not ok:
            all_pass = False

    print("\nChecks:")
    _check("Met mast u-wind [m/s]", ux_mast, ref_ux_mast,   allowed_wind_error)
    _check("Met mast v-wind [m/s]", uy_mast, ref_uy_mast,   allowed_wind_error)
    _check("Ug geostrophic [m/s]",  ug,      ref_ug,        allowed_ug_error)
    _check("Vg geostrophic [m/s]",  vg,      ref_vg,        allowed_ug_error)
    _check("Friction velocity ustar [m/s]", ustar, ref_ustar, allowed_ustar_error)
    _check("BL height [m]",         bl_h,    ref_bl_h,      allowed_bl_error)

    # Strict component error check (primary acceptance criterion)
    err_u_ok = abs(err_u) < allowed_wind_error
    err_v_ok = abs(err_v) < allowed_wind_error
    if err_u_ok and err_v_ok:
        print(f"\n✓ PASS: Wind error within allowed_error={allowed_wind_error} m/s on both components")
    else:
        print(f"\n✗ FAIL: Wind error exceeds allowed_error={allowed_wind_error} m/s "
              f"(err_u={err_u:.4f}, err_v={err_v:.4f})")
        all_pass = False

    print("\n" + "=" * 70)
    if all_pass:
        print("SCM initialization regression test PASSED.")
    else:
        print("SCM initialization regression test FAILED.")
    print("=" * 70)
    return all_pass


def main():
    parser = argparse.ArgumentParser(
        description="SCM initialization regression test: Python 1D SCM vs. reference")
    parser.add_argument('input_file', help='Path to inputs.i')
    parser.add_argument('work_dir',   help='Working directory')
    args = parser.parse_args()

    os.chdir(args.work_dir)
    ok = verify_scm_initialization(args.input_file, args.work_dir)
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
