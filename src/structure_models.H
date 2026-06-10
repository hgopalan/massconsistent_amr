#ifndef STRUCTURE_MODELS_H
#define STRUCTURE_MODELS_H

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

namespace StructureLoading {

using amrex::Real;

/**
 * @brief Damage state classification for structures.
 *
 * Maps wind loading severity to structural damage categories.
 */
enum class DamageState {
    NONE = 0,           ///< No damage
    MINOR = 1,          ///< Minor damage (cosmetic)
    MODERATE = 2,       ///< Moderate damage (some repairs needed)
    SEVERE = 3,         ///< Severe damage (major repairs required)
    DESTRUCTION = 4     ///< Total destruction / collapse
};

/**
 * @brief Representation of a general tall structure (building, tower, antenna).
 *
 * Holds structural geometry, mass distribution, material properties, and computed
 * wind loading metrics including base shear, bending moment, acceleration, and
 * fragility-based damage state assessment.
 */
struct GeneralStructure {
    int id = 0;
    Real x = 0.0, y = 0.0;                  ///< Base center coordinates [m]
    Real z_base = 0.0;                      ///< Base elevation above MSL [m]
    Real height = 0.0;                      ///< Total height of structure [m]
    Real width = 0.0;                       ///< Width of structure (x-direction) [m]
    Real depth = 0.0;                       ///< Depth of structure (y-direction) [m]
    Real mass = 0.0;                        ///< Total structural mass [kg]
    Real mass_per_height = 0.0;             ///< Distributed mass per unit height [kg/m]
    Real drag_coeff = 1.3;                  ///< Aerodynamic drag coefficient [dimensionless]
    Real natural_frequency = 0.5;           ///< Fundamental natural frequency [Hz]
    Real critical_damping_ratio = 0.05;     ///< Structural damping ratio [dimensionless]
    Real yield_stress = 250.0e6;            ///< Yield stress of structural material [Pa]
    Real density = 7850.0;                  ///< Material density (e.g., steel) [kg/m³]
    Real elastic_modulus = 200.0e9;         ///< Young's modulus of material [Pa]
    int structure_type = 0;                 ///< 0=building, 1=tower, 2=antenna, 3=chimney

    // Computed outputs:
    Real avg_wind_speed = 0.0;              ///< Average wind speed at structure [m/s]
    Real max_wind_speed = 0.0;              ///< Maximum wind speed at structure [m/s]
    Real base_shear_static = 0.0;           ///< Static base shear force [N]
    Real base_shear_dynamic = 0.0;          ///< Dynamic base shear force [N]
    Real overturning_moment = 0.0;          ///< Maximum overturning moment at base [N·m]
    Real max_deflection = 0.0;              ///< Maximum lateral deflection at top [m]
    Real max_acceleration = 0.0;            ///< Maximum acceleration at top [m/s²]
    Real stress_ratio = 0.0;                ///< Stress ratio (max stress / yield stress) [dimensionless]
    Real fundamental_period = 0.0;          ///< Fundamental period T = 1/f [s]
    Real resonance_factor = 0.0;            ///< Gust resonance amplification factor [dimensionless]
    Real damage_ratio = 0.0;                ///< Damage probability ratio (0-1) [dimensionless]
    DamageState damage_state = DamageState::NONE;  ///< Classified damage state
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
 * @brief Parses a structure geometry and properties CSV file.
 * @param filename Absolute path to the CSV file.
 * @param structures Vector of GeneralStructures to populate.
 */
inline void read_structures_file(const std::string& filename, std::vector<GeneralStructure>& structures) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        amrex::Abort("wind_solver: cannot open structures file: " + filename);
    }
    std::string line;
    int id = 0;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        // Skip header line if found
        if (line.find("x") != std::string::npos || line.find("X") != std::string::npos ||
            line.find("structure_id") != std::string::npos || line.find("height") != std::string::npos) {
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
        // [structure_id (optional)], x, y, z_base, height, width, depth, mass, mass_per_height,
        // drag_coeff, natural_freq, damping_ratio, yield_stress, elastic_modulus, structure_type
        if (tokens.size() >= 14) {
            int offset = 0;
            if (tokens.size() == 15 || !is_numeric(tokens[0])) {
                offset = 1;
            }
            Real x = std::stod(tokens[offset + 0]);
            Real y = std::stod(tokens[offset + 1]);
            Real z_base = std::stod(tokens[offset + 2]);
            Real height = std::stod(tokens[offset + 3]);
            Real width = std::stod(tokens[offset + 4]);
            Real depth = std::stod(tokens[offset + 5]);
            Real mass = std::stod(tokens[offset + 6]);
            Real mass_per_height = std::stod(tokens[offset + 7]);
            Real drag_coeff = std::stod(tokens[offset + 8]);
            Real natural_frequency = std::stod(tokens[offset + 9]);
            Real critical_damping = std::stod(tokens[offset + 10]);
            Real yield_stress = std::stod(tokens[offset + 11]);
            Real elastic_modulus = std::stod(tokens[offset + 12]);
            int structure_type = static_cast<int>(std::stod(tokens[offset + 13]));
            
            GeneralStructure s;
            s.id = id++;
            s.x = x;
            s.y = y;
            s.z_base = z_base;
            s.height = height;
            s.width = width;
            s.depth = depth;
            s.mass = mass;
            s.mass_per_height = mass_per_height;
            s.drag_coeff = drag_coeff;
            s.natural_frequency = natural_frequency;
            s.critical_damping_ratio = critical_damping;
            s.yield_stress = yield_stress;
            s.elastic_modulus = elastic_modulus;
            s.structure_type = structure_type;
            s.fundamental_period = (natural_frequency > 0.0) ? (1.0 / natural_frequency) : 0.0;
            
            structures.push_back(s);
        }
    }
    amrex::Print() << "wind_solver: loaded " << structures.size() << " structures from " << filename << "\n";
}

