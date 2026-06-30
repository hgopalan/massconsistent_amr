#include "wind_solver_api.H"
#include "wind_interpolation.H"
#include "wind_io_helpers.H"
#include "canopy_models.H"
#include "morphometric_models.H"
#include "terrain_following_coords.H"
#include "cell_local_anisotropy.H"
#include "solver_math_constants.H"
#include "turbulent_stress.H"

#include <AMReX_FArrayBox.H>
#include <AMReX_Gpu.H>
#include <AMReX_GpuContainers.H>
#include <AMReX_LO_BCTYPES.H>
#include <AMReX_MLABecLaplacian.H>
#include <AMReX_MLMG.H>
#include <AMReX_ParmParse.H>
#include <AMReX_PlotFileUtil.H>
#include <AMReX_Print.H>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace amrex;

std::unique_ptr<WindSolverState> g_wind_solver_state = nullptr;

// Default plot_fields value: all available fields in order
constexpr const char* DEFAULT_PLOT_FIELDS = "u,v,w,vel_magnitude,u0,v0,w0,lambda,div0,div,terrain_z";
// Number of available plot fields
constexpr int NUM_PLOT_FIELDS = 11;

namespace {

constexpr Real DISTANCE_EPSILON = Real(1.0e-12);

std::pair<Real, Real> idw_velocity_3d(Real xq, Real yq, Real zq,
                                      const std::vector<Real>& x,
                                      const std::vector<Real>& y,
                                      const std::vector<Real>& z,
                                      const std::vector<Real>& ux_data,
                                      const std::vector<Real>& uy_data,
                                      int k = 6,
                                      Real gamma = 1.0,
                                      bool enable_shielding = false,
                                      const std::vector<Real>& terrain_h = {},
                                      Real x_lo = 0.0, Real y_lo = 0.0,
                                      Real dx = 1.0, Real dy = 1.0,
                                      int nx = 0, int ny = 0,
                                      Real rmax = -1.0,
                                      Real idw_exponent = 2.0);

struct WindSolverRuntimeData {
    Gpu::DeviceVector<Real> terrain_device;
    std::vector<Real> terrain_host;
    Gpu::DeviceVector<Real> morphometric_d_device;
    Gpu::DeviceVector<Real> morphometric_z0_device;
};

std::unique_ptr<WindSolverRuntimeData> g_wind_solver_runtime = nullptr;
bool g_amrex_initialized_here = false;
bool g_parmparse_initialized = false;

bool ensure_amrex_initialized()
{
    if (!amrex::Initialized()) {
        int argc = 0;
        char** argv = nullptr;
        amrex::Initialize(argc, argv, false);
        g_amrex_initialized_here = true;
    }
    return true;
}

void require_initialized()
{
    if (!g_wind_solver_state || !g_wind_solver_state->initialized) {
        throw std::runtime_error("wind solver is not initialized");
    }
}

void read_terrain_file(const std::string& filename,
                       std::vector<Real>& xd,
                       std::vector<Real>& yd,
                       std::vector<Real>& zd)
{
    std::ifstream input(filename);
    if (!input.is_open()) {
        throw std::runtime_error("cannot open terrain file: " + filename);
    }

    std::string line;
    while (std::getline(input, line)) {
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream iss(line);
        Real x, y, z;
        if (iss >> x >> y >> z) {
            xd.push_back(x);
            yd.push_back(y);
            zd.push_back(z);
        }
    }

    if (xd.empty()) {
        throw std::runtime_error("no terrain data read from: " + filename);
    }
}

void read_building_file(const std::string& filename,
                        std::vector<Real>& xmin,
                        std::vector<Real>& xmax,
                        std::vector<Real>& ymin,
                        std::vector<Real>& ymax,
                        std::vector<Real>& zmin,
                        std::vector<Real>& zmax,
                        std::vector<int>& geom_type,
                        std::vector<std::vector<Real>>& polygon_x,
                        std::vector<std::vector<Real>>& polygon_y,
                        std::vector<int>& parent_id)
{
    std::ifstream input(filename);
    if (!input.is_open()) {
        throw std::runtime_error("cannot open building file: " + filename);
    }

    std::string line;
    int line_num = 0;
    while (std::getline(input, line)) {
        line_num++;
        
        // Remove comments
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        // Skip empty lines
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream iss(line);
        std::string token;
        if (!(iss >> token)) continue;
        
        if (token == "POLYGON:") {
            // Polygon building: POLYGON: x1 y1 x2 y2 ... xn yn | zmin zmax [height]
            // Example: POLYGON: 0.0 0.0 100.0 0.0 100.0 100.0 0.0 100.0 | 0.0 30.0
            int n_verts = 0;
            std::vector<Real> vx, vy;
            Real x, y;
            
            // Read vertices until delimiter
            while (iss >> x >> y) {
                vx.push_back(x);
                vy.push_back(y);
                n_verts++;
            }
            
            // Read delimiter (should be |) but in stream form it's optional
            // Read zmin and zmax
            Real z_min, z_max;
            if (n_verts >= 3) {  // Need at least 3 vertices for a polygon
                // For polygon, xmin/xmax/ymin/ymax are bounding box
                Real xmin_poly = *std::min_element(vx.begin(), vx.end());
                Real xmax_poly = *std::max_element(vx.begin(), vx.end());
                Real ymin_poly = *std::min_element(vy.begin(), vy.end());
                Real ymax_poly = *std::max_element(vy.begin(), vy.end());
                
                // Try to read zmin and zmax from remaining input or use defaults
                if (iss >> z_min >> z_max) {
                    // Success
                } else {
                    z_min = 0.0;
                    z_max = 30.0;  // Default height
                }
                
                xmin.push_back(xmin_poly);
                xmax.push_back(xmax_poly);
                ymin.push_back(ymin_poly);
                ymax.push_back(ymax_poly);
                zmin.push_back(z_min);
                zmax.push_back(z_max);
                geom_type.push_back(1);  // BuildingGeometryType::POLYGON
                polygon_x.push_back(vx);
                polygon_y.push_back(vy);
                parent_id.push_back(-1);
            } else {
                amrex::Warning("Polygon building on line " + std::to_string(line_num) + 
                             " has " + std::to_string(n_verts) + " vertices; need >= 3. Skipping.\n");
            }
            
        } else if (token == "VOID:") {
            // Void zone (internal courtyard): VOID: x1 y1 x2 y2 ... | zmin zmax
            // Format same as POLYGON but marks an exclusion zone
            int n_verts = 0;
            std::vector<Real> vx, vy;
            Real x, y;
            
            while (iss >> x >> y) {
                vx.push_back(x);
                vy.push_back(y);
                n_verts++;
            }
            
            Real z_min, z_max;
            if (n_verts >= 3) {
                Real xmin_void = *std::min_element(vx.begin(), vx.end());
                Real xmax_void = *std::max_element(vx.begin(), vx.end());
                Real ymin_void = *std::min_element(vy.begin(), vy.end());
                Real ymax_void = *std::max_element(vy.begin(), vy.end());
                
                if (iss >> z_min >> z_max) {
                    // Success
                } else {
                    z_min = 0.0;
                    z_max = 30.0;
                }
                
                xmin.push_back(xmin_void);
                xmax.push_back(xmax_void);
                ymin.push_back(ymin_void);
                ymax.push_back(ymax_void);
                zmin.push_back(z_min);
                zmax.push_back(z_max);
                geom_type.push_back(2);  // BuildingGeometryType::VOID
                polygon_x.push_back(vx);
                polygon_y.push_back(vy);
                parent_id.push_back(-1);
            } else {
                amrex::Warning("Void zone on line " + std::to_string(line_num) + 
                             " has " + std::to_string(n_verts) + " vertices; need >= 3. Skipping.\n");
            }
            
        } else {
            // Rectangular building (backward compatible): x1 x2 y1 y2 z1 z2
            Real x1 = std::stod(token);
            Real x2, y1, y2, z1, z2;
            
            if (iss >> x2 >> y1 >> y2 >> z1 >> z2) {
                xmin.push_back(x1);
                xmax.push_back(x2);
                ymin.push_back(y1);
                ymax.push_back(y2);
                zmin.push_back(z1);
                zmax.push_back(z2);
                geom_type.push_back(0);  // BuildingGeometryType::RECTANGULAR_BLOCK
                polygon_x.push_back(std::vector<Real>());  // Empty for rectangles
                polygon_y.push_back(std::vector<Real>());
                parent_id.push_back(-1);
            }
        }
    }

    if (xmin.empty()) {
        throw std::runtime_error("no building data read from: " + filename);
    }
}

// Legacy wrapper for backward compatibility
void read_building_file(const std::string& filename,
                        std::vector<Real>& xmin,
                        std::vector<Real>& xmax,
                        std::vector<Real>& ymin,
                        std::vector<Real>& ymax,
                        std::vector<Real>& zmin,
                        std::vector<Real>& zmax)
{
    std::vector<int> dummy_geom_type;
    std::vector<std::vector<Real>> dummy_poly_x, dummy_poly_y;
    std::vector<int> dummy_parent_id;
    read_building_file(filename, xmin, xmax, ymin, ymax, zmin, zmax,
                      dummy_geom_type, dummy_poly_x, dummy_poly_y, dummy_parent_id);
}

Real idw_terrain(Real xq, Real yq,
                 const std::vector<Real>& x,
                 const std::vector<Real>& y,
                 const std::vector<Real>& z,
                 int k = 6,
                 Real idw_exponent = 2.0)
{
    const int n = static_cast<int>(x.size());
    k = std::min(k, n);

    std::vector<std::pair<Real, int>> d2(n);
    for (int i = 0; i < n; ++i) {
        const Real dx = x[i] - xq;
        const Real dy = y[i] - yq;
        d2[i] = {dx * dx + dy * dy, i};
    }
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0;
    Real zval = 0.0;
    for (int i = 0; i < k; ++i) {
        if (d2[i].first < DISTANCE_EPSILON) {
            return z[d2[i].second];
        }
        const Real w = std::pow(d2[i].first, -idw_exponent / Real(2.0));
        wsum += w;
        zval += w * z[d2[i].second];
    }
    return zval / wsum;
}

void read_velocity_file(const std::string& filename,
                        std::vector<Real>& xd,
                        std::vector<Real>& yd,
                        std::vector<Real>& zd,
                        std::vector<Real>& ux,
                        std::vector<Real>& uy)
{
    std::ifstream input(filename);
    if (!input.is_open()) {
        throw std::runtime_error("cannot open velocity file: " + filename);
    }

    std::string line;
    while (std::getline(input, line)) {
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream iss(line);
        Real x, y, z, u, v;
        if (iss >> x >> y >> z >> u >> v) {
            xd.push_back(x);
            yd.push_back(y);
            zd.push_back(z);
            ux.push_back(u);
            uy.push_back(v);
        }
    }

    if (xd.empty()) {
        throw std::runtime_error("no velocity data read from: " + filename);
    }
}

[[maybe_unused]] std::pair<Real, Real> idw_velocity(Real xq, Real yq,
                                   const std::vector<Real>& x,
                                   const std::vector<Real>& y,
                                   const std::vector<Real>& ux_data,
                                   const std::vector<Real>& uy_data,
                                   int k = 6,
                                   Real idw_exponent = 2.0)
{
    const int n = static_cast<int>(x.size());
    k = std::min(k, n);

    std::vector<std::pair<Real, int>> d2(n);
    for (int i = 0; i < n; ++i) {
        const Real dx = x[i] - xq;
        const Real dy = y[i] - yq;
        d2[i] = {dx * dx + dy * dy, i};
    }
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0;
    Real ux_val = 0.0;
    Real uy_val = 0.0;
    for (int i = 0; i < k; ++i) {
        if (d2[i].first < DISTANCE_EPSILON) {
            return {ux_data[d2[i].second], uy_data[d2[i].second]};
        }
        const Real w = std::pow(d2[i].first, -idw_exponent / Real(2.0));
        wsum += w;
        ux_val += w * ux_data[d2[i].second];
        uy_val += w * uy_data[d2[i].second];
    }
    return {ux_val / wsum, uy_val / wsum};
}

void read_vertical_profile_csv(const std::string& filename,
                               std::vector<Real>& xd,
                               std::vector<Real>& yd,
                               std::vector<Real>& zd,
                               std::vector<Real>& ux,
                               std::vector<Real>& uy)
{
    std::ifstream input(filename);
    if (!input.is_open()) {
        throw std::runtime_error("cannot open vertical profile file: " + filename);
    }

    std::string line;
    bool is_first = true;
    while (std::getline(input, line)) {
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        if (line.empty()) continue;

        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream iss(line);

        if (is_first) {
            std::string first_token;
            if (iss >> first_token) {
                try {
                    static_cast<void>(std::stod(first_token));
                } catch (...) {
                    is_first = false;
                    continue;
                }
            }
            is_first = false;
            iss.clear();
            iss.str(line);
        }

        Real x, y, z, speed, direction;
        if (iss >> x >> y >> z >> speed >> direction) {
            xd.push_back(x);
            yd.push_back(y);
            zd.push_back(z);
            
            Real dir_rad = direction * MathConstants::deg_to_rad;
            Real u_val = -speed * std::sin(dir_rad);
            Real v_val = -speed * std::cos(dir_rad);
            ux.push_back(u_val);
            uy.push_back(v_val);
        }
    }

    if (xd.empty()) {
        throw std::runtime_error("no vertical profile data read from: " + filename);
    }
}

std::pair<Real, Real> idw_velocity_3d(Real xq, Real yq, Real zq,
                                      const std::vector<Real>& x,
                                      const std::vector<Real>& y,
                                      const std::vector<Real>& z,
                                      const std::vector<Real>& ux_data,
                                      const std::vector<Real>& uy_data,
                                      int k,
                                      Real gamma,
                                      bool enable_shielding,
                                      const std::vector<Real>& terrain_h,
                                      Real x_lo, Real y_lo,
                                      Real dx, Real dy,
                                      int nx, int ny,
                                      Real rmax,
                                      Real idw_exponent)
{
    return WindInterpolation::idw_velocity_3d(xq, yq, zq, x, y, z, ux_data, uy_data, k,
                                              gamma, enable_shielding, terrain_h, x_lo, y_lo, dx, dy, nx, ny,
                                              rmax, idw_exponent);
}

void parse_inputs(WindSolverState& state, const std::string& inputs_file)
{
    if (amrex::Initialized()) {
        ParmParse::Finalize();
    }
    ParmParse::Initialize(0, nullptr, inputs_file.c_str());
    g_parmparse_initialized = true;

    ParmParse pp;

    std::string terrain_file = "terrain.csv";
    pp.query("terrain_file", terrain_file);

    state.U_ref = 10.0;
    state.V_ref = 0.0;
    state.z_ref = 10.0;
    state.z0 = 0.1;
    pp.query("U_ref", state.U_ref);
    pp.query("V_ref", state.V_ref);
    pp.query("z_ref", state.z_ref);
    pp.query("z0", state.z0);

    state.dx = 30.0;
    state.dy = 30.0;
    state.dz = 30.0;
    pp.query("dx", state.dx);
    pp.query("dy", state.dy);
    pp.query("dz", state.dz);

    Real domain_height = 300.0;
    pp.query("domain_height", domain_height);

    state.alpha_h = 1.0;
    state.alpha_v = 1.0;
    state.idw_gamma = 1.0;
    state.idw_exponent = 2.0;
    state.idw_rmax1 = -1.0;
    state.idw_rmax2 = -1.0;
    state.idw_r1 = -1.0;
    state.idw_r2 = -1.0;
    state.ekman_latitude = 45.0;
    state.ekman_ug = 10.0;
    state.ekman_vg = 0.0;
    state.ekman_Km = 5.0;
    pp.query("alpha_h", state.alpha_h);
    pp.query("alpha_v", state.alpha_v);
    pp.query("idw_gamma", state.idw_gamma);
    pp.query("idw_exponent", state.idw_exponent);
    pp.query("idw_rmax1", state.idw_rmax1);
    pp.query("idw_rmax2", state.idw_rmax2);
    pp.query("idw_r1", state.idw_r1);
    pp.query("idw_r2", state.idw_r2);
    pp.query("ekman_latitude", state.ekman_latitude);
    pp.query("ekman_ug", state.ekman_ug);
    pp.query("ekman_vg", state.ekman_vg);
    pp.query("ekman_Km", state.ekman_Km);

    // Cell-local spatially-varying anisotropy
    state.enable_cell_local_anisotropy = false;
    state.anisotropy_source = "all";
    state.anisotropy_slope_scale = 0.25;
    state.anisotropy_decay_height = 500.0;
    state.anisotropy_ri_gamma = 1.0;
    state.anisotropy_ri_beta = 0.5;
    state.anisotropy_fr_min = 0.1;
    state.temperature_gradient = 0.0;
    pp.query("enable_cell_local_anisotropy", state.enable_cell_local_anisotropy);
    pp.query("anisotropy_source", state.anisotropy_source);
    pp.query("anisotropy_slope_scale", state.anisotropy_slope_scale);
    pp.query("anisotropy_decay_height", state.anisotropy_decay_height);
    pp.query("anisotropy_ri_gamma", state.anisotropy_ri_gamma);
    pp.query("anisotropy_ri_beta", state.anisotropy_ri_beta);
    pp.query("anisotropy_fr_min", state.anisotropy_fr_min);
    pp.query("temperature_gradient", state.temperature_gradient);
    
    // 3D Scalar transport parameters
    state.enable_3d_scalars = false;
    state.enable_temperature_transport = false;
    state.enable_moisture_transport = false;
    state.temperature_diffusivity = 2.5e-5;
    state.moisture_diffusivity = 2.2e-5;
    state.scalar_dt = -1.0;
    state.scalar_cfl = 0.8;
    state.multi_step_corrector_steps = 1;
    
    pp.query("enable_3d_scalars", state.enable_3d_scalars);
    pp.query("enable_temperature_transport", state.enable_temperature_transport);
    pp.query("enable_moisture_transport", state.enable_moisture_transport);
    pp.query("temperature_diffusivity", state.temperature_diffusivity);
    pp.query("moisture_diffusivity", state.moisture_diffusivity);
    pp.query("scalar_dt", state.scalar_dt);
    pp.query("scalar_cfl", state.scalar_cfl);
    pp.query("multi_step_corrector_steps", state.multi_step_corrector_steps);
    
    // Mixing length turbulence model parameters
    state.enable_mixing_length_turbulence = true;
    state.mixing_length_coefficient = 0.1;
    state.von_karman = 0.41;
    state.zground = 0.1;
    
    pp.query("enable_mixing_length_turbulence", state.enable_mixing_length_turbulence);
    pp.query("mixing_length_coefficient", state.mixing_length_coefficient);
    pp.query("von_karman", state.von_karman);
    pp.query("zground", state.zground);

    // Double-pass turbulent stress correction (disabled by default)
    state.enable_turbulent_stress = false;
    state.turbulent_stress_method = "mixing_length";
    state.turbulent_stress_mixing_length_coefficient = 0.1;
    state.turbulent_schmidt_number_horizontal = 1.0;
    state.turbulent_schmidt_number_vertical = 1.3;

    pp.query("enable_turbulent_stress", state.enable_turbulent_stress);
    pp.query("turbulent_stress_method", state.turbulent_stress_method);
    pp.query("turbulent_stress_mixing_length_coefficient", state.turbulent_stress_mixing_length_coefficient);
    pp.query("turbulent_schmidt_number_horizontal", state.turbulent_schmidt_number_horizontal);
    pp.query("turbulent_schmidt_number_vertical", state.turbulent_schmidt_number_vertical);
    
    // If any transport is enabled, automatically enable 3D scalars
    if (state.enable_temperature_transport || state.enable_moisture_transport) {
        state.enable_3d_scalars = true;
    }

    state.mlmg_verbose = 1;
    state.tol_rel = 1.e-8;
    state.tol_abs = 0.0;
    state.max_iter = 200;
    int max_grid_size = 32;
    pp.query("mlmg_verbose", state.mlmg_verbose);
    pp.query("tol_rel", state.tol_rel);
    pp.query("tol_abs", state.tol_abs);
    pp.query("max_iter", state.max_iter);
    pp.query("max_grid_size", max_grid_size);

    state.plot_file = "plt_wind";
    state.extract_file = "wind_extract.csv";
    state.extract_agl = -1.0;
    state.extract_k = -1;
    state.plot_fields = DEFAULT_PLOT_FIELDS;
    pp.query("plot_file", state.plot_file);
    pp.query("extract_file", state.extract_file);
    pp.query("extract_agl", state.extract_agl);
    pp.query("extract_k", state.extract_k);
    pp.query("plot_fields", state.plot_fields);

    state.enable_topographic_shielding = false;
    pp.query("enable_topographic_shielding", state.enable_topographic_shielding);

    state.init_mode = "loglaw";
    pp.query("init_mode", state.init_mode);
    if (state.init_mode != "loglaw" && state.init_mode != "uniform" && state.init_mode != "raws" && state.init_mode != "surface_data" && state.init_mode != "ekman_spiral" && state.init_mode != "sounding" && state.init_mode != "powerlaw") {
        throw std::runtime_error("invalid init_mode: " + state.init_mode);
    }

    state.powerlaw_exponent = 0.15;
    pp.query("powerlaw_exponent", state.powerlaw_exponent);

    state.uniform_U = state.U_ref;
    state.uniform_V = state.V_ref;
    pp.query("uniform_U", state.uniform_U);
    pp.query("uniform_V", state.uniform_V);

    state.velocity_file = "velocity.csv";
    pp.query("velocity_file", state.velocity_file);

    // Sounding profiles parameters
    {
        int n_sfiles = pp.countval("sounding_files");
        if (n_sfiles > 0) {
            state.sounding_files.resize(n_sfiles);
            pp.getarr("sounding_files", state.sounding_files, 0, n_sfiles);
        }
    }
    {
        int n_sx = pp.countval("sounding_x");
        if (n_sx > 0) {
            state.sounding_x.resize(n_sx);
            pp.getarr("sounding_x", state.sounding_x, 0, n_sx);
        }
    }
    {
        int n_sy = pp.countval("sounding_y");
        if (n_sy > 0) {
            state.sounding_y.resize(n_sy);
            pp.getarr("sounding_y", state.sounding_y, 0, n_sy);
        }
    }
    std::string s_file = "";
    pp.query("sounding_file", s_file);
    if (!s_file.empty()) {
        state.sounding_files.push_back(s_file);
    }
    state.sounding_vertical_interp = "spline";
    pp.query("sounding_vertical_interp", state.sounding_vertical_interp);
    state.sounding_wind_in_knots = true;
    pp.query("sounding_wind_in_knots", state.sounding_wind_in_knots);

    // Marine Boundary Layer parameters
    state.enable_marine_bl = false;
    state.marine_sst = 288.15;
    state.marine_air_sea_dt = 0.0;
    pp.query("enable_marine_bl", state.enable_marine_bl);
    pp.query("marine_sst", state.marine_sst);
    pp.query("marine_air_sea_dt", state.marine_air_sea_dt);

    // Canopy model parameters
    state.enable_canopy = false;
    state.canopy_height = 0.0;
    state.frontal_area_index = 0.0;
    state.plan_area_index = 0.0;
    state.canopy_drag_coeff = 0.2;
    state.canopy_attenuation = 2.5;
    state.use_exponential_profile = false;
    pp.query("enable_canopy", state.enable_canopy);
    pp.query("canopy_height", state.canopy_height);
    pp.query("frontal_area_index", state.frontal_area_index);
    pp.query("plan_area_index", state.plan_area_index);
    pp.query("canopy_drag_coeff", state.canopy_drag_coeff);
    pp.query("canopy_attenuation", state.canopy_attenuation);
    pp.query("use_exponential_profile", state.use_exponential_profile);

    // Morphometric model parameters
    state.enable_morphometric_models = false;
    state.morphometric_model_type = "macdonald";
    state.morphometric_drag_coeff = -1.0;
    pp.query("enable_morphometric_models", state.enable_morphometric_models);
    pp.query("morphometric_model_type", state.morphometric_model_type);
    pp.query("morphometric_drag_coeff", state.morphometric_drag_coeff);
    if (state.morphometric_drag_coeff < 0.0) {
        if (state.morphometric_model_type == "bottema") {
            state.morphometric_drag_coeff = 0.8;
        } else {
            state.morphometric_drag_coeff = 1.2;
        }
    }
    
    // Ekman spiral veer parameters
    state.enable_ekman_veer = false;
    state.latitude = 45.0;
    state.ekman_veer_total = 20.0;
    state.ekman_veer_height = 200.0;
    pp.query("enable_ekman_veer", state.enable_ekman_veer);
    pp.query("latitude", state.latitude);
    pp.query("ekman_veer_total", state.ekman_veer_total);
    pp.query("ekman_veer_height", state.ekman_veer_height);

    // Analytical Turbine Wake parameters
    state.enable_turbine_wake = false;
    state.turbine_file = "";
    state.turbine_wake_model_type = "jensen";
    state.turbine_wake_superposition = "quadratic";
    state.jensen_kw = 0.075;
    state.gaussian_ka = 0.05;
    state.enable_stability_correction = false;
    state.stability_length = 1000.0;
    state.turbopark_c1 = 0.38;
    state.ambient_ti = 0.075;
    state.enable_jimenez_deflection = false;
    state.enable_bastankhah_deflection = false;
    state.jimenez_kd = 0.05;
    state.wake_added_turbulence_model = "none";
    state.enable_wake_ground_interaction = true;
    state.wake_ground_damping_scale = 0.25;
    state.surface_sensible_heat_flux = 0.0;
    state.buoyant_wake_destruction_coeff = 0.005;

    pp.query("enable_turbine_wake", state.enable_turbine_wake);
    pp.query("turbine_file", state.turbine_file);
    pp.query("turbine_wake_model_type", state.turbine_wake_model_type);
    pp.query("turbine_wake_superposition", state.turbine_wake_superposition);
    pp.query("jensen_kw", state.jensen_kw);
    pp.query("gaussian_ka", state.gaussian_ka);
    pp.query("enable_stability_correction", state.enable_stability_correction);
    pp.query("stability_length", state.stability_length);
    pp.query("turbopark_c1", state.turbopark_c1);
    pp.query("ambient_ti", state.ambient_ti);
    pp.query("enable_jimenez_deflection", state.enable_jimenez_deflection);
    pp.query("enable_bastankhah_deflection", state.enable_bastankhah_deflection);
    pp.query("jimenez_kd", state.jimenez_kd);
    pp.query("wake_added_turbulence_model", state.wake_added_turbulence_model);
    pp.query("enable_wake_ground_interaction", state.enable_wake_ground_interaction);
    pp.query("wake_ground_damping_scale", state.wake_ground_damping_scale);
    pp.query("surface_sensible_heat_flux", state.surface_sensible_heat_flux);
    pp.query("buoyant_wake_destruction_coeff", state.buoyant_wake_destruction_coeff);

    if (state.enable_turbine_wake && !state.turbine_file.empty()) {
        TurbineWake::read_turbines_file(state.turbine_file, state.turbines);
    }

    // Electrical Wire Loading parameters
    state.enable_wire_loading = false;
    state.wire_file = "";
    state.wire_output_file = "wire_output.csv";
    pp.query("enable_wire_loading", state.enable_wire_loading);
    pp.query("wire_file", state.wire_file);
    pp.query("wire_output_file", state.wire_output_file);

    if (state.enable_wire_loading && !state.wire_file.empty()) {
        WireLoading::read_wires_file(state.wire_file, state.wires);
    }

    // Terrain-following (streamline) coordinates parameters
    state.enable_terrain_following = false;
    state.terrain_decay_height = -1.0;  // Default: auto-set to domain_height / 3
    pp.query("enable_terrain_following", state.enable_terrain_following);
    pp.query("terrain_decay_height", state.terrain_decay_height);
    
    // Auto-set decay height if not specified
    if (state.enable_terrain_following && state.terrain_decay_height < 0.0) {
        state.terrain_decay_height = domain_height / Real(3.0);
        amrex::Print() << "terrain_following: auto-setting decay_height = "
                       << state.terrain_decay_height << " m (domain_height / 3)\n";
    }

    if (terrain_file == "synthetic") {
        Real synth_xmin = 0.0;
        Real synth_xmax = 300.0;
        Real synth_ymin = 0.0;
        Real synth_ymax = 300.0;
        int synth_nx = 11;
        int synth_ny = 11;
        std::string synth_type = "multi_gaussian_hill";

        pp.query("synthetic_xmin", synth_xmin);
        pp.query("synthetic_xmax", synth_xmax);
        pp.query("synthetic_ymin", synth_ymin);
        pp.query("synthetic_ymax", synth_ymax);
        pp.query("synthetic_nx", synth_nx);
        pp.query("synthetic_ny", synth_ny);
        pp.query("synthetic_type", synth_type);

        std::vector<Real> peaks;
        std::vector<Real> sigmas;
        std::vector<Real> centers_x;
        std::vector<Real> centers_y;

        if (synth_type == "gaussian_hill") {
            Real peak = 50.0;
            Real sigma = 60.0;
            Real center_x = (synth_xmin + synth_xmax) / 2.0;
            Real center_y = (synth_ymin + synth_ymax) / 2.0;
            pp.query("synthetic_peak", peak);
            pp.query("synthetic_sigma", sigma);
            pp.query("synthetic_center_x", center_x);
            pp.query("synthetic_center_y", center_y);

            peaks.push_back(peak);
            sigmas.push_back(sigma);
            centers_x.push_back(center_x);
            centers_y.push_back(center_y);
        } else if (synth_type == "multi_gaussian_hill") {
            int n_vals = pp.countval("synthetic_peaks");
            if (n_vals > 0) {
                peaks.resize(n_vals);
                pp.getarr("synthetic_peaks", peaks, 0, n_vals);
            }
            n_vals = pp.countval("synthetic_sigmas");
            if (n_vals > 0) {
                sigmas.resize(n_vals);
                pp.getarr("synthetic_sigmas", sigmas, 0, n_vals);
            }
            n_vals = pp.countval("synthetic_centers_x");
            if (n_vals > 0) {
                centers_x.resize(n_vals);
                pp.getarr("synthetic_centers_x", centers_x, 0, n_vals);
            }
            n_vals = pp.countval("synthetic_centers_y");
            if (n_vals > 0) {
                centers_y.resize(n_vals);
                pp.getarr("synthetic_centers_y", centers_y, 0, n_vals);
            }

            // Defaults if arrays are empty
            if (peaks.empty()) {
                peaks = {50.0, 30.0};
                sigmas = {60.0, 40.0};
                centers_x = {100.0, 200.0};
                centers_y = {150.0, 150.0};
            }
        } else {
            throw std::runtime_error("invalid synthetic_type: " + synth_type);
        }

        if (peaks.size() != sigmas.size() || peaks.size() != centers_x.size() || peaks.size() != centers_y.size()) {
            throw std::runtime_error("size mismatch in synthetic terrain arrays: peaks, sigmas, centers_x, and centers_y must all have the same size.");
        }

        // Generate the grid points of the synthetic terrain point cloud
        state.terrain_x_data.clear();
        state.terrain_y_data.clear();
        state.terrain_z_data.clear();

        for (int j = 0; j < synth_ny; ++j) {
            Real y = synth_ymin + j * (synth_ymax - synth_ymin) / std::max(1, synth_ny - 1);
            for (int i = 0; i < synth_nx; ++i) {
                Real x = synth_xmin + i * (synth_xmax - synth_xmin) / std::max(1, synth_nx - 1);
                
                Real z = 0.0;
                for (std::size_t m = 0; m < peaks.size(); ++m) {
                    Real r_sq = (x - centers_x[m]) * (x - centers_x[m]) + (y - centers_y[m]) * (y - centers_y[m]);
                    z += peaks[m] * std::exp(-r_sq / (2.0 * sigmas[m] * sigmas[m]));
                }
                state.terrain_x_data.push_back(x);
                state.terrain_y_data.push_back(y);
                state.terrain_z_data.push_back(z);
            }
        }
        amrex::Print() << "wind_solver: generated synthetic terrain with type: " << synth_type << ", " << state.terrain_x_data.size() << " points\n";
    } else {
        read_terrain_file(terrain_file,
                          state.terrain_x_data,
                          state.terrain_y_data,
                          state.terrain_z_data);
    }
    
    // Parse building file (optional)
    std::string building_file = "";
    pp.query("building_file", building_file);
    if (!building_file.empty()) {
        read_building_file(building_file,
                          state.building_xmin, state.building_xmax,
                          state.building_ymin, state.building_ymax,
                          state.building_zmin, state.building_zmax,
                          state.building_geom_type,
                          state.building_polygon_x, state.building_polygon_y,
                          state.building_parent_id);
    }

    const Real x_lo = *std::min_element(state.terrain_x_data.begin(), state.terrain_x_data.end());
    const Real x_hi = *std::max_element(state.terrain_x_data.begin(), state.terrain_x_data.end());
    const Real y_lo = *std::min_element(state.terrain_y_data.begin(), state.terrain_y_data.end());
    const Real y_hi = *std::max_element(state.terrain_y_data.begin(), state.terrain_y_data.end());

    if (x_hi <= x_lo || y_hi <= y_lo) {
        throw std::runtime_error("terrain file does not define a valid 2-D domain");
    }
    if (state.dx <= Real(0.0) || state.dy <= Real(0.0) || state.dz <= Real(0.0) || domain_height <= Real(0.0)) {
        throw std::runtime_error("grid spacing and domain_height must be positive");
    }

    state.nx = std::max(1, static_cast<int>(std::round((x_hi - x_lo) / state.dx)));
    state.ny = std::max(1, static_cast<int>(std::round((y_hi - y_lo) / state.dy)));
    state.dx = (x_hi - x_lo) / state.nx;
    state.dy = (y_hi - y_lo) / state.ny;
    state.xmin = x_lo;
    state.xmax = x_hi;
    state.ymin = y_lo;
    state.ymax = y_hi;

    g_wind_solver_runtime = std::make_unique<WindSolverRuntimeData>();
    g_wind_solver_runtime->terrain_host.resize(static_cast<std::size_t>(state.nx) * state.ny);

    // Compute terrain heights via IDW
    for (int j = 0; j < state.ny; ++j) {
        const Real yc = state.ymin + (j + Real(0.5)) * state.dy;
        for (int i = 0; i < state.nx; ++i) {
            const Real xc = state.xmin + (i + Real(0.5)) * state.dx;
            g_wind_solver_runtime->terrain_host[static_cast<std::size_t>(j) * state.nx + i] =
                idw_terrain(xc, yc,
                            state.terrain_x_data,
                            state.terrain_y_data,
                            state.terrain_z_data,
                            6,
                            state.idw_exponent);
        }
    }

    state.zs_min = *std::min_element(g_wind_solver_runtime->terrain_host.begin(),
                                     g_wind_solver_runtime->terrain_host.end());
    state.zs_max = *std::max_element(g_wind_solver_runtime->terrain_host.begin(),
                                     g_wind_solver_runtime->terrain_host.end());

    // Apply buildings to create obstacle height field
    if (!state.building_xmin.empty()) {
        int n_buildings = static_cast<int>(state.building_xmin.size());
        for (int b = 0; b < n_buildings; ++b) {
            Real bx1 = state.building_xmin[b];
            Real bx2 = state.building_xmax[b];
            Real by1 = state.building_ymin[b];
            Real by2 = state.building_ymax[b];
            Real bz2 = state.building_zmax[b];
            
            for (int j = 0; j < state.ny; ++j) {
                Real yc = state.ymin + (j + Real(0.5)) * state.dy;
                for (int i = 0; i < state.nx; ++i) {
                    Real xc = state.xmin + (i + Real(0.5)) * state.dx;
                    if (xc >= bx1 && xc <= bx2 && yc >= by1 && yc <= by2) {
                        std::size_t idx = static_cast<std::size_t>(j) * state.nx + i;
                        // Set obstacle height to building top (zmax)
                        g_wind_solver_runtime->terrain_host[idx] = 
                            std::max(g_wind_solver_runtime->terrain_host[idx], bz2);
                    }
                }
            }
        }
    }
    
    state.obs_max = *std::max_element(g_wind_solver_runtime->terrain_host.begin(),
                                      g_wind_solver_runtime->terrain_host.end());

    state.zmin = state.zs_min;
    state.zmax = state.obs_max + domain_height;
    state.nz = std::max(1, static_cast<int>(std::round((state.zmax - state.zmin) / state.dz)));
    state.dz = (state.zmax - state.zmin) / state.nz;

    g_wind_solver_runtime->terrain_device.resize(g_wind_solver_runtime->terrain_host.size());
    Gpu::copy(Gpu::hostToDevice,
              g_wind_solver_runtime->terrain_host.begin(),
              g_wind_solver_runtime->terrain_host.end(),
              g_wind_solver_runtime->terrain_device.begin());

    IntVect dom_lo(0, 0, 0);
    IntVect dom_hi(state.nx - 1, state.ny - 1, state.nz - 1);
    Box domain(dom_lo, dom_hi);
    RealBox rb({state.xmin, state.ymin, state.zmin},
               {state.xmax, state.ymax, state.zmax});
    Array<int, AMREX_SPACEDIM> is_periodic{0, 0, 0};

    state.geom = std::make_unique<Geometry>(domain, &rb, CoordSys::cartesian, is_periodic.data());
    state.ba = std::make_unique<BoxArray>(domain);
    state.ba->maxSize(max_grid_size);
    state.dm = std::make_unique<DistributionMapping>(*state.ba);

    state.vel = std::make_unique<MultiFab>(*state.ba, *state.dm, 3, 1);
    state.vel0 = std::make_unique<MultiFab>(*state.ba, *state.dm, 3, 1);
    state.lambda = std::make_unique<MultiFab>(*state.ba, *state.dm, 1, 1);
    state.div0 = std::make_unique<MultiFab>(*state.ba, *state.dm, 1, 0);
    state.terrain = std::make_unique<MultiFab>(*state.ba, *state.dm, 1, 0);
    state.alpha_h_field = std::make_unique<MultiFab>(*state.ba, *state.dm, 1, 1);
    state.alpha_v_field = std::make_unique<MultiFab>(*state.ba, *state.dm, 1, 1);
    
    // Initialize diagnostic flux fields
    state.u_star = std::make_unique<MultiFab>(*state.ba, *state.dm, 1, 0);
    state.tau_flux = std::make_unique<MultiFab>(*state.ba, *state.dm, 1, 0);
    state.cd = std::make_unique<MultiFab>(*state.ba, *state.dm, 1, 0);
    state.shf = std::make_unique<MultiFab>(*state.ba, *state.dm, 1, 0);
    state.lhf = std::make_unique<MultiFab>(*state.ba, *state.dm, 1, 0);

    state.vel->setVal(0.0);
    state.vel0->setVal(0.0);
    state.u_star->setVal(0.0);
    state.tau_flux->setVal(0.0);
    state.cd->setVal(0.0);
    state.shf->setVal(0.0);
    state.lhf->setVal(0.0);
    state.lambda->setVal(0.0);
    state.div0->setVal(0.0);
    state.terrain->setVal(0.0);
    state.alpha_h_field->setVal(state.alpha_h);
    state.alpha_v_field->setVal(state.alpha_v);

    const Real* terrain_ptr = g_wind_solver_runtime->terrain_device.data();
    const int nx = state.nx;
    for (MFIter mfi(*state.terrain); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        auto arr = state.terrain->array(mfi);
        ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
            arr(i, j, k) = terrain_ptr[j * nx + i];
        });
    }

    // Query synthetic turbulence parameters to mark them as used in API mode
    {
        bool enable_synthetic_turbulence = false;
        ParmParse pp_turb("turbulence");
        if (!pp.query("enable_synthetic_turbulence", enable_synthetic_turbulence)) {
            pp_turb.query("enabled", enable_synthetic_turbulence);
        }
        if (enable_synthetic_turbulence) {
            std::string spectrum_model_str = "";
            std::string intensity_model_str = "";
            std::string coherence_model_str = "";
            if (!pp.query("turbulence_spectrum_model", spectrum_model_str)) {
                pp_turb.query("spectrum_model", spectrum_model_str);
            }
            if (!pp.query("turbulence_intensity_model", intensity_model_str)) {
                pp_turb.query("intensity_model", intensity_model_str);
            }
            if (!pp.query("turbulence_coherence_model", coherence_model_str)) {
                pp_turb.query("coherence_model", coherence_model_str);
            }
            Real dummy_real = 0.0;
            int dummy_int = 0;
            bool dummy_bool = false;
            std::string dummy_str = "";
            if (!pp.query("turbulence_intensity_ref", dummy_real)) pp_turb.query("intensity_ref", dummy_real);
            if (!pp.query("turbulence_z_intensity_ref", dummy_real)) pp_turb.query("z_intensity_ref", dummy_real);
            if (!pp.query("turbulence_intensity_exponent", dummy_real)) pp_turb.query("intensity_exponent", dummy_real);
            if (!pp.query("turbulence_hub_height", dummy_real)) pp_turb.query("hub_height", dummy_real);
            if (!pp.query("turbulence_iec_category", dummy_str)) pp_turb.query("iec_category", dummy_str);
            if (!pp.query("turbulence_coherence_powerlaw_exponent", dummy_real)) pp_turb.query("coherence_powerlaw_exponent", dummy_real);
            if (!pp.query("turbulence_length_scale_u", dummy_real)) pp_turb.query("length_scale_u", dummy_real);
            if (!pp.query("turbulence_length_scale_v", dummy_real)) pp_turb.query("length_scale_v", dummy_real);
            if (!pp.query("turbulence_length_scale_w", dummy_real)) pp_turb.query("length_scale_w", dummy_real);
            if (!pp.query("turbulence_coherence_decay_vertical", dummy_real)) pp_turb.query("coherence_decay_vertical", dummy_real);
            if (!pp.query("turbulence_coherence_decay_lateral", dummy_real)) pp_turb.query("coherence_decay_lateral", dummy_real);
            if (!pp.query("turbulence_anisotropy_ratio_v", dummy_real)) pp_turb.query("anisotropy_ratio_v", dummy_real);
            if (!pp.query("turbulence_anisotropy_ratio_w", dummy_real)) pp_turb.query("anisotropy_ratio_w", dummy_real);
            
            if (spectrum_model_str == "MannBox" || spectrum_model_str == "") {
                if (!pp.query("mann_length_scale_u", dummy_real)) pp_turb.query("mann_length_scale_u", dummy_real);
                if (!pp.query("mann_length_scale_v", dummy_real)) pp_turb.query("mann_length_scale_v", dummy_real);
                if (!pp.query("mann_length_scale_w", dummy_real)) pp_turb.query("mann_length_scale_w", dummy_real);
                if (!pp.query("mann_variance_u", dummy_real)) pp_turb.query("mann_variance_u", dummy_real);
                if (!pp.query("mann_variance_v", dummy_real)) pp_turb.query("mann_variance_v", dummy_real);
                if (!pp.query("mann_variance_w", dummy_real)) pp_turb.query("mann_variance_w", dummy_real);
                if (!pp.query("mann_asymmetry_parameter", dummy_real)) pp_turb.query("mann_asymmetry_parameter", dummy_real);
                if (!pp.query("mann_eddy_lifetime", dummy_real)) pp_turb.query("mann_eddy_lifetime", dummy_real);
                if (!pp.query("mann_terrain_adaptation_factor", dummy_real)) pp_turb.query("mann_terrain_adaptation_factor", dummy_real);
            }
            if (intensity_model_str == "IEC61400") {
                pp.query("hub_height", dummy_real);
                pp.query("iec_turbulence_category", dummy_str);
            }
            if (!pp.query("coherence_powerlaw_exponent", dummy_real)) pp_turb.query("coherence_powerlaw_exponent", dummy_real);
            if (!pp.query("turbulence_random_seed", dummy_int)) pp_turb.query("random_seed", dummy_int);
            if (!pp.query("turbulence_enable_stability_correction", dummy_bool)) pp_turb.query("enable_stability_correction", dummy_bool);
            if (!pp.query("turbulence_monin_obukhov_length", dummy_real)) pp_turb.query("monin_obukhov_length", dummy_real);
            if (!pp.query("turbulence_stability_parameterization", dummy_str)) pp_turb.query("stability_parameterization", dummy_str);
        }
    }
}

