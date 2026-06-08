#ifndef TURBINE_WAKE_MODELS_H
#define TURBINE_WAKE_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_MultiFab.H>
#include <AMReX_Vector.H>
#include <AMReX_Print.H>
#include <AMReX_MFIter.H>
#include <string>
#include <vector>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#ifdef MASSCONSISTENT_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace TurbineWake {

using amrex::Real;

enum class TurbineWakeModelType {
    JENSEN,
    BASTANKHAH_GAUSSIAN,
    TURBOPARK,
    GAUSS_CURL_HYBRID
};

enum class WakeAddedTurbulenceModelType {
    NONE,
    CRESPO_HERNANDEZ,
    FRANDSEN
};

enum class SuperpositionType {
    LINEAR,
    QUADRATIC, // Sum of squares
    MAX
};

struct PowerCurve {
    std::vector<Real> wind_speeds;
    std::vector<Real> power_values;

    Real interpolate(Real speed) const {
        if (wind_speeds.empty()) return Real(0.0);
        if (speed <= wind_speeds.front()) return power_values.front();
        if (speed >= wind_speeds.back()) return power_values.back();

        // Linear interpolation
        for (std::size_t i = 0; i < wind_speeds.size() - 1; ++i) {
            if (speed >= wind_speeds[i] && speed <= wind_speeds[i+1]) {
                Real t = (speed - wind_speeds[i]) / (wind_speeds[i+1] - wind_speeds[i]);
                return power_values[i] + t * (power_values[i+1] - power_values[i]);
            }
        }
        return Real(0.0);
    }
};

struct CtCurve {
    std::vector<Real> wind_speeds;
    std::vector<Real> ct_values;
    Real default_ct = Real(0.8);

    Real interpolate(Real speed) const {
        if (wind_speeds.empty()) return default_ct;
        if (speed <= wind_speeds.front()) return ct_values.front();
        if (speed >= wind_speeds.back()) return ct_values.back();

        // Linear interpolation
        for (std::size_t i = 0; i < wind_speeds.size() - 1; ++i) {
            if (speed >= wind_speeds[i] && speed <= wind_speeds[i+1]) {
                Real t = (speed - wind_speeds[i]) / (wind_speeds[i+1] - wind_speeds[i]);
                return ct_values[i] + t * (ct_values[i+1] - ct_values[i]);
            }
        }
        return default_ct;
    }
};

struct Turbine {
    int id = 0;
    Real x = 0.0;
    Real y = 0.0;
    Real hub_height = 0.0; // z measured from terrain height (AGL)
    Real rotor_diameter = 0.0;
    CtCurve ct_curve;
    PowerCurve power_curve;
    std::string power_curve_file = "";
    Real yaw = 0.0;        // yaw angle in degrees
    Real orientation = 0.0; // orientation angle in degrees
    Real tilt = 0.0;       // tilt angle in degrees

    // Outputs computed during simulation
    Real inflow_wind_speed = 0.0;
    Real u_hub = 0.0;
    Real v_hub = 0.0;
    Real direction_deg = 0.0;
    Real power_output = 0.0;
    Real z_terrain = 0.0;
    Real z_hub = 0.0;
    Real local_ti = 0.0;
};

inline void read_power_curve_json(const std::string& filename, [[maybe_unused]] PowerCurve& pc, [[maybe_unused]] CtCurve& cc) {
#ifdef MASSCONSISTENT_USE_NLOHMANN_JSON
    std::ifstream f(filename);
    if (!f.is_open()) {
        amrex::Print() << "Warning: Could not open JSON power curve file: " << filename << "\n";
        return;
    }
    try {
        nlohmann::json j;
        f >> j;

        auto get_array = [](const nlohmann::json& node, const std::vector<std::string>& keys) -> std::vector<Real> {
            for (const auto& key : keys) {
                if (node.contains(key) && node[key].is_array()) {
                    std::vector<Real> res;
                    for (const auto& val : node[key]) {
                        if (val.is_number()) {
                            res.push_back(val.get<Real>());
                        }
                    }
                    if (!res.empty()) return res;
                }
            }
            return {};
        };

        std::vector<std::string> ws_keys = {"wind_speed", "wind_speeds", "speed", "speeds", "ws"};
        std::vector<std::string> p_keys = {"power", "power_values", "power_curve", "powers", "p"};
        std::vector<std::string> ct_keys = {"thrust_coefficient", "thrust_coefficients", "ct", "ct_values", "ct_curve", "thrust"};

        std::vector<Real> ws_power, p_vals, ws_ct, ct_vals;

        // 1. Try FLORIS v3 format with nested "power_thrust_table"
        if (j.contains("power_thrust_table") && j["power_thrust_table"].is_object()) {
            auto table = j["power_thrust_table"];
            ws_power = get_array(table, ws_keys);
            p_vals = get_array(table, p_keys);
            ct_vals = get_array(table, ct_keys);
            ws_ct = ws_power;
        }

        // 2. Try nested power_curve / ct_curve objects
        if (ws_power.empty() && j.contains("power_curve") && j["power_curve"].is_object()) {
            auto pc_node = j["power_curve"];
            ws_power = get_array(pc_node, ws_keys);
            p_vals = get_array(pc_node, p_keys);
            if (p_vals.empty()) p_vals = get_array(pc_node, {"values", "power_values"});
        }
        if (ws_ct.empty() && j.contains("ct_curve") && j["ct_curve"].is_object()) {
            auto cc_node = j["ct_curve"];
            ws_ct = get_array(cc_node, ws_keys);
            ct_vals = get_array(cc_node, ct_keys);
            if (ct_vals.empty()) ct_vals = get_array(cc_node, {"values", "ct_values"});
        }

        // 3. Fallback to top-level arrays
        if (ws_power.empty()) ws_power = get_array(j, ws_keys);
        if (p_vals.empty()) p_vals = get_array(j, p_keys);
        if (ws_ct.empty()) ws_ct = ws_power;
        if (ct_vals.empty()) ct_vals = get_array(j, ct_keys);

        if (!ws_power.empty() && !p_vals.empty()) {
            pc.wind_speeds = ws_power;
            pc.power_values = p_vals;
        }
        if (!ws_ct.empty() && !ct_vals.empty()) {
            cc.wind_speeds = ws_ct;
            cc.ct_values = ct_vals;
        }

        amrex::Print() << "wind_solver: loaded power curve (" << pc.wind_speeds.size() 
                       << " points) and thrust curve (" << cc.wind_speeds.size() 
                       << " points) from JSON: " << filename << "\n";

    } catch (const std::exception& e) {
        amrex::Print() << "Warning: Error parsing JSON power curve file: " << filename 
                       << ". Exception: " << e.what() << "\n";
    }
#else
    amrex::Print() << "Warning: JSON power curves are not supported without nlohmann_json. "
                   << "Could not load " << filename << "\n";
#endif
}

