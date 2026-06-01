// ==========================================================================
// wind_solver.cpp
// Terrain-following mass-consistent 3-D wind solver (QUIC-URB style)
//
// Reads a terrain file (X Y Z), a reference wind vector at a specified
// height, and surface roughness.  Constructs a log-law initial wind field
// over the terrain and enforces mass consistency by solving the anisotropic
// Poisson equation
//
//   -(α_h² ∂²λ/∂x² + α_h² ∂²λ/∂y² + α_v² ∂²λ/∂z²) = -(∇·u₀)
//
// via AMReX MLMG (MLABecLaplacian) on a single-level (level-0) 3-D
// Cartesian grid.  The corrected divergence-free wind field is written as
// an AMReX plotfile and, optionally, as a terrain-aligned CSV slice.
//
// Terrain-following initialisation:
//   For each horizontal column (i, j) the local terrain elevation z_s is
//   interpolated from the data file with inverse-distance weighting (IDW).
//   The vertical domain spans [z_lo, z_hi] where
//       z_lo = min terrain elevation   (= zs_min from the terrain file)
//       z_hi = max terrain elevation + domain_height
//   The physical height of cell centre (i, j, k) is
//       z_phys = z_lo + (k + 0.5) * dz
//   The height above ground level (AGL) for column (i, j) is
//       z_agl(i,j,k) = z_phys - z_terrain(i,j)
//   Cells where z_agl <= 0 are inside the terrain and are zeroed/masked.
//   The log-law profile is evaluated at z_agl:
//       u(z_agl) = u* / κ  * ln((z_agl + z0) / z0)
//       u*       = κ * |U_ref| / ln((z_ref + z0) / z0)
//   u* is constant (z_ref is height above local terrain), but z_agl varies
//   per column, so the log-law is applied independently for each (i, j).
//
// Mass-consistent correction (Lagrange multiplier method, Sherman 1978):
//   Minimise E = ∫[(u−u₀)²/α_h² + (v−v₀)²/α_h² + (w−w₀)²/α_v²] dV
//   subject to ∇·u = 0.
//   Euler-Lagrange conditions give:
//       u = u₀ − α_h² ∂λ/∂x
//       v = v₀ − α_h² ∂λ/∂y
//       w = w₀ − α_v² ∂λ/∂z
//   Substituting into ∇·u = 0 yields the anisotropic Poisson equation
//   solved above.
//
// Terrain-aligned extraction:
//   After the corrected wind field is computed, an optional 2-D slice can be
//   extracted and written as a CSV.  The slice is taken at a fixed k-index
//   (constant physical z = z_lo + (k+0.5)*dz), so each row reports the
//   per-column terrain elevation and the resulting per-column AGL:
//       z_physical(k)  = z_lo + (k + 0.5) * dz   [m above sea-level]
//       z_agl(i,j)     = z_physical(k) - z_terrain(i,j)
//   Specify the extraction level with ONE of:
//       extract_agl  = <height_m>   # target AGL [m] above minimum terrain (snapped to nearest cell)
//       extract_k    = <k_index>    # explicit k-index (0 = lowest cell)
//   The output CSV (extract_file) has columns:
//       x, y, z_terrain, z_physical, z_agl, u, v, w, speed
//
// Usage:  wind_solver inputs.i   (or  wind_solver key=value ...)
//
// Key parameters (with defaults):
//   terrain_file  = terrain.csv   # X Y Z, whitespace- or comma-separated
//   U_ref         = 10.0          # reference wind x-component [m/s]
//   V_ref         = 0.0           # reference wind y-component [m/s]
//   z_ref         = 10.0          # reference height above local terrain [m]
//   z0            = 0.1           # aerodynamic roughness length [m]
//   dx            = 30.0          # grid spacing x [m]
//   dy            = 30.0          # grid spacing y [m]
//   dz            = 30.0          # grid spacing z [m]
//   domain_height = 300.0         # vertical extent above maximum terrain elevation [m]
//   alpha_h       = 1.0           # horizontal Lagrange anisotropy factor
//   alpha_v       = 1.0           # vertical   Lagrange anisotropy factor
//   mlmg_verbose  = 1             # MLMG verbosity (0 = silent, 4 = max)
//   tol_rel       = 1.e-8         # MLMG relative tolerance
//   mlmg_max_iter = 200           # MLMG maximum iterations
//   mlmg_max_fmg_iter = 20        # MLMG maximum FMG iterations
//   mlmg_pre_smooth = 16          # MLMG pre-smoothing iterations
//   mlmg_post_smooth = 16         # MLMG post-smoothing iterations
//   mlmg_bottom_solver = default  # MLMG bottom solver: default, bicgstab, cg, smoother
//   max_grid_size = 32            # maximum AMReX box size (per dimension; 64-256 for GPUs)
//   plot_file     = plt_wind      # output plotfile prefix
//   extract_agl   = -1.0          # terrain-aligned extraction AGL [m] (<0 = off)
//   extract_k     = -1            # explicit k-index extraction (<0 = off)
//   extract_file  = wind_extract.csv  # terrain-aligned CSV output filename
//   building_file = buildings.csv # optional CSV file with building boxes
//                                 # format: xmin xmax ymin ymax zmin zmax (one per line)
//                                 # buildings mask cells where z_phys < building_zmax
// ==========================================================================

#include "canopy_models.H"
#include "wake_models.H"
#include "math_constants.H"
#include "stability_models.H"
#include "porosity_models.H"
#include "wall_functions.H"
#include "buoyancy_models.H"

#include <AMReX.H>
#include <AMReX_ParmParse.H>
#include <AMReX_Print.H>
#include <AMReX_Geometry.H>
#include <AMReX_MultiFab.H>
#include <AMReX_BoxArray.H>
#include <AMReX_DistributionMapping.H>
#include <AMReX_MLABecLaplacian.H>
#include <AMReX_MLMG.H>
#include <AMReX_LO_BCTYPES.H>
#include <AMReX_PlotFileUtil.H>
#include <AMReX_GpuLaunch.H>
#include <AMReX_VisMF.H>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <stdexcept>

using namespace amrex;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr Real DISTANCE_EPSILON = Real(1.0e-12);  // threshold for exact spatial matches

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Read an X Y Z terrain file (whitespace or comma separated; '#' comments).
static void read_terrain_file(const std::string& filename,
                               std::vector<Real>& xd,
                               std::vector<Real>& yd,
                               std::vector<Real>& zd)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open terrain file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x, y, z;
        if (ss >> x >> y >> z) {
            xd.push_back(x);
            yd.push_back(y);
            zd.push_back(z);
        }
    }
    if (xd.empty())
        amrex::Abort("wind_solver: no data read from terrain file: " + filename);

    amrex::Print() << "wind_solver: read " << xd.size()
                   << " terrain points from " << filename << "\n";
}

// IDW interpolation: terrain height at query point (xq, yq)
// Uses k nearest data points with inverse-square-distance weights.
static Real idw_terrain(Real xq, Real yq,
                        const std::vector<Real>& x,
                        const std::vector<Real>& y,
                        const std::vector<Real>& z,
                        int k = 6)
{
    int n = static_cast<int>(x.size());
    k = std::min(k, n);

    // Squared distances to all data points
    std::vector<std::pair<Real, int>> d2(n);
    for (int i = 0; i < n; ++i) {
        Real dx = x[i] - xq;
        Real dy = y[i] - yq;
        d2[i] = {dx * dx + dy * dy, i};
    }
    // Partial sort: first k elements are the k nearest
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0, zval = 0.0;
    for (int i = 0; i < k; ++i) {
        if (d2[i].first < DISTANCE_EPSILON) return z[d2[i].second]; // exact hit
        Real w = Real(1.0) / d2[i].first;  // inverse-square-distance weight
        wsum += w;
        zval += w * z[d2[i].second];
    }
    return zval / wsum;
}

// IDW interpolation: wind velocity at query point (xq, yq)
// Uses k nearest data points with inverse-square-distance weights.
// Returns (ux, uy) pair.
static std::pair<Real, Real> idw_velocity(Real xq, Real yq,
                                          const std::vector<Real>& x,
                                          const std::vector<Real>& y,
                                          const std::vector<Real>& ux_data,
                                          const std::vector<Real>& uy_data,
                                          int k = 6)
{
    int n = static_cast<int>(x.size());
    k = std::min(k, n);

    // Squared distances to all data points
    std::vector<std::pair<Real, int>> d2(n);
    for (int i = 0; i < n; ++i) {
        Real dx = x[i] - xq;
        Real dy = y[i] - yq;
        d2[i] = {dx * dx + dy * dy, i};
    }
    // Partial sort: first k elements are the k nearest
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0, ux_val = 0.0, uy_val = 0.0;
    for (int i = 0; i < k; ++i) {
        if (d2[i].first < DISTANCE_EPSILON) {
            return {ux_data[d2[i].second], uy_data[d2[i].second]}; // exact hit
        }
        Real w = Real(1.0) / d2[i].first;  // inverse-square-distance weight
        wsum += w;
        ux_val += w * ux_data[d2[i].second];
        uy_val += w * uy_data[d2[i].second];
    }
    return {ux_val / wsum, uy_val / wsum};
}

// IDW interpolation: surface data (USTAR, Z0, U10, V10) at query point (xq, yq)
// Returns tuple: (ustar, z0, u10, v10)
static std::tuple<Real, Real, Real, Real> idw_surface_data(
    Real xq, Real yq,
    const std::vector<Real>& x,
    const std::vector<Real>& y,
    const std::vector<Real>& ustar_data,
    const std::vector<Real>& z0_data,
    const std::vector<Real>& u10_data,
    const std::vector<Real>& v10_data,
    int k = 6)
{
    int n = static_cast<int>(x.size());
    k = std::min(k, n);

    std::vector<std::pair<Real, int>> d2(n);
    for (int i = 0; i < n; ++i) {
        Real dx = x[i] - xq;
        Real dy = y[i] - yq;
        d2[i] = {dx * dx + dy * dy, i};
    }
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0, ustar_val = 0.0, z0_val = 0.0, u10_val = 0.0, v10_val = 0.0;
    for (int i = 0; i < k; ++i) {
        if (d2[i].first < DISTANCE_EPSILON) {
            int idx = d2[i].second;
            return {ustar_data[idx], z0_data[idx], u10_data[idx], v10_data[idx]};
        }
        Real w = Real(1.0) / d2[i].first;
        wsum += w;
        ustar_val += w * ustar_data[d2[i].second];
        z0_val += w * z0_data[d2[i].second];
        u10_val += w * u10_data[d2[i].second];
        v10_val += w * v10_data[d2[i].second];
    }
    return {ustar_val / wsum, z0_val / wsum, u10_val / wsum, v10_val / wsum};
}

// Read X Y Z U V velocity file (whitespace or comma separated; '#' comments).
// Used for RAWS or synthetic wind data initialization.
static void read_velocity_file(const std::string& filename,
                               std::vector<Real>& xd,
                               std::vector<Real>& yd,
                               std::vector<Real>& zd,
                               std::vector<Real>& ux,
                               std::vector<Real>& uy)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open velocity file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x, y, z, u_x, u_y;
        if (ss >> x >> y >> z >> u_x >> u_y) {
            xd.push_back(x);
            yd.push_back(y);
            zd.push_back(z);
            ux.push_back(u_x);
            uy.push_back(u_y);
        }
    }
    if (xd.empty())
        amrex::Abort("wind_solver: no data read from velocity file: " + filename);

    amrex::Print() << "wind_solver: read " << xd.size()
                   << " velocity points from " << filename << "\n";
}

// Read X Y Z0 roughness file (whitespace or comma separated; '#' comments).
// Format: X Y Z0
// where Z0 = aerodynamic roughness length [m]
static void read_roughness_file(const std::string& filename,
                                std::vector<Real>& xd,
                                std::vector<Real>& yd,
                                std::vector<Real>& z0_d)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open roughness file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x, y, z0;
        if (ss >> x >> y >> z0) {
            xd.push_back(x);
            yd.push_back(y);
            z0_d.push_back(z0);
        }
    }
    if (xd.empty())
        amrex::Abort("wind_solver: no data read from roughness file: " + filename);

    amrex::Print() << "wind_solver: read " << xd.size()
                   << " roughness points from " << filename << "\n";
}

// Read X Y Z USTAR Z0 U10 V10 surface data file (whitespace or comma separated; '#' comments).
// Used for HRRR-style surface parameters with per-column friction velocity and roughness.
// Format: X Y Z USTAR Z0 U10 V10
// where USTAR = friction velocity [m/s], Z0 = roughness length [m], U10/V10 = 10m wind [m/s]
static void read_surface_data_file(const std::string& filename,
                                   std::vector<Real>& xd,
                                   std::vector<Real>& yd,
                                   std::vector<Real>& zd,
                                   std::vector<Real>& ustar_d,
                                   std::vector<Real>& z0_d,
                                   std::vector<Real>& u10_d,
                                   std::vector<Real>& v10_d)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open surface data file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x, y, z, ustar, z0, u10, v10;
        if (ss >> x >> y >> z >> ustar >> z0 >> u10 >> v10) {
            xd.push_back(x);
            yd.push_back(y);
            zd.push_back(z);
            ustar_d.push_back(ustar);
            z0_d.push_back(z0);
            u10_d.push_back(u10);
            v10_d.push_back(v10);
        }
    }
    if (xd.empty())
        amrex::Abort("wind_solver: no data read from surface data file: " + filename);

    amrex::Print() << "wind_solver: read " << xd.size()
                   << " surface data points from " << filename << "\n";
}

// Read Z T temperature profile file (whitespace or comma separated; '#' comments).
// Format: Z T
// where Z = height above sea level [m], T = temperature [K]
static void read_temperature_file(const std::string& filename,
                                  std::vector<Real>& zd,
                                  std::vector<Real>& Td)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open temperature file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real z, T;
        if (ss >> z >> T) {
            zd.push_back(z);
            Td.push_back(T);
        }
    }
    if (zd.empty())
        amrex::Abort("wind_solver: no data read from temperature file: " + filename);

    amrex::Print() << "wind_solver: read " << zd.size()
                   << " temperature profile points from " << filename << "\n";
}

// Read building file: xmin xmax ymin ymax zmin zmax (whitespace or comma separated; '#' comments).
static void read_building_file(const std::string& filename,
                               std::vector<Real>& xmin,
                               std::vector<Real>& xmax,
                               std::vector<Real>& ymin,
                               std::vector<Real>& ymax,
                               std::vector<Real>& zmin,
                               std::vector<Real>& zmax,
                               std::vector<Real>& rotation)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open building file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x1, x2, y1, y2, z1, z2, angle = 0.0;
        if (ss >> x1 >> x2 >> y1 >> y2 >> z1 >> z2) {
            // Phase 3 Enhancement: Optional rotation angle (7th column, in degrees)
            // If provided, angle is converted from degrees to radians
            if (ss >> angle) {
                angle = angle * MathConstants::deg_to_rad;
            }
            xmin.push_back(x1);
            xmax.push_back(x2);
            ymin.push_back(y1);
            ymax.push_back(y2);
            zmin.push_back(z1);
            zmax.push_back(z2);
            rotation.push_back(angle);
        }
    }
    if (xmin.empty())
        amrex::Abort("wind_solver: no data read from building file: " + filename);

    amrex::Print() << "wind_solver: read " << xmin.size()
                   << " building(s) from " << filename << "\n";
}

// Read porous building file: xmin xmax ymin ymax zmin zmax porosity [rotation_angle]
// (whitespace or comma separated; '#' comments).
static void read_porous_building_file(const std::string& filename,
                               std::vector<Real>& xmin,
                               std::vector<Real>& xmax,
                               std::vector<Real>& ymin,
                               std::vector<Real>& ymax,
                               std::vector<Real>& zmin,
                               std::vector<Real>& zmax,
                               std::vector<Real>& porosity,
                               std::vector<Real>& rotation)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open porous building file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real x1, x2, y1, y2, z1, z2, por = 0.0, angle = 0.0;
        if (ss >> x1 >> x2 >> y1 >> y2 >> z1 >> z2 >> por) {
            // Optional rotation angle (8th column, in degrees)
            if (ss >> angle) {
                angle = angle * MathConstants::deg_to_rad;
            }
            xmin.push_back(x1);
            xmax.push_back(x2);
            ymin.push_back(y1);
            ymax.push_back(y2);
            zmin.push_back(z1);
            zmax.push_back(z2);
            porosity.push_back(por);
            rotation.push_back(angle);
        }
    }
    if (xmin.empty())
        amrex::Abort("wind_solver: no data read from porous building file: " + filename);

    amrex::Print() << "wind_solver: read " << xmin.size()
                   << " porous building(s) from " << filename << "\n";
}

// Read time series file: time U_ref V_ref (whitespace or comma separated; '#' comments)
static void read_time_series_file(const std::string& filename,
                                   std::vector<Real>& times,
                                   std::vector<Real>& U_refs,
                                   std::vector<Real>& V_refs)
{
    std::ifstream f(filename);
    if (!f.is_open())
        amrex::Abort("wind_solver: cannot open time series file: " + filename);

    std::string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        // replace commas with spaces
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real t, u, v;
        if (ss >> t >> u >> v) {
            times.push_back(t);
            U_refs.push_back(u);
            V_refs.push_back(v);
        }
    }
    if (times.empty())
        amrex::Abort("wind_solver: no data read from time series file: " + filename);

    amrex::Print() << "wind_solver: read " << times.size()
                   << " time points from " << filename << "\n";
}

