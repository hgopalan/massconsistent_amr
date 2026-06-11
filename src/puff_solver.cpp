// ============================================================================
// puff_solver.cpp
// Gaussian puff dispersion solver
//
// This program reads a steady wind field (from massconsistent wind solver
// plotfile or other source) and computes time-evolving plume dispersion using
// Gaussian puff parameterization.
//
// Usage: puff_solver puff_inputs.i wind_plotfile_prefix
//
// Input parameters:
//   enable_puff          = true/false              # Enable puff model
//   source_x, source_y   = x, y coordinates [m]   # Source location
//   source_z             = z coordinate [m]        # Source height
//   emission_rate        = strength [units/s]      # Emission strength
//   emission_duration    = duration [s]            # How long to emit
//   K_h, K_v             = diffusivities [m²/s]    # Horizontal/vertical
//   sigma_y0, sigma_z0   = initial spread [m]      # Initial puff size
//   dt_puff              = time step [s]           # Puff advection time step
//   n_steps_puff         = number of steps         # Total simulation time
//   output_freq_puff     = output frequency        # Write every N steps
// ============================================================================

#include "puff_models.H"
#include "lpdm_models.H"
#include "thermodynamic_lid_models.H"
#include "turbine_wake_models.H"
#include "solver_math_constants.H"

#include <AMReX.H>
#include <AMReX_ParmParse.H>
#include <AMReX_Print.H>
#include <AMReX_Geometry.H>
#include <AMReX_MultiFab.H>
#include <AMReX_VisMF.H>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>

using namespace amrex;

struct EmissionPoint {
    Real time;
    Real rate;
};

static std::vector<EmissionPoint> read_emissions_file(const std::string& filename) {
    std::vector<EmissionPoint> profile;
    if (filename.empty()) return profile;
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        amrex::Print() << "Warning: Could not open emissions file " << filename << "\n";
        return profile;
    }
    std::string line;
    // Skip header line if present
    if (std::getline(infile, line)) {
        if (!line.empty() && (std::isdigit(line[0]) || line[0] == '-' || line[0] == '.')) {
            // First line starts with a digit/sign, so it's probably data
            std::istringstream iss(line);
            std::string t_str, r_str;
            if (std::getline(iss, t_str, ',') && std::getline(iss, r_str, ',')) {
                try {
                    EmissionPoint ep;
                    ep.time = std::stod(t_str);
                    ep.rate = std::stod(r_str);
                    profile.push_back(ep);
                } catch (...) {}
            }
        }
    }
    while (std::getline(infile, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string t_str, r_str;
        if (std::getline(iss, t_str, ',') && std::getline(iss, r_str, ',')) {
            try {
                EmissionPoint ep;
                ep.time = std::stod(t_str);
                ep.rate = std::stod(r_str);
                profile.push_back(ep);
            } catch (...) {}
        }
    }
    // Sort profile by time to be safe
    std::sort(profile.begin(), profile.end(), [](const EmissionPoint& a, const EmissionPoint& b) {
        return a.time < b.time;
    });
    return profile;
}

static Real interpolate_emission_rate(Real time, const std::vector<EmissionPoint>& profile, Real default_rate) {
    if (profile.empty()) return default_rate;
    if (time <= profile.front().time) return profile.front().rate;
    if (time >= profile.back().time) return profile.back().rate;
    
    // Find interval
    for (size_t i = 0; i < profile.size() - 1; ++i) {
        if (time >= profile[i].time && time <= profile[i + 1].time) {
            Real t0 = profile[i].time;
            Real t1 = profile[i + 1].time;
            Real r0 = profile[i].rate;
            Real r1 = profile[i + 1].rate;
            if (std::abs(t1 - t0) < 1.0e-10) return r0;
            return r0 + (r1 - r0) * (time - t0) / (t1 - t0);
        }
    }
    return default_rate;
}

static void write_hazard_boundaries(
    const std::string& filename,
    const std::vector<Real>& concentration,
    int nx, int ny, int nz,
    Real xmin, Real ymin, Real dx, Real dy,
    Real threshold_red, Real threshold_orange, Real threshold_yellow, Real threshold_lfl)
{
    std::ofstream outf(filename);
    if (!outf.is_open()) {
        return;
    }
    outf << "# Hazard Threat Zone Boundaries\n";
    outf << "zone,x,y\n";
    outf << std::scientific << std::setprecision(6);

    auto check_boundary = [&](Real T, const std::string& zone_name) {
        if (T <= 0.0) return;
        // Use ground-level (k = 0) or maximum concentration over height (k)
        // Let's do max over height as it is most conservative for threat zones
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                // Find max concentration in column (i, j)
                Real max_C = 0.0;
                for (int k = 0; k < nz; ++k) {
                    max_C = std::max(max_C, concentration[i + j * nx + k * nx * ny]);
                }
                
                if (max_C >= T) {
                    // Check if on boundary of threshold
                    bool is_bnd = false;
                    if (i == 0 || i == nx - 1 || j == 0 || j == ny - 1) {
                        is_bnd = true;
                    } else {
                        // Check 4-neighbors
                        Real n_left = 0.0, n_right = 0.0, n_down = 0.0, n_up = 0.0;
                        for (int k = 0; k < nz; ++k) {
                            n_left = std::max(n_left, concentration[(i - 1) + j * nx + k * nx * ny]);
                            n_right = std::max(n_right, concentration[(i + 1) + j * nx + k * nx * ny]);
                            n_down = std::max(n_down, concentration[i + (j - 1) * nx + k * nx * ny]);
                            n_up = std::max(n_up, concentration[i + (j + 1) * nx + k * nx * ny]);
                        }
                        if (n_left < T || n_right < T || n_down < T || n_up < T) {
                            is_bnd = true;
                        }
                    }
                    if (is_bnd) {
                        Real x = xmin + (i + 0.5) * dx;
                        Real y = ymin + (j + 0.5) * dy;
                        outf << zone_name << "," << x << "," << y << "\n";
                    }
                }
            }
        }
    };

    check_boundary(threshold_red, "red");
    check_boundary(threshold_orange, "orange");
    check_boundary(threshold_yellow, "yellow");
    check_boundary(threshold_lfl, "lfl");

    outf.close();
}

// ============================================================================
// Read terrain data from CSV file (similar to wind_solver.cpp)
// ============================================================================
static void read_terrain_file(const std::string& filename,
                               std::vector<Real>& x_terr,
                               std::vector<Real>& y_terr,
                               std::vector<Real>& z_terr)
{
    std::ifstream infile(filename);
    if (!infile) {
        amrex::Abort("puff_solver: cannot open terrain file: " + filename);
    }
    
    std::string line;
    while (std::getline(infile, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Parse X Y Z
        std::istringstream iss(line);
        Real x, y, z;
        char comma;
        
        if (iss >> x >> y >> z) {
            x_terr.push_back(x);
            y_terr.push_back(y);
            z_terr.push_back(z);
        } else if (iss >> x >> comma >> y >> comma >> z) {
            x_terr.push_back(x);
            y_terr.push_back(y);
            z_terr.push_back(z);
        }
    }
    
    if (x_terr.empty()) {
        amrex::Abort("puff_solver: no terrain data read from: " + filename);
    }
    
    amrex::Print() << "  Terrain: read " << x_terr.size() << " points from " 
                   << filename << "\n";
}

// ============================================================================
// Read building data from CSV file
// ============================================================================
static void read_building_file(const std::string& filename,
                                std::vector<Building>& buildings)
{
    std::ifstream infile(filename);
    if (!infile) {
        amrex::Abort("puff_solver: cannot open building file: " + filename);
    }
    
    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream iss(line);
        Building bldg;
        Real rotation_deg = 0.0;
        
        if (iss >> bldg.xmin >> bldg.xmax >> bldg.ymin >> bldg.ymax 
                >> bldg.zmin >> bldg.zmax) {
            iss >> rotation_deg;  // Optional rotation
            bldg.rotation = rotation_deg * MathConstants::pi / 180.0;  // Convert to radians
            
            bldg.height = bldg.zmax - bldg.zmin;
            bldg.width = bldg.ymax - bldg.ymin;
            bldg.length = bldg.xmax - bldg.xmin;
            
            buildings.push_back(bldg);
        }
    }
    
    amrex::Print() << "  Buildings: read " << buildings.size() 
                   << " buildings from " << filename << "\n";
}

// ============================================================================
// Trilinear interpolation of velocity at a point
// ============================================================================
Real interpolate_velocity_component(
    Real x, Real y, Real z,
    const MultiFab& vel,
    const Geometry& geom,
    int component)  // 0=u, 1=v, 2=w
{
    const auto& domain = geom.Domain();
    const auto& problo = geom.ProbLo();
    const auto& cellsize = geom.CellSize();
    
    Real dx = cellsize[0];
    Real dy = cellsize[1];
    Real dz = cellsize[2];
    
    // Grid indices for lower cell center
    int i0 = static_cast<int>(std::floor((x - problo[0] - 0.5 * dx) / dx));
    int j0 = static_cast<int>(std::floor((y - problo[1] - 0.5 * dy) / dy));
    int k0 = static_cast<int>(std::floor((z - problo[2] - 0.5 * dz) / dz));
    
    int i1 = i0 + 1;
    int j1 = j0 + 1;
    int k1 = k0 + 1;
    
    // Clamp to domain limits
    int imin = domain.smallEnd(0);
    int imax = domain.bigEnd(0) - 1;
    int jmin = domain.smallEnd(1);
    int jmax = domain.bigEnd(1) - 1;
    int kmin = domain.smallEnd(2);
    int kmax = domain.bigEnd(2) - 1;
    
    i0 = std::max(imin, std::min(imax, i0));
    i1 = std::max(imin, std::min(imax, i1));
    j0 = std::max(jmin, std::min(jmax, j0));
    j1 = std::max(jmin, std::min(jmax, j1));
    k0 = std::max(kmin, std::min(kmax, k0));
    k1 = std::max(kmin, std::min(kmax, k1));
    
    Real x0 = problo[0] + (i0 + 0.5) * dx;
    Real y0 = problo[1] + (j0 + 0.5) * dy;
    Real z0 = problo[2] + (k0 + 0.5) * dz;
    
    Real x_frac = (i0 == i1) ? 0.0 : (x - x0) / dx;
    Real y_frac = (j0 == j1) ? 0.0 : (y - y0) / dy;
    Real z_frac = (k0 == k1) ? 0.0 : (z - z0) / dz;
    
    x_frac = std::max(0.0, std::min(1.0, x_frac));
    y_frac = std::max(0.0, std::min(1.0, y_frac));
    z_frac = std::max(0.0, std::min(1.0, z_frac));
    
    for (MFIter mfi(vel); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        
        if (bx.contains(IntVect(i0, j0, k0))) {
            auto vel_array = vel.array(mfi);
            
            Real c000 = vel_array(i0, j0, k0, component);
            Real c100 = vel_array(i1, j0, k0, component);
            Real c010 = vel_array(i0, j1, k0, component);
            Real c110 = vel_array(i1, j1, k0, component);
            Real c001 = vel_array(i0, j0, k1, component);
            Real c101 = vel_array(i1, j0, k1, component);
            Real c011 = vel_array(i0, j1, k1, component);
            Real c111 = vel_array(i1, j1, k1, component);
            
            Real c00 = c000 * (1.0 - x_frac) + c100 * x_frac;
            Real c10 = c010 * (1.0 - x_frac) + c110 * x_frac;
            Real c01 = c001 * (1.0 - x_frac) + c101 * x_frac;
            Real c11 = c011 * (1.0 - x_frac) + c111 * x_frac;
            
            Real c0 = c00 * (1.0 - y_frac) + c10 * y_frac;
            Real c1 = c01 * (1.0 - y_frac) + c11 * y_frac;
            
            return c0 * (1.0 - z_frac) + c1 * z_frac;
        }
    }
    
    return Real(0.0);  // Default if outside domain on this process
}