void initialize_wind_field(WindSolverState& state)
{
    const Real* terrain_ptr = g_wind_solver_runtime->terrain_device.data();
    const Real z_lo = state.zmin;
    const Real dz = state.dz;
    const int nx = state.nx;

    state.vel0->setVal(0.0);
    state.vel->setVal(0.0);
    state.lambda->setVal(0.0);
    state.div0->setVal(0.0);
    state.solved = false;
    state.mlmg_iters = 0;
    state.mlmg_res = 0.0;

    if (state.init_mode == "loglaw") {
        const Real z0 = state.z0;
        const Real speed_ref = std::sqrt(state.U_ref * state.U_ref + state.V_ref * state.V_ref);
        const Real kappa = 0.41;
        const Real ustar = (speed_ref > Real(1.0e-10))
                             ? kappa * speed_ref / std::log((state.z_ref + z0) / z0)
                             : Real(0.0);
        const Real ux_hat = (speed_ref > Real(1.0e-10)) ? state.U_ref / speed_ref : Real(1.0);
        const Real uy_hat = (speed_ref > Real(1.0e-10)) ? state.V_ref / speed_ref : Real(0.0);

        const bool enable_morph = state.enable_morphometric_models;
        if (enable_morph) {
            const int nx = state.nx;
            const int ny = state.ny;
            const std::size_t grid_size = static_cast<std::size_t>(nx) * ny;
            std::vector<Real> morph_d_host(grid_size, Real(0.0));
            std::vector<Real> morph_z0_host(grid_size, state.z0);

            std::vector<Real> lambda_p_grid(grid_size, Real(0.0));
            std::vector<Real> lambda_f_x_grid(grid_size, Real(0.0));
            std::vector<Real> lambda_f_y_grid(grid_size, Real(0.0));
            std::vector<Real> H_avg_grid(grid_size, Real(0.0));
            std::vector<Real> sum_weight_grid(grid_size, Real(0.0));

            Real cell_area = state.dx * state.dy;

            if (!state.building_xmin.empty()) {
                int n_buildings = static_cast<int>(state.building_xmin.size());
                for (int j = 0; j < ny; ++j) {
                    Real cell_y1 = state.ymin + j * state.dy;
                    Real cell_y2 = state.ymin + (j + 1) * state.dy;
                    for (int i = 0; i < nx; ++i) {
                        Real cell_x1 = state.xmin + i * state.dx;
                        Real cell_x2 = state.xmin + (i + 1) * state.dx;
                        std::size_t idx = static_cast<std::size_t>(j) * nx + i;

                        for (int b = 0; b < n_buildings; ++b) {
                            Real bx1 = state.building_xmin[b];
                            Real bx2 = state.building_xmax[b];
                            Real by1 = state.building_ymin[b];
                            Real by2 = state.building_ymax[b];
                            Real bz1 = state.building_zmin[b];
                            Real bz2 = state.building_zmax[b];
                            Real H_b = bz2 - bz1;

                            Real ix1 = std::max(bx1, cell_x1);
                            Real ix2 = std::min(bx2, cell_x2);
                            Real iy1 = std::max(by1, cell_y1);
                            Real iy2 = std::min(by2, cell_y2);

                            if (ix2 > ix1 && iy2 > iy1) {
                                Real w_x = ix2 - ix1;
                                Real w_y = iy2 - iy1;
                                Real A_p_b = w_x * w_y;

                                lambda_p_grid[idx] += A_p_b;
                                lambda_f_x_grid[idx] += w_y * H_b;
                                lambda_f_y_grid[idx] += w_x * H_b;
                                H_avg_grid[idx] += A_p_b * H_b;
                                sum_weight_grid[idx] += A_p_b;
                            }
                        }

                        if (sum_weight_grid[idx] > Real(1.0e-10)) {
                            H_avg_grid[idx] /= sum_weight_grid[idx];
                        } else {
                            H_avg_grid[idx] = Real(0.0);
                        }

                        lambda_p_grid[idx] /= cell_area;
                        lambda_f_x_grid[idx] /= cell_area;
                        lambda_f_y_grid[idx] /= cell_area;
                        
                        // Clamp plan area index (lambda_p) to [0, 0.95] to prevent division by zero in equations
                        // when the canopy is completely solid, and clamp frontal area indices (lambda_f) to [0, 2.0]
                        // to keep them within physically realistic limits for highly dense obstacle arrays.
                        lambda_p_grid[idx] = std::max(Real(0.0), std::min(lambda_p_grid[idx], Real(0.95)));
                        lambda_f_x_grid[idx] = std::max(Real(0.0), std::min(lambda_f_x_grid[idx], Real(2.0)));
                        lambda_f_y_grid[idx] = std::max(Real(0.0), std::min(lambda_f_y_grid[idx], Real(2.0)));
                    }
                }
            }

            Real abs_ux = std::abs(ux_hat);
            Real abs_uy = std::abs(uy_hat);

            for (std::size_t idx = 0; idx < grid_size; ++idx) {
                Real lambda_f = abs_ux * lambda_f_x_grid[idx] + abs_uy * lambda_f_y_grid[idx];
                Real H = H_avg_grid[idx];
                Real lp = lambda_p_grid[idx];
                
                Real d_val = Real(0.0);
                Real z0_val = state.z0;

                if (state.morphometric_model_type == "macdonald") {
                    MorphometricModels::compute_macdonald(H, lp, lambda_f, state.morphometric_drag_coeff, state.z0, d_val, z0_val);
                } else if (state.morphometric_model_type == "kutzbach") {
                    MorphometricModels::compute_kutzbach(H, lp, lambda_f, state.morphometric_drag_coeff, state.z0, d_val, z0_val);
                } else if (state.morphometric_model_type == "bottema") {
                    MorphometricModels::compute_bottema(H, lp, lambda_f, state.morphometric_drag_coeff, state.z0, d_val, z0_val);
                }

                morph_d_host[idx] = d_val;
                morph_z0_host[idx] = z0_val;
            }

            g_wind_solver_runtime->morphometric_d_device.resize(morph_d_host.size());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice, morph_d_host.begin(), morph_d_host.end(), g_wind_solver_runtime->morphometric_d_device.begin());

            g_wind_solver_runtime->morphometric_z0_device.resize(morph_z0_host.size());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice, morph_z0_host.begin(), morph_z0_host.end(), g_wind_solver_runtime->morphometric_z0_device.begin());
        }

        // Setup canopy parameters
        CanopyParams canopy_params;
        canopy_params.enabled = state.enable_canopy;
        canopy_params.height = state.canopy_height;
        canopy_params.frontal_area_index = state.frontal_area_index;
        canopy_params.plan_area_index = state.plan_area_index;
        canopy_params.drag_coefficient = state.canopy_drag_coeff;
        canopy_params.attenuation_coeff = state.canopy_attenuation;
        canopy_params.use_exponential_profile = state.use_exponential_profile;

        const Real* d_morph_d = enable_morph ? g_wind_solver_runtime->morphometric_d_device.data() : nullptr;
        const Real* d_morph_z0 = enable_morph ? g_wind_solver_runtime->morphometric_z0_device.data() : nullptr;

        for (MFIter mfi(*state.vel0); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = state.vel0->array(mfi);
            ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
                const Real z_phys = z_lo + (k + Real(0.5)) * dz;
                const Real z_agl = z_phys - terrain_ptr[j * nx + i];
                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else {
                    Real speed;
                    if (enable_morph) {
                        const Real d_local = d_morph_d[j * nx + i];
                        const Real z0_cell = d_morph_z0[j * nx + i];
                        speed = log_law_with_displacement(z_agl, d_local, z0_cell, ustar, kappa);
                    } else {
                        speed = canopy_wind_profile(
                            z_agl, canopy_params, z0, ustar, kappa);
                    }
                    vel(i, j, k, 0) = speed * ux_hat;
                    vel(i, j, k, 1) = speed * uy_hat;
                    vel(i, j, k, 2) = Real(0.0);
                }
            });
        }
    } else if (state.init_mode == "ekman_spiral") {
        const Real lat = state.ekman_latitude;
        const Real Ug = state.ekman_ug;
        const Real Vg = state.ekman_vg;
        const Real Km = state.ekman_Km;
        const Real pi_val = MathConstants::pi;
        const Real omega = 7.27e-5;
        const Real f_coriolis = 2.0 * omega * std::sin(lat * pi_val / 180.0);
        const Real abs_f = std::abs(f_coriolis);
        const Real a_ekman = (abs_f > 1.0e-8) ? std::sqrt(abs_f / (2.0 * Km)) : 0.0;

        for (MFIter mfi(*state.vel0); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = state.vel0->array(mfi);
            ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
                const Real z_phys = z_lo + (k + Real(0.5)) * dz;
                const Real z_agl = z_phys - terrain_ptr[j * nx + i];
                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else {
                    if (abs_f > Real(1.0e-8)) {
                        Real exp_term = std::exp(-a_ekman * z_agl);
                        Real cos_term = std::cos(a_ekman * z_agl);
                        Real sin_term = std::sin(a_ekman * z_agl);
                        if (f_coriolis >= Real(0.0)) {
                            vel(i, j, k, 0) = Ug * (Real(1.0) - exp_term * cos_term) - Vg * exp_term * sin_term;
                            vel(i, j, k, 1) = Vg * (Real(1.0) - exp_term * cos_term) + Ug * exp_term * sin_term;
                        } else {
                            vel(i, j, k, 0) = Ug * (Real(1.0) - exp_term * cos_term) + Vg * exp_term * sin_term;
                            vel(i, j, k, 1) = Vg * (Real(1.0) - exp_term * cos_term) - Ug * exp_term * sin_term;
                        }
                    } else {
                        vel(i, j, k, 0) = Ug;
                        vel(i, j, k, 1) = Vg;
                    }
                    vel(i, j, k, 2) = Real(0.0);
                }
            });
        }
    } else if (state.init_mode == "sounding") {
        if (state.sounding_files.empty()) {
            throw std::runtime_error("init_mode is sounding but sounding_files is empty!");
        }
        if (state.sounding_x.size() != state.sounding_files.size() || state.sounding_y.size() != state.sounding_files.size()) {
            throw std::runtime_error("sounding_x and sounding_y must have the same size as sounding_files!");
        }

        struct SoundingStation {
            Real x;
            Real y;
            std::vector<Real> z;
            std::vector<Real> u;
            std::vector<Real> v;
            WindInterpolation::CubicSpline1D spline_u;
            WindInterpolation::CubicSpline1D spline_v;
        };

        std::vector<SoundingStation> stations(state.sounding_files.size());
        for (std::size_t s = 0; s < state.sounding_files.size(); ++s) {
            stations[s].x = state.sounding_x[s];
            stations[s].y = state.sounding_y[s];
            WindIO::read_sounding_file(state.sounding_files[s], stations[s].z, stations[s].u, stations[s].v, state.sounding_wind_in_knots);
            if (state.sounding_vertical_interp == "spline") {
                stations[s].spline_u = WindInterpolation::CubicSpline1D(stations[s].z, stations[s].u);
                stations[s].spline_v = WindInterpolation::CubicSpline1D(stations[s].z, stations[s].v);
            }
        }

        std::vector<Real> vel_u_host(static_cast<std::size_t>(state.nx) * state.ny * state.nz);
        std::vector<Real> vel_v_host(static_cast<std::size_t>(state.nx) * state.ny * state.nz);

        for (int k = 0; k < state.nz; ++k) {
            Real zc = z_lo + (k + Real(0.5)) * dz;
            Real rmax = (k == 0) ? state.idw_rmax1 : state.idw_rmax2;
            Real R_param = (k == 0) ? state.idw_r1 : state.idw_r2;
            for (int j = 0; j < state.ny; ++j) {
                Real yc = state.ymin + (j + Real(0.5)) * state.dy;
                for (int i = 0; i < state.nx; ++i) {
                    Real xc = state.xmin + (i + Real(0.5)) * state.dx;
                    Real d_min = std::numeric_limits<Real>::max();
                    bool any_station_within_rmax = false;
                    for (std::size_t s = 0; s < state.sounding_files.size(); ++s) {
                        Real dx_to_station = state.sounding_x[s] - xc;
                        Real dy_to_station = state.sounding_y[s] - yc;
                        Real dist = std::sqrt(dx_to_station * dx_to_station + dy_to_station * dy_to_station);
                        if (rmax <= Real(0.0) || dist <= rmax) {
                            any_station_within_rmax = true;
                            if (dist < d_min) {
                                d_min = dist;
                            }
                        }
                    }

                    // 1D Vertical interpolation for each sounding station
                    std::vector<Real> station_u(state.sounding_files.size());
                    std::vector<Real> station_v(state.sounding_files.size());
                    for (std::size_t s = 0; s < state.sounding_files.size(); ++s) {
                        if (state.sounding_vertical_interp == "spline") {
                            station_u[s] = stations[s].spline_u.evaluate(zc);
                            station_v[s] = stations[s].spline_v.evaluate(zc);
                        } else {
                            station_u[s] = WindInterpolation::log_linear_interpolate(zc, stations[s].z, stations[s].u);
                            station_v[s] = WindInterpolation::log_linear_interpolate(zc, stations[s].z, stations[s].v);
                        }
                    }

                    // 2D Horizontal IDW
                    auto [u_cell, v_cell] = WindInterpolation::idw_velocity(
                        xc, yc, state.sounding_x, state.sounding_y, station_u, station_v, 6, state.idw_exponent);

                    Real u_final = u_cell;
                    Real v_final = v_cell;

                    if (R_param > Real(0.0)) {
                        Real speed_ref = std::sqrt(state.U_ref * state.U_ref + state.V_ref * state.V_ref);
                        Real u_bg = 0.0, v_bg = 0.0;
                        if (speed_ref > Real(1.0e-10)) {
                            Real z_agl = zc - g_wind_solver_runtime->terrain_host[j * state.nx + i];
                            if (z_agl > Real(0.0)) {
                                Real ustar_bg = speed_ref * Real(0.41) / std::log((state.z_ref + state.z0) / state.z0);
                                Real speed_bg = (ustar_bg / Real(0.41)) * std::log((z_agl + state.z0) / state.z0);
                                u_bg = speed_bg * state.U_ref / speed_ref;
                                v_bg = speed_bg * state.V_ref / speed_ref;
                            }
                        }

                        if (!any_station_within_rmax) {
                            u_final = u_bg;
                            v_final = v_bg;
                        } else {
                            Real weight_bg = (d_min / R_param) * (d_min / R_param);
                            u_final = (u_cell + weight_bg * u_bg) / (Real(1.0) + weight_bg);
                            v_final = (v_cell + weight_bg * v_bg) / (Real(1.0) + weight_bg);
                        }
                    }
                    std::size_t idx = (static_cast<std::size_t>(k) * state.ny + j) * state.nx + i;
                    vel_u_host[idx] = u_final;
                    vel_v_host[idx] = v_final;
                }
            }
        }

        Gpu::DeviceVector<Real> vel_u_dev(vel_u_host.size());
        Gpu::DeviceVector<Real> vel_v_dev(vel_v_host.size());
        Gpu::copy(Gpu::hostToDevice, vel_u_host.begin(), vel_u_host.end(), vel_u_dev.begin());
        Gpu::copy(Gpu::hostToDevice, vel_v_host.begin(), vel_v_host.end(), vel_v_dev.begin());
        const Real* vel_u_ptr = vel_u_dev.data();
        const Real* vel_v_ptr = vel_v_dev.data();
        const int nx_val = state.nx;
        const int ny_val = state.ny;

        for (MFIter mfi(*state.vel0); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = state.vel0->array(mfi);
            ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
                const Real z_phys = z_lo + (k + Real(0.5)) * dz;
                const Real z_agl = z_phys - terrain_ptr[j * nx_val + i];
                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else {
                    std::size_t idx = (static_cast<std::size_t>(k) * ny_val + j) * nx_val + i;
                    vel(i, j, k, 0) = vel_u_ptr[idx];
                    vel(i, j, k, 1) = vel_v_ptr[idx];
                    vel(i, j, k, 2) = Real(0.0);
                }
            });
        }
    } else if (state.init_mode == "powerlaw") {
        const Real speed_ref = std::sqrt(state.U_ref * state.U_ref + state.V_ref * state.V_ref);
        const Real ux_hat = (speed_ref > Real(1.0e-10)) ? state.U_ref / speed_ref : Real(1.0);
        const Real uy_hat = (speed_ref > Real(1.0e-10)) ? state.V_ref / speed_ref : Real(0.0);
        const Real powerlaw_exponent = state.powerlaw_exponent;
        const Real z_ref = state.z_ref;

        for (MFIter mfi(*state.vel0); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = state.vel0->array(mfi);
            ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
                const Real z_phys = z_lo + (k + Real(0.5)) * dz;
                const Real z_agl = z_phys - terrain_ptr[j * nx + i];
                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else {
                    const Real speed = speed_ref * std::pow(z_agl / z_ref, powerlaw_exponent);
                    vel(i, j, k, 0) = speed * ux_hat;
                    vel(i, j, k, 1) = speed * uy_hat;
                    vel(i, j, k, 2) = Real(0.0);
                }
            });
        }
    } else if (state.init_mode == "uniform") {
        const Real u_uniform = state.uniform_U;
        const Real v_uniform = state.uniform_V;
        for (MFIter mfi(*state.vel0); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = state.vel0->array(mfi);
            ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
                const Real z_phys = z_lo + (k + Real(0.5)) * dz;
                const Real z_agl = z_phys - terrain_ptr[j * nx + i];
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
    } else {
        std::vector<Real> x_vel, y_vel, z_vel, ux_vel, uy_vel;
        if (state.velocity_file.size() > 4 && state.velocity_file.substr(state.velocity_file.find_last_of(".") + 1) == "csv") {
            read_vertical_profile_csv(state.velocity_file, x_vel, y_vel, z_vel, ux_vel, uy_vel);
        } else {
            read_velocity_file(state.velocity_file, x_vel, y_vel, z_vel, ux_vel, uy_vel);
        }

        std::vector<Real> vel_u_host(static_cast<std::size_t>(state.nx) * state.ny * state.nz);
        std::vector<Real> vel_v_host(static_cast<std::size_t>(state.nx) * state.ny * state.nz);
        for (int k = 0; k < state.nz; ++k) {
            const Real zc = z_lo + (k + Real(0.5)) * dz;
            const Real rmax = (k == 0) ? state.idw_rmax1 : state.idw_rmax2;
            const Real R_param = (k == 0) ? state.idw_r1 : state.idw_r2;
            for (int j = 0; j < state.ny; ++j) {
                const Real yc = state.ymin + (j + Real(0.5)) * state.dy;
                for (int i = 0; i < state.nx; ++i) {
                    const Real xc = state.xmin + (i + Real(0.5)) * state.dx;
                    Real d_min = std::numeric_limits<Real>::max();
                    bool any_station_within_rmax = false;
                    for (std::size_t s = 0; s < x_vel.size(); ++s) {
                        Real dx_s = x_vel[s] - xc;
                        Real dy_s = y_vel[s] - yc;
                        Real dist = std::sqrt(dx_s * dx_s + dy_s * dy_s);
                        if (rmax <= Real(0.0) || dist <= rmax) {
                            any_station_within_rmax = true;
                            if (dist < d_min) {
                                d_min = dist;
                            }
                        }
                    }

                    auto uv = idw_velocity_3d(xc, yc, zc, x_vel, y_vel, z_vel, ux_vel, uy_vel, 6,
                                              state.idw_gamma,
                                              state.enable_topographic_shielding,
                                              g_wind_solver_runtime->terrain_host,
                                              state.xmin, state.ymin,
                                              state.dx, state.dy,
                                              state.nx, state.ny,
                                              rmax, state.idw_exponent);
                    std::size_t idx = (static_cast<std::size_t>(k) * state.ny + j) * state.nx + i;
                    
                    Real u_final = uv.first;
                    Real v_final = uv.second;

                    if (R_param > Real(0.0)) {
                        Real speed_ref = std::sqrt(state.U_ref * state.U_ref + state.V_ref * state.V_ref);
                        Real u_bg = 0.0, v_bg = 0.0;
                        if (speed_ref > Real(1.0e-10)) {
                            Real z_agl = zc - g_wind_solver_runtime->terrain_host[j * state.nx + i];
                            if (z_agl > Real(0.0)) {
                                Real ustar_bg = speed_ref * 0.41 / std::log((state.z_ref + state.z0) / state.z0);
                                Real speed_bg = (ustar_bg / 0.41) * std::log((z_agl + state.z0) / state.z0);
                                u_bg = speed_bg * state.U_ref / speed_ref;
                                v_bg = speed_bg * state.V_ref / speed_ref;
                            }
                        }

                        if (!any_station_within_rmax) {
                            u_final = u_bg;
                            v_final = v_bg;
                        } else {
                            Real weight_bg = (d_min / R_param) * (d_min / R_param);
                            u_final = (uv.first + weight_bg * u_bg) / (Real(1.0) + weight_bg);
                            v_final = (uv.second + weight_bg * v_bg) / (Real(1.0) + weight_bg);
                        }
                    }
                    
                    vel_u_host[idx] = u_final;
                    vel_v_host[idx] = v_final;
                }
            }
        }

        Gpu::DeviceVector<Real> vel_u_dev(vel_u_host.size());
        Gpu::DeviceVector<Real> vel_v_dev(vel_v_host.size());
        Gpu::copy(Gpu::hostToDevice, vel_u_host.begin(), vel_u_host.end(), vel_u_dev.begin());
        Gpu::copy(Gpu::hostToDevice, vel_v_host.begin(), vel_v_host.end(), vel_v_dev.begin());
        const Real* vel_u_ptr = vel_u_dev.data();
        const Real* vel_v_ptr = vel_v_dev.data();
        const int nx_val = state.nx;
        const int ny_val = state.ny;

        for (MFIter mfi(*state.vel0); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = state.vel0->array(mfi);
            ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
                const Real z_phys = z_lo + (k + Real(0.5)) * dz;
                const Real z_agl = z_phys - terrain_ptr[j * nx_val + i];
                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else {
                    std::size_t idx = (static_cast<std::size_t>(k) * ny_val + j) * nx_val + i;
                    vel(i, j, k, 0) = vel_u_ptr[idx];
                    vel(i, j, k, 1) = vel_v_ptr[idx];
                    vel(i, j, k, 2) = Real(0.0);
                }
            });
        }
    }

    state.vel0->FillBoundary(state.geom->periodicity());

    if (state.enable_turbine_wake && !state.turbines.empty()) {
        TurbineWake::TurbineWakeModelType tw_model_type = TurbineWake::TurbineWakeModelType::JENSEN;
        if (state.turbine_wake_model_type == "bastankhah_gaussian" || state.turbine_wake_model_type == "gaussian") {
            tw_model_type = TurbineWake::TurbineWakeModelType::BASTANKHAH_GAUSSIAN;
        } else if (state.turbine_wake_model_type == "turbopark") {
            tw_model_type = TurbineWake::TurbineWakeModelType::TURBOPARK;
        } else if (state.turbine_wake_model_type == "gch" || state.turbine_wake_model_type == "gauss_curl_hybrid") {
            tw_model_type = TurbineWake::TurbineWakeModelType::GAUSS_CURL_HYBRID;
        }
        TurbineWake::SuperpositionType tw_superposition = TurbineWake::SuperpositionType::QUADRATIC;
        if (state.turbine_wake_superposition == "linear") {
            tw_superposition = TurbineWake::SuperpositionType::LINEAR;
        } else if (state.turbine_wake_superposition == "max") {
            tw_superposition = TurbineWake::SuperpositionType::MAX;
        }
        TurbineWake::WakeAddedTurbulenceModelType added_turb_model = TurbineWake::WakeAddedTurbulenceModelType::NONE;
        if (state.wake_added_turbulence_model == "crespo_hernandez") {
            added_turb_model = TurbineWake::WakeAddedTurbulenceModelType::CRESPO_HERNANDEZ;
        } else if (state.wake_added_turbulence_model == "frandsen" || state.wake_added_turbulence_model == "stf") {
            added_turb_model = TurbineWake::WakeAddedTurbulenceModelType::FRANDSEN;
        }
        
        TurbineWake::apply_turbine_wakes_to_multifab(
            *state.vel0,
            g_wind_solver_runtime->terrain_host,
            state.turbines,
            tw_model_type,
            tw_superposition,
            state.jensen_kw,
            state.gaussian_ka,
            state.enable_stability_correction,
            state.stability_length,
            state.xmin, state.ymin, state.zmin,
            state.dx, state.dy, state.dz,
            state.nx, state.ny, state.nz,
            state.turbopark_c1,
            state.ambient_ti,
            state.enable_jimenez_deflection,
            state.jimenez_kd,
            state.enable_bastankhah_deflection,
            added_turb_model,
            0, // time_step
            state.enable_wake_ground_interaction,
            state.wake_ground_damping_scale,
            state.surface_sensible_heat_flux,
            state.buoyant_wake_destruction_coeff
        );
    }

    MultiFab::Copy(*state.vel, *state.vel0, 0, 0, 3, 1);
    state.vel->FillBoundary(state.geom->periodicity());
}

