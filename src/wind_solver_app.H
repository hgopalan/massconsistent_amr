#ifndef WIND_SOLVER_APP_H
#define WIND_SOLVER_APP_H

#include <AMReX.H>
#include <AMReX_Geometry.H>
#include <AMReX_MultiFab.H>
#include <vector>
#include <string>
#include <memory>

#include "synthetic_turbulence.H"
#include "turbine_wake_models.H"
#include "wire_models.H"
#include "bridge_models.H"
#include "structure_models.H"
#include "thermodynamic_lid_models.H"
#include "datacenter_heat_source.H"

namespace amrex {
    class EBFArrayBoxFactory;
}

/**
 * @brief Representation of a detected urban street canyon.
 * 
 * Defines the physical boundaries and characteristics of a street canyon formed
 * between two parallel buildings.
 */
struct StreetCanyon {
    amrex::Real xmin, xmax;   ///< Coordinates of the canyon width/span boundaries [m]
    amrex::Real ymin, ymax;   ///< Coordinates of the canyon length boundaries [m]
    amrex::Real zmin, zmax;   ///< Height limits of the canyon [m]
    amrex::Real aspect_ratio; ///< Aspect ratio of the canyon (height / width)
    int direction;            ///< Orientation: 0 for Y-aligned (parallel buildings separated in X, wind along X), 
                              ///<              1 for X-aligned (parallel buildings separated in Y, wind along Y)
    amrex::Real h_canyon;     ///< Height of the canyon walls [m]
    amrex::Real w_canyon;     ///< Width of the canyon street [m]
};

class WindSolverApp {
public:
    WindSolverApp() = default;
    ~WindSolverApp() = default;

    void initialize();
    void execute();

    void parse_inputs();
    void setup_geometry_and_mesh();
    void allocate_data_fields();
    void initialize_wind_fields(int time_step);
    void execute_poisson_solve(int time_step);
    void apply_divergence_corrections(int time_step);
    void compute_diagnostics_and_output(int time_step);
    
    // 3D Scalar transport methods
    void solve_transport_equations(int time_step, amrex::Real dt_transport);
    void recalculate_wind_after_transport(int time_step);
    void solve_scalar_transport(
        amrex::MultiFab& scalar_new,
        const amrex::MultiFab& scalar_old,
        const amrex::MultiFab& vel,
        amrex::Real diffusivity,
        amrex::Real dt,
        const std::string& scalar_name);
    void compute_eddy_diffusivity_mixing_length(
        amrex::MultiFab& kappa_eddy);
    amrex::Real compute_adaptive_dt_transport();

    // Double-pass turbulent stress correction
    void compute_turbulent_velocity_increment(amrex::MultiFab& vel_increment);
    void apply_turbulent_stress_correction(int time_step);
    
    // Data center heat source methods
    void apply_datacenter_heat_source(
        amrex::MultiFab& temp_source,
        amrex::Real dt);
    void compute_datacenter_plume_diagnostics();

private:
    std::vector<StreetCanyon> detect_street_canyons(
        const std::vector<amrex::Real>& b_xmin, const std::vector<amrex::Real>& b_xmax,
        const std::vector<amrex::Real>& b_ymin, const std::vector<amrex::Real>& b_ymax,
        const std::vector<amrex::Real>& b_zmin, const std::vector<amrex::Real>& b_zmax,
        const std::vector<amrex::Real>& b_rotation);
    
    /**
     * @brief Validate configuration for conflicting or incompatible capabilities.
     *        Issues warnings or errors if incompatible features are enabled together.
     * @throws std::runtime_error if critical conflicts are found
     */
    void validate_configuration();

    // Timing
    amrex::Real t_total = 0.0;
    amrex::Real t_phase = 0.0;

    // --- INPUT PARAMETERS ---
    std::string terrain_file = "terrain.csv";
    std::string init_mode = "loglaw";
    amrex::Real U_ref = 10.0;
    amrex::Real V_ref =  0.0;
    amrex::Real z_ref = 10.0;
    amrex::Real z0    =  0.1;