inline void read_power_curve_file(const std::string& filename, PowerCurve& pc, CtCurve& cc) {
    if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".json") {
        read_power_curve_json(filename, pc, cc);
        return;
    }
    std::ifstream f(filename);
    if (!f.is_open()) {
        amrex::Print() << "Warning: Could not open power curve file: " << filename 
                       << ". Power output will be 0 and Ct default_ct.\n";
        return;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        // Skip header lines starting with letters
        if (std::isalpha(line[0])) continue;
        
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        Real ws, power, ct;
        if (ss >> ws >> power) {
            pc.wind_speeds.push_back(ws);
            pc.power_values.push_back(power);
            if (ss >> ct) {
                cc.wind_speeds.push_back(ws);
                cc.ct_values.push_back(ct);
            }
        }
    }
}

inline bool is_numeric(const std::string& s) {
    if (s.empty()) return false;
    char* p;
    strtod(s.c_str(), &p);
    if (p == s.c_str()) return false;
    return *p == '\0';
}

inline void read_turbines_file(const std::string& filename, std::vector<Turbine>& turbines) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        amrex::Abort("wind_solver: cannot open turbines file: " + filename);
    }
    std::string line;
    int id = 0;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        // Skip header lines
        if (line.find("x") != std::string::npos || line.find("X") != std::string::npos) continue;
        
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (ss >> token) {
            tokens.push_back(token);
        }
        
        if (tokens.size() >= 5) {
            Real x = std::stod(tokens[0]);
            Real y = std::stod(tokens[1]);
            Real hub_h = std::stod(tokens[2]);
            Real rot_d = std::stod(tokens[3]);
            Real default_ct = std::stod(tokens[4]);
            
            Turbine t;
            t.id = id++;
            t.x = x;
            t.y = y;
            t.hub_height = hub_h;
            t.rotor_diameter = rot_d;
            t.ct_curve.default_ct = default_ct;
            t.yaw = 0.0;
            t.orientation = 0.0;
            t.tilt = 0.0;
            t.power_curve_file = "";
            
            bool parsed_yaw = false;
            bool parsed_orientation = false;
            bool parsed_tilt = false;
            for (std::size_t i = 5; i < tokens.size(); ++i) {
                const std::string& tok = tokens[i];
                if (is_numeric(tok)) {
                    if (!parsed_yaw) {
                        t.yaw = std::stod(tok);
                        parsed_yaw = true;
                    } else if (!parsed_orientation) {
                        t.orientation = std::stod(tok);
                        parsed_orientation = true;
                    } else if (!parsed_tilt) {
                        t.tilt = std::stod(tok);
                        parsed_tilt = true;
                    } else {
                        amrex::Print() << "Warning: Unexpected extra numeric column in turbines file: " << tok << "\n";
                    }
                } else {
                    if (t.power_curve_file.empty()) {
                        t.power_curve_file = tok;
                    } else {
                        amrex::Print() << "Warning: Unexpected extra non-numeric column in turbines file: " << tok << "\n";
                    }
                }
            }
            
            if (!t.power_curve_file.empty()) {
                std::string resolved_pc_file = t.power_curve_file;
                if (t.power_curve_file[0] != '/' && filename.find('/') != std::string::npos) {
                    std::size_t last_slash = filename.find_last_of('/');
                    resolved_pc_file = filename.substr(0, last_slash + 1) + t.power_curve_file;
                }
                read_power_curve_file(resolved_pc_file, t.power_curve, t.ct_curve);
            }
            turbines.push_back(t);
        }
    }
    amrex::Print() << "wind_solver: loaded " << turbines.size() << " turbines from " << filename << "\n";
}

inline Real compute_bastankhah_deflection(
    Real yaw,
    Real rotor_diameter,
    Real inflow_wind_speed,
    Real ct,
    Real local_ti,
    Real x_down,
    Real gaussian_ka_base,
    Real stability_factor,
    Real L_obukhov,
    bool enable_stability)
{
    Real gamma_rad = yaw * M_PI / 180.0;
    Real cos_yaw = std::cos(gamma_rad);
    Real ct_clamped = std::max(Real(0.0), std::min(ct, Real(0.99)));
    Real ct_cos_yaw = std::max(Real(1e-4), std::min(ct_clamped * cos_yaw, Real(0.99)));
    Real uR = inflow_wind_speed * ct_cos_yaw / (Real(2.0) * (Real(1.0) - std::sqrt(Real(1.0) - ct_cos_yaw)));
    Real u0 = inflow_wind_speed * std::sqrt(Real(1.0) - ct_clamped);

    Real alpha_param = Real(0.58);
    Real beta_param = Real(0.077);
    Real denom = std::sqrt(Real(2.0)) * (Real(4.0) * alpha_param * local_ti + Real(2.0) * beta_param * (Real(1.0) - std::sqrt(Real(1.0) - ct_clamped)));
    if (denom < Real(1e-5)) denom = Real(1e-5);
    Real x0 = rotor_diameter * std::abs(cos_yaw) * (Real(1.0) + std::sqrt(Real(1.0) - ct_cos_yaw)) / denom;

    Real k = gaussian_ka_base * stability_factor;
    if (k < Real(1e-5)) k = Real(1e-5);

    Real sigma_z0 = rotor_diameter * Real(0.5) * std::sqrt(uR / (inflow_wind_speed + u0));
    Real sigma_y0 = sigma_z0 * std::abs(cos_yaw);

    Real theta_c0 = Real(0.3) * (gamma_rad / cos_yaw) * (Real(1.0) - std::sqrt(Real(1.0) - ct_cos_yaw));
    Real delta0 = std::tan(theta_c0) * x0;

    if (x_down <= x0) {
        return delta0 * (x_down / x0);
    } else {
        Real sigma_y = k * (x_down - x0) + sigma_y0;
        Real sigma_z = k * (x_down - x0) + sigma_z0;

        Real C0 = Real(1.0) - std::sqrt(Real(1.0) - ct_clamped);
        Real M0 = C0 * (Real(2.0) - C0);
        if (M0 < Real(1e-5)) M0 = Real(1e-5);
        Real exp_1_12 = std::exp(Real(1.0)/Real(12.0));
        Real exp_1_3 = std::exp(Real(1.0)/Real(3.0));
        Real E0 = C0 * C0 - Real(3.0) * exp_1_12 * C0 + Real(3.0) * exp_1_3;

        Real mid = std::sqrt((sigma_y * sigma_z) / (sigma_y0 * sigma_z0));
        Real sqrt_M0 = std::sqrt(M0);
        Real ln_num = (Real(1.6) + sqrt_M0) * (Real(1.6) * mid - sqrt_M0);
        Real ln_den = (Real(1.6) - sqrt_M0) * (Real(1.6) * mid + sqrt_M0);
        if (ln_num < Real(1e-5)) ln_num = Real(1e-5);
        if (ln_den < Real(1e-5)) ln_den = Real(1e-5);
        Real log_t = std::log(ln_num / ln_den);

        Real mult = theta_c0 * (E0 / Real(5.2)) * std::sqrt((sigma_y0 * sigma_z0) / (k * k * M0));
        return delta0 + mult * log_t;
    }
}