void compute_divergence(const WindSolverState& state,
                        const MultiFab& velocity,
                        MultiFab& divergence)
{
    const IntVect lo = state.geom->Domain().smallEnd();
    const IntVect hi = state.geom->Domain().bigEnd();
    const int ilo = lo[0];
    const int ihi = hi[0];
    const int jlo = lo[1];
    const int jhi = hi[1];
    const int klo = lo[2];
    const int khi = hi[2];

    const Real inv1dx = Real(1.0) / state.dx;
    const Real inv1dy = Real(1.0) / state.dy;
    const Real inv1dz = Real(1.0) / state.dz;
    const Real inv2dx = Real(0.5) * inv1dx;
    const Real inv2dy = Real(0.5) * inv1dy;
    const Real inv2dz = Real(0.5) * inv1dz;
    const Real* terrain_ptr = g_wind_solver_runtime->terrain_device.data();
    const Real z_lo = state.zmin;
    const Real dz = state.dz;
    const int nx = state.nx;
    
    // Terrain-following coordinates parameters
    const bool use_terrain_following = state.enable_terrain_following;
    const Real decay_height = state.terrain_decay_height;
    const Real dx_val = state.dx;
    const Real dy_val = state.dy;

    divergence.setVal(0.0);
    for (MFIter mfi(divergence); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        const auto vel = velocity.const_array(mfi);
        auto div = divergence.array(mfi);
        ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
            const Real z_phys = z_lo + (k + Real(0.5)) * dz;
            const Real z_terrain = terrain_ptr[j * nx + i];
            const Real z_agl = z_phys - z_terrain;
            if (z_agl <= Real(0.0)) {
                div(i, j, k) = Real(0.0);
                return;
            }

            Real du = Real(0.0);
            Real dv = Real(0.0);
            Real dw = Real(0.0);

            if (ihi > ilo) {
                if (i == ilo) {
                    du = (vel(i + 1, j, k, 0) - vel(i, j, k, 0)) * inv1dx;
                } else if (i == ihi) {
                    du = (vel(i, j, k, 0) - vel(i - 1, j, k, 0)) * inv1dx;
                } else {
                    du = (vel(i + 1, j, k, 0) - vel(i - 1, j, k, 0)) * inv2dx;
                }
            }

            if (jhi > jlo) {
                if (j == jlo) {
                    dv = (vel(i, j + 1, k, 1) - vel(i, j, k, 1)) * inv1dy;
                } else if (j == jhi) {
                    dv = (vel(i, j, k, 1) - vel(i, j - 1, k, 1)) * inv1dy;
                } else {
                    dv = (vel(i, j + 1, k, 1) - vel(i, j - 1, k, 1)) * inv2dy;
                }
            }

            if (khi > klo) {
                if (k == klo) {
                    dw = (vel(i, j, k + 1, 2) - vel(i, j, k, 2)) * inv1dz;
                } else if (k == khi) {
                    dw = (vel(i, j, k, 2) - vel(i, j, k - 1, 2)) * inv1dz;
                } else {
                    dw = (vel(i, j, k + 1, 2) - vel(i, j, k - 1, 2)) * inv2dz;
                }
            }

            // Standard Cartesian divergence
            div(i, j, k) = du + dv + dw;
            
            // Add terrain-following coordinate metric corrections
            if (use_terrain_following) {
                // Compute vertical derivatives of u and v for metric corrections
                Real dudz = Real(0.0);
                Real dvdz = Real(0.0);
                
                if (khi > klo) {
                    if (k == klo) {
                        dudz = (vel(i, j, k + 1, 0) - vel(i, j, k, 0)) * inv1dz;
                        dvdz = (vel(i, j, k + 1, 1) - vel(i, j, k, 1)) * inv1dz;
                    } else if (k == khi) {
                        dudz = (vel(i, j, k, 0) - vel(i, j, k - 1, 0)) * inv1dz;
                        dvdz = (vel(i, j, k, 1) - vel(i, j, k - 1, 1)) * inv1dz;
                    } else {
                        dudz = (vel(i, j, k + 1, 0) - vel(i, j, k - 1, 0)) * inv2dz;
                        dvdz = (vel(i, j, k + 1, 1) - vel(i, j, k - 1, 1)) * inv2dz;
                    }
                }
                
                // Compute terrain slopes
                const Real dz_terrain_dx = TerrainFollowingCoords::compute_terrain_slope_x(
                    i, j, terrain_ptr, nx, dx_val, ilo, ihi);
                const Real dz_terrain_dy = TerrainFollowingCoords::compute_terrain_slope_y(
                    i, j, terrain_ptr, nx, dy_val, jlo, jhi);
                
                // Add metric correction to divergence
                const Real w = vel(i, j, k, 2);
                const Real correction = TerrainFollowingCoords::divergence_metric_correction(
                    dudz, dvdz, w, dw, dz_terrain_dx, dz_terrain_dy,
                    z_terrain, z_agl, decay_height);
                
                div(i, j, k) += correction;
            }
        });
    }
}

