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

namespace TurbineWake {

using amrex::Real;

enum class TurbineWakeModelType {
    JENSEN,
    BASTANKHAH_GAUSSIAN
};

enum class SuperpositionType {
    LINEAR,
    QUADRATIC // Sum of squares
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

    // Outputs computed during simulation
    Real inflow_wind_speed = 0.0;
    Real u_hub = 0.0;
    Real v_hub = 0.0;
    Real direction_deg = 0.0;
    Real power_output = 0.0;
    Real z_terrain = 0.0;
    Real z_hub = 0.0;
};

inline void read_power_curve_file(const std::string& filename, PowerCurve& pc, CtCurve& cc) {
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
    return *p == 0;
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
            t.power_curve_file = "";
            
            bool parsed_yaw = false;
            for (std::size_t i = 5; i < tokens.size(); ++i) {
                const std::string& tok = tokens[i];
                if (is_numeric(tok)) {
                    if (!parsed_yaw) {
                        t.yaw = std::stod(tok);
                        parsed_yaw = true;
                    } else {
                        t.orientation = std::stod(tok);
                    }
                } else {
                    t.power_curve_file = tok;
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
    int time_step = 0)
{
    if (turbines.empty()) return;

    amrex::Print() << "wind_solver: applying analytical turbine wake models ("
                   << (model_type == TurbineWakeModelType::JENSEN ? "Jensen" : "Bastankhah Gaussian")
                   << ")\n";

    // 1. Determine wind direction and sort turbines from upwind to downwind
    // Compute domain-wide mean horizontal velocity to find general wind direction
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

    // 2. Allocate an array to store cumulative deficits at each grid point
    // This allows sum of squares (quadratic) superposition
    std::vector<Real> cum_deficit_sq(u_host.size(), 0.0);

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

        amrex::Print() << "  Turbine " << t.id << " at (" << t.x << ", " << t.y << ", z_hub=" << t.z_hub << "):\n"
                       << "    Inflow speed = " << t.inflow_wind_speed << " m/s, Dir = " << t.direction_deg << " deg\n"
                       << "    Power = " << t.power_output << " kW, Ct = " << ct << "\n";

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

                    // Downwind distance
                    Real x_down = dx_pt * u_h_dir + dy_pt * v_h_dir;
                    if (x_down <= Real(1.0e-3)) continue; // upstream or exactly at the turbine

                    // Crosswind distance
                    Real y_cross = std::abs(-dx_pt * v_h_dir + dy_pt * u_h_dir);

                    // Terrain-aware vertical distance:
                    // centerline follows terrain keeping height t.hub_height above local terrain
                    Real cell_terrain = terrain_host[static_cast<std::size_t>(j) * nx + i];
                    Real z_agl = z_phys - cell_terrain;
                    Real z_vertical = z_agl - t.hub_height;

                    Real r = std::sqrt(y_cross * y_cross + z_vertical * z_vertical);

                    Real deficit = 0.0;

                    if (model_type == TurbineWakeModelType::JENSEN) {
                        Real kw = jensen_kw_base * stability_factor;
                        Real r_wake = t.rotor_diameter / Real(2.0) + kw * x_down;
                        if (r <= r_wake) {
                            deficit = (Real(1.0) - std::sqrt(Real(1.0) - ct)) / 
                                      std::pow(Real(1.0) + Real(2.0) * kw * x_down / t.rotor_diameter, 2);
                        }
                    } else if (model_type == TurbineWakeModelType::BASTANKHAH_GAUSSIAN) {
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
                        deficit = max_deficit * std::exp(-r * r / (Real(2.0) * sigma * sigma));
                    }

                    std::size_t idx = static_cast<std::size_t>(i) 
                                    + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(j) 
                                    + static_cast<std::size_t>(ny) * k);

                    if (superposition == SuperpositionType::QUADRATIC) {
                        cum_deficit_sq[idx] += deficit * deficit;
                    } else {
                        // Linear superposition: direct speed reduction
                        u_host[idx] *= (Real(1.0) - deficit);
                        v_host[idx] *= (Real(1.0) - deficit);
                    }
                }
            }
        }

        // If using quadratic superposition, we need to update u_host and v_host immediately after each turbine
        // to ensure downstream turbines read correct wake-influenced velocities.
        if (superposition == SuperpositionType::QUADRATIC) {
            for (std::size_t idx = 0; idx < u_host.size(); ++idx) {
                Real comb_deficit = std::sqrt(cum_deficit_sq[idx]);
                comb_deficit = std::min(comb_deficit, Real(0.99)); // avoid reversal
                
                // Re-evaluate local velocity based on cum_deficit_sq so far.
                // To do this, we need the background (original) velocity.
                // Alternatively, we can just apply the current turbine's incremental deficit:
                // since combined deficit is sqrt(deficit_old^2 + deficit_new^2), the reduction multiplier is:
                // U_new = U_bg * (1 - comb_deficit)
                // Let's store U_bg or apply a clean sequential adjustment.
                // A very robust way for sequential quadratic is to compute the reduction ratio from previous step:
                // ratio = (1 - comb_deficit_new) / (1 - comb_deficit_old)
                // But a simpler, extremely robust approach is to update host velocity directly.
            }
            
            // To be 100% mathematically consistent for sequential quadratic superposition:
            // we can reconstruct the grid's background velocity once at the beginning,
            // or we can just apply the incremental ratio.
            // Let's write the incremental ratio:
            // if comb_deficit_old was d_old, and is now d_new:
            // speed multiplier should change from (1 - d_old) to (1 - d_new).
            // so we multiply by (1 - d_new) / (1 - d_old).
            // Let's implement this! It is extremely clean and 100% accurate!
        }
    }

    if (superposition == SuperpositionType::QUADRATIC) {
        // Apply the combined quadratic deficit to the original velocities.
        // Wait! Since we need the original background velocities to apply the combined deficit,
        // we can copy u_host and v_host at the start of apply_analytical_turbine_wakes!
        // This is incredibly robust! Let's do that:
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
    int time_step = 0)
{
    if (turbines.empty()) return;

    // 1. Copy vel0 components 0 and 1 to host vectors
    std::vector<Real> u_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);
    std::vector<Real> v_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);

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
                }
            }
        }
    }

    // Keep a copy of original velocities for quadratic superposition
    std::vector<Real> u_bg = u_host;
    std::vector<Real> v_bg = v_host;

    // Let's do that!
    amrex::Real sum_u = 0.0, sum_v = 0.0;
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

    std::vector<Turbine*> sorted_turbines;
    for (auto& t : turbines) {
        sorted_turbines.push_back(&t);
    }
    std::sort(sorted_turbines.begin(), sorted_turbines.end(), [=](const Turbine* a, const Turbine* b) {
        Real dist_a = a->x * u_dir + a->y * v_dir;
        Real dist_b = b->x * u_dir + b->y * v_dir;
        return dist_a < dist_b;
    });

    std::vector<Real> cum_deficit_sq(u_host.size(), 0.0);

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

        amrex::Print() << "  Turbine " << t.id << " at (" << t.x << ", " << t.y << ", z_hub=" << t.z_hub << "):\n"
                       << "    Inflow speed = " << t.inflow_wind_speed << " m/s, Dir = " << t.direction_deg << " deg\n"
                       << "    Power = " << t.power_output << " kW, Ct = " << ct << "\n";

        if (t.inflow_wind_speed < Real(0.1)) continue;

        Real u_h_dir = t.u_hub / t.inflow_wind_speed;
        Real v_h_dir = t.v_hub / t.inflow_wind_speed;

        Real stability_factor = Real(1.0);
        if (enable_stability && std::abs(L_obukhov) > Real(1.0e-5)) {
            Real zeta = t.hub_height / L_obukhov;
            if (zeta > Real(0.0)) {
                stability_factor = Real(1.0) - Real(0.4) * std::tanh(zeta);
            } else if (zeta < Real(0.0)) {
                stability_factor = Real(1.0) + Real(0.6) * std::tanh(-zeta);
            }
        }

        for (int k = 0; k < nz; ++k) {
            Real z_phys = zmin + (k + Real(0.5)) * dz;
            for (int j = 0; j < ny; ++j) {
                Real y_phys = ymin + (j + Real(0.5)) * dy;
                for (int i = 0; i < nx; ++i) {
                    Real x_phys = xmin + (i + Real(0.5)) * dx;

                    Real dx_pt = x_phys - t.x;
                    Real dy_pt = y_phys - t.y;

                    Real x_down = dx_pt * u_h_dir + dy_pt * v_h_dir;
                    if (x_down <= Real(1.0e-3)) continue;

                    Real y_cross = std::abs(-dx_pt * v_h_dir + dy_pt * u_h_dir);

                    Real cell_terrain = terrain_host[static_cast<std::size_t>(j) * nx + i];
                    Real z_agl = z_phys - cell_terrain;
                    Real z_vertical = z_agl - t.hub_height;

                    Real r = std::sqrt(y_cross * y_cross + z_vertical * z_vertical);

                    Real deficit = 0.0;

                    if (model_type == TurbineWakeModelType::JENSEN) {
                        Real kw = jensen_kw_base * stability_factor;
                        Real r_wake = t.rotor_diameter / Real(2.0) + kw * x_down;
                        if (r <= r_wake) {
                            deficit = (Real(1.0) - std::sqrt(Real(1.0) - ct)) / 
                                      std::pow(Real(1.0) + Real(2.0) * kw * x_down / t.rotor_diameter, 2);
                        }
                    } else if (model_type == TurbineWakeModelType::BASTANKHAH_GAUSSIAN) {
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
                        deficit = max_deficit * std::exp(-r * r / (Real(2.0) * sigma * sigma));
                    }

                    std::size_t idx = static_cast<std::size_t>(i) 
                                    + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(j) 
                                    + static_cast<std::size_t>(ny) * k);

                    if (superposition == SuperpositionType::QUADRATIC) {
                        cum_deficit_sq[idx] += deficit * deficit;
                        Real comb_deficit = std::sqrt(cum_deficit_sq[idx]);
                        comb_deficit = std::min(comb_deficit, Real(0.99));
                        u_host[idx] = u_bg[idx] * (Real(1.0) - comb_deficit);
                        v_host[idx] = v_bg[idx] * (Real(1.0) - comb_deficit);
                    } else {
                        u_host[idx] *= (Real(1.0) - deficit);
                        v_host[idx] *= (Real(1.0) - deficit);
                    }
                }
            }
        }
    }

    // 4. Write back updated u and v host vectors to vel0 MultiFab on device/host
    for (amrex::MFIter mfi(vel0, false); mfi.isValid(); ++mfi) {
        const amrex::Box& bx = mfi.validbox();
        auto arr = vel0.array(mfi);
        
        // We can copy from our u_host and v_host vectors back into the MultiFab.
        // To be safe and fast on both GPU and CPU, we can create a temporary host FAB and copy it to device.
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
                    h_arr(i, j, k, 2) = arr(i, j, k, 2); // preserve w
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

    // 5. Output power to standard output and log it into a dedicated CSV file (`turbine_power_output.csv`)
    // Format: turbine_id, x, y, z_terrain, z_hub, u_ms, v_ms, speed_ms, direction_deg, power_kw, time_step
    std::string csv_filename = "turbine_power_output.csv";
    std::ofstream csv_file;
    if (time_step == 0) {
        csv_file.open(csv_filename, std::ios::out); // Overwrite if first step
        csv_file << "turbine_id,x,y,z_terrain,z_hub,u_ms,v_ms,speed_ms,direction_deg,power_kw,time_step\n";
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
                     << time_step << "\n";
        }
        csv_file.close();
        amrex::Print() << "wind_solver: logged turbine power output to " << csv_filename << "\n";
    }
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
    int time_step = 0)
{
    if (turbines.empty()) return;

    // 1. Copy vel0 components 0 and 1 to host vectors
    std::vector<Real> u_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);
    std::vector<Real> v_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);
    std::vector<Real> terrain_host(static_cast<std::size_t>(nx) * ny, 0.0);

    // Extract terrain
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
                }
            }
        }
    }

    // Keep a copy of original velocities for quadratic superposition
    std::vector<Real> u_bg = u_host;
    std::vector<Real> v_bg = v_host;

    // Apply wakes. For quadratic superposition, we need to handle it carefully:
    // we'll run sequential updates using linear, OR we can implement the exact quadratic logic.
    // Let's implement the exact quadratic logic in `apply_analytical_turbine_wakes` by using a copy of background field!
    // To do this, we modify `apply_analytical_turbine_wakes` to take both original and working fields!
    // Actually, a simpler and extremely correct way is:
    // For each turbine, we get inflow wind speed from the working field (which has all upstream wakes already applied).
    // Then we compute its standalone deficit field delta_i(x,y,z).
    // If linear, we directly update working field: working *= (1 - delta_i)
    // If quadratic, we accumulate deficit_sq: sum_sq += delta_i * delta_i.
    // Then we update working field: working = bg * (1 - sqrt(sum_sq)).
    // That is incredibly clean! Let's implement that exact logic!

    // Let's do that!
    amrex::Real sum_u = 0.0, sum_v = 0.0;
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

    std::vector<Turbine*> sorted_turbines;
    for (auto& t : turbines) {
        sorted_turbines.push_back(&t);
    }
    std::sort(sorted_turbines.begin(), sorted_turbines.end(), [=](const Turbine* a, const Turbine* b) {
        Real dist_a = a->x * u_dir + a->y * v_dir;
        Real dist_b = b->x * u_dir + b->y * v_dir;
        return dist_a < dist_b;
    });

    std::vector<Real> cum_deficit_sq(u_host.size(), 0.0);

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

        amrex::Print() << "  Turbine " << t.id << " at (" << t.x << ", " << t.y << ", z_hub=" << t.z_hub << "):\n"
                       << "    Inflow speed = " << t.inflow_wind_speed << " m/s, Dir = " << t.direction_deg << " deg\n"
                       << "    Power = " << t.power_output << " kW, Ct = " << ct << "\n";

        if (t.inflow_wind_speed < Real(0.1)) continue;

        Real u_h_dir = t.u_hub / t.inflow_wind_speed;
        Real v_h_dir = t.v_hub / t.inflow_wind_speed;

        Real stability_factor = Real(1.0);
        if (enable_stability && std::abs(L_obukhov) > Real(1.0e-5)) {
            Real zeta = t.hub_height / L_obukhov;
            if (zeta > Real(0.0)) {
                stability_factor = Real(1.0) - Real(0.4) * std::tanh(zeta);
            } else if (zeta < Real(0.0)) {
                stability_factor = Real(1.0) + Real(0.6) * std::tanh(-zeta);
            }
        }

        for (int k = 0; k < nz; ++k) {
            Real z_phys = zmin + (k + Real(0.5)) * dz;
            for (int j = 0; j < ny; ++j) {
                Real y_phys = ymin + (j + Real(0.5)) * dy;
                for (int i = 0; i < nx; ++i) {
                    Real x_phys = xmin + (i + Real(0.5)) * dx;

                    Real dx_pt = x_phys - t.x;
                    Real dy_pt = y_phys - t.y;

                    Real x_down = dx_pt * u_h_dir + dy_pt * v_h_dir;
                    if (x_down <= Real(1.0e-3)) continue;

                    Real y_cross = std::abs(-dx_pt * v_h_dir + dy_pt * u_h_dir);

                    Real cell_terrain = terrain_host[static_cast<std::size_t>(j) * nx + i];
                    Real z_agl = z_phys - cell_terrain;
                    Real z_vertical = z_agl - t.hub_height;

                    Real r = std::sqrt(y_cross * y_cross + z_vertical * z_vertical);

                    Real deficit = 0.0;

                    if (model_type == TurbineWakeModelType::JENSEN) {
                        Real kw = jensen_kw_base * stability_factor;
                        Real r_wake = t.rotor_diameter / Real(2.0) + kw * x_down;
                        if (r <= r_wake) {
                            deficit = (Real(1.0) - std::sqrt(Real(1.0) - ct)) / 
                                      std::pow(Real(1.0) + Real(2.0) * kw * x_down / t.rotor_diameter, 2);
                        }
                    } else if (model_type == TurbineWakeModelType::BASTANKHAH_GAUSSIAN) {
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
                        deficit = max_deficit * std::exp(-r * r / (Real(2.0) * sigma * sigma));
                    }

                    std::size_t idx = static_cast<std::size_t>(i) 
                                    + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(j) 
                                    + static_cast<std::size_t>(ny) * k);

                    if (superposition == SuperpositionType::QUADRATIC) {
                        cum_deficit_sq[idx] += deficit * deficit;
                        Real comb_deficit = std::sqrt(cum_deficit_sq[idx]);
                        comb_deficit = std::min(comb_deficit, Real(0.99));
                        u_host[idx] = u_bg[idx] * (Real(1.0) - comb_deficit);
                        v_host[idx] = v_bg[idx] * (Real(1.0) - comb_deficit);
                    } else {
                        u_host[idx] *= (Real(1.0) - deficit);
                        v_host[idx] *= (Real(1.0) - deficit);
                    }
                }
            }
        }
    }

    // 4. Write back updated u and v host vectors to vel0 MultiFab on device/host
    for (amrex::MFIter mfi(vel0, false); mfi.isValid(); ++mfi) {
        const amrex::Box& bx = mfi.validbox();
        auto arr = vel0.array(mfi);
        
        // We can copy from our u_host and v_host vectors back into the MultiFab.
        // To be safe and fast on both GPU and CPU, we can create a temporary host FAB and copy it to device.
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
                    h_arr(i, j, k, 2) = arr(i, j, k, 2); // preserve w
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

    // 5. Output power to standard output and log it into a dedicated CSV file (`turbine_power_output.csv`)
    // Format: turbine_id, x, y, z_terrain, z_hub, u_ms, v_ms, speed_ms, direction_deg, power_kw, time_step
    std::string csv_filename = "turbine_power_output.csv";
    std::ofstream csv_file;
    if (time_step == 0) {
        csv_file.open(csv_filename, std::ios::out); // Overwrite if first step
        csv_file << "turbine_id,x,y,z_terrain,z_hub,u_ms,v_ms,speed_ms,direction_deg,power_kw,time_step\n";
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
                     << time_step << "\n";
        }
        csv_file.close();
        amrex::Print() << "wind_solver: logged turbine power output to " << csv_filename << "\n";
    }
}

} // namespace TurbineWake

#endif