// ============================================================================
// Read velocity field from AMReX plotfile
// ============================================================================
void read_velocity_plotfile(
    const std::string& plotfile_prefix,
    std::unique_ptr<MultiFab>& vel_mf,
    std::unique_ptr<Geometry>& geom,
    amrex::Box& domain,
    int& ng)
{
    amrex::ignore_unused(vel_mf, geom, domain, ng);
    // Note: This is simplified. In practice, would use AMReX::VisMF utilities
    amrex::Print() << "puff_solver: reading velocity from " << plotfile_prefix << "\n";
    
    // For now, we'll create a simple uniform wind field for testing
    // In production, would call:
    //   VisMF::Read(vel_mf, plotfile_prefix);
    
    // TODO: Implement proper plotfile reading
}

// ============================================================================
// Compute turbine wake-added turbulence intensity (TI) at a given point
// ============================================================================
Real compute_turbine_wake_added_ti(
    Real px, Real py, Real pz,
    const std::vector<TurbineWake::Turbine>& turbines,
    TurbineWake::TurbineWakeModelType model_type,
    TurbineWake::WakeAddedTurbulenceModelType added_turb_model,
    Real wind_dir_x, Real wind_dir_y,
    Real ambient_ti_base = 0.075,
    Real surface_sensible_heat_flux = 0.0,
    Real buoyant_wake_destruction_coeff = 0.005)
{
    if (added_turb_model == TurbineWake::WakeAddedTurbulenceModelType::NONE || turbines.empty()) {
        return 0.0;
    }

    Real added_ti_sq = 0.0;
    for (const auto& t : turbines) {
        Real dx_pt = px - t.x;
        Real dy_pt = py - t.y;

        // Downwind distance relative to turbine
        Real x_down = dx_pt * wind_dir_x + dy_pt * wind_dir_y;
        if (x_down <= Real(1.0e-3)) continue;

        // Crosswind distance
        Real y_cross = std::abs(-dx_pt * wind_dir_y + dy_pt * wind_dir_x);
        Real z_vertical = pz - (t.hub_height + t.z_terrain); // hub height relative to terrain

        Real r = std::sqrt(y_cross * y_cross + z_vertical * z_vertical);

        // Peak added TI on centerline
        Real delta_I_peak = 0.0;
        Real s_dist = std::max(Real(1.0), x_down / t.rotor_diameter);
        Real ct = t.ct_curve.interpolate(10.0); // Use representative 10 m/s speed or thrust of 0.8
        Real ct_clamped = std::max(Real(1e-4), std::min(ct, Real(0.99)));

        if (added_turb_model == TurbineWake::WakeAddedTurbulenceModelType::CRESPO_HERNANDEZ) {
            Real a = (Real(1.0) - std::sqrt(Real(1.0) - ct_clamped)) / Real(2.0);
            Real decay_exponent = Real(-0.32);
            if (surface_sensible_heat_flux > Real(0.0)) {
                decay_exponent *= (Real(1.0) + buoyant_wake_destruction_coeff * surface_sensible_heat_flux);
            }
            delta_I_peak = Real(0.73) * std::pow(a, Real(0.832)) * std::pow(ambient_ti_base, Real(0.0325)) * std::pow(s_dist, decay_exponent);
        } else if (added_turb_model == TurbineWake::WakeAddedTurbulenceModelType::FRANDSEN) {
            Real decay_coef = Real(0.8);
            if (surface_sensible_heat_flux > Real(0.0)) {
                decay_coef *= (Real(1.0) + buoyant_wake_destruction_coeff * surface_sensible_heat_flux);
            }
            delta_I_peak = Real(1.0) / (Real(1.5) + decay_coef * s_dist / std::sqrt(ct_clamped));
        }

        // Radial spreading factor
        Real factor = 0.0;
        if (model_type == TurbineWake::TurbineWakeModelType::JENSEN) {
            Real kw = 0.075; // Default kw
            Real r_wake = t.rotor_diameter / Real(2.0) + kw * x_down;
            if (r <= r_wake) {
                factor = Real(1.0);
            }
        } else { // Gaussian or TurbOPark
            Real ka = 0.04; // Default ka
            Real sigma = ka * x_down + Real(0.2) * t.rotor_diameter;
            factor = std::exp(-r * r / (Real(2.0) * sigma * sigma));
        }

        Real delta_I_local = delta_I_peak * factor;
        added_ti_sq += delta_I_local * delta_I_local;
    }

    return std::sqrt(added_ti_sq);
}