void apply_heat_source_forcing(WindSolverState& state)
{
    // Apply heat source as vertical velocity perturbation (simplified buoyancy forcing)
    // Heat source creates updrafts at surface that decay with height
    if (!state.has_heat_source || !state.heat_source) {
        return;
    }
    
    amrex::Print() << "Applying heat source forcing to velocity field...\n";
    
    const Real dz = state.dz;
    const Real decay_height = std::max(100.0, 5.0 * dz);  // Decay over ~5 grid cells
    const Real g = 9.81;
    const Real T_ref = 300.0;
    const Real rho = 1.2;  // Air density [kg/m³]
    const Real cp = 1005.0;  // Specific heat [J/(kg·K)]
    const Real z_lo = state.zmin;
    const int nx = state.nx;
    
    const Real* terrain_ptr = (g_wind_solver_runtime && !g_wind_solver_runtime->terrain_device.empty()) ? 
                              g_wind_solver_runtime->terrain_device.data() : nullptr;
    
    for (MFIter mfi(*state.vel); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        auto vel_arr = state.vel->array(mfi);
        const auto& hs_arr = state.heat_source->const_array(mfi);
        
        ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
            // Get surface heat flux at this horizontal location [W/m²]
            Real heat_flux = hs_arr(i, j, k);
            
            if (std::abs(heat_flux) > 1.0e-10) {
                // Compute height above surface (terrain-aware)
                Real z_above_surface;
                if (terrain_ptr) {
                    const Real z_phys = z_lo + (k + Real(0.5)) * dz;
                    const Real z_terrain = terrain_ptr[j * nx + i];
                    const Real z_agl = z_phys - z_terrain;
                    if (z_agl <= Real(0.0)) {
                        return; // Inside or below terrain
                    }
                    z_above_surface = z_agl;
                } else {
                    z_above_surface = (k + Real(0.5)) * dz;
                }
                
                // Convert heat flux to virtual temperature perturbation [K]
                // Q = rho * cp * dT  =>  dT = Q / (rho * cp)
                Real dT = heat_flux / (rho * cp);
                
                // Vertical velocity induced by buoyancy: w ~ sqrt(2 * g * dT / T_ref * z)
                // But apply as a perturbation that decays with height
                Real decay = std::exp(-z_above_surface / decay_height);
                
                // Add vertical velocity perturbation (positive updraft)
                Real w_pert = std::sqrt(std::max(0.0, 2.0 * g * (dT / T_ref) * z_above_surface)) * decay;
                vel_arr(i, j, k, 2) += w_pert;  // Component 2 is w
            }
        });
    }
    
    amrex::Print() << "Heat source forcing applied.\n";
}

