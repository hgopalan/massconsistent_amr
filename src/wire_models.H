#ifndef WIRE_MODELS_H
#define WIRE_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_MultiFab.H>
#include <AMReX_Vector.H>
#include <AMReX_Print.H>
#include <AMReX_MFIter.H>
#include <AMReX_ParallelDescriptor.H>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WireLoading {

using amrex::Real;

struct WireSpan {
    int id = 0;
    Real x1 = 0.0, y1 = 0.0, z1 = 0.0;
    Real x2 = 0.0, y2 = 0.0, z2 = 0.0;
    Real diameter = 0.0; // [m]
    Real mass_density = 0.0; // [kg/m]
    Real drag_coeff = 1.0; // [dimensionless]
    Real resistance = 0.0; // [ohm/m]
    Real emissivity = 0.5;
    Real absorptivity = 0.5;
    Real current = 0.0; // [A]

    // Computed outputs:
    Real avg_wind_speed = 0.0;
    Real max_wind_speed = 0.0;
    Real total_drag_force = 0.0; // [N]
    Real max_drag_force_per_m = 0.0; // [N/m]
    Real conductor_temp = 0.0; // [K]
    Real max_capacity = 0.0; // [A]
    Real sway_angle = 0.0; // [degrees]
};

// Simple helper to check if a string is numeric
inline bool is_numeric(const std::string& s) {
    if (s.empty()) return false;
    char* p;
    strtod(s.c_str(), &p);
    if (p == s.c_str()) return false;
    return *p == '\0';
}

inline void read_wires_file(const std::string& filename, std::vector<WireSpan>& wires) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        amrex::Abort("wind_solver: cannot open wires file: " + filename);
    }
    std::string line;
    int id = 0;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        // Skip header lines
        if (line.find("x1") != std::string::npos || line.find("X1") != std::string::npos || line.find("span_id") != std::string::npos) continue;
        
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (ss >> token) {
            tokens.push_back(token);
        }
        
        // Expected columns:
        // span_id, x1, y1, z1, x2, y2, z2, diameter, mass_density, drag_coeff, resistance, emissivity, absorptivity, current
        // Or without span_id (13 columns)
        if (tokens.size() >= 13) {
            int offset = 0;
            if (tokens.size() == 14 || !is_numeric(tokens[0])) {
                offset = 1;
            }
            Real x1 = std::stod(tokens[offset + 0]);
            Real y1 = std::stod(tokens[offset + 1]);
            Real z1 = std::stod(tokens[offset + 2]);
            Real x2 = std::stod(tokens[offset + 3]);
            Real y2 = std::stod(tokens[offset + 4]);
            Real z2 = std::stod(tokens[offset + 5]);
            Real diameter = std::stod(tokens[offset + 6]);
            Real mass_density = std::stod(tokens[offset + 7]);
            Real drag_coeff = std::stod(tokens[offset + 8]);
            Real resistance = std::stod(tokens[offset + 9]);
            Real emissivity = std::stod(tokens[offset + 10]);
            Real absorptivity = std::stod(tokens[offset + 11]);
            Real current = std::stod(tokens[offset + 12]);
            
            WireSpan w;
            w.id = id++;
            w.x1 = x1;
            w.y1 = y1;
            w.z1 = z1;
            w.x2 = x2;
            w.y2 = y2;
            w.z2 = z2;
            w.diameter = diameter;
            w.mass_density = mass_density;
            w.drag_coeff = drag_coeff;
            w.resistance = resistance;
            w.emissivity = emissivity;
            w.absorptivity = absorptivity;
            w.current = current;
            
            wires.push_back(w);
        }
    }
    amrex::Print() << "wind_solver: loaded " << wires.size() << " wires from " << filename << "\n";
}