/**
 * @brief Computes gust response factor for structures.
 *
 * Combines mean wind effect with resonant gust amplification.
 * Higher damping reduces resonance peak. Peak occurs near natural frequency.
 *
 * @param wind_speed Wind speed at structure [m/s]
 * @param natural_freq Natural frequency [Hz]
 * @param damping_ratio Critical damping ratio [dimensionless]
 * @param gust_freq Dominant gust frequency [Hz]
 * @return Gust response factor (typically 1.5-3.5) [dimensionless]
 */
inline Real compute_gust_response_factor(Real wind_speed, Real natural_freq, 
                                          Real damping_ratio, Real gust_freq) {
    // Davenport gust factor or similar empirical relation
    // G = 1 + 0.92 * gR * gf, where gR is resonant factor
    if (wind_speed < 1.0 || natural_freq < 0.01 || damping_ratio <= 0.0) {
        return 1.0;
    }
    
    // Resonance frequency ratio
    Real freq_ratio = (gust_freq > 0.0) ? (gust_freq / natural_freq) : 0.0;
    
    // Resonant response factor (peaks near unity frequency ratio)
    Real resonant_factor = 1.0;
    if (std::abs(freq_ratio - 1.0) < 0.5) {
        // Near-resonance amplification
        resonant_factor = 1.0 / (2.0 * damping_ratio);
    }
    
    // Background response factor (quasi-static)
    Real background_factor = std::sqrt(1.0 + 2.0 * damping_ratio);
    
    // Combined gust factor (simplified)
    Real gust_factor = std::sqrt(background_factor * background_factor + 
                                  0.5 * resonant_factor * resonant_factor);
    
    return std::max(1.0, std::min(4.0, gust_factor));
}

/**
 * @brief Computes damage ratio based on wind speed and structural properties.
 *
 * Uses fragility-curve-like relationship to estimate probability of damage.
 * Assumes lognormal distribution in wind speed.
 *
 * @param base_shear Computed base shear force [N]
 * @param yield_stress Yield stress of material [Pa]
 * @param cross_section_area Cross-sectional area [m²]
 * @return Damage ratio 0-1 (0=no damage, 1=complete failure)
 */
inline Real compute_damage_ratio(Real base_shear, Real yield_stress, Real cross_section_area) {
    if (cross_section_area <= 0.0 || yield_stress <= 0.0) {
        return 0.0;
    }
    
    Real yield_force = yield_stress * cross_section_area;
    Real stress_ratio = base_shear / yield_force;
    
    // Fragility curve: lognormal-like relationship
    // Median capacity at stress_ratio = 0.5 (50% damage probability)
    Real median_ratio = 0.5;
    Real beta = 0.4;  // log-std deviation parameter
    
    if (stress_ratio < 0.1 * median_ratio) {
        return 0.0;
    }
    
    Real log_ratio = std::log(stress_ratio / median_ratio) / beta;
    // Cumulative normal distribution approximation
    Real damage = 0.5 * (1.0 + std::tanh(log_ratio / std::sqrt(2.0)));
    
    return std::max(0.0, std::min(1.0, damage));
}

/**
 * @brief Classifies damage state based on damage ratio.
 */
inline DamageState classify_damage_state(Real damage_ratio) {
    if (damage_ratio < 0.1) return DamageState::NONE;
    if (damage_ratio < 0.3) return DamageState::MINOR;
    if (damage_ratio < 0.6) return DamageState::MODERATE;
    if (damage_ratio < 0.9) return DamageState::SEVERE;
    return DamageState::DESTRUCTION;
}