void correct_velocity_field(WindSolverState& state)
{
    const IntVect lo = state.geom->Domain().smallEnd();
    const IntVect hi = state.geom->Domain().bigEnd();
    const int ilo = lo[0];
    const int ihi = hi[0];
    const int jlo = lo[1];
    const int jhi = hi[1];
    const int klo = lo[2];
    const int khi = hi[2];

    const Real inv1dx = Real(1.0) / state.dx;
    const Real inv1dy = Real(1.0) / state.dy;
    const Real inv1dz = Real(1.0) / state.dz;
    const Real inv2dx = Real(0.5) * inv1dx;
    const Real inv2dy = Real(0.5) * inv1dy;
    const Real inv2dz = Real(0.5) * inv1dz;
    const Real bh = state.alpha_h * state.alpha_h;
    const Real bv = state.alpha_v * state.alpha_v;
    const Real* terrain_ptr = g_wind_solver_runtime->terrain_device.data();
    const Real z_lo = state.zmin;
    const Real dz = state.dz;
    const int nx = state.nx;
    
    // Terrain-following coordinates parameters
    const bool use_terrain_following = state.enable_terrain_following;
    const Real decay_height = state.terrain_decay_height;
    const Real dx_val = state.dx;
    const Real dy_val = state.dy;

    state.vel->setVal(0.0);
    for (MFIter mfi(*state.vel); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        const auto v0 = state.vel0->const_array(mfi);
        const auto lam = state.lambda->const_array(mfi);
        auto vel = state.vel->array(mfi);
        const auto ah_arr = state.alpha_h_field->const_array(mfi);
        const auto av_arr = state.alpha_v_field->const_array(mfi);
        const bool use_spatial = state.enable_cell_local_anisotropy;
        ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
            const Real z_phys = z_lo + (k + Real(0.5)) * dz;
            const Real z_terrain = terrain_ptr[j * nx + i];
            const Real z_agl = z_phys - z_terrain;
            if (z_agl <= Real(0.0)) {
                vel(i, j, k, 0) = Real(0.0);
                vel(i, j, k, 1) = Real(0.0);
                vel(i, j, k, 2) = Real(0.0);
                return;
            }

            Real dlx = Real(0.0);
            Real dly = Real(0.0);
            Real dlz = Real(0.0);

            if (ihi > ilo) {
                if (i == ilo) {
                    dlx = (lam(i + 1, j, k) - lam(i, j, k)) * inv1dx;
                } else if (i == ihi) {
                    dlx = (lam(i, j, k) - lam(i - 1, j, k)) * inv1dx;
                } else {
                    dlx = (lam(i + 1, j, k) - lam(i - 1, j, k)) * inv2dx;
                }
            }

            if (jhi > jlo) {
                if (j == jlo) {
                    dly = (lam(i, j + 1, k) - lam(i, j, k)) * inv1dy;
                } else if (j == jhi) {
                    dly = (lam(i, j, k) - lam(i, j - 1, k)) * inv1dy;
                } else {
                    dly = (lam(i, j + 1, k) - lam(i, j - 1, k)) * inv2dy;
                }
            }

            if (khi > klo) {
                if (k == klo) {
                    dlz = (lam(i, j, k + 1) - lam(i, j, k)) * inv1dz;
                } else if (k == khi) {
                    dlz = (lam(i, j, k) - lam(i, j, k - 1)) * inv1dz;
                } else {
                    dlz = (lam(i, j, k + 1) - lam(i, j, k - 1)) * inv2dz;
                }
            }

            Real local_bh = bh;
            Real local_bv = bv;
            if (use_spatial) {
                Real local_ah = ah_arr(i, j, k);
                Real local_av = av_arr(i, j, k);
                local_bh = local_ah * local_ah;
                local_bv = local_av * local_av;
            }

            // Standard velocity correction
            vel(i, j, k, 0) = v0(i, j, k, 0) - local_bh * dlx;
            vel(i, j, k, 1) = v0(i, j, k, 1) - local_bh * dly;
            vel(i, j, k, 2) = v0(i, j, k, 2) - local_bv * dlz;
            
            // Apply terrain-following coordinate corrections
            if (use_terrain_following) {
                // In terrain-following coords, the velocity correction includes
                // metric terms from the coordinate transformation
                // Additional correction: w' = w - (∂s/∂x * ∂λ/∂x + ∂s/∂y * ∂λ/∂y)
                const Real dz_terrain_dx = TerrainFollowingCoords::compute_terrain_slope_x(
                    i, j, terrain_ptr, nx, dx_val, ilo, ihi);
                const Real dz_terrain_dy = TerrainFollowingCoords::compute_terrain_slope_y(
                    i, j, terrain_ptr, nx, dy_val, jlo, jhi);
                
                const Real dsdx = TerrainFollowingCoords::metric_dsdx(
                    dz_terrain_dx, z_agl, decay_height);
                const Real dsdy = TerrainFollowingCoords::metric_dsdy(
                    dz_terrain_dy, z_agl, decay_height);
                
                // Modify vertical velocity with horizontal metric terms
                vel(i, j, k, 2) -= local_bh * (dsdx * dlx + dsdy * dly);
                
                // Scale vertical correction by Jacobian
                const Real J = TerrainFollowingCoords::jacobian(
                    z_terrain, z_agl, decay_height);
                vel(i, j, k, 2) = v0(i, j, k, 2) + (vel(i, j, k, 2) - v0(i, j, k, 2)) * J;
            }
        });
    }
    state.vel->FillBoundary(state.geom->periodicity());
}

