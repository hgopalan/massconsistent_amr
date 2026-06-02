// ============================================================================
// wind_solver_api.H
// API for Python bindings to control mass-consistent wind solver from Python
//
// This header provides functions to:
// - Initialize the wind solver from an inputs file
// - Solve for the mass-consistent wind field
// - Extract MultiFab data (velocity, lambda, terrain, etc.) as numpy arrays
// - Update boundary conditions or parameters
// - Clean up solver state
//
// The solver state is stored in a global singleton structure to persist
// between Python function calls.
// ============================================================================

#ifndef WIND_SOLVER_API_H
#define WIND_SOLVER_API_H

#include <AMReX.H>
#include <AMReX_Geometry.H>
#include <AMReX_MultiFab.H>
#include <AMReX_BoxArray.H>
#include <AMReX_DistributionMapping.H>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// WindSolverState
//
// Singleton structure that holds all solver state between Python calls.
// This allows initialization once, then multiple solves or parameter updates.
// ============================================================================
struct WindSolverState {
    // Initialization flag
    bool initialized = false;
    
    // Geometry and distribution
    std::unique_ptr<amrex::Geometry> geom;
    std::unique_ptr<amrex::BoxArray> ba;
    std::unique_ptr<amrex::DistributionMapping> dm;
    
    // Field data
    std::unique_ptr<amrex::MultiFab> vel;      // 3-component velocity (u, v, w)
    std::unique_ptr<amrex::MultiFab> vel0;     // Initial (uncorrected) velocity
    std::unique_ptr<amrex::MultiFab> lambda;   // Lagrange multiplier
    std::unique_ptr<amrex::MultiFab> div0;     // Initial divergence
    std::unique_ptr<amrex::MultiFab> terrain;  // Terrain elevation
    
    // Grid parameters
    int nx, ny, nz;
    double dx, dy, dz;
    double xmin, xmax, ymin, ymax, zmin, zmax;
    
    // Terrain data
    std::vector<amrex::Real> terrain_x_data;
    std::vector<amrex::Real> terrain_y_data;
    std::vector<amrex::Real> terrain_z_data;
    amrex::Real zs_min;  // Minimum terrain elevation
    amrex::Real zs_max;  // Maximum terrain elevation
    
    // Building data
    std::vector<amrex::Real> building_xmin;
    std::vector<amrex::Real> building_xmax;
    std::vector<amrex::Real> building_ymin;
    std::vector<amrex::Real> building_ymax;
    std::vector<amrex::Real> building_zmin;
    std::vector<amrex::Real> building_zmax;
    amrex::Real obs_max;  // Maximum obstacle elevation (terrain + buildings)
    
    // Solver parameters
    amrex::Real U_ref;     // Reference wind x-component [m/s]
    amrex::Real V_ref;     // Reference wind y-component [m/s]
    amrex::Real z_ref;     // Reference height above local terrain [m]
    amrex::Real z0;        // Aerodynamic roughness length [m]
    amrex::Real alpha_h;   // Horizontal Lagrange anisotropy factor
    amrex::Real alpha_v;   // Vertical Lagrange anisotropy factor
    
    // Canopy model parameters
    bool enable_canopy;                // Enable canopy model
    amrex::Real canopy_height;         // Canopy height [m]
    amrex::Real frontal_area_index;    // Frontal area index λf
    amrex::Real plan_area_index;       // Plan area index λp
    amrex::Real canopy_drag_coeff;     // Canopy drag coefficient Cd
    amrex::Real canopy_attenuation;    // Exponential attenuation coefficient α
    bool use_exponential_profile;      // Use Shaw-Pereira exponential profile
    
    // Ekman spiral veer parameters
    bool enable_ekman_veer;            // Enable Ekman spiral wind veer
    amrex::Real latitude;              // Latitude [degrees] for Coriolis parameter
    amrex::Real ekman_veer_total;      // Total veer from surface to top [degrees]
    amrex::Real ekman_veer_height;     // Height scale for veer profile [m]
    
