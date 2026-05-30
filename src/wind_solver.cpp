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
//   max_grid_size = 32            # maximum AMReX box size (per dimension)
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
                constexpr Real pi = std::acos(Real(-1.0));
                angle = angle * pi / Real(180.0);  // Convert degrees to radians
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
        // ----------------------------------------------------------------
        // 1. Parse user inputs
        // ----------------------------------------------------------------
        ParmParse pp;

        std::string terrain_file = "terrain.csv";
        pp.query("terrain_file", terrain_file);

        // Wind initialization mode: "loglaw" (default), "uniform", or "raws"
        // "loglaw"  : use log-law profile with U_ref, V_ref at z_ref height
        // "uniform" : use constant U, V everywhere (uniform_U, uniform_V parameters)
        // "raws"    : interpolate from velocity file (X Y Z U V format)
        std::string init_mode = "loglaw";
        pp.query("init_mode", init_mode);

        // Validate init_mode
        if (init_mode != "loglaw" && init_mode != "uniform" && init_mode != "raws") {
            amrex::Abort("wind_solver: invalid init_mode: " + init_mode + 
                         " (must be 'loglaw', 'uniform', or 'raws')");
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

        // RAWS mode parameters
        std::string velocity_file = "velocity.csv";
        pp.query("velocity_file", velocity_file);

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

        int  mlmg_verbose = 1;
        Real tol_rel      = 1.e-8;
        int  max_grid_size = 32;
        std::string plot_file = "plt_wind";
        pp.query("mlmg_verbose",  mlmg_verbose);
        pp.query("tol_rel",       tol_rel);
        pp.query("max_grid_size", max_grid_size);
        pp.query("plot_file",     plot_file);

         // Terrain-aligned extraction parameters
        // extract_agl  : sample at this height above local terrain [m]; snapped to
        //                the nearest cell-centre level.  Takes priority over extract_k.
        // extract_k    : sample at this k-index (0 = lowest model level).
        // Either < 0 disables that mode.  If both are < 0, no extraction is written.
        Real extract_agl = -1.0;
        int  extract_k   = -1;
        std::string extract_file = "wind_extract.csv";
        pp.query("extract_agl",  extract_agl);
        pp.query("extract_k",    extract_k);
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
        
        // Convert deriv_method string to integer for GPU capture
        // 0 = central, 1 = weno3, 2 = weno5
        int deriv_method_int = 0;
        if (deriv_method == "weno3") deriv_method_int = 1;
        else if (deriv_method == "weno5") deriv_method_int = 2;

        // ----------------------------------------------------------------
        // 2. Read terrain file and determine horizontal domain bounds
        // ----------------------------------------------------------------
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

        // ----------------------------------------------------------------
        // 5. Precompute per-column terrain height via IDW (host side)
        // ----------------------------------------------------------------
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

        // ----------------------------------------------------------------
        // 7. Determine vertical domain and build AMReX geometry
        //    Vertical range: [z_lo, z_hi] where
        //        z_lo = minimum terrain elevation (= zs_min)
        //        z_hi = maximum obstacle elevation + domain_height
        //    This ensures the domain covers all terrain and extends at least
        //    domain_height metres above the highest obstacle point.
        // ----------------------------------------------------------------
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

        // For RAWS mode: device vectors for wind field interpolation
        Gpu::DeviceVector<Real> d_vel_u(0), d_vel_v(0);
        Real const* d_vel_u_ptr = nullptr;
        Real const* d_vel_v_ptr = nullptr;

        // Common capture variables for wind field initialization and correction
        const Real dz_cap_init    = dz;
        const Real z_lo_cap_init  = z_lo;   // physical z at bottom of domain
        const int  nx_cap_init    = nx;

        // ----------------------------------------------------------------
        // 9. Fill initial wind field based on initialization mode
        // ----------------------------------------------------------------
        amrex::Print() << "wind_solver: initializing wind field with mode: " << init_mode << "\n";

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

            // Capture parameters for GPU lambda
            const Real ustar_cap = ustar;
            const Real kappa_cap = kappa;
            const Real z0_cap    = z0;
            const Real ux_h      = ux_hat;
            const Real uy_h      = uy_hat;

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
                        Real speed = canopy_wind_profile(
                            z_agl, canopy_params, z0_cap, ustar_cap, kappa_cap);
                        vel(i, j, k, 0) = speed * ux_h;
                        vel(i, j, k, 1) = speed * uy_h;
                        vel(i, j, k, 2) = Real(0.0);
                    }
                });
            }
        } else if (init_mode == "uniform") {
            // Uniform wind field initialization (constant U, V)
            const Real u_uniform = uniform_U;
            const Real v_uniform = uniform_V;

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
                        vel(i, j, k, 0) = u_uniform;
                        vel(i, j, k, 1) = v_uniform;
                        vel(i, j, k, 2) = Real(0.0);
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
        // 10. Compute divergence of initial wind  →  RHS = -(∇·u0)
        //    One-sided differences at physical domain boundaries;
        //    centred differences (or WENO) in the interior.
        //    Terrain (sub-surface) cells: rhs = 0 (not enforced).
        // ----------------------------------------------------------------
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
        const Real bh = alpha_h * alpha_h;
        const Real bv = alpha_v * alpha_v;
        Array<MultiFab, AMREX_SPACEDIM> bcoef;
        bcoef[0].define(convert(ba, IntVect(1, 0, 0)), dm, 1, 0);
        bcoef[1].define(convert(ba, IntVect(0, 1, 0)), dm, 1, 0);
        bcoef[2].define(convert(ba, IntVect(0, 0, 1)), dm, 1, 0);
        bcoef[0].setVal(bh);
        bcoef[1].setVal(bh);
        bcoef[2].setVal(bv);
        mlabec.setBCoeffs(0, GetArrOfConstPtrs(bcoef));

        // Level BC: homogeneous (λ = 0 on Dirichlet faces)
        mlabec.setLevelBC(0, nullptr);

        // ----------------------------------------------------------------
        // 12. Solve with MLMG
        // ----------------------------------------------------------------
        MLMG mlmg(mlabec);
        mlmg.setMaxIter(200);
        mlmg.setMaxFmgIter(20);
        mlmg.setVerbose(mlmg_verbose);
        mlmg.setBottomVerbose(0);
        mlmg.setPreSmooth(16);
        mlmg.setPostSmooth(16);

        lam.setVal(0.0);  // initial guess

        amrex::Print() << "wind_solver: starting MLMG Poisson solve...\n";
        mlmg.solve({&lam}, {&rhs}, tol_rel, Real(0.0));
        amrex::Print() << "wind_solver: MLMG solve complete.\n";

        // Fill interior ghost cells of λ (needed for gradient computation)
        lam.FillBoundary(geom.periodicity());

        // ----------------------------------------------------------------
        // 13. Correct velocity field:  u = u0 - α_h² ∂λ/∂x  etc.
        //     One-sided gradient at physical domain boundaries.
        //     Terrain cells are reset to zero.
        // ----------------------------------------------------------------
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

        // ----------------------------------------------------------------
        // 14. Compute diagnostics: divergence before and after correction
        // ----------------------------------------------------------------
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
        // ----------------------------------------------------------------
        const int nout = 11;
        const int nx_cap_out = nx;  // capture nx for output section
        MultiFab output(ba, dm, nout, 0);

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
            });
        }

        Vector<std::string> var_names = {
            "u", "v", "w", "vel_magnitude",
            "u0", "v0", "w0",
            "lambda",
            "div_before", "div_after",
            "terrain_z"
        };

        WriteSingleLevelPlotfile(plot_file, output, var_names, geom, 0.0, 0);
        amrex::Print() << "wind_solver: plotfile written to " << plot_file << "\n";

        // ----------------------------------------------------------------
        // 16. Optional terrain-aligned extraction
        //
        //  Determine the extraction k-index:
        //   • If extract_agl >= 0: snap to the cell whose physical centre
        //     lies at z_lo + extract_agl (i.e. extract_agl above the minimum
        //     terrain level),
        //         k_ext = clamp( floor(extract_agl / dz), 0, nz-1 )
        //   • Else if extract_k >= 0: use that index directly (clamped).
        //   • Otherwise skip.
        //
        //  For each horizontal column (i, j) the extracted point has:
        //     z_terrain  = interpolated terrain elevation [m]
        //     z_physical = z_lo + (k_ext + 0.5) * dz     [m, same for all columns]
        //     z_agl      = z_physical - z_terrain(i,j)    [m above local terrain, per-column]
        //
        //  Output CSV columns:
        //     x, y, z_terrain, z_physical, z_agl, u, v, w, speed
        // ----------------------------------------------------------------
        const bool do_extract = (extract_agl >= Real(0.0)) || (extract_k >= 0);

        if (do_extract) {
            // Determine k_ext
            // extract_agl is the requested height above local terrain [m].
            // Physical z of the target level = z_lo + (k+0.5)*dz, so
            //   k_ext = floor(extract_agl / dz)
            // (z_lo cancels when measuring AGL from the minimum-terrain baseline).
            int k_ext = -1;
            Real z_phys_ext = Real(0.0);  // physical z at k_ext cell centre

            if (extract_agl >= Real(0.0)) {
                // Snap requested AGL to the nearest cell-centre level
                k_ext = static_cast<int>(std::floor(extract_agl / dz));
                k_ext = std::max(0, std::min(nz - 1, k_ext));
                z_phys_ext = z_lo + (k_ext + Real(0.5)) * dz;
                amrex::Print() << "wind_solver: terrain-aligned extraction at AGL = "
                               << extract_agl << " m  →  k = " << k_ext
                               << "  (physical z = " << z_phys_ext << " m)\n";
            } else {
                k_ext = std::max(0, std::min(nz - 1, extract_k));
                z_phys_ext = z_lo + (k_ext + Real(0.5)) * dz;
                amrex::Print() << "wind_solver: terrain-aligned extraction at k = "
                               << k_ext << "  (physical z = " << z_phys_ext << " m)\n";
            }

            // Ensure all GPU work is complete before host-side data access
            amrex::Gpu::streamSynchronize();

            // Collect (x, y, z_terrain, z_physical, z_agl, u, v, w, speed)
            // per column for the k_ext level.
            // z_agl is computed per-column: z_phys_ext - z_terrain(i,j)
            // Each MPI rank collects its own portion; all ranks write
            // sequentially to produce a complete file.
            struct ExtPt {
                Real x, y, z_terrain, z_phys, z_agl_val;
                Real u, v, w, speed;
                int gi, gj;   // global cell indices for sort-order
            };
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

            // Sequential write: rank 0 creates the file with the header;
            // higher ranks append their portion in rank order.
            const int nranks = amrex::ParallelDescriptor::NProcs();
            const int myrank = amrex::ParallelDescriptor::MyProc();

            auto write_pts = [&](bool write_header) {
                std::ofstream outf(extract_file,
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
                           << extract_file << "  (" << (nx * ny) << " points)\n";
        }

        amrex::Print() << "wind_solver: done.\n";
    }
    amrex::Finalize();
    return 0;
}