inline Real interpolate_2d(
    const std::vector<Real>& field,
    Real xq, Real yq,
    Real xmin, Real ymin,
    Real dx, Real dy,
    int nx, int ny)
{
    Real ifrac = (xq - xmin) / dx - Real(0.5);
    Real jfrac = (yq - ymin) / dy - Real(0.5);

    int i0 = std::max(0, std::min(static_cast<int>(std::floor(ifrac)), nx - 1));
    int j0 = std::max(0, std::min(static_cast<int>(std::floor(jfrac)), ny - 1));

    int i1 = std::max(0, std::min(i0 + 1, nx - 1));
    int j1 = std::max(0, std::min(j0 + 1, ny - 1));

    Real fi = ifrac - std::floor(ifrac);
    Real fj = jfrac - std::floor(jfrac);

    if (i0 == i1) fi = 0.0;
    if (j0 == j1) fj = 0.0;

    auto get_val = [&](int i, int j) {
        std::size_t idx = static_cast<std::size_t>(i) + static_cast<std::size_t>(nx) * j;
        return field[idx];
    };

    Real c00 = get_val(i0, j0);
    Real c10 = get_val(i1, j0);
    Real c01 = get_val(i0, j1);
    Real c11 = get_val(i1, j1);

    Real c0 = c00 * (1.0 - fi) + c10 * fi;
    Real c1 = c01 * (1.0 - fi) + c11 * fi;

    return c0 * (1.0 - fj) + c1 * fj;
}

inline Real interpolate_3d(
    const std::vector<Real>& field,
    Real xq, Real yq, Real zq,
    Real xmin, Real ymin, Real zmin,
    Real dx, Real dy, Real dz,
    int nx, int ny, int nz)
{
    Real ifrac = (xq - xmin) / dx - Real(0.5);
    Real jfrac = (yq - ymin) / dy - Real(0.5);
    Real kfrac = (zq - zmin) / dz - Real(0.5);

    int i0 = std::max(0, std::min(static_cast<int>(std::floor(ifrac)), nx - 1));
    int j0 = std::max(0, std::min(static_cast<int>(std::floor(jfrac)), ny - 1));
    int k0 = std::max(0, std::min(static_cast<int>(std::floor(kfrac)), nz - 1));

    int i1 = std::max(0, std::min(i0 + 1, nx - 1));
    int j1 = std::max(0, std::min(j0 + 1, ny - 1));
    int k1 = std::max(0, std::min(k0 + 1, nz - 1));

    Real fi = ifrac - std::floor(ifrac);
    Real fj = jfrac - std::floor(jfrac);
    Real fk = kfrac - std::floor(kfrac);

    if (i0 == i1) fi = 0.0;
    if (j0 == j1) fj = 0.0;
    if (k0 == k1) fk = 0.0;

    auto get_val = [&](int i, int j, int k) {
        std::size_t idx = static_cast<std::size_t>(i) 
                        + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(j) 
                        + static_cast<std::size_t>(ny) * k);
        return field[idx];
    };

    Real c000 = get_val(i0, j0, k0);
    Real c100 = get_val(i1, j0, k0);
    Real c010 = get_val(i0, j1, k0);
    Real c110 = get_val(i1, j1, k0);
    Real c001 = get_val(i0, j0, k1);
    Real c101 = get_val(i1, j0, k1);
    Real c011 = get_val(i0, j1, k1);
    Real c111 = get_val(i1, j1, k1);

    Real c00 = c000 * (1.0 - fi) + c100 * fi;
    Real c10 = c010 * (1.0 - fi) + c110 * fi;
    Real c01 = c001 * (1.0 - fi) + c101 * fi;
    Real c11 = c011 * (1.0 - fi) + c111 * fi;

    Real c0 = c00 * (1.0 - fj) + c10 * fj;
    Real c1 = c01 * (1.0 - fj) + c11 * fj;

    return c0 * (1.0 - fk) + c1 * fk;
}