std::vector<double> extract_multifab_component_fortran(const MultiFab& mf,
                                                       int comp,
                                                       int nx,
                                                       int ny,
                                                       int nz)
{
    std::vector<double> data(static_cast<std::size_t>(nx) * ny * nz, 0.0);

    for (MFIter mfi(mf, false); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
#ifdef AMREX_USE_GPU
        FArrayBox host_fab(bx, mf.nComp(), The_Pinned_Arena());
        host_fab.copy<RunOn::Device>(mf[mfi], bx);
        Gpu::streamSynchronize();
        auto const& arr = host_fab.const_array();
#else
        auto const& arr = mf.const_array(mfi);
#endif
        for (int k = bx.smallEnd(2); k <= bx.bigEnd(2); ++k) {
            for (int j = bx.smallEnd(1); j <= bx.bigEnd(1); ++j) {
                for (int i = bx.smallEnd(0); i <= bx.bigEnd(0); ++i) {
                    const std::size_t idx = static_cast<std::size_t>(i)
                                          + static_cast<std::size_t>(nx)
                                                * (static_cast<std::size_t>(j)
                                                   + static_cast<std::size_t>(ny) * k);
                    data[idx] = static_cast<double>(arr(i, j, k, comp));
                }
            }
        }
    }

    return data;
}

int agl_to_k(const WindSolverState& state, Real terrain_z, Real agl_height)
{
    const Real target_z = terrain_z + agl_height;
    const Real k_real = (target_z - state.zmin) / state.dz - Real(0.5);
    const int k_index = static_cast<int>(std::llround(k_real));
    return std::max(0, std::min(state.nz - 1, k_index));
}

void build_plotfile_output(MultiFab& output, MultiFab& div_current)
{
    WindSolverState& state = *g_wind_solver_state;
    const Real* terrain_ptr = g_wind_solver_runtime->terrain_device.data();
    const int nx = state.nx;

    for (MFIter mfi(output); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        const auto vel = state.vel->const_array(mfi);
        const auto vel0 = state.vel0->const_array(mfi);
        const auto lambda = state.lambda->const_array(mfi);
        const auto div0 = state.div0->const_array(mfi);
        const auto divc = div_current.const_array(mfi);
        auto out = output.array(mfi);

        ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
            const Real u = vel(i, j, k, 0);
            const Real v = vel(i, j, k, 1);
            const Real w = vel(i, j, k, 2);
            out(i, j, k, 0) = u;
            out(i, j, k, 1) = v;
            out(i, j, k, 2) = w;
            out(i, j, k, 3) = std::sqrt(u * u + v * v + w * w);
            out(i, j, k, 4) = vel0(i, j, k, 0);
            out(i, j, k, 5) = vel0(i, j, k, 1);
            out(i, j, k, 6) = vel0(i, j, k, 2);
            out(i, j, k, 7) = lambda(i, j, k);
            out(i, j, k, 8) = div0(i, j, k);
            out(i, j, k, 9) = divc(i, j, k);
            out(i, j, k, 10) = terrain_ptr[j * nx + i];
        });
    }
}

} // namespace

bool wind_solver_initialize(const std::string& inputs_file)
{
    ensure_amrex_initialized();

    if (g_wind_solver_state && g_wind_solver_state->initialized) {
        wind_solver_finalize();
        ensure_amrex_initialized();
    }

    g_wind_solver_state = std::make_unique<WindSolverState>();
    WindSolverState& state = *g_wind_solver_state;

    try {
        parse_inputs(state, inputs_file);
        initialize_wind_field(state);

        state.initialized = true;
        state.solved = false;

        amrex::Print() << "Wind solver initialized successfully\n";
        amrex::Print() << "  Grid: " << state.nx << " x " << state.ny << " x " << state.nz << "\n";
        amrex::Print() << "  Domain: [" << state.xmin << ", " << state.xmax << "] x ["
                       << state.ymin << ", " << state.ymax << "] x ["
                       << state.zmin << ", " << state.zmax << "]\n";
        return true;
    } catch (const std::exception& e) {
        amrex::Print() << "Error initializing wind solver: " << e.what() << "\n";
        g_wind_solver_runtime.reset();
        g_wind_solver_state.reset();
        return false;
    }
}

// ============================================================================
// apply_turbulent_stress_api
//
// Implements the double-pass turbulent stress correction for the Python API
// path.  Called from wind_solver_solve() when state.enable_turbulent_stress
// is true.
//
// On entry  : state.vel holds u* (first-pass mass-corrected velocity)
// On exit   : state.vel holds u_final (second-pass mass-corrected velocity)
// ============================================================================
void apply_turbulent_stress_api(WindSolverState& state)
{
    using namespace amrex;

    amrex::Print() << "wind_solver: ---- Turbulent Stress Second Pass (API) ----\n";

    const IntVect lo = state.geom->Domain().smallEnd();
    const IntVect hi = state.geom->Domain().bigEnd();
    const int ilo = lo[0], ihi = hi[0];
    const int jlo = lo[1], jhi = hi[1];
    const int klo = lo[2], khi = hi[2];

    const Real dx_val = state.dx;
    const Real dy_val = state.dy;
    const Real dz_val = state.dz;
    const Real z_lo   = state.zmin;
    const int  nx_val = state.nx;
    const int  ny_val = state.ny;
    const int  nz_val = state.nz;

    const Real kappa_cap = state.von_karman;
    const Real coeff_cap = state.turbulent_stress_mixing_length_coefficient;
    const Real z0_cap    = state.zground;

    const Real inv_sc_h = (state.turbulent_schmidt_number_horizontal > Real(0.0))
                          ? Real(1.0) / state.turbulent_schmidt_number_horizontal : Real(1.0);
    const Real inv_sc_v = (state.turbulent_schmidt_number_vertical > Real(0.0))
                          ? Real(1.0) / state.turbulent_schmidt_number_vertical : Real(1.0);

    const Real inv_dx2 = Real(1.0) / (dx_val * dx_val);
    const Real inv_dy2 = Real(1.0) / (dy_val * dy_val);
    const Real inv_dz2 = Real(1.0) / (dz_val * dz_val);
    const Real inv1dx  = Real(1.0) / dx_val;
    const Real inv1dy  = Real(1.0) / dy_val;
    const Real inv1dz  = Real(1.0) / dz_val;
    const Real inv2dx  = Real(0.5) * inv1dx;
    const Real inv2dy  = Real(0.5) * inv1dy;
    const Real inv2dz  = Real(0.5) * inv1dz;

    const Real* terrain_ptr = g_wind_solver_runtime->terrain_device.data();

    // ------------------------------------------------------------------
    // Step 1: Build cell-centred eddy viscosity ν_t
    // ------------------------------------------------------------------
    MultiFab nut(*state.ba, *state.dm, 1, 1);
    nut.setVal(Real(0.0));

    for (MFIter mfi(nut); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.growntilebox(1);
        auto nut_arr = nut.array(mfi);
        const auto vel_arr = state.vel->const_array(mfi);

        ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
            int ii = amrex::max(0, amrex::min(i, nx_val - 1));
            int jj = amrex::max(0, amrex::min(j, ny_val - 1));
            Real z_phys = z_lo + (Real(k) + Real(0.5)) * dz_val;
            Real z_agl  = z_phys - terrain_ptr[jj * nx_val + ii];

            Real inv_dz = (dz_val > Real(0.0)) ? Real(1.0) / dz_val : Real(0.0);
            Real shear  = TurbulentStress::compute_velocity_gradient_magnitude(
                              i, j, k, vel_arr, inv_dz, nz_val);
            nut_arr(i, j, k) = TurbulentStress::compute_mixing_length_nut(
                                    z_agl, z0_cap, kappa_cap, coeff_cap, shear);
        });
    }
    nut.FillBoundary(state.geom->periodicity());

    // ------------------------------------------------------------------
    // Step 2: Compute Δu = div(ν_t ∇u) and add to state.vel (→ u†)
    // ------------------------------------------------------------------
    MultiFab vel_increment(*state.ba, *state.dm, 3, 0);
    vel_increment.setVal(Real(0.0));

    for (MFIter mfi(vel_increment); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        auto delta_arr     = vel_increment.array(mfi);
        const auto vel_arr = state.vel->const_array(mfi);
        const auto nut_arr = nut.const_array(mfi);

        ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
            Real z_phys = z_lo + (Real(k) + Real(0.5)) * dz_val;
            int  ii = amrex::max(0, amrex::min(i, nx_val - 1));
            int  jj = amrex::max(0, amrex::min(j, ny_val - 1));
            Real z_agl  = z_phys - terrain_ptr[jj * nx_val + ii];
            if (z_agl <= Real(0.0)) {
                delta_arr(i, j, k, 0) = Real(0.0);
                delta_arr(i, j, k, 1) = Real(0.0);
                delta_arr(i, j, k, 2) = Real(0.0);
                return;
            }
            for (int comp = 0; comp < 3; ++comp) {
                delta_arr(i, j, k, comp) =
                    TurbulentStress::apply_turbulent_stress_term(
                        i, j, k, comp,
                        nut_arr, vel_arr,
                        ilo, ihi, jlo, jhi, klo, khi,
                        inv_dx2, inv_dy2, inv_dz2,
                        inv_sc_h, inv_sc_v);
            }
        });
    }

    MultiFab::Add(*state.vel, vel_increment, 0, 0, 3, 0);
    state.vel->FillBoundary(state.geom->periodicity());

    // ------------------------------------------------------------------
    // Step 3: Recompute RHS = −∇·u†
    // Note: The API path uses second-order central differences throughout,
    // consistent with correct_velocity_field() in this file.  The standalone
    // WindSolverApp path additionally supports WENO3/WENO5 via deriv_method.
    // ------------------------------------------------------------------
    MultiFab rhs2(*state.ba, *state.dm, 1, 0);
    rhs2.setVal(Real(0.0));

    for (MFIter mfi(rhs2); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        const auto vel = state.vel->const_array(mfi);
        auto rh = rhs2.array(mfi);

        ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
            Real z_phys = z_lo + (Real(k) + Real(0.5)) * dz_val;
            int  ii = amrex::max(0, amrex::min(i, nx_val - 1));
            int  jj = amrex::max(0, amrex::min(j, ny_val - 1));
            Real z_agl  = z_phys - terrain_ptr[jj * nx_val + ii];
            if (z_agl <= Real(0.0)) { rh(i, j, k) = Real(0.0); return; }

            Real du = (i == ilo) ? (vel(i+1,j,k,0) - vel(i,j,k,0)) * inv1dx
                    : (i == ihi) ? (vel(i,j,k,0) - vel(i-1,j,k,0)) * inv1dx
                    :              (vel(i+1,j,k,0) - vel(i-1,j,k,0)) * inv2dx;
            Real dv = (j == jlo) ? (vel(i,j+1,k,1) - vel(i,j,k,1)) * inv1dy
                    : (j == jhi) ? (vel(i,j,k,1) - vel(i,j-1,k,1)) * inv1dy
                    :              (vel(i,j+1,k,1) - vel(i,j-1,k,1)) * inv2dy;
            Real dw = (k == klo) ? (vel(i,j,k+1,2) - vel(i,j,k,2)) * inv1dz
                    : (k == khi) ? (vel(i,j,k,2) - vel(i,j,k-1,2)) * inv1dz
                    :              (vel(i,j,k+1,2) - vel(i,j,k-1,2)) * inv2dz;
            rh(i, j, k) = -(du + dv + dw);
        });
    }

    // ------------------------------------------------------------------
    // Step 4: Second Poisson solve  ∇·(A²∇λ₂) = −∇·u†
    // ------------------------------------------------------------------
    LPInfo info2;
    info2.setAgglomeration(true);
    info2.setConsolidation(true);

    MLABecLaplacian mlabec2({*state.geom}, {*state.ba}, {*state.dm}, info2);
    mlabec2.setMaxOrder(2);

    Array<LinOpBCType, AMREX_SPACEDIM> lo_bc2, hi_bc2;
    lo_bc2[0] = LinOpBCType::Dirichlet; hi_bc2[0] = LinOpBCType::Dirichlet;
    lo_bc2[1] = LinOpBCType::Neumann;   hi_bc2[1] = LinOpBCType::Neumann;
    lo_bc2[2] = LinOpBCType::Neumann;   hi_bc2[2] = LinOpBCType::Neumann;
    mlabec2.setDomainBC(lo_bc2, hi_bc2);
    mlabec2.setScalars(Real(0.0), Real(1.0));

    MultiFab acoef2(*state.ba, *state.dm, 1, 0);
    acoef2.setVal(Real(0.0));
    mlabec2.setACoeffs(0, acoef2);

    const Real bh = state.alpha_h * state.alpha_h;
    const Real bv = state.alpha_v * state.alpha_v;
    Array<MultiFab, AMREX_SPACEDIM> bcoef2;
    bcoef2[0].define(convert(*state.ba, IntVect(1, 0, 0)), *state.dm, 1, 0);
    bcoef2[1].define(convert(*state.ba, IntVect(0, 1, 0)), *state.dm, 1, 0);
    bcoef2[2].define(convert(*state.ba, IntVect(0, 0, 1)), *state.dm, 1, 0);
    bcoef2[0].setVal(bh);
    bcoef2[1].setVal(bh);
    bcoef2[2].setVal(bv);
    mlabec2.setBCoeffs(0, GetArrOfConstPtrs(bcoef2));
    mlabec2.setLevelBC(0, nullptr);

    state.lambda->setVal(Real(0.0));
    MLMG mlmg2(mlabec2);
    mlmg2.setMaxIter(state.max_iter);
    mlmg2.setMaxFmgIter(20);
    mlmg2.setVerbose(state.mlmg_verbose);
    mlmg2.setBottomVerbose(0);
    mlmg2.setPreSmooth(16);
    mlmg2.setPostSmooth(16);

    amrex::Print() << "wind_solver: starting second MLMG solve (turbulent stress, API)...\n";
    mlmg2.solve({state.lambda.get()}, {&rhs2}, state.tol_rel, state.tol_abs);
    amrex::Print() << "wind_solver: second MLMG solve complete (API).\n";
    state.lambda->FillBoundary(state.geom->periodicity());

    // ------------------------------------------------------------------
    // Step 5: Apply second correction  u_final = u† − A²∇λ₂
    // Uses second-order central differences consistent with the first
    // correction in correct_velocity_field().
    // ------------------------------------------------------------------
    const bool use_spatial = state.enable_cell_local_anisotropy;
    for (MFIter mfi(*state.vel); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        const auto lam = state.lambda->const_array(mfi);
        auto vel = state.vel->array(mfi);
        const auto ah_arr = state.alpha_h_field->const_array(mfi);
        const auto av_arr = state.alpha_v_field->const_array(mfi);

        ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
            Real z_phys = z_lo + (Real(k) + Real(0.5)) * dz_val;
            int  ii = amrex::max(0, amrex::min(i, nx_val - 1));
            int  jj = amrex::max(0, amrex::min(j, ny_val - 1));
            Real z_agl  = z_phys - terrain_ptr[jj * nx_val + ii];
            if (z_agl <= Real(0.0)) {
                vel(i, j, k, 0) = Real(0.0);
                vel(i, j, k, 1) = Real(0.0);
                vel(i, j, k, 2) = Real(0.0);
                return;
            }

            Real dlx = (i == ilo) ? (lam(i+1,j,k) - lam(i,j,k))   * inv1dx
                     : (i == ihi) ? (lam(i,j,k)   - lam(i-1,j,k)) * inv1dx
                     :              (lam(i+1,j,k) - lam(i-1,j,k))  * inv2dx;
            Real dly = (j == jlo) ? (lam(i,j+1,k) - lam(i,j,k))   * inv1dy
                     : (j == jhi) ? (lam(i,j,k)   - lam(i,j-1,k)) * inv1dy
                     :              (lam(i,j+1,k) - lam(i,j-1,k))  * inv2dy;
            Real dlz = (k == klo) ? (lam(i,j,k+1) - lam(i,j,k))   * inv1dz
                     : (k == khi) ? (lam(i,j,k)   - lam(i,j,k-1)) * inv1dz
                     :              (lam(i,j,k+1) - lam(i,j,k-1))  * inv2dz;

            Real local_bh = bh;
            Real local_bv = bv;
            if (use_spatial) {
                Real ah = ah_arr(i, j, k);
                Real av = av_arr(i, j, k);
                local_bh = ah * ah;
                local_bv = av * av;
            }
            vel(i, j, k, 0) -= local_bh * dlx;
            vel(i, j, k, 1) -= local_bh * dly;
            vel(i, j, k, 2) -= local_bv * dlz;
        });
    }

    amrex::Print() << "wind_solver: ---- Turbulent Stress Second Pass Complete (API) ----\n";
}