    // Terrain-following (streamline) coordinates parameters
    bool enable_terrain_following;     // Enable terrain-following coordinates
    amrex::Real terrain_decay_height;  // Decay height scale for terrain-following transition [m]
    
    // MLMG solver parameters
    int mlmg_verbose;
    amrex::Real tol_rel;
    amrex::Real tol_abs;
    int max_iter;
    
    // Output parameters
    std::string plot_file;
    std::string extract_file;
    amrex::Real extract_agl;
    int extract_k;
    
    // Initialization mode
    std::string init_mode;  // "loglaw", "uniform", "raws"
    amrex::Real uniform_U;
    amrex::Real uniform_V;
    std::string velocity_file;
    
    // Solver status
    bool solved;
    int mlmg_iters;
    amrex::Real mlmg_res;
};

// Global singleton instance
extern std::unique_ptr<WindSolverState> g_wind_solver_state;

// ============================================================================
// API Functions
// ============================================================================

// Initialize the wind solver from an inputs file
// Returns true on success, false on failure
bool wind_solver_initialize(const std::string& inputs_file);

// Solve for the mass-consistent wind field
// Returns true on success, false on failure
bool wind_solver_solve();

// Get current solver status (solved, iterations, residual)
void wind_solver_get_status(bool& solved, int& iters, double& residual);

// Get geometry information (grid dimensions, domain bounds, cell sizes)
void wind_solver_get_geometry(
    int& nx, int& ny, int& nz,
    double& xmin, double& xmax,
    double& ymin, double& ymax,
    double& zmin, double& zmax,
    double& dx, double& dy, double& dz);

// Get terrain bounds
void wind_solver_get_terrain_bounds(double& zs_min, double& zs_max);

// Update reference wind (re-initializes velocity field)
// Returns true on success, false on failure
bool wind_solver_update_reference_wind(double U_ref, double V_ref);

// Update solver parameters (anisotropy factors, tolerances, etc.)
bool wind_solver_update_parameters(
    double alpha_h, double alpha_v,
    double tol_rel, int max_iter);

// Extract velocity field as flattened arrays
// Data returned in Fortran (column-major) order for numpy compatibility
// u_data, v_data, w_data must be pre-allocated to size nx*ny*nz
void wind_solver_get_velocity(
    std::vector<double>& u_data,
    std::vector<double>& v_data,
    std::vector<double>& w_data);

// Extract initial (uncorrected) velocity field
void wind_solver_get_velocity0(
    std::vector<double>& u_data,
    std::vector<double>& v_data,
    std::vector<double>& w_data);

// Extract Lagrange multiplier field
std::vector<double> wind_solver_get_lambda();

// Extract divergence of initial velocity field
std::vector<double> wind_solver_get_div0();

// Extract terrain elevation field (2D, nx*ny)
std::vector<double> wind_solver_get_terrain();

// Extract velocity at a specific height above ground level (AGL)
// Returns velocity at the specified AGL height for all (x,y) positions
// u_data, v_data, w_data will be resized to nx*ny
void wind_solver_get_velocity_at_agl(
    double agl_height,
    std::vector<double>& u_data,
    std::vector<double>& v_data,
    std::vector<double>& w_data);

// Extract velocity at a specific k-index
// u_data, v_data, w_data will be resized to nx*ny
void wind_solver_get_velocity_at_k(
    int k,
    std::vector<double>& u_data,
    std::vector<double>& v_data,
    std::vector<double>& w_data);

// Write plotfile
bool wind_solver_write_plotfile(const std::string& plotfile_name);

// Write terrain-aligned CSV extract
bool wind_solver_write_extract(const std::string& extract_filename, double agl_height);

// Clean up and finalize the wind solver
void wind_solver_finalize();

// Check if solver is initialized
bool wind_solver_is_initialized();

#endif // WIND_SOLVER_API_H
