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
#include "math_constants.H"

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

// Selective using declarations from amrex namespace
using amrex::Real;
using amrex::Box;
using amrex::MultiFab;
using amrex::Geometry;
using amrex::ParmParse;
using amrex::MFIter;
using amrex::ParallelFor;
using amrex::Array;
using amrex::Vector;

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
    
    // Grid indices
    int i = static_cast<int>((x - problo[0]) / dx);
    int j = static_cast<int>((y - problo[1]) / dy);
    int k = static_cast<int>((z - problo[2]) / dz);
    
    // Clamp to domain
    i = std::max(domain.smallEnd(0), std::min(domain.bigEnd(0) - 1, i));
    j = std::max(domain.smallEnd(1), std::min(domain.bigEnd(1) - 1, j));
    k = std::max(domain.smallEnd(2), std::min(domain.bigEnd(2) - 1, k));
    
    // Get box owned by this process
    for (MFIter mfi(vel); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        
        if (bx.contains(IntVect(i, j, k))) {
            auto vel_array = vel.array(mfi);
            
            // Simple nearest-neighbor for now (could improve to trilinear)
            Real val = vel_array(i, j, k, component);
            return val;
        }
    }
    
    return Real(0.0);  // Default if outside domain on this process
}

// ============================================================================
// Read velocity field from AMReX plotfile
// ============================================================================
void read_velocity_plotfile(
    const std::string& plotfile_prefix,
    std::unique_ptr<MultiFab>& /*vel_mf*/,
    std::unique_ptr<Geometry>& /*geom*/,
    amrex::Box& /*domain*/,
    int& /*ng*/)
{
    // Note: This is simplified. In practice, would use AMReX::VisMF utilities
    amrex::Print() << "puff_solver: reading velocity from " << plotfile_prefix << "\n";
    
    // For now, we'll create a simple uniform wind field for testing
    // In production, would call:
    //   VisMF::Read(vel_mf, plotfile_prefix);
    
    // TODO: Implement proper plotfile reading
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
        pp.query("enable_puff", enable_puff);
        
        if (!enable_puff) {
            amrex::Print() << "puff_solver: puff model disabled (enable_puff = false)\n";
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
        
        pp.query("dt_puff", dt_puff);
        pp.query("n_steps_puff", n_steps_puff);
        pp.query("output_freq_puff", output_freq_puff);
        
        // Wind field parameters (for uniform wind test)
        Real U_wind = 10.0;
        Real V_wind = 0.0;
        Real W_wind = 0.0;
        
        pp.query("U_wind", U_wind);
        pp.query("V_wind", V_wind);
        pp.query("W_wind", W_wind);
        
        // Terrain parameters
        std::string terrain_file = "";
        bool enable_terrain_reflection = false;
        bool use_image_source = true;
        
        pp.query("terrain_file", terrain_file);
        pp.query("enable_terrain_reflection", enable_terrain_reflection);
        pp.query("use_image_source", use_image_source);
        
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
        
        pp.query("building_file", building_file);
        pp.query("enable_building_masking", enable_building_masking);
        pp.query("enable_wake_diffusivity", enable_wake_diffusivity);
        pp.query("wake_enhancement_cavity", wake_enhancement_cavity);
        pp.query("wake_enhancement_far", wake_enhancement_far);
        
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
        amrex::Print() << "puff_solver: Gaussian puff model enabled\n";
        amrex::Print() << "  Source: (" << source_x << ", " << source_y 
                       << ", " << source_z << ")\n";
        amrex::Print() << "  Emission rate: " << emission_rate << " units/s\n";
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
        amrex::Print() << "  Initial puff size: σy₀ = " << sigma_y0 
                       << " m, σz₀ = " << sigma_z0 << " m\n";
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
        if (enable_canopy_effects) {
            amrex::Print() << "  Canopy effects: ENABLED\n";
            amrex::Print() << "    Height: " << canopy_height << " m\n";
            amrex::Print() << "    Vertical enhancement: " << canopy_enhancement_factor << "x\n";
            amrex::Print() << "    Horizontal sheltering: " << canopy_sheltering_factor << "x\n";
        }
        if (enable_canopy_deposition) {
            amrex::Print() << "  Canopy deposition: ENABLED (v_d = " << deposition_velocity << " m/s)\n";
        }
        
        // Wind direction for wake calculations
        Real wind_speed = std::sqrt(U_wind*U_wind + V_wind*V_wind);
        Real wind_dir_x = (wind_speed > 1.0e-10) ? U_wind / wind_speed : 1.0;
        Real wind_dir_y = (wind_speed > 1.0e-10) ? V_wind / wind_speed : 0.0;
        
        // ====================================================================
        // Time-stepping loop
        // ====================================================================
        
        std::vector<Puff> puffs;
        
        for (int step = 0; step < n_steps_puff; ++step) {
            Real time = step * dt_puff;
            
            // Emit new puff if still within emission duration
            if (time < emission_duration) {
                Real puff_mass = emission_rate * dt_puff;
                
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
                
                Puff new_puff = create_puff(
                    source_x, source_y, effective_source_z,
                    puff_mass, sigma_y0, sigma_z0, time);
                puffs.push_back(new_puff);
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
                                            dt_puff, terrain_height, true);
                } else {
                    advect_puff(puff, U_wind, V_wind, W_wind, dt_puff);
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
                
                // Growth with combined effects
                if (wake_factor > 1.01) {
                    update_puff_growth_with_wake(puff, K_h_eff, K_v_eff, wake_factor);
                } else {
                    update_puff_growth(puff, K_h_eff, K_v_eff);
                }
                
                // Apply canopy deposition
                if (enable_canopy_deposition && z_agl >= 0.0 && z_agl < canopy_height) {
                    apply_canopy_deposition(puff, dt_puff, z_agl, canopy_height,
                                          frontal_area_index, deposition_velocity);
                }
                
                // Update age
                update_puff_age(puff, dt_puff);
                
                // Apply first-order decay
                if (enable_decay && decay_constant > 0.0) {
                    apply_puff_decay(puff, dt_puff, decay_constant);
                }
                
                // Check bounds with terrain awareness
                if (enable_terrain_reflection) {
                    check_puff_bounds_with_terrain(puff, xmin, xmax, ymin, ymax, 
                                                   zmin, zmax, terrain_height, true);
                } else {
                    check_puff_bounds(puff, xmin, xmax, ymin, ymax, zmin, zmax);
                }
            }
            
            // Output concentration field at specified frequency
            if (step % output_freq_puff == 0) {
                amrex::Print() << "  Step " << step << " (t = " << time 
                               << " s): " << puffs.size() << " puffs\n";
                
                // Compute and write concentration field
                std::string step_file = puff_output + "_step" + std::to_string(step);
                
                // Create concentration grid
                std::vector<Real> concentration(nx * ny * nz, 0.0);
                
                for (int k = 0; k < nz; ++k) {
                    for (int j = 0; j < ny; ++j) {
                        for (int i = 0; i < nx; ++i) {
                            Real x = xmin + (i + 0.5) * dx;
                            Real y = ymin + (j + 0.5) * dy;
                            Real z = zmin + (k + 0.5) * dz;
                            
                            // Get terrain height at this point
                            Real terrain_height = 0.0;
                            if (enable_terrain_reflection && !x_terr.empty()) {
                                terrain_height = interpolate_terrain_height(
                                    x, y, x_terr, y_terr, z_terr);
                            }
                            
                            // Sum concentration from all puffs
                            Real C = 0.0;
                            for (const auto& puff : puffs) {
                                if (enable_terrain_reflection && use_image_source) {
                                    C += gaussian_puff_concentration_with_reflection(
                                        x, y, z, puff, terrain_height, true);
                                } else {
                                    C += gaussian_puff_concentration(x, y, z, puff);
                                }
                            }
                            
                            concentration[i + j * nx + k * nx * ny] = C;
                        }
                    }
                }
                
                // Write to file (simple ASCII format)
                std::ofstream outf(step_file);
                outf << "# Gaussian puff concentration field (step " << step << ")\n";
                outf << "# x [m], y [m], z [m], C [units/m³]\n";
                outf << std::scientific << std::setprecision(6);
                
                for (int k = 0; k < nz; ++k) {
                    for (int j = 0; j < ny; ++j) {
                        for (int i = 0; i < nx; ++i) {
                            Real x = xmin + (i + 0.5) * dx;
                            Real y = ymin + (j + 0.5) * dy;
                            Real z = zmin + (k + 0.5) * dz;
                            Real C = concentration[i + j * nx + k * nx * ny];
                            
                            outf << x << "," << y << "," << z << "," << C << "\n";
                        }
                    }
                }
                outf.close();
                
                amrex::Print() << "    Wrote concentration to " << step_file << "\n";
            }
        }
        
        // Count active puffs
        int n_active = 0;
        for (const auto& puff : puffs) {
            if (puff.active) n_active++;
        }
        
        amrex::Print() << "puff_solver: done.\n";
        amrex::Print() << "  Total puffs emitted: " << puffs.size() << "\n";
        amrex::Print() << "  Active puffs at end: " << n_active << "\n";
    }
    amrex::Finalize();
    return 0;
}