    // Single Column Model (SCM) parameters
    amrex::Real scm_wind_speed = 10.0;           // Wind speed at reference height [m/s]
    amrex::Real scm_wind_direction = 270.0;      // Wind direction [degrees, 0=N, 90=E, etc.]
    amrex::Real scm_ref_height = 10.0;           // Height where wind speed is specified [m AGL]
    amrex::Real scm_ref_temperature = 288.15;    // Reference temperature at surface [K]
    amrex::Real scm_lapse_rate = 0.0065;         // Temperature lapse rate [K/m]
    amrex::Real scm_domain_height = 4000.0;      // Domain height for 1D SCM [m]
    amrex::Real scm_dz = 4.0;                    // Vertical resolution for 1D SCM [m]
    amrex::Real scm_heat_flux = 0.0;             // Optional surface sensible heat flux [W/m^2]
    amrex::Real scm_monin_obukhov_length = -1.0e30; // Optional Monin-Obukhov length [m]
    amrex::Real scm_ug = 0.0;                    // Geostrophic u-wind (computed)
    amrex::Real scm_vg = 0.0;                    // Geostrophic v-wind (computed)
    std::string scm_turbulence_model = "one_equation"; // SCM turbulence model: "one_equation", "ysu", "myj"
    bool scm_enable_microphysics = false;         // Enable SCM simple microphysics (saturation adjustment)
    amrex::Real scm_initial_humidity = 0.0;       // SCM initial relative humidity [0.0 - 1.0]

    // Canopy
    bool enable_canopy = false;
    std::string canopy_file = "";
    amrex::Real canopy_height = 0.0;
    amrex::Real frontal_area_index = 0.0;
    amrex::Real plan_area_index = 0.0;
    amrex::Real canopy_drag_coeff = 0.2;
    amrex::Real canopy_attenuation = 2.5;
    bool use_exponential_profile = false;
    int canopy_profile_type = 0;

    // Morphometric Models
    bool enable_morphometric_models = false;
    std::string morphometric_model_type = "macdonald";
    amrex::Real morphometric_drag_coeff = -1.0;

    // Windbreaks
    bool enable_windbreaks = false;
    std::string windbreaks_file = "";

    // Wake
    bool enable_wake = false;
    std::string wake_model_type = "rockle";
    amrex::Real wake_c1 = 0.9;
    amrex::Real wake_c2 = 0.3;
    amrex::Real wake_separation_length = 3.0;
    bool wake_superposition = true;
    
    // Wake model enhancement parameters
    bool enable_oblique_scaling = true;
    bool enable_tall_building_correction = true;
    bool enable_gaussian_profile = false;
    bool enable_upwind_recirculation = true;
    bool enable_reference_correction = false;
    bool enable_corner_acceleration = true;
    bool enable_variance_correction = false;
    bool enable_horseshoe_vortex = true;
    bool enable_extended_farwake = true;
    bool enable_yoshie_two_layer = true;
    amrex::Real yoshie_decay_beta = amrex::Real(1.75);
    bool enable_rodi_entrainment = true;
    amrex::Real rodi_ce_coefficient = amrex::Real(1.0);
    bool enable_lopes_comfort = true;
    amrex::Real lopes_comfort_threshold = amrex::Real(5.0);
    amrex::Real lopes_assessment_height = amrex::Real(1.5);
    amrex::Real lopes_reference_frequency = amrex::Real(0.02);
    bool enable_oikonomou_aspect = true;
    amrex::Real oikonomou_beta_aspect = amrex::Real(0.25);
    bool enable_britter_hanna_urban = true;
    amrex::Real britter_hanna_alpha = amrex::Real(0.15);

    // Analytical Turbine Wake
    bool enable_turbine_wake = false;
    std::string turbine_file = "";
    std::string turbine_wake_model_type = "jensen";
    std::string turbine_wake_superposition = "quadratic";
    amrex::Real jensen_kw = 0.075;
    amrex::Real gaussian_ka = 0.05;
    // Extended analytical wake parameters
    amrex::Real turbopark_c1 = 0.38;
    amrex::Real ambient_ti = 0.075;
    bool enable_jimenez_deflection = false;
    bool enable_bastankhah_deflection = false;
    amrex::Real jimenez_kd = 0.05;
    std::string wake_added_turbulence_model = "none";
    // Advanced wake model parameters
    bool enable_wake_ground_interaction = true;
    amrex::Real wake_ground_damping_scale = 0.25;
    amrex::Real surface_sensible_heat_flux = 0.0;
    amrex::Real buoyant_wake_destruction_coeff = 0.005;
    std::vector<TurbineWake::Turbine> turbines;

    // Electrical Wire Loading
    bool enable_wire_loading = false;
    std::string wire_file = "";
    std::string wire_output_file = "wire_output.csv";
    std::vector<WireLoading::WireSpan> wires;

    // Bridge Loading Assessment
    bool enable_bridge_loading = false;
    std::string bridge_file = "";
    std::string bridge_output_file = "bridge_output.csv";
    std::vector<BridgeLoading::BridgeSpan> bridges;

    // General Structure Loading Assessment (buildings, towers, antennas)
    bool enable_structure_loading = false;
    std::string structure_file = "";
    std::string structure_output_file = "structure_output.csv";
    std::vector<StructureLoading::GeneralStructure> structures;