inline void apply_analytical_turbine_wakes(
    std::vector<Real>& u_host,
    std::vector<Real>& v_host,
    std::vector<Real>& w_host,
    const std::vector<Real>& terrain_host,
    std::vector<Turbine>& turbines,
    TurbineWakeModelType model_type,
    SuperpositionType superposition,
    Real jensen_kw_base,
    Real gaussian_ka_base,
    bool enable_stability,
    Real L_obukhov,
    Real xmin, Real ymin, Real zmin,
    Real dx, Real dy, Real dz,
    int nx, int ny, int nz,
    Real turbopark_c1_base = 0.38,
    Real ambient_ti_base = 0.075,
    bool enable_jimenez_deflection = false,
    Real jimenez_kd_base = 0.05,
    bool enable_bastankhah_deflection = false,
    WakeAddedTurbulenceModelType wake_added_turb_model = WakeAddedTurbulenceModelType::NONE,
    int time_step = 0,
    bool enable_wake_ground_interaction = true,
    Real wake_ground_damping_scale = 0.25)
{
    if (turbines.empty()) return;

    std::string model_name = "Jensen";
    if (model_type == TurbineWakeModelType::BASTANKHAH_GAUSSIAN) {
        model_name = "Bastankhah Gaussian";
    } else if (model_type == TurbineWakeModelType::TURBOPARK) {
        model_name = "TurbOPark";
    } else if (model_type == TurbineWakeModelType::GAUSS_CURL_HYBRID) {
        model_name = "Gauss-Curl Hybrid";
    }

    amrex::Print() << "wind_solver: applying analytical turbine wake models ("
                   << model_name << ")\n";

    // 1. Determine wind direction and sort turbines from upwind to downwind
    Real sum_u = 0.0, sum_v = 0.0;
    for (std::size_t idx = 0; idx < u_host.size(); ++idx) {
        sum_u += u_host[idx];
        sum_v += v_host[idx];
    }
    Real u_mean = sum_u / u_host.size();
    Real v_mean = sum_v / u_host.size();
    Real speed_mean = std::sqrt(u_mean * u_mean + v_mean * v_mean);
    if (speed_mean < Real(1.0e-5)) {
        u_mean = 1.0;
        v_mean = 0.0;
        speed_mean = 1.0;
    }
    Real u_dir = u_mean / speed_mean;
    Real v_dir = v_mean / speed_mean;

    // Sort turbines from upwind to downwind
    std::vector<Turbine*> sorted_turbines;
    for (auto& t : turbines) {
        sorted_turbines.push_back(&t);
    }
    std::sort(sorted_turbines.begin(), sorted_turbines.end(), [=](const Turbine* a, const Turbine* b) {
        Real dist_a = a->x * u_dir + a->y * v_dir;
        Real dist_b = b->x * u_dir + b->y * v_dir;
        return dist_a < dist_b;
    });

    // 2. Keep background velocities for superposition
    std::vector<Real> u_bg = u_host;
    std::vector<Real> v_bg = v_host;
    std::vector<Real> cum_deficit_sq(u_host.size(), 0.0);
    std::vector<Real> max_deficit_vec(u_host.size(), 0.0);
    std::vector<Real> cum_u_vortex(u_host.size(), 0.0);
    std::vector<Real> cum_v_vortex(u_host.size(), 0.0);

    // Initialize local_ti for all turbines
    for (auto* t_ptr : sorted_turbines) {
        t_ptr->local_ti = ambient_ti_base;
    }

    // 3. Process each turbine sequentially
    for (auto* t_ptr : sorted_turbines) {
        Turbine& t = *t_ptr;

        // Query terrain elevation at turbine location
        t.z_terrain = interpolate_2d(terrain_host, t.x, t.y, xmin, ymin, dx, dy, nx, ny);
        t.z_hub = t.z_terrain + t.hub_height;

        // Interpolate current (wake-influenced) local wind components at hub location
        t.u_hub = interpolate_3d(u_host, t.x, t.y, t.z_hub, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz);
        t.v_hub = interpolate_3d(v_host, t.x, t.y, t.z_hub, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz);
        t.inflow_wind_speed = std::sqrt(t.u_hub * t.u_hub + t.v_hub * t.v_hub);

        // Compute meteorological wind direction (0 = North, 90 = East)
        t.direction_deg = std::fmod(std::atan2(t.u_hub, t.v_hub) * Real(180.0) / Real(M_PI) + Real(360.0), Real(360.0));

        // Evaluate power and thrust
        t.power_output = t.power_curve.interpolate(t.inflow_wind_speed);
        Real ct = t.ct_curve.interpolate(t.inflow_wind_speed);

        // Calculate wake-added turbulence at this turbine's location from all upstream turbines
        if (wake_added_turb_model != WakeAddedTurbulenceModelType::NONE) {
            Real added_ti_sq = 0.0;
            for (const auto* t_up : sorted_turbines) {
                if (t_up->id == t.id) break; // processed upwind to downwind, so break when we reach ourselves

                Real dx_pt = t.x - t_up->x;
                Real dy_pt = t.y - t_up->y;
                Real u_h_dir_up = t_up->u_hub / t_up->inflow_wind_speed;
                Real v_h_dir_up = t_up->v_hub / t_up->inflow_wind_speed;

                Real x_down_up = dx_pt * u_h_dir_up + dy_pt * v_h_dir_up;
                if (x_down_up <= Real(1.0e-3)) continue;

                Real y_cross_signed_up = -dx_pt * v_h_dir_up + dy_pt * u_h_dir_up;
                Real y_offset_up = 0.0;
                Real ct_up = t_up->ct_curve.interpolate(t_up->inflow_wind_speed);
                if (enable_bastankhah_deflection && std::abs(t_up->yaw) > 1e-5) {
                   Real up_stability_factor = Real(1.0);
                   if (enable_stability && std::abs(L_obukhov) > Real(1.0e-5)) {
                       Real zeta = t_up->hub_height / L_obukhov;
                       if (zeta > Real(0.0)) {
                           up_stability_factor = Real(1.0) - Real(0.4) * std::tanh(zeta);
                       } else if (zeta < Real(0.0)) {
                           up_stability_factor = Real(1.0) + Real(0.6) * std::tanh(-zeta);
                       }
                   }
                   y_offset_up = compute_bastankhah_deflection(
                       t_up->yaw, t_up->rotor_diameter, t_up->inflow_wind_speed,
                       ct_up, t_up->local_ti, x_down_up,
                       gaussian_ka_base, up_stability_factor, L_obukhov, enable_stability
                   );
                } else if (enable_jimenez_deflection && std::abs(t_up->yaw) > 1e-5) {
                   Real gamma_rad = t_up->yaw * M_PI / 180.0;
                   Real theta_0 = (ct_up / Real(2.0)) * std::cos(gamma_rad) * std::cos(gamma_rad) * std::sin(gamma_rad);
                   Real beta_def = jimenez_kd_base;
                   if (beta_def < Real(1e-5)) beta_def = Real(1e-5);
                   y_offset_up = t_up->rotor_diameter * theta_0 * (Real(1.0) / (Real(2.0) * beta_def)) * 
                                 (Real(1.0) - Real(1.0) / (Real(1.0) + Real(2.0) * beta_def * x_down_up / t_up->rotor_diameter));
                }
                Real z_offset_up = 0.0;
                if (std::abs(t_up->tilt) > 1e-5) {
                    Real tilt_rad = t_up->tilt * M_PI / 180.0;
                    Real theta_v0 = (ct_up / Real(2.0)) * std::cos(tilt_rad) * std::cos(tilt_rad) * std::sin(tilt_rad);
                    Real beta_def = jimenez_kd_base;
                    if (beta_def < Real(1e-5)) beta_def = Real(1e-5);
                    z_offset_up = t_up->rotor_diameter * theta_v0 * (Real(1.0) / (Real(2.0) * beta_def)) * 
                                  (Real(1.0) - Real(1.0) / (Real(1.0) + Real(2.0) * beta_def * x_down_up / t_up->rotor_diameter));
                }
                Real y_cross_up = std::abs(y_cross_signed_up - y_offset_up);
                Real z_vertical_up = t.hub_height - (t_up->hub_height + z_offset_up);
                Real r_up = std::sqrt(y_cross_up * y_cross_up + z_vertical_up * z_vertical_up);

                // Peak added TI on centerline
                Real delta_I_peak = 0.0;
                Real s_dist = std::max(Real(1.0), x_down_up / t_up->rotor_diameter);
                Real ct_clamped_up = std::max(Real(1e-4), std::min(ct_up, Real(0.99)));
                
                if (wake_added_turb_model == WakeAddedTurbulenceModelType::CRESPO_HERNANDEZ) {
                    Real a = (Real(1.0) - std::sqrt(Real(1.0) - ct_clamped_up)) / Real(2.0);
                    delta_I_peak = Real(0.73) * std::pow(a, Real(0.832)) * std::pow(ambient_ti_base, Real(0.0325)) * std::pow(s_dist, Real(-0.32));
                } else if (wake_added_turb_model == WakeAddedTurbulenceModelType::FRANDSEN) {
                    delta_I_peak = Real(1.0) / (Real(1.5) + Real(0.8) * s_dist / std::sqrt(ct_clamped_up));
                }

                // Radial spreading
                Real factor = 0.0;
                if (model_type == TurbineWakeModelType::JENSEN) {
                    Real kw = jensen_kw_base;
                    if (enable_stability && std::abs(L_obukhov) > Real(1.0e-5)) {
                        Real zeta = t_up->hub_height / L_obukhov;
                        Real stability_factor = Real(1.0);
                        if (zeta > Real(0.0)) {
                            stability_factor = Real(1.0) - Real(0.4) * std::tanh(zeta);
                        } else if (zeta < Real(0.0)) {
                            stability_factor = Real(1.0) + Real(0.6) * std::tanh(-zeta);
                        }
                        kw *= stability_factor;
                    }
                    Real r_wake = t_up->rotor_diameter / Real(2.0) + kw * x_down_up;
                    if (r_up <= r_wake) {
                        factor = Real(1.0);
                    }
                } else { // Gaussian or TurbOPark
                    Real ka = gaussian_ka_base;
                    if (model_type == TurbineWakeModelType::TURBOPARK) {
                        ka = turbopark_c1_base * t_up->local_ti;
                    }
                    if (enable_stability && std::abs(L_obukhov) > Real(1.0e-5)) {
                        Real zeta = t_up->hub_height / L_obukhov;
                        Real stability_factor = Real(1.0);
                        if (zeta > Real(0.0)) {
                            stability_factor = Real(1.0) - Real(0.4) * std::tanh(zeta);
                        } else if (zeta < Real(0.0)) {
                            stability_factor = Real(1.0) + Real(0.6) * std::tanh(-zeta);
                        }
                        ka *= stability_factor;
                    }
                    
                    Real sigma = 0.0;
                    if (model_type == TurbineWakeModelType::TURBOPARK) {
                        Real sigma_0 = Real(0.25) * t_up->rotor_diameter;
                        sigma = sigma_0 + ka * x_down_up;
                    } else {
                        Real sqrt_one_minus_ct = std::sqrt(Real(1.0) - ct_clamped_up);
                        Real beta = Real(0.5) * (Real(1.0) + sqrt_one_minus_ct) / sqrt_one_minus_ct;
                        Real epsilon = Real(0.2) * std::sqrt(beta);
                        sigma = ka * x_down_up + epsilon * t_up->rotor_diameter;
                    }
                    factor = std::exp(-r_up * r_up / (Real(2.0) * sigma * sigma));
                }

                Real delta_I_local = delta_I_peak * factor;
                added_ti_sq += delta_I_local * delta_I_local;
            }
            t.local_ti = std::sqrt(ambient_ti_base * ambient_ti_base + added_ti_sq);
        }

        amrex::Print() << "  Turbine " << t.id << " at (" << t.x << ", " << t.y << ", z_hub=" << t.z_hub << "):\n"
                       << "    Inflow speed = " << t.inflow_wind_speed << " m/s, Dir = " << t.direction_deg << " deg\n"
                       << "    Power = " << t.power_output << " kW, Ct = " << ct << ", Local TI = " << t.local_ti << "\n";

        if (t.inflow_wind_speed < Real(0.1)) continue;

        // Unit vector of local hub wind direction
        Real u_h_dir = t.u_hub / t.inflow_wind_speed;
        Real v_h_dir = t.v_hub / t.inflow_wind_speed;

        // Calculate stability scaling factor
        Real stability_factor = Real(1.0);
        if (enable_stability && std::abs(L_obukhov) > Real(1.0e-5)) {
            Real zeta = t.hub_height / L_obukhov;
            if (zeta > Real(0.0)) {
                stability_factor = Real(1.0) - Real(0.4) * std::tanh(zeta);
            } else if (zeta < Real(0.0)) {
                stability_factor = Real(1.0) + Real(0.6) * std::tanh(-zeta);
            }
        }

        // Apply wake model to update velocity field
        for (int k = 0; k < nz; ++k) {
            Real z_phys = zmin + (k + Real(0.5)) * dz;
            for (int j = 0; j < ny; ++j) {
                Real y_phys = ymin + (j + Real(0.5)) * dy;
                for (int i = 0; i < nx; ++i) {
                    Real x_phys = xmin + (i + Real(0.5)) * dx;

                    Real dx_pt = x_phys - t.x;
                    Real dy_pt = y_phys - t.y;

                    // Use local wind direction at each vertical grid level (z)
                    Real u_local = interpolate_3d(u_host, t.x, t.y, z_phys, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz);
                    Real v_local = interpolate_3d(v_host, t.x, t.y, z_phys, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz);
                    Real speed_local = std::sqrt(u_local * u_local + v_local * v_local);
                    Real u_z_dir = u_h_dir;
                    Real v_z_dir = v_h_dir;
                    if (speed_local > Real(1.0e-5)) {
                       u_z_dir = u_local / speed_local;
                       v_z_dir = v_local / speed_local;
                    }

                    // Downwind distance
                    Real x_down = dx_pt * u_z_dir + dy_pt * v_z_dir;
                    if (x_down <= Real(1.0e-3)) continue; // upstream or exactly at the turbine

                    // Crosswind distance with optional deflection
                    Real y_cross_signed = -dx_pt * v_z_dir + dy_pt * u_z_dir;
                    Real y_offset = 0.0;
                    if (enable_bastankhah_deflection && std::abs(t.yaw) > 1e-5) {
                       y_offset = compute_bastankhah_deflection(
                           t.yaw, t.rotor_diameter, t.inflow_wind_speed,
                           ct, t.local_ti, x_down,
                           gaussian_ka_base, stability_factor, L_obukhov, enable_stability
                       );
                    } else if (enable_jimenez_deflection && std::abs(t.yaw) > 1e-5) {
                       Real gamma_rad = t.yaw * M_PI / 180.0;
                       Real theta_0 = (ct / Real(2.0)) * std::cos(gamma_rad) * std::cos(gamma_rad) * std::sin(gamma_rad);
                       Real beta_def = jimenez_kd_base;
                       if (beta_def < Real(1e-5)) beta_def = Real(1e-5);
                       y_offset = t.rotor_diameter * theta_0 * (Real(1.0) / (Real(2.0) * beta_def)) * 
                                     (Real(1.0) - Real(1.0) / (Real(1.0) + Real(2.0) * beta_def * x_down / t.rotor_diameter));
                    }
                    Real y_cross = std::abs(y_cross_signed - y_offset);

                    // Terrain-aware vertical distance:
                    // centerline follows terrain keeping height t.hub_height above local terrain
                    Real cell_terrain = terrain_host[static_cast<std::size_t>(j) * nx + i];
                    Real z_agl = z_phys - cell_terrain;
                    Real z_offset = 0.0;
                    if (std::abs(t.tilt) > 1e-5) {
                        Real tilt_rad = t.tilt * M_PI / 180.0;
                        Real theta_v0 = (ct / Real(2.0)) * std::cos(tilt_rad) * std::cos(tilt_rad) * std::sin(tilt_rad);
                        Real beta_def = jimenez_kd_base;
                        if (beta_def < Real(1e-5)) beta_def = Real(1e-5);
                        z_offset = t.rotor_diameter * theta_v0 * (Real(1.0) / (Real(2.0) * beta_def)) * 
                                      (Real(1.0) - Real(1.0) / (Real(1.0) + Real(2.0) * beta_def * x_down / t.rotor_diameter));
                    }
                    Real z_vertical = z_agl - (t.hub_height + z_offset);

                    Real r = std::sqrt(y_cross * y_cross + z_vertical * z_vertical);

                    Real deficit = 0.0;
                    Real deficit_real = 0.0;
                    Real deficit_mirror = 0.0;

                    // Helper to compute deficit for a given distance r
                    auto compute_deficit_for_r = [&](Real radial_distance) -> Real {
                        Real d = 0.0;
                        if (model_type == TurbineWakeModelType::JENSEN) {
                            Real kw = jensen_kw_base * stability_factor;
                            Real r_wake = t.rotor_diameter / Real(2.0) + kw * x_down;
                            if (radial_distance <= r_wake) {
                                d = (Real(1.0) - std::sqrt(Real(1.0) - ct)) / 
                                          std::pow(Real(1.0) + Real(2.0) * kw * x_down / t.rotor_diameter, 2);
                            }
                        } else if (model_type == TurbineWakeModelType::BASTANKHAH_GAUSSIAN ||
                                   model_type == TurbineWakeModelType::GAUSS_CURL_HYBRID) {
                            Real ka = gaussian_ka_base * stability_factor;
                            Real ct_clamped = std::max(Real(0.0), std::min(ct, Real(0.99)));
                            Real sqrt_one_minus_ct = std::sqrt(Real(1.0) - ct_clamped);
                            Real beta = Real(0.5) * (Real(1.0) + sqrt_one_minus_ct) / sqrt_one_minus_ct;
                            Real epsilon = Real(0.2) * std::sqrt(beta);
                            Real sigma = ka * x_down + epsilon * t.rotor_diameter;

                            Real term = Real(1.0) - ct_clamped / (Real(8.0) * (sigma / t.rotor_diameter) * (sigma / t.rotor_diameter));
                            Real max_deficit = 0.0;
                            if (term > Real(0.0)) {
                                max_deficit = Real(1.0) - std::sqrt(term);
                            } else {
                                max_deficit = Real(1.0) - sqrt_one_minus_ct;
                            }
                            d = max_deficit * std::exp(-radial_distance * radial_distance / (Real(2.0) * sigma * sigma));
                        } else if (model_type == TurbineWakeModelType::TURBOPARK) {
                            Real ka = turbopark_c1_base * t.local_ti * stability_factor;
                            Real sigma_0 = Real(0.25) * t.rotor_diameter;
                            Real sigma = sigma_0 + ka * x_down;

                            Real ct_clamped = std::max(Real(0.0), std::min(ct, Real(0.99)));
                            Real term = Real(1.0) - ct_clamped / (Real(8.0) * (sigma / t.rotor_diameter) * (sigma / t.rotor_diameter));
                            Real max_deficit = 0.0;
                            if (term > Real(0.0)) {
                                max_deficit = Real(1.0) - std::sqrt(term);
                            } else {
                                max_deficit = Real(1.0) - std::sqrt(Real(1.0) - ct_clamped);
                            }
                            d = max_deficit * std::exp(-radial_distance * radial_distance / (Real(2.0) * sigma * sigma));
                        }
                        return d;
                    };

                    deficit_real = compute_deficit_for_r(r);
                    
                    if (enable_wake_ground_interaction) {
                        Real r_mirror = std::sqrt(y_cross * y_cross + (z_agl + t.hub_height + z_offset) * (z_agl + t.hub_height + z_offset));
                        deficit_mirror = compute_deficit_for_r(r_mirror);

                        Real d_scale = wake_ground_damping_scale * t.rotor_diameter;
                        if (d_scale < Real(1e-5)) d_scale = Real(1e-5);
                        Real f_damp = Real(1.0) - std::exp(-z_agl / d_scale);
                        deficit_real *= f_damp;
                        deficit_mirror *= f_damp;

                        if (superposition == SuperpositionType::QUADRATIC) {
                            deficit = std::sqrt(deficit_real * deficit_real + deficit_mirror * deficit_mirror);
                        } else {
                            deficit = deficit_real + deficit_mirror;
                        }
                    } else {
                        deficit = deficit_real;
                    }

                    std::size_t idx = static_cast<std::size_t>(i) 
                                    + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(j) 
                                    + static_cast<std::size_t>(ny) * k);

                    if (superposition == SuperpositionType::QUADRATIC) {
                        cum_deficit_sq[idx] += deficit * deficit;
                    } else if (superposition == SuperpositionType::MAX) {
                        max_deficit_vec[idx] = std::max(max_deficit_vec[idx], deficit);
                    } else {
                        // Linear superposition: direct speed reduction
                        u_host[idx] *= (Real(1.0) - deficit);
                        v_host[idx] *= (Real(1.0) - deficit);
                    }

                    // Add GCH secondary steering vortex velocities if GCH model is active and yaw is non-zero
                    if (model_type == TurbineWakeModelType::GAUSS_CURL_HYBRID && std::abs(t.yaw) > 1e-5) {
                        Real gamma_rad = t.yaw * M_PI / 180.0;
                        Real U_inf = t.inflow_wind_speed;
                        Real D = t.rotor_diameter;
                        
                        // Initial vortex circulation strength
                        Real gamma_strength = Real(0.5) * ct * D * U_inf * std::cos(gamma_rad) * std::cos(gamma_rad) * std::sin(gamma_rad);
                        
                        // Decay downstream
                        Real gamma_strength_decayed = gamma_strength * std::exp(-Real(0.1) * x_down / D);
                        
                        // Vortex core size
                        Real epsilon_core = Real(0.2) * D;
                        Real epsilon_core_sq = epsilon_core * epsilon_core;
                        
                        // Physical core positions
                        Real y_v1 = y_offset;
                        Real z_v1 = t.hub_height + D / Real(4.0);
                        Real y_v2 = y_offset;
                        Real z_v2 = t.hub_height - D / Real(4.0);
                        
                        // Distances to physical cores
                        Real d1_sq = (y_cross_signed - y_v1) * (y_cross_signed - y_v1) + (z_agl - z_v1) * (z_agl - z_v1);
                        Real d2_sq = (y_cross_signed - y_v2) * (y_cross_signed - y_v2) + (z_agl - z_v2) * (z_agl - z_v2);
                        
                        // Circulation signs: top clockwise (-), bottom counter-clockwise (+)
                        Real g_top = -gamma_strength_decayed;
                        Real g_bottom = +gamma_strength_decayed;
                        
                        // Physical vortex velocities
                        Real v_v1 = g_top * (z_agl - z_v1) / (Real(2.0 * M_PI) * (d1_sq + epsilon_core_sq));
                        Real w_v1 = -g_top * (y_cross_signed - y_v1) / (Real(2.0 * M_PI) * (d1_sq + epsilon_core_sq));
                        
                        Real v_v2 = g_bottom * (z_agl - z_v2) / (Real(2.0 * M_PI) * (d2_sq + epsilon_core_sq));
                        Real w_v2 = -g_bottom * (y_cross_signed - y_v2) / (Real(2.0 * M_PI) * (d2_sq + epsilon_core_sq));
                        
                        Real v_vortex = v_v1 + v_v2;
                        Real w_vortex = w_v1 + w_v2;
                        
                        // Add mirror vortices below ground to enforce zero-normal-flow (w=0) at z_agl=0
                        if (enable_wake_ground_interaction) {
                            Real d1_mirror_sq = (y_cross_signed - y_v1) * (y_cross_signed - y_v1) + (z_agl + z_v1) * (z_agl + z_v1);
                            Real d2_mirror_sq = (y_cross_signed - y_v2) * (y_cross_signed - y_v2) + (z_agl + z_v2) * (z_agl + z_v2);
                            
                            Real v_v1_m = (-g_top) * (z_agl + z_v1) / (Real(2.0 * M_PI) * (d1_mirror_sq + epsilon_core_sq));
                            Real w_v1_m = -(-g_top) * (y_cross_signed - y_v1) / (Real(2.0 * M_PI) * (d1_mirror_sq + epsilon_core_sq));
                            
                            Real v_v2_m = (-g_bottom) * (z_agl + z_v2) / (Real(2.0 * M_PI) * (d2_mirror_sq + epsilon_core_sq));
                            Real w_v2_m = -(-g_bottom) * (y_cross_signed - y_v2) / (Real(2.0 * M_PI) * (d2_mirror_sq + epsilon_core_sq));
                            
                            v_vortex += (v_v1_m + v_v2_m);
                            w_vortex += (w_v1_m + w_v2_m);
                        }
                        
                        // Apply shear damping
                        Real d_scale = wake_ground_damping_scale * D;
                        if (d_scale < Real(1e-5)) d_scale = Real(1e-5);
                        Real f_damp = Real(1.0) - std::exp(-z_agl / d_scale);
                        v_vortex *= f_damp;
                        w_vortex *= f_damp;
                        
                        // Convert to global Cartesian velocities
                        Real u_vortex_global = v_vortex * (-v_z_dir);
                        Real v_vortex_global = v_vortex * u_z_dir;
                        Real w_vortex_global = w_vortex;
                        
                        // Accumulate and add to flow field
                        cum_u_vortex[idx] += u_vortex_global;
                        cum_v_vortex[idx] += v_vortex_global;
                        
                        u_host[idx] += u_vortex_global;
                        v_host[idx] += v_vortex_global;
                        w_host[idx] += w_vortex_global;
                    }
                }
            }
        }

        // If using quadratic/max superposition, update immediately after each turbine
        if (superposition == SuperpositionType::QUADRATIC || superposition == SuperpositionType::MAX) {
            for (std::size_t idx = 0; idx < u_host.size(); ++idx) {
                Real comb_deficit = 0.0;
                if (superposition == SuperpositionType::QUADRATIC) {
                    comb_deficit = std::sqrt(cum_deficit_sq[idx]);
                } else {
                    comb_deficit = max_deficit_vec[idx];
                }
                comb_deficit = std::min(comb_deficit, Real(0.99));
                u_host[idx] = u_bg[idx] * (Real(1.0) - comb_deficit) + cum_u_vortex[idx];
                v_host[idx] = v_bg[idx] * (Real(1.0) - comb_deficit) + cum_v_vortex[idx];
            }
        }
    }

    // Output power to standard output and log it into a dedicated CSV file (`turbine_power_output.csv`)
    std::string csv_filename = "turbine_power_output.csv";
    std::ofstream csv_file;
    if (time_step == 0) {
        csv_file.open(csv_filename, std::ios::out); // Overwrite if first step
        csv_file << "turbine_id,x,y,z_terrain,z_hub,u_ms,v_ms,speed_ms,direction_deg,power_kw,local_ti,time_step\n";
    } else {
        csv_file.open(csv_filename, std::ios::app); // Append for subsequent steps
    }

    if (csv_file.is_open()) {
        for (const auto& t : turbines) {
            csv_file << t.id << ","
                     << std::fixed << std::setprecision(2) << t.x << ","
                     << t.y << ","
                     << t.z_terrain << ","
                     << t.z_hub << ","
                     << std::setprecision(4) << t.u_hub << ","
                     << t.v_hub << ","
                     << t.inflow_wind_speed << ","
                     << std::setprecision(2) << t.direction_deg << ","
                     << std::setprecision(2) << t.power_output << ","
                     << std::setprecision(4) << t.local_ti << ","
                     << time_step << "\n";
        }
        csv_file.close();
        amrex::Print() << "wind_solver: logged turbine power output to " << csv_filename << "\n";
    }
}

