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

using namespace amrex;

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
    std::unique_ptr<MultiFab>& vel_mf,
    std::unique_ptr<Geometry>& geom,
    amrex::Box& domain,
    int& ng)
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
        amrex::Print() << "  Initial puff size: σy₀ = " << sigma_y0 
                       << " m, σz₀ = " << sigma_z0 << " m\n";
        amrex::Print() << "  Wind: U = " << U_wind << ", V = " << V_wind 
                       << ", W = " << W_wind << " m/s\n";
        amrex::Print() << "  Time steps: " << n_steps_puff << " @ dt = " 
                       << dt_puff << " s\n";
        amrex::Print() << "  Grid: " << nx << " x " << ny << " x " << nz 
                       << " (" << dx << " x " << dy << " x " << dz << " m)\n";
        
        // ====================================================================
        // Time-stepping loop
        // ====================================================================
        
        std::vector<Puff> puffs;
        
        for (int step = 0; step < n_steps_puff; ++step) {
            Real time = step * dt_puff;
            
            // Emit new puff if still within emission duration
            if (time < emission_duration) {
                Real puff_mass = emission_rate * dt_puff;
                Puff new_puff = create_puff(
                    source_x, source_y, source_z,
                    puff_mass, sigma_y0, sigma_z0, time);
                puffs.push_back(new_puff);
            }
            
            // Advect, grow, and update all puffs
            for (auto& puff : puffs) {
                if (puff.active) {
                    // Advection with wind
                    advect_puff(puff, U_wind, V_wind, W_wind, dt_puff);
                    
                    // Growth due to diffusion
                    update_puff_growth(puff, K_h, K_v);
                    
                    // Update age
                    update_puff_age(puff, dt_puff);
                    
                    // Check bounds
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
                            
                            // Sum concentration from all puffs
                            Real C = 0.0;
                            for (const auto& puff : puffs) {
                                C += gaussian_puff_concentration(x, y, z, puff);
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