// Bisection solver for conductor temperature
inline Real solve_conductor_temp(Real Ta, Real Un, Real diameter, Real resistance, Real emissivity, Real absorptivity, Real solar_rad, Real current) {
    auto heat_balance = [&](Real Ts) {
        Real Tf = Real(0.5) * (Ts + Ta);
        // Sutherland's Law for dynamic viscosity
        Real mu = Real(1.458e-6) * std::pow(Tf, Real(1.5)) / (Tf + Real(110.4));
        // Thermal conductivity of air
        Real kf = Real(2.624e-3) * std::pow(Tf, Real(1.5)) / (Tf + Real(245.4));
        Real nu = mu / Real(1.225);
        
        // Forced convection
        Real Re = (diameter * Real(1.225) * Un) / mu;
        Real qc_forced = Real(0.0);
        if (Re > Real(0.0)) {
            Real qc1 = (Real(1.011) + Real(0.371) * std::pow(Re, Real(0.52))) * kf * (Ts - Ta);
            Real qc2 = Real(0.0119) * std::pow(Re, Real(0.6)) * kf * (Ts - Ta);
            qc_forced = std::max(qc1, qc2);
        }
        
        // Natural convection
        Real beta = Real(1.0) / Tf;
        Real g = Real(9.81);
        Real Pr = Real(0.71);
        Real Gr = (g * beta * std::abs(Ts - Ta) * std::pow(diameter, Real(3))) / (nu * nu);
        Real Nu_nat = Real(0.48) * std::pow(Gr * Pr, Real(0.25));
        Real qc_nat = M_PI * kf * Nu_nat * (Ts - Ta);
        
        Real qc = std::max(qc_forced, qc_nat);
        
        // Radiative cooling
        Real sigma = Real(5.670374419e-8);
        Real qr = M_PI * diameter * emissivity * sigma * (std::pow(Ts, 4) - std::pow(Ta, 4));
        
        // Solar heating
        Real qs = absorptivity * solar_rad * diameter;
        
        // Joule heating
        Real qJ = current * current * resistance;
        
        return qc + qr - qs - qJ;
    };

    Real T_lo = Ta;
    Real T_hi = Ta + Real(300.0);
    for (int iter = 0; iter < 30; ++iter) {
        Real T_mid = Real(0.5) * (T_lo + T_hi);
        if (heat_balance(T_mid) > Real(0.0)) {
            T_hi = T_mid;
        } else {
            T_lo = T_mid;
        }
    }
    return Real(0.5) * (T_lo + T_hi);
}

// Ampacity calculation
inline Real compute_ampacity(Real Ta, Real Un, Real diameter, Real resistance, Real emissivity, Real absorptivity, Real solar_rad, Real T_max_safe = Real(373.15)) {
    if (T_max_safe <= Ta) return Real(0.0);
    
    Real Ts = T_max_safe;
    Real Tf = Real(0.5) * (Ts + Ta);
    Real mu = Real(1.458e-6) * std::pow(Tf, Real(1.5)) / (Tf + Real(110.4));
    Real kf = Real(2.624e-3) * std::pow(Tf, Real(1.5)) / (Tf + Real(245.4));
    Real nu = mu / Real(1.225);
    
    // Forced convection
    Real Re = (diameter * Real(1.225) * Un) / mu;
    Real qc_forced = Real(0.0);
    if (Re > Real(0.0)) {
        Real qc1 = (Real(1.011) + Real(0.371) * std::pow(Re, Real(0.52))) * kf * (Ts - Ta);
        Real qc2 = Real(0.0119) * std::pow(Re, Real(0.6)) * kf * (Ts - Ta);
        qc_forced = std::max(qc1, qc2);
    }
    
    // Natural convection
    Real beta = Real(1.0) / Tf;
    Real g = Real(9.81);
    Real Pr = Real(0.71);
    Real Gr = (g * beta * std::abs(Ts - Ta) * std::pow(diameter, Real(3))) / (nu * nu);
    Real Nu_nat = Real(0.48) * std::pow(Gr * Pr, Real(0.25));
    Real qc_nat = M_PI * kf * Nu_nat * (Ts - Ta);
    
    Real qc = std::max(qc_forced, qc_nat);
    
    // Radiative cooling
    Real sigma = Real(5.670374419e-8);
    Real qr = M_PI * diameter * emissivity * sigma * (std::pow(Ts, 4) - std::pow(Ta, 4));
    
    // Solar heating
    Real qs = absorptivity * solar_rad * diameter;
    
    Real numerator = qc + qr - qs;
    if (numerator <= Real(0.0)) return Real(0.0);
    if (resistance <= Real(0.0)) return Real(9999.0);
    
    return std::sqrt(numerator / resistance);
}