// Overloaded helper function that handles copying MultiFab to host, applying wakes, and copying back.
inline void apply_turbine_wakes_to_multifab(
    amrex::MultiFab& vel0,
    const std::vector<Real>& terrain_host,
    std::vector<Turbine>& turbines,
    TurbineWakeModelType model_type,
    SuperpositionType superposition,
    Real jensen_kw_base,
    Real gaussian_ka_base,
    bool enable_stability,
    Real L_obukhov,
    Real xmin, Real ymin, Real zmin,
    Real dx, Real dy, Real dz,
    int nx, int ny, int nz,
    Real turbopark_c1_base = 0.38,
    Real ambient_ti_base = 0.075,
    bool enable_jimenez_deflection = false,
    Real jimenez_kd_base = 0.05,
    bool enable_bastankhah_deflection = false,
    WakeAddedTurbulenceModelType wake_added_turb_model = WakeAddedTurbulenceModelType::NONE,
    int time_step = 0,
    bool enable_wake_ground_interaction = true,
    Real wake_ground_damping_scale = 0.25)
{
    if (turbines.empty()) return;

    // 1. Copy vel0 components 0, 1 and 2 to host vectors
    std::vector<Real> u_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);
    std::vector<Real> v_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);
    std::vector<Real> w_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);

    // Extract velocities
    for (amrex::MFIter mfi(vel0, false); mfi.isValid(); ++mfi) {
        const amrex::Box& bx = mfi.validbox();
#ifdef AMREX_USE_GPU
        amrex::FArrayBox host_fab(bx, vel0.nComp(), amrex::The_Pinned_Arena());
        host_fab.copy<amrex::RunOn::Device>(vel0[mfi], bx);
        amrex::Gpu::streamSynchronize();
        auto const& arr = host_fab.const_array();
#else
        auto const& arr = vel0.const_array(mfi);
#endif
        for (int k = bx.smallEnd(2); k <= bx.bigEnd(2); ++k) {
            for (int j = bx.smallEnd(1); j <= bx.bigEnd(1); ++j) {
                for (int i = bx.smallEnd(0); i <= bx.bigEnd(0); ++i) {
                    std::size_t idx = static_cast<std::size_t>(i) 
                                    + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(j) 
                                    + static_cast<std::size_t>(ny) * k);
                    u_host[idx] = arr(i, j, k, 0);
                    v_host[idx] = arr(i, j, k, 1);
                    w_host[idx] = arr(i, j, k, 2);
                }
            }
        }
    }

    // Call apply_analytical_turbine_wakes
    apply_analytical_turbine_wakes(
        u_host, v_host, w_host, terrain_host, turbines,
        model_type, superposition, jensen_kw_base, gaussian_ka_base,
        enable_stability, L_obukhov, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz,
        turbopark_c1_base, ambient_ti_base, enable_jimenez_deflection,
        jimenez_kd_base, enable_bastankhah_deflection, wake_added_turb_model, time_step,
        enable_wake_ground_interaction, wake_ground_damping_scale
    );

    // 4. Write back updated u, v, w host vectors to vel0 MultiFab on device/host
    for (amrex::MFIter mfi(vel0, false); mfi.isValid(); ++mfi) {
        const amrex::Box& bx = mfi.validbox();
        auto arr = vel0.array(mfi);
        
        amrex::FArrayBox host_fab(bx, vel0.nComp());
        auto const& h_arr = host_fab.array();
        
        for (int k = bx.smallEnd(2); k <= bx.bigEnd(2); ++k) {
            for (int j = bx.smallEnd(1); j <= bx.bigEnd(1); ++j) {
                for (int i = bx.smallEnd(0); i <= bx.bigEnd(0); ++i) {
                    std::size_t idx = static_cast<std::size_t>(i) 
                                    + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(j) 
                                    + static_cast<std::size_t>(ny) * k);
                    h_arr(i, j, k, 0) = u_host[idx];
                    h_arr(i, j, k, 1) = v_host[idx];
                    h_arr(i, j, k, 2) = w_host[idx];
                }
            }
        }
        