    // Street canyon
    bool enable_street_canyon = false;
    amrex::Real street_canyon_reduction = 0.3;

    // Uniform mode
    amrex::Real uniform_U = 10.0;
    amrex::Real uniform_V = 0.0;

    // Power-law
    amrex::Real powerlaw_exponent = 0.143;
    std::string landuse_file = "";
    bool use_landuse_powerlaw = false;

    // RAWS
    std::string velocity_file = "velocity.csv";

    // Sounding profiles
    std::vector<std::string> sounding_files;
    std::vector<amrex::Real> sounding_x;
    std::vector<amrex::Real> sounding_y;
    std::string sounding_vertical_interp = "spline"; // "spline" or "log_linear"
    bool sounding_wind_in_knots = true;

    // Surface data
    std::string surface_data_file = "surface_data.csv";

    // Windfield data
    std::string windfield_file = "windfield.csv";
     
    // Cached windfield data for memory-based wind-scalar coupling
    // When init_mode == "windfield" and scalar transport is enabled,
    // we cache the windfield data on first read to avoid re-reading from disk
    // on every time step in unsteady mode
    std::vector<amrex::Real> cached_x_wf, cached_y_wf, cached_z_wf;
    std::vector<amrex::Real> cached_ux_wf, cached_uy_wf, cached_uz_wf;
    bool windfield_data_cached = false;
 
    // Roughness file
    std::string z0_file = "";
    bool use_z0_file = false;

    // Vegetation
    bool enable_vegetation_roughness = false;
    amrex::Real vegetation_state = 1.0;
    int vegetation_state_type = 0;

    // Spacings
    amrex::Real dx_req = 30.0;
    amrex::Real dy_req = 30.0;
    amrex::Real dz_req = 30.0;
    amrex::Real domain_height = 300.0;

    // Anisotropy
    amrex::Real alpha_h = 1.0;
    amrex::Real alpha_v = 1.0;
    bool use_height_dependent_alpha_v = false;
    amrex::Real alpha_v_surface = 1.0;
    amrex::Real alpha_v_top = 1.0;
    bool enable_cell_local_anisotropy = false;
    std::string anisotropy_source = "all";
    amrex::Real anisotropy_slope_scale = 0.25;
    amrex::Real anisotropy_decay_height = 500.0;
    amrex::Real anisotropy_ri_gamma = 1.0;
    amrex::Real anisotropy_ri_beta = 0.5;
    amrex::Real anisotropy_fr_min = 0.1;
    amrex::Real idw_gamma = 1.0;
    amrex::Real idw_exponent = 2.0;
    amrex::Real idw_rmax1 = -1.0;
    amrex::Real idw_rmax2 = -1.0;
    amrex::Real idw_r1 = -1.0;
    amrex::Real idw_r2 = -1.0;

    // Stability
    bool enable_stability_correction = false;
    amrex::Real stability_length = 1000.0;
    bool use_holtslag_stability = false;
    bool enable_pg_stability = false;
    amrex::Real solar_radiation = 400.0;
    bool is_nighttime = false;
    amrex::Real cloud_cover = 0.5;

    // Atmospheric Inversion Capping Lid
    bool enable_capping_lid = false;
    amrex::Real capping_lid_height = 1000.0;
    ThermodynamicLidParams thermo_lid_params;
    std::vector<amrex::Real> thermo_lid_flux_times;
    std::vector<amrex::Real> thermo_lid_flux_values;

    // Elevation scaling
    bool enable_elevation_scaling = false;
    amrex::Real elevation_scaling_factor = 0.0;
    amrex::Real elevation_height_scale = 1000.0;

    // Orographic speedup
    bool enable_orographic_speedup = false;
    amrex::Real orographic_hill_length_scale = 100.0;
    amrex::Real orographic_speedup_factor_max = 2.0;
    amrex::Real orographic_separation_factor = 0.3;
    amrex::Real orographic_smoothing_factor = 0.5;

    // Thermal circulation
    bool enable_thermal_circulation = false;
    amrex::Real thermal_temperature_contrast = 0.0;
    amrex::Real thermal_reference_temperature = 300.0;
    amrex::Real thermal_coefficient = 1.0;
    amrex::Real thermal_vertical_decay_height = 1000.0;
    amrex::Real thermal_distance_scale = 5000.0;
    amrex::Real thermal_coastline_x = 0.0;
    amrex::Real thermal_coastline_y = 0.0;
    amrex::Real thermal_coast_normal_x = 1.0;
    amrex::Real thermal_coast_normal_y = 0.0;
    std::string land_sea_mask_file = "";