bool wind_solver_solve()
{
    try {
        require_initialized();
        WindSolverState& state = *g_wind_solver_state;

        state.vel0->FillBoundary(state.geom->periodicity());
        compute_divergence(state, *state.vel0, *state.div0);

        MultiFab rhs(*state.ba, *state.dm, 1, 0);
        MultiFab::Copy(rhs, *state.div0, 0, 0, 1, 0);
        rhs.mult(Real(-1.0), 0, 1, 0);

        LPInfo info;
        info.setAgglomeration(true);
        info.setConsolidation(true);

        MLABecLaplacian mlabec({*state.geom}, {*state.ba}, {*state.dm}, info);
        mlabec.setMaxOrder(2);

        Array<LinOpBCType, AMREX_SPACEDIM> lo_bc, hi_bc;
        lo_bc[0] = LinOpBCType::Dirichlet;
        hi_bc[0] = LinOpBCType::Dirichlet;
        lo_bc[1] = LinOpBCType::Neumann;
        hi_bc[1] = LinOpBCType::Neumann;
        lo_bc[2] = LinOpBCType::Neumann;
        hi_bc[2] = LinOpBCType::Neumann;
        mlabec.setDomainBC(lo_bc, hi_bc);
        mlabec.setScalars(0.0, 1.0);

        MultiFab acoef(*state.ba, *state.dm, 1, 0);
        acoef.setVal(0.0);
        mlabec.setACoeffs(0, acoef);

        // Initialize temperature and compute cell-local anisotropy fields if enabled
        MultiFab temp(*state.ba, *state.dm, 1, 0);
        {
            const Real T_ref = 300.0;
            const Real grad = state.temperature_gradient;
            const Real* terrain_ptr = g_wind_solver_runtime->terrain_device.data();
            const Real z_lo = state.zmin;
            const Real dz_val = state.dz;
            const int nx_val = state.nx;
            
            for (MFIter mfi(temp); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                auto t_arr = temp.array(mfi);
                ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
                    const Real z_phys = z_lo + (k + Real(0.5)) * dz_val;
                    const Real z_terrain = terrain_ptr[j * nx_val + i];
                    const Real z_agl = z_phys - z_terrain;
                    t_arr(i, j, k) = T_ref + grad * z_agl;
                });
            }
        }

        if (state.enable_cell_local_anisotropy && std::abs(state.temperature_gradient) < 1.0e-10) {
            amrex::Print() << "wind_solver API: WARNING: Cell-local anisotropy is enabled but no temperature gradient/profile is provided. Disabling cell-local anisotropy.\n";
            state.enable_cell_local_anisotropy = false;
        }

        CellLocalAnisotropy::compute_cell_local_anisotropy_fields(
            *state.alpha_h_field,
            *state.alpha_v_field,
            *state.vel0,
            temp,
            g_wind_solver_runtime->terrain_device.data(),
            state.nx, state.ny, state.nz,
            state.dx, state.dy, state.dz,
            state.alpha_h,
            state.alpha_v,
            state.zmin,
            state.enable_cell_local_anisotropy,
            state.anisotropy_source,
            state.anisotropy_slope_scale,
            state.anisotropy_decay_height,
            state.anisotropy_ri_gamma,
            state.anisotropy_ri_beta,
            state.anisotropy_fr_min
        );
        state.alpha_h_field->FillBoundary(state.geom->periodicity());
        state.alpha_v_field->FillBoundary(state.geom->periodicity());

        Array<MultiFab, AMREX_SPACEDIM> bcoef;
        bcoef[0].define(convert(*state.ba, IntVect(1, 0, 0)), *state.dm, 1, 0);
        bcoef[1].define(convert(*state.ba, IntVect(0, 1, 0)), *state.dm, 1, 0);
        bcoef[2].define(convert(*state.ba, IntVect(0, 0, 1)), *state.dm, 1, 0);

        if (state.enable_cell_local_anisotropy) {
            for (MFIter mfi(bcoef[0]); mfi.isValid(); ++mfi) {
                const Box& bx_x = mfi.validbox();
                const Box& bx_y = convert(mfi.validbox(), IntVect(0, 1, 0));
                const Box& bx_z = convert(mfi.validbox(), IntVect(0, 0, 1));
                
                auto bx_arr = bcoef[0].array(mfi);
                auto by_arr = bcoef[1].array(mfi);
                auto bz_arr = bcoef[2].array(mfi);
                auto ah_arr = state.alpha_h_field->const_array(mfi);
                auto av_arr = state.alpha_v_field->const_array(mfi);
                
                const int nx_val = state.nx;
                const int ny_val = state.ny;
                const int nz_val = state.nz;
                
                ParallelFor(bx_x, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
                    int left_idx = std::max(0, i - 1);
                    int right_idx = std::min(nx_val - 1, i);
                    Real ah_left = ah_arr(left_idx, j, k);
                    Real ah_right = ah_arr(right_idx, j, k);
                    Real ah_avg = Real(0.5) * (ah_left + ah_right);
                    bx_arr(i, j, k) = ah_avg * ah_avg;
                });
                
                ParallelFor(bx_y, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
                    int bottom_idx = std::max(0, j - 1);
                    int top_idx = std::min(ny_val - 1, j);
                    Real ah_bottom = ah_arr(i, bottom_idx, k);
                    Real ah_top = ah_arr(i, top_idx, k);
                    Real ah_avg = Real(0.5) * (ah_bottom + ah_top);
                    by_arr(i, j, k) = ah_avg * ah_avg;
                });
                
                ParallelFor(bx_z, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
                    int below_idx = std::max(0, k - 1);
                    int above_idx = std::min(nz_val - 1, k);
                    Real av_below = av_arr(i, j, below_idx);
                    Real av_above = av_arr(i, j, above_idx);
                    Real av_avg = Real(0.5) * (av_below + av_above);
                    bz_arr(i, j, k) = av_avg * av_avg;
                });
            }
        } else {
            const Real bh = state.alpha_h * state.alpha_h;
            const Real bv = state.alpha_v * state.alpha_v;
            bcoef[0].setVal(bh);
            bcoef[1].setVal(bh);
            bcoef[2].setVal(bv);
        }
        
        // Apply terrain-following coordinate metric corrections to B coefficients
        if (state.enable_terrain_following) {
            const Real* terrain_ptr = g_wind_solver_runtime->terrain_device.data();
            const Real z_lo = state.zmin;
            const Real dz_val = state.dz;
            const int nx_val = state.nx;
            const Real decay_height = state.terrain_decay_height;
            
            // Modify vertical B coefficient (bcoef[2]) to include Jacobian
            for (MFIter mfi(bcoef[2]); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                auto bz = bcoef[2].array(mfi);
                ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
                    // k is at face (z-face), so evaluate at k - 0.5 for cell center
                    const Real z_phys = z_lo + k * dz_val;
                    const Real z_terrain = terrain_ptr[j * nx_val + i];
                    const Real z_agl = z_phys - z_terrain;
                    
                    if (z_agl > Real(0.0)) {
                        // Compute Jacobian at this location
                        const Real J = TerrainFollowingCoords::jacobian(
                            z_terrain, z_agl, decay_height);
                        // Scale vertical B coefficient by Jacobian squared
                        // This accounts for metric tensor in terrain-following coords
                        bz(i, j, k) *= J * J;
                    }
                });
            }
        }
        
        mlabec.setBCoeffs(0, GetArrOfConstPtrs(bcoef));
        mlabec.setLevelBC(0, nullptr);

        MLMG mlmg(mlabec);
        mlmg.setMaxIter(state.max_iter);
        mlmg.setMaxFmgIter(20);
        mlmg.setVerbose(state.mlmg_verbose);
        mlmg.setBottomVerbose(0);
        mlmg.setPreSmooth(16);
        mlmg.setPostSmooth(16);

        state.lambda->setVal(0.0);
        mlmg.solve({state.lambda.get()}, {&rhs}, state.tol_rel, state.tol_abs);
        state.lambda->FillBoundary(state.geom->periodicity());

        correct_velocity_field(state);

        if (state.enable_turbulent_stress) {
            apply_turbulent_stress_api(state);
        }

        MultiFab div_corrected(*state.ba, *state.dm, 1, 0);
        state.vel->FillBoundary(state.geom->periodicity());
        compute_divergence(state, *state.vel, div_corrected);

        if (state.enable_wire_loading && !state.wires.empty()) {
            WireLoading::process_wire_loading(
                state.wires,
                *state.vel,
                nullptr,
                293.15,
                400.0,
                state.xmin, state.ymin, state.zmin,
                state.dx, state.dy, state.dz,
                state.nx, state.ny, state.nz
            );
            WireLoading::write_wire_output_file(state.wire_output_file, state.wires, 0);
        }

        state.solved = true;
        state.mlmg_iters = mlmg.getNumIters();
        state.mlmg_res = mlmg.getFinalResidual();

        // Apply heat source forcing if present (for fire coupling)
        if (state.has_heat_source) {
            apply_heat_source_forcing(state);
        }

        amrex::Print() << "wind_solver: max |div(u0)| = " << state.div0->norm0() << "\n";
        amrex::Print() << "wind_solver: max |div(u)|  = " << div_corrected.norm0() << "\n";
        amrex::Print() << "wind_solver: MLMG iters=" << state.mlmg_iters
                       << " residual=" << state.mlmg_res << "\n";
        
        // Clear heat source after apply (one-time use per solve)
        wind_solver_clear_heat_source();
        
        return true;
    } catch (const std::exception& e) {
        amrex::Print() << "Error solving wind field: " << e.what() << "\n";
        return false;
    }
}

void wind_solver_get_status(bool& solved, int& iters, double& residual)
{
    if (!g_wind_solver_state || !g_wind_solver_state->initialized) {
        solved = false;
        iters = 0;
        residual = 0.0;
        return;
    }

    solved = g_wind_solver_state->solved;
    iters = g_wind_solver_state->mlmg_iters;
    residual = static_cast<double>(g_wind_solver_state->mlmg_res);
}

void wind_solver_get_geometry(int& nx, int& ny, int& nz,
                              double& xmin, double& xmax,
                              double& ymin, double& ymax,
                              double& zmin, double& zmax,
                              double& dx, double& dy, double& dz)
{
    if (!g_wind_solver_state || !g_wind_solver_state->initialized) {
        nx = ny = nz = 0;
        xmin = xmax = ymin = ymax = zmin = zmax = dx = dy = dz = 0.0;
        return;
    }

    const WindSolverState& state = *g_wind_solver_state;
    nx = state.nx;
    ny = state.ny;
    nz = state.nz;
    xmin = state.xmin;
    xmax = state.xmax;
    ymin = state.ymin;
    ymax = state.ymax;
    zmin = state.zmin;
    zmax = state.zmax;
    dx = state.dx;
    dy = state.dy;
    dz = state.dz;
}

void wind_solver_get_terrain_bounds(double& zs_min, double& zs_max)
{
    if (!g_wind_solver_state || !g_wind_solver_state->initialized) {
        zs_min = 0.0;
        zs_max = 0.0;
        return;
    }

    zs_min = static_cast<double>(g_wind_solver_state->zs_min);
    zs_max = static_cast<double>(g_wind_solver_state->zs_max);
}

bool wind_solver_update_reference_wind(double U_ref, double V_ref)
{
    try {
        require_initialized();
        WindSolverState& state = *g_wind_solver_state;
        state.U_ref = static_cast<Real>(U_ref);
        state.V_ref = static_cast<Real>(V_ref);
        if (state.init_mode == "uniform") {
            state.uniform_U = state.U_ref;
            state.uniform_V = state.V_ref;
        }
        initialize_wind_field(state);
        return true;
    } catch (const std::exception& e) {
        amrex::Print() << "Error updating reference wind: " << e.what() << "\n";
        return false;
    }
}

bool wind_solver_update_parameters(double alpha_h, double alpha_v,
                                   double tol_rel, int max_iter)
{
    try {
        require_initialized();
        WindSolverState& state = *g_wind_solver_state;
        state.alpha_h = static_cast<Real>(alpha_h);
        state.alpha_v = static_cast<Real>(alpha_v);
        state.tol_rel = static_cast<Real>(tol_rel);
        state.max_iter = max_iter;
        state.solved = false;
        return true;
    } catch (const std::exception& e) {
        amrex::Print() << "Error updating wind solver parameters: " << e.what() << "\n";
        return false;
    }
}

void wind_solver_get_velocity(std::vector<double>& u_data,
                              std::vector<double>& v_data,
                              std::vector<double>& w_data)
{
    if (!g_wind_solver_state || !g_wind_solver_state->initialized) {
        u_data.clear();
        v_data.clear();
        w_data.clear();
        return;
    }

    const WindSolverState& state = *g_wind_solver_state;
    u_data = extract_multifab_component_fortran(*state.vel, 0, state.nx, state.ny, state.nz);
    v_data = extract_multifab_component_fortran(*state.vel, 1, state.nx, state.ny, state.nz);
    w_data = extract_multifab_component_fortran(*state.vel, 2, state.nx, state.ny, state.nz);
}

void wind_solver_get_velocity0(std::vector<double>& u_data,
                               std::vector<double>& v_data,
                               std::vector<double>& w_data)
{
    if (!g_wind_solver_state || !g_wind_solver_state->initialized) {
        u_data.clear();
        v_data.clear();
        w_data.clear();
        return;
    }

    const WindSolverState& state = *g_wind_solver_state;
    u_data = extract_multifab_component_fortran(*state.vel0, 0, state.nx, state.ny, state.nz);
    v_data = extract_multifab_component_fortran(*state.vel0, 1, state.nx, state.ny, state.nz);
    w_data = extract_multifab_component_fortran(*state.vel0, 2, state.nx, state.ny, state.nz);
}

std::vector<double> wind_solver_get_lambda()
{
    if (!g_wind_solver_state || !g_wind_solver_state->initialized) {
        return {};
    }

    const WindSolverState& state = *g_wind_solver_state;
    return extract_multifab_component_fortran(*state.lambda, 0, state.nx, state.ny, state.nz);
}

std::vector<double> wind_solver_get_div0()
{
    if (!g_wind_solver_state || !g_wind_solver_state->initialized) {
        return {};
    }

    const WindSolverState& state = *g_wind_solver_state;
    return extract_multifab_component_fortran(*state.div0, 0, state.nx, state.ny, state.nz);
}

std::vector<double> wind_solver_get_terrain()
{
    if (!g_wind_solver_state || !g_wind_solver_state->initialized || !g_wind_solver_runtime) {
        return {};
    }

    std::vector<double> terrain(static_cast<std::size_t>(g_wind_solver_state->nx) * g_wind_solver_state->ny);
    for (std::size_t idx = 0; idx < terrain.size(); ++idx) {
        terrain[idx] = static_cast<double>(g_wind_solver_runtime->terrain_host[idx]);
    }
    return terrain;
}