#ifdef AMREX_USE_GPU
        vel0[mfi].copy<amrex::RunOn::Device>(host_fab, bx);
#else
        vel0[mfi].copy(host_fab, bx);
#endif
    }
    vel0.FillBoundary();
}

// Overloaded helper function that handles copying MultiFab to host, applying wakes, and copying back.
inline void apply_turbine_wakes_to_multifab(
    amrex::MultiFab& vel0,
    const amrex::MultiFab& terrain,
    std::vector<Turbine>& turbines,
    TurbineWakeModelType model_type,
    SuperpositionType superposition,
    Real jensen_kw_base,
    Real gaussian_ka_base,
    bool enable_stability,
    Real L_obukhov,
    Real xmin, Real ymin, Real zmin,
    Real dx, Real dy, Real dz,
    int nx, int ny, int nz,
    Real turbopark_c1_base = 0.38,
    Real ambient_ti_base = 0.075,
    bool enable_jimenez_deflection = false,
    Real jimenez_kd_base = 0.05,
    bool enable_bastankhah_deflection = false,
    WakeAddedTurbulenceModelType wake_added_turb_model = WakeAddedTurbulenceModelType::NONE,
    int time_step = 0,
    bool enable_wake_ground_interaction = true,
    Real wake_ground_damping_scale = 0.25)
{
    if (turbines.empty()) return;

    // Extract terrain
    std::vector<Real> terrain_host(static_cast<std::size_t>(nx) * ny, 0.0);
    for (amrex::MFIter mfi(terrain, false); mfi.isValid(); ++mfi) {
        const amrex::Box& bx = mfi.validbox();
#ifdef AMREX_USE_GPU
        amrex::FArrayBox host_fab(bx, terrain.nComp(), amrex::The_Pinned_Arena());
        host_fab.copy<amrex::RunOn::Device>(terrain[mfi], bx);
        amrex::Gpu::streamSynchronize();
        auto const& arr = host_fab.const_array();
#else
        auto const& arr = terrain.const_array(mfi);
#endif
        for (int j = bx.smallEnd(1); j <= bx.bigEnd(1); ++j) {
            for (int i = bx.smallEnd(0); i <= bx.bigEnd(0); ++i) {
                std::size_t idx = static_cast<std::size_t>(i) + static_cast<std::size_t>(nx) * j;
                terrain_host[idx] = arr(i, j, 0);
            }
        }
    }

    apply_turbine_wakes_to_multifab(
        vel0, terrain_host, turbines, model_type, superposition, jensen_kw_base, gaussian_ka_base,
        enable_stability, L_obukhov, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz,
        turbopark_c1_base, ambient_ti_base, enable_jimenez_deflection, jimenez_kd_base,
        enable_bastankhah_deflection, wake_added_turb_model, time_step,
        enable_wake_ground_interaction, wake_ground_damping_scale
    );
}

} // namespace TurbineWake

#endif