    // Terrain blocking
    bool enable_topographic_shielding = false;
    bool enable_terrain_blocking = false;
    amrex::Real terrain_blocking_brunt_vaisala_frequency = 0.01;
    amrex::Real terrain_blocking_reduction_factor = 0.5;
    amrex::Real terrain_blocking_transition_froude = 1.0;
    amrex::Real terrain_blocking_flank_enhancement = 1.2;
    amrex::Real terrain_blocking_lapse_rate = 0.0065;
    amrex::Real terrain_blocking_reference_temperature = 288.0;

    // Slope flows
    bool enable_slope_flows = false;
    amrex::Real slope_flow_temperature_diff = 0.0;
    amrex::Real slope_flow_reference_temperature = 300.0;
    amrex::Real slope_flow_empirical_coefficient = 2.5;
    amrex::Real slope_flow_vertical_decay_height = 50.0;
    amrex::Real slope_flow_min_slope = 0.05;

    // Valley channeling
    bool enable_valley_channeling = false;
    amrex::Real valley_axis_angle_deg = 0.0;
    amrex::Real valley_width = 1000.0;
    amrex::Real valley_depth = 300.0;
    amrex::Real valley_channeling_strength_max = 0.8;
    amrex::Real valley_speedup_factor_narrow = 1.3;
    amrex::Real valley_slowdown_factor_wide = 0.85;

    // Gap flow
    bool enable_gap_flow = false;
    amrex::Real gap_flow_orientation = 0.0;
    amrex::Real gap_flow_width = 1000.0;
    amrex::Real gap_flow_depth = 500.0;
    amrex::Real gap_flow_pressure_coefficient = 1.0;
    amrex::Real gap_flow_speedup_max = 3.0;
    amrex::Real gap_flow_center_x = 0.0;
    amrex::Real gap_flow_center_y = 0.0;
    amrex::Real gap_flow_transition_width = 500.0;
    amrex::Real gap_flow_vertical_extent = 1000.0;

    // Time-varying
    bool enable_time_varying = false;
    std::string time_series_file = "time_series.csv";
    bool enable_data_assimilation = false;

    // Building porosity
    bool enable_building_porosity = false;
    std::string building_porosity_file = "";
    amrex::Real default_building_porosity = 0.0;
    amrex::Real porosity_drag_coefficient = 0.2;

    // Wall functions
    bool enable_wall_functions = false;
    bool enable_terrain_wall_function = false;
    bool enable_flat_surface_wall_function = false;
    bool enable_building_wall_function = false;
    amrex::Real wall_function_z0_building = 0.001;
    amrex::Real wall_function_z0_flat = 0.01;
    amrex::Real wall_function_blend_height = 2.0;
    amrex::Real wall_function_max_distance = 3.0;
    amrex::Real wall_function_flat_surface_elevation = 0.0;
    bool wall_function_enable_flat_surface = false;
    amrex::Real wall_function_min_wall_distance = 0.1;
    bool wall_function_enable_stability = false;
    amrex::Real wall_function_stability_length = 1.0e10;
    bool wall_function_enable_adaptive = false;
    amrex::Real wall_function_adaptive_threshold = 30.0;
    amrex::Real wall_function_adaptive_min_cells = 3.0;

    // Thermal Stratification with Buoyancy
    bool enable_buoyancy_stratification = false;
    std::string temperature_file = "temperature.csv";
    amrex::Real temperature_reference = 300.0;
    amrex::Real buoyancy_coefficient = 1.0;
    amrex::Real buoyancy_timescale = 10.0;
    std::string buoyancy_method = "velocity";

    // Simple Diurnal Temperature
    bool enable_diurnal_temperature = false;
    amrex::Real diurnal_temperature_amplitude = 5.0;
    amrex::Real diurnal_time_of_day = 12.0;
    amrex::Real diurnal_phase_hour = 14.0;
    amrex::Real diurnal_period = 24.0;

    // Kinematic BC
    bool enable_terrain_kinematic_bc = false;
    amrex::Real terrain_bc_relaxation = 1.0;

    // Ekman Veer
    bool enable_ekman_veer = false;
    amrex::Real latitude = 45.0;
    amrex::Real ekman_veer_total = 20.0;
    amrex::Real ekman_veer_height = 200.0;
    amrex::Real ekman_veer_total_rad = 0.0;

    // Analytical Ekman Spiral vertical profile initialization parameters
    amrex::Real ekman_latitude = 45.0;
    amrex::Real ekman_ug = 10.0;
    amrex::Real ekman_vg = 0.0;
    amrex::Real ekman_Km = 5.0;