/**
 * @brief Converts damage state enum to string.
 */
inline std::string damage_state_to_string(DamageState state) {
    switch (state) {
        case DamageState::NONE: return "NONE";
        case DamageState::MINOR: return "MINOR";
        case DamageState::MODERATE: return "MODERATE";
        case DamageState::SEVERE: return "SEVERE";
        case DamageState::DESTRUCTION: return "DESTRUCTION";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Processes general structure loading: shear, moments, stress, damage assessment.
 * @param structures Vector of GeneralStructures to update.
 * @param u_host Pre-gathered x-velocity array on host.
 * @param v_host Pre-gathered y-velocity array on host.
 * @param w_host Pre-gathered w-velocity array on host.
 * @param xmin, ymin, zmin Minimum domain physical boundaries [m].
 * @param dx, dy, dz Grid spacing [m].
 * @param nx, ny, nz Grid dimensions [cells].
 */
inline void process_structure_loading_pregathered(
    std::vector<GeneralStructure>& structures,
    const std::vector<Real>& u_host,
    const std::vector<Real>& v_host,
    const std::vector<Real>& w_host,
    Real xmin, Real ymin, Real zmin,
    Real dx, Real dy, Real dz,
    int nx, int ny, int nz)
{
    if (structures.empty()) return;

    // Air density [kg/m³]
    const Real rho = Real(1.225);
    const Real g = Real(9.81);

    for (auto& s : structures) {
        // Interpolation lambda
        auto interpolate_3d = [&](const std::vector<Real>& field, Real zc) -> Real {
            int i = std::max(0, std::min(nx - 1, static_cast<int>((s.x - xmin) / dx)));
            int j = std::max(0, std::min(ny - 1, static_cast<int>((s.y - ymin) / dy)));
            int k = std::max(0, std::min(nz - 1, static_cast<int>((zc - zmin) / dz)));
            std::size_t idx = static_cast<std::size_t>(i) + static_cast<std::size_t>(nx) * (static_cast<std::size_t>(j) + static_cast<std::size_t>(ny) * k);
            return idx < field.size() ? field[idx] : Real(0.0);
        };

        // Evaluate wind over structure height
        Real sum_u = 0.0, max_u = 0.0;
        Real sum_shear_static = 0.0, max_moment = 0.0;
        Real sum_accel = 0.0;
        int n_height = std::max(10, static_cast<int>(std::ceil(s.height / std::min(dz, 5.0))));
        
        for (int h = 0; h < n_height; ++h) {
            Real zc = s.z_base + (h + Real(0.5)) * s.height / n_height;
            
            Real u = interpolate_3d(u_host, zc);
            Real v = interpolate_3d(v_host, zc);
            Real w = interpolate_3d(w_host, zc);
            
            Real speed = std::sqrt(u*u + v*v + w*w);
            sum_u += speed;
            max_u = std::max(max_u, speed);
            
            // Horizontal drag force on vertical surface (frontal area)
            Real u_horiz_sq = u*u + v*v;
            Real frontal_area = s.width * (s.height / n_height);
            Real Fd_static = Real(0.5) * rho * s.drag_coeff * frontal_area * u_horiz_sq;
            
            sum_shear_static += Fd_static;
            
            // Moment arm from base
            Real moment_arm = zc - s.z_base;
            max_moment = std::max(max_moment, Fd_static * moment_arm);
            
            // Acceleration: distributed mass approach
            Real mass_seg = s.mass_per_height * (s.height / n_height);
            if (mass_seg > 0.0) {
                Real accel = Fd_static / mass_seg;
                sum_accel += accel;
            }
        }
        
        s.avg_wind_speed = (n_height > 0) ? (sum_u / n_height) : 0.0;
        s.max_wind_speed = max_u;
        s.base_shear_static = sum_shear_static;
        
        // Dynamic amplification via gust response factor
        Real gust_freq = s.max_wind_speed / (2.0 * s.width);  // Strouhal ~ 0.5
        if (gust_freq < 0.01) gust_freq = 0.1;
        Real G = compute_gust_response_factor(s.max_wind_speed, s.natural_frequency,
                                               s.critical_damping_ratio, gust_freq);
        s.base_shear_dynamic = s.base_shear_static * G;
        
        s.overturning_moment = max_moment;
        s.max_acceleration = (n_height > 0) ? (sum_accel / n_height) : 0.0;
        
        // Lateral deflection (simplified cantilever beam formula)
        // δ_max ≈ (F * H³) / (3 * E * I), where I is second moment of inertia
        if (s.elastic_modulus > 0.0 && s.width > 0.0 && s.height > 0.0) {
            Real I = (s.width * s.depth * s.depth * s.depth) / 12.0;  // Rectangle
            if (I > 0.0) {
                Real deflection = (s.base_shear_dynamic * s.height * s.height * s.height) /
                                  (3.0 * s.elastic_modulus * I);
                s.max_deflection = std::max(0.0, deflection);
            }
        }
        
        // Fundamental period (approximate)
        s.fundamental_period = (s.natural_frequency > 0.0) ? (1.0 / s.natural_frequency) : 1.0;
        
        // Gust resonance factor
        Real gust_freq2 = s.max_wind_speed / (2.0 * s.width);
        if (gust_freq2 < 0.01) gust_freq2 = 0.1;
        s.resonance_factor = compute_gust_response_factor(s.max_wind_speed, s.natural_frequency,
                                                           s.critical_damping_ratio, gust_freq2);
        
        // Stress ratio (max stress / yield stress)
        Real cross_section = s.width * s.depth;
        if (cross_section > 0.0) {
            s.stress_ratio = s.base_shear_dynamic / (s.yield_stress * cross_section);
        } else {
            s.stress_ratio = 0.0;
        }
        
        // Damage assessment via fragility curve
        s.damage_ratio = compute_damage_ratio(s.base_shear_dynamic, s.yield_stress, cross_section);
        s.damage_state = classify_damage_state(s.damage_ratio);
    }
}

/**
 * @brief Processes structure loading using distributed wind field MultiFab.
 * @param structures Vector of GeneralStructures to update.
 * @param vel Distributed AMReEx MultiFab containing 3D velocity field.
 * @param xmin, ymin, zmin Minimum domain physical boundaries [m].
 * @param dx, dy, dz Grid spacing [m].
 * @param nx, ny, nz Grid dimensions [cells].
 */
inline void process_structure_loading(
    std::vector<GeneralStructure>& structures,
    const amrex::MultiFab& vel,
    Real xmin, Real ymin, Real zmin,
    Real dx, Real dy, Real dz,
    int nx, int ny, int nz)
{
    if (structures.empty()) return;

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

    process_structure_loading_pregathered(
        structures, u_host, v_host, w_host,
        xmin, ymin, zmin,
        dx, dy, dz,
        nx, ny, nz
    );
}

/**
 * @brief Logs the calculated structure metrics into a dedicated CSV summary file.
 * @param filename File name or absolute path of destination CSV file.
 * @param structures Vector of GeneralStructures containing calculated metrics.
 * @param time_step Current simulation time step.
 */
inline void write_structure_output_file(const std::string& filename, 
                                         const std::vector<GeneralStructure>& structures, 
                                         int time_step) {
    if (amrex::ParallelDescriptor::IOProcessor()) {
        std::ofstream csv_file;
        if (time_step == 0) {
            csv_file.open(filename, std::ios::out);
            csv_file << "structure_id,x,y,z_base,height,width,depth,mass,mass_per_height,"
                     << "drag_coeff,natural_freq,damping_ratio,yield_stress,elastic_modulus,structure_type,"
                     << "avg_wind_speed,max_wind_speed,base_shear_static,base_shear_dynamic,"
                     << "overturning_moment,max_deflection,max_acceleration,stress_ratio,"
                     << "fundamental_period,resonance_factor,damage_ratio,damage_state,time_step\n";
        } else {
            csv_file.open(filename, std::ios::app);
        }

        if (csv_file.is_open()) {
            for (const auto& s : structures) {
                csv_file << s.id << ","
                         << std::fixed << std::setprecision(2)
                         << s.x << "," << s.y << "," << s.z_base << ","
                         << s.height << "," << s.width << "," << s.depth << ","
                         << std::setprecision(4)
                         << s.mass << "," << s.mass_per_height << ","
                         << s.drag_coeff << "," << s.natural_frequency << "," << s.critical_damping_ratio << ","
                         << s.yield_stress << "," << s.elastic_modulus << ","
                         << s.structure_type << ","
                         << s.avg_wind_speed << "," << s.max_wind_speed << ","
                         << s.base_shear_static << "," << s.base_shear_dynamic << ","
                         << s.overturning_moment << "," << s.max_deflection << ","
                         << s.max_acceleration << "," << s.stress_ratio << ","
                         << s.fundamental_period << "," << s.resonance_factor << ","
                         << s.damage_ratio << "," << damage_state_to_string(s.damage_state) << ","
                         << time_step << "\n";
            }
            csv_file.close();
            amrex::Print() << "wind_solver: logged structure loading output to " << filename << "\n";
        }
    }
}

} // namespace StructureLoading

#endif // STRUCTURE_MODELS_H
