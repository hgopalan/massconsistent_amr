#ifndef BRIDGE_MODELS_H
#define BRIDGE_MODELS_H

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

namespace BridgeLoading {

using amrex::Real;

/**
 * @brief Representation of a bridge deck span under wind loading.
 *
 * Holds structural geometry, mass properties, and computed wind loading metrics
 * including vertical/lateral sway, base shear, bending moment, and resonance risk.
 */
struct BridgeSpan {
    int id = 0;
    Real x1 = 0.0, y1 = 0.0, z1 = 0.0; ///< Start coordinates of the span [m]
    Real x2 = 0.0, y2 = 0.0, z2 = 0.0; ///< End coordinates of the span [m]
    Real deck_width = 0.0;              ///< Deck width [m]
    Real deck_depth = 0.0;              ///< Deck height/depth [m]
    Real mass_per_length = 0.0;         ///< Distributed mass per unit length [kg/m]
    Real drag_coeff = 1.2;              ///< Aerodynamic drag coefficient (vertical wind)
    Real side_drag_coeff = 0.6;         ///< Side drag coefficient (lateral wind)
    Real natural_frequency = 0.5;       ///< Fundamental bridge frequency [Hz]
    Real critical_damping_ratio = 0.05; ///< Structural damping ratio [dimensionless]

    // Computed outputs:
    Real avg_wind_speed = 0.0;          ///< Average wind speed along span [m/s]
    Real max_wind_speed = 0.0;          ///< Maximum wind speed along span [m/s]
    Real vertical_sway_angle = 0.0;     ///< Vertical/lateral sway angle from wind [degrees]
    Real base_shear_force = 0.0;        ///< Total horizontal shear force at base [N]
    Real bending_moment = 0.0;          ///< Maximum bending moment [N·m]
    Real vortex_shedding_freq = 0.0;    ///< Vortex shedding frequency [Hz]
    Real resonance_ratio = 0.0;         ///< Ratio of vortex freq to natural freq (unity = resonance)
    Real max_acceleration = 0.0;        ///< Maximum deck acceleration [m/s²]
    Real comfort_assessment = 0.0;      ///< Comfort metric (0=safe, 1=unsafe) [dimensionless]
};

/**
 * @brief Helper function to check if a string represents a valid floating-point number.
 */
inline bool is_numeric(const std::string& s) {
    if (s.empty()) return false;
    char* p;
    strtod(s.c_str(), &p);
    if (p == s.c_str()) return false;
    return *p == '\0';
}

/**
 * @brief Parses a bridge span layout CSV file.
 * @param filename Absolute path to the CSV file.
 * @param bridges Vector of BridgeSpans to populate.
 */
inline void read_bridges_file(const std::string& filename, std::vector<BridgeSpan>& bridges) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        amrex::Abort("wind_solver: cannot open bridges file: " + filename);
    }
    std::string line;
    int id = 0;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        // Skip header line if found
        if (line.find("x1") != std::string::npos || line.find("X1") != std::string::npos || 
            line.find("span_id") != std::string::npos) {
            continue;
        }
        
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (ss >> token) {
            tokens.push_back(token);
        }
        
        // Expected columns:
        // [span_id (optional)], x1, y1, z1, x2, y2, z2, deck_width, deck_depth, mass_per_length, drag_coeff, side_drag_coeff, natural_freq, damping_ratio
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
            Real deck_width = std::stod(tokens[offset + 6]);
            Real deck_depth = std::stod(tokens[offset + 7]);
            Real mass_per_length = std::stod(tokens[offset + 8]);
            Real drag_coeff = std::stod(tokens[offset + 9]);
            Real side_drag_coeff = std::stod(tokens[offset + 10]);
            Real natural_frequency = std::stod(tokens[offset + 11]);
            Real critical_damping = std::stod(tokens[offset + 12]);
            
            BridgeSpan b;
            b.id = id++;
            b.x1 = x1;
            b.y1 = y1;
            b.z1 = z1;
            b.x2 = x2;
            b.y2 = y2;
            b.z2 = z2;
            b.deck_width = deck_width;
            b.deck_depth = deck_depth;
            b.mass_per_length = mass_per_length;
            b.drag_coeff = drag_coeff;
            b.side_drag_coeff = side_drag_coeff;
            b.natural_frequency = natural_frequency;
            b.critical_damping_ratio = critical_damping;
            
            bridges.push_back(b);
        }
    }
    amrex::Print() << "wind_solver: loaded " << bridges.size() << " bridges from " << filename << "\n";
}

/**
 * @brief Computes Strouhal number for vortex shedding on bluff body (bridge deck).
 * @param Re Reynolds number based on deck width.
 * @return Strouhal number [dimensionless].
 */