    // Wind Direction Gradient
    bool enable_wind_direction_gradient = false;
    amrex::Real wind_direction_shear_rate = 0.0;
    amrex::Real wind_direction_shear_rate_rad = 0.0;

    // Spatial Alpha
    bool use_spatial_alpha_coefficients = false;
    std::string alpha_coefficients_file = "";

    // Fetch dependent
    bool enable_fetch_roughness_transition = false;
    amrex::Real fetch_transition_blending_height = 100.0;

    // Divergence source
    bool enable_divergence_source = false;
    std::string divergence_source_file = "";
    amrex::Real divergence_source_constant = 0.0;

    // Diurnal roughness
    bool enable_diurnal_roughness = false;
    amrex::Real roughness_amplitude = 0.3;
    amrex::Real roughness_phase_offset = 0.0;

    // Sky View Factor and Solar Shading
    // Unified terrain+building approach for computing SVF and shading
    bool enable_sky_view_factor = false;
    bool enable_solar_shading = false;
    amrex::Real latitude_degrees = 40.0;      // Default: Boulder, CO
    amrex::Real longitude_degrees = -105.0;   // Default: Boulder, CO
    amrex::Real day_of_year = 172.0;          // Default: June 21 (summer solstice)
    amrex::Real hour_of_day = 12.0;           // Default: Noon
    amrex::Real max_horizon_distance = 1000.0; // Max distance for horizon check [m]

    // BL decay
    bool enable_bl_decay = false;
    amrex::Real bl_depth_param = 1000.0;
    amrex::Real decay_height_scale = 1000.0;
    amrex::Real bl_transition_height = 200.0;

    // BL depth diagnostic
    bool enable_bl_depth_diagnostic = false;
    amrex::Real richardson_critical = 0.25;
    amrex::Real richardson_min_wind_shear = 0.001;

    // Froude scaling
    bool enable_froude_height_scaling = false;

    // Ageostrophic balance
    bool enable_ageostrophic_balance = false;
    amrex::Real ageostrophic_latitude = 45.0;
    amrex::Real ageostrophic_pressure_grad_x = 0.0;
    amrex::Real ageostrophic_pressure_grad_y = 0.0;
    amrex::Real ageostrophic_air_density = 1.225;
    amrex::Real ageostrophic_fraction = 0.1;

    // Time-varying thermal circulation
    bool enable_time_varying_thermal_amplitude = false;
    std::string thermal_amplitude_file = "";
    amrex::Real thermal_amplitude_time_of_day = 12.0;

    // MLMG solver
    int mlmg_verbose = 1;
    amrex::Real tol_rel = 1.e-8;
    int mlmg_max_iter = 200;
    int mlmg_max_fmg_iter = 20;
    int mlmg_pre_smooth = 16;
    int mlmg_post_smooth = 16;
    std::string mlmg_bottom_solver = "default";
    int max_grid_size = 32;
    std::string plot_file = "plt_wind";
    std::vector<std::string> plot_vars;

    // Flux diagnostics
    bool enable_flux_diagnostics = false;
    bool flux_compute_sensible_heat = false;
    bool flux_compute_latent_heat = false;
    amrex::Real flux_theta_star = 1.0;
    amrex::Real flux_q_star = 1.0e-3;
    amrex::Real surface_temperature = 288.15;
    amrex::Real heat_flux_scale = 1.0;
    amrex::Real relative_humidity = 0.5;

    // Landuse classification
    bool enable_landuse_roughness = false;
    std::string landuse_file_class = "";
    amrex::Real charnock_alpha = 0.011;
    bool enable_mosaic_roughness = false;

    // Marine Boundary Layer mixing height
    bool enable_marine_bl = false;
    amrex::Real marine_sst = 288.15;
    amrex::Real marine_air_sea_dt = 0.0;

    // Precipitation
    std::string precipitation_file = "";
    std::vector<amrex::Real> precipitation_times;
    std::vector<amrex::Real> precipitation_rates;
    amrex::Real current_precipitation_rate = 0.0;
    amrex::Real precipitation_stability_threshold = 1.0;

    // Directional bias
    bool enable_directional_bias = false;
    amrex::Real bias_direction_offset = 0.0;
    amrex::Real bias_speed_factor = 1.0;
    bool bias_periodic_enabled = false;
    amrex::Real bias_periodic_amplitude = 0.0;

    // Simplified Richardson
    bool enable_simplified_richardson = false;
    bool use_golder_curves = true;

    // Roughness blocking
    bool enable_roughness_blocking = false;
    amrex::Real building_roughness_factor = 0.04;

    // Coriolis latitude scaling
    bool enable_coriolis_latitude = false;
    amrex::Real domain_latitude = 45.0;

