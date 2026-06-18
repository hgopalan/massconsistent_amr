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
#include "turbine_wake_models.H"
#include "wire_models.H"

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
    std::unique_ptr<amrex::MultiFab> alpha_h_field; // Cell-local horizontal anisotropy field
    std::unique_ptr<amrex::MultiFab> alpha_v_field; // Cell-local vertical anisotropy field
    
    // Diagnostic flux fields
    std::unique_ptr<amrex::MultiFab> u_star;   // Friction velocity [m/s]
    std::unique_ptr<amrex::MultiFab> tau_flux; // Momentum flux [Pa]
    std::unique_ptr<amrex::MultiFab> cd;       // Drag coefficient [dimensionless]
    std::unique_ptr<amrex::MultiFab> shf;      // Sensible heat flux [W/m²]
    std::unique_ptr<amrex::MultiFab> lhf;      // Latent heat flux [W/m²]
    
    // 3D Scalar transport fields
    std::unique_ptr<amrex::MultiFab> temp_3d;      // 3D temperature field [K]
    std::unique_ptr<amrex::MultiFab> moisture_3d;  // 3D moisture field [kg/kg]
    std::unique_ptr<amrex::MultiFab> temp_3d_old;  // Temperature from previous time step [K]
    std::unique_ptr<amrex::MultiFab> moisture_3d_old; // Moisture from previous time step [kg/kg]
    
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
    
    // Polygon building support (optional - for arbitrary footprints, courtyards, etc.)
    std::vector<int> building_geom_type;     // BuildingGeometryType for each building
    std::vector<std::vector<amrex::Real>> building_polygon_x;  // Polygon vertices (if geom_type == POLYGON)
    std::vector<std::vector<amrex::Real>> building_polygon_y;  // Polygon vertices (if geom_type == POLYGON)
    std::vector<int> building_parent_id;     // Parent building ID (for grouping courtyards with main building)
    
    amrex::Real obs_max;  // Maximum obstacle elevation (terrain + buildings)
    
    // Solver parameters
    amrex::Real U_ref;     // Reference wind x-component [m/s]
    amrex::Real V_ref;     // Reference wind y-component [m/s]
    amrex::Real z_ref;     // Reference height above local terrain [m]
    amrex::Real z0;        // Aerodynamic roughness length [m]
    amrex::Real alpha_h;   // Horizontal Lagrange anisotropy factor
    amrex::Real alpha_v;   // Vertical Lagrange anisotropy factor
    amrex::Real idw_gamma; // Vertical scaling parameter for 3D IDW meteorological interpolation
    amrex::Real idw_exponent; // Exponent parameter for Inverse Distance Weighting (IDW) interpolation
    amrex::Real idw_rmax1;
    amrex::Real idw_rmax2;
    amrex::Real idw_r1;
    amrex::Real idw_r2;
    amrex::Real ekman_latitude;
    amrex::Real ekman_ug;
    amrex::Real ekman_vg;
    amrex::Real ekman_Km;
    
    // Cell-local spatially-varying anisotropy parameters
    bool enable_cell_local_anisotropy;
    std::string anisotropy_source;
    amrex::Real anisotropy_slope_scale;
    amrex::Real anisotropy_decay_height;
    amrex::Real anisotropy_ri_gamma;
    amrex::Real anisotropy_ri_beta;
    amrex::Real anisotropy_fr_min;
    amrex::Real temperature_gradient;
    
    // Canopy model parameters
    bool enable_canopy;                // Enable canopy model
    amrex::Real canopy_height;         // Canopy height [m]
    amrex::Real frontal_area_index;    // Frontal area index λf
    amrex::Real plan_area_index;       // Plan area index λp
    amrex::Real canopy_drag_coeff;     // Canopy drag coefficient Cd
    amrex::Real canopy_attenuation;    // Exponential attenuation coefficient α
    bool use_exponential_profile;      // Use Shaw-Pereira exponential profile

    // Morphometric model parameters
    bool enable_morphometric_models;
    std::string morphometric_model_type;
    amrex::Real morphometric_drag_coeff;
    
    // Ekman spiral veer parameters
    bool enable_ekman_veer;            // Enable Ekman spiral wind veer
    amrex::Real latitude;              // Latitude [degrees] for Coriolis parameter
    amrex::Real ekman_veer_total;      // Total veer from surface to top [degrees]
    amrex::Real ekman_veer_height;     // Height scale for veer profile [m]
    
    // Terrain-following (streamline) coordinates parameters
    bool enable_terrain_following;     // Enable terrain-following coordinates
    amrex::Real terrain_decay_height;  // Decay height scale for terrain-following transition [m]
    
    // Elevation scaling parameters
    bool enable_elevation_scaling;     // Enable elevation-dependent wind speed scaling
    amrex::Real elevation_scaling_factor;  // Wind speed change with elevation [1/m]
    amrex::Real elevation_height_scale;    // Characteristic height scale [m]
    
    // Land-use classification parameters
    bool enable_landuse_roughness;     // Enable land-use z0 classification
    std::string landuse_file;          // Path to land-use map file
    
    // Flux diagnostics parameters
    bool enable_flux_diagnostics;      // Compute SHF, LHF, τ, Cd fields
    bool flux_compute_sensible_heat;   // Compute sensible heat flux
    bool flux_compute_latent_heat;     // Compute latent heat flux
    amrex::Real flux_theta_star;       // Temperature scale for SHF [K]
    amrex::Real flux_q_star;           // Moisture scale for LHF [kg/kg]
    
    // Directional bias correction parameters
    bool enable_directional_bias;      // Enable directional bias correction
    amrex::Real bias_direction_offset; // Constant directional bias [degrees]
    amrex::Real bias_speed_factor;     // Constant speed bias factor [dimensionless]
    bool bias_periodic_enabled;        // Enable periodic directional bias
    amrex::Real bias_periodic_amplitude; // Periodic bias amplitude [degrees]
    
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
    bool enable_topographic_shielding;
    std::string init_mode;  // "loglaw", "uniform", "raws"
    amrex::Real uniform_U;
    amrex::Real uniform_V;
    std::string velocity_file;
    
    // Sounding profiles
    std::vector<std::string> sounding_files;
    std::vector<amrex::Real> sounding_x;
    std::vector<amrex::Real> sounding_y;
    std::string sounding_vertical_interp;
    bool sounding_wind_in_knots;

    // Single Column Model (SCM) parameters
    amrex::Real scm_wind_speed;           // Wind speed at reference height [m/s]
    amrex::Real scm_wind_direction;       // Wind direction [degrees, 0=N, 90=E, etc.]
    amrex::Real scm_ref_height;           // Height where wind speed is specified [m AGL]
    amrex::Real scm_ref_temperature;      // Reference temperature at surface [K]
    amrex::Real scm_lapse_rate;           // Temperature lapse rate [K/m]
    amrex::Real scm_domain_height;        // Domain height for SCM [m]
    amrex::Real scm_dz;                   // Uniform vertical spacing for SCM [m]
    amrex::Real scm_ug;                   // Computed geostrophic wind x-component [m/s] (output from recursion)
    amrex::Real scm_vg;                   // Computed geostrophic wind y-component [m/s] (output from recursion)

    // Marine Boundary Layer mixing height
    bool enable_marine_bl;
    amrex::Real marine_sst;
    amrex::Real marine_air_sea_dt;
    
    // Analytical Turbine Wake
    bool enable_turbine_wake;
    std::string turbine_file;
    std::string turbine_wake_model_type;
    std::string turbine_wake_superposition;
    amrex::Real jensen_kw;
    amrex::Real gaussian_ka;
    bool enable_stability_correction;
    amrex::Real stability_length;
    // Extended analytical wake parameters
    amrex::Real turbopark_c1;
    amrex::Real ambient_ti;
    bool enable_jimenez_deflection;
    bool enable_bastankhah_deflection;
    amrex::Real jimenez_kd;
    std::string wake_added_turbulence_model;
    // Advanced wake model parameters
    bool enable_wake_ground_interaction;
    amrex::Real wake_ground_damping_scale;
    amrex::Real surface_sensible_heat_flux;
    amrex::Real buoyant_wake_destruction_coeff;
    std::vector<TurbineWake::Turbine> turbines;

    // Electrical Wire Loading
    bool enable_wire_loading;
    std::string wire_file;
    std::string wire_output_file;
    std::vector<WireLoading::WireSpan> wires;

    // Solver status
    bool solved;
    int mlmg_iters;
    amrex::Real mlmg_res;
    
    // 3D Scalar transport parameters
    bool enable_3d_scalars = false;              // Enable 3D temperature and moisture fields
    bool enable_temperature_transport = false;   // Enable temperature transport equation solving
    bool enable_moisture_transport = false;      // Enable moisture transport equation solving
    amrex::Real temperature_diffusivity = 2.5e-5; // Thermal diffusivity [m²/s]
    amrex::Real moisture_diffusivity = 2.2e-5;   // Moisture diffusivity [m²/s]
    amrex::Real scalar_dt = -1.0;                // Time step for scalar transport (negative = auto-compute via CFL)
    amrex::Real scalar_cfl = 0.8;                // CFL number for scalar transport
    int multi_step_corrector_steps = 1;          // Number of corrector iterations for temperature-dependent mass-consistent solve
    
    // Mixing length model parameters for eddy diffusivity
    bool enable_mixing_length_turbulence = true; // Use mixing length model for eddy diffusivity
    amrex::Real mixing_length_coefficient = 0.1; // Proportionality constant for mixing length: l = coeff * Δz
    amrex::Real von_karman = 0.41;               // Von Kármán constant
    amrex::Real zground = 0.1;                   // Ground roughness for mixing length calculation [m]
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

// Turbine API functions
bool wind_solver_add_turbine(double x, double y, double hub_height, double rotor_diameter, double default_ct, const std::string& power_curve_file, double yaw = 0.0, double orientation = 0.0, double tilt = 0.0);
void wind_solver_clear_turbines();
std::vector<double> wind_solver_get_turbine_power_outputs();
std::vector<double> wind_solver_get_turbine_inflow_speeds();
std::vector<double> wind_solver_get_turbine_yaws();
std::vector<double> wind_solver_get_turbine_orientations();
std::vector<double> wind_solver_get_turbine_tilts();
std::vector<double> wind_solver_get_turbine_u_hubs();
std::vector<double> wind_solver_get_turbine_v_hubs();
std::vector<double> wind_solver_get_turbine_z_terrains();

#endif // WIND_SOLVER_API_H