inline Real compute_strouhal_number(Real Re) {
    // Simplified empirical Strouhal for bridge decks: St ≈ 0.1-0.2 for Re ~ 10^4
    if (Re < 1.0) return 0.0;
    if (Re < 1000.0) return 0.15;
    if (Re < 100000.0) return 0.12;
    return 0.10;
}

/**
 * @brief Processes bridge loading: drag forces, sway, moments, resonance assessment.
 * @param bridges Vector of BridgeSpans to update.
 * @param u_host Pre-gathered x-velocity array on host.
 * @param v_host Pre-gathered y-velocity array on host.
 * @param w_host Pre-gathered w-velocity array on host.
 * @param xmin, ymin, zmin Minimum domain physical boundaries [m].
 * @param dx, dy, dz Grid spacing [m].
 * @param nx, ny, nz Grid dimensions [cells].
 */
inline void process_bridge_loading_pregathered(
    std::vector<BridgeSpan>& bridges,
    const std::vector<Real>& u_host,
    const std::vector<Real>& v_host,
    const std::vector<Real>& w_host,
    Real xmin, Real ymin, Real zmin,
    Real dx, Real dy, Real dz,
    int nx, int ny, int nz)
{
    if (bridges.empty()) return;

    // Air density [kg/m³]
    const Real rho = Real(1.225);
    const Real g = Real(9.81);

    for (auto& b : bridges) {
        Real dx_span = b.x2 - b.x1;
        Real dy_span = b.y2 - b.y1;
        Real dz_span = b.z2 - b.z1;
        Real L = std::sqrt(dx_span*dx_span + dy_span*dy_span + dz_span*dz_span);
        if (L <= 1.0e-6) continue;

        Real tx = dx_span / L;
        Real ty = dy_span / L;
        Real tz = dz_span / L;

        int num_segs = std::max(1, static_cast<int>(std::ceil(L / std::min(dx, dy))));
        Real ds = L / num_segs;

        Real sum_u = 0.0, max_u = 0.0;
        Real sum_shear = 0.0, max_moment = 0.0;
        Real sum_accel = 0.0;

        for (int s = 0; s < num_segs; ++s) {
            Real xc = b.x1 + (s + Real(0.5)) * ds * tx;
            Real yc = b.y1 + (s + Real(0.5)) * ds * ty;
            Real zc = b.z1 + (s + Real(0.5)) * ds * tz;

            // Interpolate wind at segment center
            auto interpolate_3d = [&](const std::vector<Real>& field) -> Real {
                int i = std::max(0, std::min(nx - 1, static_cast<int>((xc - xmin) / dx)));
                int j = std::max(0, std::min(ny - 1, static_cast<int>((yc - ymin) / dy)));
                int k = std::max(0, std::min(nz - 1, static_cast<int>((zc - zmin) / dz)));
                std::size_t idx = static_cast<std::size_t>(i) + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(j) + static_cast<std::size_t>(ny) * k);
                return idx < field.size() ? field[idx] : Real(0.0);
            };

            Real u = interpolate_3d(u_host);
            Real v = interpolate_3d(v_host);
            Real w = interpolate_3d(w_host);

            Real speed = std::sqrt(u*u + v*v + w*w);
            sum_u += speed;
            max_u = std::max(max_u, speed);

            // Vertical drag force (wind hitting deck from side) - primarily from w and perpendicular (u,v)
            Real u_perp_sq = u*u + v*v;
            Real Fd_vertical = Real(0.5) * rho * b.drag_coeff * b.deck_width * u_perp_sq;
            
            // Lateral drag (crosswind)
            Real w_perp_sq = std::max(Real(0.0), w*w);
            Real Fd_lateral = Real(0.5) * rho * b.side_drag_coeff * b.deck_depth * w_perp_sq;

            sum_shear += (Fd_vertical + Fd_lateral) * ds;

            // Moment: drag force × moment arm (distance from base)
            Real moment_arm = (num_segs - s) * ds;
            max_moment = std::max(max_moment, (Fd_vertical + Fd_lateral) * moment_arm);

            // Acceleration: F = m*a, so a = F / m
            Real mass_seg = b.mass_per_length * ds;
            if (mass_seg > 0.0) {
                Real accel = (Fd_vertical + Fd_lateral) / mass_seg;
                sum_accel += accel;
            }
        }

        b.avg_wind_speed = sum_u / num_segs;
        b.max_wind_speed = max_u;
        b.base_shear_force = sum_shear;
        b.bending_moment = max_moment;
        b.max_acceleration = (num_segs > 0) ? (sum_accel / num_segs) : Real(0.0);

        // Sway angle approximation: θ = arctan(a / g)
        if (b.max_acceleration > 0.0) {
            b.vertical_sway_angle = std::atan(b.max_acceleration / g) * Real(180.0) / M_PI;
        } else {
            b.vertical_sway_angle = 0.0;
        }

        // Vortex shedding frequency: f_vortex = St * U / D
        Real Re = (rho * b.max_wind_speed * b.deck_width) / Real(1.8e-5);
        Real St = compute_strouhal_number(Re);
        b.vortex_shedding_freq = (b.deck_width > 0.0) ? (St * b.max_wind_speed / b.deck_width) : Real(0.0);

        // Resonance ratio: how close vortex freq is to natural freq
        if (b.natural_frequency > 0.0) {
            b.resonance_ratio = b.vortex_shedding_freq / b.natural_frequency;
        } else {
            b.resonance_ratio = 0.0;
        }

        // Comfort assessment (simplified): higher accelerations and resonance → higher risk
        // Comfort threshold typically ~0.5 m/s²
        Real comfort_accel = b.max_acceleration / Real(0.5);
        Real resonance_penalty = (std::abs(b.resonance_ratio - Real(1.0)) < Real(0.1)) ? Real(2.0) : Real(1.0);
        b.comfort_assessment = std::min(Real(1.0), comfort_accel * resonance_penalty);
    }
}