    // Power-law profile above BL
    bool enable_power_law_profile = false;
    amrex::Real power_law_exponent = 0.15;

    // Heat flux diagnostic
    bool enable_heat_flux_diagnostics = false;
    amrex::Real heat_flux_theta_star = 0.1;

    // Divergence damping filter
    bool enable_divergence_damping = false;
    amrex::Real damping_coefficient = -1.0;
    amrex::Real damping_coefficient_h = -1.0;
    amrex::Real damping_coefficient_v = -1.0;
    int damping_iterations = 1;

    // Perturbation pressure gradient
    bool enable_perturbation_pressure = false;
    amrex::Real pressure_tol_rel = 1.0e-6;
    int pressure_max_iter = 100;
    amrex::Real pressure_scale = 0.5;

    // O'Brien Vertical Velocity Adjustment
    bool enable_obrien_w_adjustment = false;

    // Multi-scale terrain analysis
    bool enable_terrain_analysis = false;
    amrex::Real slope_threshold_moderate = 0.1;
    amrex::Real slope_threshold_steep = 0.3;
    amrex::Real roughness_factor_moderate = 0.2;
    amrex::Real roughness_factor_steep = 0.8;
    amrex::Real transition_zone_width = 0.02;

    // Surface-to-mixed transition smoothing
    bool enable_transition_smoothing = false;
    amrex::Real transition_height_scale = 100.0;
    amrex::Real transition_layer_height = 300.0;
    
    // 3D Scalar transport parameters
    bool enable_3d_scalars = false;              // Enable 3D temperature and moisture fields
    bool enable_temperature_transport = false;   // Enable temperature transport equation solving
    bool enable_moisture_transport = false;      // Enable moisture transport equation solving
    amrex::Real temperature_diffusivity = 2.5e-5; // Thermal diffusivity [m²/s]
    amrex::Real moisture_diffusivity = 2.2e-5;   // Moisture diffusivity [m²/s]
    amrex::Real scalar_dt = -1.0;                // Time step for scalar transport (negative = auto-compute via CFL)
    amrex::Real scalar_cfl = 0.8;                // CFL number for scalar transport
    int multi_step_corrector_steps = 1;          // Number of corrector iterations for temperature-dependent mass-consistent solve
    std::string scalar_coupling_mode = "segregated"; // Mode for scalar transport: "segregated" or "coupled"
     
    // Temperature-wind recalculation parameters (coupled feedback after transport)
    bool enable_temperature_wind_recalculation = false; // Enable wind recalculation after temperature transport
    int temperature_wind_recalc_iterations = 2;         // Number of iterations for temperature-wind coupling
    amrex::Real temperature_wind_recalc_tolerance = 0.01; // Convergence tolerance for w-velocity [m/s]
     
    // Data center heat source parameters
    bool datacenter_enabled = false;             // Enable data center heat source modeling
    std::vector<amrex::Real> datacenter_heat_release;  // Heat release rates [W] for each facility
    std::vector<amrex::Real> datacenter_x;      // X-coordinates of facility centers [m]
    std::vector<amrex::Real> datacenter_y;      // Y-coordinates of facility centers [m]
    std::vector<amrex::Real> datacenter_z;      // Z-coordinates/heights of facilities [m]
    std::vector<amrex::Real> datacenter_area;   // Facility footprint areas [m²]
    std::vector<amrex::Real> datacenter_sigma_x; // Gaussian spreads in X [m]
    std::vector<amrex::Real> datacenter_sigma_y; // Gaussian spreads in Y [m]
    std::vector<amrex::Real> datacenter_sigma_z; // Gaussian spreads in Z [m]
    std::vector<std::string> datacenter_names;  // Names/identifiers for facilities
    
    // Legacy single-center parameters (for backward compatibility)
    // These are mapped to the first element of the vectors
    amrex::Real datacenter_heat_release_single = 0.0;  // Single center heat release [W] (deprecated, use vector)
    amrex::Real datacenter_x_single = 0.0;             // Single center X [m] (deprecated, use vector)
    amrex::Real datacenter_y_single = 0.0;             // Single center Y [m] (deprecated, use vector)
    amrex::Real datacenter_z_single = 10.0;            // Single center Z [m] (deprecated, use vector)
    amrex::Real datacenter_area_single = 1000.0;       // Single center area [m²] (deprecated, use vector)
    amrex::Real datacenter_sigma_x_single = 50.0;      // Single center sigma_x [m] (deprecated, use vector)
    amrex::Real datacenter_sigma_y_single = 50.0;      // Single center sigma_y [m] (deprecated, use vector)
    amrex::Real datacenter_sigma_z_single = 5.0;       // Single center sigma_z [m] (deprecated, use vector)
    