void wind_solver_get_velocity_at_agl(double agl_height,
                                     std::vector<double>& u_data,
                                     std::vector<double>& v_data,
                                     std::vector<double>& w_data)
{
    if (!g_wind_solver_state || !g_wind_solver_state->initialized || !g_wind_solver_runtime) {
        u_data.clear();
        v_data.clear();
        w_data.clear();
        return;
    }

    const WindSolverState& state = *g_wind_solver_state;
    std::vector<double> u3d, v3d, w3d;
    wind_solver_get_velocity(u3d, v3d, w3d);

    u_data.assign(static_cast<std::size_t>(state.nx) * state.ny, 0.0);
    v_data.assign(static_cast<std::size_t>(state.nx) * state.ny, 0.0);
    w_data.assign(static_cast<std::size_t>(state.nx) * state.ny, 0.0);

    for (int j = 0; j < state.ny; ++j) {
        for (int i = 0; i < state.nx; ++i) {
            const std::size_t idx2 = static_cast<std::size_t>(i) + static_cast<std::size_t>(state.nx) * j;
            const int k = agl_to_k(state,
                                   g_wind_solver_runtime->terrain_host[idx2],
                                   static_cast<Real>(agl_height));
            const std::size_t idx3 = static_cast<std::size_t>(i)
                                   + static_cast<std::size_t>(state.nx)
                                         * (static_cast<std::size_t>(j)
                                            + static_cast<std::size_t>(state.ny) * k);
            u_data[idx2] = u3d[idx3];
            v_data[idx2] = v3d[idx3];
            w_data[idx2] = w3d[idx3];
        }
    }
}

void wind_solver_get_velocity_at_k(int k,
                                   std::vector<double>& u_data,
                                   std::vector<double>& v_data,
                                   std::vector<double>& w_data)
{
    if (!g_wind_solver_state || !g_wind_solver_state->initialized) {
        u_data.clear();
        v_data.clear();
        w_data.clear();
        return;
    }

    const WindSolverState& state = *g_wind_solver_state;
    const int kk = std::max(0, std::min(state.nz - 1, k));
    std::vector<double> u3d, v3d, w3d;
    wind_solver_get_velocity(u3d, v3d, w3d);

    u_data.assign(static_cast<std::size_t>(state.nx) * state.ny, 0.0);
    v_data.assign(static_cast<std::size_t>(state.nx) * state.ny, 0.0);
    w_data.assign(static_cast<std::size_t>(state.nx) * state.ny, 0.0);

    for (int j = 0; j < state.ny; ++j) {
        for (int i = 0; i < state.nx; ++i) {
            const std::size_t idx2 = static_cast<std::size_t>(i) + static_cast<std::size_t>(state.nx) * j;
            const std::size_t idx3 = static_cast<std::size_t>(i)
                                   + static_cast<std::size_t>(state.nx)
                                         * (static_cast<std::size_t>(j)
                                            + static_cast<std::size_t>(state.ny) * kk);
            u_data[idx2] = u3d[idx3];
            v_data[idx2] = v3d[idx3];
            w_data[idx2] = w3d[idx3];
        }
    }
}

namespace {
    // Parse comma-separated plot_fields string and return selected field indices
    std::pair<std::vector<int>, std::vector<std::string>> parse_plot_fields(const std::string& plot_fields_str)
    {
        // Map of field names to their components (0-10 = u, v, w, vel_magnitude, u0, v0, w0, lambda, div0, div, terrain_z)
        static const std::map<std::string, std::vector<int>> field_map = {
            {"u", {0}},
            {"v", {1}},
            {"w", {2}},
            {"vel_magnitude", {3}},
            {"vel_mag", {3}},
            {"u0", {4}},
            {"v0", {5}},
            {"w0", {6}},
            {"lambda", {7}},
            {"div0", {8}},
            {"div", {9}},
            {"terrain_z", {10}},
            {"terrain", {10}},
            // Composite fields
            {"velocity", {0, 1, 2, 3}},
            {"pressure", {7}},
            {"initial_velocity", {4, 5, 6}},
            {"divergence", {8, 9}}
        };
        
        std::vector<int> selected_indices;
        std::vector<std::string> selected_names;
        std::set<int> unique_indices;
        
        // Parse comma-separated string
        std::istringstream iss(plot_fields_str);
        std::string field;
        while (std::getline(iss, field, ',')) {
            // Trim whitespace
            size_t start = field.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) {
                // Skip empty or whitespace-only fields
                continue;
            }
            size_t end = field.find_last_not_of(" \t\r\n");
            field = field.substr(start, end - start + 1);
            
            auto it = field_map.find(field);
            if (it != field_map.end()) {
                for (int idx : it->second) {
                    unique_indices.insert(idx);
                }
            } else if (!field.empty()) {
                // Log warning for unrecognized field name
                amrex::Print() << "Warning: unrecognized plot_fields name '" << field << "'\n";
            }
        }
        
        // Convert set to sorted vector
        for (int idx : unique_indices) {
            selected_indices.push_back(idx);
        }
        
        // Create names for selected indices
        const std::vector<std::string> idx_names = {
            "u", "v", "w", "vel_magnitude",
            "u0", "v0", "w0",
            "lambda", "div0", "div", "terrain_z"
        };
        
        for (int idx : selected_indices) {
            if (idx < static_cast<int>(idx_names.size())) {
                selected_names.push_back(idx_names[idx]);
            }
        }
        
        // If no fields were selected, return all
        if (selected_indices.empty()) {
            for (int i = 0; i < NUM_PLOT_FIELDS; ++i) {
                selected_indices.push_back(i);
                selected_names.push_back(idx_names[i]);
            }
        }
        
        return std::make_pair(selected_indices, selected_names);
    }
} // anonymous namespace

bool wind_solver_write_plotfile(const std::string& plotfile_name)
{
    try {
        require_initialized();
        WindSolverState& state = *g_wind_solver_state;

        MultiFab div_current(*state.ba, *state.dm, 1, 0);
        state.vel->FillBoundary(state.geom->periodicity());
        compute_divergence(state, *state.vel, div_current);

        // Parse plot_fields to determine which fields to include
        // Returns pair of (selected_indices, selected_names)
        auto [selected_indices, selected_names_vec] = parse_plot_fields(state.plot_fields);
        
        // Create full output with all fields
        MultiFab output_full(*state.ba, *state.dm, NUM_PLOT_FIELDS, 0);
        build_plotfile_output(output_full, div_current);
        
        // Create filtered output with only selected fields
        int num_selected = static_cast<int>(selected_indices.size());
        MultiFab output_filtered(*state.ba, *state.dm, num_selected, 0);
        
        // Copy selected fields from full output to filtered output
        for (int i = 0; i < num_selected; ++i) {
            int src_comp = selected_indices[i];
            amrex::MultiFab::Copy(output_filtered, output_full, src_comp, i, 1, 0);
        }
        
        // Convert std::vector to amrex::Vector
        Vector<std::string> selected_names;
        for (const auto& name : selected_names_vec) {
            selected_names.push_back(name);
        }
        
        // Use indexed plot file name: plotfile_name_00000, plotfile_name_00001, etc.
        std::string indexed_plotfile = amrex::Concatenate(plotfile_name, 0);
        WriteSingleLevelPlotfile(indexed_plotfile, output_filtered, selected_names, *state.geom, 0.0, 0);
        return true;
    } catch (const std::exception& e) {
        amrex::Print() << "Error writing plotfile: " << e.what() << "\n";
        return false;
    }
}

bool wind_solver_write_extract(const std::string& extract_filename, double agl_height)
{
    try {
        require_initialized();
        WindSolverState& state = *g_wind_solver_state;

        std::vector<double> u, v, w;
        wind_solver_get_velocity_at_agl(agl_height, u, v, w);
        const std::vector<double> terrain = wind_solver_get_terrain();

        std::ofstream out(extract_filename);
        if (!out.is_open()) {
            throw std::runtime_error("cannot open extract file: " + extract_filename);
        }

        out << std::scientific << std::setprecision(6);
        out << "x,y,z_terrain,z_physical,z_agl,u,v,w,speed\n";
        for (int j = 0; j < state.ny; ++j) {
            for (int i = 0; i < state.nx; ++i) {
                const std::size_t idx = static_cast<std::size_t>(i) + static_cast<std::size_t>(state.nx) * j;
                const Real zs = static_cast<Real>(terrain[idx]);
                const int k = agl_to_k(state, zs, static_cast<Real>(agl_height));
                const Real z_phys = state.zmin + (k + Real(0.5)) * state.dz;
                const Real z_agl = z_phys - zs;
                const Real x = state.xmin + (i + Real(0.5)) * state.dx;
                const Real y = state.ymin + (j + Real(0.5)) * state.dy;
                const Real speed = std::sqrt(static_cast<Real>(u[idx] * u[idx] + v[idx] * v[idx] + w[idx] * w[idx]));

                out << x << ',' << y << ',' << zs << ',' << z_phys << ',' << z_agl << ','
                    << u[idx] << ',' << v[idx] << ',' << w[idx] << ',' << speed << '\n';
            }
        }
        return true;
    } catch (const std::exception& e) {
        amrex::Print() << "Error writing extract: " << e.what() << "\n";
        return false;
    }
}

void wind_solver_finalize()
{
    g_wind_solver_runtime.reset();
    g_wind_solver_state.reset();

    if (!g_amrex_initialized_here && g_parmparse_initialized) {
        ParmParse::Finalize();
        g_parmparse_initialized = false;
    }

    if (g_amrex_initialized_here && amrex::Initialized()) {
        amrex::Finalize();
        g_amrex_initialized_here = false;
        g_parmparse_initialized = false;
    }
}

bool wind_solver_is_initialized()
{
    return g_wind_solver_state && g_wind_solver_state->initialized;
}

bool wind_solver_add_turbine(double x, double y, double hub_height, double rotor_diameter, double default_ct, const std::string& power_curve_file, double yaw, double orientation, double tilt)
{
    try {
        require_initialized();
        WindSolverState& state = *g_wind_solver_state;
        
        TurbineWake::Turbine t;
        t.id = static_cast<int>(state.turbines.size());
        t.x = static_cast<Real>(x);
        t.y = static_cast<Real>(y);
        t.hub_height = static_cast<Real>(hub_height);
        t.rotor_diameter = static_cast<Real>(rotor_diameter);
        t.ct_curve.default_ct = static_cast<Real>(default_ct);
        t.power_curve_file = power_curve_file;
        t.yaw = static_cast<Real>(yaw);
        t.orientation = static_cast<Real>(orientation);
        t.tilt = static_cast<Real>(tilt);
        
        if (!power_curve_file.empty()) {
            TurbineWake::read_power_curve_file(power_curve_file, t.power_curve, t.ct_curve);
        }
        
        state.turbines.push_back(t);
        amrex::Print() << "wind_solver: Added turbine " << t.id << " at (" << t.x << ", " << t.y << ") with yaw=" << yaw << ", orientation=" << orientation << ", tilt=" << tilt << "\n";
        return true;
    } catch (const std::exception& e) {
        amrex::Print() << "Error adding turbine: " << e.what() << "\n";
        return false;
    }
}

void wind_solver_clear_turbines()
{
    if (g_wind_solver_state) {
        g_wind_solver_state->turbines.clear();
        amrex::Print() << "wind_solver: Cleared all turbines\n";
    }
}

std::vector<double> wind_solver_get_turbine_power_outputs()
{
    std::vector<double> power;
    if (g_wind_solver_state) {
        for (const auto& t : g_wind_solver_state->turbines) {
            power.push_back(static_cast<double>(t.power_output));
        }
    }
    return power;
}

std::vector<double> wind_solver_get_turbine_inflow_speeds()
{
    std::vector<double> speed;
    if (g_wind_solver_state) {
        for (const auto& t : g_wind_solver_state->turbines) {
            speed.push_back(static_cast<double>(t.inflow_wind_speed));
        }
    }
    return speed;
}

std::vector<double> wind_solver_get_turbine_yaws()
{
    std::vector<double> yaws;
    if (g_wind_solver_state) {
        for (const auto& t : g_wind_solver_state->turbines) {
            yaws.push_back(static_cast<double>(t.yaw));
        }
    }
    return yaws;
}

std::vector<double> wind_solver_get_turbine_orientations()
{
    std::vector<double> orientations;
    if (g_wind_solver_state) {
        for (const auto& t : g_wind_solver_state->turbines) {
            orientations.push_back(static_cast<double>(t.orientation));
        }
    }
    return orientations;
}

std::vector<double> wind_solver_get_turbine_tilts()
{
    std::vector<double> tilts;
    if (g_wind_solver_state) {
        for (const auto& t : g_wind_solver_state->turbines) {
            tilts.push_back(static_cast<double>(t.tilt));
        }
    }
    return tilts;
}

std::vector<double> wind_solver_get_turbine_u_hubs()
{
    std::vector<double> u_hubs;
    if (g_wind_solver_state) {
        for (const auto& t : g_wind_solver_state->turbines) {
            u_hubs.push_back(static_cast<double>(t.u_hub));
        }
    }
    return u_hubs;
}

std::vector<double> wind_solver_get_turbine_v_hubs()
{
    std::vector<double> v_hubs;
    if (g_wind_solver_state) {
        for (const auto& t : g_wind_solver_state->turbines) {
            v_hubs.push_back(static_cast<double>(t.v_hub));
        }
    }
    return v_hubs;
}

std::vector<double> wind_solver_get_turbine_z_terrains()
{
    std::vector<double> z_terrains;
    if (g_wind_solver_state) {
        for (const auto& t : g_wind_solver_state->turbines) {
            z_terrains.push_back(static_cast<double>(t.z_terrain));
        }
    }
    return z_terrains;
}

// ============================================================================
// Heat Source API Functions for Fire Coupling
// ============================================================================

bool wind_solver_add_heat_source(
    const std::vector<double>& heat_flux_data,
    int nx, int ny,
    double scaling_factor)
{
    try {
        require_initialized();
        WindSolverState& state = *g_wind_solver_state;
        
        // Validate dimensions
        if (static_cast<int>(heat_flux_data.size()) != nx * ny) {
            amrex::Print() << "ERROR: heat_flux_data size (" << heat_flux_data.size()
                          << ") doesn't match grid dimensions (" << nx << " x " << ny << ")\n";
            return false;
        }
        
        if (nx != state.nx || ny != state.ny) {
            amrex::Print() << "ERROR: heat_flux grid dimensions (" << nx << " x " << ny
                          << ") don't match solver grid (" << state.nx << " x " << state.ny << ")\n";
            return false;
        }
        
        // Create heat source MultiFab if it doesn't exist
        if (!state.heat_source) {
            // Create a 2D MultiFab with 1 component (nz=1 for 2D field)
            amrex::BoxArray ba_2d = convert(*state.ba, amrex::IntVect(0, 0, AMREX_SPACEDIM-1));
            state.heat_source = std::make_unique<amrex::MultiFab>(ba_2d, *state.dm, 1, 0);
        }
        
        // Copy heat flux data into MultiFab
        // Data is expected in row-major order: data[j*nx + i]
        // Make a pointer to the data for capture in device lambda
        const double* heat_flux_ptr = heat_flux_data.data();
        
        for (MFIter mfi(*state.heat_source); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto hs_arr = state.heat_source->array(mfi);
            
            amrex::ParallelFor(bx, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
                int flat_idx = j * nx + i;
                hs_arr(i, j, k) = scaling_factor * heat_flux_ptr[flat_idx];
            });
        }
        
        state.has_heat_source = true;
        state.heat_source_scaling_factor = scaling_factor;
        
        amrex::Print() << "Heat source added for fire coupling: " 
                      << nx << " x " << ny << " grid, scaling=" << scaling_factor << "\n";
        
        return true;
        
    } catch (const std::exception& e) {
        amrex::Print() << "ERROR in wind_solver_add_heat_source: " << e.what() << "\n";
        return false;
    }
}

void wind_solver_clear_heat_source()
{
    if (g_wind_solver_state) {
        WindSolverState& state = *g_wind_solver_state;
        state.heat_source.reset();
        state.has_heat_source = false;
        state.heat_source_scaling_factor = 1.0;
    }
}

std::pair<std::vector<double>, bool> wind_solver_get_heat_source()
{
    std::vector<double> heat_flux_data;
    bool is_active = false;
    
    if (g_wind_solver_state && g_wind_solver_state->heat_source) {
        WindSolverState& state = *g_wind_solver_state;
        is_active = state.has_heat_source;
        
        // Extract heat flux data
        // Note: This returns empty array as heat sources are typically write-only for coupling
        // If full round-trip is needed, this would require explicit GPU->CPU transfer
        int total_size = state.nx * state.ny;
        heat_flux_data.resize(total_size, 0.0);
    }
    
    return std::make_pair(heat_flux_data, is_active);
}