// ============================================================================
// Main puff solver
// ============================================================================
int main(int argc, char* argv[])
{
    amrex::Initialize(argc, argv);
    {
        ParmParse pp;
        
        // Puff model parameters
        bool enable_puff = false;
        bool enable_lpdm = false;
        pp.query("enable_puff", enable_puff);
        pp.query("enable_lpdm", enable_lpdm);
        
        int particles_per_step = 100;
        int lpdm_random_seed = 42;
        pp.query("particles_per_step", particles_per_step);
        pp.query("lpdm_random_seed", lpdm_random_seed);
        
        if (!enable_puff && !enable_lpdm) {
            amrex::Print() << "puff_solver: both puff and lpdm models disabled\n";
            amrex::Finalize();
            return 0;
        }
        
        // Source location and emission
        Real source_x = 150.0;
        Real source_y = 150.0;
        Real source_z = 10.0;
        Real emission_rate = 1.0;
        Real emission_duration = 100.0;
        
        pp.query("source_x", source_x);
        pp.query("source_y", source_y);
        pp.query("source_z", source_z);
        pp.query("emission_rate", emission_rate);
        pp.query("emission_duration", emission_duration);
        
        // Indoor infiltration, time-varying emissions, and hazard threat zone parameters
        bool enable_indoor_infiltration = false;
        Real ach = 1.5;
        std::string emissions_file = "";
        Real threshold_red = 0.0;
        Real threshold_orange = 0.0;
        Real threshold_yellow = 0.0;
        Real threshold_lfl = 0.0;
        std::string threat_zones_output = "";

        pp.query("enable_indoor_infiltration", enable_indoor_infiltration);
        pp.query("ach", ach);
        pp.query("emissions_file", emissions_file);
        pp.query("threshold_red", threshold_red);
        pp.query("threshold_orange", threshold_orange);
        pp.query("threshold_yellow", threshold_yellow);
        pp.query("threshold_lfl", threshold_lfl);
        pp.query("threat_zones_output", threat_zones_output);

        std::vector<EmissionPoint> emissions_profile = read_emissions_file(emissions_file);
        
        // Diffusivity and initial puff size
        Real K_h = 1.0;
        Real K_v = 0.5;
        Real sigma_y0 = 1.0;
        Real sigma_z0 = 1.0;
        
        pp.query("K_h", K_h);
        pp.query("K_v", K_v);
        pp.query("sigma_y0", sigma_y0);
        pp.query("sigma_z0", sigma_z0);
        
        // Height-dependent diffusivity parameters
        bool enable_height_dependent_K = false;
        std::string K_profile = "constant";
        Real K_power_law_exponent = 0.5;
        Real K_reference_height = 10.0;
        pp.query("enable_height_dependent_K", enable_height_dependent_K);
        pp.query("K_profile", K_profile);
        pp.query("K_power_law_exponent", K_power_law_exponent);
        pp.query("K_reference_height", K_reference_height);
        
        // First-order decay parameters
        bool enable_decay = false;
        Real decay_constant = 0.0;  // [1/s]
        pp.query("enable_decay", enable_decay);
        pp.query("decay_constant", decay_constant);
        
        // Plume rise parameters (Briggs buoyancy formula)
        bool enable_plume_rise = false;
        Real heat_flux = 0.0;  // Buoyancy flux F [m⁴/s³]
        pp.query("enable_plume_rise", enable_plume_rise);
        pp.query("heat_flux", heat_flux);
        
        // Time stepping
        Real dt_puff = 1.0;
        int n_steps_puff = 100;
        int output_freq_puff = 10;
        bool enable_adaptive_time_stepping = false;
        Real cfl_limit = 0.5;
        
        pp.query("dt_puff", dt_puff);
        pp.query("n_steps_puff", n_steps_puff);
        pp.query("output_freq_puff", output_freq_puff);
        pp.query("enable_adaptive_time_stepping", enable_adaptive_time_stepping);
        pp.query("cfl_limit", cfl_limit);
        
        // Wind field parameters (for uniform wind test)
        Real U_wind = 10.0;
        Real V_wind = 0.0;
        Real W_wind = 0.0;
        
        pp.query("U_wind", U_wind);
        pp.query("V_wind", V_wind);
        pp.query("W_wind", W_wind);
        
        // CALPUFF Enhancements
        bool coupled_mode = false;
        bool unsteady_wind = false;
        std::string wind_plotfile_prefix = "";
        std::vector<std::string> wind_plotfiles;
        
        pp.query("coupled_mode", coupled_mode);
        pp.query("unsteady_wind", unsteady_wind);
        pp.query("wind_plotfile_prefix", wind_plotfile_prefix);
        pp.queryarr("wind_plotfiles", wind_plotfiles);
        
        // If a command-line argument is passed (after the inputs file), use it as wind_plotfile_prefix
        if (argc > 2) {
            std::string arg2(argv[2]);
            if (arg2.find(".i") == std::string::npos) {
                wind_plotfile_prefix = arg2;
                coupled_mode = true;
            }
        }
        
        std::string dispersion_scheme = "constant"; // "constant", "pasquill_gifford", "mcelroy_pooler", "turbulence"
        bool is_urban = false;
        int pg_stability_class = 3; // Default Neutral (D)
        
        pp.query("dispersion_scheme", dispersion_scheme);
        pp.query("is_urban", is_urban);
        pp.query("pg_stability_class", pg_stability_class);
        
        // If we have standard PG stability enabled, map it to the index 0-5
        bool enable_pg_stability = false;
        pp.query("enable_pg_stability", enable_pg_stability);
        if (enable_pg_stability) {
            Real solar_radiation = 500.0;
            bool is_nighttime = false;
            Real cloud_cover = 0.5;
            pp.query("solar_radiation", solar_radiation);
            pp.query("is_nighttime", is_nighttime);
            pp.query("cloud_cover", cloud_cover);
            
            Real speed_ref = std::sqrt(U_wind * U_wind + V_wind * V_wind);
            PGStabilityClass pg_class = pasquill_gifford_class(speed_ref, solar_radiation, is_nighttime, cloud_cover);
            pg_stability_class = static_cast<int>(pg_class);
        }
        
        Real base_surface_resistance = 100.0;
        Real molecular_diffusivity = 1.5e-5;
        bool is_snow = false;
        
        pp.query("base_surface_resistance", base_surface_resistance);
        pp.query("molecular_diffusivity", molecular_diffusivity);
        pp.query("is_snow", is_snow);
        
        std::string source_type = "point"; // "point", "line", "area", "volume"
        pp.query("source_type", source_type);
        
        // Line source bounds
        Real line_start_x = 0.0, line_start_y = 0.0, line_start_z = 10.0;
        Real line_end_x = 10.0, line_end_y = 10.0, line_end_z = 10.0;
        int num_line_segments = 5;
        pp.query("line_start_x", line_start_x);
        pp.query("line_start_y", line_start_y);
        pp.query("line_start_z", line_start_z);
        pp.query("line_end_x", line_end_x);
        pp.query("line_end_y", line_end_y);
        pp.query("line_end_z", line_end_z);
        pp.query("num_line_segments", num_line_segments);
        
        // Area source bounds
        Real area_xmin = 0.0, area_xmax = 10.0, area_ymin = 0.0, area_ymax = 10.0, area_z = 10.0;
        int num_area_puffs_x = 3, num_area_puffs_y = 3;
        pp.query("area_xmin", area_xmin);
        pp.query("area_xmax", area_xmax);
        pp.query("area_ymin", area_ymin);
        pp.query("area_ymax", area_ymax);
        pp.query("area_z", area_z);
        pp.query("num_area_puffs_x", num_area_puffs_x);
        pp.query("num_area_puffs_y", num_area_puffs_y);
        
        // Volume source bounds
        Real volume_xmin = 0.0, volume_xmax = 10.0, volume_ymin = 0.0, volume_ymax = 10.0, volume_zmin = 0.0, volume_zmax = 10.0;
        int num_volume_puffs_x = 2, num_volume_puffs_y = 2, num_volume_puffs_z = 2;
        pp.query("volume_xmin", volume_xmin);
        pp.query("volume_xmax", volume_xmax);
        pp.query("volume_ymin", volume_ymin);
        pp.query("volume_ymax", volume_ymax);
        pp.query("volume_zmin", volume_zmin);
        pp.query("volume_zmax", volume_zmax);
        pp.query("num_volume_puffs_x", num_volume_puffs_x);
        pp.query("num_volume_puffs_y", num_volume_puffs_y);
        pp.query("num_volume_puffs_z", num_volume_puffs_z);
        
        // Receptors
        std::string receptor_file = "";
        pp.query("receptor_file", receptor_file);
        
        bool enable_visibility = false;
        pp.query("enable_visibility", enable_visibility);
        
        bool enable_chemistry = false;
        Real k1_rate = 0.01 / 3600.0;
        Real k2_rate = 0.02 / 3600.0;
        Real k3_rate = 0.05 / 3600.0;
        Real roughness = 0.1;
        Real u_star = 0.4;
        Real L_obukhov = 1.0e10;
        std::string receptor_output = "receptor_concentration.csv";

        pp.query("enable_chemistry", enable_chemistry);
        pp.query("k1_rate", k1_rate);
        pp.query("k2_rate", k2_rate);
        pp.query("k3_rate", k3_rate);
        pp.query("roughness", roughness);
        pp.query("u_star", u_star);
        pp.query("L_obukhov", L_obukhov);
        pp.query("receptor_output", receptor_output);

        if (enable_pg_stability && !pp.contains("L_obukhov")) {
            L_obukhov = pg_class_to_obukhov_length(static_cast<PGStabilityClass>(pg_stability_class));
        }
        
        // Read receptors file
        struct Receptor {
            Real x, y, z;
            std::string name;
        };
        std::vector<Receptor> receptors;
        if (!receptor_file.empty()) {
            std::ifstream rfile(receptor_file);
            if (rfile.is_open()) {
                std::string rline;
                // Check if has header and skip
                if (std::getline(rfile, rline)) {
                    std::string trimmed = rline;
                    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
                    if (!trimmed.empty() && trimmed[0] != '#') {
                        if (std::isalpha(trimmed[0])) {
                            // It's a header line, skip
                        } else {
                            // Not a header, parse it
                            std::istringstream riss(rline);
                            Receptor rec;
                            char rcomma;
                            if (riss >> rec.x >> rcomma >> rec.y >> rcomma >> rec.z) {
                                rec.name = "receptor_" + std::to_string(receptors.size());
                                receptors.push_back(rec);
                            }
                        }
                    }
                }
                while (std::getline(rfile, rline)) {
                    if (rline.empty() || rline[0] == '#') continue;
                    std::istringstream riss(rline);
                    Receptor rec;
                    char rcomma;
                    if (riss >> rec.x >> rcomma >> rec.y >> rcomma >> rec.z) {
                        rec.name = "receptor_" + std::to_string(receptors.size());
                        receptors.push_back(rec);
                    }
                }
                amrex::Print() << "puff_solver: Loaded " << receptors.size() << " discrete receptors from " << receptor_file << "\n";
            } else {
                amrex::Print() << "puff_solver: Warning - could not open receptor file " << receptor_file << "\n";
            }
        }
        
        // Terrain parameters
        std::string terrain_file = "";
        bool enable_terrain_reflection = false;
        bool use_image_source = true;
        
        pp.query("terrain_file", terrain_file);
        pp.query("enable_terrain_reflection", enable_terrain_reflection);
        pp.query("use_image_source", use_image_source);

        // Atmospheric Inversion Capping Lid
        bool enable_capping_lid = false;
        Real capping_lid_height = 1000.0;
        std::string capping_lid_file = "";
        pp.query("enable_capping_lid", enable_capping_lid);
        pp.query("capping_lid_height", capping_lid_height);
        pp.query("capping_lid_file", capping_lid_file);

        ThermodynamicLidParams thermo_lid_params;
        std::vector<Real> thermo_lid_flux_times;
        std::vector<Real> thermo_lid_flux_values;
        parse_thermodynamic_lid_inputs(thermo_lid_params);
        if (thermo_lid_params.enabled) {
            enable_capping_lid = true;
            capping_lid_height = thermo_lid_params.initial_zi;
            if (!thermo_lid_params.flux_file.empty()) {
                std::ifstream check_file(thermo_lid_params.flux_file);
                if (check_file.good()) {
                    read_thermodynamic_flux_file(thermo_lid_params.flux_file,
                                                 thermo_lid_flux_times,
                                                 thermo_lid_flux_values);
                } else {
                    amrex::Print() << "puff_solver: WARNING - thermodynamic lid flux file specified but not found: "
                                   << thermo_lid_params.flux_file << "\n";
                }
            }
        }
        
        std::vector<Real> x_lid_pts, y_lid_pts, z_lid_pts;
        if (!capping_lid_file.empty()) {
            read_terrain_file(capping_lid_file, x_lid_pts, y_lid_pts, z_lid_pts);
        }
        
        std::vector<Real> x_terr, y_terr, z_terr;
        if (!terrain_file.empty()) {
            read_terrain_file(terrain_file, x_terr, y_terr, z_terr);
            enable_terrain_reflection = true;  // Auto-enable if file provided
        }
        
        // Building parameters
        std::string building_file = "";
        bool enable_building_masking = false;
        bool enable_wake_diffusivity = false;
        Real wake_enhancement_cavity = 3.0;
        Real wake_enhancement_far = 1.5;
        
        // Advanced downwash parameters
        bool enable_cavity_trapping = false;
        bool enable_plume_deformation = false;
        Real aermod_prime_cavity_factor = 0.67;
        Real cavity_recirculation_strength = 0.8;

        pp.query("building_file", building_file);
        pp.query("enable_building_masking", enable_building_masking);
        pp.query("enable_wake_diffusivity", enable_wake_diffusivity);
        pp.query("wake_enhancement_cavity", wake_enhancement_cavity);
        pp.query("wake_enhancement_far", wake_enhancement_far);
        pp.query("enable_cavity_trapping", enable_cavity_trapping);
        pp.query("enable_plume_deformation", enable_plume_deformation);
        pp.query("aermod_prime_cavity_factor", aermod_prime_cavity_factor);
        pp.query("cavity_recirculation_strength", cavity_recirculation_strength);
        
        std::vector<Building> buildings;
        WakeParams wake_params;
        wake_params.enabled = enable_wake_diffusivity;
        pp.query("wake_c1", wake_params.c1);
        pp.query("wake_c2", wake_params.c2);
        pp.query("wake_separation_length", wake_params.separation_length);
        
        if (!building_file.empty()) {
            read_building_file(building_file, buildings);
            enable_building_masking = true;  // Auto-enable if file provided
        }

        // Turbine parameters for dispersion
        std::string turbine_file = "";
        bool enable_turbine_wake_diffusivity = false;
        std::string turbine_wake_model_type = "jensen";
        std::string wake_added_turb_model = "crespo_hernandez";
        Real turbine_wake_diffusivity_factor = 2.0;
        Real surface_sensible_heat_flux = 0.0;
        Real buoyant_wake_destruction_coeff = 0.005;

        pp.query("turbine_file", turbine_file);
        pp.query("enable_turbine_wake_diffusivity", enable_turbine_wake_diffusivity);
        pp.query("turbine_wake_model_type", turbine_wake_model_type);
        pp.query("wake_added_turb_model", wake_added_turb_model);
        pp.query("turbine_wake_diffusivity_factor", turbine_wake_diffusivity_factor);
        pp.query("surface_sensible_heat_flux", surface_sensible_heat_flux);
        pp.query("buoyant_wake_destruction_coeff", buoyant_wake_destruction_coeff);

        std::vector<TurbineWake::Turbine> turbines;
        if (!turbine_file.empty()) {
            TurbineWake::read_turbines_file(turbine_file, turbines);
            enable_turbine_wake_diffusivity = true;  // Auto-enable if file provided
        }

        TurbineWake::TurbineWakeModelType turb_wake_model = TurbineWake::TurbineWakeModelType::JENSEN;
        if (turbine_wake_model_type == "bastankhah_gaussian" || turbine_wake_model_type == "gaussian") {
            turb_wake_model = TurbineWake::TurbineWakeModelType::BASTANKHAH_GAUSSIAN;
        } else if (turbine_wake_model_type == "turbopark") {
            turb_wake_model = TurbineWake::TurbineWakeModelType::TURBOPARK;
        } else if (turbine_wake_model_type == "gch" || turbine_wake_model_type == "gauss_curl_hybrid") {
            turb_wake_model = TurbineWake::TurbineWakeModelType::GAUSS_CURL_HYBRID;
        }

        TurbineWake::WakeAddedTurbulenceModelType added_turb_model = TurbineWake::WakeAddedTurbulenceModelType::CRESPO_HERNANDEZ;
        if (wake_added_turb_model == "frandsen") {
            added_turb_model = TurbineWake::WakeAddedTurbulenceModelType::FRANDSEN;
        } else if (wake_added_turb_model == "none") {
            added_turb_model = TurbineWake::WakeAddedTurbulenceModelType::NONE;
        }
        
        // Canopy parameters
        bool enable_canopy_effects = false;
        Real canopy_height = 0.0;
        Real frontal_area_index = 0.0;
        Real canopy_enhancement_factor = 3.0;
        Real canopy_sheltering_factor = 0.8;
        bool enable_canopy_deposition = false;
        Real deposition_velocity = 0.01;
        
        pp.query("enable_canopy_effects", enable_canopy_effects);
        pp.query("canopy_height", canopy_height);
        pp.query("frontal_area_index", frontal_area_index);
        pp.query("canopy_enhancement_factor", canopy_enhancement_factor);
        pp.query("canopy_sheltering_factor", canopy_sheltering_factor);
        pp.query("enable_canopy_deposition", enable_canopy_deposition);
        pp.query("deposition_velocity", deposition_velocity);
        
        // Gravitational settling parameters
        bool enable_settling = false;
        Real particle_density = 1000.0;
        Real particle_diameter = 10.0e-6;
        Real air_viscosity = 1.8e-5;
        Real gravity = 9.81;
        pp.query("enable_settling", enable_settling);
        pp.query("particle_density", particle_density);
        pp.query("particle_diameter", particle_diameter);
        pp.query("air_viscosity", air_viscosity);
        pp.query("gravity", gravity);

        // Ground Dry deposition parameter
        bool enable_puff_deposition = false;
        pp.query("enable_puff_deposition", enable_puff_deposition);

        // Wet deposition / Precipitation scavenging parameters
        bool enable_wet_deposition = false;
        Real scavenging_coeff_base = 1.0e-4;
        Real precipitation_rate = 1.0;
        Real scavenging_exponent = 0.8;
        pp.query("enable_wet_deposition", enable_wet_deposition);
        pp.query("scavenging_coeff_base", scavenging_coeff_base);
        pp.query("precipitation_rate", precipitation_rate);
        pp.query("scavenging_exponent", scavenging_exponent);

        // Ambient-condition-driven chemical decay parameters
        bool enable_dynamic_decay = false;
        Real temp_ref = 298.15;
        Real temp_coeff = 0.04;
        Real rh_ref = 50.0;
        Real rh_coeff = 0.005;
        Real solar_ref = 500.0;
        Real solar_coeff = 1.0;
        Real ambient_temp = 298.15;
        Real ambient_rh = 50.0;
        Real ambient_solar = 500.0;
        pp.query("enable_dynamic_decay", enable_dynamic_decay);
        pp.query("temp_ref", temp_ref);
        pp.query("temp_coeff", temp_coeff);
        pp.query("rh_ref", rh_ref);
        pp.query("rh_coeff", rh_coeff);
        pp.query("solar_ref", solar_ref);
        pp.query("solar_coeff", solar_coeff);
        pp.query("ambient_temp", ambient_temp);
        pp.query("ambient_rh", ambient_rh);
        pp.query("ambient_solar", ambient_solar);
        
        // Domain parameters
        Real xmin = 0.0, xmax = 300.0;
        Real ymin = 0.0, ymax = 300.0;
        Real zmin = 0.0, zmax = 100.0;
        
        pp.query("xmin", xmin);
        pp.query("xmax", xmax);
        pp.query("ymin", ymin);
        pp.query("ymax", ymax);
        pp.query("zmin", zmin);
        pp.query("zmax", zmax);
        
        Real dx = 10.0, dy = 10.0, dz = 10.0;
        pp.query("dx", dx);
        pp.query("dy", dy);
        pp.query("dz", dz);
        
        // Concentration output grid
        int nx = static_cast<int>((xmax - xmin) / dx);
        int ny = static_cast<int>((ymax - ymin) / dy);
        int nz = static_cast<int>((zmax - zmin) / dz);
        
        std::string puff_output = "puff_concentration.csv";
        pp.query("puff_output", puff_output);
        
        // Print settings
        if (enable_lpdm) {
            amrex::Print() << "puff_solver: Lagrangian Particle Dispersion Model (LPDM) enabled\n";
            amrex::Print() << "  Particles per step: " << particles_per_step << "\n";
            amrex::Print() << "  Random seed: " << lpdm_random_seed << "\n";
        } else {
            amrex::Print() << "puff_solver: Gaussian puff model enabled\n";
        }
        amrex::Print() << "  Source: (" << source_x << ", " << source_y 
                       << ", " << source_z << ")\n";
        amrex::Print() << "  Emission rate: " << emission_rate << " units/s\n";
        if (!emissions_profile.empty()) {
            amrex::Print() << "  Time-varying emissions enabled (" << emissions_profile.size() << " points from " << emissions_file << ")\n";
        }
        if (enable_indoor_infiltration) {
            amrex::Print() << "  Indoor infiltration model enabled (ACH = " << ach << ")\n";
        }
        if (threshold_red > 0.0 || threshold_orange > 0.0 || threshold_yellow > 0.0 || threshold_lfl > 0.0) {
            amrex::Print() << "  Threat zones detection enabled: Red=" << threshold_red 
                           << ", Orange=" << threshold_orange << ", Yellow=" << threshold_yellow 
                           << ", LFL=" << threshold_lfl << "\n";
        }
        amrex::Print() << "  Emission duration: " << emission_duration << " s\n";
        amrex::Print() << "  K_h = " << K_h << " m²/s, K_v = " << K_v << " m²/s\n";
        if (enable_height_dependent_K) {
            amrex::Print() << "  Height-dependent diffusivity enabled\n";
            amrex::Print() << "    Profile: " << K_profile << "\n";
            amrex::Print() << "    Power-law exponent: " << K_power_law_exponent << "\n";
            amrex::Print() << "    Reference height: " << K_reference_height << " m\n";
        }
        if (enable_decay) {
            amrex::Print() << "  First-order decay enabled\n";
            amrex::Print() << "    Decay constant: " << decay_constant << " 1/s\n";
            amrex::Print() << "    Half-life: " << (0.693147 / std::max(decay_constant, 1.0e-10)) << " s\n";
        }
        if (enable_plume_rise) {
            amrex::Print() << "  Plume rise enabled (Briggs formula)\n";
            amrex::Print() << "    Buoyancy flux: " << heat_flux << " m⁴/s³\n";
        }
        if (!enable_lpdm) {
            amrex::Print() << "  Initial puff size: σy₀ = " << sigma_y0 
                           << " m, σz₀ = " << sigma_z0 << " m\n";
        }
        if (enable_adaptive_time_stepping) {
            Real max_vel_over_dx = std::max({std::abs(U_wind) / dx, std::abs(V_wind) / dy, std::abs(W_wind) / dz});
            if (max_vel_over_dx > 1.0e-12) {
                Real dt_cfl = cfl_limit / max_vel_over_dx;
                if (dt_cfl < dt_puff) {
                    Real original_dt = dt_puff;
                    Real original_total_time = dt_puff * n_steps_puff;
                    dt_puff = dt_cfl;
                    n_steps_puff = static_cast<int>(std::ceil(original_total_time / dt_puff));
                    Real scale_factor = original_dt / dt_puff;
                    output_freq_puff = std::max(1, static_cast<int>(std::round(output_freq_puff * scale_factor)));
                    amrex::Print() << "  Adaptive time-stepping: ENABLED (CFL limit = " << cfl_limit << ")\n"
                                   << "    dt_puff scaled from " << original_dt << " s to " << dt_puff << " s\n"
                                   << "    n_steps_puff adjusted to " << n_steps_puff << "\n"
                                   << "    output_freq_puff adjusted to " << output_freq_puff << "\n";
                } else {
                    amrex::Print() << "  Adaptive time-stepping: ENABLED (CFL limit = " << cfl_limit << ", static dt_puff = " << dt_puff << " s is stable)\n";
                }
            } else {
                amrex::Print() << "  Adaptive time-stepping: ENABLED (CFL limit = " << cfl_limit << ", wind is zero)\n";
            }
        }
        amrex::Print() << "  Wind: U = " << U_wind << ", V = " << V_wind 
                       << ", W = " << W_wind << " m/s\n";
        amrex::Print() << "  Time steps: " << n_steps_puff << " @ dt = " 
                       << dt_puff << " s\n";
        amrex::Print() << "  Grid: " << nx << " x " << ny << " x " << nz 
                       << " (" << dx << " x " << dy << " x " << dz << " m)\n";
        
        // Print terrain/building/canopy status
        if (enable_terrain_reflection) {
            amrex::Print() << "  Terrain reflection: ENABLED (" << x_terr.size() << " points)\n";
            amrex::Print() << "    Image source method: " << (use_image_source ? "YES" : "NO") << "\n";
        }
        if (enable_building_masking) {
            amrex::Print() << "  Building masking: ENABLED (" << buildings.size() << " buildings)\n";
        }
        if (enable_wake_diffusivity) {
            amrex::Print() << "  Wake diffusivity: ENABLED\n";
            amrex::Print() << "    Cavity enhancement: " << wake_enhancement_cavity << "x\n";
            amrex::Print() << "    Far wake enhancement: " << wake_enhancement_far << "x\n";
        }
        if (enable_turbine_wake_diffusivity) {
            amrex::Print() << "  Turbine Wake diffusivity: ENABLED\n";
            amrex::Print() << "    Turbine file: " << turbine_file << " (" << turbines.size() << " turbines)\n";
            amrex::Print() << "    Turbine wake model: " << turbine_wake_model_type << "\n";
            amrex::Print() << "    Wake added turbulence model: " << wake_added_turb_model << "\n";
            amrex::Print() << "    Diffusivity factor: " << turbine_wake_diffusivity_factor << "\n";
            if (surface_sensible_heat_flux > 0.0) {
                amrex::Print() << "    Buoyant wake destruction: ENABLED\n"
                               << "      Sensible heat flux: " << surface_sensible_heat_flux << " W/m²\n"
                               << "      Destruction coefficient: " << buoyant_wake_destruction_coeff << " m²/W\n";
            }
        }
        if (enable_cavity_trapping) {
            amrex::Print() << "  Cavity trapping (AERMOD PRIME): ENABLED\n";
            amrex::Print() << "    Cavity factor: " << aermod_prime_cavity_factor << "\n";
            amrex::Print() << "    Recirculation strength: " << cavity_recirculation_strength << "\n";
        }
        if (enable_plume_deformation) {
            amrex::Print() << "  Plume deformation under shear: ENABLED\n";
        }
        if (enable_canopy_effects) {
            amrex::Print() << "  Canopy effects: ENABLED\n";
            amrex::Print() << "    Height: " << canopy_height << " m\n";
            amrex::Print() << "    Vertical enhancement: " << canopy_enhancement_factor << "x\n";
            amrex::Print() << "    Horizontal sheltering: " << canopy_sheltering_factor << "x\n";
        }
        if (enable_canopy_deposition) {
            amrex::Print() << "  Canopy deposition: ENABLED (v_d = " << deposition_velocity << " m/s)\n";
        }
        if (enable_settling) {
            amrex::Print() << "  Gravitational settling: ENABLED\n";
            amrex::Print() << "    Particle density: " << particle_density << " kg/m³\n";
            amrex::Print() << "    Particle diameter: " << particle_diameter << " m\n";
            amrex::Print() << "    Air viscosity: " << air_viscosity << " Pa·s\n";
        }
        if (enable_puff_deposition) {
            amrex::Print() << "  Ground dry deposition: ENABLED (v_d = " << deposition_velocity << " m/s)\n";
        }
        if (enable_wet_deposition) {
            amrex::Print() << "  Wet deposition (precip scavenging): ENABLED\n";
            amrex::Print() << "    Scavenging coeff base: " << scavenging_coeff_base << " 1/s\n";
            amrex::Print() << "    Precipitation rate: " << precipitation_rate << " mm/hr\n";
            amrex::Print() << "    Scavenging exponent: " << scavenging_exponent << "\n";
        }
        if (enable_dynamic_decay) {
            amrex::Print() << "  Ambient-condition-driven chemical decay: ENABLED\n";
            amrex::Print() << "    Reference Temp: " << temp_ref << " K, Coeff: " << temp_coeff << "\n";
            amrex::Print() << "    Reference RH: " << rh_ref << " %, Coeff: " << rh_coeff << "\n";
            amrex::Print() << "    Reference Solar: " << solar_ref << " W/m², Coeff: " << solar_coeff << "\n";
            amrex::Print() << "    Ambient conditions: Temp=" << ambient_temp << " K, RH=" << ambient_rh << " %, Solar=" << ambient_solar << " W/m²\n";
        }
        
        // Wind direction for wake calculations
        Real wind_speed = std::sqrt(U_wind*U_wind + V_wind*V_wind);
        Real wind_dir_x = (wind_speed > 1.0e-10) ? U_wind / wind_speed : 1.0;
        Real wind_dir_y = (wind_speed > 1.0e-10) ? V_wind / wind_speed : 0.0;
        
        // Initialize multi-dimensional wind field variables
        std::unique_ptr<MultiFab> vel_mf = nullptr;
        std::unique_ptr<Geometry> geom = nullptr;
        Box domain_box;
        int ng = 1;
        
        Real v_s = 0.0;
        if (enable_settling) {
            v_s = compute_settling_velocity(particle_density, particle_diameter, gravity, air_viscosity);
        }

        Real wet_decay_factor = 1.0;
        if (enable_wet_deposition) {
            Real Lambda = scavenging_coeff_base * std::pow(precipitation_rate, scavenging_exponent);
            wet_decay_factor = std::exp(-Lambda * dt_puff);
        }

        // ====================================================================
        // Time-stepping loop
        // ====================================================================
        
        std::vector<Puff> puffs;
        std::vector<LpdParticle> particles;
        std::vector<Real> C_in(nx * ny * nz, 0.0);
        
        std::mt19937 gen(lpdm_random_seed);
        std::normal_distribution<Real> normal_dist(0.0, 1.0);
        
        for (int step = 0; step < n_steps_puff; ++step) {
            Real time = step * dt_puff;
            
            if (coupled_mode) {
                bool load_wind = false;
                std::string current_wind_file = "";
                if (step == 0) {
                    load_wind = true;
                } else if (unsteady_wind) {
                    load_wind = true;
                }
                
                if (load_wind) {
                    if (!wind_plotfiles.empty() && step < wind_plotfiles.size()) {
                        current_wind_file = wind_plotfiles[step];
                    } else if (!wind_plotfile_prefix.empty()) {
                        std::string opt1 = wind_plotfile_prefix + "_step" + std::to_string(step);
                        std::string opt2 = wind_plotfile_prefix + std::to_string(step);
                        if (amrex::FileExists(opt1 + "/Header")) {
                            current_wind_file = opt1;
                        } else if (amrex::FileExists(opt2 + "/Header")) {
                            current_wind_file = opt2;
                        } else {
                            current_wind_file = wind_plotfile_prefix;
                        }
                    }
                    
                    if (!current_wind_file.empty()) {
                        read_velocity_plotfile(current_wind_file, vel_mf, geom, domain_box, ng);
                    } else {
                        // fallback to uniform wind multi-dimensional construction
                        if (step == 0) {
                            amrex::Print() << "puff_solver: full 3D wind plotfile not specified, constructing uniform 3D wind for backwards compatibility\n";
                            Box domain_box_full(IntVect(0,0,0), IntVect(nx-1, ny-1, nz-1));
                            BoxArray ba(domain_box_full);
                            ba.maxSize(16);
                            DistributionMapping dm(ba);
                            
                            RealBox real_box({xmin, ymin, zmin}, {xmax, ymax, zmax});
                            Array<int, 3> is_periodic{0, 0, 0};
                            geom = std::make_unique<Geometry>(domain_box_full, real_box, CoordSys::cartesian, is_periodic);
                            domain_box = domain_box_full;
                            ng = 1;
                            
                            vel_mf = std::make_unique<MultiFab>(ba, dm, 3, ng);
                            vel_mf->setVal(U_wind, 0, 1);
                            vel_mf->setVal(V_wind, 1, 1);
                            vel_mf->setVal(W_wind, 2, 1);
                        }
                    }
                }
            }
            Real current_capping_lid_height = capping_lid_height;
            if (thermo_lid_params.enabled) {
                current_capping_lid_height = compute_thermodynamic_zi(time, thermo_lid_params, thermo_lid_flux_times, thermo_lid_flux_values);
            }
            
            Real current_emission_rate = emission_rate;
            if (!emissions_profile.empty()) {
                current_emission_rate = interpolate_emission_rate(time, emissions_profile, emission_rate);
            }
            
            if (enable_lpdm) {
                // Emit new particles if still within emission duration
                if (time < emission_duration) {
                Real step_emitted_mass = current_emission_rate * dt_puff;
                Real particle_mass = step_emitted_mass / particles_per_step;
                    
                // Compute effective source height with plume rise
                Real effective_source_z = source_z;
                if (enable_plume_rise && heat_flux > 0.0) {
                    Real representative_distance = std::max(100.0, 10.0 * source_z);
                    Real plume_rise = compute_plume_rise(heat_flux, representative_distance, 
                                                         std::max(wind_speed, MIN_WIND_SPEED_FOR_PLUME_RISE));
                    effective_source_z = source_z + plume_rise;
                }
                    
                std::uniform_real_distribution<Real> dis(0.0, 1.0);
                if (source_type == "line") {
                    for (int p_idx = 0; p_idx < particles_per_step; ++p_idx) {
                        Real frac = static_cast<Real>(p_idx) / std::max(1, particles_per_step - 1);
                        Real px = line_start_x + frac * (line_end_x - line_start_x);
                        Real py = line_start_y + frac * (line_end_y - line_start_y);
                        Real pz = line_start_z + frac * (line_end_z - line_start_z);
                        if (enable_plume_rise && heat_flux > 0.0) {
                            Real representative_distance = std::max(100.0, 10.0 * pz);
                            Real plume_rise = compute_plume_rise(heat_flux, representative_distance, 
                                                                 std::max(wind_speed, MIN_WIND_SPEED_FOR_PLUME_RISE));
                            pz += plume_rise;
                        }
                        LpdParticle new_p = create_particle(px, py, pz, particle_mass, time);
                        particles.push_back(new_p);
                    }
                } else if (source_type == "area") {
                    for (int p_idx = 0; p_idx < particles_per_step; ++p_idx) {
                        Real rx = dis(gen);
                        Real ry = dis(gen);
                        Real px = area_xmin + rx * (area_xmax - area_xmin);
                        Real py = area_ymin + ry * (area_ymax - area_ymin);
                        Real pz = area_z;
                        if (enable_plume_rise && heat_flux > 0.0) {
                            Real representative_distance = std::max(100.0, 10.0 * pz);
                            Real plume_rise = compute_plume_rise(heat_flux, representative_distance, 
                                                                 std::max(wind_speed, MIN_WIND_SPEED_FOR_PLUME_RISE));
                            pz += plume_rise;
                        }
                        LpdParticle new_p = create_particle(px, py, pz, particle_mass, time);
                        particles.push_back(new_p);
                    }
                } else if (source_type == "volume") {
                    for (int p_idx = 0; p_idx < particles_per_step; ++p_idx) {
                        Real rx = dis(gen);
                        Real ry = dis(gen);
                        Real rz = dis(gen);
                        Real px = volume_xmin + rx * (volume_xmax - volume_xmin);
                        Real py = volume_ymin + ry * (volume_ymax - volume_ymin);
                        Real pz = volume_zmin + rz * (volume_zmax - volume_zmin);
                        LpdParticle new_p = create_particle(px, py, pz, particle_mass, time);
                        particles.push_back(new_p);
                    }
                } else {
                    for (int p_idx = 0; p_idx < particles_per_step; ++p_idx) {
                        LpdParticle new_p = create_particle(
                            source_x, source_y, effective_source_z,
                            particle_mass, time);
                        particles.push_back(new_p);
                    }
                }
                }
                
                // Define lambda to compute effective vertical diffusivity at any (x, y, z)
                auto get_K_v_eff = [&](Real px, Real py, Real pz, Real terr_h) -> Real {
                    Real Kv_val = K_v;
                    Real z_agl_val = pz - terr_h;
                    if (enable_height_dependent_K && z_agl_val >= 0.0) {
                        PuffParams temp_params;
                        temp_params.enable_height_dependent_K = enable_height_dependent_K;
                        temp_params.K_profile = K_profile;
                        temp_params.K_power_law_exponent = K_power_law_exponent;
                        temp_params.K_reference_height = K_reference_height;
                        
                        Kv_val = compute_K_height_dependent(z_agl_val, K_v, temp_params);
                    }
                    
                    if (enable_canopy_effects && z_agl_val >= 0.0) {
                        Real K_h_dummy = K_h;
                        compute_canopy_diffusivity(
                            z_agl_val, canopy_height, K_h_dummy, Kv_val,
                            canopy_enhancement_factor, canopy_sheltering_factor,
                            K_h_dummy, Kv_val);
                    }
                    
                    Real wake_fac = 1.0;
                    if (enable_wake_diffusivity && !buildings.empty()) {
                        for (const auto& building : buildings) {
                            Real bldg_factor = compute_wake_enhancement_factor(
                                px, py, pz, building,
                                wind_speed, wind_dir_x, wind_dir_y,
                                wake_params, wake_enhancement_cavity, wake_enhancement_far);
                            wake_fac = std::max(wake_fac, bldg_factor);
                        }
                    }
                    Kv_val *= wake_fac;

                    if (enable_turbine_wake_diffusivity && !turbines.empty()) {
                        Real added_ti = compute_turbine_wake_added_ti(
                            px, py, pz, turbines,
                            turb_wake_model, added_turb_model,
                            wind_dir_x, wind_dir_y, 0.075,
                            surface_sensible_heat_flux, buoyant_wake_destruction_coeff);
                        Kv_val *= (1.0 + turbine_wake_diffusivity_factor * added_ti);
                    }
                    return Kv_val;
                };

                // Advect and update all particles
                for (auto& p : particles) {
                if (!p.active) continue;
                    
                // Get terrain height at particle location
                Real terrain_height = 0.0;
                if (enable_terrain_reflection && !x_terr.empty()) {
                    terrain_height = interpolate_terrain_height(
                        p.x, p.y, x_terr, y_terr, z_terr);
                }
                    
                // Get local capping lid height
                Real local_capping_lid_height = current_capping_lid_height;
                if (!x_lid_pts.empty()) {
                    local_capping_lid_height = interpolate_terrain_height(
                        p.x, p.y, x_lid_pts, y_lid_pts, z_lid_pts);
                }
                    
                // Check if particle is inside building - deactivate if so
                if (enable_building_masking && !buildings.empty()) {
                    if (point_in_any_building(p.x, p.y, p.z, buildings)) {
                        p.active = false;
                        continue;
                    }
                }
                    
                // Compute effective diffusivities
                Real K_h_eff = K_h;
                Real K_v_eff = K_v;
                    
                // Apply height-dependent diffusivity
                Real z_agl = p.z - terrain_height;
                if (enable_height_dependent_K && z_agl >= 0.0) {
                    PuffParams temp_params;
                    temp_params.enable_height_dependent_K = enable_height_dependent_K;
                    temp_params.K_profile = K_profile;
                    temp_params.K_power_law_exponent = K_power_law_exponent;
                    temp_params.K_reference_height = K_reference_height;
                        
                    K_h_eff = compute_K_height_dependent(z_agl, K_h, temp_params);
                    K_v_eff = compute_K_height_dependent(z_agl, K_v, temp_params);
                }
                    
                // Apply canopy effects
                if (enable_canopy_effects && z_agl >= 0.0) {
                    compute_canopy_diffusivity(
                        z_agl, canopy_height, K_h_eff, K_v_eff,
                        canopy_enhancement_factor, canopy_sheltering_factor,
                        K_h_eff, K_v_eff);
                }
                    
                // Apply wake enhancement
                Real wake_factor = 1.0;
                if (enable_wake_diffusivity && !buildings.empty()) {
                    for (const auto& building : buildings) {
                        Real bldg_factor = compute_wake_enhancement_factor(
                            p.x, p.y, p.z, building,
                            wind_speed, wind_dir_x, wind_dir_y,
                            wake_params, wake_enhancement_cavity, wake_enhancement_far);
                        wake_factor = std::max(wake_factor, bldg_factor);
                    }
                }
                    
                // Enhance diffusivities based on wake factor
                K_h_eff *= wake_factor;
                K_v_eff *= wake_factor;

                // Apply turbine wake diffusivity enhancement
                if (enable_turbine_wake_diffusivity && !turbines.empty()) {
                    Real added_ti = compute_turbine_wake_added_ti(
                        p.x, p.y, p.z, turbines,
                        turb_wake_model, added_turb_model,
                        wind_dir_x, wind_dir_y, 0.075,
                        surface_sensible_heat_flux, buoyant_wake_destruction_coeff);
                    K_h_eff *= (1.0 + turbine_wake_diffusivity_factor * added_ti);
                    K_v_eff *= (1.0 + turbine_wake_diffusivity_factor * added_ti);
                }
                    
                // Apply vertical drift correction velocity w_drift = dKv/dz
                Real w_drift = 0.0;
                {
                    Real delta_z = 0.1;
                    Real z_plus = p.z + delta_z;
                    Real z_minus = p.z - delta_z;
                    if (z_minus < terrain_height) {
                        Real Kv_z = K_v_eff;
                        Real Kv_plus = get_K_v_eff(p.x, p.y, p.z + delta_z, terrain_height);
                        w_drift = (Kv_plus - Kv_z) / delta_z;
                    } else {
                        Real Kv_plus = get_K_v_eff(p.x, p.y, z_plus, terrain_height);
                        Real Kv_minus = get_K_v_eff(p.x, p.y, z_minus, terrain_height);
                        w_drift = (Kv_plus - Kv_minus) / (2.0 * delta_z);
                    }
                }
                    
                // Apply chemistry and multi-species physics
                if (enable_chemistry) {
                    // Apply chemical transformation
                    apply_chemical_transformation(p.species_mass, dt_puff, k1_rate, k2_rate, k3_rate);

                    // Apply canopy deposition
                    if (enable_canopy_deposition && z_agl >= 0.0 && z_agl < canopy_height) {
                        if (canopy_height > 0.0 && frontal_area_index > 0.0) {
                            const Real decay_rate = deposition_velocity * frontal_area_index / canopy_height;
                            for (int s = 0; s < LpdParticle::NUM_SPECIES; ++s) {
                                p.species_mass[s] *= std::exp(-decay_rate * dt_puff);
                            }
                        }
                    }

                    // Apply dry deposition using Wesely models
                    if (enable_puff_deposition && z_agl >= 0.0) {
                        for (int s = 0; s < LpdParticle::NUM_SPECIES; ++s) {
                            Real v_d_s = deposition_velocity;
                            if (s == 0) {
                                v_d_s = compute_wesely_deposition_velocity(z_agl, roughness, u_star, L_obukhov, molecular_diffusivity, air_viscosity, base_surface_resistance, v_s);
                            } else if (s == 2) {
                                v_d_s = compute_wesely_deposition_velocity(z_agl, roughness, u_star, L_obukhov, molecular_diffusivity * 1.2, air_viscosity, base_surface_resistance * 1.5, v_s);
                            } else {
                                v_d_s = compute_wesely_deposition_velocity(z_agl, roughness, u_star, L_obukhov, molecular_diffusivity, air_viscosity, base_surface_resistance, v_s);
                            }
                            if (z_agl >= amrex::Real(0.0) && z_agl < amrex::Real(2.0)) {
                                amrex::Real decay_rate = v_d_s / std::max(dz, amrex::Real(0.1));
                                p.species_mass[s] *= std::exp(-decay_rate * dt_puff);
                            }
                        }
                    }

                    // Apply wet deposition
                    if (enable_wet_deposition) {
                        for (int s = 0; s < LpdParticle::NUM_SPECIES; ++s) {
                            Real lambda_s = compute_wet_scavenging_coeff(s, precipitation_rate, is_snow, scavenging_coeff_base, scavenging_exponent);
                            p.species_mass[s] *= std::exp(-lambda_s * dt_puff);
                        }
                    }

                    // Apply dynamic or regular decay (to keep logic consistent)
                    if (enable_dynamic_decay && decay_constant > 0.0) {
                        Real lambda_dynamic = compute_dynamic_decay_constant(
                            z_agl, decay_constant, ambient_temp, ambient_rh, ambient_solar,
                            temp_ref, temp_coeff, rh_ref, rh_coeff, solar_ref, solar_coeff,
                            enable_canopy_effects, canopy_height, frontal_area_index);
                        for (int s = 0; s < LpdParticle::NUM_SPECIES; ++s) {
                            p.species_mass[s] *= std::exp(-lambda_dynamic * dt_puff);
                        }
                    } else if (enable_decay && decay_constant > 0.0) {
                        for (int s = 0; s < LpdParticle::NUM_SPECIES; ++s) {
                            p.species_mass[s] *= std::exp(-decay_constant * dt_puff);
                        }
                    }

                    // Compute total mass as sum of species masses
                    p.mass = 0.0;
                    for (int s = 0; s < LpdParticle::NUM_SPECIES; ++s) {
                        p.mass += p.species_mass[s];
                    }
                } else {
                    // Standard single-species updates
                    if (enable_canopy_deposition && z_agl >= 0.0 && z_agl < canopy_height) {
                        if (canopy_height > 0.0 && frontal_area_index > 0.0) {
                            const Real decay_rate = deposition_velocity * frontal_area_index / canopy_height;
                            p.mass *= std::exp(-decay_rate * dt_puff);
                        }
                    }
                    if (enable_puff_deposition && z_agl >= 0.0) {
                        apply_particle_deposition(p, dt_puff, z_agl, deposition_velocity, dz);
                    }
                    if (enable_wet_deposition) {
                        p.mass *= wet_decay_factor;
                    }
                    if (enable_dynamic_decay && decay_constant > 0.0) {
                        Real lambda_dynamic = compute_dynamic_decay_constant(
                            z_agl, decay_constant, ambient_temp, ambient_rh, ambient_solar,
                            temp_ref, temp_coeff, rh_ref, rh_coeff, solar_ref, solar_coeff,
                            enable_canopy_effects, canopy_height, frontal_area_index);
                        p.mass *= std::exp(-lambda_dynamic * dt_puff);
                    } else if (enable_decay && decay_constant > 0.0) {
                        p.mass *= std::exp(-decay_constant * dt_puff);
                    }
                    p.species_mass[0] = p.mass;
                }
                    
                if (p.mass < 1.0e-12) {
                    p.active = false;
                    continue;
                }
                    
                // Generate random walk steps
                Real rand_dx = std::sqrt(2.0 * K_h_eff * dt_puff) * normal_dist(gen);
                Real rand_dy = std::sqrt(2.0 * K_h_eff * dt_puff) * normal_dist(gen);
                Real rand_dz = std::sqrt(2.0 * K_v_eff * dt_puff) * normal_dist(gen);
                    
                // Advection with terrain reflection
                if (enable_terrain_reflection) {
                    advect_particle_with_terrain(p, U_wind, V_wind, W_wind, 
                                                rand_dx, rand_dy, rand_dz,
                                                dt_puff, terrain_height, true, v_s,
                                                enable_capping_lid, local_capping_lid_height,
                                                w_drift);
                } else {
                    advect_particle(p, U_wind, V_wind, W_wind, 
                                    rand_dx, rand_dy, rand_dz, dt_puff, v_s,
                                    enable_capping_lid, local_capping_lid_height,
                                    w_drift);
                }
                    
                // Check bounds with terrain awareness
                if (enable_terrain_reflection) {
                    check_particle_bounds_with_terrain(p, xmin, xmax, ymin, ymax, 
                                                       zmin, zmax, terrain_height, true);
                } else {
                    check_particle_bounds(p, xmin, xmax, ymin, ymax, zmin, zmax);
                }
                    
                // Update age
                p.age += dt_puff;
                }
            } else {
                // Emit new puff if still within emission duration
                if (time < emission_duration) {
                Real puff_mass = current_emission_rate * dt_puff;
                    
                // Compute effective source height with plume rise
                Real effective_source_z = source_z;
                if (enable_plume_rise && heat_flux > 0.0) {
                    // Use a representative downwind distance for initial plume rise
                    // Typical choice: use 100 m minimum, or 10× source height if larger
                    Real representative_distance = std::max(100.0, 10.0 * source_z);
                    Real plume_rise = compute_plume_rise(heat_flux, representative_distance, 
                                                         std::max(wind_speed, MIN_WIND_SPEED_FOR_PLUME_RISE));
                    effective_source_z = source_z + plume_rise;
                }
                    
                if (source_type == "line") {
                    Real segment_mass = puff_mass / num_line_segments;
                    for (int seg = 0; seg < num_line_segments; ++seg) {
                        Real frac = (static_cast<Real>(seg) + 0.5) / num_line_segments;
                        Real px = line_start_x + frac * (line_end_x - line_start_x);
                        Real py = line_start_y + frac * (line_end_y - line_start_y);
                        Real pz = line_start_z + frac * (line_end_z - line_start_z);
                        if (enable_plume_rise && heat_flux > 0.0) {
                            Real representative_distance = std::max(100.0, 10.0 * pz);
                            Real plume_rise = compute_plume_rise(heat_flux, representative_distance, 
                                                                 std::max(wind_speed, MIN_WIND_SPEED_FOR_PLUME_RISE));
                            pz += plume_rise;
                        }
                        Puff new_puff = create_puff(px, py, pz, segment_mass, sigma_y0, sigma_z0, time);
                        puffs.push_back(new_puff);
                    }
                } else if (source_type == "area") {
                    Real dx_area = (area_xmax - area_xmin) / num_area_puffs_x;
                    Real dy_area = (area_ymax - area_ymin) / num_area_puffs_y;
                    Real sub_mass = puff_mass / (num_area_puffs_x * num_area_puffs_y);
                    for (int i_area = 0; i_area < num_area_puffs_x; ++i_area) {
                        for (int j_area = 0; j_area < num_area_puffs_y; ++j_area) {
                            Real px = area_xmin + (static_cast<Real>(i_area) + 0.5) * dx_area;
                            Real py = area_ymin + (static_cast<Real>(j_area) + 0.5) * dy_area;
                            Real pz = area_z;
                            if (enable_plume_rise && heat_flux > 0.0) {
                                Real representative_distance = std::max(100.0, 10.0 * pz);
                                Real plume_rise = compute_plume_rise(heat_flux, representative_distance, 
                                                                     std::max(wind_speed, MIN_WIND_SPEED_FOR_PLUME_RISE));
                                pz += plume_rise;
                            }
                            Puff new_puff = create_puff(px, py, pz, sub_mass, sigma_y0, sigma_z0, time);
                            puffs.push_back(new_puff);
                        }
                    }
                } else if (source_type == "volume") {
                    Real dx_vol = (volume_xmax - volume_xmin) / num_volume_puffs_x;
                    Real dy_vol = (volume_ymax - volume_ymin) / num_volume_puffs_y;
                    Real dz_vol = (volume_zmax - volume_zmin) / num_volume_puffs_z;
                    Real sub_mass = puff_mass / (num_volume_puffs_x * num_volume_puffs_y * num_volume_puffs_z);
                    for (int i_vol = 0; i_vol < num_volume_puffs_x; ++i_vol) {
                        for (int j_vol = 0; j_vol < num_volume_puffs_y; ++j_vol) {
                            for (int k_vol = 0; k_vol < num_volume_puffs_z; ++k_vol) {
                                Real px = volume_xmin + (static_cast<Real>(i_vol) + 0.5) * dx_vol;
                                Real py = volume_ymin + (static_cast<Real>(j_vol) + 0.5) * dy_vol;
                                Real pz = volume_zmin + (static_cast<Real>(k_vol) + 0.5) * dz_vol;
                                Puff new_puff = create_puff(px, py, pz, sub_mass, sigma_y0, sigma_z0, time);
                                puffs.push_back(new_puff);
                            }
                        }
                    }
                } else {
                    Puff new_puff = create_puff(
                        source_x, source_y, effective_source_z,
                        puff_mass, sigma_y0, sigma_z0, time);
                    puffs.push_back(new_puff);
                }
                }
                
                // Advect, grow, and update all puffs
                for (auto& puff : puffs) {
                if (!puff.active) continue;
                    
                // Get terrain height at puff location
                Real terrain_height = 0.0;
                if (enable_terrain_reflection && !x_terr.empty()) {
                    terrain_height = interpolate_terrain_height(
                        puff.x, puff.y, x_terr, y_terr, z_terr);
                }
                    
                // Get local capping lid height
                Real local_capping_lid_height = current_capping_lid_height;
                if (!x_lid_pts.empty()) {
                    local_capping_lid_height = interpolate_terrain_height(
                        puff.x, puff.y, x_lid_pts, y_lid_pts, z_lid_pts);
                }
                    
                // Check if puff is inside building - deactivate if so
                if (enable_building_masking && !buildings.empty()) {
                    if (point_in_any_building(puff.x, puff.y, puff.z, buildings)) {
                        puff.active = false;
                        continue;
                    }
                }
                    
                // Advection with terrain reflection
                if (enable_terrain_reflection) {
                    advect_puff_with_terrain(puff, U_wind, V_wind, W_wind, 
                                             dt_puff, terrain_height, true, v_s,
                                             enable_capping_lid, local_capping_lid_height);
                } else {
                    advect_puff(puff, U_wind, V_wind, W_wind, dt_puff, v_s,
                                enable_capping_lid, local_capping_lid_height);
                }
                    
                // Compute effective diffusivities
                Real K_h_eff = K_h;
                Real K_v_eff = K_v;
                    
                // Apply height-dependent diffusivity
                Real z_agl = puff.z - terrain_height;
                if (enable_height_dependent_K && z_agl >= 0.0) {
                    // Create temporary PuffParams for height-dependent K function
                    PuffParams temp_params;
                    temp_params.enable_height_dependent_K = enable_height_dependent_K;
                    temp_params.K_profile = K_profile;
                    temp_params.K_power_law_exponent = K_power_law_exponent;
                    temp_params.K_reference_height = K_reference_height;
                        
                    K_h_eff = compute_K_height_dependent(z_agl, K_h, temp_params);
                    K_v_eff = compute_K_height_dependent(z_agl, K_v, temp_params);
                }
                    
                // Apply canopy effects
                if (enable_canopy_effects && z_agl >= 0.0) {
                    compute_canopy_diffusivity(
                        z_agl, canopy_height, K_h_eff, K_v_eff,
                        canopy_enhancement_factor, canopy_sheltering_factor,
                        K_h_eff, K_v_eff);
                }
                    
                // Apply wake enhancement
                Real wake_factor = 1.0;
                if (enable_wake_diffusivity && !buildings.empty()) {
                    for (const auto& building : buildings) {
                        Real bldg_factor = compute_wake_enhancement_factor(
                            puff.x, puff.y, puff.z, building,
                            wind_speed, wind_dir_x, wind_dir_y,
                            wake_params, wake_enhancement_cavity, wake_enhancement_far);
                        wake_factor = std::max(wake_factor, bldg_factor);
                    }
                }

                // Apply turbine wake diffusivity enhancement
                if (enable_turbine_wake_diffusivity && !turbines.empty()) {
                    Real added_ti = compute_turbine_wake_added_ti(
                        puff.x, puff.y, puff.z, turbines,
                        turb_wake_model, added_turb_model,
                        wind_dir_x, wind_dir_y, 0.075,
                        surface_sensible_heat_flux, buoyant_wake_destruction_coeff);
                    K_h_eff *= (1.0 + turbine_wake_diffusivity_factor * added_ti);
                    K_v_eff *= (1.0 + turbine_wake_diffusivity_factor * added_ti);
                }
                    
                // Growth with combined effects
                if (dispersion_scheme == "pasquill_gifford" || dispersion_scheme == "mcelroy_pooler") {
                    Real downwind_dist = wind_speed * puff.age;
                    Real sy = sigma_y0;
                    Real sz = sigma_z0;
                    bool is_urban_local = (dispersion_scheme == "mcelroy_pooler");
                    compute_analytical_dispersion(downwind_dist, pg_stability_class, is_urban_local, sy, sz);
                    puff.sigma_y = sy * wake_factor;
                    puff.sigma_z = sz * wake_factor;
                } else {
                    if (wake_factor > 1.01) {
                        update_puff_growth_with_wake(puff, K_h_eff, K_v_eff, wake_factor);
                    } else {
                        update_puff_growth(puff, K_h_eff, K_v_eff);
                    }
                }
                    
                // Apply chemistry and multi-species physics
                if (enable_chemistry) {
                    // Apply chemical transformation
                    apply_chemical_transformation(puff.species_mass, dt_puff, k1_rate, k2_rate, k3_rate);

                    // Apply canopy deposition
                    if (enable_canopy_deposition && z_agl >= 0.0 && z_agl < canopy_height) {
                        if (canopy_height > 0.0 && frontal_area_index > 0.0) {
                            const Real decay_rate = deposition_velocity * frontal_area_index / canopy_height;
                            const Real factor = std::exp(-decay_rate * dt_puff);
                            for (int s = 0; s < Puff::NUM_SPECIES; ++s) {
                                puff.species_mass[s] *= factor;
                            }
                        }
                    }

                    // Apply dry deposition using Wesely models
                    if (enable_puff_deposition) {
                        for (int s = 0; s < Puff::NUM_SPECIES; ++s) {
                            Real v_d_s = deposition_velocity;
                            if (s == 0) {
                                v_d_s = compute_wesely_deposition_velocity(z_agl, roughness, u_star, L_obukhov, molecular_diffusivity, air_viscosity, base_surface_resistance, v_s);
                            } else if (s == 2) {
                                v_d_s = compute_wesely_deposition_velocity(z_agl, roughness, u_star, L_obukhov, molecular_diffusivity * 1.2, air_viscosity, base_surface_resistance * 1.5, v_s);
                            } else {
                                v_d_s = compute_wesely_deposition_velocity(z_agl, roughness, u_star, L_obukhov, molecular_diffusivity, air_viscosity, base_surface_resistance, v_s);
                            }
                            
                            if (puff.z < terrain_height + amrex::Real(3.0) * puff.sigma_z) {
                                const amrex::Real dz_val = terrain_height - puff.z;
                                if (puff.sigma_y > amrex::Real(1.0e-10) && puff.sigma_z > amrex::Real(1.0e-10)) {
                                    const amrex::Real exponent_z = -amrex::Real(0.5) * dz_val * dz_val / (puff.sigma_z * puff.sigma_z);
                                    if (exponent_z >= amrex::Real(-100.0)) {
                                        const amrex::Real normalization = amrex::Real(1.0) / (
                                            std::pow(amrex::Real(2.0) * 3.14159265358979323846, amrex::Real(1.5)) * 
                                            puff.sigma_y * puff.sigma_y * puff.sigma_z
                                        );
                                        const amrex::Real C_ground = puff.species_mass[s] * normalization * std::exp(exponent_z);
                                        const amrex::Real effective_area = 3.14159265358979323846 * amrex::Real(4.0) * puff.sigma_y * puff.sigma_y;
                                        const amrex::Real deposition_flux = v_d_s * C_ground;
                                        const amrex::Real mass_deposited = deposition_flux * effective_area * dt_puff;
                                        const amrex::Real mass_deposited_safe = std::min(mass_deposited, puff.species_mass[s]);
                                        puff.species_mass[s] -= mass_deposited_safe;
                                    }
                                }
                            }
                        }
                    }

                    // Apply wet deposition
                    if (enable_wet_deposition) {
                        for (int s = 0; s < Puff::NUM_SPECIES; ++s) {
                            Real lambda_s = compute_wet_scavenging_coeff(s, precipitation_rate, is_snow, scavenging_coeff_base, scavenging_exponent);
                            puff.species_mass[s] *= std::exp(-lambda_s * dt_puff);
                        }
                    }

                    // Apply elevated inversion penetration
                    if (enable_capping_lid && current_capping_lid_height > 0.0) {
                        Real inversion_penetration = compute_inversion_penetration_fraction(puff.z, puff.sigma_z, current_capping_lid_height);
                        if (inversion_penetration > 0.0) {
                            for (int s = 0; s < Puff::NUM_SPECIES; ++s) {
                                puff.species_mass[s] *= (1.0 - inversion_penetration);
                            }
                        }
                    }

                    // Apply dynamic or regular decay (to keep logic consistent)
                    if (enable_dynamic_decay && decay_constant > 0.0) {
                        Real lambda_dynamic = compute_dynamic_decay_constant(
                            z_agl, decay_constant, ambient_temp, ambient_rh, ambient_solar,
                            temp_ref, temp_coeff, rh_ref, rh_coeff, solar_ref, solar_coeff,
                            enable_canopy_effects, canopy_height, frontal_area_index);
                        const Real factor = std::exp(-lambda_dynamic * dt_puff);
                        for (int s = 0; s < Puff::NUM_SPECIES; ++s) {
                            puff.species_mass[s] *= factor;
                        }
                    } else if (enable_decay && decay_constant > 0.0) {
                        const Real factor = std::exp(-decay_constant * dt_puff);
                        for (int s = 0; s < Puff::NUM_SPECIES; ++s) {
                            puff.species_mass[s] *= factor;
                        }
                    }

                    // Compute total mass as sum of species masses
                    puff.mass = 0.0;
                    for (int s = 0; s < Puff::NUM_SPECIES; ++s) {
                        puff.mass += puff.species_mass[s];
                    }
                } else {
                    // Standard single-species updates
                    if (enable_canopy_deposition && z_agl >= 0.0 && z_agl < canopy_height) {
                        apply_canopy_deposition(puff, dt_puff, z_agl, canopy_height,
                                              frontal_area_index, deposition_velocity);
                    }
                    if (enable_puff_deposition) {
                        apply_puff_deposition(puff, dt_puff, terrain_height, deposition_velocity);
                    }
                    if (enable_wet_deposition) {
                        puff.mass *= wet_decay_factor;
                    }
                    if (enable_capping_lid && current_capping_lid_height > 0.0) {
                        Real inversion_penetration = compute_inversion_penetration_fraction(puff.z, puff.sigma_z, current_capping_lid_height);
                        puff.mass *= (1.0 - inversion_penetration);
                    }
                    if (enable_dynamic_decay && decay_constant > 0.0) {
                        Real lambda_dynamic = compute_dynamic_decay_constant(
                            z_agl, decay_constant, ambient_temp, ambient_rh, ambient_solar,
                            temp_ref, temp_coeff, rh_ref, rh_coeff, solar_ref, solar_coeff,
                            enable_canopy_effects, canopy_height, frontal_area_index);
                        apply_puff_decay(puff, dt_puff, lambda_dynamic);
                    } else if (enable_decay && decay_constant > 0.0) {
                        apply_puff_decay(puff, dt_puff, decay_constant);
                    }
                    puff.species_mass[0] = puff.mass;
                }
                    
                // Update age
                update_puff_age(puff, dt_puff);
                    
                // Check bounds with terrain awareness
                if (enable_terrain_reflection) {
                    check_puff_bounds_with_terrain(puff, xmin, xmax, ymin, ymax, 
                                                   zmin, zmax, terrain_height, true);
                } else {
                    check_puff_bounds(puff, xmin, xmax, ymin, ymax, zmin, zmax);
                }
                }
            }
            
            // Compute concentration field (C_out) at every step
            std::vector<Real> concentration(nx * ny * nz, 0.0);
            std::vector<std::vector<Real>> species_concentration;
            if (enable_chemistry) {
                species_concentration.resize(LpdParticle::NUM_SPECIES, std::vector<Real>(nx * ny * nz, 0.0));
            }
            if (enable_lpdm) {
                for (const auto& p : particles) {
                    if (!p.active) continue;
                        
                    int i = static_cast<int>((p.x - xmin) / dx);
                    int j = static_cast<int>((p.y - ymin) / dy);
                    int k = static_cast<int>((p.z - zmin) / dz);
                        
                    if (i >= 0 && i < nx && j >= 0 && j < ny && k >= 0 && k < nz) {
                        int idx = i + j * nx + k * nx * ny;
                        concentration[idx] += p.mass;
                        if (enable_chemistry) {
                            for (int s = 0; s < LpdParticle::NUM_SPECIES; ++s) {
                                species_concentration[s][idx] += p.species_mass[s];
                            }
                        }
                    }
                }
                    
                Real cell_vol = dx * dy * dz;
                for (int idx = 0; idx < nx * ny * nz; ++idx) {
                    concentration[idx] /= cell_vol;
                    if (enable_chemistry) {
                        for (int s = 0; s < LpdParticle::NUM_SPECIES; ++s) {
                            species_concentration[s][idx] /= cell_vol;
                        }
                    }
                }
            } else {
                for (int k = 0; k < nz; ++k) {
                    for (int j = 0; j < ny; ++j) {
                        for (int i = 0; i < nx; ++i) {
                            Real x = xmin + (i + 0.5) * dx;
                            Real y = ymin + (j + 0.5) * dy;
                            Real z = zmin + (k + 0.5) * dz;
                            int idx = i + j * nx + k * nx * ny;
                                
                            // Get terrain height at this point
                            Real terrain_height = 0.0;
                            if (enable_terrain_reflection && !x_terr.empty()) {
                                terrain_height = interpolate_terrain_height(
                                    x, y, x_terr, y_terr, z_terr);
                            }
                                
                            // Sum concentration from all puffs
                            Real C = 0.0;
                            std::vector<Real> C_s(Puff::NUM_SPECIES, 0.0);
                            for (const auto& puff : puffs) {
                                if (!puff.active) continue;
                                Real p_conc = 0.0;
                                if ((enable_terrain_reflection || enable_capping_lid) && use_image_source) {
                                   Real local_capping_lid_height = current_capping_lid_height;
                                    if (!x_lid_pts.empty()) {
                                        local_capping_lid_height = interpolate_terrain_height(
                                            x, y, x_lid_pts, y_lid_pts, z_lid_pts);
                                    }
                                    p_conc = gaussian_puff_concentration_with_reflection(
                                        x, y, z, puff, terrain_height, true,
                                        enable_capping_lid, local_capping_lid_height);
                                } else {
                                    p_conc = gaussian_puff_concentration(x, y, z, puff);
                                }
                                C += p_conc;
                                if (enable_chemistry && puff.mass > 0.0) {
                                    Real scale = p_conc / puff.mass;
                                    for (int s = 0; s < Puff::NUM_SPECIES; ++s) {
                                        C_s[s] += puff.species_mass[s] * scale;
                                    }
                                }
                            }
                                
                            concentration[idx] = C;
                            if (enable_chemistry) {
                                for (int s = 0; s < Puff::NUM_SPECIES; ++s) {
                                    species_concentration[s][idx] = C_s[s];
                                }
                            }
                        }
                    }
                }
            }

            // Apply indoor infiltration model if enabled
            if (enable_indoor_infiltration && !buildings.empty()) {
                Real ach_per_sec = ach / 3600.0;
                for (int k = 0; k < nz; ++k) {
                    for (int j = 0; j < ny; ++j) {
                        for (int i = 0; i < nx; ++i) {
                            Real x = xmin + (i + 0.5) * dx;
                            Real y = ymin + (j + 0.5) * dy;
                            Real z = zmin + (k + 0.5) * dz;
                            int idx = i + j * nx + k * nx * ny;
                            if (point_in_any_building(x, y, z, buildings)) {
                                C_in[idx] += dt_puff * ach_per_sec * (concentration[idx] - C_in[idx]);
                                concentration[idx] = C_in[idx];
                            }
                        }
                    }
                }
            }

            // Output concentration field at specified frequency
            if (step % output_freq_puff == 0) {
                if (enable_lpdm) {
                    amrex::Print() << "  Step " << step << " (t = " << time 
                                   << " s): " << particles.size() << " particles\n";
                } else {
                    amrex::Print() << "  Step " << step << " (t = " << time 
                                   << " s): " << puffs.size() << " puffs\n";
                }
                
                // Write discrete receptors output if enabled
                if (!receptors.empty()) {
                    std::string rstep_file = receptor_output + "_step" + std::to_string(step);
                    std::ofstream routf(rstep_file);
                    routf << "# Discrete Receptors Concentration and Visibility (step " << step << ")\n";
                    if (enable_chemistry) {
                        routf << "# name,x,y,z,C_total,SO2,Sulfate,NOx,HNO3,Nitrate";
                        if (enable_visibility) {
                            routf << ",b_ext,visual_range,deciview,fog_prob,icing_prob";
                        }
                        routf << "\n";
                    } else {
                        routf << "# name,x,y,z,C\n";
                    }
                    routf << std::scientific << std::setprecision(6);
                    
                    for (const auto& rec : receptors) {
                        Real C = 0.0;
                        Real SO2_conc = 0.0, Sulfate_conc = 0.0, NOx_conc = 0.0, HNO3_conc = 0.0, Nitrate_conc = 0.0;
                        
                        Real r_terr_h = 0.0;
                        if (enable_terrain_reflection && !x_terr.empty()) {
                            r_terr_h = interpolate_terrain_height(rec.x, rec.y, x_terr, y_terr, z_terr);
                        }
                        
                        if (enable_lpdm) {
                            int i = static_cast<int>((rec.x - xmin) / dx);
                            int j = static_cast<int>((rec.y - ymin) / dy);
                            int k = static_cast<int>((rec.z - zmin) / dz);
                            if (i >= 0 && i < nx && j >= 0 && j < ny && k >= 0 && k < nz) {
                                int idx = i + j * nx + k * nx * ny;
                                C = concentration[idx];
                                if (enable_chemistry) {
                                    SO2_conc = species_concentration[0][idx];
                                    Sulfate_conc = species_concentration[1][idx];
                                    NOx_conc = species_concentration[2][idx];
                                    HNO3_conc = species_concentration[3][idx];
                                    Nitrate_conc = species_concentration[4][idx];
                                }
                            }
                        } else {
                            for (const auto& puff : puffs) {
                                if (!puff.active) continue;
                                Real p_conc = 0.0;
                                if ((enable_terrain_reflection || enable_capping_lid) && use_image_source) {
                                   Real local_capping_lid_height = current_capping_lid_height;
                                    if (!x_lid_pts.empty()) {
                                        local_capping_lid_height = interpolate_terrain_height(
                                            rec.x, rec.y, x_lid_pts, y_lid_pts, z_lid_pts);
                                    }
                                    p_conc = gaussian_puff_concentration_with_reflection(
                                        rec.x, rec.y, rec.z, puff, r_terr_h, true,
                                        enable_capping_lid, local_capping_lid_height);
                                } else {
                                    p_conc = gaussian_puff_concentration(rec.x, rec.y, rec.z, puff);
                                }
                                C += p_conc;
                                if (enable_chemistry && puff.mass > 0.0) {
                                    Real scale = p_conc / puff.mass;
                                    SO2_conc += puff.species_mass[0] * scale;
                                    Sulfate_conc += puff.species_mass[1] * scale;
                                    NOx_conc += puff.species_mass[2] * scale;
                                    HNO3_conc += puff.species_mass[3] * scale;
                                    Nitrate_conc += puff.species_mass[4] * scale;
                                }
                            }
                        }
                        
                        routf << rec.name << "," << rec.x << "," << rec.y << "," << rec.z << "," << C;
                        if (enable_chemistry) {
                            routf << "," << SO2_conc << "," << Sulfate_conc << "," << NOx_conc << "," << HNO3_conc << "," << Nitrate_conc;
                            if (enable_visibility) {
                                Real b_ext = 10.0;
                                Real visual_range = 3912.0 / b_ext;
                                Real deciview = 0.0;
                                compute_visibility_metrics(Sulfate_conc, Nitrate_conc, ambient_rh, b_ext, visual_range, deciview);
                                
                                Real fog_prob = 0.0;
                                Real icing_prob = 0.0;
                                compute_fog_icing_probability(ambient_temp, ambient_rh, fog_prob, icing_prob);
                                
                                routf << "," << b_ext << "," << visual_range << "," << deciview << "," << fog_prob << "," << icing_prob;
                            }
                        }
                        routf << "\n";
                    }
                    routf.close();
                    amrex::Print() << "    Wrote receptors to " << rstep_file << "\n";
                }
                
                // Compute and write concentration field
                std::string step_file = puff_output + "_step" + std::to_string(step);
                
                // Write to file (simple ASCII format)
                std::ofstream outf(step_file);
                outf << "# LPDM or Gaussian puff concentration field (step " << step << ")\n";
                if (enable_chemistry) {
                    outf << "# x [m], y [m], z [m], C_total [units/m³], SO2 [units/m³], Sulfate [units/m³], NOx [units/m³], HNO3 [units/m³], Nitrate [units/m³]\n";
                } else {
                    outf << "# x [m], y [m], z [m], C [units/m³]\n";
                }
                outf << std::scientific << std::setprecision(6);
                
                for (int k = 0; k < nz; ++k) {
                    for (int j = 0; j < ny; ++j) {
                        for (int i = 0; i < nx; ++i) {
                            Real x = xmin + (i + 0.5) * dx;
                            Real y = ymin + (j + 0.5) * dy;
                            Real z = zmin + (k + 0.5) * dz;
                            int idx = i + j * nx + k * nx * ny;
                            Real C = concentration[idx];
                                
                            if (enable_chemistry) {
                                Real SO2_c = species_concentration[0][idx];
                                Real Sulfate_c = species_concentration[1][idx];
                                Real NOx_c = species_concentration[2][idx];
                                Real HNO3_c = species_concentration[3][idx];
                                Real Nitrate_c = species_concentration[4][idx];
                                outf << x << "," << y << "," << z << "," << C << "," << SO2_c << "," << Sulfate_c << "," << NOx_c << "," << HNO3_c << "," << Nitrate_c << "\n";
                            } else {
                                outf << x << "," << y << "," << z << "," << C << "\n";
                            }
                        }
                    }
                }
                outf.close();
                
                amrex::Print() << "    Wrote concentration to " << step_file << "\n";

                // Write threat zones boundaries if enabled
                if (!threat_zones_output.empty() && 
                    (threshold_red > 0.0 || threshold_orange > 0.0 || threshold_yellow > 0.0 || threshold_lfl > 0.0)) {
                    std::string tz_file = threat_zones_output + "_step" + std::to_string(step) + ".csv";
                    write_hazard_boundaries(tz_file, concentration, nx, ny, nz, xmin, ymin, dx, dy,
                                            threshold_red, threshold_orange, threshold_yellow, threshold_lfl);
                    amrex::Print() << "    Wrote threat zones to " << tz_file << "\n";
                }
            }
        }
        
        // Count active elements
        int n_active = 0;
        if (enable_lpdm) {
            for (const auto& p : particles) {
                if (p.active) n_active++;
            }
            amrex::Print() << "puff_solver: done.\n";
            amrex::Print() << "  Total particles emitted: " << particles.size() << "\n";
            amrex::Print() << "  Active particles at end: " << n_active << "\n";
        } else {
            for (const auto& puff : puffs) {
                if (puff.active) n_active++;
            }
            amrex::Print() << "puff_solver: done.\n";
            amrex::Print() << "  Total puffs emitted: " << puffs.size() << "\n";
            amrex::Print() << "  Active puffs at end: " << n_active << "\n";
        }
    }
    amrex::Finalize();
    return 0;
}