    // Mixing length model parameters for eddy diffusivity
    bool enable_mixing_length_turbulence = true; // Use mixing length model for eddy diffusivity
    amrex::Real mixing_length_coefficient = 0.1; // Proportionality constant for mixing length: l = coeff * Δz
    amrex::Real von_karman = 0.41;               // Von Kármán constant
    amrex::Real zground = 0.1;                   // Ground roughness for mixing length calculation [m]

    // Double-pass turbulent stress correction parameters (default OFF)
    bool enable_turbulent_stress = false;                         // Enable second mass-correction pass with turbulent stress
    std::string turbulent_stress_method = "mixing_length";        // Eddy-viscosity method: "mixing_length" (or future "smagorinsky")
    amrex::Real turbulent_stress_mixing_length_coefficient = 0.1; // Mixing-length coefficient for turbulent stress: l = coeff * κ * (z+z0)
    amrex::Real turbulent_schmidt_number_horizontal = 1.0;        // Schmidt number for horizontal turbulent stress (ν_t / Sc_h)
    amrex::Real turbulent_schmidt_number_vertical = 1.3;          // Schmidt number for vertical turbulent stress (ν_t / Sc_v)

    // Extraction
    std::vector<amrex::Real> extract_agl_list;
    std::vector<int>  extract_k_list;
    std::string extract_file = "wind_extract.csv";

    // Derivative method
    std::string deriv_method = "central";
    int deriv_method_int = 0;

    // Synthetic Turbulence
    bool enable_synthetic_turbulence = false;
    bool enable_terrain_aware_masking = false;
    amrex::Real terrain_mask_transition_height = 3.0;
    TurbulenceParams turb_params;
    std::string turbulence_export_format = "bts";
    std::string turbulence_output_file = "turbulence.bts";
    
    // Data center heat source structures (vector for multiple facilities)
    std::vector<DataCenterHeatSourceParams> datacenter_params;

    // --- COMPUTED & STATE VARIABLES ---
    std::vector<amrex::Real> x_terr, y_terr, z_terr;
    amrex::Real x_lo = 0.0, x_hi = 0.0, y_lo = 0.0, y_hi = 0.0;
    int nx = 0, ny = 0, nz = 0;
    amrex::Real dx = 0.0, dy = 0.0, dz = 0.0;

    // Buildings
    std::string building_file = "";
    std::vector<amrex::Real> building_xmin, building_xmax;
    std::vector<amrex::Real> building_ymin, building_ymax;
    std::vector<amrex::Real> building_zmin, building_zmax;
    std::vector<amrex::Real> building_rotation;
    std::vector<int> building_shape;
    std::vector<amrex::Real> building_pitch_or_radius;
    std::vector<amrex::Real> building_pitch_direction;
    
    // Polygon building support (for arbitrary footprints, courtyards, etc.)
    std::vector<int> building_geom_type;     // BuildingGeometryType for each building (0=rectangular, 1=polygon, 2=void)
    std::vector<std::vector<amrex::Real>> building_polygon_x;  // Polygon vertices if geom_type == POLYGON
    std::vector<std::vector<amrex::Real>> building_polygon_y;  // Polygon vertices if geom_type == POLYGON

    std::vector<amrex::Real> porous_building_xmin, porous_building_xmax;
    std::vector<amrex::Real> porous_building_ymin, porous_building_ymax;
    std::vector<amrex::Real> porous_building_zmin, porous_building_zmax;
    std::vector<amrex::Real> porous_building_porosity;
    std::vector<amrex::Real> porous_building_rotation;

    std::vector<amrex::Real> windbreak_x1, windbreak_y1;
    std::vector<amrex::Real> windbreak_x2, windbreak_y2;
    std::vector<amrex::Real> windbreak_height, windbreak_blockage, windbreak_drag_coeff;

    // Time-varying timeseries data
    std::vector<amrex::Real> time_series_times;
    std::vector<amrex::Real> time_series_U_refs;
    std::vector<amrex::Real> time_series_V_refs;
    int num_time_steps = 1;

    // Spatially-varying alpha data
    std::vector<amrex::Real> x_alpha, y_alpha, alpha_h_data, alpha_v_data;

    // Divergence source data
    std::vector<amrex::Real> x_div_src, y_alpha_div_src, z_alpha_div_src, div_src_data;

    // Temperature profile data
    std::vector<amrex::Real> z_temp, T_temp;

    // Coastal data for thermal circulation
    std::vector<amrex::Real> x_mask, y_mask, mask_val;