// WENO 3 stencil for derivative at interior points
// Assumes uniform grid spacing, returns du/dx with WENO-3 approximation
// Uses 3-point stencil: f[-1], f[0], f[+1] with spacing h
AMREX_GPU_DEVICE static Real weno3_deriv(Real fm1, Real f0, Real fp1, Real h)
{
    // WENO-3 is 3rd order accurate in smooth regions
    // Bias toward central difference with upwind bias for discontinuities
    const Real eps = Real(1.0e-12);
    
    // Compute smoothness indicators for each stencil
    Real IS_forward = (fp1 - f0) * (fp1 - f0);   // smoothness of [f0, fp1]
    Real IS_backward = (f0 - fm1) * (f0 - fm1);  // smoothness of [fm1, f0]
    
    // Weights
    const Real gamma_forward = Real(2.0) / Real(3.0);
    const Real gamma_backward = Real(1.0) / Real(3.0);
    Real w_forward = gamma_forward / ((eps + IS_forward) * (eps + IS_forward));
    Real w_backward = gamma_backward / ((eps + IS_backward) * (eps + IS_backward));
    Real w_sum = w_forward + w_backward;
    w_forward /= w_sum;
    w_backward /= w_sum;
    
    // Stencil derivatives (2nd order)
    Real d_forward = (fp1 - f0) / h;   // forward difference
    Real d_backward = (f0 - fm1) / h;  // backward difference
    
    // WENO combination
    return w_forward * d_forward + w_backward * d_backward;
}