// Main process function to calculate wire loading and ratings using pre-gathered arrays
inline void process_wire_loading_pregathered(
    std::vector<WireSpan>& wires,
    const std::vector<Real>& u_host,
    const std::vector<Real>& v_host,
    const std::vector<Real>& w_host,
    const std::vector<Real>& T_host,
    Real solar_radiation,
    Real xmin, Real ymin, Real zmin,
    Real dx, Real dy, Real dz,
    int nx, int ny, int nz)
{
    if (wires.empty()) return;

    // Compute metrics for each wire span
    for (auto& w : wires) {
        Real dx_span = w.x2 - w.x1;
        Real dy_span = w.y2 - w.y1;
        Real dz_span = w.z2 - w.z1;
        Real L = std::sqrt(dx_span*dx_span + dy_span*dy_span + dz_span*dz_span);
        if (L <= 1.0e-6) continue;

        Real tx = dx_span / L;
        Real ty = dy_span / L;
        Real tz = dz_span / L;

        int num_segs = std::max(1, static_cast<int>(std::ceil(L / std::min(dx, dy))));
        Real ds = L / num_segs;

        Real sum_u = 0.0;
        Real sum_drag = 0.0;
        Real sum_temp = 0.0;
        Real min_ampacity = 1.0e10;
        Real max_u = 0.0;
        Real max_drag_m = 0.0;

        for (int s = 0; s < num_segs; ++s) {
            Real xc = w.x1 + (s + Real(0.5)) * ds * tx;
            Real yc = w.y1 + (s + Real(0.5)) * ds * ty;
            Real zc = w.z1 + (s + Real(0.5)) * ds * tz;

            // Interpolate wind at segment center
            Real u = TurbineWake::interpolate_3d(u_host, xc, yc, zc, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz);
            Real v = TurbineWake::interpolate_3d(v_host, xc, yc, zc, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz);
            Real w_vel = TurbineWake::interpolate_3d(w_host, xc, yc, zc, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz);
            Real Ta = TurbineWake::interpolate_3d(T_host, xc, yc, zc, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz);

            Real speed = std::sqrt(u*u + v*v + w_vel*w_vel);
            sum_u += speed;
            max_u = std::max(max_u, speed);

            // Perpendicular component of wind to span
            Real u_p = u * tx + v * ty + w_vel * tz;
            Real u_n_sq = std::max(Real(0.0), speed*speed - u_p*u_p);
            Real u_n = std::sqrt(u_n_sq);

            // Aerodynamic drag force per meter
            Real Fd = Real(0.5) * Real(1.225) * w.drag_coeff * w.diameter * u_n_sq;
            sum_drag += Fd * ds;
            max_drag_m = std::max(max_drag_m, Fd);

            // Solve steady-state conductor temperature
            Real Ts = solve_conductor_temp(Ta, u_n, w.diameter, w.resistance, w.emissivity, w.absorptivity, solar_radiation, w.current);
            sum_temp += Ts;

            // Calculate Dynamic Line Rating (ampacity)
            Real amp = compute_ampacity(Ta, u_n, w.diameter, w.resistance, w.emissivity, w.absorptivity, solar_radiation);
            min_ampacity = std::min(min_ampacity, amp);
        }

        w.avg_wind_speed = sum_u / num_segs;
        w.max_wind_speed = max_u;
        w.total_drag_force = sum_drag;
        w.max_drag_force_per_m = max_drag_m;
        w.conductor_temp = sum_temp / num_segs;
        w.max_capacity = (min_ampacity > 1.0e9) ? 0.0 : min_ampacity;

        // Sway angle
        Real Fd_avg = sum_drag / L;
        Real Fg = w.mass_density * Real(9.81);
        if (Fg > 0.0) {
            w.sway_angle = std::atan(Fd_avg / Fg) * Real(180.0) / M_PI;
        } else {
            w.sway_angle = 0.0;
        }
    }
}