/**
 * @brief Processes bridge loading using distributed wind field MultiFab.
 * @param bridges Vector of BridgeSpans to update.
 * @param vel Distributed AMReEx MultiFab containing 3D velocity field.
 * @param xmin, ymin, zmin Minimum domain physical boundaries [m].
 * @param dx, dy, dz Grid spacing [m].
 * @param nx, ny, nz Grid dimensions [cells].
 */
inline void process_bridge_loading(
    std::vector<BridgeSpan>& bridges,
    const amrex::MultiFab& vel,
    Real xmin, Real ymin, Real zmin,
    Real dx, Real dy, Real dz,
    int nx, int ny, int nz)
{
    if (bridges.empty()) return;

    // Gather velocities
    std::vector<Real> u_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);
    std::vector<Real> v_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);
    std::vector<Real> w_host(static_cast<std::size_t>(nx) * ny * nz, 0.0);

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

    amrex::ParallelDescriptor::ReduceRealSum(u_host.data(), u_host.size());
    amrex::ParallelDescriptor::ReduceRealSum(v_host.data(), v_host.size());
    amrex::ParallelDescriptor::ReduceRealSum(w_host.data(), w_host.size());

    process_bridge_loading_pregathered(
        bridges, u_host, v_host, w_host,
        xmin, ymin, zmin,
        dx, dy, dz,
        nx, ny, nz
    );
}

/**
 * @brief Logs the calculated bridge metrics into a dedicated CSV summary file.
 * @param filename File name or absolute path of destination CSV file.
 * @param bridges Vector of BridgeSpans containing calculated metrics.
 * @param time_step Current simulation time step.
 */
inline void write_bridge_output_file(const std::string& filename, const std::vector<BridgeSpan>& bridges, int time_step) {
    if (amrex::ParallelDescriptor::IOProcessor()) {
        std::ofstream csv_file;
        if (time_step == 0) {
            csv_file.open(filename, std::ios::out);
            csv_file << "bridge_id,x1,y1,z1,x2,y2,z2,deck_width,deck_depth,mass_per_length,drag_coeff,side_drag_coeff,natural_freq,damping_ratio,"
                     << "avg_wind_speed,max_wind_speed,vertical_sway_angle,base_shear_force,bending_moment,vortex_shedding_freq,resonance_ratio,max_acceleration,comfort_assessment,time_step\n";
        } else {
            csv_file.open(filename, std::ios::app);
        }

        if (csv_file.is_open()) {
            for (const auto& b : bridges) {
                csv_file << b.id << ","
                         << std::fixed << std::setprecision(2)
                         << b.x1 << "," << b.y1 << "," << b.z1 << ","
                         << b.x2 << "," << b.y2 << "," << b.z2 << ","
                         << std::setprecision(4)
                         << b.deck_width << "," << b.deck_depth << "," << b.mass_per_length << ","
                         << b.drag_coeff << "," << b.side_drag_coeff << ","
                         << b.natural_frequency << "," << b.critical_damping_ratio << ","
                         << b.avg_wind_speed << "," << b.max_wind_speed << ","
                         << b.vertical_sway_angle << "," << b.base_shear_force << ","
                         << b.bending_moment << "," << b.vortex_shedding_freq << ","
                         << b.resonance_ratio << "," << b.max_acceleration << ","
                         << b.comfort_assessment << ","
                         << time_step << "\n";
            }
            csv_file.close();
            amrex::Print() << "wind_solver: logged bridge loading output to " << filename << "\n";
        }
    }
}

} // namespace BridgeLoading

#endif // BRIDGE_MODELS_H