// WENO 5 stencil for derivative at interior points
// Assumes uniform grid spacing, returns du/dx with WENO-5 approximation
// Uses 5-point stencil: f[-2], f[-1], f[0], f[+1], f[+2] with spacing h
AMREX_GPU_DEVICE static Real weno5_deriv(Real fm2, Real fm1, Real f0, Real fp1, Real fp2, Real h)
{
    // WENO-5 is 5th order accurate in smooth regions, 3rd order near shocks
    const Real eps = Real(1.0e-12);
    
    // Compute smoothness indicators for each sub-stencil
    Real IS_forward = (fp2 - Real(2.0)*fp1 + f0)*(fp2 - Real(2.0)*fp1 + f0)*Real(0.25) + 
                      (Real(3.0)*fp2 - Real(4.0)*fp1 + f0)*(Real(3.0)*fp2 - Real(4.0)*fp1 + f0)/Real(12.0);
    
    Real IS_central = (fp1 - Real(2.0)*f0 + fm1)*(fp1 - Real(2.0)*f0 + fm1)*Real(0.25) + 
                      (fp1 - fm1)*(fp1 - fm1)/Real(12.0);
    
    Real IS_backward = (f0 - Real(2.0)*fm1 + fm2)*(f0 - Real(2.0)*fm1 + fm2)*Real(0.25) + 
                       (Real(3.0)*f0 - Real(4.0)*fm1 + fm2)*(Real(3.0)*f0 - Real(4.0)*fm1 + fm2)/Real(12.0);
    
    // Weights
    const Real gamma_forward = Real(0.1);
    const Real gamma_central = Real(0.6);
    const Real gamma_backward = Real(0.3);
    Real w_forward = gamma_forward / ((eps + IS_forward) * (eps + IS_forward));
    Real w_central = gamma_central / ((eps + IS_central) * (eps + IS_central));
    Real w_backward = gamma_backward / ((eps + IS_backward) * (eps + IS_backward));
    Real w_sum = w_forward + w_central + w_backward;
    w_forward /= w_sum;
    w_central /= w_sum;
    w_backward /= w_sum;
    
    // Stencil derivatives (2nd order accurate)
    Real d_forward = (fp2 - Real(4.0)*fp1 + Real(3.0)*f0) / (Real(2.0)*h);  // forward biased
    Real d_central = (fp1 - fm1) / (Real(2.0)*h);                           // central
    Real d_backward = (Real(-3.0)*f0 + Real(4.0)*fm1 - fm2) / (Real(2.0)*h); // backward biased
    
    // WENO combination
    return w_forward * d_forward + w_central * d_central + w_backward * d_backward;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    amrex::Initialize(argc, argv);
    {
        // Performance timing
        Real t_total = amrex::second();
        Real t_phase = 0.0;
        
        // ----------------------------------------------------------------
        // 1. Parse user inputs
        // ----------------------------------------------------------------
        t_phase = amrex::second();
        ParmParse pp;

        std::string terrain_file = "terrain.csv";
        pp.query("terrain_file", terrain_file);

        // Wind initialization mode: "loglaw" (default), "uniform", "raws", "surface_data", or "powerlaw"
        // "loglaw"       : use log-law profile with U_ref, V_ref at z_ref height
        // "uniform"      : constant horizontal wind (uniform_U, uniform_V) at all heights
        // "raws"         : interpolate from velocity file (X Y Z U V format)
        // "surface_data" : construct vertical profiles from surface parameters (X Y Z USTAR Z0 U10 V10)
        // "powerlaw"     : power-law profile u(z) = U_ref * (z/z_ref)^alpha
        std::string init_mode = "loglaw";
        pp.query("init_mode", init_mode);

        // Validate init_mode
        if (init_mode != "loglaw" && init_mode != "uniform" && init_mode != "raws" && 
            init_mode != "surface_data" && init_mode != "powerlaw") {
            amrex::Abort("wind_solver: invalid init_mode: " + init_mode + 
                         " (must be 'loglaw', 'uniform', 'raws', 'surface_data', or 'powerlaw')");
        }

        Real U_ref = 10.0;  // x-component of reference wind [m/s]
        Real V_ref =  0.0;  // y-component of reference wind [m/s]
        Real z_ref = 10.0;  // reference height above local terrain [m]
        Real z0    =  0.1;  // aerodynamic roughness length [m]
        pp.query("U_ref", U_ref);
        pp.query("V_ref", V_ref);
        pp.query("z_ref", z_ref);
        pp.query("z0",    z0);

        // Canopy model parameters
        bool enable_canopy = false;
        Real canopy_height = 0.0;
        Real frontal_area_index = 0.0;
        Real plan_area_index = 0.0;
        Real canopy_drag_coeff = 0.2;
        Real canopy_attenuation = 2.5;
        bool use_exponential_profile = false;
        pp.query("enable_canopy", enable_canopy);
        pp.query("canopy_height", canopy_height);
        pp.query("frontal_area_index", frontal_area_index);
        pp.query("plan_area_index", plan_area_index);
        pp.query("canopy_drag_coeff", canopy_drag_coeff);
        pp.query("canopy_attenuation", canopy_attenuation);
        pp.query("use_exponential_profile", use_exponential_profile);

        // Wake model parameters
        bool enable_wake = false;
        Real wake_c1 = 0.9;  // Cavity length coefficient
        Real wake_c2 = 0.3;  // Wake deficit coefficient
        Real wake_separation_length = 3.0;  // Wake separation length factor
        bool wake_superposition = true;  // Use wake superposition for multiple buildings
        pp.query("enable_wake", enable_wake);
        pp.query("wake_c1", wake_c1);
        pp.query("wake_c2", wake_c2);
        pp.query("wake_separation_length", wake_separation_length);
        pp.query("wake_superposition", wake_superposition);
        
        // Street canyon parameters
        bool enable_street_canyon = false;
        Real street_canyon_reduction = 0.3;  // Velocity reduction factor in canyon (0-1)
        pp.query("enable_street_canyon", enable_street_canyon);
        pp.query("street_canyon_reduction", street_canyon_reduction);

        // Uniform mode parameters
        Real uniform_U = U_ref;  // default to U_ref
        Real uniform_V = V_ref;  // default to V_ref
        pp.query("uniform_U", uniform_U);
        pp.query("uniform_V", uniform_V);

        // Power-law mode parameters
        Real powerlaw_exponent = 0.143;  // ~1/7 typical for neutral conditions
        pp.query("powerlaw_exponent", powerlaw_exponent);

        // RAWS mode parameters
        std::string velocity_file = "velocity.csv";
        pp.query("velocity_file", velocity_file);

        // Surface data mode parameters (for HRRR-style initialization)
        std::string surface_data_file = "surface_data.csv";
        pp.query("surface_data_file", surface_data_file);

        // Position-dependent roughness file (for spatially-varying z0)
        std::string z0_file = "";
        bool use_z0_file = false;
        pp.query("z0_file", z0_file);
        if (!z0_file.empty()) {
            use_z0_file = true;
        }

        Real dx_req = 30.0;
        Real dy_req = 30.0;
        Real dz_req = 30.0;
        pp.query("dx", dx_req);
        pp.query("dy", dy_req);
        pp.query("dz", dz_req);

        Real domain_height = 300.0;  // [m] vertical domain extent
        pp.query("domain_height", domain_height);

        Real alpha_h = 1.0;  // horizontal Lagrange anisotropy coeff
        Real alpha_v = 1.0;  // vertical   Lagrange anisotropy coeff
        pp.query("alpha_h", alpha_h);
        pp.query("alpha_v", alpha_v);

        // Height-dependent alpha_v
        // Allow alpha_v to vary linearly with height: alpha_v(z) = alpha_v_surface + (alpha_v_top - alpha_v_surface) * (z - z_lo) / (z_hi - z_lo)
        bool use_height_dependent_alpha_v = false;
        Real alpha_v_surface = alpha_v;  // alpha_v at surface (default to constant alpha_v)
        Real alpha_v_top = alpha_v;      // alpha_v at domain top (default to constant alpha_v)
        pp.query("use_height_dependent_alpha_v", use_height_dependent_alpha_v);
        pp.query("alpha_v_surface", alpha_v_surface);
        pp.query("alpha_v_top", alpha_v_top);

        // Non-Neutral Log-Law (Businger-Dyer profiles)
        // Stability correction parameters for Monin-Obukhov similarity theory
        bool enable_stability_correction = false;
        Real stability_length = 1000.0;  // Obukhov length L [m] (>0 stable, <0 unstable, very large for neutral)
        pp.query("enable_stability_correction", enable_stability_correction);
        pp.query("stability_length", stability_length);

        // Elevation-Dependent Wind Speed Scaling
        // Scale reference wind based on terrain elevation for mountain-valley effects
        bool enable_elevation_scaling = false;
        Real elevation_scaling_factor = 0.0;    // Scaling factor (0 = no scaling)
        Real elevation_height_scale = 1000.0;   // Characteristic height scale [m]
        pp.query("enable_elevation_scaling", enable_elevation_scaling);
        pp.query("elevation_scaling_factor", elevation_scaling_factor);
        pp.query("elevation_height_scale", elevation_height_scale);

        // Time-Varying Wind Boundary Conditions
        // Allow time-dependent inflow conditions for transient simulations
        bool enable_time_varying = false;
        std::string time_series_file = "time_series.csv";
        pp.query("enable_time_varying", enable_time_varying);
        pp.query("time_series_file", time_series_file);

        // Building Porosity Model
        // Allow partial flow through porous buildings (trees, fences)
        bool enable_building_porosity = false;
        std::string building_porosity_file = "";
        Real default_building_porosity = 0.0;  // Default porosity (0 = solid)
        Real porosity_drag_coefficient = 0.2;  // Drag coefficient for porous flow
        pp.query("enable_building_porosity", enable_building_porosity);
        pp.query("building_porosity_file", building_porosity_file);
        pp.query("default_building_porosity", default_building_porosity);
        pp.query("porosity_drag_coefficient", porosity_drag_coefficient);

        // Wall Function Parameters
        // NEW REQUIREMENT: Allow switching between no-slip and log-law boundary conditions
        // Default is false (no-slip) for backward compatibility
        bool enable_wall_functions = false;
        bool enable_terrain_wall_function = false;
        bool enable_flat_surface_wall_function = false;
        bool enable_building_wall_function = false;
        Real wall_function_z0_building = 0.001;  // Building wall roughness [m]
        Real wall_function_z0_flat = 0.01;       // Flat surface roughness [m]
        Real wall_function_blend_height = 2.0;   // Blending layer height [cells]
        Real wall_function_max_distance = 3.0;   // Max distance for wall function [cells]
        Real wall_function_flat_surface_elevation = 0.0;  // Elevation of flat surface [m]
        bool wall_function_enable_flat_surface = false;   // Use flat surface mode
        Real wall_function_min_wall_distance = 0.1;       // Minimum distance from wall [m]
        
        // Stability correction for wall functions
        bool wall_function_enable_stability = false;     // Enable Monin-Obukhov corrections
        Real wall_function_stability_length = 1.0e10;    // Obukhov length L [m]
        
        // Adaptive activation based on grid resolution
        bool wall_function_enable_adaptive = false;      // Enable adaptive activation
        Real wall_function_adaptive_threshold = 30.0;    // Max dz/z0 ratio for activation
        Real wall_function_adaptive_min_cells = 3.0;     // Min cells in log layer
        
        pp.query("enable_wall_functions", enable_wall_functions);
        pp.query("enable_terrain_wall_function", enable_terrain_wall_function);
        pp.query("enable_flat_surface_wall_function", enable_flat_surface_wall_function);
        pp.query("enable_building_wall_function", enable_building_wall_function);
        pp.query("wall_function_z0_building", wall_function_z0_building);
        pp.query("wall_function_z0_flat", wall_function_z0_flat);
        pp.query("wall_function_blend_height", wall_function_blend_height);
        pp.query("wall_function_max_distance", wall_function_max_distance);
        pp.query("wall_function_flat_surface_elevation", wall_function_flat_surface_elevation);
        pp.query("wall_function_enable_flat_surface", wall_function_enable_flat_surface);
        pp.query("wall_function_min_wall_distance", wall_function_min_wall_distance);
        
        // Query new stability and adaptive parameters
        pp.query("wall_function_enable_stability", wall_function_enable_stability);
        pp.query("wall_function_stability_length", wall_function_stability_length);
        pp.query("wall_function_enable_adaptive", wall_function_enable_adaptive);
        pp.query("wall_function_adaptive_threshold", wall_function_adaptive_threshold);
        pp.query("wall_function_adaptive_min_cells", wall_function_adaptive_min_cells);
        
        // Auto-enable sub-features if master enable is true
        if (enable_wall_functions) {
            if (!pp.contains("enable_terrain_wall_function")) {
                enable_terrain_wall_function = true;
            }
        }

        // Thermal Stratification with Buoyancy
        // Add buoyancy effects from temperature stratification to vertical momentum
        bool enable_buoyancy_stratification = false;
        std::string temperature_file = "temperature.csv";
        Real temperature_reference = 300.0;  // Reference temperature T₀ [K]
        Real buoyancy_coefficient = 1.0;     // Tuning parameter for buoyancy strength
        Real buoyancy_timescale = 10.0;      // Characteristic time scale Δt [s]
        pp.query("enable_buoyancy_stratification", enable_buoyancy_stratification);
        pp.query("temperature_file", temperature_file);
        pp.query("temperature_reference", temperature_reference);
        pp.query("buoyancy_coefficient", buoyancy_coefficient);
        pp.query("buoyancy_timescale", buoyancy_timescale);

        // Kinematic Terrain-Following Boundary Condition
        // Enforce w = u·∇h at terrain surface instead of simply zeroing
        bool enable_terrain_kinematic_bc = false;
        Real terrain_bc_relaxation = 1.0;  // Relaxation factor (1.0 = strict, <1.0 = relaxed)
        pp.query("enable_terrain_kinematic_bc", enable_terrain_kinematic_bc);
        pp.query("terrain_bc_relaxation", terrain_bc_relaxation);

        // Ekman Spiral Wind Veer Correction
        // Add wind direction rotation (veer) with height due to Coriolis effects
        bool enable_ekman_veer = false;
        Real latitude = 45.0;               // Latitude [degrees] for Coriolis parameter (positive = North)
        Real ekman_veer_total = 20.0;       // Total wind veer from surface to domain top [degrees]
        Real ekman_veer_height = 200.0;     // Height scale for veer profile [m]
        pp.query("enable_ekman_veer", enable_ekman_veer);
        pp.query("latitude", latitude);
        pp.query("ekman_veer_total", ekman_veer_total);
        pp.query("ekman_veer_height", ekman_veer_height);
        
        // Convert ekman_veer_total from degrees to radians for internal use
        Real ekman_veer_total_rad = ekman_veer_total * MathConstants::pi / Real(180.0);

        int  mlmg_verbose = 1;
        Real tol_rel      = 1.e-8;
        int  mlmg_max_iter = 200;
        int  mlmg_max_fmg_iter = 20;
        int  mlmg_pre_smooth = 16;
        int  mlmg_post_smooth = 16;
        std::string mlmg_bottom_solver = "default";
        int  max_grid_size = 32;
        std::string plot_file = "plt_wind";
        pp.query("mlmg_verbose",  mlmg_verbose);
        pp.query("tol_rel",       tol_rel);
        pp.query("mlmg_max_iter", mlmg_max_iter);
        pp.query("mlmg_max_fmg_iter", mlmg_max_fmg_iter);
        pp.query("mlmg_pre_smooth", mlmg_pre_smooth);
        pp.query("mlmg_post_smooth", mlmg_post_smooth);
        pp.query("mlmg_bottom_solver", mlmg_bottom_solver);
        pp.query("max_grid_size", max_grid_size);
        pp.query("plot_file",     plot_file);

         // Terrain-aligned extraction parameters
        // extract_agl  : sample at this height above local terrain [m]; snapped to
        //                the nearest cell-centre level.  Takes priority over extract_k.
        //                Can be a single value or comma-separated list: "10.0, 50.0, 100.0"
        // extract_k    : sample at this k-index (0 = lowest model level).
        // Either < 0 disables that mode.  If both are < 0, no extraction is written.
        std::vector<Real> extract_agl_list;
        std::vector<int>  extract_k_list;
        std::string extract_file = "wind_extract.csv";
        
        // Parse extract_agl (single value or space-separated list)
        {
            int n_agl = pp.countval("extract_agl");
            if (n_agl > 0) {
                extract_agl_list.resize(n_agl);
                pp.getarr("extract_agl", extract_agl_list, 0, n_agl);
            }
        }
        
        // Parse extract_k (single value or space-separated list)
        {
            int n_k = pp.countval("extract_k");
            if (n_k > 0) {
                extract_k_list.resize(n_k);
                pp.getarr("extract_k", extract_k_list, 0, n_k);
            }
        }
        
        pp.query("extract_file", extract_file);

        // Derivative computation method: "central", "weno3", or "weno5"
        // "central" (default): 2nd order central differences (one-sided at boundaries)
        // "weno3": 3rd order WENO scheme (WENO-3)
        // "weno5": 5th order WENO scheme (WENO-5)
        std::string deriv_method = "central";
        pp.query("deriv_method", deriv_method);
        
        // Validate deriv_method
        if (deriv_method != "central" && deriv_method != "weno3" && deriv_method != "weno5") {
            amrex::Abort("wind_solver: invalid deriv_method: " + deriv_method + 
                         " (must be 'central', 'weno3', or 'weno5')");
        }
        amrex::Print() << "wind_solver: using " << deriv_method << " derivatives\n";
        
        // Print timing for input parsing
        amrex::Print() << "wind_solver: input parsing time = " 
                       << (amrex::second() - t_phase) << " s\n";
        
        // Convert deriv_method string to integer for GPU capture
        // 0 = central, 1 = weno3, 2 = weno5
        int deriv_method_int = 0;
        if (deriv_method == "weno3") deriv_method_int = 1;
        else if (deriv_method == "weno5") deriv_method_int = 2;

        // ----------------------------------------------------------------
        // 2. Read terrain file and determine horizontal domain bounds
        // ----------------------------------------------------------------
        t_phase = amrex::second();
        std::vector<Real> x_terr, y_terr, z_terr;
        read_terrain_file(terrain_file, x_terr, y_terr, z_terr);

        Real x_lo = *std::min_element(x_terr.begin(), x_terr.end());
        Real x_hi = *std::max_element(x_terr.begin(), x_terr.end());
        Real y_lo = *std::min_element(y_terr.begin(), y_terr.end());
        Real y_hi = *std::max_element(y_terr.begin(), y_terr.end());

        amrex::Print() << "wind_solver: terrain x [" << x_lo << ", " << x_hi << "] m\n";
        amrex::Print() << "wind_solver: terrain y [" << y_lo << ", " << y_hi << "] m\n";

        // ----------------------------------------------------------------
        // 3. Determine horizontal grid dimensions from requested spacing
        // ----------------------------------------------------------------
        int nx = std::max(1, static_cast<int>(std::round((x_hi - x_lo) / dx_req)));
        int ny = std::max(1, static_cast<int>(std::round((y_hi - y_lo) / dy_req)));

        // Actual horizontal cell sizes (may differ slightly from requested if the
        // domain size is not an exact multiple of dx_req / dy_req).
        Real dx = (x_hi - x_lo) / nx;
        Real dy = (y_hi - y_lo) / ny;

        // ----------------------------------------------------------------
        // 4. Read building file (optional)
        //    Buildings are defined in a CSV file: xmin xmax ymin ymax zmin zmax [rotation]
        //    One building per line, whitespace or comma separated
        //    Optional rotation column (in degrees) specifies building orientation (Phase 3)
        // ----------------------------------------------------------------
        std::vector<Real> building_xmin, building_xmax;
        std::vector<Real> building_ymin, building_ymax;
        std::vector<Real> building_zmin, building_zmax;
        std::vector<Real> building_rotation;
        
        std::string building_file = "";
        pp.query("building_file", building_file);
        if (!building_file.empty()) {
            read_building_file(building_file, 
                             building_xmin, building_xmax,
                             building_ymin, building_ymax,
                             building_zmin, building_zmax,
                             building_rotation);
        }

        // Read porous building file (if enabled)
        std::vector<Real> porous_building_xmin, porous_building_xmax;
        std::vector<Real> porous_building_ymin, porous_building_ymax;
        std::vector<Real> porous_building_zmin, porous_building_zmax;
        std::vector<Real> porous_building_porosity;
        std::vector<Real> porous_building_rotation;
        
        if (enable_building_porosity && !building_porosity_file.empty()) {
            read_porous_building_file(building_porosity_file,
                                    porous_building_xmin, porous_building_xmax,
                                    porous_building_ymin, porous_building_ymax,
                                    porous_building_zmin, porous_building_zmax,
                                    porous_building_porosity,
                                    porous_building_rotation);
        }

        // Read time series file (if enabled)
        std::vector<Real> time_series_times;
        std::vector<Real> time_series_U_refs;
        std::vector<Real> time_series_V_refs;
        
        if (enable_time_varying) {
            read_time_series_file(time_series_file,
                                 time_series_times,
                                 time_series_U_refs,
                                 time_series_V_refs);
            
            // Override U_ref and V_ref with first time point
            // Note: Full time-stepping implementation would require restructuring the solver loop.
            // This implementation uses the first time point as a proof-of-concept.
            // Future enhancement: wrap solver in time loop for transient simulations.
            if (!time_series_times.empty()) {
                U_ref = time_series_U_refs[0];
                V_ref = time_series_V_refs[0];
                amrex::Print() << "wind_solver: time-varying mode enabled, using t=" 
                              << time_series_times[0] << " s with U_ref=" << U_ref 
                              << " m/s, V_ref=" << V_ref << " m/s\n";
                amrex::Print() << "wind_solver: note - full time-stepping requires solver loop restructuring\n";
                amrex::Print() << "wind_solver: for now, using first time point from series with " 
                              << time_series_times.size() << " total time points\n";
            }
        }

        amrex::Print() << "wind_solver: terrain reading time = " 
                       << (amrex::second() - t_phase) << " s\n";

        // ----------------------------------------------------------------
        // 5. Precompute per-column terrain height via IDW (host side)
        // ----------------------------------------------------------------
        t_phase = amrex::second();
        // terrain_h[j*nx + i] = interpolated elevation at column (i,j) [m]
        std::vector<Real> terrain_h(static_cast<std::size_t>(nx) * ny);

        for (int j = 0; j < ny; ++j) {
            Real yc = y_lo + (j + 0.5) * dy;
            for (int i = 0; i < nx; ++i) {
                Real xc = x_lo + (i + 0.5) * dx;
                terrain_h[static_cast<std::size_t>(j) * nx + i] =
                    idw_terrain(xc, yc, x_terr, y_terr, z_terr);
            }
        }

        // ----------------------------------------------------------------
        // 5a. Read temperature profile (if buoyancy stratification enabled)
        // ----------------------------------------------------------------
        std::vector<Real> z_temp, T_temp;  // Temperature profile data
        if (enable_buoyancy_stratification) {
            read_temperature_file(temperature_file, z_temp, T_temp);
            amrex::Print() << "wind_solver: buoyancy stratification enabled\n";
            amrex::Print() << "  temperature_reference = " << temperature_reference << " K\n";
            amrex::Print() << "  buoyancy_coefficient = " << buoyancy_coefficient << "\n";
            amrex::Print() << "  buoyancy_timescale = " << buoyancy_timescale << " s\n";
        }

        // ----------------------------------------------------------------
        // 5b. Compute terrain gradients (if kinematic BC enabled)
        // ----------------------------------------------------------------
        std::vector<Real> terrain_grad_x(static_cast<std::size_t>(nx) * ny, 0.0);
        std::vector<Real> terrain_grad_y(static_cast<std::size_t>(nx) * ny, 0.0);
        
        if (enable_terrain_kinematic_bc) {
            amrex::Print() << "wind_solver: kinematic terrain BC enabled\n";
            amrex::Print() << "  terrain_bc_relaxation = " << terrain_bc_relaxation << "\n";
            
            // Compute ∂h/∂x and ∂h/∂y using central differences
            for (int j = 0; j < ny; ++j) {
                for (int i = 0; i < nx; ++i) {
                    std::size_t idx = static_cast<std::size_t>(j) * nx + i;
                    
                    // ∂h/∂x: central difference (one-sided at boundaries)
                    if (i == 0) {
                        // Forward difference
                        Real h_ip1 = terrain_h[static_cast<std::size_t>(j) * nx + (i+1)];
                        Real h_i   = terrain_h[idx];
                        terrain_grad_x[idx] = (h_ip1 - h_i) / dx;
                    } else if (i == nx - 1) {
                        // Backward difference
                        Real h_i   = terrain_h[idx];
                        Real h_im1 = terrain_h[static_cast<std::size_t>(j) * nx + (i-1)];
                        terrain_grad_x[idx] = (h_i - h_im1) / dx;
                    } else {
                        // Central difference
                        Real h_ip1 = terrain_h[static_cast<std::size_t>(j) * nx + (i+1)];
                        Real h_im1 = terrain_h[static_cast<std::size_t>(j) * nx + (i-1)];
                        terrain_grad_x[idx] = (h_ip1 - h_im1) / (2.0 * dx);
                    }
                    
                    // ∂h/∂y: central difference (one-sided at boundaries)
                    if (j == 0) {
                        // Forward difference
                        Real h_jp1 = terrain_h[static_cast<std::size_t>(j+1) * nx + i];
                        Real h_j   = terrain_h[idx];
                        terrain_grad_y[idx] = (h_jp1 - h_j) / dy;
                    } else if (j == ny - 1) {
                        // Backward difference
                        Real h_j   = terrain_h[idx];
                        Real h_jm1 = terrain_h[static_cast<std::size_t>(j-1) * nx + i];
                        terrain_grad_y[idx] = (h_j - h_jm1) / dy;
                    } else {
                        // Central difference
                        Real h_jp1 = terrain_h[static_cast<std::size_t>(j+1) * nx + i];
                        Real h_jm1 = terrain_h[static_cast<std::size_t>(j-1) * nx + i];
                        terrain_grad_y[idx] = (h_jp1 - h_jm1) / (2.0 * dy);
                    }
                }
            }
        }

        // ----------------------------------------------------------------
        // 6. Compute obstacle height field (terrain + buildings)
        //    z_obstacle[j*nx + i] = max(terrain_height, building_top)
        //    For each cell, check all buildings to see if cell is inside any building
        // ----------------------------------------------------------------
        std::vector<Real> obstacle_h = terrain_h;  // Start with terrain
        
        if (!building_xmin.empty()) {
            int n_buildings = static_cast<int>(building_xmin.size());
            for (int b = 0; b < n_buildings; ++b) {
                Real bx1 = building_xmin[b];
                Real bx2 = building_xmax[b];
                Real by1 = building_ymin[b];
                Real by2 = building_ymax[b];
                Real bz1 = building_zmin[b];
                Real bz2 = building_zmax[b];
                
                amrex::Print() << "wind_solver: building " << b + 1 
                               << ": x=[" << bx1 << ", " << bx2 << "] m"
                               << ", y=[" << by1 << ", " << by2 << "] m"
                               << ", z=[" << bz1 << ", " << bz2 << "] m\n";
                
                // Mark all cells within building footprint
                for (int j = 0; j < ny; ++j) {
                    Real yc = y_lo + (j + 0.5) * dy;
                    for (int i = 0; i < nx; ++i) {
                        Real xc = x_lo + (i + 0.5) * dx;
                        // Check if cell center is inside building footprint
                        if (xc >= bx1 && xc <= bx2 && yc >= by1 && yc <= by2) {
                            std::size_t idx = static_cast<std::size_t>(j) * nx + i;
                            // Building height (relative to its base)
                            Real building_height = bz2 - bz1;
                            // Set obstacle height to terrain + building height (terrain-aligned)
                            // If bz1 > 0, it's treated as an absolute offset that gets added to terrain
                            Real adjusted_building_top = terrain_h[idx] + building_height;
                            obstacle_h[idx] = std::max(obstacle_h[idx], adjusted_building_top);
                        }
                    }
                }
            }
        }

        // Copy obstacle height field to device for use in GPU kernels
        Gpu::DeviceVector<Real> d_terr(obstacle_h.size());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                         obstacle_h.begin(), obstacle_h.end(), d_terr.begin());
        Real const* d_terr_ptr = d_terr.data();

// Print wall function configuration
        if (enable_wall_functions) {
            amrex::Print() << "wind_solver: wall functions ENABLED\n";
            if (enable_terrain_wall_function) {
                amrex::Print() << "  terrain wall function: ENABLED\n";
            }
            if (enable_flat_surface_wall_function) {
                amrex::Print() << "  flat surface wall function: ENABLED\n";
                amrex::Print() << "    z0_flat = " << wall_function_z0_flat << " m\n";
                if (wall_function_enable_flat_surface) {
                    amrex::Print() << "    flat surface elevation = " 
                                  << wall_function_flat_surface_elevation << " m\n";
                }
            }
            if (enable_building_wall_function) {
                amrex::Print() << "  building wall function: ENABLED\n";
                amrex::Print() << "    z0_building = " << wall_function_z0_building << " m\n";
            }
            amrex::Print() << "  blend height = " << wall_function_blend_height << " cells\n";
            amrex::Print() << "  max distance = " << wall_function_max_distance << " cells\n";
            
            // Print stability correction status
            if (wall_function_enable_stability) {
                amrex::Print() << "  stability correction: ENABLED\n";
                amrex::Print() << "    Obukhov length L = " << wall_function_stability_length << " m\n";
                if (wall_function_stability_length > 0) {
                    amrex::Print() << "    (stable conditions)\n";
                } else if (wall_function_stability_length < 0) {
                    amrex::Print() << "    (unstable conditions)\n";
                } else {
                    amrex::Print() << "    (neutral conditions)\n";
                }
            } else {
                amrex::Print() << "  stability correction: DISABLED (neutral log-law)\n";
            }
            
            // Print adaptive activation status
            if (wall_function_enable_adaptive) {
                amrex::Print() << "  adaptive activation: ENABLED\n";
                amrex::Print() << "    resolution threshold = " << wall_function_adaptive_threshold << " (dz/z0)\n";
                amrex::Print() << "    min cells in log layer = " << wall_function_adaptive_min_cells << "\n";
            } else {
                amrex::Print() << "  adaptive activation: DISABLED (always active when enabled)\n";
            }
        } else {
            amrex::Print() << "wind_solver: wall functions DISABLED (using no-slip boundary conditions)\n";
        }
        // Copy terrain gradients to device (if kinematic BC enabled)
        Gpu::DeviceVector<Real> d_terr_grad_x, d_terr_grad_y;
        Real const* d_terr_grad_x_ptr = nullptr;
        Real const* d_terr_grad_y_ptr = nullptr;
        
        if (enable_terrain_kinematic_bc) {
            d_terr_grad_x.resize(terrain_grad_x.size());
            d_terr_grad_y.resize(terrain_grad_y.size());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                            terrain_grad_x.begin(), terrain_grad_x.end(), d_terr_grad_x.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                            terrain_grad_y.begin(), terrain_grad_y.end(), d_terr_grad_y.begin());
            d_terr_grad_x_ptr = d_terr_grad_x.data();
            d_terr_grad_y_ptr = d_terr_grad_y.data();
        }

        // Summary statistics
        Real zs_min = *std::min_element(terrain_h.begin(), terrain_h.end());
        Real zs_max = *std::max_element(terrain_h.begin(), terrain_h.end());
        Real obs_max = *std::max_element(obstacle_h.begin(), obstacle_h.end());
        amrex::Print() << "wind_solver: terrain elevation [" << zs_min
                       << ", " << zs_max << "] m\n";
        if (!building_xmin.empty()) {
            amrex::Print() << "wind_solver: obstacle height (terrain+buildings) max = "
                           << obs_max << " m\n";
        }

        amrex::Print() << "wind_solver: terrain interpolation time = " 
                       << (amrex::second() - t_phase) << " s\n";

        // ----------------------------------------------------------------
        // 7. Determine vertical domain and build AMReX geometry
        //    Vertical range: [z_lo, z_hi] where
        //        z_lo = minimum terrain elevation (= zs_min)
        //        z_hi = maximum obstacle elevation + domain_height
        //    This ensures the domain covers all terrain and extends at least
        //    domain_height metres above the highest obstacle point.
        // ----------------------------------------------------------------
        t_phase = amrex::second();
        Real z_lo = zs_min;
        Real z_hi = obs_max + domain_height;
        int  nz   = std::max(1, static_cast<int>(std::round((z_hi - z_lo) / dz_req)));
        Real dz   = (z_hi - z_lo) / nz;

        amrex::Print() << "wind_solver: grid " << nx << " x " << ny << " x " << nz
                       << "  (dx=" << dx << " m, dy=" << dy << " m, dz=" << dz << " m)\n";
        amrex::Print() << "wind_solver: vertical domain [" << z_lo
                       << ", " << z_hi << "] m\n";

        IntVect dom_lo(0, 0, 0);
        IntVect dom_hi(nx - 1, ny - 1, nz - 1);
        Box domain(dom_lo, dom_hi);

        RealBox rb({x_lo, y_lo, z_lo}, {x_hi, y_hi, z_hi});
        Array<int, AMREX_SPACEDIM> is_periodic{0, 0, 0};
        Geometry geom(domain, &rb, CoordSys::cartesian, is_periodic.data());

        BoxArray ba(domain);
        ba.maxSize(max_grid_size);
        DistributionMapping dm(ba);

        // ----------------------------------------------------------------
        // 8. Allocate MultiFabs
        //    lam   – Lagrange multiplier λ                   [1 comp,  ng=1]
        //    rhs   – Poisson RHS = -(∇·u0)                  [1 comp,  ng=0]
        // ----------------------------------------------------------------
        MultiFab vel0(ba, dm, 3, 1);
        MultiFab lam (ba, dm, 1, 1);
        MultiFab rhs (ba, dm, 1, 0);

        vel0.setVal(0.0);
        lam .setVal(0.0);
        rhs .setVal(0.0);

        // Temperature MultiFab (if buoyancy stratification enabled)
        MultiFab temp(ba, dm, 1, 0);
        temp.setVal(temperature_reference);  // Initialize to reference temperature
        
        // Copy temperature profile data to device (if buoyancy enabled)
        Gpu::DeviceVector<Real> d_temp_z, d_temp_T;
        Real const* d_temp_z_ptr = nullptr;
        Real const* d_temp_T_ptr = nullptr;
        int n_temp_points = 0;
        
        if (enable_buoyancy_stratification && !z_temp.empty()) {
            n_temp_points = static_cast<int>(z_temp.size());
            d_temp_z.resize(z_temp.size());
            d_temp_T.resize(T_temp.size());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                            z_temp.begin(), z_temp.end(), d_temp_z.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                            T_temp.begin(), T_temp.end(), d_temp_T.begin());
            d_temp_z_ptr = d_temp_z.data();
            d_temp_T_ptr = d_temp_T.data();
            
            amrex::Print() << "wind_solver: temperature profile copied to device (" 
                          << n_temp_points << " points)\n";
        }

        // For RAWS mode: device vectors for wind field interpolation
        Gpu::DeviceVector<Real> d_vel_u(0), d_vel_v(0);
        Real const* d_vel_u_ptr = nullptr;
        Real const* d_vel_v_ptr = nullptr;

        // Common capture variables for wind field initialization and correction
        const Real dz_cap_init    = dz;
        const Real z_lo_cap_init  = z_lo;   // physical z at bottom of domain
        const int  nx_cap_init    = nx;
        const Real z0_cap         = z0;      // surface roughness for diagnostics
        const bool use_pos_z0     = use_z0_file;  // position-dependent z0 flag

        amrex::Print() << "wind_solver: grid setup time = " 
                       << (amrex::second() - t_phase) << " s\n";

        // ----------------------------------------------------------------
        // 9. Fill initial wind field based on initialization mode
        // ----------------------------------------------------------------
        t_phase = amrex::second();
        amrex::Print() << "wind_solver: initializing wind field with mode: " << init_mode << "\n";

        // Handle position-dependent roughness for loglaw mode
        std::vector<Real> z0_h(static_cast<std::size_t>(nx) * ny, z0);  // default to constant z0
        Gpu::DeviceVector<Real> d_z0_pos;
        const Real* d_z0_pos_ptr = nullptr;
        
        if (init_mode == "loglaw" && use_z0_file) {
            amrex::Print() << "wind_solver: reading position-dependent roughness from " << z0_file << "\n";
            std::vector<Real> x_z0, y_z0, z0_data;
            read_roughness_file(z0_file, x_z0, y_z0, z0_data);
            
            // Interpolate z0 to grid columns using IDW
            for (int j = 0; j < ny; ++j) {
                for (int i = 0; i < nx; ++i) {
                    Real xc = x_lo + (i + Real(0.5)) * dx;
                    Real yc = y_lo + (j + Real(0.5)) * dy;
                    
                    // IDW interpolation (same method as terrain)
                    Real z0_interp = z0;  // fallback to constant
                    Real wsum = 0.0;
                    Real z0_sum = 0.0;
                    std::vector<std::pair<Real, int>> d2(x_z0.size());
                    for (std::size_t m = 0; m < x_z0.size(); ++m) {
                        Real dx_pt = xc - x_z0[m];
                        Real dy_pt = yc - y_z0[m];
                        d2[m] = {dx_pt * dx_pt + dy_pt * dy_pt, static_cast<int>(m)};
                    }
                    std::sort(d2.begin(), d2.end());
                    
                    const int n_pts = std::min(6, static_cast<int>(d2.size()));
                    for (int m = 0; m < n_pts; ++m) {
                        Real dist = std::sqrt(d2[m].first);
                        if (dist < Real(1.0e-12)) {
                            z0_interp = z0_data[d2[m].second];
                            wsum = 1.0;
                            break;
                        }
                        Real w = Real(1.0) / (dist * dist);
                        wsum += w;
                        z0_sum += w * z0_data[d2[m].second];
                    }
                    if (wsum > Real(0.0)) {
                        z0_interp = z0_sum / wsum;
                    }
                    
                    z0_h[static_cast<std::size_t>(j) * nx + i] = z0_interp;
                }
            }
            
            // Copy to device
            d_z0_pos.resize(z0_h.size());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice, z0_h.begin(), z0_h.end(), d_z0_pos.begin());
            d_z0_pos_ptr = d_z0_pos.data();
            
            amrex::Print() << "wind_solver: position-dependent roughness interpolated to grid\n";
        }

        if (init_mode == "loglaw") {
            // Log-law profile initialization
            Real speed_ref = std::sqrt(U_ref * U_ref + V_ref * V_ref);
            const Real kappa = 0.41;  // von Karman constant

            // Compute friction velocity from reference speed and height
            // u* = κ * |U_ref| / ln((z_ref + z0) / z0)
            Real ustar = (speed_ref > Real(1.0e-10))
                       ? kappa * speed_ref / std::log((z_ref + z0) / z0)
                       : Real(0.0);

            Real ux_hat = (speed_ref > Real(1.0e-10)) ? U_ref / speed_ref : Real(1.0);
            Real uy_hat = (speed_ref > Real(1.0e-10)) ? V_ref / speed_ref : Real(0.0);

            // Setup canopy parameters
            CanopyParams canopy_params;
            canopy_params.enabled = enable_canopy;
            canopy_params.height = canopy_height;
            canopy_params.frontal_area_index = frontal_area_index;
            canopy_params.plan_area_index = plan_area_index;
            canopy_params.drag_coefficient = canopy_drag_coeff;
            canopy_params.attenuation_coeff = canopy_attenuation;
            canopy_params.use_exponential_profile = use_exponential_profile;

            // Print canopy model status
            if (enable_canopy) {
                amrex::Print() << "wind_solver: canopy model enabled\n";
                amrex::Print() << "  canopy_height = " << canopy_height << " m\n";
                amrex::Print() << "  frontal_area_index = " << frontal_area_index << "\n";
                amrex::Print() << "  plan_area_index = " << plan_area_index << "\n";
                amrex::Print() << "  canopy_drag_coeff = " << canopy_drag_coeff << "\n";
                if (use_exponential_profile) {
                    amrex::Print() << "  using Shaw-Pereira exponential profile\n";
                    amrex::Print() << "  attenuation_coeff = " << canopy_attenuation << "\n";
                } else {
                    amrex::Print() << "  using MacDonald displacement height\n";
                }
            }
            
            // Print Ekman veer status
            if (enable_ekman_veer) {
                amrex::Print() << "wind_solver: Ekman spiral wind veer enabled\n";
                amrex::Print() << "  latitude = " << latitude << " degrees\n";
                amrex::Print() << "  total_veer = " << ekman_veer_total << " degrees\n";
                amrex::Print() << "  veer_height = " << ekman_veer_height << " m\n";
            }

            // Capture parameters for GPU lambda
            const Real ustar_cap = ustar;
            const Real kappa_cap = kappa;
            const Real z0_cap    = z0;
            const Real z_ref_cap = z_ref;
            const Real ux_h      = ux_hat;
            const Real uy_h      = uy_hat;
            const bool use_pos_z0 = use_z0_file;
            
            // Capture stability correction parameters
            const bool use_stability = enable_stability_correction;
            const Real L_obukhov = stability_length;
            
            // Capture elevation scaling parameters
            const bool use_elev_scaling = enable_elevation_scaling;
            const Real elev_scale_factor = elevation_scaling_factor;
            const Real elev_height_scale = elevation_height_scale;
            const Real terrain_min = zs_min;
            
            // Wall function parameters
            const bool use_wall_func = enable_wall_functions;
            const bool use_terrain_wall = enable_terrain_wall_function;
            const Real wf_blend_height = wall_function_blend_height;
            const Real speed_ref_cap = speed_ref;  // For wall function reference
            
            // New wall function enhancements
            const bool wf_enable_stability = wall_function_enable_stability;
            const Real wf_stability_length = wall_function_stability_length;
            const bool wf_enable_adaptive = wall_function_enable_adaptive;
            const Real wf_adaptive_threshold = wall_function_adaptive_threshold;

            // Capture buoyancy parameters
            const bool use_buoyancy = enable_buoyancy_stratification;
            const Real T_ref = temperature_reference;
            const Real buoy_coeff = buoyancy_coefficient;
            const Real buoy_dt = buoyancy_timescale;
            const int n_temp_pts = n_temp_points;
            
            // Capture kinematic BC parameters
            const bool use_kinematic_bc = enable_terrain_kinematic_bc;
            const Real bc_relax = terrain_bc_relaxation;
            
            // Capture Ekman veer parameters
            const bool use_ekman = enable_ekman_veer;
            const Real veer_height = ekman_veer_height;
            const Real veer_total = ekman_veer_total_rad;

            for (MFIter mfi(vel0); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                auto vel = vel0.array(mfi);

                amrex::ParallelFor(bx,
                    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
                {
                    // Height above local terrain for this column
                    Real z_physical = z_lo_cap_init + (k + Real(0.5)) * dz_cap_init;
                    Real terrain_elev = d_terr_ptr[j * nx_cap_init + i];
                    Real z_agl      = z_physical - terrain_elev;

// Handle near-terrain and below-terrain cells
                    if (z_agl <= Real(0.0)) {
                        // Below terrain - always zero velocity
                        vel(i, j, k, 0) = Real(0.0);
                        vel(i, j, k, 1) = Real(0.0);
                        vel(i, j, k, 2) = Real(0.0);
                    } else if (use_wall_func && use_terrain_wall && z_agl <= wf_blend_height * dz_cap_init) {
                        // Near terrain - apply wall function if enabled
                        // Use position-dependent z0 if available
                        Real z0_local = use_pos_z0 ? d_z0_pos_ptr[j * nx_cap_init + i] : z0_cap;
                        
                        // Apply flat surface wall function with blending
                        // (For now, simplified version for flat terrain assumption)
                        Real u_outer = vel(i, j, k, 0);
                        Real v_outer = vel(i, j, k, 1);
                        Real w_outer = vel(i, j, k, 2);
                        
                        // Compute outer flow velocity using standard log-law
                        Real ustar_local = ustar_cap;
                        if (use_pos_z0 && z0_local > Real(1.0e-10)) {
                            Real speed_ref_denom = std::log((z_ref_cap + z0_cap) / z0_cap);
                            Real speed_ref_local = (speed_ref_denom > Real(1.0e-10)) 
                                ? ustar_cap * speed_ref_denom / kappa_cap : Real(0.0);
                            Real log_term = std::log((z_ref_cap + z0_local) / z0_local);
                            ustar_local = (log_term > Real(1.0e-10)) 
                                ? kappa_cap * speed_ref_local / log_term : Real(0.0);
                        }
                        
                        if (use_elev_scaling && elev_height_scale > Real(1.0e-10)) {
                            Real scale = elevation_wind_scaling(Real(1.0), terrain_elev, 
                                                               terrain_min, elev_scale_factor, 
                                                               elev_height_scale);
                            ustar_local *= scale;
                        }
                        
                        Real speed;
                        if (use_stability && std::abs(L_obukhov) > Real(1.0e-10)) {
                            speed = wind_profile_stability(z_agl, z0_local, ustar_local, 
                                                          kappa_cap, L_obukhov);
                        } else {
                            speed = canopy_wind_profile(
                                z_agl, canopy_params, z0_local, ustar_local, kappa_cap);
                        }
                        
                        u_outer = speed * ux_h;
                        v_outer = speed * uy_h;
                        w_outer = Real(0.0);
                        
                        // Apply wall function with blending
                        Real u_wf = vel(i, j, k, 0);
                        Real v_wf = vel(i, j, k, 1);
                        Real w_wf = vel(i, j, k, 2);
                        apply_flat_surface_wall_function_blended(
                            u_wf, v_wf, w_wf,
                            u_outer, v_outer, w_outer,
                            z_agl, z0_local, speed_ref_cap, z_ref_cap,
                            dz_cap_init, wf_blend_height, kappa_cap,
                            wf_enable_stability, wf_stability_length,
                            wf_enable_adaptive, wf_adaptive_threshold);
                        
                        vel(i, j, k, 0) = u_wf;
                        vel(i, j, k, 1) = v_wf;
                        vel(i, j, k, 2) = w_wf;
if (z_agl <= Real(0.0)) {
                        vel(i, j, k, 0) = Real(0.0);
                        vel(i, j, k, 1) = Real(0.0);
                        vel(i, j, k, 2) = Real(0.0);
                    } else {
                        // Use position-dependent z0 if available, otherwise use constant
                        Real z0_local = use_pos_z0 ? d_z0_pos_ptr[j * nx_cap_init + i] : z0_cap;
                        
                        // Recompute ustar with local z0 if using position-dependent roughness
                        Real ustar_local = ustar_cap;
                        if (use_pos_z0 && z0_local > Real(1.0e-10)) {
                            Real speed_ref_denom = std::log((z_ref_cap + z0_cap) / z0_cap);
                            Real speed_ref_local = (speed_ref_denom > Real(1.0e-10)) 
                                ? ustar_cap * speed_ref_denom / kappa_cap : Real(0.0);
                            Real log_term = std::log((z_ref_cap + z0_local) / z0_local);
                            ustar_local = (log_term > Real(1.0e-10)) 
                                ? kappa_cap * speed_ref_local / log_term : Real(0.0);
                        }
                        
                        // Apply elevation scaling to modify ustar
                        if (use_elev_scaling && elev_height_scale > Real(1.0e-10)) {
                            Real scale = elevation_wind_scaling(Real(1.0), terrain_elev, 
                                                               terrain_min, elev_scale_factor, 
                                                               elev_height_scale);
                            ustar_local *= scale;
                        }
                        
                        // Apply stability correction to wind profile
                        Real speed;
                        if (use_stability && std::abs(L_obukhov) > Real(1.0e-10)) {
                            // Use non-neutral log-law with Businger-Dyer corrections
                            speed = wind_profile_stability(z_agl, z0_local, ustar_local, 
                                                          kappa_cap, L_obukhov);
                        } else {
                            // Use standard canopy wind profile (includes neutral log-law)
                            speed = canopy_wind_profile(
                                z_agl, canopy_params, z0_local, ustar_local, kappa_cap);
                        }
                        
                        // Apply Ekman veer rotation
                        Real u_vel, v_vel;
                        if (use_ekman) {
                            // Compute veer angle at this height
                            Real veer_angle = ekman_veer_angle(z_agl, veer_height, veer_total);
                            
                            // Apply rotation to horizontal wind components
                            Real u_base = speed * ux_h;
                            Real v_base = speed * uy_h;
                            apply_ekman_veer(u_base, v_base, veer_angle, u_vel, v_vel);
                        } else {
                            // No veer - use base wind direction
                            u_vel = speed * ux_h;
                            v_vel = speed * uy_h;
                        }
                        Real w_vel = Real(0.0);
                        
                        // Add buoyancy effects to vertical velocity
                        if (use_buoyancy && n_temp_pts > 0) {
                            // Interpolate temperature from profile
                            Real T_local = T_ref;  // Default to reference temperature
                            
                            // Linear interpolation from temperature profile
                            if (n_temp_pts == 1) {
                                T_local = d_temp_T_ptr[0];
                            } else if (z_physical <= d_temp_z_ptr[0]) {
                                // Below first point: use first value
                                T_local = d_temp_T_ptr[0];
                            } else if (z_physical >= d_temp_z_ptr[n_temp_pts - 1]) {
                                // Above last point: use last value
                                T_local = d_temp_T_ptr[n_temp_pts - 1];
                            } else {
                                // Find bracketing points and interpolate
                                for (int m = 0; m < n_temp_pts - 1; ++m) {
                                    if (z_physical >= d_temp_z_ptr[m] && 
                                        z_physical <= d_temp_z_ptr[m + 1]) {
                                        T_local = temperature_linear_interp(
                                            z_physical,
                                            d_temp_z_ptr[m], d_temp_T_ptr[m],
                                            d_temp_z_ptr[m + 1], d_temp_T_ptr[m + 1]);
                                        break;
                                    }
                                }
                            }
                            
                            // Compute buoyancy-induced vertical velocity
                            w_vel += buoyancy_velocity(T_local, T_ref, buoy_dt, buoy_coeff);
                        }
                        
                        // Apply kinematic terrain BC at first cell above terrain
                        // Check if this is the first cell above terrain (k is smallest with z_agl > 0)
                        if (use_kinematic_bc && k > 0) {
                            Real z_physical_below = z_lo_cap_init + (k - Real(0.5)) * dz_cap_init;
                            Real z_agl_below = z_physical_below - terrain_elev;
                            
                            // If cell below is inside terrain, this is the interface cell
                            if (z_agl_below <= Real(0.0)) {
                                // Apply kinematic BC: w = u·∇h
                                std::size_t idx_2d = static_cast<std::size_t>(j) * nx_cap_init + i;
                                Real dh_dx = d_terr_grad_x_ptr[idx_2d];
                                Real dh_dy = d_terr_grad_y_ptr[idx_2d];
                                w_vel = terrain_kinematic_w(u_vel, v_vel, dh_dx, dh_dy, bc_relax);
                            }
                        }
                        
                        vel(i, j, k, 0) = u_vel;
                        vel(i, j, k, 1) = v_vel;
                        vel(i, j, k, 2) = w_vel;
                    }
                });
            }
        } else if (init_mode == "uniform") {
            // Uniform wind field initialization (constant U, V)
            const Real u_uniform = uniform_U;
            const Real v_uniform = uniform_V;
// Wall function parameters
            const bool use_wall_func = enable_wall_functions;
            const bool use_terrain_wall = enable_terrain_wall_function;
            const Real wf_blend_height = wall_function_blend_height;
            const Real z0_local_cap = z0;
            const Real z_ref_cap = z_ref;
            const Real kappa_cap = 0.41;
            const Real speed_ref_cap = std::sqrt(u_uniform * u_uniform + v_uniform * v_uniform);
            
            // New wall function enhancements
            const bool wf_enable_stability = wall_function_enable_stability;
            const Real wf_stability_length = wall_function_stability_length;
            const bool wf_enable_adaptive = wall_function_enable_adaptive;
            const Real wf_adaptive_threshold = wall_function_adaptive_threshold;

            // Capture buoyancy parameters
            const bool use_buoyancy = enable_buoyancy_stratification;
            const Real T_ref = temperature_reference;
            const Real buoy_coeff = buoyancy_coefficient;
            const Real buoy_dt = buoyancy_timescale;
            const int n_temp_pts = n_temp_points;
            
            // Capture kinematic BC parameters
            const bool use_kinematic_bc = enable_terrain_kinematic_bc;
            const Real bc_relax = terrain_bc_relaxation;

            for (MFIter mfi(vel0); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                auto vel = vel0.array(mfi);

                amrex::ParallelFor(bx,
                    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
                {
                    // Height above local terrain for this column
                    Real z_physical = z_lo_cap_init + (k + Real(0.5)) * dz_cap_init;
                    Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_init + i];

                    if (z_agl <= Real(0.0)) {
                        vel(i, j, k, 0) = Real(0.0);
                        vel(i, j, k, 1) = Real(0.0);
                        vel(i, j, k, 2) = Real(0.0);
                    } else if (use_wall_func && use_terrain_wall && z_agl <= wf_blend_height * dz_cap_init) {
                        // Near terrain - apply wall function if enabled
                        Real u_wf = vel(i, j, k, 0);
                        Real v_wf = vel(i, j, k, 1);
                        Real w_wf = vel(i, j, k, 2);
                        apply_flat_surface_wall_function_blended(
                            u_wf, v_wf, w_wf,
                            u_uniform, v_uniform, Real(0.0),
                            z_agl, z0_local_cap, speed_ref_cap, z_ref_cap,
                            dz_cap_init, wf_blend_height, kappa_cap,
                            wf_enable_stability, wf_stability_length,
                            wf_enable_adaptive, wf_adaptive_threshold);
                        
                        vel(i, j, k, 0) = u_wf;
                        vel(i, j, k, 1) = v_wf;
                        vel(i, j, k, 2) = w_wf;
                    } else {
                        Real u_vel = u_uniform;
                        Real v_vel = v_uniform;
                        Real w_vel = Real(0.0);
                        
                        // Add buoyancy effects to vertical velocity
                        if (use_buoyancy && n_temp_pts > 0) {
                            Real T_local = T_ref;
                            if (n_temp_pts == 1) {
                                T_local = d_temp_T_ptr[0];
                            } else if (z_physical <= d_temp_z_ptr[0]) {
                                T_local = d_temp_T_ptr[0];
                            } else if (z_physical >= d_temp_z_ptr[n_temp_pts - 1]) {
                                T_local = d_temp_T_ptr[n_temp_pts - 1];
                            } else {
                                for (int m = 0; m < n_temp_pts - 1; ++m) {
                                    if (z_physical >= d_temp_z_ptr[m] && 
                                        z_physical <= d_temp_z_ptr[m + 1]) {
                                        T_local = temperature_linear_interp(
                                            z_physical,
                                            d_temp_z_ptr[m], d_temp_T_ptr[m],
                                            d_temp_z_ptr[m + 1], d_temp_T_ptr[m + 1]);
                                        break;
                                    }
                                }
                            }
                            w_vel += buoyancy_velocity(T_local, T_ref, buoy_dt, buoy_coeff);
                        }
                        
                        // Apply kinematic terrain BC at interface
                        if (use_kinematic_bc && k > 0) {
                            Real z_physical_below = z_lo_cap_init + (k - Real(0.5)) * dz_cap_init;
                            Real z_agl_below = z_physical_below - d_terr_ptr[j * nx_cap_init + i];
                            if (z_agl_below <= Real(0.0)) {
                                std::size_t idx_2d = static_cast<std::size_t>(j) * nx_cap_init + i;
                                Real dh_dx = d_terr_grad_x_ptr[idx_2d];
                                Real dh_dy = d_terr_grad_y_ptr[idx_2d];
                                w_vel = terrain_kinematic_w(u_vel, v_vel, dh_dx, dh_dy, bc_relax);
                            }
                        }
                        
                        vel(i, j, k, 0) = u_vel;
                        vel(i, j, k, 1) = v_vel;
                        vel(i, j, k, 2) = w_vel;
                    }
                });
            }
        } else if (init_mode == "raws") {
            // RAWS/velocity file initialization via IDW interpolation
            std::vector<Real> x_vel, y_vel, z_vel, ux_vel, uy_vel;
            read_velocity_file(velocity_file, x_vel, y_vel, z_vel, ux_vel, uy_vel);

            // Precompute per-column wind velocity via IDW
            // vel_u_h[j*nx + i] and vel_v_h[j*nx + i] = interpolated velocity at column (i,j)
            std::vector<Real> vel_u_h(static_cast<std::size_t>(nx) * ny);
            std::vector<Real> vel_v_h(static_cast<std::size_t>(nx) * ny);

            for (int j = 0; j < ny; ++j) {
                Real yc = y_lo + (j + 0.5) * dy;
                for (int i = 0; i < nx; ++i) {
                    Real xc = x_lo + (i + 0.5) * dx;
                    auto [ux_interp, uy_interp] = idw_velocity(xc, yc, x_vel, y_vel, ux_vel, uy_vel);
                    vel_u_h[static_cast<std::size_t>(j) * nx + i] = ux_interp;
                    vel_v_h[static_cast<std::size_t>(j) * nx + i] = uy_interp;
                }
            }

            // Copy to device for use in GPU kernels
            d_vel_u.resize(vel_u_h.size());
            d_vel_v.resize(vel_v_h.size());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             vel_u_h.begin(), vel_u_h.end(), d_vel_u.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             vel_v_h.begin(), vel_v_h.end(), d_vel_v.begin());
            d_vel_u_ptr = d_vel_u.data();
            d_vel_v_ptr = d_vel_v.data();

            for (MFIter mfi(vel0); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                auto vel = vel0.array(mfi);

                amrex::ParallelFor(bx,
                    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
                {
                    // Height above local terrain for this column
                    Real z_physical = z_lo_cap_init + (k + Real(0.5)) * dz_cap_init;
                    Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_init + i];

                    if (z_agl <= Real(0.0)) {
                        vel(i, j, k, 0) = Real(0.0);
                        vel(i, j, k, 1) = Real(0.0);
                        vel(i, j, k, 2) = Real(0.0);
                    } else {
                        vel(i, j, k, 0) = d_vel_u_ptr[j * nx_cap_init + i];
                        vel(i, j, k, 1) = d_vel_v_ptr[j * nx_cap_init + i];
                        vel(i, j, k, 2) = Real(0.0);
                    }
                });
            }
        } else if (init_mode == "surface_data") {
            // Surface data initialization via IDW interpolation of USTAR, Z0, U10, V10
            // Constructs per-column vertical profiles using local friction velocity and roughness
            std::vector<Real> x_surf, y_surf, z_surf, ustar_surf, z0_surf, u10_surf, v10_surf;
            read_surface_data_file(surface_data_file, x_surf, y_surf, z_surf, 
                                  ustar_surf, z0_surf, u10_surf, v10_surf);

            // Precompute per-column surface parameters via IDW
            std::vector<Real> ustar_h(static_cast<std::size_t>(nx) * ny);
            std::vector<Real> z0_h(static_cast<std::size_t>(nx) * ny);
            std::vector<Real> u10_h(static_cast<std::size_t>(nx) * ny);
            std::vector<Real> v10_h(static_cast<std::size_t>(nx) * ny);

            for (int j = 0; j < ny; ++j) {
                Real yc = y_lo + (j + 0.5) * dy;
                for (int i = 0; i < nx; ++i) {
                    Real xc = x_lo + (i + 0.5) * dx;
                    auto [ustar_interp, z0_interp, u10_interp, v10_interp] = 
                        idw_surface_data(xc, yc, x_surf, y_surf, ustar_surf, z0_surf, u10_surf, v10_surf);
                    ustar_h[static_cast<std::size_t>(j) * nx + i] = ustar_interp;
                    z0_h[static_cast<std::size_t>(j) * nx + i] = z0_interp;
                    u10_h[static_cast<std::size_t>(j) * nx + i] = u10_interp;
                    v10_h[static_cast<std::size_t>(j) * nx + i] = v10_interp;
                }
            }

            // Copy to device for GPU kernels
            Gpu::DeviceVector<Real> d_ustar(ustar_h.size());
            Gpu::DeviceVector<Real> d_z0(z0_h.size());
            Gpu::DeviceVector<Real> d_u10(u10_h.size());
            Gpu::DeviceVector<Real> d_v10(v10_h.size());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice, ustar_h.begin(), ustar_h.end(), d_ustar.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice, z0_h.begin(), z0_h.end(), d_z0.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice, u10_h.begin(), u10_h.end(), d_u10.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice, v10_h.begin(), v10_h.end(), d_v10.begin());

            const Real* d_ustar_ptr = d_ustar.data();
            const Real* d_z0_ptr = d_z0.data();
            const Real* d_u10_ptr = d_u10.data();
            const Real* d_v10_ptr = d_v10.data();
            const Real kappa_cap = Real(0.41);  // von Karman constant

            // Setup canopy parameters (shared with loglaw)
            CanopyParams canopy_params;
            canopy_params.enabled = enable_canopy;
            canopy_params.height = canopy_height;
            canopy_params.frontal_area_index = frontal_area_index;
            canopy_params.plan_area_index = plan_area_index;
            canopy_params.drag_coefficient = canopy_drag_coeff;
            canopy_params.attenuation_coeff = canopy_attenuation;
            canopy_params.use_exponential_profile = use_exponential_profile;
            
            // Capture Ekman veer parameters
            const bool use_ekman = enable_ekman_veer;
            const Real veer_height = ekman_veer_height;
            const Real veer_total = ekman_veer_total_rad;

            for (MFIter mfi(vel0); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                auto vel = vel0.array(mfi);

                amrex::ParallelFor(bx,
                    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
                {
                    // Height above local terrain for this column
                    Real z_physical = z_lo_cap_init + (k + Real(0.5)) * dz_cap_init;
                    Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_init + i];

                    if (z_agl <= Real(0.0)) {
                        vel(i, j, k, 0) = Real(0.0);
                        vel(i, j, k, 1) = Real(0.0);
                        vel(i, j, k, 2) = Real(0.0);
                    } else {
                        // Get column-specific surface parameters
                        std::size_t idx = static_cast<std::size_t>(j) * nx_cap_init + i;
                        Real ustar_col = d_ustar_ptr[idx];
                        Real z0_col = d_z0_ptr[idx];
                        Real u10_col = d_u10_ptr[idx];
                        Real v10_col = d_v10_ptr[idx];

                        // Compute wind direction from 10m winds
                        Real speed_10m = std::sqrt(u10_col * u10_col + v10_col * v10_col);
                        Real ux_hat = (speed_10m > Real(1.0e-10)) ? u10_col / speed_10m : Real(1.0);
                        Real uy_hat = (speed_10m > Real(1.0e-10)) ? v10_col / speed_10m : Real(0.0);

                        // Construct vertical profile using log-law with column-specific ustar and z0
                        Real speed = canopy_wind_profile(
                            z_agl, canopy_params, z0_col, ustar_col, kappa_cap);
                        // Apply Ekman veer rotation
                        Real u_vel, v_vel;
                        if (use_ekman) {
                            // Compute veer angle at this height
                            Real veer_angle = ekman_veer_angle(z_agl, veer_height, veer_total);
                            
                            // Apply rotation to horizontal wind components
                            Real u_base = speed * ux_hat;
                            Real v_base = speed * uy_hat;
                            apply_ekman_veer(u_base, v_base, veer_angle, u_vel, v_vel);
                        } else {
                            // No veer - use base wind direction
                            u_vel = speed * ux_hat;
                            v_vel = speed * uy_hat;
                        }
                        
                        vel(i, j, k, 0) = u_vel;
                        vel(i, j, k, 1) = v_vel;
                        vel(i, j, k, 2) = Real(0.0);
                    }
                });
            }
        } else if (init_mode == "powerlaw") {
            // Power-law profile initialization
            // u(z) = U_ref * (z/z_ref)^alpha
            // Typical exponent: alpha ≈ 1/7 (0.143) for neutral atmospheric conditions
            Real speed_ref = std::sqrt(U_ref * U_ref + V_ref * V_ref);
            Real ux_hat = (speed_ref > Real(1.0e-10)) ? U_ref / speed_ref : Real(1.0);
            Real uy_hat = (speed_ref > Real(1.0e-10)) ? V_ref / speed_ref : Real(0.0);
            
            const Real exponent = powerlaw_exponent;
            const Real z_ref_cap = z_ref;
            const Real speed_ref_cap = speed_ref;
            const Real ux_h = ux_hat;
            const Real uy_h = uy_hat;

            amrex::Print() << "wind_solver: power-law profile initialization\n";
            amrex::Print() << "  U_ref = " << U_ref << " m/s, V_ref = " << V_ref << " m/s\n";
            amrex::Print() << "  z_ref = " << z_ref << " m\n";
            amrex::Print() << "  powerlaw_exponent = " << powerlaw_exponent << "\n";

            // Capture buoyancy parameters
            const bool use_buoyancy = enable_buoyancy_stratification;
            const Real T_ref = temperature_reference;
            const Real buoy_coeff = buoyancy_coefficient;
            const Real buoy_dt = buoyancy_timescale;
            const int n_temp_pts = n_temp_points;
            
            // Capture kinematic BC parameters
            const bool use_kinematic_bc = enable_terrain_kinematic_bc;
            const Real bc_relax = terrain_bc_relaxation;
            
            // Capture Ekman veer parameters
            const bool use_ekman = enable_ekman_veer;
            const Real veer_height = ekman_veer_height;
            const Real veer_total = ekman_veer_total_rad;

            for (MFIter mfi(vel0); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                auto vel = vel0.array(mfi);

                amrex::ParallelFor(bx,
                    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
                {
                    // Height above local terrain for this column
                    Real z_physical = z_lo_cap_init + (k + Real(0.5)) * dz_cap_init;
                    Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_init + i];

                    if (z_agl <= Real(0.0)) {
                        vel(i, j, k, 0) = Real(0.0);
                        vel(i, j, k, 1) = Real(0.0);
                        vel(i, j, k, 2) = Real(0.0);
                    } else {
                        // Power-law profile: u(z) = U_ref * (z/z_ref)^alpha
                        Real z_ratio = z_agl / z_ref_cap;
                        // Clamp z_ratio to avoid extrapolation below reference height
                        z_ratio = (z_ratio < Real(0.01)) ? Real(0.01) : z_ratio;
                        Real speed = speed_ref_cap * std::pow(z_ratio, exponent);
                        
                        // Apply Ekman veer rotation
                        Real u_vel, v_vel;
                        if (use_ekman) {
                            // Compute veer angle at this height
                            Real veer_angle = ekman_veer_angle(z_agl, veer_height, veer_total);
                            
                            // Apply rotation to horizontal wind components
                            Real u_base = speed * ux_h;
                            Real v_base = speed * uy_h;
                            apply_ekman_veer(u_base, v_base, veer_angle, u_vel, v_vel);
                        } else {
                            // No veer - use base wind direction
                            u_vel = speed * ux_h;
                            v_vel = speed * uy_h;
                        }
                        Real w_vel = Real(0.0);
                        
                        // Add buoyancy effects to vertical velocity
                        if (use_buoyancy && n_temp_pts > 0) {
                            Real T_local = T_ref;
                            if (n_temp_pts == 1) {
                                T_local = d_temp_T_ptr[0];
                            } else if (z_physical <= d_temp_z_ptr[0]) {
                                T_local = d_temp_T_ptr[0];
                            } else if (z_physical >= d_temp_z_ptr[n_temp_pts - 1]) {
                                T_local = d_temp_T_ptr[n_temp_pts - 1];
                            } else {
                                for (int m = 0; m < n_temp_pts - 1; ++m) {
                                    if (z_physical >= d_temp_z_ptr[m] && 
                                        z_physical <= d_temp_z_ptr[m + 1]) {
                                        T_local = temperature_linear_interp(
                                            z_physical,
                                            d_temp_z_ptr[m], d_temp_T_ptr[m],
                                            d_temp_z_ptr[m + 1], d_temp_T_ptr[m + 1]);
                                        break;
                                    }
                                }
                            }
                            w_vel += buoyancy_velocity(T_local, T_ref, buoy_dt, buoy_coeff);
                        }
                        
                        // Apply kinematic terrain BC at interface
                        if (use_kinematic_bc && k > 0) {
                            Real z_physical_below = z_lo_cap_init + (k - Real(0.5)) * dz_cap_init;
                            Real z_agl_below = z_physical_below - d_terr_ptr[j * nx_cap_init + i];
                            if (z_agl_below <= Real(0.0)) {
                                std::size_t idx_2d = static_cast<std::size_t>(j) * nx_cap_init + i;
                                Real dh_dx = d_terr_grad_x_ptr[idx_2d];
                                Real dh_dy = d_terr_grad_y_ptr[idx_2d];
                                w_vel = terrain_kinematic_w(u_vel, v_vel, dh_dx, dh_dy, bc_relax);
                            }
                        }
                        
                        vel(i, j, k, 0) = u_vel;
                        vel(i, j, k, 1) = v_vel;
                        vel(i, j, k, 2) = w_vel;
                    }
                });
            }
        }

        // Fill interior (inter-box) ghost cells via MPI exchange
        vel0.FillBoundary(geom.periodicity());

        // ----------------------------------------------------------------
        // 9a. Apply wake model (if enabled)
        //     Modifies the initial velocity field to account for building wakes
        //     using the Röckle (1990) parameterization
        //     Phase 2: Supports wake superposition and street canyon effects
        // ----------------------------------------------------------------
        if (enable_wake && !building_xmin.empty()) {
            amrex::Print() << "wind_solver: applying wake model (Röckle formulation)\n";
            amrex::Print() << "  cavity length coeff c1 = " << wake_c1 << "\n";
            amrex::Print() << "  wake deficit coeff c2 = " << wake_c2 << "\n";
            amrex::Print() << "  separation length factor = " << wake_separation_length << "\n";
            amrex::Print() << "  wake superposition = " << (wake_superposition ? "enabled" : "disabled") << "\n";
            if (enable_street_canyon) {
                amrex::Print() << "  street canyon effects enabled (reduction factor = " << street_canyon_reduction << ")\n";
            }
            
            // Set up wake parameters
            WakeParams wake_params;
            wake_params.enabled = true;
            wake_params.c1 = wake_c1;
            wake_params.c2 = wake_c2;
            wake_params.separation_length = wake_separation_length;
            
            // Copy building data to device
            int n_buildings = static_cast<int>(building_xmin.size());
            Gpu::DeviceVector<Real> d_bldg_xmin(n_buildings);
            Gpu::DeviceVector<Real> d_bldg_xmax(n_buildings);
            Gpu::DeviceVector<Real> d_bldg_ymin(n_buildings);
            Gpu::DeviceVector<Real> d_bldg_ymax(n_buildings);
            Gpu::DeviceVector<Real> d_bldg_zmin(n_buildings);
            Gpu::DeviceVector<Real> d_bldg_zmax(n_buildings);
            Gpu::DeviceVector<Real> d_bldg_rotation(n_buildings);
            
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             building_xmin.begin(), building_xmin.end(), d_bldg_xmin.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             building_xmax.begin(), building_xmax.end(), d_bldg_xmax.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             building_ymin.begin(), building_ymin.end(), d_bldg_ymin.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             building_ymax.begin(), building_ymax.end(), d_bldg_ymax.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             building_zmin.begin(), building_zmin.end(), d_bldg_zmin.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             building_zmax.begin(), building_zmax.end(), d_bldg_zmax.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             building_rotation.begin(), building_rotation.end(), d_bldg_rotation.begin());
            
            Real const* d_bldg_xmin_ptr = d_bldg_xmin.data();
            Real const* d_bldg_xmax_ptr = d_bldg_xmax.data();
            Real const* d_bldg_ymin_ptr = d_bldg_ymin.data();
            Real const* d_bldg_ymax_ptr = d_bldg_ymax.data();
            Real const* d_bldg_zmin_ptr = d_bldg_zmin.data();
            Real const* d_bldg_zmax_ptr = d_bldg_zmax.data();
            Real const* d_bldg_rotation_ptr = d_bldg_rotation.data();
            
            const int n_bldg_cap = n_buildings;
            const Real dx_wake = dx;
            const Real dy_wake = dy;
            const Real dz_wake = dz;
            const Real x_lo_wake = x_lo;
            const Real y_lo_wake = y_lo;
            const Real z_lo_wake = z_lo;
            const bool use_superposition = wake_superposition;
            const bool use_street_canyon = enable_street_canyon;
            const Real canyon_reduction = street_canyon_reduction;
            
            for (MFIter mfi(vel0); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                auto vel = vel0.array(mfi);
                
                amrex::ParallelFor(bx,
                    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
                {
                    // Physical coordinates
                    Real x = x_lo_wake + (i + Real(0.5)) * dx_wake;
                    Real y = y_lo_wake + (j + Real(0.5)) * dy_wake;
                    Real z = z_lo_wake + (k + Real(0.5)) * dz_wake;
                    
                    // Get current velocity
                    Real u = vel(i, j, k, 0);
                    Real v = vel(i, j, k, 1);
                    Real w = vel(i, j, k, 2);
                    
                    // Apply wake model (with or without superposition)
                    if (use_superposition && n_bldg_cap > 1) {
                        // Phase 2: Use wake superposition for multiple buildings
                        // Phase 3 Enhancement: Now supports building orientations
                        apply_wake_superposition(
                            x, y, z, u, v, w,
                            d_bldg_xmin_ptr, d_bldg_xmax_ptr,
                            d_bldg_ymin_ptr, d_bldg_ymax_ptr,
                            d_bldg_zmin_ptr, d_bldg_zmax_ptr,
                            d_bldg_rotation_ptr,
                            n_bldg_cap, wake_params);
                    } else {
                        // Original method: Apply wake from each building independently
                        for (int b = 0; b < n_bldg_cap; ++b) {
                            Building bldg = compute_building_dimensions(
                                d_bldg_xmin_ptr[b], d_bldg_xmax_ptr[b],
                                d_bldg_ymin_ptr[b], d_bldg_ymax_ptr[b],
                                d_bldg_zmin_ptr[b], d_bldg_zmax_ptr[b]);
                            bldg.rotation = d_bldg_rotation_ptr[b];
                            
                            apply_single_building_wake(x, y, z, u, v, w, bldg, wake_params);
                        }
                    }
                    
                    // Apply street canyon effects (if enabled)
                    if (use_street_canyon && n_bldg_cap > 1) {
                        // Compute average building height for street canyon model
                        Real avg_height = Real(0.0);
                        for (int b = 0; b < n_bldg_cap; ++b) {
                            avg_height += (d_bldg_zmax_ptr[b] - d_bldg_zmin_ptr[b]);
                        }
                        avg_height /= Real(n_bldg_cap);
                        
                        // Estimate street width as average spacing between buildings
                        // Simplified approach: uses 2*dx as proxy for street width
                        // For more accurate results, compute actual minimum distance
                        // between adjacent building faces in the building array
                        Real street_width = Real(2.0) * dx_wake;
                        
                        apply_street_canyon_effect(
                            z, u, v, w,
                            avg_height, street_width, canyon_reduction);
                    }
                    
                    // Update velocity field
                    vel(i, j, k, 0) = u;
                    vel(i, j, k, 1) = v;
                    vel(i, j, k, 2) = w;
                });
            }
            
            // Fill boundary after wake modification
            vel0.FillBoundary(geom.periodicity());
        }

        // ----------------------------------------------------------------
        // 9b. Apply building porosity model (if enabled) - Feature 8
        //     Modifies velocity in porous buildings based on porosity parameter
        // ----------------------------------------------------------------
        if (enable_building_porosity && !porous_building_xmin.empty()) {
            amrex::Print() << "wind_solver: applying building porosity model\n";
            amrex::Print() << "  porosity drag coefficient = " << porosity_drag_coefficient << "\n";
            
            // Setup porosity parameters
            PorosityParams porosity_params;
            porosity_params.enabled = true;
            porosity_params.default_porosity = default_building_porosity;
            porosity_params.drag_coefficient = porosity_drag_coefficient;
            
            // Copy porous building data to device
            int n_porous = static_cast<int>(porous_building_xmin.size());
            Gpu::DeviceVector<Real> d_porous_xmin(n_porous);
            Gpu::DeviceVector<Real> d_porous_xmax(n_porous);
            Gpu::DeviceVector<Real> d_porous_ymin(n_porous);
            Gpu::DeviceVector<Real> d_porous_ymax(n_porous);
            Gpu::DeviceVector<Real> d_porous_zmin(n_porous);
            Gpu::DeviceVector<Real> d_porous_zmax(n_porous);
            Gpu::DeviceVector<Real> d_porous_porosity(n_porous);
            Gpu::DeviceVector<Real> d_porous_rotation(n_porous);
            
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             porous_building_xmin.begin(), porous_building_xmin.end(), d_porous_xmin.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             porous_building_xmax.begin(), porous_building_xmax.end(), d_porous_xmax.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             porous_building_ymin.begin(), porous_building_ymin.end(), d_porous_ymin.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             porous_building_ymax.begin(), porous_building_ymax.end(), d_porous_ymax.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             porous_building_zmin.begin(), porous_building_zmin.end(), d_porous_zmin.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             porous_building_zmax.begin(), porous_building_zmax.end(), d_porous_zmax.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             porous_building_porosity.begin(), porous_building_porosity.end(), d_porous_porosity.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                             porous_building_rotation.begin(), porous_building_rotation.end(), d_porous_rotation.begin());
            
            Real const* d_porous_xmin_ptr = d_porous_xmin.data();
            Real const* d_porous_xmax_ptr = d_porous_xmax.data();
            Real const* d_porous_ymin_ptr = d_porous_ymin.data();
            Real const* d_porous_ymax_ptr = d_porous_ymax.data();
            Real const* d_porous_zmin_ptr = d_porous_zmin.data();
            Real const* d_porous_zmax_ptr = d_porous_zmax.data();
            Real const* d_porous_porosity_ptr = d_porous_porosity.data();
            Real const* d_porous_rotation_ptr = d_porous_rotation.data();
            
            const int n_porous_cap = n_porous;
            const Real dx_porous = dx;
            const Real dy_porous = dy;
            const Real dz_porous = dz;
            const Real x_lo_porous = x_lo;
            const Real y_lo_porous = y_lo;
            const Real z_lo_porous = z_lo;
            const Real drag_coeff = porosity_drag_coefficient;
            
            for (MFIter mfi(vel0); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                auto vel = vel0.array(mfi);
                
                amrex::ParallelFor(bx,
                    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
                {
                    // Physical coordinates
                    Real x = x_lo_porous + (i + Real(0.5)) * dx_porous;
                    Real y = y_lo_porous + (j + Real(0.5)) * dy_porous;
                    Real z = z_lo_porous + (k + Real(0.5)) * dz_porous;
                    
                    // Check all porous buildings to find porosity at this location
                    Real porosity = Real(1.0);  // default: fully open (no porous building)
                    
                    for (int b = 0; b < n_porous_cap; ++b) {
                        PorousBuilding pb;
                        pb.xmin = d_porous_xmin_ptr[b];
                        pb.xmax = d_porous_xmax_ptr[b];
                        pb.ymin = d_porous_ymin_ptr[b];
                        pb.ymax = d_porous_ymax_ptr[b];
                        pb.zmin = d_porous_zmin_ptr[b];
                        pb.zmax = d_porous_zmax_ptr[b];
                        pb.porosity = d_porous_porosity_ptr[b];
                        pb.rotation = d_porous_rotation_ptr[b];
                        
                        Real p = point_in_porous_building(x, y, z, pb);
                        if (p < porosity) {
                            porosity = p;  // use minimum porosity (maximum blockage)
                        }
                    }
                    
                    // Apply porosity drag if inside a porous building
                    if (porosity < Real(0.999)) {
                        Real u = vel(i, j, k, 0);
                        Real v = vel(i, j, k, 1);
                        Real w = vel(i, j, k, 2);
                        
                        apply_porosity_drag(u, v, w, porosity, drag_coeff, 
                                          dx_porous, dy_porous, dz_porous);
                        
                        vel(i, j, k, 0) = u;
                        vel(i, j, k, 1) = v;
                        vel(i, j, k, 2) = w;
                    }
                });
            }
            
            // Fill boundary after porosity modification
            vel0.FillBoundary(geom.periodicity());
        }

        amrex::Print() << "wind_solver: wind initialization time = " 
                       << (amrex::second() - t_phase) << " s\n";

        // ----------------------------------------------------------------
        // 10. Compute divergence of initial wind  →  RHS = -(∇·u0)
        //    One-sided differences at physical domain boundaries;
        //    centred differences (or WENO) in the interior.
        //    Terrain (sub-surface) cells: rhs = 0 (not enforced).
        // ----------------------------------------------------------------
        t_phase = amrex::second();
        const IntVect glo = domain.smallEnd();
        const IntVect ghi = domain.bigEnd();
        const int ilo = glo[0], ihi = ghi[0];
        const int jlo = glo[1], jhi = ghi[1];
        const int klo = glo[2], khi = ghi[2];
        const Real inv2dx = Real(0.5) / dx;
        const Real inv2dy = Real(0.5) / dy;
        const Real inv2dz = Real(0.5) / dz;
        const Real inv1dx = Real(1.0) / dx;
        const Real inv1dy = Real(1.0) / dy;
        const Real inv1dz = Real(1.0) / dz;
        
        // Capture deriv_method_int for GPU lambda
        const int deriv_method_cap = deriv_method_int;
        const Real dx_cap = dx;
        const Real dy_cap = dy;
        const Real dz_cap_div = dz;  // rename to avoid conflict
        const Real z_lo_cap_div = z_lo;   // capture z_lo for divergence computation
        const int  nx_cap_div   = nx;     // capture nx for divergence computation

        for (MFIter mfi(rhs); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            const auto vel = vel0.const_array(mfi);
            auto rh = rhs.array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                // Height above terrain for this cell
                Real z_physical = z_lo_cap_div + (k + Real(0.5)) * dz_cap_div;
                Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_div + i];
                if (z_agl <= Real(0.0)) { rh(i, j, k) = Real(0.0); return; }

                Real du, dv, dw;
                
                // du/dx - choose method based on deriv_method_cap
                if (deriv_method_cap == 0) {  // central
                    if (i == ilo)
                        du = (vel(i+1,j,k,0) - vel(i,j,k,0)) * inv1dx;
                    else if (i == ihi)
                        du = (vel(i,j,k,0) - vel(i-1,j,k,0)) * inv1dx;
                    else
                        du = (vel(i+1,j,k,0) - vel(i-1,j,k,0)) * inv2dx;
                } else if (deriv_method_cap == 1) {  // weno3
                    if (i == ilo)
                        du = (vel(i+1,j,k,0) - vel(i,j,k,0)) * inv1dx;
                    else if (i == ihi)
                        du = (vel(i,j,k,0) - vel(i-1,j,k,0)) * inv1dx;
                    else
                        du = weno3_deriv(vel(i-1,j,k,0), vel(i,j,k,0), vel(i+1,j,k,0), dx_cap);
                } else {  // weno5 (deriv_method_cap == 2)
                    if (i <= ilo+1)
                        du = (vel(i+1,j,k,0) - vel(i,j,k,0)) * inv1dx;
                    else if (i >= ihi-1)
                        du = (vel(i,j,k,0) - vel(i-1,j,k,0)) * inv1dx;
                    else
                        du = weno5_deriv(vel(i-2,j,k,0), vel(i-1,j,k,0), vel(i,j,k,0), 
                                        vel(i+1,j,k,0), vel(i+2,j,k,0), dx_cap);
                }

                // dv/dy
                if (deriv_method_cap == 0) {  // central
                    if (j == jlo)
                        dv = (vel(i,j+1,k,1) - vel(i,j,k,1)) * inv1dy;
                    else if (j == jhi)
                        dv = (vel(i,j,k,1) - vel(i,j-1,k,1)) * inv1dy;
                    else
                        dv = (vel(i,j+1,k,1) - vel(i,j-1,k,1)) * inv2dy;
                } else if (deriv_method_cap == 1) {  // weno3
                    if (j == jlo)
                        dv = (vel(i,j+1,k,1) - vel(i,j,k,1)) * inv1dy;
                    else if (j == jhi)
                        dv = (vel(i,j,k,1) - vel(i,j-1,k,1)) * inv1dy;
                    else
                        dv = weno3_deriv(vel(i,j-1,k,1), vel(i,j,k,1), vel(i,j+1,k,1), dy_cap);
                } else {  // weno5
                    if (j <= jlo+1)
                        dv = (vel(i,j+1,k,1) - vel(i,j,k,1)) * inv1dy;
                    else if (j >= jhi-1)
                        dv = (vel(i,j,k,1) - vel(i,j-1,k,1)) * inv1dy;
                    else
                        dv = weno5_deriv(vel(i,j-2,k,1), vel(i,j-1,k,1), vel(i,j,k,1), 
                                        vel(i,j+1,k,1), vel(i,j+2,k,1), dy_cap);
                }

                // dw/dz
                if (deriv_method_cap == 0) {  // central
                    if (k == klo)
                        dw = (vel(i,j,k+1,2) - vel(i,j,k,2)) * inv1dz;
                    else if (k == khi)
                        dw = (vel(i,j,k,2) - vel(i,j,k-1,2)) * inv1dz;
                    else
                        dw = (vel(i,j,k+1,2) - vel(i,j,k-1,2)) * inv2dz;
                } else if (deriv_method_cap == 1) {  // weno3
                    if (k == klo)
                        dw = (vel(i,j,k+1,2) - vel(i,j,k,2)) * inv1dz;
                    else if (k == khi)
                        dw = (vel(i,j,k,2) - vel(i,j,k-1,2)) * inv1dz;
                    else
                        dw = weno3_deriv(vel(i,j,k-1,2), vel(i,j,k,2), vel(i,j,k+1,2), dz_cap_div);
                } else {  // weno5
                    if (k <= klo+1)
                        dw = (vel(i,j,k+1,2) - vel(i,j,k,2)) * inv1dz;
                    else if (k >= khi-1)
                        dw = (vel(i,j,k,2) - vel(i,j,k-1,2)) * inv1dz;
                    else
                        dw = weno5_deriv(vel(i,j,k-2,2), vel(i,j,k-1,2), vel(i,j,k,2), 
                                        vel(i,j,k+1,2), vel(i,j,k+2,2), dz_cap_div);
                }

                rh(i, j, k) = -(du + dv + dw);   // rhs = -div(u0)
            });
        }

        amrex::Print() << "wind_solver: RHS computation time = " 
                       << (amrex::second() - t_phase) << " s\n";

        // ----------------------------------------------------------------
        // 11. Set up MLABecLaplacian and MLMG for the Poisson solve
        //
        //   Operator:  -(α_h² ∂²λ/∂x² + α_h² ∂²λ/∂y² + α_v² ∂²λ/∂z²) = rhs
        //
        //   Domain BCs:
        //     x-faces (lo, hi): Dirichlet λ = 0  (inflow / outflow)
        //     y-faces (lo, hi): Neumann ∂λ/∂y = 0 (lateral symmetry)
        //     z-faces (lo, hi): Neumann ∂λ/∂z = 0 (ground, top)
        // ----------------------------------------------------------------
        t_phase = amrex::second();
        LPInfo info;
        info.setAgglomeration(true);
        info.setConsolidation(true);

        MLABecLaplacian mlabec({geom}, {ba}, {dm}, info);
        mlabec.setMaxOrder(2);

        // Boundary condition types
        Array<LinOpBCType, AMREX_SPACEDIM> lo_bc, hi_bc;
        lo_bc[0] = LinOpBCType::Dirichlet;
        hi_bc[0] = LinOpBCType::Dirichlet;
        lo_bc[1] = LinOpBCType::Neumann;
        hi_bc[1] = LinOpBCType::Neumann;
        lo_bc[2] = LinOpBCType::Neumann;
        hi_bc[2] = LinOpBCType::Neumann;
        mlabec.setDomainBC(lo_bc, hi_bc);

        // Scalars: α_a = 0 (no identity term), β_b = 1 (full diffusion)
        mlabec.setScalars(0.0, 1.0);

        // A coefficients (not used since α_a = 0, but must be set)
        MultiFab acoef(ba, dm, 1, 0);
        acoef.setVal(0.0);
        mlabec.setACoeffs(0, acoef);

        // B coefficients (face-centred, anisotropic)
        //   b_x = b_y = alpha_h², b_z = alpha_v²
        //   b_z can vary with height if use_height_dependent_alpha_v is true
        const Real bh = alpha_h * alpha_h;
        const Real bv = alpha_v * alpha_v;
        Array<MultiFab, AMREX_SPACEDIM> bcoef;
        bcoef[0].define(convert(ba, IntVect(1, 0, 0)), dm, 1, 0);
        bcoef[1].define(convert(ba, IntVect(0, 1, 0)), dm, 1, 0);
        bcoef[2].define(convert(ba, IntVect(0, 0, 1)), dm, 1, 0);
        bcoef[0].setVal(bh);
        bcoef[1].setVal(bh);
        
        if (use_height_dependent_alpha_v) {
            // Set height-dependent alpha_v for z-direction
            amrex::Print() << "wind_solver: using height-dependent alpha_v\n";
            amrex::Print() << "  alpha_v_surface = " << alpha_v_surface << "\n";
            amrex::Print() << "  alpha_v_top = " << alpha_v_top << "\n";
            
            const Real alpha_v_surf_sq = alpha_v_surface * alpha_v_surface;
            const Real alpha_v_top_sq = alpha_v_top * alpha_v_top;
            const Real z_lo_alphav = z_lo;
            const Real z_hi_alphav = z_hi;
            
            for (MFIter mfi(bcoef[2]); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                auto bz = bcoef[2].array(mfi);
                
                amrex::ParallelFor(bx,
                    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
                {
                    // z-face is located at k (not k+0.5 for cell center)
                    Real z_face = z_lo_alphav + k * dz;
                    Real z_frac = (z_face - z_lo_alphav) / (z_hi_alphav - z_lo_alphav);
                    z_frac = std::max(Real(0.0), std::min(Real(1.0), z_frac));
                    
                    // Linear interpolation: alpha_v^2(z) = alpha_v_surf^2 + (alpha_v_top^2 - alpha_v_surf^2) * z_frac
                    Real alpha_v_sq = alpha_v_surf_sq + (alpha_v_top_sq - alpha_v_surf_sq) * z_frac;
                    bz(i, j, k) = alpha_v_sq;
                });
            }
        } else {
            bcoef[2].setVal(bv);
        }
        mlabec.setBCoeffs(0, GetArrOfConstPtrs(bcoef));

        // Level BC: homogeneous (λ = 0 on Dirichlet faces)
        mlabec.setLevelBC(0, nullptr);

        amrex::Print() << "wind_solver: Poisson operator setup time = " 
                       << (amrex::second() - t_phase) << " s\n";

        // ----------------------------------------------------------------
        // 12. Solve with MLMG
        // ----------------------------------------------------------------
        t_phase = amrex::second();
        MLMG mlmg(mlabec);
        mlmg.setMaxIter(mlmg_max_iter);
        mlmg.setMaxFmgIter(mlmg_max_fmg_iter);
        mlmg.setVerbose(mlmg_verbose);
        mlmg.setBottomVerbose(0);
        mlmg.setPreSmooth(mlmg_pre_smooth);
        mlmg.setPostSmooth(mlmg_post_smooth);
        
        // Set bottom solver based on user input
        if (mlmg_bottom_solver == "bicgstab") {
            mlmg.setBottomSolver(MLMG::BottomSolver::bicgstab);
            amrex::Print() << "wind_solver: using BiCGStab bottom solver\n";
        } else if (mlmg_bottom_solver == "cg") {
            mlmg.setBottomSolver(MLMG::BottomSolver::cg);
            amrex::Print() << "wind_solver: using CG bottom solver\n";
        } else if (mlmg_bottom_solver == "smoother") {
            mlmg.setBottomSolver(MLMG::BottomSolver::smoother);
            amrex::Print() << "wind_solver: using smoother-only bottom solver\n";
        } else if (mlmg_bottom_solver != "default") {
            amrex::Print() << "wind_solver: warning: unknown bottom solver '" 
                          << mlmg_bottom_solver << "', using default\n";
        }

        lam.setVal(0.0);  // initial guess

        amrex::Print() << "wind_solver: starting MLMG Poisson solve...\n";
        mlmg.solve({&lam}, {&rhs}, tol_rel, Real(0.0));
        amrex::Print() << "wind_solver: MLMG solve complete.\n";
        amrex::Print() << "wind_solver: Poisson solve time = " 
                       << (amrex::second() - t_phase) << " s\n";

        // Fill interior ghost cells of λ (needed for gradient computation)
        lam.FillBoundary(geom.periodicity());

        // ----------------------------------------------------------------
        // 13. Correct velocity field:  u = u0 - α_h² ∂λ/∂x  etc.
        //     One-sided gradient at physical domain boundaries.
        //     Terrain cells are reset to zero.
        // ----------------------------------------------------------------
        t_phase = amrex::second();
        MultiFab vel_c(ba, dm, 3, 0);

        for (MFIter mfi(vel_c); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            const auto v0  = vel0.const_array(mfi);
            const auto la  = lam.const_array(mfi);
            auto       vc  = vel_c.array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real z_physical = z_lo_cap_init + (k + Real(0.5)) * dz_cap_init;
                Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_init + i];
                if (z_agl <= Real(0.0)) {
                    vc(i, j, k, 0) = Real(0.0);
                    vc(i, j, k, 1) = Real(0.0);
                    vc(i, j, k, 2) = Real(0.0);
                    return;
                }

                Real dlx, dly, dlz;
                
                // ∂λ/∂x - choose method based on deriv_method_cap
                if (deriv_method_cap == 0) {  // central
                    if (i == ilo)
                        dlx = (la(i+1,j,k) - la(i,j,k)) * inv1dx;
                    else if (i == ihi)
                        dlx = (la(i,j,k) - la(i-1,j,k)) * inv1dx;
                    else
                        dlx = (la(i+1,j,k) - la(i-1,j,k)) * inv2dx;
                } else if (deriv_method_cap == 1) {  // weno3
                    if (i == ilo)
                        dlx = (la(i+1,j,k) - la(i,j,k)) * inv1dx;
                    else if (i == ihi)
                        dlx = (la(i,j,k) - la(i-1,j,k)) * inv1dx;
                    else
                        dlx = weno3_deriv(la(i-1,j,k), la(i,j,k), la(i+1,j,k), dx_cap);
                } else {  // weno5
                    if (i <= ilo+1)
                        dlx = (la(i+1,j,k) - la(i,j,k)) * inv1dx;
                    else if (i >= ihi-1)
                        dlx = (la(i,j,k) - la(i-1,j,k)) * inv1dx;
                    else
                        dlx = weno5_deriv(la(i-2,j,k), la(i-1,j,k), la(i,j,k), 
                                         la(i+1,j,k), la(i+2,j,k), dx_cap);
                }

                // ∂λ/∂y
                if (deriv_method_cap == 0) {  // central
                    if (j == jlo)
                        dly = (la(i,j+1,k) - la(i,j,k)) * inv1dy;
                    else if (j == jhi)
                        dly = (la(i,j,k) - la(i,j-1,k)) * inv1dy;
                    else
                        dly = (la(i,j+1,k) - la(i,j-1,k)) * inv2dy;
                } else if (deriv_method_cap == 1) {  // weno3
                    if (j == jlo)
                        dly = (la(i,j+1,k) - la(i,j,k)) * inv1dy;
                    else if (j == jhi)
                        dly = (la(i,j,k) - la(i,j-1,k)) * inv1dy;
                    else
                        dly = weno3_deriv(la(i,j-1,k), la(i,j,k), la(i,j+1,k), dy_cap);
                } else {  // weno5
                    if (j <= jlo+1)
                        dly = (la(i,j+1,k) - la(i,j,k)) * inv1dy;
                    else if (j >= jhi-1)
                        dly = (la(i,j,k) - la(i,j-1,k)) * inv1dy;
                    else
                        dly = weno5_deriv(la(i,j-2,k), la(i,j-1,k), la(i,j,k), 
                                         la(i,j+1,k), la(i,j+2,k), dy_cap);
                }

                // ∂λ/∂z
                if (deriv_method_cap == 0) {  // central
                    if (k == klo)
                        dlz = (la(i,j,k+1) - la(i,j,k)) * inv1dz;
                    else if (k == khi)
                        dlz = (la(i,j,k) - la(i,j,k-1)) * inv1dz;
                    else
                        dlz = (la(i,j,k+1) - la(i,j,k-1)) * inv2dz;
                } else if (deriv_method_cap == 1) {  // weno3
                    if (k == klo)
                        dlz = (la(i,j,k+1) - la(i,j,k)) * inv1dz;
                    else if (k == khi)
                        dlz = (la(i,j,k) - la(i,j,k-1)) * inv1dz;
                    else
                        dlz = weno3_deriv(la(i,j,k-1), la(i,j,k), la(i,j,k+1), dz_cap_div);
                } else {  // weno5
                    if (k <= klo+1)
                        dlz = (la(i,j,k+1) - la(i,j,k)) * inv1dz;
                    else if (k >= khi-1)
                        dlz = (la(i,j,k) - la(i,j,k-1)) * inv1dz;
                    else
                        dlz = weno5_deriv(la(i,j,k-2), la(i,j,k-1), la(i,j,k), 
                                         la(i,j,k+1), la(i,j,k+2), dz_cap_div);
                }

                vc(i, j, k, 0) = v0(i, j, k, 0) - bh * dlx;
                vc(i, j, k, 1) = v0(i, j, k, 1) - bh * dly;
                vc(i, j, k, 2) = v0(i, j, k, 2) - bv * dlz;
            });
        }

        amrex::Print() << "wind_solver: velocity correction time = " 
                       << (amrex::second() - t_phase) << " s\n";

        // ----------------------------------------------------------------
        // 14. Compute diagnostics: divergence before and after correction
        // ----------------------------------------------------------------
        t_phase = amrex::second();
        MultiFab div_before(ba, dm, 1, 0);
        MultiFab div_after (ba, dm, 1, 0);

        // Need ghost cells for vel_c (for div_after stencil)
        MultiFab vel_c_g(ba, dm, 3, 1);
        MultiFab::Copy(vel_c_g, vel_c, 0, 0, 3, 0);
        vel_c_g.FillBoundary(geom.periodicity());

        for (MFIter mfi(div_before); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            const auto v0b = vel0.const_array(mfi);
            const auto vcg = vel_c_g.const_array(mfi);
            auto db = div_before.array(mfi);
            auto da = div_after .array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real z_physical = z_lo_cap_init + (k + Real(0.5)) * dz_cap_init;
                Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_init + i];

                // --- divergence before ---
                Real du_b, dv_b, dw_b;
                
                if (deriv_method_cap == 0) {  // central
                    if (i == ilo) du_b = (v0b(i+1,j,k,0)-v0b(i,j,k,0))*inv1dx;
                    else if (i == ihi) du_b = (v0b(i,j,k,0)-v0b(i-1,j,k,0))*inv1dx;
                    else du_b = (v0b(i+1,j,k,0)-v0b(i-1,j,k,0))*inv2dx;

                    if (j == jlo) dv_b = (v0b(i,j+1,k,1)-v0b(i,j,k,1))*inv1dy;
                    else if (j == jhi) dv_b = (v0b(i,j,k,1)-v0b(i,j-1,k,1))*inv1dy;
                    else dv_b = (v0b(i,j+1,k,1)-v0b(i,j-1,k,1))*inv2dy;

                    if (k == klo) dw_b = (v0b(i,j,k+1,2)-v0b(i,j,k,2))*inv1dz;
                    else if (k == khi) dw_b = (v0b(i,j,k,2)-v0b(i,j,k-1,2))*inv1dz;
                    else dw_b = (v0b(i,j,k+1,2)-v0b(i,j,k-1,2))*inv2dz;
                } else if (deriv_method_cap == 1) {  // weno3
                    if (i == ilo) du_b = (v0b(i+1,j,k,0)-v0b(i,j,k,0))*inv1dx;
                    else if (i == ihi) du_b = (v0b(i,j,k,0)-v0b(i-1,j,k,0))*inv1dx;
                    else du_b = weno3_deriv(v0b(i-1,j,k,0), v0b(i,j,k,0), v0b(i+1,j,k,0), dx_cap);

                    if (j == jlo) dv_b = (v0b(i,j+1,k,1)-v0b(i,j,k,1))*inv1dy;
                    else if (j == jhi) dv_b = (v0b(i,j,k,1)-v0b(i,j-1,k,1))*inv1dy;
                    else dv_b = weno3_deriv(v0b(i,j-1,k,1), v0b(i,j,k,1), v0b(i,j+1,k,1), dy_cap);

                    if (k == klo) dw_b = (v0b(i,j,k+1,2)-v0b(i,j,k,2))*inv1dz;
                    else if (k == khi) dw_b = (v0b(i,j,k,2)-v0b(i,j,k-1,2))*inv1dz;
                    else dw_b = weno3_deriv(v0b(i,j,k-1,2), v0b(i,j,k,2), v0b(i,j,k+1,2), dz_cap_div);
                } else {  // weno5
                    if (i <= ilo+1) du_b = (v0b(i+1,j,k,0)-v0b(i,j,k,0))*inv1dx;
                    else if (i >= ihi-1) du_b = (v0b(i,j,k,0)-v0b(i-1,j,k,0))*inv1dx;
                    else du_b = weno5_deriv(v0b(i-2,j,k,0), v0b(i-1,j,k,0), v0b(i,j,k,0),
                                           v0b(i+1,j,k,0), v0b(i+2,j,k,0), dx_cap);

                    if (j <= jlo+1) dv_b = (v0b(i,j+1,k,1)-v0b(i,j,k,1))*inv1dy;
                    else if (j >= jhi-1) dv_b = (v0b(i,j,k,1)-v0b(i,j-1,k,1))*inv1dy;
                    else dv_b = weno5_deriv(v0b(i,j-2,k,1), v0b(i,j-1,k,1), v0b(i,j,k,1),
                                           v0b(i,j+1,k,1), v0b(i,j+2,k,1), dy_cap);

                    if (k <= klo+1) dw_b = (v0b(i,j,k+1,2)-v0b(i,j,k,2))*inv1dz;
                    else if (k >= khi-1) dw_b = (v0b(i,j,k,2)-v0b(i,j,k-1,2))*inv1dz;
                    else dw_b = weno5_deriv(v0b(i,j,k-2,2), v0b(i,j,k-1,2), v0b(i,j,k,2),
                                           v0b(i,j,k+1,2), v0b(i,j,k+2,2), dz_cap_div);
                }

                db(i,j,k) = (z_agl <= Real(0.0)) ? Real(0.0) : (du_b+dv_b+dw_b);

                // --- divergence after ---
                Real du_a, dv_a, dw_a;
                
                if (deriv_method_cap == 0) {  // central
                    if (i == ilo) du_a = (vcg(i+1,j,k,0)-vcg(i,j,k,0))*inv1dx;
                    else if (i == ihi) du_a = (vcg(i,j,k,0)-vcg(i-1,j,k,0))*inv1dx;
                    else du_a = (vcg(i+1,j,k,0)-vcg(i-1,j,k,0))*inv2dx;

                    if (j == jlo) dv_a = (vcg(i,j+1,k,1)-vcg(i,j,k,1))*inv1dy;
                    else if (j == jhi) dv_a = (vcg(i,j,k,1)-vcg(i,j-1,k,1))*inv1dy;
                    else dv_a = (vcg(i,j+1,k,1)-vcg(i,j-1,k,1))*inv2dy;

                    if (k == klo) dw_a = (vcg(i,j,k+1,2)-vcg(i,j,k,2))*inv1dz;
                    else if (k == khi) dw_a = (vcg(i,j,k,2)-vcg(i,j,k-1,2))*inv1dz;
                    else dw_a = (vcg(i,j,k+1,2)-vcg(i,j,k-1,2))*inv2dz;
                } else if (deriv_method_cap == 1) {  // weno3
                    if (i == ilo) du_a = (vcg(i+1,j,k,0)-vcg(i,j,k,0))*inv1dx;
                    else if (i == ihi) du_a = (vcg(i,j,k,0)-vcg(i-1,j,k,0))*inv1dx;
                    else du_a = weno3_deriv(vcg(i-1,j,k,0), vcg(i,j,k,0), vcg(i+1,j,k,0), dx_cap);

                    if (j == jlo) dv_a = (vcg(i,j+1,k,1)-vcg(i,j,k,1))*inv1dy;
                    else if (j == jhi) dv_a = (vcg(i,j,k,1)-vcg(i,j-1,k,1))*inv1dy;
                    else dv_a = weno3_deriv(vcg(i,j-1,k,1), vcg(i,j,k,1), vcg(i,j+1,k,1), dy_cap);

                    if (k == klo) dw_a = (vcg(i,j,k+1,2)-vcg(i,j,k,2))*inv1dz;
                    else if (k == khi) dw_a = (vcg(i,j,k,2)-vcg(i,j,k-1,2))*inv1dz;
                    else dw_a = weno3_deriv(vcg(i,j,k-1,2), vcg(i,j,k,2), vcg(i,j,k+1,2), dz_cap_div);
                } else {  // weno5
                    if (i <= ilo+1) du_a = (vcg(i+1,j,k,0)-vcg(i,j,k,0))*inv1dx;
                    else if (i >= ihi-1) du_a = (vcg(i,j,k,0)-vcg(i-1,j,k,0))*inv1dx;
                    else du_a = weno5_deriv(vcg(i-2,j,k,0), vcg(i-1,j,k,0), vcg(i,j,k,0),
                                           vcg(i+1,j,k,0), vcg(i+2,j,k,0), dx_cap);

                    if (j <= jlo+1) dv_a = (vcg(i,j+1,k,1)-vcg(i,j,k,1))*inv1dy;
                    else if (j >= jhi-1) dv_a = (vcg(i,j,k,1)-vcg(i,j-1,k,1))*inv1dy;
                    else dv_a = weno5_deriv(vcg(i,j-2,k,1), vcg(i,j-1,k,1), vcg(i,j,k,1),
                                           vcg(i,j+1,k,1), vcg(i,j+2,k,1), dy_cap);

                    if (k <= klo+1) dw_a = (vcg(i,j,k+1,2)-vcg(i,j,k,2))*inv1dz;
                    else if (k >= khi-1) dw_a = (vcg(i,j,k,2)-vcg(i,j,k-1,2))*inv1dz;
                    else dw_a = weno5_deriv(vcg(i,j,k-2,2), vcg(i,j,k-1,2), vcg(i,j,k,2),
                                           vcg(i,j,k+1,2), vcg(i,j,k+2,2), dz_cap_div);
                }

                da(i,j,k) = (z_agl <= Real(0.0)) ? Real(0.0) : (du_a+dv_a+dw_a);
            });
        }

        Real div_b_max = div_before.norm0();
        Real div_a_max = div_after .norm0();
        amrex::Print() << "wind_solver: max |div(u)| before correction = "
                       << div_b_max << " s⁻¹\n";
        amrex::Print() << "wind_solver: max |div(u)| after  correction = "
                       << div_a_max << " s⁻¹\n";

        // ----------------------------------------------------------------
        // 15. Assemble output MultiFab and write plotfile
        //
        //    Components:
        //      0  u             corrected x-wind [m/s]
        //      1  v             corrected y-wind [m/s]
        //      2  w             corrected z-wind [m/s]
        //      3  vel_magnitude |U| [m/s]
        //      4  u0            initial (log-law) x-wind [m/s]
        //      5  v0            initial (log-law) y-wind [m/s]
        //      6  w0            initial (log-law) z-wind [m/s]
        //      7  lambda        Lagrange multiplier [m²/s]
        //      8  div_before    ∇·u₀ before correction [s⁻¹]
        //      9  div_after     ∇·u  after  correction [s⁻¹]
        //     10  terrain_z     terrain elevation at column [m]
        //     11  heat_flux     surface sensible heat flux Q_H [W/m²]
        //     12  drag_coeff    drag coefficient Cd [-]
        // ----------------------------------------------------------------
        const int nout = 13;
        const int nx_cap_out = nx;  // capture nx for output section
        MultiFab output(ba, dm, nout, 0);
        
        // Compute diagnostics (heat flux and drag coefficient)
        // Constants for heat flux calculation
        const Real rho_air = 1.225;      // air density [kg/m³] at sea level, 15°C
        const Real cp_air = 1005.0;      // specific heat at constant pressure [J/(kg·K)]
        const Real theta_star = 0.1;     // characteristic temperature scale [K] (typical for neutral conditions)
        const Real kappa_diag = 0.41;    // von Karman constant
        const Real z_ref_diag = z_ref;   // reference height for diagnostics

        for (MFIter mfi(output); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            const auto vc   = vel_c.const_array(mfi);
            const auto v0a  = vel0.const_array(mfi);
            const auto la   = lam.const_array(mfi);
            const auto dib  = div_before.const_array(mfi);
            const auto dia  = div_after.const_array(mfi);
            auto out = output.array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real u = vc(i,j,k,0), v = vc(i,j,k,1), w = vc(i,j,k,2);
                out(i,j,k, 0) = u;
                out(i,j,k, 1) = v;
                out(i,j,k, 2) = w;
                out(i,j,k, 3) = std::sqrt(u*u + v*v + w*w);
                out(i,j,k, 4) = v0a(i,j,k,0);
                out(i,j,k, 5) = v0a(i,j,k,1);
                out(i,j,k, 6) = v0a(i,j,k,2);
                out(i,j,k, 7) = la(i,j,k);
                out(i,j,k, 8) = dib(i,j,k);
                out(i,j,k, 9) = dia(i,j,k);
                out(i,j,k,10) = d_terr_ptr[j * nx_cap_out + i];
                
                // Surface sensible heat flux Q_H = ρ c_p u* θ*
                // Compute friction velocity from near-surface wind speed
                Real z_physical = z_lo_cap_init + (k + Real(0.5)) * dz_cap_init;
                Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_out + i];
                Real u_mag = std::sqrt(u*u + v*v);
                Real ustar_local = Real(0.0);
                Real heat_flux = Real(0.0);
                Real Cd = Real(0.0);
                
                if (z_agl > Real(0.0) && u_mag > Real(1.0e-6)) {
                    // Use local z0 if available, otherwise constant
                    Real z0_local = use_pos_z0 ? d_z0_pos_ptr[j * nx_cap_out + i] : z0_cap;
                    z0_local = std::max(z0_local, Real(1.0e-6));  // avoid division by zero
                    
                    // Estimate u* from near-surface velocity using log-law inverse
                    // u* = κ u / ln((z + z0) / z0)
                    Real log_term = std::log((z_agl + z0_local) / z0_local);
                    if (log_term > Real(0.1)) {
                        ustar_local = kappa_diag * u_mag / log_term;
                        
                        // Heat flux: Q_H = ρ c_p u* θ*  [W/m²]
                        heat_flux = rho_air * cp_air * ustar_local * theta_star;
                        
                        // Drag coefficient Cd = (κ / ln(z/z0))²
                        Cd = (kappa_diag / log_term) * (kappa_diag / log_term);
                    }
                }
                
                out(i,j,k,11) = heat_flux;
                out(i,j,k,12) = Cd;
            });
        }

        Vector<std::string> var_names = {
            "u", "v", "w", "vel_magnitude",
            "u0", "v0", "w0",
            "lambda",
            "div_before", "div_after",
            "terrain_z",
            "heat_flux", "drag_coeff"
        };

        amrex::Print() << "wind_solver: divergence computation time = " 
                       << (amrex::second() - t_phase) << " s\n";

        t_phase = amrex::second();
        WriteSingleLevelPlotfile(plot_file, output, var_names, geom, 0.0, 0);
        amrex::Print() << "wind_solver: plotfile written to " << plot_file << "\n";
        amrex::Print() << "wind_solver: output writing time = " 
                       << (amrex::second() - t_phase) << " s\n";

        // ----------------------------------------------------------------
        // 16. Optional terrain-aligned extraction (multi-height support)
        //
        //  Determine the extraction k-indices (can extract multiple heights):
        //   • If extract_agl_list is non-empty: snap each to the nearest cell
        //   • Else if extract_k_list is non-empty: use those indices directly
        //   • Otherwise skip.
        //
        //  For each height, write a separate CSV file.
        //  Output CSV columns:
        //     x, y, z_terrain, z_physical, z_agl, u, v, w, speed
        // ----------------------------------------------------------------
        const bool do_extract = !extract_agl_list.empty() || !extract_k_list.empty();

        if (do_extract) {
            // Build list of (k_index, agl_value) pairs to extract
            std::vector<std::pair<int, Real>> extraction_levels;
            
            // Process extract_agl_list (priority)
            if (!extract_agl_list.empty()) {
                for (Real agl_req : extract_agl_list) {
                    int k_ext = static_cast<int>(std::floor(agl_req / dz));
                    k_ext = std::max(0, std::min(nz - 1, k_ext));
                    extraction_levels.push_back({k_ext, agl_req});
                }
            }
            // Otherwise process extract_k_list
            else if (!extract_k_list.empty()) {
                for (int k_req : extract_k_list) {
                    int k_ext = std::max(0, std::min(nz - 1, k_req));
                    Real agl_est = (k_ext + Real(0.5)) * dz;
                    extraction_levels.push_back({k_ext, agl_est});
                }
            }
            
            // Extract each level
            for (size_t level_idx = 0; level_idx < extraction_levels.size(); ++level_idx) {
                int k_ext = extraction_levels[level_idx].first;
                Real agl_target = extraction_levels[level_idx].second;
                Real z_phys_ext = z_lo + (k_ext + Real(0.5)) * dz;
                
                amrex::Print() << "wind_solver: terrain-aligned extraction " << (level_idx + 1)
                               << "/" << extraction_levels.size() << " at AGL = "
                               << agl_target << " m  →  k = " << k_ext
                               << "  (physical z = " << z_phys_ext << " m)\n";

                // Ensure all GPU work is complete before host-side data access
                amrex::Gpu::streamSynchronize();

                // Struct for extraction points
                struct ExtPt {
                    Real x, y, z_terrain, z_phys, z_agl_val;
                    Real u, v, w, speed;
                    int gi, gj;   // global cell indices for sort-order
                };
                
                // Collect data points
                std::vector<ExtPt> local_pts;
                local_pts.reserve(static_cast<std::size_t>(nx) * ny / 4 + 1);

                for (MFIter mfi(vel_c, false /*no tiling*/); mfi.isValid(); ++mfi) {
                    const Box& bx = mfi.validbox();
                    // Skip boxes that do not contain the extraction level
                    if (k_ext < bx.smallEnd(2) || k_ext > bx.bigEnd(2)) continue;

                    // On CPU builds const_array() returns host-accessible data.
                    // On GPU builds a Gpu::streamSynchronize() above ensures the
                    // data is up to date; array() here still accesses device memory,
                    // so copy the slice to a host FArrayBox first.
#ifdef AMREX_USE_GPU
                    Box slice_bx(IntVect(bx.smallEnd(0), bx.smallEnd(1), k_ext),
                                  IntVect(bx.bigEnd(0),   bx.bigEnd(1),   k_ext));
                    FArrayBox slice_fab(slice_bx, 3, The_Pinned_Arena());
                    slice_fab.copy<RunOn::Device>(vel_c[mfi], slice_bx);
                    amrex::Gpu::streamSynchronize();
                    auto const& vc = slice_fab.const_array();
#else
                    auto const& vc = vel_c.const_array(mfi);
#endif

                    for (int j = bx.smallEnd(1); j <= bx.bigEnd(1); ++j) {
                        for (int i = bx.smallEnd(0); i <= bx.bigEnd(0); ++i) {
                            Real zs      = terrain_h[static_cast<std::size_t>(j) * nx + i];
                            Real xc      = x_lo + (i + Real(0.5)) * dx;
                            Real yc      = y_lo + (j + Real(0.5)) * dy;
                            Real z_agl_c = z_phys_ext - zs;  // per-column AGL
                            Real u_  = vc(i, j, k_ext, 0);
                            Real v_  = vc(i, j, k_ext, 1);
                            Real w_  = vc(i, j, k_ext, 2);
                            Real spd = std::sqrt(u_*u_ + v_*v_ + w_*w_);
                            local_pts.push_back({xc, yc, zs, z_phys_ext,
                                                 z_agl_c, u_, v_, w_, spd,
                                                 i, j});
                        }
                    }
                }

                // Sort local portion by (j, i) for reproducible output ordering
                std::sort(local_pts.begin(), local_pts.end(),
                          [](const ExtPt& a, const ExtPt& b) {
                              return (a.gj != b.gj) ? (a.gj < b.gj) : (a.gi < b.gi);
                          });

                // Generate output filename for this height
                std::string output_file;
                if (extraction_levels.size() == 1) {
                    // Single height - use the specified extract_file
                    output_file = extract_file;
                } else {
                    // Multiple heights - append height to filename
                    // e.g., wind_extract.csv -> wind_extract_10m.csv
                    size_t dot_pos = extract_file.find_last_of('.');
                    std::string base = (dot_pos != std::string::npos) ? 
                                       extract_file.substr(0, dot_pos) : extract_file;
                    std::string ext = (dot_pos != std::string::npos) ? 
                                      extract_file.substr(dot_pos) : ".csv";
                    std::ostringstream fname;
                    fname << base << "_" << static_cast<int>(agl_target) << "m" << ext;
                    output_file = fname.str();
                }

                // Sequential write: rank 0 creates the file with the header;
                // higher ranks append their portion in rank order.
                const int nranks = amrex::ParallelDescriptor::NProcs();
                const int myrank = amrex::ParallelDescriptor::MyProc();

                auto write_pts = [&](bool write_header) {
                    std::ofstream outf(output_file,
                                       write_header ? std::ios::out
                                                    : std::ios::app);
                        outf << std::scientific << std::setprecision(6);
                    if (write_header) {
                        outf << "x,y,z_terrain,z_physical,z_agl,u,v,w,speed\n";
                    }
                    for (const auto& p : local_pts) {
                        outf << p.x       << ","
                             << p.y       << ","
                             << p.z_terrain << ","
                             << p.z_phys  << ","
                             << p.z_agl_val << ","
                             << p.u       << ","
                             << p.v       << ","
                             << p.w       << ","
                             << p.speed   << "\n";
                    }
                };

                if (myrank == 0) {
                    write_pts(true /*header*/);
                }
                for (int r = 1; r < nranks; ++r) {
                    amrex::ParallelDescriptor::Barrier();
                    if (myrank == r) {
                        write_pts(false /*no header — append*/);
                    }
                }
                amrex::ParallelDescriptor::Barrier();

                amrex::Print() << "wind_solver: terrain-aligned extraction written to "
                               << output_file << "  (" << (nx * ny) << " points)\n";
            } // end loop over extraction levels
        }

        // Print total execution time
        amrex::Print() << "wind_solver: ========================================\n";
        amrex::Print() << "wind_solver: total execution time = " 
                       << (amrex::second() - t_total) << " s\n";
        amrex::Print() << "wind_solver: ========================================\n";
        amrex::Print() << "wind_solver: done.\n";
    }
    amrex::Finalize();
    return 0;
}