// Main process function to calculate wire loading and ratings
inline void process_wire_loading(
    std::vector<WireSpan>& wires,
    const amrex::MultiFab& vel,
    const amrex::MultiFab* temp_ptr,
    Real temperature_reference,
    Real solar_radiation,
    Real xmin, Real ymin, Real zmin,
    Real dx, Real dy, Real dz,
    int nx, int ny, int nz)
{
    if (wires.empty()) return;

    // Gather velocities and temperature
    std::vector<Real> u_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);
    std::vector<Real> v_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);
    std::vector<Real> w_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);
    std::vector<Real> T_host(static_cast<std::size_t>(nx) * ny * nz, temperature_reference);

    for (amrex::MFIter mfi(vel, false); mfi.isValid(); ++mfi) {
        const amrex::Box& bx = mfi.validbox();
#ifdef AMREX_USE_GPU
        amrex::FArrayBox host_fab(bx, vel.nComp(), amrex::The_Pinned_Arena());
        host_fab.copy<amrex::RunOn::Device>(vel[mfi], bx);
        amrex::Gpu::streamSynchronize();
        auto const& arr = host_fab.const_array();
#else
        auto const& arr = vel.const_array(mfi);
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

    if (temp_ptr) {
        for (amrex::MFIter mfi(*temp_ptr, false); mfi.isValid(); ++mfi) {
            const amrex::Box& bx = mfi.validbox();
#ifdef AMREX_USE_GPU
            amrex::FArrayBox host_fab(bx, temp_ptr->nComp(), amrex::The_Pinned_Arena());
            host_fab.copy<amrex::RunOn::Device>((*temp_ptr)[mfi], bx);
            amrex::Gpu::streamSynchronize();
            auto const& arr = host_fab.const_array();
#else
            auto const& arr = temp_ptr->const_array(mfi);
#endif
            for (int k = bx.smallEnd(2); k <= bx.bigEnd(2); ++k) {
                for (int j = bx.smallEnd(1); j <= bx.bigEnd(1); ++j) {
                    for (int i = bx.smallEnd(0); i <= bx.bigEnd(0); ++i) {
                        std::size_t idx = static_cast<std::size_t>(i) 
                                        + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(j) 
                                        + static_cast<std::size_t>(ny) * k);
                        T_host[idx] = arr(i, j, k, 0);
                    }
                }
            }
        }
        amrex::ParallelDescriptor::ReduceRealSum(T_host.data(), T_host.size());
    }

    amrex::ParallelDescriptor::ReduceRealSum(u_host.data(), u_host.size());
    amrex::ParallelDescriptor::ReduceRealSum(v_host.data(), v_host.size());
    amrex::ParallelDescriptor::ReduceRealSum(w_host.data(), w_host.size());

    // Compute metrics for each wire span
    for (auto& w : wires) {
        Real dx_span = w.x2 - w.x1;
        Real dy_span = w.y2 - w.y1;
        Real dz_span = w.z2 - w.z1;
        Real L = std::sqrt(dx_span*dx_span + dy_span*dy_span + dz_span*dz_span);
        if (L <= 1.0e-6) continue;

        Real tx = dx_span / L;
        Real ty = dy_span / L;
        Real tz = dz_span / L;

        int num_segs = std::max(1, static_cast<int>(std::ceil(L / std::min(dx, dy))));
        Real ds = L / num_segs;

        Real sum_u = 0.0;
        Real sum_drag = 0.0;
        Real sum_temp = 0.0;
        Real min_ampacity = 1.0e10;
        Real max_u = 0.0;
        Real max_drag_m = 0.0;

        for (int s = 0; s < num_segs; ++s) {
            Real xc = w.x1 + (s + Real(0.5)) * ds * tx;
            Real yc = w.y1 + (s + Real(0.5)) * ds * ty;
            Real zc = w.z1 + (s + Real(0.5)) * ds * tz;

            // Interpolate wind at segment center
            Real u = TurbineWake::interpolate_3d(u_host, xc, yc, zc, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz);
            Real v = TurbineWake::interpolate_3d(v_host, xc, yc, zc, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz);
            Real w_vel = TurbineWake::interpolate_3d(w_host, xc, yc, zc, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz);
            Real Ta = TurbineWake::interpolate_3d(T_host, xc, yc, zc, xmin, ymin, zmin, dx, dy, dz, nx, ny, nz);

            Real speed = std::sqrt(u*u + v*v + w_vel*w_vel);
            sum_u += speed;
            max_u = std::max(max_u, speed);

            // Perpendicular component of wind to span
            Real u_p = u * tx + v * ty + w_vel * tz;
            Real u_n_sq = std::max(Real(0.0), speed*speed - u_p*u_p);
            Real u_n = std::sqrt(u_n_sq);

            // Aerodynamic drag force per meter
            Real Fd = Real(0.5) * Real(1.225) * w.drag_coeff * w.diameter * u_n_sq;
            sum_drag += Fd * ds;
            max_drag_m = std::max(max_drag_m, Fd);

            // Solve steady-state conductor temperature
            Real Ts = solve_conductor_temp(Ta, u_n, w.diameter, w.resistance, w.emissivity, w.absorptivity, solar_radiation, w.current);
            sum_temp += Ts;

            // Calculate Dynamic Line Rating (ampacity)
            Real amp = compute_ampacity(Ta, u_n, w.diameter, w.resistance, w.emissivity, w.absorptivity, solar_radiation);
            min_ampacity = std::min(min_ampacity, amp);
        }

        w.avg_wind_speed = sum_u / num_segs;
        w.max_wind_speed = max_u;
        w.total_drag_force = sum_drag;
        w.max_drag_force_per_m = max_drag_m;
        w.conductor_temp = sum_temp / num_segs;
        w.max_capacity = (min_ampacity > 1.0e9) ? 0.0 : min_ampacity;

        // Sway angle
        Real Fd_avg = sum_drag / L;
        Real Fg = w.mass_density * Real(9.81);
        if (Fg > 0.0) {
            w.sway_angle = std::atan(Fd_avg / Fg) * Real(180.0) / M_PI;
        } else {
            w.sway_angle = 0.0;
        }
    }
}