    // Host terrain heights
    std::vector<amrex::Real> terrain_h;
    amrex::Real zs_min = 0.0, zs_max = 0.0;

    // Host building heights/obstacle heights
    std::vector<amrex::Real> obstacle_h;
    amrex::Real obs_max = 0.0;

    // AMReX Objects
    bool enable_eb = false;
    amrex::Real eb_threshold = 0.5;
    std::string eb_geom_type_name;
    amrex::Vector<amrex::Real> eb_box_lo;
    amrex::Vector<amrex::Real> eb_box_hi;
    bool eb_box_has_fluid_inside = false;
    std::unique_ptr<amrex::EBFArrayBoxFactory> eb_factory;

    std::unique_ptr<amrex::Geometry> geom_ptr;
    std::unique_ptr<amrex::BoxArray> ba_ptr;
    std::unique_ptr<amrex::DistributionMapping> dm_ptr;

    // Field MultiFabs (Layer 1 & 2)
    std::unique_ptr<amrex::MultiFab> vel0_ptr;
    std::unique_ptr<amrex::MultiFab> vel_c_ptr;
    std::unique_ptr<amrex::MultiFab> lam_ptr;
    std::unique_ptr<amrex::MultiFab> rhs_ptr;
    std::unique_ptr<amrex::MultiFab> alpha_h_field_ptr;
    std::unique_ptr<amrex::MultiFab> alpha_v_field_ptr;

    std::unique_ptr<amrex::MultiFab> lambda_damped_ptr;
    std::unique_ptr<amrex::MultiFab> p_prime_ptr;
    std::unique_ptr<amrex::MultiFab> terrain_type_ptr;
    std::unique_ptr<amrex::MultiFab> terrain_slope_ptr;
    std::unique_ptr<amrex::MultiFab> terrain_curvature_ptr;
    std::unique_ptr<amrex::MultiFab> terrain_aspect_ptr;
    std::unique_ptr<amrex::MultiFab> adaptive_roughness_ptr;
    std::unique_ptr<amrex::MultiFab> adaptive_stability_ptr;
    std::unique_ptr<amrex::MultiFab> temp_ptr;
    
    // 3D Scalar transport fields
    std::unique_ptr<amrex::MultiFab> temp_3d_ptr;        // 3D temperature field [K]
    std::unique_ptr<amrex::MultiFab> moisture_3d_ptr;    // 3D moisture field [kg/kg]
    std::unique_ptr<amrex::MultiFab> temp_3d_old_ptr;    // Temperature from previous time step
    std::unique_ptr<amrex::MultiFab> moisture_3d_old_ptr; // Moisture from previous time step

    // Diagnostics & Output fields
    std::unique_ptr<amrex::MultiFab> synthetic_turbulence_fluc_ptr;
    bool has_synthetic_turbulence = false;
    std::unique_ptr<amrex::MultiFab> u_star_ptr;
    std::unique_ptr<amrex::MultiFab> tau_flux_ptr;
    std::unique_ptr<amrex::MultiFab> cd_ptr;
    std::unique_ptr<amrex::MultiFab> shf_ptr;
    std::unique_ptr<amrex::MultiFab> lhf_ptr;
    std::unique_ptr<amrex::MultiFab> z_bl_diag_ptr;
    std::unique_ptr<amrex::MultiFab> p_pert_ptr;

    // GPU-compatible Device arrays
    amrex::Gpu::DeviceVector<amrex::Real> d_terrain_h;
    amrex::Gpu::DeviceVector<amrex::Real> d_obstacle_h;
    amrex::Gpu::DeviceVector<amrex::Real> d_z0_pos;
    amrex::Gpu::DeviceVector<amrex::Real> d_landuse_pos;
    amrex::Gpu::DeviceVector<amrex::Real> d_canopy_height;
    amrex::Gpu::DeviceVector<amrex::Real> d_frontal_area_index;
    
    // Morphometric models
    std::vector<amrex::Real> morphometric_d;
    std::vector<amrex::Real> morphometric_z0;
    amrex::Gpu::DeviceVector<amrex::Real> d_morphometric_d;
    amrex::Gpu::DeviceVector<amrex::Real> d_morphometric_z0;
    
    // RAWS
    amrex::Gpu::DeviceVector<amrex::Real> d_raws_x, d_raws_y, d_raws_z, d_raws_u, d_raws_v;
    
    // Surface Data
    amrex::Gpu::DeviceVector<amrex::Real> d_surf_x, d_raws_surf_y, d_surf_z, d_surf_ustar, d_surf_z0, d_surf_u10, d_surf_v10;
};

#endif // WIND_SOLVER_APP_H