// Writes wire summary report to CSV file
inline void write_wire_output_file(const std::string& filename, const std::vector<WireSpan>& wires, int time_step) {
    if (amrex::ParallelDescriptor::IOProcessor()) {
        std::ofstream csv_file;
        if (time_step == 0) {
            csv_file.open(filename, std::ios::out);
            csv_file << "wire_id,x1,y1,z1,x2,y2,z2,diameter,mass_density,drag_coeff,resistance,emissivity,absorptivity,current,"
                     << "avg_wind_speed,max_wind_speed,total_drag_force,max_drag_force_per_m,conductor_temp_K,max_capacity_A,sway_angle_deg,time_step\n";
        } else {
            csv_file.open(filename, std::ios::app);
        }

        if (csv_file.is_open()) {
            for (const auto& w : wires) {
                csv_file << w.id << ","
                         << std::fixed << std::setprecision(2)
                         << w.x1 << "," << w.y1 << "," << w.z1 << ","
                         << w.x2 << "," << w.y2 << "," << w.z2 << ","
                         << std::setprecision(4)
                         << w.diameter << "," << w.mass_density << "," << w.drag_coeff << ","
                         << std::setprecision(6)
                         << w.resistance << ","
                         << std::setprecision(4)
                         << w.emissivity << "," << w.absorptivity << "," << w.current << ","
                         << w.avg_wind_speed << "," << w.max_wind_speed << ","
                         << w.total_drag_force << "," << w.max_drag_force_per_m << ","
                         << w.conductor_temp << "," << w.max_capacity << "," << w.sway_angle << ","
                         << time_step << "\n";
            }
            csv_file.close();
            amrex::Print() << "wind_solver: logged wire loading output to " << filename << "\n";
        }
    }
}

} // namespace WireLoading

#endif // WIRE_MODELS_H
