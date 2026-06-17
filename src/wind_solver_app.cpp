#include "wind_solver_app.H"
#include "canopy_models.H"
#include "morphometric_models.H"
#include "wake_models.H"
#include "solver_math_constants.H"
#include "stability_models.H"
#include "porosity_models.H"
#include "wall_functions.H"
#include "buoyancy_models.H"
#include "orographic_models.H"
#include "cell_local_anisotropy.H"
#include "thermal_circulation_models.H"
#include "terrain_blocking_models.H"
#include "slope_flow_models.H"
#include "valley_channeling_models.H"
#include "gap_flow_models.H"
#include "richardson_number_models.H"
#include "diurnal_roughness_models.H"
#include "boundary_layer_decay_models.H"
#include "ageostrophic_models.H"
#include "flux_diagnostics.H"
#include "landuse_roughness.H"
#include "directional_bias_correction.H"
#include "simplified_richardson_method.H"
#include "roughness_blocking_method.H"
#include "coriolis_latitude_scaling.H"
#include "sky_view_factor.H"
#include "synthetic_turbulence.H"
#include "random_field_synthesis.H"
#include "temporal_synthesis.H"
#include "turbsim_bts_export.H"
#include "turbulence_validation.H"

#include "wind_io_helpers.H"
#include "wind_interpolation.H"
#include "numerical_derivatives.H"
#include "roughness_transitions.H"

#include <AMReX_ParmParse.H>
#include <AMReX_Print.H>
#include <AMReX_MLABecLaplacian.H>
#include <AMReX_MLMG.H>
#include <AMReX_LO_BCTYPES.H>
#include <AMReX_PlotFileUtil.H>
#include <AMReX_EB2.H>
#include <AMReX_EB2_IF.H>
#include <AMReX_EBFabFactory.H>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iomanip>

using namespace amrex;

void WindSolverApp::initialize() {
    parse_inputs();
    setup_geometry_and_mesh();
    allocate_data_fields();
}

void WindSolverApp::execute() {
    t_total = amrex::second();
    bool is_wind_steady = !enable_time_varying;

    if (enable_3d_scalars && scalar_coupling_mode == "segregated") {
        amrex::Print() << "wind_solver: [Segregated Mode] Running one pseudo-time step for wind solver\n";
        initialize_wind_fields(0);
        execute_poisson_solve(0);
        apply_divergence_corrections(0);
        
        for (int time_step = 0; time_step < num_time_steps; ++time_step) {
            amrex::Print() << "wind_solver: [Segregated Mode] step " << time_step << " of " << num_time_steps << "\n";
            if (enable_temperature_transport || enable_moisture_transport) {
                amrex::Real dt_transport = compute_adaptive_dt_transport();
                solve_transport_equations(time_step, dt_transport);
            }
            compute_diagnostics_and_output(time_step);
        }
    } else {
        // "coupled" mode, or non-scalar mode
        amrex::Print() << "wind_solver: Running coupled/unsteady simulation over " << num_time_steps << " steps\n";
        for (int time_step = 0; time_step < num_time_steps; ++time_step) {
            bool run_wind = true;
            if (is_wind_steady && scalar_coupling_mode == "coupled" && time_step > 0) {
                run_wind = false; // Run with frozen wind after initial correction
            }
            
            if (run_wind) {
                initialize_wind_fields(time_step);
                execute_poisson_solve(time_step);
                apply_divergence_corrections(time_step);
            } else {
                amrex::Print() << "wind_solver: [Coupled Mode] step " << time_step << " - using frozen wind field\n";
            }
            
            if (enable_3d_scalars && (enable_temperature_transport || enable_moisture_transport)) {
                amrex::Real dt_transport = compute_adaptive_dt_transport();
                solve_transport_equations(time_step, dt_transport);
            }
            
            compute_diagnostics_and_output(time_step);
        }
    }
    amrex::Print() << "wind_solver: ========================================\n";
    amrex::Print() << "wind_solver: total execution time = " 
                   << (amrex::second() - t_total) << " s\n";
    amrex::Print() << "wind_solver: ========================================\n";
    amrex::Print() << "wind_solver: done.\n";
}

void WindSolverApp::parse_inputs() {
    t_phase = amrex::second();
    ParmParse pp;

    pp.query("terrain_file", terrain_file);
    pp.query("init_mode", init_mode);

    if (init_mode != "loglaw" && init_mode != "uniform" && init_mode != "raws" && 
        init_mode != "surface_data" && init_mode != "powerlaw" && init_mode != "windfield" &&
        init_mode != "deaves_harris" && init_mode != "powerlaw_above_bl" && init_mode != "ekman_spiral" && init_mode != "sounding") {
        amrex::Abort("wind_solver: invalid init_mode: " + init_mode + 
                     " (must be 'loglaw', 'uniform', 'raws', 'surface_data', 'powerlaw', 'windfield', 'deaves_harris', 'powerlaw_above_bl', 'ekman_spiral', or 'sounding')");
    }

    pp.query("U_ref", U_ref);
    pp.query("V_ref", V_ref);
    pp.query("z_ref", z_ref);
    pp.query("z0",    z0);

    // Canopy model parameters
    pp.query("enable_canopy", enable_canopy);
    pp.query("canopy_file", canopy_file);
    pp.query("canopy_height", canopy_height);
    pp.query("frontal_area_index", frontal_area_index);
    pp.query("plan_area_index", plan_area_index);
    pp.query("canopy_drag_coeff", canopy_drag_coeff);
    pp.query("canopy_attenuation", canopy_attenuation);
    pp.query("use_exponential_profile", use_exponential_profile);
    pp.query("canopy_profile_type", canopy_profile_type);

    // Morphometric model parameters
    pp.query("enable_morphometric_models", enable_morphometric_models);
    pp.query("morphometric_model_type", morphometric_model_type);
    pp.query("morphometric_drag_coeff", morphometric_drag_coeff);
    if (morphometric_drag_coeff < 0.0) {
        if (morphometric_model_type == "bottema") {
            morphometric_drag_coeff = 0.8;
        } else {
            // Default for Macdonald or other models
            morphometric_drag_coeff = 1.2;
        }
    }

    // Sub-grid Windbreak and Linear Barrier Drag
    pp.query("enable_windbreaks", enable_windbreaks);
    pp.query("windbreaks_file", windbreaks_file);

    // Wake model parameters
    pp.query("enable_wake", enable_wake);
    pp.query("wake_model_type", wake_model_type);
    pp.query("wake_c1", wake_c1);
    pp.query("wake_c2", wake_c2);
    pp.query("wake_separation_length", wake_separation_length);
    pp.query("wake_superposition", wake_superposition);
    
    // Wake model enhancement parameters
    pp.query("enable_oblique_scaling", enable_oblique_scaling);
    pp.query("enable_tall_building_correction", enable_tall_building_correction);
    pp.query("enable_gaussian_profile", enable_gaussian_profile);
    pp.query("enable_upwind_recirculation", enable_upwind_recirculation);
    pp.query("enable_reference_correction", enable_reference_correction);
    pp.query("enable_corner_acceleration", enable_corner_acceleration);
    pp.query("enable_variance_correction", enable_variance_correction);
    pp.query("enable_horseshoe_vortex", enable_horseshoe_vortex);
    pp.query("enable_extended_farwake", enable_extended_farwake);
    pp.query("enable_yoshie_two_layer", enable_yoshie_two_layer);
    
    amrex::Real yoshie_decay_beta = amrex::Real(1.75);
    pp.query("yoshie_decay_beta", yoshie_decay_beta);
    
    pp.query("enable_rodi_entrainment", enable_rodi_entrainment);
    
    amrex::Real rodi_ce_coefficient = amrex::Real(1.0);
    pp.query("rodi_ce_coefficient", rodi_ce_coefficient);
    
    pp.query("enable_lopes_comfort", enable_lopes_comfort);
    
    amrex::Real lopes_comfort_threshold = amrex::Real(5.0);
    pp.query("lopes_comfort_threshold", lopes_comfort_threshold);
    
    amrex::Real lopes_assessment_height = amrex::Real(1.5);
    pp.query("lopes_assessment_height", lopes_assessment_height);
    
    amrex::Real lopes_reference_frequency = amrex::Real(0.02);
    pp.query("lopes_reference_frequency", lopes_reference_frequency);
    
    pp.query("enable_oikonomou_aspect", enable_oikonomou_aspect);
    
    amrex::Real oikonomou_beta_aspect = amrex::Real(0.25);
    pp.query("oikonomou_beta_aspect", oikonomou_beta_aspect);
    
    pp.query("enable_britter_hanna_urban", enable_britter_hanna_urban);
    
    amrex::Real britter_hanna_alpha = amrex::Real(0.15);
    pp.query("britter_hanna_alpha", britter_hanna_alpha);

    // Analytical Turbine Wake parameters
    pp.query("enable_turbine_wake", enable_turbine_wake);
    pp.query("turbine_file", turbine_file);
    pp.query("turbine_wake_model_type", turbine_wake_model_type);
    pp.query("turbine_wake_superposition", turbine_wake_superposition);
    pp.query("jensen_kw", jensen_kw);
    pp.query("gaussian_ka", gaussian_ka);
    pp.query("turbopark_c1", turbopark_c1);
    pp.query("ambient_ti", ambient_ti);
    pp.query("enable_jimenez_deflection", enable_jimenez_deflection);
    pp.query("enable_bastankhah_deflection", enable_bastankhah_deflection);
    pp.query("jimenez_kd", jimenez_kd);
    pp.query("wake_added_turbulence_model", wake_added_turbulence_model);
    pp.query("enable_wake_ground_interaction", enable_wake_ground_interaction);
    pp.query("wake_ground_damping_scale", wake_ground_damping_scale);
    pp.query("surface_sensible_heat_flux", surface_sensible_heat_flux);
    pp.query("buoyant_wake_destruction_coeff", buoyant_wake_destruction_coeff);
    
    if (enable_turbine_wake && !turbine_file.empty()) {
        TurbineWake::read_turbines_file(turbine_file, turbines);
    }

    // Electrical Wire Loading parameters
    pp.query("enable_wire_loading", enable_wire_loading);
    pp.query("wire_file", wire_file);
    pp.query("wire_output_file", wire_output_file);

    if (enable_wire_loading && !wire_file.empty()) {
        WireLoading::read_wires_file(wire_file, wires);
    }

    // Bridge Loading Assessment parameters
    pp.query("enable_bridge_loading", enable_bridge_loading);
    pp.query("bridge_file", bridge_file);
    pp.query("bridge_output_file", bridge_output_file);

    if (enable_bridge_loading && !bridge_file.empty()) {
        BridgeLoading::read_bridges_file(bridge_file, bridges);
    }

    // General Structure Loading Assessment parameters (buildings, towers, antennas)
    pp.query("enable_structure_loading", enable_structure_loading);
    pp.query("structure_file", structure_file);
    pp.query("structure_output_file", structure_output_file);

    if (enable_structure_loading && !structure_file.empty()) {
        StructureLoading::read_structures_file(structure_file, structures);
    }
    
    // Street canyon parameters
    pp.query("enable_street_canyon", enable_street_canyon);
    pp.query("street_canyon_reduction", street_canyon_reduction);

    // Embedded Boundary parameters
    pp.query("enable_eb", enable_eb);
    pp.query("eb_threshold", eb_threshold);
    
    // EB2 geometry parameters
    if (enable_eb) {
        std::string eb_geom_type;
        pp.query("eb2.geom_type", eb_geom_type);
        
        if (eb_geom_type == "box") {
            // Parse box parameters
            int n_lo = pp.countval("eb2.box_lo");
            int n_hi = pp.countval("eb2.box_hi");
            
            if (n_lo >= 3 && n_hi >= 3) {
                eb_box_lo.resize(3);
                eb_box_hi.resize(3);
                pp.getarr("eb2.box_lo", eb_box_lo, 0, 3);
                pp.getarr("eb2.box_hi", eb_box_hi, 0, 3);
                pp.query("eb2.box_has_fluid_inside", eb_box_has_fluid_inside);
                
                // Store for later use in allocate_data_fields()
                eb_geom_type_name = "box";
            } else {
                amrex::Print() << "warning: eb2.box_lo and eb2.box_hi must have 3 components each\n";
                enable_eb = false;
            }
        }
    }

    // Uniform mode parameters
    uniform_U = U_ref;  // default to U_ref
    uniform_V = V_ref;  // default to V_ref
    pp.query("uniform_U", uniform_U);
    pp.query("uniform_V", uniform_V);

    // Power-law mode parameters
    pp.query("powerlaw_exponent", powerlaw_exponent);
    
    pp.query("landuse_file", landuse_file);
    if (!landuse_file.empty()) {
        use_landuse_powerlaw = true;
    }

    // RAWS mode parameters
    pp.query("velocity_file", velocity_file);

    // Sounding profiles parameters
    {
        int n_sfiles = pp.countval("sounding_files");
        if (n_sfiles > 0) {
            sounding_files.resize(n_sfiles);
            pp.getarr("sounding_files", sounding_files, 0, n_sfiles);
        }
    }
    {
        int n_sx = pp.countval("sounding_x");
        if (n_sx > 0) {
            sounding_x.resize(n_sx);
            pp.getarr("sounding_x", sounding_x, 0, n_sx);
        }
    }
    {
        int n_sy = pp.countval("sounding_y");
        if (n_sy > 0) {
            sounding_y.resize(n_sy);
            pp.getarr("sounding_y", sounding_y, 0, n_sy);
        }
    }
    std::string s_file = "";
    pp.query("sounding_file", s_file);
    if (!s_file.empty()) {
        sounding_files.push_back(s_file);
    }
    pp.query("sounding_vertical_interp", sounding_vertical_interp);
    pp.query("sounding_wind_in_knots", sounding_wind_in_knots);

    // Surface data mode parameters (for HRRR-style initialization)
    pp.query("surface_data_file", surface_data_file);

    // Windfield mode parameters
    pp.query("windfield_file", windfield_file);

    // Position-dependent roughness file (for spatially-varying z0)
    pp.query("z0_file", z0_file);
    if (!z0_file.empty()) {
        use_z0_file = true;
    }
    
    // Vegetation Attenuation Factor for Roughness
    pp.query("enable_vegetation_roughness", enable_vegetation_roughness);
    pp.query("vegetation_state", vegetation_state);
    pp.query("vegetation_state_type", vegetation_state_type);

    pp.query("dx", dx_req);
    pp.query("dy", dy_req);
    pp.query("dz", dz_req);

    pp.query("domain_height", domain_height);

    pp.query("alpha_h", alpha_h);
    pp.query("alpha_v", alpha_v);
    pp.query("idw_gamma", idw_gamma);
    pp.query("idw_exponent", idw_exponent);
    pp.query("idw_rmax1", idw_rmax1);
    pp.query("idw_rmax2", idw_rmax2);
    pp.query("idw_r1", idw_r1);
    pp.query("idw_r2", idw_r2);

    // Analytical Ekman Spiral vertical profile initialization parameters
    ekman_latitude = latitude;
    pp.query("ekman_latitude", ekman_latitude);
    ekman_ug = U_ref;
    pp.query("ekman_ug", ekman_ug);
    ekman_vg = V_ref;
    pp.query("ekman_vg", ekman_vg);
    ekman_Km = Real(5.0);
    pp.query("ekman_Km", ekman_Km);

    // Height-dependent alpha_v
    pp.query("use_height_dependent_alpha_v", use_height_dependent_alpha_v);
    pp.query("alpha_v_surface", alpha_v_surface);
    pp.query("alpha_v_top", alpha_v_top);

    // Cell-local spatially-varying anisotropy
    pp.query("enable_cell_local_anisotropy", enable_cell_local_anisotropy);
    pp.query("anisotropy_source", anisotropy_source);
    pp.query("anisotropy_slope_scale", anisotropy_slope_scale);
    pp.query("anisotropy_decay_height", anisotropy_decay_height);
    pp.query("anisotropy_ri_gamma", anisotropy_ri_gamma);
    pp.query("anisotropy_ri_beta", anisotropy_ri_beta);
    pp.query("anisotropy_fr_min", anisotropy_fr_min);

    // Non-Neutral Log-Law
    pp.query("enable_stability_correction", enable_stability_correction);
    pp.query("stability_length", stability_length);
    
    // Alternative Stability Functions
    pp.query("use_holtslag_stability", use_holtslag_stability);

    // Pasquill-Gifford Stability
    pp.query("enable_pg_stability", enable_pg_stability);
    pp.query("solar_radiation", solar_radiation);
    pp.query("is_nighttime", is_nighttime);
    pp.query("cloud_cover", cloud_cover);
    if (enable_pg_stability) {
        Real speed_ref = std::sqrt(U_ref * U_ref + V_ref * V_ref);
        PGStabilityClass pg_class = pasquill_gifford_class(speed_ref, solar_radiation, is_nighttime, cloud_cover);
        stability_length = pg_class_to_obukhov_length(pg_class);
        enable_stability_correction = true;
    }

    // Atmospheric Inversion Capping Lid
    pp.query("enable_topographic_shielding", enable_topographic_shielding);
    pp.query("enable_capping_lid", enable_capping_lid);
    pp.query("capping_lid_height", capping_lid_height);
    parse_thermodynamic_lid_inputs(thermo_lid_params);
    if (thermo_lid_params.enabled) {
        enable_capping_lid = true;
    }

    // Elevation-Dependent Wind Speed Scaling
    pp.query("enable_elevation_scaling", enable_elevation_scaling);
    pp.query("elevation_scaling_factor", elevation_scaling_factor);
    pp.query("elevation_height_scale", elevation_height_scale);

    // Orographic Speed-up and Flow Separation
    pp.query("enable_orographic_speedup", enable_orographic_speedup);
    pp.query("orographic_hill_length_scale", orographic_hill_length_scale);
    pp.query("orographic_speedup_factor_max", orographic_speedup_factor_max);
    pp.query("orographic_separation_factor", orographic_separation_factor);
    pp.query("orographic_smoothing_factor", orographic_smoothing_factor);

    // Sea Breeze Parameterization (Thermal Circulation)
    pp.query("enable_thermal_circulation", enable_thermal_circulation);
    pp.query("thermal_temperature_contrast", thermal_temperature_contrast);
    pp.query("thermal_reference_temperature", thermal_reference_temperature);
    pp.query("thermal_coefficient", thermal_coefficient);
    pp.query("thermal_vertical_decay_height", thermal_vertical_decay_height);
    pp.query("thermal_distance_scale", thermal_distance_scale);
    pp.query("thermal_coastline_x", thermal_coastline_x);
    pp.query("thermal_coastline_y", thermal_coastline_y);
    pp.query("thermal_coast_normal_x", thermal_coast_normal_x);
    pp.query("thermal_coast_normal_y", thermal_coast_normal_y);
    pp.query("land_sea_mask_file", land_sea_mask_file);
    
    // Normalize coast normal vector
    Real coast_normal_mag = std::sqrt(thermal_coast_normal_x * thermal_coast_normal_x + 
                                     thermal_coast_normal_y * thermal_coast_normal_y);
    if (coast_normal_mag > Real(1.0e-10)) {
        thermal_coast_normal_x /= coast_normal_mag;
        thermal_coast_normal_y /= coast_normal_mag;
    }

    // Froude Number Terrain Blocking
    pp.query("enable_terrain_blocking", enable_terrain_blocking);
    pp.query("terrain_blocking_brunt_vaisala_frequency", terrain_blocking_brunt_vaisala_frequency);
    pp.query("terrain_blocking_reduction_factor", terrain_blocking_reduction_factor);
    pp.query("terrain_blocking_transition_froude", terrain_blocking_transition_froude);
    pp.query("terrain_blocking_flank_enhancement", terrain_blocking_flank_enhancement);
    pp.query("terrain_blocking_lapse_rate", terrain_blocking_lapse_rate);
    pp.query("terrain_blocking_reference_temperature", terrain_blocking_reference_temperature);
    
    if (enable_terrain_blocking && pp.contains("terrain_blocking_lapse_rate")) {
        terrain_blocking_brunt_vaisala_frequency = brunt_vaisala_frequency(
            terrain_blocking_reference_temperature, terrain_blocking_lapse_rate);
    }

    // Katabatic/Anabatic Slope Flows Parameterization
    pp.query("enable_slope_flows", enable_slope_flows);
    pp.query("slope_flow_temperature_diff", slope_flow_temperature_diff);
    pp.query("slope_flow_reference_temperature", slope_flow_reference_temperature);
    pp.query("slope_flow_empirical_coefficient", slope_flow_empirical_coefficient);
    pp.query("slope_flow_vertical_decay_height", slope_flow_vertical_decay_height);
    pp.query("slope_flow_min_slope", slope_flow_min_slope);

    // Valley channeling parameters
    pp.query("enable_valley_channeling", enable_valley_channeling);
    pp.query("valley_axis_angle_deg", valley_axis_angle_deg);
    pp.query("valley_width", valley_width);
    pp.query("valley_depth", valley_depth);
    pp.query("valley_channeling_strength_max", valley_channeling_strength_max);
    pp.query("valley_speedup_factor_narrow", valley_speedup_factor_narrow);
    pp.query("valley_slowdown_factor_wide", valley_slowdown_factor_wide);

    // Gap Flow Parameterization
    pp.query("enable_gap_flow", enable_gap_flow);
    pp.query("gap_flow_orientation", gap_flow_orientation);
    pp.query("gap_flow_width", gap_flow_width);
    pp.query("gap_flow_depth", gap_flow_depth);
    pp.query("gap_flow_pressure_coefficient", gap_flow_pressure_coefficient);
    pp.query("gap_flow_speedup_max", gap_flow_speedup_max);
    pp.query("gap_flow_center_x", gap_flow_center_x);
    pp.query("gap_flow_center_y", gap_flow_center_y);
    pp.query("gap_flow_transition_width", gap_flow_transition_width);
    pp.query("gap_flow_vertical_extent", gap_flow_vertical_extent);

    // Time-Varying Wind Boundary Conditions
    pp.query("enable_time_varying", enable_time_varying);
    pp.query("time_series_file", time_series_file);

    // Building Porosity Model
    pp.query("enable_building_porosity", enable_building_porosity);
    pp.query("building_porosity_file", building_porosity_file);
    pp.query("default_building_porosity", default_building_porosity);
    pp.query("porosity_drag_coefficient", porosity_drag_coefficient);

    // Wall Function Parameters
    pp.query("enable_wall_functions", enable_wall_functions);
    pp.query("enable_terrain_wall_function", enable_terrain_wall_function);
    pp.query("enable_flat_surface_wall_function", enable_flat_surface_wall_function);
    pp.query("enable_building_wall_function", enable_building_wall_function);
    pp.query("wall_function_z0_building", wall_function_z0_building);
    pp.query("wall_function_z0_flat", wall_function_z0_flat);
    pp.query("wall_function_blend_height", wall_function_blend_height);
    pp.query("wall_function_max_distance", wall_function_max_distance);
    pp.query("wall_function_flat_surface_elevation", wall_function_flat_surface_elevation);
    pp.query("wall_function_enable_flat_surface", wall_function_enable_flat_surface);
    pp.query("wall_function_min_wall_distance", wall_function_min_wall_distance);
    
    pp.query("wall_function_enable_stability", wall_function_enable_stability);
    pp.query("wall_function_stability_length", wall_function_stability_length);
    pp.query("wall_function_enable_adaptive", wall_function_enable_adaptive);
    pp.query("wall_function_adaptive_threshold", wall_function_adaptive_threshold);
    pp.query("wall_function_adaptive_min_cells", wall_function_adaptive_min_cells);
    
    if (enable_wall_functions) {
        if (!pp.contains("enable_terrain_wall_function")) {
            enable_terrain_wall_function = true;
        }
    }

    // Thermal Stratification with Buoyancy
    pp.query("enable_buoyancy_stratification", enable_buoyancy_stratification);
    pp.query("temperature_file", temperature_file);
    pp.query("temperature_reference", temperature_reference);
    pp.query("buoyancy_coefficient", buoyancy_coefficient);
    pp.query("buoyancy_timescale", buoyancy_timescale);
    pp.query("buoyancy_method", buoyancy_method);
    
    // Simple Diurnal Temperature Profile
    pp.query("enable_diurnal_temperature", enable_diurnal_temperature);
    pp.query("diurnal_temperature_amplitude", diurnal_temperature_amplitude);
    pp.query("diurnal_time_of_day", diurnal_time_of_day);
    pp.query("diurnal_phase_hour", diurnal_phase_hour);
    pp.query("diurnal_period", diurnal_period);

    // Kinematic Terrain-Following Boundary Condition
    pp.query("enable_terrain_kinematic_bc", enable_terrain_kinematic_bc);
    pp.query("terrain_bc_relaxation", terrain_bc_relaxation);
    
    // 3D Scalar Transport Parameters
    pp.query("enable_3d_scalars", enable_3d_scalars);
    pp.query("enable_temperature_transport", enable_temperature_transport);
    pp.query("enable_moisture_transport", enable_moisture_transport);
    pp.query("temperature_diffusivity", temperature_diffusivity);
    pp.query("moisture_diffusivity", moisture_diffusivity);
    pp.query("scalar_dt", scalar_dt);
    pp.query("scalar_cfl", scalar_cfl);
    pp.query("multi_step_corrector_steps", multi_step_corrector_steps);
    pp.query("scalar_coupling_mode", scalar_coupling_mode);
    if (scalar_coupling_mode != "segregated" && scalar_coupling_mode != "coupled") {
        amrex::Abort("wind_solver: invalid scalar_coupling_mode: " + scalar_coupling_mode + 
                     " (must be 'segregated' or 'coupled')");
    }
    
    // Mixing length turbulence model parameters
    pp.query("enable_mixing_length_turbulence", enable_mixing_length_turbulence);
    pp.query("mixing_length_coefficient", mixing_length_coefficient);
    pp.query("von_karman", von_karman);
    pp.query("zground", zground);
    
    // If any transport is enabled, automatically enable 3D scalars
    if (enable_temperature_transport || enable_moisture_transport) {
        enable_3d_scalars = true;
    }

    // Ekman Spiral Wind Veer Correction
    pp.query("enable_ekman_veer", enable_ekman_veer);
    pp.query("latitude", latitude);
    pp.query("ekman_veer_total", ekman_veer_total);
    pp.query("ekman_veer_height", ekman_veer_height);
    ekman_veer_total_rad = ekman_veer_total * MathConstants::pi / Real(180.0);

    // Wind Direction Gradient
    pp.query("enable_wind_direction_gradient", enable_wind_direction_gradient);
    pp.query("wind_direction_shear_rate", wind_direction_shear_rate);
    wind_direction_shear_rate_rad = wind_direction_shear_rate * MathConstants::pi / Real(180.0) / Real(100.0);

    // Spatially-varying Lagrange coefficients
    pp.query("use_spatial_alpha_coefficients", use_spatial_alpha_coefficients);
    pp.query("alpha_coefficients_file", alpha_coefficients_file);
    if (!alpha_coefficients_file.empty()) {
        use_spatial_alpha_coefficients = true;
    }
    if (enable_cell_local_anisotropy) {
        use_spatial_alpha_coefficients = true;
    }

    // Fetch-dependent roughness transition
    pp.query("enable_fetch_roughness_transition", enable_fetch_roughness_transition);
    pp.query("fetch_transition_blending_height", fetch_transition_blending_height);
    
    // Divergence Source Terms
    pp.query("enable_divergence_source", enable_divergence_source);
    pp.query("divergence_source_file", divergence_source_file);
    pp.query("divergence_source_constant", divergence_source_constant);
    if (!divergence_source_file.empty()) {
        enable_divergence_source = true;
    }

    // Diurnal z₀ Variations
    pp.query("enable_diurnal_roughness", enable_diurnal_roughness);
    pp.query("roughness_amplitude", roughness_amplitude);
    pp.query("roughness_phase_offset", roughness_phase_offset);

    // Sky View Factor and Solar Shading parameters
    pp.query("enable_sky_view_factor", enable_sky_view_factor);
    pp.query("enable_solar_shading", enable_solar_shading);
    pp.query("latitude_degrees", latitude_degrees);
    pp.query("longitude_degrees", longitude_degrees);
    pp.query("day_of_year", day_of_year);
    pp.query("hour_of_day", hour_of_day);
    pp.query("max_horizon_distance", max_horizon_distance);
    
    // Exponential Wind Decay Above BL
    pp.query("enable_bl_decay", enable_bl_decay);
    pp.query("bl_depth_param", bl_depth_param);
    pp.query("decay_height_scale", decay_height_scale);
    pp.query("bl_transition_height", bl_transition_height);
    
    // Boundary Layer Depth Diagnostic
    pp.query("enable_bl_depth_diagnostic", enable_bl_depth_diagnostic);
    pp.query("richardson_critical", richardson_critical);
    pp.query("richardson_min_wind_shear", richardson_min_wind_shear);
    
    // Froude Number Height Scaling
    pp.query("enable_froude_height_scaling", enable_froude_height_scaling);
    
    // Ageostrophic Wind Balance
    pp.query("enable_ageostrophic_balance", enable_ageostrophic_balance);
    pp.query("ageostrophic_latitude", ageostrophic_latitude);
    pp.query("ageostrophic_pressure_grad_x", ageostrophic_pressure_grad_x);
    pp.query("ageostrophic_pressure_grad_y", ageostrophic_pressure_grad_y);
    pp.query("ageostrophic_air_density", ageostrophic_air_density);
    pp.query("ageostrophic_fraction", ageostrophic_fraction);
    
    // Time-Series Thermal Circulation Forcing
    pp.query("enable_time_varying_thermal_amplitude", enable_time_varying_thermal_amplitude);
    pp.query("thermal_amplitude_file", thermal_amplitude_file);
    thermal_amplitude_time_of_day = diurnal_time_of_day;
    pp.query("thermal_amplitude_time_of_day", thermal_amplitude_time_of_day);

    pp.query("mlmg_verbose",  mlmg_verbose);
    pp.query("tol_rel",       tol_rel);
    pp.query("mlmg_max_iter", mlmg_max_iter);
    pp.query("mlmg_max_fmg_iter", mlmg_max_fmg_iter);
    pp.query("mlmg_pre_smooth", mlmg_pre_smooth);
    pp.query("mlmg_post_smooth", mlmg_post_smooth);
    pp.query("mlmg_bottom_solver", mlmg_bottom_solver);
    pp.query("max_grid_size", max_grid_size);
    pp.query("plot_file",     plot_file);
    
    // Flux Diagnostics
    pp.query("enable_flux_diagnostics", enable_flux_diagnostics);
    pp.query("flux_compute_sensible_heat", flux_compute_sensible_heat);
    pp.query("flux_compute_latent_heat", flux_compute_latent_heat);
    pp.query("flux_theta_star", flux_theta_star);
    pp.query("flux_q_star", flux_q_star);
    pp.query("surface_temperature", surface_temperature);
    pp.query("heat_flux_scale", heat_flux_scale);
    pp.query("relative_humidity", relative_humidity);
    
    // Land-use Roughness Classification
    pp.query("enable_landuse_roughness", enable_landuse_roughness);
    bool enable_landuse_classification = false;
    pp.query("enable_landuse_classification", enable_landuse_classification);
    if (enable_landuse_classification) {
        enable_landuse_roughness = true;
    }
    pp.query("landuse_file", landuse_file_class);
    pp.query("charnock_alpha", charnock_alpha);
    pp.query("enable_mosaic_roughness", enable_mosaic_roughness);

    // Marine Boundary Layer parameters
    pp.query("enable_marine_bl", enable_marine_bl);
    pp.query("marine_sst", marine_sst);
    pp.query("marine_air_sea_dt", marine_air_sea_dt);

    // Precipitation Parameters
    pp.query("precipitation_file", precipitation_file);
    pp.query("precipitation_stability_threshold", precipitation_stability_threshold);
    
    // Directional Bias Correction
    pp.query("enable_directional_bias", enable_directional_bias);
    pp.query("bias_direction_offset", bias_direction_offset);
    pp.query("bias_speed_factor", bias_speed_factor);
    pp.query("bias_periodic_enabled", bias_periodic_enabled);
    pp.query("bias_periodic_amplitude", bias_periodic_amplitude);

    // Simplified Richardson Number Method
    pp.query("enable_simplified_richardson", enable_simplified_richardson);
    pp.query("use_golder_curves", use_golder_curves);
    
    // Roughness Blocking from Buildings
    pp.query("enable_roughness_blocking", enable_roughness_blocking);
    pp.query("building_roughness_factor", building_roughness_factor);
    
    // Coriolis Latitude Scaling
    pp.query("enable_coriolis_latitude", enable_coriolis_latitude);
    pp.query("domain_latitude", domain_latitude);
    
    // Power-Law Wind Profile Above Boundary Layer
    pp.query("enable_power_law_profile", enable_power_law_profile);
    pp.query("power_law_exponent", power_law_exponent);
    
    // Heat Flux Diagnostic Enhancement
    pp.query("enable_heat_flux_diagnostics", enable_heat_flux_diagnostics);
    pp.query("heat_flux_theta_star", heat_flux_theta_star);

    // Divergence Damping Filter
    pp.query("enable_divergence_damping", enable_divergence_damping);
    pp.query("damping_coefficient", damping_coefficient);
    pp.query("damping_coefficient_h", damping_coefficient_h);
    pp.query("damping_coefficient_v", damping_coefficient_v);
    pp.query("damping_iterations", damping_iterations);
    
    // Perturbation Pressure Gradient
    pp.query("enable_perturbation_pressure", enable_perturbation_pressure);
    pp.query("pressure_tol_rel", pressure_tol_rel);
    pp.query("pressure_max_iter", pressure_max_iter);
    pp.query("pressure_scale", pressure_scale);
    
    // O'Brien Vertical Velocity Adjustment
    pp.query("enable_obrien_w_adjustment", enable_obrien_w_adjustment);
    
    // Multi-Scale Terrain Analysis
    pp.query("enable_terrain_analysis", enable_terrain_analysis);
    pp.query("slope_threshold_moderate", slope_threshold_moderate);
    pp.query("slope_threshold_steep", slope_threshold_steep);
    pp.query("roughness_factor_moderate", roughness_factor_moderate);
    pp.query("roughness_factor_steep", roughness_factor_steep);
    pp.query("transition_zone_width", transition_zone_width);
    
    // Surface-Layer-to-Mixed-Layer Transition Smoothing
    pp.query("enable_transition_smoothing", enable_transition_smoothing);
    pp.query("transition_height_scale", transition_height_scale);
    pp.query("bl_transition_height", transition_layer_height);

    // Terrain-aligned extraction parameters
    {
        int n_agl = pp.countval("extract_agl");
        if (n_agl > 0) {
            extract_agl_list.resize(n_agl);
            pp.getarr("extract_agl", extract_agl_list, 0, n_agl);
        }
    }
    {
        int n_k = pp.countval("extract_k");
        if (n_k > 0) {
            extract_k_list.resize(n_k);
            pp.getarr("extract_k", extract_k_list, 0, n_k);
        }
    }
    pp.query("extract_file", extract_file);

    pp.query("deriv_method", deriv_method);
    if (deriv_method != "central" && deriv_method != "weno3" && deriv_method != "weno5") {
        amrex::Abort("wind_solver: invalid deriv_method: " + deriv_method + 
                     " (must be 'central', 'weno3', or 'weno5')");
    }
    
    // Synthetic Turbulence Parameters
    ParmParse pp_turb("turbulence");
    if (!pp.query("enable_synthetic_turbulence", enable_synthetic_turbulence)) {
        pp_turb.query("enabled", enable_synthetic_turbulence);
    }
    turb_params.enabled = enable_synthetic_turbulence;
    
    if (enable_synthetic_turbulence) {
        pp.query("enable_terrain_aware_masking", enable_terrain_aware_masking);
        pp.query("terrain_mask_transition_height", terrain_mask_transition_height);

        std::string spectrum_model_str = "VonKarman";
        std::string intensity_model_str = "PowerLaw";
        std::string coherence_model_str = "Gaussian";
        if (!pp.query("turbulence_spectrum_model", spectrum_model_str)) {
            pp_turb.query("spectrum_model", spectrum_model_str);
        }
        if (!pp.query("turbulence_intensity_model", intensity_model_str)) {
            pp_turb.query("intensity_model", intensity_model_str);
        }
        if (!pp.query("turbulence_coherence_model", coherence_model_str)) {
            pp_turb.query("coherence_model", coherence_model_str);
        }
        
        if (spectrum_model_str == "VonKarman") {
            turb_params.spectrum_model = TurbulenceModel::VonKarman;
        } else if (spectrum_model_str == "Kaimal") {
            turb_params.spectrum_model = TurbulenceModel::Kaimal;
        } else if (spectrum_model_str == "MannBox") {
            turb_params.spectrum_model = TurbulenceModel::MannBox;
        } else {
            amrex::Abort("wind_solver: invalid turbulence_spectrum_model: " + spectrum_model_str + 
                         " (must be 'VonKarman', 'Kaimal', or 'MannBox')");
        }
        
        if (intensity_model_str == "PowerLaw") {
            turb_params.intensity_model = IntensityModel::PowerLaw;
        } else if (intensity_model_str == "Logarithmic") {
            turb_params.intensity_model = IntensityModel::Logarithmic;
        } else if (intensity_model_str == "Constant") {
            turb_params.intensity_model = IntensityModel::Constant;
        } else if (intensity_model_str == "IEC61400") {
            turb_params.intensity_model = IntensityModel::IEC61400;
        } else if (intensity_model_str == "SmoothProfile") {
            turb_params.intensity_model = IntensityModel::SmoothProfile;
        } else {
            amrex::Abort("wind_solver: invalid turbulence_intensity_model: " + intensity_model_str + 
                         " (must be 'PowerLaw', 'Logarithmic', 'Constant', 'IEC61400', or 'SmoothProfile')");
        }
        
        if (coherence_model_str == "Gaussian") {
            turb_params.coherence_model = CoherenceModel::Gaussian;
        } else if (coherence_model_str == "Exponential") {
            turb_params.coherence_model = CoherenceModel::Exponential;
        } else if (coherence_model_str == "QuadraticExponential") {
            turb_params.coherence_model = CoherenceModel::QuadraticExponential;
        } else if (coherence_model_str == "PowerLaw") {
            turb_params.coherence_model = CoherenceModel::PowerLaw;
        } else {
            amrex::Abort("wind_solver: invalid turbulence_coherence_model: " + coherence_model_str + 
                         " (must be 'Gaussian', 'Exponential', 'QuadraticExponential', or 'PowerLaw')");
        }
        
        if (!pp.query("turbulence_intensity_ref", turb_params.intensity_ref)) {
            pp_turb.query("intensity_ref", turb_params.intensity_ref);
        }
        if (!pp.query("turbulence_z_intensity_ref", turb_params.z_intensity_ref)) {
            pp_turb.query("z_intensity_ref", turb_params.z_intensity_ref);
        }
        if (!pp.query("turbulence_intensity_exponent", turb_params.intensity_exponent)) {
            pp_turb.query("intensity_exponent", turb_params.intensity_exponent);
        }
        if (!pp.query("turbulence_hub_height", turb_params.hub_height)) {
            pp_turb.query("hub_height", turb_params.hub_height);
        }
        if (!pp.query("turbulence_iec_category", turb_params.iec_turbulence_category)) {
            pp_turb.query("iec_category", turb_params.iec_turbulence_category);
        }
        if (!pp.query("turbulence_coherence_powerlaw_exponent", turb_params.coherence_powerlaw_exponent)) {
            pp_turb.query("coherence_powerlaw_exponent", turb_params.coherence_powerlaw_exponent);
        }
        if (!pp.query("turbulence_length_scale_u", turb_params.length_scale_u)) {
            pp_turb.query("length_scale_u", turb_params.length_scale_u);
        }
        if (!pp.query("turbulence_length_scale_v", turb_params.length_scale_v)) {
            pp_turb.query("length_scale_v", turb_params.length_scale_v);
        }
        if (!pp.query("turbulence_length_scale_w", turb_params.length_scale_w)) {
            pp_turb.query("length_scale_w", turb_params.length_scale_w);
        }
        if (!pp.query("turbulence_coherence_decay_vertical", turb_params.coherence_decay_vertical)) {
            pp_turb.query("coherence_decay_vertical", turb_params.coherence_decay_vertical);
        }
        if (!pp.query("turbulence_coherence_decay_lateral", turb_params.coherence_decay_lateral)) {
            pp_turb.query("coherence_decay_lateral", turb_params.coherence_decay_lateral);
        }
        if (!pp.query("turbulence_anisotropy_ratio_v", turb_params.anisotropy_ratio_v)) {
            pp_turb.query("anisotropy_ratio_v", turb_params.anisotropy_ratio_v);
        }
        if (!pp.query("turbulence_anisotropy_ratio_w", turb_params.anisotropy_ratio_w)) {
            pp_turb.query("anisotropy_ratio_w", turb_params.anisotropy_ratio_w);
        }
        
        if (turb_params.spectrum_model == TurbulenceModel::MannBox) {
            if (!pp.query("mann_length_scale_u", turb_params.mann_length_scale_u)) {
                pp_turb.query("mann_length_scale_u", turb_params.mann_length_scale_u);
            }
            if (!pp.query("mann_length_scale_v", turb_params.mann_length_scale_v)) {
                pp_turb.query("mann_length_scale_v", turb_params.mann_length_scale_v);
            }
            if (!pp.query("mann_length_scale_w", turb_params.mann_length_scale_w)) {
                pp_turb.query("mann_length_scale_w", turb_params.mann_length_scale_w);
            }
            if (!pp.query("mann_variance_u", turb_params.mann_variance_u)) {
                pp_turb.query("mann_variance_u", turb_params.mann_variance_u);
            }
            if (!pp.query("mann_variance_v", turb_params.mann_variance_v)) {
                pp_turb.query("mann_variance_v", turb_params.mann_variance_v);
            }
            if (!pp.query("mann_variance_w", turb_params.mann_variance_w)) {
                pp_turb.query("mann_variance_w", turb_params.mann_variance_w);
            }
            if (!pp.query("mann_asymmetry_parameter", turb_params.mann_asymmetry_parameter)) {
                pp_turb.query("mann_asymmetry_parameter", turb_params.mann_asymmetry_parameter);
            }
            if (!pp.query("mann_eddy_lifetime", turb_params.mann_eddy_lifetime)) {
                pp_turb.query("mann_eddy_lifetime", turb_params.mann_eddy_lifetime);
            }
            if (!pp.query("mann_terrain_adaptation_factor", turb_params.mann_terrain_adaptation_factor)) {
                pp_turb.query("mann_terrain_adaptation_factor", turb_params.mann_terrain_adaptation_factor);
            }
        }
        
        if (turb_params.intensity_model == IntensityModel::IEC61400) {
            if (!pp.query("hub_height", turb_params.hub_height)) {
                pp_turb.query("hub_height", turb_params.hub_height);
            }
            if (!pp.query("iec_turbulence_category", turb_params.iec_turbulence_category)) {
                pp_turb.query("iec_category", turb_params.iec_turbulence_category);
            }
        }
        
        if (!pp.query("coherence_powerlaw_exponent", turb_params.coherence_powerlaw_exponent)) {
            pp_turb.query("coherence_powerlaw_exponent", turb_params.coherence_powerlaw_exponent);
        }
        
        int random_seed_int = 12345;
        if (!pp.query("turbulence_random_seed", random_seed_int)) {
            pp_turb.query("random_seed", random_seed_int);
        }
        turb_params.random_seed = static_cast<unsigned int>(std::max(1, random_seed_int));
         
        if (!pp.query("turbulence_enable_stability_correction", turb_params.enable_stability_correction)) {
            pp_turb.query("enable_stability_correction", turb_params.enable_stability_correction);
        }
        if (!pp.query("turbulence_monin_obukhov_length", turb_params.monin_obukhov_length)) {
            pp_turb.query("monin_obukhov_length", turb_params.monin_obukhov_length);
        }
         
        std::string stability_param_str = "BusingerDyer";
        if (!pp.query("turbulence_stability_parameterization", stability_param_str)) {
            pp_turb.query("stability_parameterization", stability_param_str);
        }
        if (stability_param_str == "BusingerDyer") {
            turb_params.use_holtslag_stability = false;
        } else if (stability_param_str == "HoltslagDeBruin") {
            turb_params.use_holtslag_stability = true;
        } else {
            amrex::Abort("wind_solver: invalid turbulence_stability_parameterization: " + stability_param_str + 
                         " (must be 'BusingerDyer' or 'HoltslagDeBruin')");
        }
        
        pp.query("turbulence_export_format", turbulence_export_format);
        pp.query("turbulence_output_file", turbulence_output_file);
        
        if (turbulence_export_format != "bts") {
            amrex::Abort("wind_solver: invalid turbulence_export_format: " + turbulence_export_format + 
                         " (only 'bts' is currently supported)");
        }
        
        if (turb_params.intensity_ref < SyntheticTurbulence::Constants::intensity_min || 
            turb_params.intensity_ref > SyntheticTurbulence::Constants::intensity_max) {
            amrex::Print() << "WARNING: turbulence_intensity_ref = " << turb_params.intensity_ref 
                           << " is outside typical range [0.01, 0.30]\n";
        }
        if (turb_params.z_intensity_ref < 0.0) {
            amrex::Abort("wind_solver: turbulence_z_intensity_ref must be >= 0.0");
        }
        if (turb_params.intensity_exponent < 0.0 || turb_params.intensity_exponent > 0.5) {
            amrex::Print() << "WARNING: turbulence_intensity_exponent = " << turb_params.intensity_exponent 
                           << " is outside typical range [0.0, 0.5]\n";
        }
        if (turb_params.length_scale_u <= 0.0 || turb_params.length_scale_v <= 0.0 || turb_params.length_scale_w <= 0.0) {
            amrex::Abort("wind_solver: all length scales (u, v, w) must be > 0.0");
        }
        if (turb_params.anisotropy_ratio_v < 0.0 || turb_params.anisotropy_ratio_v > 1.0) {
            amrex::Print() << "WARNING: turbulence_anisotropy_ratio_v = " << turb_params.anisotropy_ratio_v 
                           << " is outside typical range [0.0, 1.0]\n";
        }
        if (turb_params.anisotropy_ratio_w < 0.0 || turb_params.anisotropy_ratio_w > 1.0) {
            amrex::Print() << "WARNING: turbulence_anisotropy_ratio_w = " << turb_params.anisotropy_ratio_w 
                           << " is outside typical range [0.0, 1.0]\n";
        }
        if (turb_params.spectrum_model == TurbulenceModel::MannBox) {
            if (turb_params.mann_length_scale_u <= 0.0 || turb_params.mann_length_scale_v <= 0.0 || 
                turb_params.mann_length_scale_w <= 0.0) {
                amrex::Abort("wind_solver: all Mann Box length scales (u, v, w) must be > 0.0");
            }
            if (turb_params.mann_variance_u <= 0.0 || turb_params.mann_variance_v <= 0.0 || 
                turb_params.mann_variance_w <= 0.0) {
                amrex::Abort("wind_solver: all Mann Box variance scales (u, v, w) must be > 0.0");
            }
            if (turb_params.mann_asymmetry_parameter <= 0.0) {
                amrex::Abort("wind_solver: mann_asymmetry_parameter must be > 0.0");
            }
            if (turb_params.mann_eddy_lifetime <= 0.0) {
                amrex::Abort("wind_solver: mann_eddy_lifetime must be > 0.0");
            }
            if (turb_params.mann_terrain_adaptation_factor <= 0.0) {
                amrex::Abort("wind_solver: mann_terrain_adaptation_factor must be > 0.0");
            }
        }
        if (turb_params.intensity_model == IntensityModel::IEC61400) {
            if (turb_params.hub_height <= 0.0) {
                amrex::Abort("wind_solver: hub_height must be > 0.0 for IEC61400 model");
            }
            if (turb_params.iec_turbulence_category < 0 || turb_params.iec_turbulence_category > 2) {
                amrex::Abort("wind_solver: iec_turbulence_category must be 0 (A), 1 (B), or 2 (C)");
            }
        }
        if (turb_params.coherence_model == CoherenceModel::PowerLaw) {
            if (turb_params.coherence_powerlaw_exponent <= 0.0) {
                amrex::Print() << "WARNING: coherence_powerlaw_exponent = " << turb_params.coherence_powerlaw_exponent 
                               << " is outside typical range (0, inf)\n";
            }
        }
        
        amrex::Print() << "wind_solver: Synthetic turbulence ENABLED\n"
                       << "  spectrum_model: " << spectrum_model_str << "\n"
                       << "  intensity_model: " << intensity_model_str << "\n"
                       << "  coherence_model: " << coherence_model_str << "\n"
                       << "  intensity_ref: " << turb_params.intensity_ref << "\n"
                       << "  length_scales: u=" << turb_params.length_scale_u 
                       << ", v=" << turb_params.length_scale_v 
                       << ", w=" << turb_params.length_scale_w << " [m]\n"
                       << "  anisotropy_ratios: v/u=" << turb_params.anisotropy_ratio_v 
                       << ", w/u=" << turb_params.anisotropy_ratio_w << "\n";
        
        if (turb_params.spectrum_model == TurbulenceModel::MannBox) {
            amrex::Print() << "  [Mann Box Model Parameters]\n"
                           << "    mann_length_scales: u=" << turb_params.mann_length_scale_u 
                           << ", v=" << turb_params.mann_length_scale_v 
                           << ", w=" << turb_params.mann_length_scale_w << " [m]\n"
                           << "    mann_variance_scales: u=" << turb_params.mann_variance_u 
                           << ", v=" << turb_params.mann_variance_v 
                           << ", w=" << turb_params.mann_variance_w << "\n"
                           << "    mann_asymmetry_parameter: " << turb_params.mann_asymmetry_parameter << "\n"
                           << "    mann_eddy_lifetime: " << turb_params.mann_eddy_lifetime << " [s]\n"
                           << "    mann_terrain_adaptation_factor: " << turb_params.mann_terrain_adaptation_factor << "\n";
        }
        if (turb_params.intensity_model == IntensityModel::IEC61400) {
            amrex::Print() << "  [IEC 61400-1 Parameters]\n"
                           << "    hub_height: " << turb_params.hub_height << " [m]\n"
                           << "    turbulence_category: " << turb_params.iec_turbulence_category 
                           << " (0=A, 1=B, 2=C)\n";
        }
        if (turb_params.coherence_model == CoherenceModel::PowerLaw) {
            amrex::Print() << "  coherence_powerlaw_exponent: " << turb_params.coherence_powerlaw_exponent << "\n";
        }
        amrex::Print() << "  enable_terrain_aware_masking: " << enable_terrain_aware_masking << "\n";
        if (enable_terrain_aware_masking) {
            amrex::Print() << "  terrain_mask_transition_height: " << terrain_mask_transition_height << " [m]\n";
        }
        amrex::Print() << "  export_format: " << turbulence_export_format << "\n"
                       << "  output_file: " << turbulence_output_file << "\n";
    }

    amrex::Print() << "wind_solver: input parsing time = " 
                   << (amrex::second() - t_phase) << " s\n";
    
    // Print Solver Configuration Information
    amrex::Print() << "wind_solver: ========================================\n"
                   << "wind_solver: Solver Configuration\n"
                   << "wind_solver: ========================================\n";
    
    #ifdef AMREX_USE_MPI
    amrex::Print() << "wind_solver: Parallelization: MPI enabled\n"
                   << "wind_solver:   nprocs = " << amrex::ParallelDescriptor::NProcs() << "\n";
    #else
    amrex::Print() << "wind_solver: Parallelization: Serial (MPI disabled)\n";
    #endif
    
    #ifdef AMREX_USE_CUDA
    amrex::Print() << "wind_solver: GPU Backend: NVIDIA CUDA\n";
    #elif defined(AMREX_USE_HIP)
    amrex::Print() << "wind_solver: GPU Backend: AMD HIP/ROCm\n";
    #elif defined(AMREX_USE_SYCL)
    amrex::Print() << "wind_solver: GPU Backend: Intel SYCL/oneAPI\n";
    #else
    amrex::Print() << "wind_solver: GPU Backend: None (CPU-only)\n";
    #endif
    
    #ifdef AMREX_USE_FFT
    #ifdef AMREX_USE_CUDA
    amrex::Print() << "wind_solver: FFT Backend: cuFFT (NVIDIA CUDA)\n";
    #elif defined(AMREX_USE_HIP)
    amrex::Print() << "wind_solver: FFT Backend: rocFFT (AMD HIP/ROCm)\n";
    #elif defined(AMREX_USE_SYCL)
    amrex::Print() << "wind_solver: FFT Backend: oneMKL (Intel SYCL/oneAPI)\n";
    #else
    amrex::Print() << "wind_solver: FFT Backend: FFTPACK (CPU)\n";
    #endif
    #else
    amrex::Print() << "wind_solver: FFT Backend: Not available (AMREX_USE_FFT disabled)\n";
    #endif
    
    amrex::Print() << "wind_solver: AMReX version: " << amrex::Version() << "\n";
    amrex::Print() << "wind_solver: Derivative method: " << deriv_method << "\n";
    
    if (enable_synthetic_turbulence) {
        amrex::Print() << "wind_solver: Synthetic turbulence: enabled\n";
    } else {
        amrex::Print() << "wind_solver: Synthetic turbulence: disabled\n";
    }
    amrex::Print() << "wind_solver: ========================================\n\n";
    
    deriv_method_int = 0;
    if (deriv_method == "weno3") deriv_method_int = 1;
    else if (deriv_method == "weno5") deriv_method_int = 2;
    
    // Validate configuration for conflicts
    validate_configuration();
}

void WindSolverApp::validate_configuration() {
    /**
     * @brief Check for conflicting or incompatible capability combinations
     */
    
    bool has_warning = false;
    
    // --- CONFLICT 1: Building Wakes + Turbine Wakes ---
    // These use different physical models and methodologies
    if (enable_wake && enable_turbine_wake) {
        amrex::Print() << "wind_solver: *** WARNING ***\n";
        amrex::Print() << "wind_solver: Both building wakes (enable_wake=true) and turbine wakes\n";
        amrex::Print() << "wind_solver: (enable_turbine_wake=true) are enabled simultaneously.\n";
        amrex::Print() << "wind_solver: These models use incompatible methodologies and mixing them may\n";
        amrex::Print() << "wind_solver: produce unphysical results. It is recommended to use ONE of:\n";
        amrex::Print() << "wind_solver:   - Building wakes for urban wind modeling\n";
        amrex::Print() << "wind_solver:   - Turbine wakes for wind farm modeling\n";
        amrex::Print() << "wind_solver: *** PROCEEDING WITH CAUTION ***\n";
        has_warning = true;
    }
    
    // --- CONFLICT 2: Canopy + Buildings ---
    // Canopy is a porous media approach while buildings are solid obstacles
    if (enable_canopy && !building_file.empty() && building_file != "") {
        amrex::Print() << "wind_solver: *** INFO ***\n";
        amrex::Print() << "wind_solver: Both canopy (enable_canopy=true) and buildings are present.\n";
        amrex::Print() << "wind_solver: These can be combined, but ensure they do not spatially overlap.\n";
        amrex::Print() << "wind_solver: Canopy represents vegetation (porous drag), buildings are solid.\n";
    }
    
    // --- CONFLICT 3: Multiple Initialization Modes ---
    // Verify that only one primary initialization mode is selected
    int init_modes_count = 0;
    if (init_mode == "loglaw") init_modes_count++;
    if (init_mode == "uniform") init_modes_count++;
    if (init_mode == "raws" || init_mode == "surface_data") init_modes_count++;
    if (init_mode == "mann_box") init_modes_count++;
    if (init_mode == "windfield") init_modes_count++;
    
    if (init_modes_count > 1) {
        amrex::Print() << "wind_solver: *** WARNING ***\n";
        amrex::Print() << "wind_solver: Multiple initialization modes detected.\n";
        amrex::Print() << "wind_solver: Using: " << init_mode << "\n";
    }
    
    // --- CONFLICT 4: Buoyancy + Incompatible Features ---
    // Buoyancy requires temperature information
    if (enable_buoyancy_stratification && temperature_file.empty()) {
        amrex::Print() << "wind_solver: *** WARNING ***\n";
        amrex::Print() << "wind_solver: Buoyancy stratification enabled (enable_buoyancy_stratification=true)\n";
        amrex::Print() << "wind_solver: but no temperature file specified (temperature_file empty).\n";
        amrex::Print() << "wind_solver: Buoyancy effects will not be active. Specify temperature_file to enable.\n";
        has_warning = true;
    }
    
    // --- CONFLICT 5: Stability Correction Redundancy ---
    // Both Monin-Obukhov stability and diurnal temperature can be over-determined
    if (enable_stability_correction && enable_diurnal_temperature && enable_buoyancy_stratification) {
        amrex::Print() << "wind_solver: *** INFO ***\n";
        amrex::Print() << "wind_solver: Stability correction, diurnal temperature, and buoyancy all enabled.\n";
        amrex::Print() << "wind_solver: These features are compatible but can create complex feedback loops.\n";
        amrex::Print() << "wind_solver: Monitor results for physically reasonable behavior.\n";
    }
    
    // --- CONFLICT 6: Turbine Wake Models Compatibility ---
    // Bastankhah deflection requires Bastankhah wake model
    if (enable_bastankhah_deflection && turbine_wake_model_type != "bastankhah") {
        amrex::Print() << "wind_solver: *** WARNING ***\n";
        amrex::Print() << "wind_solver: Bastankhah yaw deflection enabled (enable_bastankhah_deflection=true)\n";
        amrex::Print() << "wind_solver: but turbine_wake_model_type = " << turbine_wake_model_type << "\n";
        amrex::Print() << "wind_solver: Bastankhah deflection only works with Bastankhah wake model.\n";
        amrex::Print() << "wind_solver: Set turbine_wake_model_type = bastankhah to use deflection.\n";
        has_warning = true;
    }
    
    // --- CONFLICT 7: Data Assimilation with Time-Varying Wind ---
    // Data assimilation can be complex with time-varying forcing
    if (enable_data_assimilation && enable_time_varying) {
        amrex::Print() << "wind_solver: *** INFO ***\n";
        amrex::Print() << "wind_solver: Data assimilation enabled with time-varying forcing.\n";
        amrex::Print() << "wind_solver: Assimilation windows should not exceed temporal variation scale.\n";
    }
    
    // --- CONFLICT 8: Street Canyon + Building Wakes ---
    // Street canyon detection can conflict with explicit building wake modeling
    if (enable_street_canyon && enable_wake) {
        amrex::Print() << "wind_solver: *** INFO ***\n";
        amrex::Print() << "wind_solver: Street canyon modeling enabled with building wakes.\n";
        amrex::Print() << "wind_solver: Street canyon parameterizations will apply within detected canyons.\n";
        amrex::Print() << "wind_solver: Ensure buildings are properly specified in building_file.\n";
    }
    
    // --- CAPABILITY: Wall Functions Limitations ---
    // Wall functions have specific domain requirements
    if ((enable_terrain_wall_function || enable_building_wall_function) && 
        (dz > 10.0 || dz < 1.0)) {
        amrex::Print() << "wind_solver: *** WARNING ***\n";
        amrex::Print() << "wind_solver: Wall functions enabled with dz = " << dz << " m.\n";
        amrex::Print() << "wind_solver: Wall functions typically require 1m < dz < 10m.\n";
        amrex::Print() << "wind_solver: Current grid spacing may not be suitable.\n";
        has_warning = true;
    }
    
    // --- CAPABILITY: Synthetic Turbulence Domain Requirements ---
    // Synthetic turbulence needs sufficient domain height
    if (enable_synthetic_turbulence && domain_height < 200.0) {
        amrex::Print() << "wind_solver: *** WARNING ***\n";
        amrex::Print() << "wind_solver: Synthetic turbulence enabled with small domain_height = " 
                       << domain_height << " m.\n";
        amrex::Print() << "wind_solver: Recommend domain_height > 200m for realistic turbulence.\n";
    }
    
    if (has_warning) {
        amrex::Print() << "wind_solver: ========================================\n";
        amrex::Print() << "wind_solver: Configuration validation complete. Check warnings above.\n";
        amrex::Print() << "wind_solver: ========================================\n\n";
    }
}

    t_phase = amrex::second();
    if (terrain_file == "synthetic") {
        ParmParse pp;
        Real synth_xmin = 0.0;
        Real synth_xmax = 300.0;
        Real synth_ymin = 0.0;
        Real synth_ymax = 300.0;
        int synth_nx = 11;
        int synth_ny = 11;
        std::string synth_type = "multi_gaussian_hill";

        pp.query("synthetic_xmin", synth_xmin);
        pp.query("synthetic_xmax", synth_xmax);
        pp.query("synthetic_ymin", synth_ymin);
        pp.query("synthetic_ymax", synth_ymax);
        pp.query("synthetic_nx", synth_nx);
        pp.query("synthetic_ny", synth_ny);
        pp.query("synthetic_type", synth_type);

        std::vector<Real> peaks;
        std::vector<Real> sigmas;
        std::vector<Real> centers_x;
        std::vector<Real> centers_y;

        if (synth_type == "gaussian_hill") {
            Real peak = 50.0;
            Real sigma = 60.0;
            Real center_x = (synth_xmin + synth_xmax) / 2.0;
            Real center_y = (synth_ymin + synth_ymax) / 2.0;
            pp.query("synthetic_peak", peak);
            pp.query("synthetic_sigma", sigma);
            pp.query("synthetic_center_x", center_x);
            pp.query("synthetic_center_y", center_y);

            peaks.push_back(peak);
            sigmas.push_back(sigma);
            centers_x.push_back(center_x);
            centers_y.push_back(center_y);
        } else if (synth_type == "multi_gaussian_hill") {
            int n_vals = pp.countval("synthetic_peaks");
            if (n_vals > 0) {
                peaks.resize(n_vals);
                pp.getarr("synthetic_peaks", peaks, 0, n_vals);
            }
            n_vals = pp.countval("synthetic_sigmas");
            if (n_vals > 0) {
                sigmas.resize(n_vals);
                pp.getarr("synthetic_sigmas", sigmas, 0, n_vals);
            }
            n_vals = pp.countval("synthetic_centers_x");
            if (n_vals > 0) {
                centers_x.resize(n_vals);
                pp.getarr("synthetic_centers_x", centers_x, 0, n_vals);
            }
            n_vals = pp.countval("synthetic_centers_y");
            if (n_vals > 0) {
                centers_y.resize(n_vals);
                pp.getarr("synthetic_centers_y", centers_y, 0, n_vals);
            }

            // Defaults if arrays are empty
            if (peaks.empty()) {
                peaks = {50.0, 30.0};
                sigmas = {60.0, 40.0};
                centers_x = {100.0, 200.0};
                centers_y = {150.0, 150.0};
            }
        } else {
            amrex::Abort("wind_solver: invalid synthetic_type: " + synth_type);
        }

        if (peaks.size() != sigmas.size() || peaks.size() != centers_x.size() || peaks.size() != centers_y.size()) {
            amrex::Abort("wind_solver: size mismatch in synthetic terrain arrays: peaks, sigmas, centers_x, and centers_y must all have the same size.");
        }

        // Generate the grid points of the synthetic terrain point cloud
        x_terr.clear();
        y_terr.clear();
        z_terr.clear();

        for (int j = 0; j < synth_ny; ++j) {
            Real y = synth_ymin + j * (synth_ymax - synth_ymin) / std::max(1, synth_ny - 1);
            for (int i = 0; i < synth_nx; ++i) {
                Real x = synth_xmin + i * (synth_xmax - synth_xmin) / std::max(1, synth_nx - 1);
                
                Real z = 0.0;
                for (std::size_t m = 0; m < peaks.size(); ++m) {
                    Real r_sq = (x - centers_x[m]) * (x - centers_x[m]) + (y - centers_y[m]) * (y - centers_y[m]);
                    z += peaks[m] * std::exp(-r_sq / (2.0 * sigmas[m] * sigmas[m]));
                }
                x_terr.push_back(x);
                y_terr.push_back(y);
                z_terr.push_back(z);
            }
        }
        amrex::Print() << "wind_solver: generated synthetic terrain with type: " << synth_type << ", " << x_terr.size() << " points\n";
    } else {
        WindIO::read_terrain_file(terrain_file, x_terr, y_terr, z_terr);
    }

    x_lo = *std::min_element(x_terr.begin(), x_terr.end());
    x_hi = *std::max_element(x_terr.begin(), x_terr.end());
    y_lo = *std::min_element(y_terr.begin(), y_terr.end());
    y_hi = *std::max_element(y_terr.begin(), y_terr.end());

    amrex::Print() << "wind_solver: terrain x [" << x_lo << ", " << x_hi << "] m\n";
    amrex::Print() << "wind_solver: terrain y [" << y_lo << ", " << y_hi << "] m\n";

    nx = std::max(1, static_cast<int>(std::round((x_hi - x_lo) / dx_req)));
    ny = std::max(1, static_cast<int>(std::round((y_hi - y_lo) / dy_req)));

    dx = (x_hi - x_lo) / nx;
    dy = (y_hi - y_lo) / ny;

    std::string building_file = "";
    ParmParse pp;
    pp.query("building_file", building_file);
    if (!building_file.empty()) {
        WindIO::read_building_file(building_file, 
                                 building_xmin, building_xmax,
                                 building_ymin, building_ymax,
                                 building_zmin, building_zmax,
                                 building_rotation,
                                 building_shape,
                                 building_pitch_or_radius,
                                 building_pitch_direction,
                                 building_geom_type,
                                 building_polygon_x,
                                 building_polygon_y);
    }

    if (enable_building_porosity && !building_porosity_file.empty()) {
        WindIO::read_porous_building_file(building_porosity_file,
                                        porous_building_xmin, porous_building_xmax,
                                        porous_building_ymin, porous_building_ymax,
                                        porous_building_zmin, porous_building_zmax,
                                        porous_building_porosity,
                                        porous_building_rotation);
    }

    if (enable_windbreaks && !windbreaks_file.empty()) {
        WindIO::read_windbreaks_file(windbreaks_file,
                                     windbreak_x1, windbreak_y1,
                                     windbreak_x2, windbreak_y2,
                                     windbreak_height,
                                     windbreak_blockage,
                                     windbreak_drag_coeff);
    }

    num_time_steps = 1;
    pp.query("num_time_steps", num_time_steps);
    if (!enable_time_varying && !enable_3d_scalars) {
        num_time_steps = 1;
    }
    if (enable_time_varying) {
        std::ifstream check_file(time_series_file);
        if (check_file.good()) {
            WindIO::read_time_series_file(time_series_file,
                                         time_series_times,
                                         time_series_U_refs,
                                         time_series_V_refs);
            
            if (!time_series_times.empty()) {
                num_time_steps = static_cast<int>(time_series_times.size());
                amrex::Print() << "wind_solver: time-varying mode ENABLED - " 
                              << num_time_steps << " time steps will be computed\n";
                amrex::Print() << "wind_solver: time range: [" << time_series_times.front() 
                              << ", " << time_series_times.back() << "] s\n";
            } else {
                enable_time_varying = false;
            }
        } else {
            amrex::Print() << "wind_solver: WARNING - time-varying mode requested but file not found: "
                          << time_series_file << "\n";
            amrex::Print() << "wind_solver: disabling time-varying flow field and using default U_ref, V_ref\n";
            enable_time_varying = false;
        }
    }

    if (!precipitation_file.empty()) {
        std::ifstream check_file(precipitation_file);
        if (check_file.good()) {
            WindIO::read_precipitation_file(precipitation_file,
                                            precipitation_times,
                                            precipitation_rates);
        } else {
            amrex::Print() << "wind_solver: WARNING - precipitation file specified but not found: "
                           << precipitation_file << "\n";
        }
    }

    if (thermo_lid_params.enabled && !thermo_lid_params.flux_file.empty()) {
        std::ifstream check_file(thermo_lid_params.flux_file);
        if (check_file.good()) {
            read_thermodynamic_flux_file(thermo_lid_params.flux_file,
                                         thermo_lid_flux_times,
                                         thermo_lid_flux_values);
        } else {
            amrex::Print() << "wind_solver: WARNING - thermodynamic lid flux file specified but not found: "
                           << thermo_lid_params.flux_file << "\n";
        }
    }

    amrex::Print() << "wind_solver: terrain reading time = " 
                   << (amrex::second() - t_phase) << " s\n";

    t_phase = amrex::second();
    terrain_h.resize(static_cast<std::size_t>(nx) * ny);

    for (int j = 0; j < ny; ++j) {
        Real yc = y_lo + (j + 0.5) * dy;
        for (int i = 0; i < nx; ++i) {
            Real xc = x_lo + (i + 0.5) * dx;
            terrain_h[static_cast<std::size_t>(j) * nx + i] =
                WindInterpolation::idw_terrain(xc, yc, x_terr, y_terr, z_terr, 6, idw_exponent);
        }
    }

    if (enable_buoyancy_stratification || enable_cell_local_anisotropy) {
        bool has_temperature_file = false;
        {
            std::ifstream f(temperature_file);
            if (f.good()) {
                has_temperature_file = true;
            }
        }

        if (has_temperature_file) {
            WindIO::read_temperature_file(temperature_file, z_temp, T_temp);
            
            if (enable_diurnal_temperature) {
                amrex::Print() << "wind_solver: diurnal temperature variation enabled\n";
                amrex::Print() << "  diurnal_temperature_amplitude = " << diurnal_temperature_amplitude << " K\n";
                amrex::Print() << "  diurnal_time_of_day = " << diurnal_time_of_day << " hours\n";
                amrex::Print() << "  diurnal_phase_hour = " << diurnal_phase_hour << " hours\n";
                amrex::Print() << "  diurnal_period = " << diurnal_period << " hours\n";
                
                for (std::size_t m = 0; m < T_temp.size(); ++m) {
                    Real T_mean = T_temp[m];
                    T_temp[m] = diurnal_temperature(T_mean, diurnal_temperature_amplitude,
                                                   diurnal_time_of_day, diurnal_phase_hour, 
                                                   diurnal_period);
                }
            }
        } else {
            if (enable_buoyancy_stratification) {
                amrex::Abort("wind_solver: buoyancy stratification enabled but temperature file cannot be opened: " + temperature_file);
            } else {
                amrex::Print() << "wind_solver: WARNING: Cell-local anisotropy is enabled but temperature file '" << temperature_file << "' cannot be opened. Skipping temperature profile reading.\n";
            }
        }
        
        if (enable_buoyancy_stratification && has_temperature_file) {
            amrex::Print() << "wind_solver: buoyancy stratification enabled\n";
            amrex::Print() << "  temperature_reference = " << temperature_reference << " K\n";
            amrex::Print() << "  buoyancy_coefficient = " << buoyancy_coefficient << "\n";
            amrex::Print() << "  buoyancy_method = " << buoyancy_method << "\n";
            if (buoyancy_method == "velocity") {
                amrex::Print() << "  buoyancy_timescale = " << buoyancy_timescale << " s\n";
            }
        }
    }

    if (use_spatial_alpha_coefficients && !alpha_coefficients_file.empty()) {
        WindIO::read_alpha_coefficients_file(alpha_coefficients_file, x_alpha, y_alpha, 
                                            alpha_h_data, alpha_v_data);
        amrex::Print() << "wind_solver: spatially-varying Lagrange coefficients enabled\n";
        amrex::Print() << "  alpha_coefficients_file = " << alpha_coefficients_file << "\n";
        amrex::Print() << "  number of data points = " << x_alpha.size() << "\n";
    }

    obstacle_h = terrain_h;
    
    if (!building_xmin.empty()) {
        int n_buildings = static_cast<int>(building_xmin.size());
        for (int b = 0; b < n_buildings; ++b) {
            Real bx1 = building_xmin[b];
            Real bx2 = building_xmax[b];
            Real by1 = building_ymin[b];
            Real by2 = building_ymax[b];
            Real bz1 = building_zmin[b];
            Real bz2 = building_zmax[b];
            
            amrex::Print() << "wind_solver: building " << b + 1 
                           << ": x=[" << bx1 << ", " << bx2 << "] m"
                           << ", y=[" << by1 << ", " << by2 << "] m"
                           << ", z=[" << bz1 << ", " << bz2 << "] m\n";
            
            for (int j = 0; j < ny; ++j) {
                Real yc = y_lo + (j + 0.5) * dy;
                for (int i = 0; i < nx; ++i) {
                    Real xc = x_lo + (i + 0.5) * dx;
                    if (xc >= bx1 && xc <= bx2 && yc >= by1 && yc <= by2) {
                        std::size_t idx = static_cast<std::size_t>(j) * nx + i;
                        Real building_height = bz2 - bz1;
                        Real adjusted_building_top = terrain_h[idx] + building_height;
                        obstacle_h[idx] = std::max(obstacle_h[idx], adjusted_building_top);
                    }
                }
            }
        }
    }

    if (enable_wall_functions) {
        amrex::Print() << "wind_solver: wall functions ENABLED\n";
        if (enable_terrain_wall_function) {
            amrex::Print() << "  terrain wall function: ENABLED\n";
        }
        if (enable_flat_surface_wall_function) {
            amrex::Print() << "  flat surface wall function: ENABLED\n";
            amrex::Print() << "    z0_flat = " << wall_function_z0_flat << " m\n";
            if (wall_function_enable_flat_surface) {
                amrex::Print() << "    flat surface elevation = " 
                              << wall_function_flat_surface_elevation << " m\n";
            }
        }
        if (enable_building_wall_function) {
            amrex::Print() << "  building wall function: ENABLED\n";
            amrex::Print() << "    z0_building = " << wall_function_z0_building << " m\n";
        }
        amrex::Print() << "  blend height = " << wall_function_blend_height << " cells\n";
        amrex::Print() << "  max distance = " << wall_function_max_distance << " cells\n";
        
        if (wall_function_enable_stability) {
            amrex::Print() << "  stability correction: ENABLED\n";
            amrex::Print() << "    Obukhov length L = " << wall_function_stability_length << " m\n";
        } else {
            amrex::Print() << "  stability correction: DISABLED (neutral log-law)\n";
        }
        
        if (wall_function_enable_adaptive) {
            amrex::Print() << "  adaptive activation: ENABLED\n";
            amrex::Print() << "    resolution threshold = " << wall_function_adaptive_threshold << " (dz/z0)\n";
            amrex::Print() << "    min cells in log layer = " << wall_function_adaptive_min_cells << "\n";
        } else {
            amrex::Print() << "  adaptive activation: DISABLED (always active when enabled)\n";
        }
    } else {
        amrex::Print() << "wind_solver: wall functions DISABLED (using no-slip boundary conditions)\n";
    }

    zs_min = *std::min_element(terrain_h.begin(), terrain_h.end());
    zs_max = *std::max_element(terrain_h.begin(), terrain_h.end());
    obs_max = *std::max_element(obstacle_h.begin(), obstacle_h.end());
    amrex::Print() << "wind_solver: terrain elevation [" << zs_min
                   << ", " << zs_max << "] m\n";
    if (!building_xmin.empty()) {
        amrex::Print() << "wind_solver: obstacle height (terrain+buildings) max = "
                       << obs_max << " m\n";
    }

    amrex::Print() << "wind_solver: terrain interpolation time = " 
                   << (amrex::second() - t_phase) << " s\n";

    t_phase = amrex::second();
    Real z_lo = zs_min;
    Real z_hi = obs_max + domain_height;
    nz   = std::max(1, static_cast<int>(std::round((z_hi - z_lo) / dz_req)));
    dz   = (z_hi - z_lo) / nz;

    amrex::Print() << "wind_solver: grid " << nx << " x " << ny << " x " << nz
                   << "  (dx=" << dx << " m, dy=" << dy << " m, dz=" << dz << " m)\n";
    amrex::Print() << "wind_solver: vertical domain [" << z_lo
                   << ", " << z_hi << "] m\n";

    IntVect dom_lo(0, 0, 0);
    IntVect dom_hi(nx - 1, ny - 1, nz - 1);
    Box domain(dom_lo, dom_hi);

    RealBox rb({x_lo, y_lo, z_lo}, {x_hi, y_hi, z_hi});
    Array<int, AMREX_SPACEDIM> is_periodic{0, 0, 0};
    
    geom_ptr = std::make_unique<Geometry>(domain, &rb, CoordSys::cartesian, is_periodic.data());

    ba_ptr = std::make_unique<BoxArray>(domain);
    ba_ptr->maxSize(max_grid_size);
    dm_ptr = std::make_unique<DistributionMapping>(*ba_ptr);
    
    amrex::Print() << "wind_solver: grid setup time = " 
                   << (amrex::second() - t_phase) << " s\n";
}

void WindSolverApp::allocate_data_fields() {
    t_phase = amrex::second();

    if (enable_eb) {
        amrex::Print() << "wind_solver: initializing Embedded Boundary (EB)\n";
        
        // Set up EB2 geometry based on input parameters
        if (eb_geom_type_name == "box") {
            if (eb_box_lo.size() >= 3 && eb_box_hi.size() >= 3) {
                // Create a box geometry using EB2
                amrex::EB2::BoxIF box(amrex::RealArray{eb_box_lo[0], eb_box_lo[1], eb_box_lo[2]},
                                      amrex::RealArray{eb_box_hi[0], eb_box_hi[1], eb_box_hi[2]},
                                      eb_box_has_fluid_inside);
                amrex::EB2::GeometryShop<amrex::EB2::BoxIF> gshop(box);
                amrex::EB2::Build(gshop, *geom_ptr, 0, 0);
            } else {
                amrex::Abort("wind_solver: Invalid EB2 box parameters. Must have 3 components for box_lo and box_hi.");
            }
        } else {
            // Build with default (empty geometry)
            amrex::EB2::Build(*geom_ptr, 0, 0);
        }
        
        eb_factory = amrex::makeEBFabFactory(*geom_ptr, *ba_ptr, *dm_ptr, amrex::Vector<int>{1, 1, 1}, amrex::EBSupport::volume);
        amrex::Print() << "wind_solver: EB initialization complete\n";
    }
    
    d_terrain_h.resize(terrain_h.size());
    amrex::Gpu::copy(amrex::Gpu::hostToDevice, terrain_h.begin(), terrain_h.end(), d_terrain_h.begin());
    
    d_obstacle_h.resize(obstacle_h.size());
    amrex::Gpu::copy(amrex::Gpu::hostToDevice, obstacle_h.begin(), obstacle_h.end(), d_obstacle_h.begin());

    const std::size_t grid_size = static_cast<std::size_t>(nx) * ny;
    morphometric_d.assign(grid_size, Real(0.0));
    morphometric_z0.assign(grid_size, z0);

    if (enable_morphometric_models) {
        amrex::Print() << "wind_solver: computing localized morphometric parameters on the grid...\n";
        
        std::vector<Real> lambda_p_grid(grid_size, Real(0.0));
        std::vector<Real> lambda_f_x_grid(grid_size, Real(0.0));
        std::vector<Real> lambda_f_y_grid(grid_size, Real(0.0));
        std::vector<Real> H_avg_grid(grid_size, Real(0.0));
        std::vector<Real> sum_weight_grid(grid_size, Real(0.0));

        Real cell_area = dx * dy;

        if (!building_xmin.empty()) {
            int n_buildings = static_cast<int>(building_xmin.size());
            for (int j = 0; j < ny; ++j) {
                Real cell_y1 = y_lo + j * dy;
                Real cell_y2 = y_lo + (j + 1) * dy;
                for (int i = 0; i < nx; ++i) {
                    Real cell_x1 = x_lo + i * dx;
                    Real cell_x2 = x_lo + (i + 1) * dx;
                    std::size_t idx = static_cast<std::size_t>(j) * nx + i;

                    for (int b = 0; b < n_buildings; ++b) {
                        Real bx1 = building_xmin[b];
                        Real bx2 = building_xmax[b];
                        Real by1 = building_ymin[b];
                        Real by2 = building_ymax[b];
                        Real bz1 = building_zmin[b];
                        Real bz2 = building_zmax[b];
                        Real H_b = bz2 - bz1;

                        Real ix1 = std::max(bx1, cell_x1);
                        Real ix2 = std::min(bx2, cell_x2);
                        Real iy1 = std::max(by1, cell_y1);
                        Real iy2 = std::min(by2, cell_y2);

                        if (ix2 > ix1 && iy2 > iy1) {
                            Real w_x = ix2 - ix1;
                            Real w_y = iy2 - iy1;
                            Real A_p_b = w_x * w_y;

                            lambda_p_grid[idx] += A_p_b;
                            lambda_f_x_grid[idx] += w_y * H_b;
                            lambda_f_y_grid[idx] += w_x * H_b;
                            H_avg_grid[idx] += A_p_b * H_b;
                            sum_weight_grid[idx] += A_p_b;
                        }
                    }

                    if (sum_weight_grid[idx] > Real(1.0e-10)) {
                        H_avg_grid[idx] /= sum_weight_grid[idx];
                    } else {
                        H_avg_grid[idx] = Real(0.0);
                    }

                    lambda_p_grid[idx] /= cell_area;
                    lambda_f_x_grid[idx] /= cell_area;
                    lambda_f_y_grid[idx] /= cell_area;
                    
                    // Clamp plan area index (lambda_p) to [0, 0.95] to prevent division by zero in equations
                    // when the canopy is completely solid, and clamp frontal area indices (lambda_f) to [0, 2.0]
                    // to keep them within physically realistic limits for highly dense obstacle arrays.
                    lambda_p_grid[idx] = std::max(Real(0.0), std::min(lambda_p_grid[idx], Real(0.95)));
                    lambda_f_x_grid[idx] = std::max(Real(0.0), std::min(lambda_f_x_grid[idx], Real(2.0)));
                    lambda_f_y_grid[idx] = std::max(Real(0.0), std::min(lambda_f_y_grid[idx], Real(2.0)));
                }
            }
        }

        Real speed_ref = std::sqrt(U_ref * U_ref + V_ref * V_ref);
        Real ux_hat = (speed_ref > Real(1.0e-10)) ? U_ref / speed_ref : Real(1.0);
        Real uy_hat = (speed_ref > Real(1.0e-10)) ? V_ref / speed_ref : Real(0.0);

        Real abs_ux = std::abs(ux_hat);
        Real abs_uy = std::abs(uy_hat);

        for (std::size_t idx = 0; idx < grid_size; ++idx) {
            Real lambda_f = abs_ux * lambda_f_x_grid[idx] + abs_uy * lambda_f_y_grid[idx];
            Real H = H_avg_grid[idx];
            Real lp = lambda_p_grid[idx];
            
            Real d_val = Real(0.0);
            Real z0_val = z0;

            if (morphometric_model_type == "macdonald") {
                MorphometricModels::compute_macdonald(H, lp, lambda_f, morphometric_drag_coeff, z0, d_val, z0_val);
            } else if (morphometric_model_type == "kutzbach") {
                MorphometricModels::compute_kutzbach(H, lp, lambda_f, morphometric_drag_coeff, z0, d_val, z0_val);
            } else if (morphometric_model_type == "bottema") {
                MorphometricModels::compute_bottema(H, lp, lambda_f, morphometric_drag_coeff, z0, d_val, z0_val);
            }

            morphometric_d[idx] = d_val;
            morphometric_z0[idx] = z0_val;
        }

        amrex::Print() << "  Morphometric grid computation complete.\n";
    }

    d_morphometric_d.resize(morphometric_d.size());
    amrex::Gpu::copy(amrex::Gpu::hostToDevice, morphometric_d.begin(), morphometric_d.end(), d_morphometric_d.begin());

    d_morphometric_z0.resize(morphometric_z0.size());
    amrex::Gpu::copy(amrex::Gpu::hostToDevice, morphometric_z0.begin(), morphometric_z0.end(), d_morphometric_z0.begin());

    vel0_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 3, 1);
    vel_c_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 3, 0);
    lam_ptr  = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 1);
    rhs_ptr  = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    
    alpha_h_field_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 1);
    alpha_v_field_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 1);

    vel0_ptr->setVal(0.0);
    vel_c_ptr->setVal(0.0);
    lam_ptr->setVal(0.0);
    rhs_ptr->setVal(0.0);
    
    if (use_spatial_alpha_coefficients && !alpha_h_data.empty()) {
        Gpu::DeviceVector<Real> d_x_alpha(x_alpha.size());
        Gpu::DeviceVector<Real> d_y_alpha(y_alpha.size());
        Gpu::DeviceVector<Real> d_alpha_h(alpha_h_data.size());
        Gpu::DeviceVector<Real> d_alpha_v(alpha_v_data.size());
        
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, x_alpha.begin(), x_alpha.end(), d_x_alpha.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, y_alpha.begin(), y_alpha.end(), d_y_alpha.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, alpha_h_data.begin(), alpha_h_data.end(), d_alpha_h.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, alpha_v_data.begin(), alpha_v_data.end(), d_alpha_v.begin());
        
        const Real* d_x_alpha_ptr = d_x_alpha.data();
        const Real* d_y_alpha_ptr = d_y_alpha.data();
        const Real* d_alpha_h_ptr = d_alpha_h.data();
        const Real* d_alpha_v_ptr = d_alpha_v.data();
        const int n_alpha_pts = static_cast<int>(x_alpha.size());
        
        const Real x_lo_cap = x_lo;
        const Real y_lo_cap = y_lo;
        const Real dx_cap = dx;
        const Real dy_cap = dy;
        const Real idw_exponent_cap = idw_exponent;
        
        for (MFIter mfi(*alpha_h_field_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto alpha_h_arr = alpha_h_field_ptr->array(mfi);
            auto alpha_v_arr = alpha_v_field_ptr->array(mfi);
            
            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real xc = x_lo_cap + (i + Real(0.5)) * dx_cap;
                Real yc = y_lo_cap + (j + Real(0.5)) * dy_cap;
                
                auto alpha_vals = WindInterpolation::idw_alpha_coefficients_gpu(xc, yc,
                    d_x_alpha_ptr, d_y_alpha_ptr,
                    d_alpha_h_ptr, d_alpha_v_ptr, n_alpha_pts, 6, idw_exponent_cap);
                
                alpha_h_arr(i, j, k) = alpha_vals.first;
                alpha_v_arr(i, j, k) = alpha_vals.second;
            });
        }
        
        alpha_h_field_ptr->FillBoundary(geom_ptr->periodicity());
        alpha_v_field_ptr->FillBoundary(geom_ptr->periodicity());
        
        amrex::Print() << "wind_solver: filled spatially-varying alpha coefficient fields\n";
    } else {
        alpha_h_field_ptr->setVal(alpha_h);
        alpha_v_field_ptr->setVal(alpha_v);
    }

    lambda_damped_ptr  = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 1);
    p_prime_ptr        = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 1);
    terrain_type_ptr   = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    terrain_slope_ptr  = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    terrain_curvature_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    terrain_aspect_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    adaptive_roughness_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    adaptive_stability_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    temp_ptr           = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    
    // Allocate 3D scalar transport fields if enabled
    if (enable_3d_scalars) {
        temp_3d_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 1);
        moisture_3d_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 1);
        temp_3d_old_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 1);
        moisture_3d_old_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 1);
        
        temp_3d_ptr->setVal(temperature_reference);
        moisture_3d_ptr->setVal(0.0);
        temp_3d_old_ptr->setVal(temperature_reference);
        moisture_3d_old_ptr->setVal(0.0);
    }

    lambda_damped_ptr->setVal(0.0);
    p_prime_ptr->setVal(0.0);
    terrain_type_ptr->setVal(0);
    terrain_slope_ptr->setVal(0.0);
    terrain_curvature_ptr->setVal(0.0);
    terrain_aspect_ptr->setVal(0.0);
    adaptive_roughness_ptr->setVal(z0);
    adaptive_stability_ptr->setVal(1.0);
    temp_ptr->setVal(temperature_reference);

    u_star_ptr     = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    tau_flux_ptr   = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 2, 0);
    cd_ptr         = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    shf_ptr        = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    lhf_ptr        = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    z_bl_diag_ptr  = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);
    p_pert_ptr     = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 1, 0);

    u_star_ptr->setVal(0.0);
    tau_flux_ptr->setVal(0.0);
    cd_ptr->setVal(0.0);
    shf_ptr->setVal(0.0);
    lhf_ptr->setVal(0.0);
    z_bl_diag_ptr->setVal(0.0);
    p_pert_ptr->setVal(0.0);
}

void WindSolverApp::initialize_wind_fields(int time_step) {
    Real current_time = enable_time_varying ? time_series_times[time_step] : Real(0.0);
    if (thermo_lid_params.enabled) {
        capping_lid_height = compute_thermodynamic_zi(current_time, thermo_lid_params, thermo_lid_flux_times, thermo_lid_flux_values);
        amrex::Print() << "wind_solver: thermodynamic lid model '" << thermo_lid_params.model 
                       << "' calculated z_i(t) = " << capping_lid_height << " m at t = " << current_time << " s\n";
    }

    if (enable_time_varying) {
        U_ref = time_series_U_refs[time_step];
        V_ref = time_series_V_refs[time_step];
        amrex::Print() << "\n";
        amrex::Print() << "wind_solver: ========== TIME STEP " << (time_step + 1) << " / " 
                     << num_time_steps << " ==========\n";
        amrex::Print() << "wind_solver: t = " << time_series_times[time_step] << " s, "
                     << "U_ref = " << U_ref << " m/s, V_ref = " << V_ref << " m/s\n";
    } else if (num_time_steps > 1) {
        amrex::Print() << "\nwind_solver: TIME STEP " << (time_step + 1) << " / " 
                     << num_time_steps << "\n";
    }

    current_precipitation_rate = 0.0;
    if (!precipitation_times.empty()) {
        Real current_time = enable_time_varying ? time_series_times[time_step] : Real(0.0);
        if (precipitation_times.size() == 1) {
            current_precipitation_rate = precipitation_rates[0];
        } else if (current_time <= precipitation_times.front()) {
            current_precipitation_rate = precipitation_rates.front();
        } else if (current_time >= precipitation_times.back()) {
            current_precipitation_rate = precipitation_rates.back();
        } else {
            for (std::size_t m = 0; m < precipitation_times.size() - 1; ++m) {
                if (current_time >= precipitation_times[m] && current_time <= precipitation_times[m+1]) {
                    Real t0 = precipitation_times[m];
                    Real t1 = precipitation_times[m+1];
                    Real r0 = precipitation_rates[m];
                    Real r1 = precipitation_rates[m+1];
                    Real factor = (current_time - t0) / (t1 - t0);
                    current_precipitation_rate = r0 + factor * (r1 - r0);
                    break;
                }
            }
        }
        amrex::Print() << "wind_solver: current precipitation rate = " << current_precipitation_rate << " mm/h\n";
    }

    // Populate temp_ptr with interpolated local temperature for diagnostics and Richardson mapping
    if (!z_temp.empty()) {
        const int n_temp_pts = static_cast<int>(z_temp.size());
        amrex::Gpu::DeviceVector<Real> d_temp_z_init(n_temp_pts), d_temp_T_init(n_temp_pts);
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, z_temp.begin(), z_temp.end(), d_temp_z_init.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, T_temp.begin(), T_temp.end(), d_temp_T_init.begin());
        Real const* d_temp_z_ptr = d_temp_z_init.data();
        Real const* d_temp_T_ptr = d_temp_T_init.data();
        const Real T_ref_val = temperature_reference;
        const Real z_lo_cap_val = zs_min;
        const Real dz_cap_val = dz;

        for (MFIter mfi(*temp_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto temp_arr = temp_ptr->array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real z_physical = z_lo_cap_val + (k + Real(0.5)) * dz_cap_val;
                Real T_local = T_ref_val;
                
                if (n_temp_pts == 1) {
                    T_local = d_temp_T_ptr[0];
                } else if (z_physical <= d_temp_z_ptr[0]) {
                    T_local = d_temp_T_ptr[0];
                } else if (z_physical >= d_temp_z_ptr[n_temp_pts - 1]) {
                    T_local = d_temp_T_ptr[n_temp_pts - 1];
                } else {
                    for (int m = 0; m < n_temp_pts - 1; ++m) {
                        if (z_physical >= d_temp_z_ptr[m] && 
                            z_physical <= d_temp_z_ptr[m + 1]) {
                            T_local = temperature_linear_interp(
                                z_physical,
                                d_temp_z_ptr[m], d_temp_T_ptr[m],
                                d_temp_z_ptr[m + 1], d_temp_T_ptr[m + 1]);
                             break;
                        }
                    }
                }
                temp_arr(i, j, k) = T_local;
            });
        }
        amrex::Gpu::streamSynchronize();
    } else {
        temp_ptr->setVal(temperature_reference);
    }

    if (enable_pg_stability) {
        Real speed_ref = std::sqrt(U_ref * U_ref + V_ref * V_ref);
        PGStabilityClass pg_class = pasquill_gifford_class(speed_ref, solar_radiation, is_nighttime, cloud_cover);
        
        if (current_precipitation_rate > precipitation_stability_threshold) {
            if (pg_class == PGStabilityClass::A || pg_class == PGStabilityClass::B || pg_class == PGStabilityClass::C) {
                pg_class = PGStabilityClass::D; // Force Neutral
                amrex::Print() << "wind_solver: Precipitation rate exceeds threshold. Adjusting unstable PGT stability class to Neutral (D).\n";
            }
        }
        stability_length = pg_class_to_obukhov_length(pg_class);
        enable_stability_correction = true;
    } else if (enable_stability_correction) {
        if (stability_length < 0.0 && current_precipitation_rate > precipitation_stability_threshold) {
            stability_length = 10000.0; // Force Neutral
            amrex::Print() << "wind_solver: Precipitation rate exceeds threshold. Adjusting unstable Obukhov length to Neutral (10000.0 m).\n";
        }
    }
    
    if (wall_function_enable_stability) {
        if (wall_function_stability_length < 0.0 && current_precipitation_rate > precipitation_stability_threshold) {
            wall_function_stability_length = 10000.0; // Force Neutral
            amrex::Print() << "wind_solver: Precipitation rate exceeds threshold. Adjusting unstable wall function Obukhov length to Neutral (10000.0 m).\n";
        }
    }
    
    vel0_ptr->setVal(0.0);
    lam_ptr->setVal(0.0);
    rhs_ptr->setVal(0.0);

    t_phase = amrex::second();
    amrex::Print() << "wind_solver: initializing wind field with mode: " << init_mode << "\n";

    std::vector<Real> z0_h(static_cast<std::size_t>(nx) * ny, z0);
    std::vector<Real> landuse_h(static_cast<std::size_t>(nx) * ny, -1.0);
    const Real* d_z0_pos_ptr = nullptr;
    const Real* d_landuse_pos_ptr = nullptr;
    
    if (init_mode == "loglaw" && (use_z0_file || enable_landuse_roughness)) {
        if (enable_landuse_roughness) {
            std::string lu_file = landuse_file_class.empty() ? landuse_file : landuse_file_class;
            if (lu_file.empty()) {
                amrex::Abort("wind_solver: enable_landuse_roughness is true but no landuse_file is specified!");
            }
            amrex::Print() << "wind_solver: reading landuse classification for roughness from " << lu_file << "\n";
            std::vector<Real> x_lu, y_lu, landuse_data;
            WindIO::read_roughness_file(lu_file, x_lu, y_lu, landuse_data);
            
            for (int j = 0; j < ny; ++j) {
                for (int i = 0; i < nx; ++i) {
                    Real xc = x_lo + (i + Real(0.5)) * dx;
                    Real yc = y_lo + (j + Real(0.5)) * dy;
                    
                    std::vector<std::pair<Real, int>> d2(x_lu.size());
                    for (std::size_t m = 0; m < x_lu.size(); ++m) {
                        Real dx_pt = xc - x_lu[m];
                        Real dy_pt = yc - y_lu[m];
                        d2[m] = {dx_pt * dx_pt + dy_pt * dy_pt, static_cast<int>(m)};
                    }
                    std::sort(d2.begin(), d2.end());
                    
                    Real z0_val = Real(0.0);
                    Real landuse_interp = -1.0;
                    
                    if (enable_mosaic_roughness) {
                        Real wsum = 0.0;
                        Real ln_z0_sum = 0.0;
                        Real lu_sum = 0.0;
                        bool exact_match = false;
                        
                        const int n_pts = std::min(6, static_cast<int>(d2.size()));
                        for (int m = 0; m < n_pts; ++m) {
                            Real dist = std::sqrt(d2[m].first);
                            if (dist < Real(1.0e-12)) {
                                int lu_type = static_cast<int>(std::round(landuse_data[d2[m].second]));
                                z0_val = get_z0_from_landuse(lu_type);
                                landuse_interp = Real(lu_type);
                                exact_match = true;
                                break;
                            }
                            Real w = Real(1.0) / (dist * dist);
                            wsum += w;
                            
                            int lu_type = static_cast<int>(std::round(landuse_data[d2[m].second]));
                            Real z0_m = get_z0_from_landuse(lu_type);
                            ln_z0_sum += w * std::log(std::max(z0_m, Real(1.0e-6)));
                            lu_sum += w * landuse_data[d2[m].second];
                        }
                        
                        if (!exact_match && wsum > Real(0.0)) {
                            z0_val = std::exp(ln_z0_sum / wsum);
                            landuse_interp = lu_sum / wsum;
                        }
                    } else {
                        Real wsum = 0.0;
                        Real lu_sum = 0.0;
                        const int n_pts = std::min(6, static_cast<int>(d2.size()));
                        for (int m = 0; m < n_pts; ++m) {
                            Real dist = std::sqrt(d2[m].first);
                            if (dist < Real(1.0e-12)) {
                                landuse_interp = landuse_data[d2[m].second];
                                wsum = 1.0;
                                break;
                            }
                            Real w = Real(1.0) / (dist * dist);
                            wsum += w;
                            lu_sum += w * landuse_data[d2[m].second];
                        }
                        if (wsum > Real(0.0)) {
                            landuse_interp = lu_sum / wsum;
                        }
                        int lu_type_dom = static_cast<int>(std::round(landuse_interp));
                        z0_val = get_z0_from_landuse(lu_type_dom);
                    }
                    
                    int lu_type = static_cast<int>(std::round(landuse_interp));
                    landuse_h[static_cast<std::size_t>(j) * nx + i] = Real(lu_type);
                    z0_h[static_cast<std::size_t>(j) * nx + i] = z0_val;
                }
            }
            
            d_landuse_pos.resize(landuse_h.size());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice, landuse_h.begin(), landuse_h.end(), d_landuse_pos.begin());
            d_landuse_pos_ptr = d_landuse_pos.data();
            
            use_z0_file = true; // treat as having spatially varying roughness
        }
        
        if (use_z0_file && !enable_landuse_roughness) {
            amrex::Print() << "wind_solver: reading position-dependent roughness from " << z0_file << "\n";
            std::vector<Real> x_z0, y_z0, z0_data;
            WindIO::read_roughness_file(z0_file, x_z0, y_z0, z0_data);
            
            for (int j = 0; j < ny; ++j) {
                for (int i = 0; i < nx; ++i) {
                    Real xc = x_lo + (i + Real(0.5)) * dx;
                    Real yc = y_lo + (j + Real(0.5)) * dy;
                    
                    Real z0_interp = z0;
                    Real wsum = 0.0;
                    Real z0_sum = 0.0;
                    std::vector<std::pair<Real, int>> d2(x_z0.size());
                    for (std::size_t m = 0; m < x_z0.size(); ++m) {
                        Real dx_pt = xc - x_z0[m];
                        Real dy_pt = yc - y_z0[m];
                        d2[m] = {dx_pt * dx_pt + dy_pt * dy_pt, static_cast<int>(m)};
                    }
                    std::sort(d2.begin(), d2.end());
                    
                    const int n_pts = std::min(6, static_cast<int>(d2.size()));
                    for (int m = 0; m < n_pts; ++m) {
                        Real dist = std::sqrt(d2[m].first);
                        if (dist < Real(1.0e-12)) {
                            z0_interp = z0_data[d2[m].second];
                            wsum = 1.0;
                            break;
                        }
                        Real w = Real(1.0) / (dist * dist);
                        wsum += w;
                        z0_sum += w * z0_data[d2[m].second];
                    }
                    if (wsum > Real(0.0)) {
                        z0_interp = z0_sum / wsum;
                    }
                    
                    z0_h[static_cast<std::size_t>(j) * nx + i] = z0_interp;
                }
            }
        }
        
        d_z0_pos.resize(z0_h.size());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, z0_h.begin(), z0_h.end(), d_z0_pos.begin());
        d_z0_pos_ptr = d_z0_pos.data();
        
        amrex::Print() << "wind_solver: position-dependent roughness interpolated to grid\n";
    }

    std::vector<Real> canopy_height_h(static_cast<std::size_t>(nx) * ny, canopy_height);
    std::vector<Real> frontal_area_index_h(static_cast<std::size_t>(nx) * ny, frontal_area_index);
    const Real* d_canopy_height_ptr = nullptr;
    const Real* d_frontal_area_index_ptr = nullptr;

    if (enable_canopy && !canopy_file.empty()) {
        amrex::Print() << "wind_solver: reading position-dependent canopy from " << canopy_file << "\n";
        std::vector<Real> x_can, y_can, h_can, fai_can;
        WindIO::read_canopy_file(canopy_file, x_can, y_can, h_can, fai_can);
        
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                Real xc = x_lo + (i + Real(0.5)) * dx;
                Real yc = y_lo + (j + Real(0.5)) * dy;
                
                Real h_interp = canopy_height;
                Real fai_interp = frontal_area_index;
                Real wsum = 0.0;
                Real h_sum = 0.0;
                Real fai_sum = 0.0;
                
                std::vector<std::pair<Real, int>> d2(x_can.size());
                for (std::size_t m = 0; m < x_can.size(); ++m) {
                    Real dx_pt = xc - x_can[m];
                    Real dy_pt = yc - y_can[m];
                    d2[m] = {dx_pt * dx_pt + dy_pt * dy_pt, static_cast<int>(m)};
                }
                std::sort(d2.begin(), d2.end());
                
                // Use up to 6 nearest neighbors for Inverse Distance Weighting (IDW) 
                // to balance local smoothness with computational efficiency.
                const int n_pts = std::min(6, static_cast<int>(d2.size()));
                for (int m = 0; m < n_pts; ++m) {
                    Real dist = std::sqrt(d2[m].first);
                    if (dist < Real(1.0e-12)) {
                        h_interp = h_can[d2[m].second];
                        fai_interp = fai_can[d2[m].second];
                        wsum = 1.0;
                        break;
                    }
                    Real w = Real(1.0) / (dist * dist);
                    wsum += w;
                    h_sum += w * h_can[d2[m].second];
                    fai_sum += w * fai_can[d2[m].second];
                }
                if (wsum > Real(0.0) && d2[0].first >= Real(1.0e-12)) {
                    h_interp = h_sum / wsum;
                    fai_interp = fai_sum / wsum;
                }
                
                canopy_height_h[static_cast<std::size_t>(j) * nx + i] = h_interp;
                frontal_area_index_h[static_cast<std::size_t>(j) * nx + i] = fai_interp;
            }
        }
        
        d_canopy_height.resize(canopy_height_h.size());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, canopy_height_h.begin(), canopy_height_h.end(), d_canopy_height.begin());
        d_canopy_height_ptr = d_canopy_height.data();
        
        d_frontal_area_index.resize(frontal_area_index_h.size());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, frontal_area_index_h.begin(), frontal_area_index_h.end(), d_frontal_area_index.begin());
        d_frontal_area_index_ptr = d_frontal_area_index.data();
        
        amrex::Print() << "wind_solver: position-dependent canopy interpolated to grid\n";
    }

    const int nx_cap = nx;
    const int ny_cap = ny;
    const Real dx_cap = dx;
    const Real dy_cap = dy;
    const Real dz_cap = dz;
    const Real z_lo_cap = zs_min;
    const Real* d_terr_ptr = d_obstacle_h.data();
    const Real z0_cap = z0;

    if (enable_terrain_analysis) {
        amrex::Print() << "wind_solver: computing multi-scale terrain analysis...\n";
        
        for (MFIter mfi(*terrain_slope_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto slope_arr = terrain_slope_ptr->array(mfi);
            auto curv_arr = terrain_curvature_ptr->array(mfi);
            auto aspect_arr = terrain_aspect_ptr->array(mfi);
            auto ttype_arr = terrain_type_ptr->array(mfi);
            auto adap_rough = adaptive_roughness_ptr->array(mfi);
            auto adap_stab = adaptive_stability_ptr->array(mfi);
            
            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                if (i > 0 && i < nx_cap - 1 && j > 0 && j < ny_cap - 1) {
                    Real h_c  = d_terr_ptr[j * nx_cap + i];
                    Real h_e  = d_terr_ptr[j * nx_cap + (i+1)];
                    Real h_w  = d_terr_ptr[j * nx_cap + (i-1)];
                    Real h_n  = d_terr_ptr[(j+1) * nx_cap + i];
                    Real h_s  = d_terr_ptr[(j-1) * nx_cap + i];
                    
                    Real dh_dx = (h_e - h_w) / (Real(2.0) * dx_cap);
                    Real dh_dy = (h_n - h_s) / (Real(2.0) * dy_cap);
                    Real slope = std::sqrt(dh_dx * dh_dx + dh_dy * dh_dy);
                    
                    Real d2h_dx2 = (h_e - Real(2.0) * h_c + h_w) / (dx_cap * dx_cap);
                    Real d2h_dy2 = (h_n - Real(2.0) * h_c + h_s) / (dy_cap * dy_cap);
                    Real curvature = d2h_dx2 + d2h_dy2;
                    
                    Real aspect = std::atan2(dh_dy, dh_dx);
                    
                    slope_arr(i, j, k) = slope;
                    curv_arr(i, j, k) = curvature;
                    aspect_arr(i, j, k) = aspect;
                    
                    Real slope_threshold_moderate = Real(0.1);
                    Real slope_threshold_steep = Real(0.3);
                    
                    int ttype;
                    if (slope < slope_threshold_moderate) {
                        ttype = 0;
                    } else if (slope < slope_threshold_steep) {
                        ttype = 1;
                    } else {
                        ttype = 2;
                    }
                    ttype_arr(i, j, k) = ttype;
                    
                    Real rough_fact_mod = Real(0.15);
                    Real rough_fact_steep = Real(0.75);
                    
                    Real z0_adaptive;
                    if (ttype == 0) {
                        z0_adaptive = z0_cap;
                    } else if (ttype == 1) {
                        z0_adaptive = z0_cap * (Real(1.0) + rough_fact_mod);
                    } else {
                        z0_adaptive = z0_cap * (Real(1.0) + rough_fact_steep);
                    }
                    adap_rough(i, j, k) = z0_adaptive;
                    
                    Real stab_flat = Real(0.8);
                    Real stab_mod = Real(1.0);
                    Real stab_steep = Real(1.3);
                    
                    Real stab_scale;
                    if (ttype == 0) {
                        stab_scale = stab_flat;
                    } else if (ttype == 1) {
                        stab_scale = stab_mod;
                    } else {
                        stab_scale = stab_steep;
                    }
                    adap_stab(i, j, k) = stab_scale;
                } else {
                    slope_arr(i, j, k) = Real(0.0);
                    curv_arr(i, j, k) = Real(0.0);
                    aspect_arr(i, j, k) = Real(0.0);
                    ttype_arr(i, j, k) = 0;
                    adap_rough(i, j, k) = z0_cap;
                    adap_stab(i, j, k) = Real(1.0);
                }
            });
        }
        amrex::Print() << "wind_solver: terrain analysis complete\n";
    }

    Gpu::DeviceVector<Real> d_terr_grad_x, d_terr_grad_y;
    Real const* d_terr_grad_x_ptr = nullptr;
    Real const* d_terr_grad_y_ptr = nullptr;
    
    if (enable_terrain_kinematic_bc) {
        std::vector<Real> terrain_grad_x(static_cast<std::size_t>(nx) * ny, 0.0);
        std::vector<Real> terrain_grad_y(static_cast<std::size_t>(nx) * ny, 0.0);
        
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                std::size_t idx = static_cast<std::size_t>(j) * nx + i;
                if (i == 0) {
                    terrain_grad_x[idx] = (terrain_h[static_cast<std::size_t>(j) * nx + (i+1)] - terrain_h[idx]) / dx;
                } else if (i == nx - 1) {
                    terrain_grad_x[idx] = (terrain_h[idx] - terrain_h[static_cast<std::size_t>(j) * nx + (i-1)]) / dx;
                } else {
                    terrain_grad_x[idx] = (terrain_h[static_cast<std::size_t>(j) * nx + (i+1)] - terrain_h[static_cast<std::size_t>(j) * nx + (i-1)]) / (2.0 * dx);
                }
                
                if (j == 0) {
                    terrain_grad_y[idx] = (terrain_h[static_cast<std::size_t>(j+1) * nx + i] - terrain_h[idx]) / dy;
                } else if (j == ny - 1) {
                    terrain_grad_y[idx] = (terrain_h[idx] - terrain_h[static_cast<std::size_t>(j-1) * nx + i]) / dy;
                } else {
                    terrain_grad_y[idx] = (terrain_h[static_cast<std::size_t>(j+1) * nx + i] - terrain_h[static_cast<std::size_t>(j-1) * nx + i]) / (2.0 * dy);
                }
            }
        }
        
        d_terr_grad_x.resize(terrain_grad_x.size());
        d_terr_grad_y.resize(terrain_grad_y.size());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, terrain_grad_x.begin(), terrain_grad_x.end(), d_terr_grad_x.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, terrain_grad_y.begin(), terrain_grad_y.end(), d_terr_grad_y.begin());
        d_terr_grad_x_ptr = d_terr_grad_x.data();
        d_terr_grad_y_ptr = d_terr_grad_y.data();
    }

    Gpu::DeviceVector<Real> d_temp_z, d_temp_T;
    Real const* d_temp_z_ptr = nullptr;
    Real const* d_temp_T_ptr = nullptr;
    int n_temp_points = 0;
    
    if (enable_buoyancy_stratification && !z_temp.empty()) {
        n_temp_points = static_cast<int>(z_temp.size());
        d_temp_z.resize(z_temp.size());
        d_temp_T.resize(T_temp.size());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, z_temp.begin(), z_temp.end(), d_temp_z.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, T_temp.begin(), T_temp.end(), d_temp_T.begin());
        d_temp_z_ptr = d_temp_z.data();
        d_temp_T_ptr = d_temp_T.data();
    }

    Gpu::DeviceVector<Real> d_vel_u(0), d_vel_v(0), d_vel_w(0);
    Real const* d_vel_u_ptr = nullptr;
    Real const* d_vel_v_ptr = nullptr;
    Real const* d_vel_w_ptr = nullptr;

    if (init_mode == "loglaw" || init_mode == "deaves_harris" || init_mode == "powerlaw_above_bl") {
        Real speed_ref = std::sqrt(U_ref * U_ref + V_ref * V_ref);
        const Real kappa = 0.41;

        Real ustar = 0.0;
        if (init_mode == "deaves_harris") {
            Real zg = bl_depth_param;
            Real ratio = z_ref / zg;
            Real term = std::log((z_ref + z0) / z0) + Real(5.75) * ratio - Real(1.88) * ratio * ratio - Real(1.33) * std::pow(ratio, 3) + Real(0.25) * std::pow(ratio, 4);
            ustar = (speed_ref > Real(1.0e-10) && std::abs(term) > Real(1.0e-10))
                  ? kappa * speed_ref / term
                  : Real(0.0);
        } else {
            ustar = (speed_ref > Real(1.0e-10))
                  ? kappa * speed_ref / std::log((z_ref + z0) / z0)
                  : Real(0.0);
        }

        Real ux_hat = (speed_ref > Real(1.0e-10)) ? U_ref / speed_ref : Real(1.0);
        Real uy_hat = (speed_ref > Real(1.0e-10)) ? V_ref / speed_ref : Real(0.0);

        OrographicParams orog_params;
        orog_params.enabled = enable_orographic_speedup;
        orog_params.hill_length_scale = orographic_hill_length_scale;
        orog_params.speedup_factor_max = orographic_speedup_factor_max;
        orog_params.separation_factor = orographic_separation_factor;
        orog_params.smoothing_factor = orographic_smoothing_factor;
        
        ThermalCirculationParams thermal_params;
        thermal_params.enabled = enable_thermal_circulation;
        thermal_params.temperature_contrast = thermal_temperature_contrast;
        thermal_params.reference_temperature = thermal_reference_temperature;
        thermal_params.thermal_coefficient = thermal_coefficient;
        thermal_params.vertical_decay_height = thermal_vertical_decay_height;
        thermal_params.distance_scale = thermal_distance_scale;
        
        const Real coastline_x = thermal_coastline_x;
        const Real coastline_y = thermal_coastline_y;
        const Real coast_normal_x = thermal_coast_normal_x;
        const Real coast_normal_y = thermal_coast_normal_y;
        
        TerrainBlockingParams blocking_params;
        blocking_params.enabled = enable_terrain_blocking;
        blocking_params.brunt_vaisala_frequency = terrain_blocking_brunt_vaisala_frequency;
        blocking_params.blocking_reduction_factor = terrain_blocking_reduction_factor;
        blocking_params.transition_froude = terrain_blocking_transition_froude;
        blocking_params.flank_enhancement_factor = terrain_blocking_flank_enhancement;

        SlopeFlowParams slope_flow_params;
        slope_flow_params.enabled = enable_slope_flows;
        slope_flow_params.temperature_diff = slope_flow_temperature_diff;
        slope_flow_params.reference_temperature = slope_flow_reference_temperature;
        slope_flow_params.empirical_coefficient = slope_flow_empirical_coefficient;
        slope_flow_params.vertical_decay_height = slope_flow_vertical_decay_height;
        slope_flow_params.min_slope = slope_flow_min_slope;

        ValleyChannelingParams valley_params;
        valley_params.enabled = enable_valley_channeling;
        valley_params.valley_axis_angle = valley_axis_angle_deg * (MathConstants::pi / 180.0);
        valley_params.valley_width = valley_width;
        valley_params.valley_depth = valley_depth;
        valley_params.channeling_strength_max = valley_channeling_strength_max;
        valley_params.speedup_factor_narrow = valley_speedup_factor_narrow;
        valley_params.slowdown_factor_wide = valley_slowdown_factor_wide;

        GapFlowParams gap_params;
        gap_params.enabled = enable_gap_flow;
        gap_params.gap_orientation_deg = gap_flow_orientation;
        gap_params.gap_width = gap_flow_width;
        gap_params.gap_depth = gap_flow_depth;
        gap_params.pressure_coefficient = gap_flow_pressure_coefficient;
        gap_params.speedup_factor_max = gap_flow_speedup_max;
        gap_params.gap_center_x = gap_flow_center_x;
        gap_params.gap_center_y = gap_flow_center_y;
        gap_params.transition_width = gap_flow_transition_width;
        gap_params.vertical_extent = gap_flow_vertical_extent;

        CanopyParams canopy_params;
        canopy_params.enabled = enable_canopy;
        canopy_params.height = canopy_height;
        canopy_params.frontal_area_index = frontal_area_index;
        canopy_params.plan_area_index = plan_area_index;
        canopy_params.drag_coefficient = canopy_drag_coeff;
        canopy_params.attenuation_coeff = canopy_attenuation;
        canopy_params.use_exponential_profile = use_exponential_profile;
        canopy_params.profile_type = canopy_profile_type;

        if (enable_canopy) {
            amrex::Print() << "wind_solver: canopy model enabled\n";
            amrex::Print() << "  canopy_height = " << canopy_height << " m\n";
            amrex::Print() << "  frontal_area_index = " << frontal_area_index << "\n";
            amrex::Print() << "  plan_area_index = " << plan_area_index << "\n";
            amrex::Print() << "  canopy_drag_coeff = " << canopy_drag_coeff << "\n";
            if (use_exponential_profile) {
                amrex::Print() << "  using Shaw-Pereira exponential profile (profile_type = " << canopy_profile_type << ")\n";
                amrex::Print() << "  attenuation_coeff = " << canopy_attenuation << "\n";
            } else {
                amrex::Print() << "  using MacDonald displacement height\n";
            }
        }

        if (enable_morphometric_models) {
            amrex::Print() << "wind_solver: morphometric models enabled\n";
            amrex::Print() << "  model_type = " << morphometric_model_type << "\n";
            amrex::Print() << "  drag_coefficient = " << morphometric_drag_coeff << "\n";
        }
        
        if (enable_ekman_veer) {
            amrex::Print() << "wind_solver: Ekman spiral wind veer enabled\n";
            amrex::Print() << "  latitude = " << latitude << " degrees\n";
            amrex::Print() << "  total_veer = " << ekman_veer_total << " degrees\n";
            amrex::Print() << "  veer_height = " << ekman_veer_height << " m\n";
        }
        
        if (enable_wind_direction_gradient) {
            amrex::Print() << "wind_solver: linear wind direction gradient enabled\n";
            amrex::Print() << "  shear_rate = " << wind_direction_shear_rate << " degrees/100m\n";
        }
        
        if (enable_fetch_roughness_transition) {
            amrex::Print() << "wind_solver: fetch-dependent roughness transition enabled (infrastructure)\n";
            amrex::Print() << "  blending_height = " << fetch_transition_blending_height << " m\n";
        }

        if (enable_orographic_speedup) {
            amrex::Print() << "wind_solver: orographic speedup enabled\n";
            amrex::Print() << "  hill_length_scale = " << orographic_hill_length_scale << " m\n";
            amrex::Print() << "  speedup_factor_max = " << orographic_speedup_factor_max << "\n";
            amrex::Print() << "  separation_factor = " << orographic_separation_factor << "\n";
            amrex::Print() << "  smoothing_factor = " << orographic_smoothing_factor << "\n";
        }
        
        if (enable_thermal_circulation) {
            amrex::Print() << "wind_solver: thermal circulation enabled\n";
            amrex::Print() << "  temperature_contrast = " << thermal_temperature_contrast << " K\n";
            amrex::Print() << "  reference_temperature = " << thermal_reference_temperature << " K\n";
            amrex::Print() << "  thermal_coefficient = " << thermal_coefficient << "\n";
            amrex::Print() << "  vertical_decay_height = " << thermal_vertical_decay_height << " m\n";
            amrex::Print() << "  distance_scale = " << thermal_distance_scale << " m\n";
            amrex::Print() << "  coastline = (" << thermal_coastline_x << ", " << thermal_coastline_y << ") m\n";
            amrex::Print() << "  coast_normal = (" << thermal_coast_normal_x << ", " << thermal_coast_normal_y << ")\n";
        }
        
        if (enable_terrain_blocking) {
            amrex::Print() << "wind_solver: Froude number terrain blocking enabled\n";
            amrex::Print() << "  brunt_vaisala_frequency = " << terrain_blocking_brunt_vaisala_frequency << " 1/s\n";
            amrex::Print() << "  reduction_factor = " << terrain_blocking_reduction_factor << "\n";
            amrex::Print() << "  transition_froude = " << terrain_blocking_transition_froude << "\n";
            amrex::Print() << "  flank_enhancement = " << terrain_blocking_flank_enhancement << "\n";
            amrex::Print() << "  reference_temperature = " << terrain_blocking_reference_temperature << " K\n";
        }
        
        if (enable_slope_flows) {
            amrex::Print() << "wind_solver: katabatic/anabatic slope flows enabled\n";
            amrex::Print() << "  temperature_diff = " << slope_flow_temperature_diff << " K\n";
            amrex::Print() << "  reference_temperature = " << slope_flow_reference_temperature << " K\n";
            amrex::Print() << "  empirical_coefficient = " << slope_flow_empirical_coefficient << " m/s\n";
            amrex::Print() << "  vertical_decay_height = " << slope_flow_vertical_decay_height << " m\n";
            amrex::Print() << "  min_slope = " << slope_flow_min_slope << "\n";
        }

        if (enable_gap_flow) {
            amrex::Print() << "wind_solver: gap flow parameterization enabled\n";
            amrex::Print() << "  gap_orientation = " << gap_flow_orientation << " degrees\n";
            amrex::Print() << "  gap_width = " << gap_flow_width << " m\n";
            amrex::Print() << "  gap_depth = " << gap_flow_depth << " m\n";
            amrex::Print() << "  pressure_coefficient = " << gap_flow_pressure_coefficient << "\n";
            amrex::Print() << "  speedup_max = " << gap_flow_speedup_max << "\n";
            amrex::Print() << "  gap_center = (" << gap_flow_center_x << ", " << gap_flow_center_y << ") m\n";
            amrex::Print() << "  transition_width = " << gap_flow_transition_width << " m\n";
            amrex::Print() << "  vertical_extent = " << gap_flow_vertical_extent << " m\n";
        }

        const int init_mode_val = (init_mode == "deaves_harris") ? 1 : ((init_mode == "powerlaw_above_bl") ? 2 : 0);
        const Real zg_val = bl_depth_param;
        const Real powerlaw_exp_val = powerlaw_exponent;

        const Real ustar_cap = ustar;
        const Real kappa_cap = kappa;
        const Real z_ref_cap = z_ref;
        const Real ux_h      = ux_hat;
        const Real uy_h      = uy_hat;
        const Real U_ref_cap = U_ref;
        const Real V_ref_cap = V_ref;
        const bool use_pos_z0 = use_z0_file;
        
        const bool use_stability = enable_stability_correction;
        const Real L_obukhov = stability_length;
        const bool use_holtslag = use_holtslag_stability;
        
        const bool use_elev_scaling = enable_elevation_scaling;
        const Real elev_scale_factor = elevation_scaling_factor;
        const Real elev_height_scale = elevation_height_scale;
        const Real terrain_min = zs_min;
        
        const bool use_veg_roughness = enable_vegetation_roughness;
        const Real veg_state_val = vegetation_state;
        const int veg_state_type_val = vegetation_state_type;
         
        const bool use_wall_func = enable_wall_functions;
        const bool use_terrain_wall = enable_terrain_wall_function;
        const Real wf_blend_height = wall_function_blend_height;
        const Real speed_ref_cap = speed_ref;
        
        const bool wf_enable_stability = wall_function_enable_stability;
        const Real wf_stability_length = wall_function_stability_length;
        const bool wf_enable_adaptive = wall_function_enable_adaptive;
        const Real wf_adaptive_threshold = wall_function_adaptive_threshold;

        const bool use_buoyancy = enable_buoyancy_stratification;
        const Real T_ref = temperature_reference;
        const Real buoy_coeff = buoyancy_coefficient;
        const Real buoy_dt = buoyancy_timescale;
        const int n_temp_pts = n_temp_points;
        const bool buoy_use_velocity = (buoyancy_method == "velocity");
        
        const bool use_kinematic_bc = enable_terrain_kinematic_bc;
        const Real bc_relax = terrain_bc_relaxation;
        
        const bool use_ekman = enable_ekman_veer;
        const Real veer_height = ekman_veer_height;
        const Real veer_total = ekman_veer_total_rad;
        
        const bool use_wind_dir_gradient = enable_wind_direction_gradient;
        const Real dir_shear_rate = wind_direction_shear_rate_rad;

        const bool cap_enable_coriolis_latitude = enable_coriolis_latitude;
        const Real cap_domain_latitude = domain_latitude;
        const Real cap_x_lo = x_lo;
        const Real cap_y_lo = y_lo;
        const Real cap_dy = dy;
        const Real cap_y_center = y_lo + Real(0.5) * (y_hi - y_lo);

        const bool enable_landuse_roughness_val = enable_landuse_roughness;
        const Real* d_landuse_pos_ptr_val = d_landuse_pos_ptr;
        const Real charnock_alpha_val = charnock_alpha;

        const bool enable_morph_val = enable_morphometric_models;
        const Real* d_morph_d_ptr = d_morphometric_d.data();
        const Real* d_morph_z0_ptr = d_morphometric_z0.data();

        const bool use_simplified_richardson_val = enable_simplified_richardson;
        const bool use_golder_curves_val = use_golder_curves;
        const int nz_val = nz;

        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);
            auto adap_rough = adaptive_roughness_ptr->array(mfi);
            const auto temp_arr = temp_ptr->const_array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real z_physical = z_lo_cap + (k + Real(0.5)) * dz_cap;
                Real terrain_elev = d_terr_ptr[j * nx_cap + i];
                Real z_agl      = z_physical - terrain_elev;

                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else if (use_wall_func && use_terrain_wall && z_agl <= wf_blend_height * dz_cap) {
                    Real z0_local = enable_morph_val ? d_morph_z0_ptr[j * nx_cap + i] : (use_pos_z0 ? d_z0_pos_ptr[j * nx_cap + i] : z0_cap);
                    
                    if (use_veg_roughness) {
                        Real veg_factor = vegetation_roughness_factor(veg_state_val, veg_state_type_val);
                        z0_local *= veg_factor;
                    }
                    
                    Real u_outer = vel(i, j, k, 0);
                    Real v_outer = vel(i, j, k, 1);
                    Real w_outer = vel(i, j, k, 2);
                    
                    Real ustar_local = ustar_cap;
                    if (use_pos_z0 && z0_local > Real(1.0e-10)) {
                        Real speed_ref_denom = std::log((z_ref_cap + z0_cap) / z0_cap);
                        Real speed_ref_local = (speed_ref_denom > Real(1.0e-10)) 
                            ? ustar_cap * speed_ref_denom / kappa_cap : Real(0.0);
                        Real log_term = std::log((z_ref_cap + z0_local) / z0_local);
                        ustar_local = (log_term > Real(1.0e-10)) 
                            ? kappa_cap * speed_ref_local / log_term : Real(0.0);
                    }

                    if (enable_landuse_roughness_val && d_landuse_pos_ptr_val) {
                        int lu_type = static_cast<int>(std::round(d_landuse_pos_ptr_val[j * nx_cap + i]));
                        if (lu_type == static_cast<int>(LandUseCategory::WATER)) {
                            Real z0_water = compute_charnock_roughness(charnock_alpha_val, ustar_local);
                            z0_local = z0_water;
                            if (use_pos_z0 && z0_local > Real(1.0e-10)) {
                                Real speed_ref_denom = std::log((z_ref_cap + z0_cap) / z0_cap);
                                Real speed_ref_local = (speed_ref_denom > Real(1.0e-10)) 
                                    ? ustar_cap * speed_ref_denom / kappa_cap : Real(0.0);
                                Real log_term = std::log((z_ref_cap + z0_local) / z0_local);
                                ustar_local = (log_term > Real(1.0e-10)) 
                                    ? kappa_cap * speed_ref_local / log_term : Real(0.0);
                            }
                        }
                    }

                    adap_rough(i, j, k) = z0_local;
                    
                    if (use_elev_scaling && elev_height_scale > Real(1.0e-10)) {
                        Real scale = elevation_wind_scaling(Real(1.0), terrain_elev, 
                                                           terrain_min, elev_scale_factor, 
                                                           elev_height_scale);
                        ustar_local *= scale;
                    }
                    
                    CanopyParams cell_canopy_params = canopy_params;
                    if (d_canopy_height_ptr) {
                        cell_canopy_params.height = d_canopy_height_ptr[j * nx_cap + i];
                    }
                    if (d_frontal_area_index_ptr) {
                        cell_canopy_params.frontal_area_index = d_frontal_area_index_ptr[j * nx_cap + i];
                    }
                    Real speed;
                    if (enable_morph_val) {
                        Real d_local = d_morph_d_ptr[j * nx_cap + i];
                        speed = log_law_with_displacement(z_agl, d_local, z0_local, ustar_local, kappa_cap);
                    } else if (init_mode_val == 1) { // Deaves-Harris
                        Real ratio = z_agl / zg_val;
                        ratio = (ratio > Real(1.0)) ? Real(1.0) : ((ratio < Real(0.0)) ? Real(0.0) : ratio);
                        Real term = std::log((z_agl + z0_local) / z0_local) + Real(5.75) * ratio - Real(1.88) * ratio * ratio - Real(1.33) * std::pow(ratio, 3) + Real(0.25) * std::pow(ratio, 4);
                        speed = (ustar_local / kappa_cap) * term;
                    } else if (init_mode_val == 2) { // Power-Law above BL
                        if (z_agl <= zg_val) {
                            speed = (ustar_local / kappa_cap) * std::log((z_agl + z0_local) / z0_local);
                        } else {
                            Real speed_bl = (ustar_local / kappa_cap) * std::log((zg_val + z0_local) / z0_local);
                            speed = speed_bl * std::pow(z_agl / zg_val, powerlaw_exp_val);
                        }
                    } else if (use_stability || use_simplified_richardson_val) {
                        Real L_local = L_obukhov;
                        if (use_simplified_richardson_val) {
                            int k_start = 0;
                            while (k_start < nz_val && (z_lo_cap + (Real(k_start) + Real(0.5)) * dz_cap - d_terr_ptr[j * nx_cap + i] <= Real(0.0))) {
                                k_start++;
                            }
                            if (k_start < nz_val && k >= k_start) {
                                Real theta_s = temp_arr(i, j, k_start);
                                Real theta_z = temp_arr(i, j, k);
                                Real speed_neutral = (ustar_local / kappa_cap) * std::log((z_agl + z0_local) / z0_local);
                                Real ri_b = compute_bulk_richardson_number(theta_s, theta_z, z_agl, speed_neutral, theta_s);
                                L_local = compute_obukhov_length_from_richardson(ri_b, z0_local, use_golder_curves_val);
                            }
                        }
                        if (std::abs(L_local) > Real(1.0e-10)) {
                            speed = wind_profile_stability(z_agl, z0_local, ustar_local, 
                                                          kappa_cap, L_local, use_holtslag);
                        } else {
                            speed = canopy_wind_profile(
                                z_agl, cell_canopy_params, z0_local, ustar_local, kappa_cap);
                        }
                    } else {
                        speed = canopy_wind_profile(
                            z_agl, cell_canopy_params, z0_local, ustar_local, kappa_cap);
                    }
                    
                    u_outer = speed * ux_h;
                    v_outer = speed * uy_h;
                    w_outer = Real(0.0);
                    
                    Real u_wf = vel(i, j, k, 0);
                    Real v_wf = vel(i, j, k, 1);
                    Real w_wf = vel(i, j, k, 2);
                    apply_flat_surface_wall_function_blended(
                        u_wf, v_wf, w_wf,
                        u_outer, v_outer, w_outer,
                        z_agl, z0_local, speed_ref_cap, z_ref_cap,
                        dz_cap, wf_blend_height, kappa_cap,
                        wf_enable_stability, wf_stability_length,
                        wf_enable_adaptive, wf_adaptive_threshold);
                    
                    vel(i, j, k, 0) = u_wf;
                    vel(i, j, k, 1) = v_wf;
                    vel(i, j, k, 2) = w_wf;
                } else {
                    Real z0_local = enable_morph_val ? d_morph_z0_ptr[j * nx_cap + i] : (use_pos_z0 ? d_z0_pos_ptr[j * nx_cap + i] : z0_cap);
                    
                    if (use_veg_roughness) {
                        Real veg_factor = vegetation_roughness_factor(veg_state_val, veg_state_type_val);
                        z0_local *= veg_factor;
                    }
                    
                    Real ustar_local = ustar_cap;
                    if (use_pos_z0 && z0_local > Real(1.0e-10)) {
                        Real speed_ref_denom = std::log((z_ref_cap + z0_cap) / z0_cap);
                        Real speed_ref_local = (speed_ref_denom > Real(1.0e-10)) 
                            ? ustar_cap * speed_ref_denom / kappa_cap : Real(0.0);
                        Real log_term = std::log((z_ref_cap + z0_local) / z0_local);
                        ustar_local = (log_term > Real(1.0e-10)) 
                            ? kappa_cap * speed_ref_local / log_term : Real(0.0);
                    }

                    if (enable_landuse_roughness_val && d_landuse_pos_ptr_val) {
                        int lu_type = static_cast<int>(std::round(d_landuse_pos_ptr_val[j * nx_cap + i]));
                        if (lu_type == static_cast<int>(LandUseCategory::WATER)) {
                            Real z0_water = compute_charnock_roughness(charnock_alpha_val, ustar_local);
                            z0_local = z0_water;
                            if (use_pos_z0 && z0_local > Real(1.0e-10)) {
                                Real speed_ref_denom = std::log((z_ref_cap + z0_cap) / z0_cap);
                                Real speed_ref_local = (speed_ref_denom > Real(1.0e-10)) 
                                    ? ustar_cap * speed_ref_denom / kappa_cap : Real(0.0);
                                Real log_term = std::log((z_ref_cap + z0_local) / z0_local);
                                ustar_local = (log_term > Real(1.0e-10)) 
                                    ? kappa_cap * speed_ref_local / log_term : Real(0.0);
                            }
                        }
                    }

                    adap_rough(i, j, k) = z0_local;
                    
                    if (use_elev_scaling && elev_height_scale > Real(1.0e-10)) {
                        Real scale = elevation_wind_scaling(Real(1.0), terrain_elev, 
                                                           terrain_min, elev_scale_factor, 
                                                           elev_height_scale);
                        ustar_local *= scale;
                    }
                    
                    CanopyParams cell_canopy_params = canopy_params;
                    if (d_canopy_height_ptr) {
                        cell_canopy_params.height = d_canopy_height_ptr[j * nx_cap + i];
                    }
                    if (d_frontal_area_index_ptr) {
                        cell_canopy_params.frontal_area_index = d_frontal_area_index_ptr[j * nx_cap + i];
                    }
                    Real speed;
                    if (enable_morph_val) {
                        Real d_local = d_morph_d_ptr[j * nx_cap + i];
                        speed = log_law_with_displacement(z_agl, d_local, z0_local, ustar_local, kappa_cap);
                    } else if (init_mode_val == 1) { // Deaves-Harris
                        Real ratio = z_agl / zg_val;
                        ratio = (ratio > Real(1.0)) ? Real(1.0) : ((ratio < Real(0.0)) ? Real(0.0) : ratio);
                        Real term = std::log((z_agl + z0_local) / z0_local) + Real(5.75) * ratio - Real(1.88) * ratio * ratio - Real(1.33) * std::pow(ratio, 3) + Real(0.25) * std::pow(ratio, 4);
                        speed = (ustar_local / kappa_cap) * term;
                    } else if (init_mode_val == 2) { // Power-Law above BL
                        if (z_agl <= zg_val) {
                            speed = (ustar_local / kappa_cap) * std::log((z_agl + z0_local) / z0_local);
                        } else {
                            Real speed_bl = (ustar_local / kappa_cap) * std::log((zg_val + z0_local) / z0_local);
                            speed = speed_bl * std::pow(z_agl / zg_val, powerlaw_exp_val);
                        }
                    } else if (use_stability || use_simplified_richardson_val) {
                        Real L_local = L_obukhov;
                        if (use_simplified_richardson_val) {
                            int k_start = 0;
                            while (k_start < nz_val && (z_lo_cap + (Real(k_start) + Real(0.5)) * dz_cap - d_terr_ptr[j * nx_cap + i] <= Real(0.0))) {
                                k_start++;
                            }
                            if (k_start < nz_val && k >= k_start) {
                                Real theta_s = temp_arr(i, j, k_start);
                                Real theta_z = temp_arr(i, j, k);
                                Real speed_neutral = (ustar_local / kappa_cap) * std::log((z_agl + z0_local) / z0_local);
                                Real ri_b = compute_bulk_richardson_number(theta_s, theta_z, z_agl, speed_neutral, theta_s);
                                L_local = compute_obukhov_length_from_richardson(ri_b, z0_local, use_golder_curves_val);
                            }
                        }
                        if (std::abs(L_local) > Real(1.0e-10)) {
                            speed = wind_profile_stability(z_agl, z0_local, ustar_local, 
                                                          kappa_cap, L_local, use_holtslag);
                        } else {
                            speed = canopy_wind_profile(
                                z_agl, cell_canopy_params, z0_local, ustar_local, kappa_cap);
                        }
                    } else {
                        speed = canopy_wind_profile(
                            z_agl, cell_canopy_params, z0_local, ustar_local, kappa_cap);
                    }
                    
                    Real u_vel, v_vel;
                    if (use_ekman) {
                        Real local_veer_height = veer_height;
                        if (cap_enable_coriolis_latitude) {
                            Real y_coord = cap_y_lo + (j + Real(0.5)) * cap_dy;
                            Real f_ref = compute_latitude_coriolis_parameter(cap_domain_latitude);
                            Real f_loc = compute_latitude_dependent_coriolis(y_coord, cap_y_center, cap_domain_latitude);
                            if (std::abs(f_loc) > Real(1.0e-8) && std::abs(f_ref) > Real(1.0e-8)) {
                                local_veer_height *= std::sqrt(std::abs(f_ref) / std::abs(f_loc));
                            }
                        }
                        Real veer_angle = ekman_veer_angle(z_agl, local_veer_height, veer_total);
                        
                        Real u_base = speed * ux_h;
                        Real v_base = speed * uy_h;
                        apply_ekman_veer(u_base, v_base, veer_angle, u_vel, v_vel);
                    } else if (use_wind_dir_gradient) {
                        Real dir_angle = wind_direction_gradient_angle(z_agl, dir_shear_rate);
                        
                        Real u_base = speed * ux_h;
                        Real v_base = speed * uy_h;
                        apply_ekman_veer(u_base, v_base, dir_angle, u_vel, v_vel);
                    } else {
                        u_vel = speed * ux_h;
                        v_vel = speed * uy_h;
                    }
                    Real w_vel = Real(0.0);
                    
                    if (use_buoyancy && n_temp_pts > 0) {
                        Real T_local = T_ref;
                        
                        if (n_temp_pts == 1) {
                            T_local = d_temp_T_ptr[0];
                        } else if (z_physical <= d_temp_z_ptr[0]) {
                            T_local = d_temp_T_ptr[0];
                        } else if (z_physical >= d_temp_z_ptr[n_temp_pts - 1]) {
                            T_local = d_temp_T_ptr[n_temp_pts - 1];
                        } else {
                            for (int m = 0; m < n_temp_pts - 1; ++m) {
                                if (z_physical >= d_temp_z_ptr[m] && 
                                    z_physical <= d_temp_z_ptr[m + 1]) {
                                    T_local = temperature_linear_interp(
                                        z_physical,
                                        d_temp_z_ptr[m], d_temp_T_ptr[m],
                                        d_temp_z_ptr[m + 1], d_temp_T_ptr[m + 1]);
                                    break;
                                }
                            }
                        }
                        
                        if (buoy_use_velocity) {
                            w_vel += buoyancy_velocity(T_local, T_ref, buoy_dt, buoy_coeff);
                        }
                    }
                    
                    if (use_kinematic_bc && k > 0) {
                        Real z_physical_below = z_lo_cap + (k - Real(0.5)) * dz_cap;
                        Real z_agl_below = z_physical_below - terrain_elev;
                        
                        if (z_agl_below <= Real(0.0)) {
                            std::size_t idx_2d = static_cast<std::size_t>(j) * nx_cap + i;
                            Real dh_dx = d_terr_grad_x_ptr[idx_2d];
                            Real dh_dy = d_terr_grad_y_ptr[idx_2d];
                            w_vel = terrain_kinematic_w(u_vel, v_vel, dh_dx, dh_dy, bc_relax);
                        }
                    }
                    
                    if (orog_params.enabled) {
                        int im = std::max(i - 1, 0);
                        int ip = std::min(i + 1, nx_cap - 1);
                        int jm = std::max(j - 1, 0);
                        int jp = std::min(j + 1, ny_cap - 1);
                        
                        Real z_xm = d_terr_ptr[j * nx_cap + im];
                        Real z_xp = d_terr_ptr[j * nx_cap + ip];
                        Real z_ym = d_terr_ptr[jm * nx_cap + i];
                        Real z_yp = d_terr_ptr[jp * nx_cap + i];
                        
                        Real slope = compute_terrain_slope(
                            z_xm, z_xp, z_ym, z_yp, 
                            dx_cap, dy_cap);
                        
                        Real curvature = compute_terrain_curvature(
                            terrain_elev, z_xm, z_xp, z_ym, z_yp,
                            dx_cap, dy_cap);
                        
                        Real speedup_factor = jackson_hunt_speedup_factor(
                            slope, curvature, z_agl, orog_params);
                        
                        apply_orographic_speedup(u_vel, v_vel, speedup_factor);
                    }
                    
                    if (thermal_params.enabled) {
                        Real x_cell = cap_x_lo + (i + Real(0.5)) * dx_cap;
                        Real y_cell = cap_y_lo + (j + Real(0.5)) * dy_cap;
                        
                        Real land_sea_mask = Real(1.0);
                        Real dist_from_coast = compute_distance_from_coast(
                            land_sea_mask, x_cell, y_cell, coastline_x, coastline_y);
                        
                        apply_thermal_circulation(
                            u_vel, v_vel, dist_from_coast, z_agl,
                            coast_normal_x, coast_normal_y, thermal_params);
                    }
                    
                    if (blocking_params.enabled) {
                        int im = std::max(i - 1, 0);
                        int ip = std::min(i + 1, nx_cap - 1);
                        int jm = std::max(j - 1, 0);
                        int jp = std::min(j + 1, ny_cap - 1);
                        
                        Real z_xm = d_terr_ptr[j * nx_cap + im];
                        Real z_xp = d_terr_ptr[j * nx_cap + ip];
                        Real z_ym = d_terr_ptr[jm * nx_cap + i];
                        Real z_yp = d_terr_ptr[jp * nx_cap + i];
                        
                        Real slope_x = (z_xp - z_xm) / (Real(2.0) * dx_cap);
                        Real slope_y = (z_yp - z_ym) / (Real(2.0) * dy_cap);
                        
                        Real curvature = compute_terrain_curvature(
                            terrain_elev, z_xm, z_xp, z_ym, z_yp,
                            dx_cap, dy_cap);
                        
                        Real wind_speed = std::sqrt(u_vel * u_vel + v_vel * v_vel);
                        Real obstacle_height = terrain_elev - terrain_min;
                        
                        apply_terrain_blocking(
                            u_vel, v_vel, wind_speed, obstacle_height,
                            slope_x, slope_y, curvature, blocking_params);
                    }
                    
                    if (slope_flow_params.enabled) {
                        int im = std::max(i - 1, 0);
                        int ip = std::min(i + 1, nx_cap - 1);
                        int jm = std::max(j - 1, 0);
                        int jp = std::min(j + 1, ny_cap - 1);
                        
                        Real z_xm = d_terr_ptr[j * nx_cap + im];
                        Real z_xp = d_terr_ptr[j * nx_cap + ip];
                        Real z_ym = d_terr_ptr[jm * nx_cap + i];
                        Real z_yp = d_terr_ptr[jp * nx_cap + i];
                        
                        apply_slope_flow(
                            u_vel, v_vel, z_xm, z_xp, z_ym, z_yp,
                            dx_cap, dy_cap, z_agl, slope_flow_params);
                    }

                    if (gap_params.enabled) {
                        Real x_cell = cap_x_lo + (i + Real(0.5)) * dx_cap;
                        Real y_cell = cap_y_lo + (j + Real(0.5)) * dy_cap;
                        
                        Real U_synoptic = U_ref_cap;
                        Real V_synoptic = V_ref_cap;
                        
                        Real u_gap, v_gap;
                        apply_gap_flow(
                            x_cell, y_cell, z_agl,
                            u_vel, v_vel,
                            U_synoptic, V_synoptic,
                            gap_params,
                            u_gap, v_gap);
                        
                        u_vel = u_gap;
                        v_vel = v_gap;
                    }
                    
                    if (valley_params.enabled) {
                        int im = std::max(i - 1, 0);
                        int ip = std::min(i + 1, nx_cap - 1);
                        int jm = std::max(j - 1, 0);
                        int jp = std::min(j + 1, ny_cap - 1);
                        
                        Real z_center = terrain_elev;
                        Real z_xm = d_terr_ptr[j * nx_cap + im];
                        Real z_xp = d_terr_ptr[j * nx_cap + ip];
                        Real z_ym = d_terr_ptr[jm * nx_cap + i];
                        Real z_yp = d_terr_ptr[jp * nx_cap + i];
                        
                        compute_valley_channeling(
                            u_vel, v_vel,
                            z_center, z_xm, z_xp, z_ym, z_yp,
                            dx_cap, dy_cap, valley_params);
                    }
                    
                    vel(i, j, k, 0) = u_vel;
                    vel(i, j, k, 1) = v_vel;
                    vel(i, j, k, 2) = w_vel;
                }
            });
        }
    } else if (init_mode == "ekman_spiral") {
        const Real lat = ekman_latitude;
        const Real Ug = ekman_ug;
        const Real Vg = ekman_vg;
        const Real Km = ekman_Km;
        const Real pi_val = MathConstants::pi;
        const Real omega = 7.27e-5;
        const Real f_coriolis = 2.0 * omega * std::sin(lat * pi_val / 180.0);
        const Real abs_f = std::abs(f_coriolis);
        const Real a_ekman = (abs_f > 1.0e-8) ? std::sqrt(abs_f / (2.0 * Km)) : 0.0;

        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real z_physical = z_lo_cap + (k + Real(0.5)) * dz_cap;
                Real z_agl      = z_physical - d_terr_ptr[j * nx_cap + i];

                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else {
                    if (abs_f > Real(1.0e-8)) {
                        Real exp_term = std::exp(-a_ekman * z_agl);
                        Real cos_term = std::cos(a_ekman * z_agl);
                        Real sin_term = std::sin(a_ekman * z_agl);
                        if (f_coriolis >= Real(0.0)) {
                            vel(i, j, k, 0) = Ug * (Real(1.0) - exp_term * cos_term) - Vg * exp_term * sin_term;
                            vel(i, j, k, 1) = Vg * (Real(1.0) - exp_term * cos_term) + Ug * exp_term * sin_term;
                        } else {
                            vel(i, j, k, 0) = Ug * (Real(1.0) - exp_term * cos_term) + Vg * exp_term * sin_term;
                            vel(i, j, k, 1) = Vg * (Real(1.0) - exp_term * cos_term) - Ug * exp_term * sin_term;
                        }
                    } else {
                        vel(i, j, k, 0) = Ug;
                        vel(i, j, k, 1) = Vg;
                    }
                    vel(i, j, k, 2) = Real(0.0);
                }
            });
        }
    } else if (init_mode == "uniform") {
        const Real u_uniform = uniform_U;
        const Real v_uniform = uniform_V;
        const bool use_wall_func = enable_wall_functions;
        const bool use_terrain_wall = enable_terrain_wall_function;
        const Real wf_blend_height = wall_function_blend_height;
        const Real z0_local_cap = z0;
        const Real z_ref_cap = z_ref;
        const Real kappa_cap = 0.41;
        const Real speed_ref_cap = std::sqrt(u_uniform * u_uniform + v_uniform * v_uniform);
        
        const bool wf_enable_stability = wall_function_enable_stability;
        const Real wf_stability_length = wall_function_stability_length;
        const bool wf_enable_adaptive = wall_function_enable_adaptive;
        const Real wf_adaptive_threshold = wall_function_adaptive_threshold;

        const bool use_buoyancy = enable_buoyancy_stratification;
        const Real T_ref = temperature_reference;
        const Real buoy_coeff = buoyancy_coefficient;
        const Real buoy_dt = buoyancy_timescale;
        const int n_temp_pts = n_temp_points;
        const bool buoy_use_velocity = (buoyancy_method == "velocity");
        
        const bool use_kinematic_bc = enable_terrain_kinematic_bc;
        const Real bc_relax = terrain_bc_relaxation;

        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real z_physical = z_lo_cap + (k + Real(0.5)) * dz_cap;
                Real z_agl      = z_physical - d_terr_ptr[j * nx_cap + i];

                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else if (use_wall_func && use_terrain_wall && z_agl <= wf_blend_height * dz_cap) {
                    Real u_wf = vel(i, j, k, 0);
                    Real v_wf = vel(i, j, k, 1);
                    Real w_wf = vel(i, j, k, 2);
                    apply_flat_surface_wall_function_blended(
                        u_wf, v_wf, w_wf,
                        u_uniform, v_uniform, Real(0.0),
                        z_agl, z0_local_cap, speed_ref_cap, z_ref_cap,
                        dz_cap, wf_blend_height, kappa_cap,
                        wf_enable_stability, wf_stability_length,
                        wf_enable_adaptive, wf_adaptive_threshold);
                    
                    vel(i, j, k, 0) = u_wf;
                    vel(i, j, k, 1) = v_wf;
                    vel(i, j, k, 2) = w_wf;
                } else {
                    Real u_vel = u_uniform;
                    Real v_vel = v_uniform;
                    Real w_vel = Real(0.0);
                    
                    if (use_buoyancy && n_temp_pts > 0) {
                        Real T_local = T_ref;
                        if (n_temp_pts == 1) {
                            T_local = d_temp_T_ptr[0];
                        } else if (z_physical <= d_temp_z_ptr[0]) {
                            T_local = d_temp_T_ptr[0];
                        } else if (z_physical >= d_temp_z_ptr[n_temp_pts - 1]) {
                            T_local = d_temp_T_ptr[n_temp_pts - 1];
                        } else {
                            for (int m = 0; m < n_temp_pts - 1; ++m) {
                                if (z_physical >= d_temp_z_ptr[m] && 
                                    z_physical <= d_temp_z_ptr[m + 1]) {
                                    T_local = temperature_linear_interp(
                                        z_physical,
                                        d_temp_z_ptr[m], d_temp_T_ptr[m],
                                        d_temp_z_ptr[m + 1], d_temp_T_ptr[m + 1]);
                                    break;
                                }
                            }
                        }
                        if (buoy_use_velocity) {
                            w_vel += buoyancy_velocity(T_local, T_ref, buoy_dt, buoy_coeff);
                        }
                    }
                    
                    if (use_kinematic_bc && k > 0) {
                        Real z_physical_below = z_lo_cap + (k - Real(0.5)) * dz_cap;
                        Real z_agl_below = z_physical_below - d_terr_ptr[j * nx_cap + i];
                        if (z_agl_below <= Real(0.0)) {
                            std::size_t idx_2d = static_cast<std::size_t>(j) * nx_cap + i;
                            Real dh_dx = d_terr_grad_x_ptr[idx_2d];
                            Real dh_dy = d_terr_grad_y_ptr[idx_2d];
                            w_vel = terrain_kinematic_w(u_vel, v_vel, dh_dx, dh_dy, bc_relax);
                        }
                    }
                    
                    vel(i, j, k, 0) = u_vel;
                    vel(i, j, k, 1) = v_vel;
                    vel(i, j, k, 2) = w_vel;
                }
            });
        }
    } else if (init_mode == "raws") {
        std::vector<Real> x_vel, y_vel, z_vel, ux_vel, uy_vel;
        if (velocity_file.size() > 4 && velocity_file.substr(velocity_file.find_last_of(".") + 1) == "csv") {
            WindIO::read_vertical_profile_csv(velocity_file, x_vel, y_vel, z_vel, ux_vel, uy_vel);
        } else {
            WindIO::read_velocity_file(velocity_file, x_vel, y_vel, z_vel, ux_vel, uy_vel);
        }

        std::vector<Real> vel_u_h(static_cast<std::size_t>(nx) * ny * nz);
        std::vector<Real> vel_v_h(static_cast<std::size_t>(nx) * ny * nz);

        for (int k = 0; k < nz; ++k) {
            Real zc = z_lo_cap + (k + Real(0.5)) * dz_cap;
            Real rmax = (k == 0) ? idw_rmax1 : idw_rmax2;
            Real R_param = (k == 0) ? idw_r1 : idw_r2;
            for (int j = 0; j < ny; ++j) {
                Real yc = y_lo + (j + Real(0.5)) * dy;
                for (int i = 0; i < nx; ++i) {
                    Real xc = x_lo + (i + Real(0.5)) * dx;
                    Real d_min = std::numeric_limits<Real>::max();
                    bool any_station_within_rmax = false;
                    for (std::size_t s = 0; s < x_vel.size(); ++s) {
                        Real dx_s = x_vel[s] - xc;
                        Real dy_s = y_vel[s] - yc;
                        Real dist = std::sqrt(dx_s * dx_s + dy_s * dy_s);
                        if (rmax <= Real(0.0) || dist <= rmax) {
                            any_station_within_rmax = true;
                            if (dist < d_min) {
                                d_min = dist;
                            }
                        }
                    }

                    auto [ux_interp, uy_interp] = WindInterpolation::idw_velocity_3d(
                        xc, yc, zc, x_vel, y_vel, z_vel, ux_vel, uy_vel, 6,
                        idw_gamma, enable_topographic_shielding, terrain_h, x_lo, y_lo, dx, dy, nx, ny,
                        rmax, idw_exponent);
                    
                    Real u_final = ux_interp;
                    Real v_final = uy_interp;

                    if (R_param > Real(0.0)) {
                        Real speed_ref = std::sqrt(U_ref * U_ref + V_ref * V_ref);
                        Real u_bg = 0.0, v_bg = 0.0;
                        if (speed_ref > Real(1.0e-10)) {
                            Real z_agl = zc - terrain_h[j * nx + i];
                            if (z_agl > Real(0.0)) {
                                Real ustar_bg = speed_ref * Real(0.41) / std::log((z_ref + z0) / z0);
                                Real speed_bg = (ustar_bg / Real(0.41)) * std::log((z_agl + z0) / z0);
                                u_bg = speed_bg * U_ref / speed_ref;
                                v_bg = speed_bg * V_ref / speed_ref;
                            }
                        }

                        if (!any_station_within_rmax) {
                            u_final = u_bg;
                            v_final = v_bg;
                        } else {
                            Real weight_bg = (d_min / R_param) * (d_min / R_param);
                            u_final = (ux_interp + weight_bg * u_bg) / (Real(1.0) + weight_bg);
                            v_final = (uy_interp + weight_bg * v_bg) / (Real(1.0) + weight_bg);
                        }
                    }
                    std::size_t idx = (static_cast<std::size_t>(k) * ny + j) * nx + i;
                    vel_u_h[idx] = u_final;
                    vel_v_h[idx] = v_final;
                }
            }
        }

        d_vel_u.resize(vel_u_h.size());
        d_vel_v.resize(vel_v_h.size());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, vel_u_h.begin(), vel_u_h.end(), d_vel_u.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, vel_v_h.begin(), vel_v_h.end(), d_vel_v.begin());
        d_vel_u_ptr = d_vel_u.data();
        d_vel_v_ptr = d_vel_v.data();

        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real z_physical = z_lo_cap + (k + Real(0.5)) * dz_cap;
                Real z_agl      = z_physical - d_terr_ptr[j * nx_cap + i];

                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else {
                    std::size_t idx = (static_cast<std::size_t>(k) * ny_cap + j) * nx_cap + i;
                    vel(i, j, k, 0) = d_vel_u_ptr[idx];
                    vel(i, j, k, 1) = d_vel_v_ptr[idx];
                    vel(i, j, k, 2) = Real(0.0);
                }
            });
        }
    } else if (init_mode == "sounding") {
        if (sounding_files.empty()) {
            amrex::Abort("wind_solver: init_mode is sounding but sounding_files is empty!");
        }
        if (sounding_x.size() != sounding_files.size() || sounding_y.size() != sounding_files.size()) {
            amrex::Abort("wind_solver: sounding_x and sounding_y must have the same size as sounding_files!");
        }

        struct SoundingStation {
            Real x;
            Real y;
            std::vector<Real> z;
            std::vector<Real> u;
            std::vector<Real> v;
            WindInterpolation::CubicSpline1D spline_u;
            WindInterpolation::CubicSpline1D spline_v;
        };

        std::vector<SoundingStation> stations(sounding_files.size());
        for (std::size_t s = 0; s < sounding_files.size(); ++s) {
            stations[s].x = sounding_x[s];
            stations[s].y = sounding_y[s];
            WindIO::read_sounding_file(sounding_files[s], stations[s].z, stations[s].u, stations[s].v, sounding_wind_in_knots);
            if (sounding_vertical_interp == "spline") {
                stations[s].spline_u = WindInterpolation::CubicSpline1D(stations[s].z, stations[s].u);
                stations[s].spline_v = WindInterpolation::CubicSpline1D(stations[s].z, stations[s].v);
            }
        }

        std::vector<Real> vel_u_h(static_cast<std::size_t>(nx) * ny * nz);
        std::vector<Real> vel_v_h(static_cast<std::size_t>(nx) * ny * nz);

        for (int k = 0; k < nz; ++k) {
            Real zc = z_lo_cap + (k + Real(0.5)) * dz_cap;
            Real rmax = (k == 0) ? idw_rmax1 : idw_rmax2;
            Real R_param = (k == 0) ? idw_r1 : idw_r2;
            for (int j = 0; j < ny; ++j) {
                Real yc = y_lo + (j + Real(0.5)) * dy;
                for (int i = 0; i < nx; ++i) {
                    Real xc = x_lo + (i + Real(0.5)) * dx;
                    Real d_min = std::numeric_limits<Real>::max();
                    bool any_station_within_rmax = false;
                    for (std::size_t s = 0; s < sounding_files.size(); ++s) {
                       Real dx_to_station = sounding_x[s] - xc;
                       Real dy_to_station = sounding_y[s] - yc;
                       Real dist = std::sqrt(dx_to_station * dx_to_station + dy_to_station * dy_to_station);
                       if (rmax <= Real(0.0) || dist <= rmax) {
                           any_station_within_rmax = true;
                           if (dist < d_min) {
                               d_min = dist;
                           }
                       }
                    }

                    // 1D Vertical interpolation for each sounding station
                    std::vector<Real> station_u(sounding_files.size());
                    std::vector<Real> station_v(sounding_files.size());
                    for (std::size_t s = 0; s < sounding_files.size(); ++s) {
                       if (sounding_vertical_interp == "spline") {
                           station_u[s] = stations[s].spline_u.evaluate(zc);
                           station_v[s] = stations[s].spline_v.evaluate(zc);
                       } else {
                           station_u[s] = WindInterpolation::log_linear_interpolate(zc, stations[s].z, stations[s].u);
                           station_v[s] = WindInterpolation::log_linear_interpolate(zc, stations[s].z, stations[s].v);
                       }
                    }

                    // 2D Horizontal IDW
                    auto [u_cell, v_cell] = WindInterpolation::idw_velocity(
                       xc, yc, sounding_x, sounding_y, station_u, station_v, 6, idw_exponent);

                    Real u_final = u_cell;
                    Real v_final = v_cell;

                    if (R_param > Real(0.0)) {
                       Real speed_ref = std::sqrt(U_ref * U_ref + V_ref * V_ref);
                       Real u_bg = 0.0, v_bg = 0.0;
                       if (speed_ref > Real(1.0e-10)) {
                           Real z_agl = zc - terrain_h[j * nx + i];
                           if (z_agl > Real(0.0)) {
                               Real ustar_bg = speed_ref * Real(0.41) / std::log((z_ref + z0) / z0);
                               Real speed_bg = (ustar_bg / Real(0.41)) * std::log((z_agl + z0) / z0);
                               u_bg = speed_bg * U_ref / speed_ref;
                               v_bg = speed_bg * V_ref / speed_ref;
                           }
                       }

                       if (!any_station_within_rmax) {
                           u_final = u_bg;
                           v_final = v_bg;
                       } else {
                           Real weight_bg = (d_min / R_param) * (d_min / R_param);
                           u_final = (u_cell + weight_bg * u_bg) / (Real(1.0) + weight_bg);
                           v_final = (v_cell + weight_bg * v_bg) / (Real(1.0) + weight_bg);
                       }
                    }
                    std::size_t idx = (static_cast<std::size_t>(k) * ny + j) * nx + i;
                    vel_u_h[idx] = u_final;
                    vel_v_h[idx] = v_final;
                }
            }
        }

        d_vel_u.resize(vel_u_h.size());
        d_vel_v.resize(vel_v_h.size());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, vel_u_h.begin(), vel_u_h.end(), d_vel_u.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, vel_v_h.begin(), vel_v_h.end(), d_vel_v.begin());
        d_vel_u_ptr = d_vel_u.data();
        d_vel_v_ptr = d_vel_v.data();

        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real z_physical = z_lo_cap + (k + Real(0.5)) * dz_cap;
                Real z_agl      = z_physical - d_terr_ptr[j * nx_cap + i];

                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else {
                    std::size_t idx = (static_cast<std::size_t>(k) * ny_cap + j) * nx_cap + i;
                    vel(i, j, k, 0) = d_vel_u_ptr[idx];
                    vel(i, j, k, 1) = d_vel_v_ptr[idx];
                    vel(i, j, k, 2) = Real(0.0);
                }
            });
        }
    } else if (init_mode == "surface_data") {
        std::vector<Real> x_surf, y_surf, z_surf, ustar_surf, z0_surf, u10_surf, v10_surf;
        WindIO::read_surface_data_file(surface_data_file, x_surf, y_surf, z_surf, 
                              ustar_surf, z0_surf, u10_surf, v10_surf);

        std::vector<Real> ustar_h(static_cast<std::size_t>(nx) * ny);
        std::vector<Real> z0_h(static_cast<std::size_t>(nx) * ny);
        std::vector<Real> u10_h(static_cast<std::size_t>(nx) * ny);
        std::vector<Real> v10_h(static_cast<std::size_t>(nx) * ny);

        for (int j = 0; j < ny; ++j) {
            Real yc = y_lo + (j + 0.5) * dy;
            for (int i = 0; i < nx; ++i) {
                Real xc = x_lo + (i + 0.5) * dx;
                auto [ustar_interp, z0_interp, u10_interp, v10_interp] = 
                    WindInterpolation::idw_surface_data(xc, yc, x_surf, y_surf, ustar_surf, z0_surf, u10_surf, v10_surf, 6, idw_exponent);
                ustar_h[static_cast<std::size_t>(j) * nx + i] = ustar_interp;
                z0_h[static_cast<std::size_t>(j) * nx + i] = z0_interp;
                u10_h[static_cast<std::size_t>(j) * nx + i] = u10_interp;
                v10_h[static_cast<std::size_t>(j) * nx + i] = v10_interp;
            }
        }

        Gpu::DeviceVector<Real> d_ustar(ustar_h.size());
        Gpu::DeviceVector<Real> d_z0(z0_h.size());
        Gpu::DeviceVector<Real> d_u10(u10_h.size());
        Gpu::DeviceVector<Real> d_v10(v10_h.size());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, ustar_h.begin(), ustar_h.end(), d_ustar.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, z0_h.begin(), z0_h.end(), d_z0.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, u10_h.begin(), u10_h.end(), d_u10.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, v10_h.begin(), v10_h.end(), d_v10.begin());

        const Real* d_ustar_ptr = d_ustar.data();
        const Real* d_z0_ptr = d_z0.data();
        const Real* d_u10_ptr = d_u10.data();
        const Real* d_v10_ptr = d_v10.data();
        const Real kappa_cap = Real(0.41);

        CanopyParams canopy_params;
        canopy_params.enabled = enable_canopy;
        canopy_params.height = canopy_height;
        canopy_params.frontal_area_index = frontal_area_index;
        canopy_params.plan_area_index = plan_area_index;
        canopy_params.drag_coefficient = canopy_drag_coeff;
        canopy_params.attenuation_coeff = canopy_attenuation;
        canopy_params.use_exponential_profile = use_exponential_profile;
        canopy_params.profile_type = canopy_profile_type;
        
        const bool use_ekman = enable_ekman_veer;
        const Real veer_height = ekman_veer_height;
        const Real veer_total = ekman_veer_total_rad;
        
        const bool use_wind_dir_gradient = enable_wind_direction_gradient;
        const Real dir_shear_rate = wind_direction_shear_rate_rad;

        const bool cap_enable_coriolis_latitude = enable_coriolis_latitude;
        const Real cap_domain_latitude = domain_latitude;
        const Real cap_y_lo = y_lo;
        const Real cap_dy = dy;
        const Real cap_y_center = y_lo + Real(0.5) * (y_hi - y_lo);

        const bool enable_morph_val = enable_morphometric_models;
        const Real* d_morph_d_ptr = d_morphometric_d.data();
        const Real* d_morph_z0_ptr = d_morphometric_z0.data();

        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real z_physical = z_lo_cap + (k + Real(0.5)) * dz_cap;
                Real z_agl      = z_physical - d_terr_ptr[j * nx_cap + i];

                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else {
                    std::size_t idx = static_cast<std::size_t>(j) * nx_cap + i;
                    Real ustar_col = d_ustar_ptr[idx];
                    Real z0_col = d_z0_ptr[idx];
                    Real u10_col = d_u10_ptr[idx];
                    Real v10_col = d_v10_ptr[idx];

                    Real speed_10m = std::sqrt(u10_col * u10_col + v10_col * v10_col);
                    Real ux_hat = (speed_10m > Real(1.0e-10)) ? u10_col / speed_10m : Real(1.0);
                    Real uy_hat = (speed_10m > Real(1.0e-10)) ? v10_col / speed_10m : Real(0.0);

                    CanopyParams cell_canopy_params = canopy_params;
                    if (d_canopy_height_ptr) {
                        cell_canopy_params.height = d_canopy_height_ptr[j * nx_cap + i];
                    }
                    if (d_frontal_area_index_ptr) {
                        cell_canopy_params.frontal_area_index = d_frontal_area_index_ptr[j * nx_cap + i];
                    }
                    Real speed;
                    if (enable_morph_val) {
                        Real d_local = d_morph_d_ptr[j * nx_cap + i];
                        Real z0_cell = d_morph_z0_ptr[j * nx_cap + i];
                        speed = log_law_with_displacement(z_agl, d_local, z0_cell, ustar_col, kappa_cap);
                    } else {
                        speed = canopy_wind_profile(
                            z_agl, cell_canopy_params, z0_col, ustar_col, kappa_cap);
                    }
                    
                    Real u_vel, v_vel;
                    if (use_ekman) {
                        Real local_veer_height = veer_height;
                        if (cap_enable_coriolis_latitude) {
                            Real y_coord = cap_y_lo + (j + Real(0.5)) * cap_dy;
                            Real f_ref = compute_latitude_coriolis_parameter(cap_domain_latitude);
                            Real f_loc = compute_latitude_dependent_coriolis(y_coord, cap_y_center, cap_domain_latitude);
                            if (std::abs(f_loc) > Real(1.0e-8) && std::abs(f_ref) > Real(1.0e-8)) {
                                local_veer_height *= std::sqrt(std::abs(f_ref) / std::abs(f_loc));
                            }
                        }
                        Real veer_angle = ekman_veer_angle(z_agl, local_veer_height, veer_total);
                        
                        Real u_base = speed * ux_hat;
                        Real v_base = speed * uy_hat;
                        apply_ekman_veer(u_base, v_base, veer_angle, u_vel, v_vel);
                    } else if (use_wind_dir_gradient) {
                        Real dir_angle = wind_direction_gradient_angle(z_agl, dir_shear_rate);
                        
                        Real u_base = speed * ux_hat;
                        Real v_base = speed * uy_hat;
                        apply_ekman_veer(u_base, v_base, dir_angle, u_vel, v_vel);
                    } else {
                        u_vel = speed * ux_hat;
                        v_vel = speed * uy_hat;
                    }
                    
                    vel(i, j, k, 0) = u_vel;
                    vel(i, j, k, 1) = v_vel;
                    vel(i, j, k, 2) = Real(0.0);
                }
            });
        }
    } else if (init_mode == "powerlaw") {
        Real speed_ref = std::sqrt(U_ref * U_ref + V_ref * V_ref);
        Real ux_hat = (speed_ref > Real(1.0e-10)) ? U_ref / speed_ref : Real(1.0);
        Real uy_hat = (speed_ref > Real(1.0e-10)) ? V_ref / speed_ref : Real(0.0);
        
        std::vector<Real> exponent_h(static_cast<std::size_t>(nx) * ny, powerlaw_exponent);
        Gpu::DeviceVector<Real> d_exponent_pos;
        const Real* d_exponent_pos_ptr = nullptr;
        
        if (use_landuse_powerlaw) {
            amrex::Print() << "wind_solver: reading land use classification from " << landuse_file << "\n";
            std::vector<Real> x_lu, y_lu, landuse_data;
            WindIO::read_roughness_file(landuse_file, x_lu, y_lu, landuse_data);
            
            for (int j = 0; j < ny; ++j) {
                for (int i = 0; i < nx; ++i) {
                    Real xc = x_lo + (i + Real(0.5)) * dx;
                    Real yc = y_lo + (j + Real(0.5)) * dy;
                    
                    Real landuse_interp = 0.0;
                    Real wsum = 0.0;
                    Real lu_sum = 0.0;
                    std::vector<std::pair<Real, int>> d2(x_lu.size());
                    for (std::size_t m = 0; m < x_lu.size(); ++m) {
                        Real dx_pt = xc - x_lu[m];
                        Real dy_pt = yc - y_lu[m];
                        d2[m] = {dx_pt * dx_pt + dy_pt * dy_pt, static_cast<int>(m)};
                    }
                    std::sort(d2.begin(), d2.end());
                    
                    const int n_pts = std::min(6, static_cast<int>(d2.size()));
                    for (int m = 0; m < n_pts; ++m) {
                        Real dist = std::sqrt(d2[m].first);
                        if (dist < Real(1.0e-12)) {
                            landuse_interp = landuse_data[d2[m].second];
                            wsum = 1.0;
                            break;
                        }
                        Real w = Real(1.0) / (dist * dist);
                        wsum += w;
                        lu_sum += w * landuse_data[d2[m].second];
                    }
                    if (wsum > Real(0.0)) {
                        landuse_interp = lu_sum / wsum;
                    }
                    
                    Real alpha_local = powerlaw_exponent;
                    int lu_type = static_cast<int>(std::round(landuse_interp));
                    if (lu_type <= 1) {
                        alpha_local = Real(0.10);
                    } else if (lu_type <= 3) {
                        alpha_local = Real(0.14);
                    } else {
                        alpha_local = Real(0.35);
                    }
                    
                    exponent_h[static_cast<std::size_t>(j) * nx + i] = alpha_local;
                }
            }
            
            d_exponent_pos.resize(exponent_h.size());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice, exponent_h.begin(), exponent_h.end(), d_exponent_pos.begin());
            d_exponent_pos_ptr = d_exponent_pos.data();
        }
        
        const Real exponent = powerlaw_exponent;
        const Real z_ref_cap = z_ref;
        const Real speed_ref_cap = speed_ref;
        const Real ux_h = ux_hat;
        const Real uy_h = uy_hat;
        const bool use_landuse_exp = use_landuse_powerlaw;

        amrex::Print() << "wind_solver: power-law profile initialization\n";
        amrex::Print() << "  U_ref = " << U_ref << " m/s, V_ref = " << V_ref << " m/s\n";
        amrex::Print() << "  z_ref = " << z_ref << " m\n";
        amrex::Print() << "  powerlaw_exponent = " << powerlaw_exponent << "\n";
        if (use_landuse_powerlaw) {
            amrex::Print() << "  using land use-based spatially-varying exponents\n";
        }

        const bool use_buoyancy = enable_buoyancy_stratification;
        const Real T_ref = temperature_reference;
        const Real buoy_coeff = buoyancy_coefficient;
        const Real buoy_dt = buoyancy_timescale;
        const int n_temp_pts = n_temp_points;
        const bool buoy_use_velocity = (buoyancy_method == "velocity");
        
        const bool use_kinematic_bc = enable_terrain_kinematic_bc;
        const Real bc_relax = terrain_bc_relaxation;
        
        const bool use_ekman = enable_ekman_veer;
        const Real veer_height = ekman_veer_height;
        const Real veer_total = ekman_veer_total_rad;
         
        const bool use_wind_dir_gradient = enable_wind_direction_gradient;
        const Real dir_shear_rate = wind_direction_shear_rate_rad;

        const bool cap_enable_coriolis_latitude = enable_coriolis_latitude;
        const Real cap_domain_latitude = domain_latitude;
        const Real cap_y_lo = y_lo;
        const Real cap_dy = dy;
        const Real cap_y_center = y_lo + Real(0.5) * (y_hi - y_lo);

        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real z_physical = z_lo_cap + (k + Real(0.5)) * dz_cap;
                Real z_agl      = z_physical - d_terr_ptr[j * nx_cap + i];

                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else {
                    Real exponent_local = exponent;
                    if (use_landuse_exp) {
                        exponent_local = d_exponent_pos_ptr[j * nx_cap + i];
                    }
                    
                    Real z_ratio = z_agl / z_ref_cap;
                    z_ratio = (z_ratio < Real(0.01)) ? Real(0.01) : z_ratio;
                    Real speed = speed_ref_cap * std::pow(z_ratio, exponent_local);
                    
                    Real u_vel, v_vel;
                    if (use_ekman) {
                        Real local_veer_height = veer_height;
                        if (cap_enable_coriolis_latitude) {
                            Real y_coord = cap_y_lo + (j + Real(0.5)) * cap_dy;
                            Real f_ref = compute_latitude_coriolis_parameter(cap_domain_latitude);
                            Real f_loc = compute_latitude_dependent_coriolis(y_coord, cap_y_center, cap_domain_latitude);
                            if (std::abs(f_loc) > Real(1.0e-8) && std::abs(f_ref) > Real(1.0e-8)) {
                                local_veer_height *= std::sqrt(std::abs(f_ref) / std::abs(f_loc));
                            }
                        }
                        Real veer_angle = ekman_veer_angle(z_agl, local_veer_height, veer_total);
                        
                        Real u_base = speed * ux_h;
                        Real v_base = speed * uy_h;
                        apply_ekman_veer(u_base, v_base, veer_angle, u_vel, v_vel);
                    } else if (use_wind_dir_gradient) {
                        Real dir_angle = wind_direction_gradient_angle(z_agl, dir_shear_rate);
                        
                        Real u_base = speed * ux_h;
                        Real v_base = speed * uy_h;
                        apply_ekman_veer(u_base, v_base, dir_angle, u_vel, v_vel);
                    } else {
                        u_vel = speed * ux_h;
                        v_vel = speed * uy_h;
                    }
                    Real w_vel = Real(0.0);
                    
                    if (use_buoyancy && n_temp_pts > 0) {
                        Real T_local = T_ref;
                        if (n_temp_pts == 1) {
                            T_local = d_temp_T_ptr[0];
                        } else if (z_physical <= d_temp_z_ptr[0]) {
                            T_local = d_temp_T_ptr[0];
                        } else if (z_physical >= d_temp_z_ptr[n_temp_pts - 1]) {
                            T_local = d_temp_T_ptr[n_temp_pts - 1];
                        } else {
                            for (int m = 0; m < n_temp_pts - 1; ++m) {
                                if (z_physical >= d_temp_z_ptr[m] && 
                                    z_physical <= d_temp_z_ptr[m + 1]) {
                                    T_local = temperature_linear_interp(
                                        z_physical,
                                        d_temp_z_ptr[m], d_temp_T_ptr[m],
                                        d_temp_z_ptr[m + 1], d_temp_T_ptr[m + 1]);
                                    break;
                                }
                            }
                        }
                        if (buoy_use_velocity) {
                            w_vel += buoyancy_velocity(T_local, T_ref, buoy_dt, buoy_coeff);
                        }
                    }
                    
                    if (use_kinematic_bc && k > 0) {
                        Real z_physical_below = z_lo_cap + (k - Real(0.5)) * dz_cap;
                        Real z_agl_below = z_physical_below - d_terr_ptr[j * nx_cap + i];
                        if (z_agl_below <= Real(0.0)) {
                            std::size_t idx_2d = static_cast<std::size_t>(j) * nx_cap + i;
                            Real dh_dx = d_terr_grad_x_ptr[idx_2d];
                            Real dh_dy = d_terr_grad_y_ptr[idx_2d];
                            w_vel = terrain_kinematic_w(u_vel, v_vel, dh_dx, dh_dy, bc_relax);
                        }
                    }
                    
                    vel(i, j, k, 0) = u_vel;
                    vel(i, j, k, 1) = v_vel;
                    vel(i, j, k, 2) = w_vel;
                }
            });
        }
    } else if (init_mode == "windfield") {
        std::vector<Real> x_wf, y_wf, z_wf, ux_wf, uy_wf, uz_wf;
        WindIO::read_windfield_file(windfield_file, x_wf, y_wf, z_wf, ux_wf, uy_wf, uz_wf);

        std::vector<Real> vel_u_h(static_cast<std::size_t>(nx) * ny * nz);
        std::vector<Real> vel_v_h(static_cast<std::size_t>(nx) * ny * nz);
        std::vector<Real> vel_w_h(static_cast<std::size_t>(nx) * ny * nz);

        for (int k = 0; k < nz; ++k) {
            Real zc = z_lo_cap + (k + Real(0.5)) * dz_cap;
            Real rmax = (k == 0) ? idw_rmax1 : idw_rmax2;
            for (int j = 0; j < ny; ++j) {
                Real yc = y_lo + (j + Real(0.5)) * dy;
                for (int i = 0; i < nx; ++i) {
                    Real xc = x_lo + (i + Real(0.5)) * dx;
                    auto [ux_interp, uy_interp, uz_interp] = WindInterpolation::idw_velocity_3d_full(
                        xc, yc, zc, x_wf, y_wf, z_wf, ux_wf, uy_wf, uz_wf, 6,
                        idw_gamma, enable_topographic_shielding, terrain_h, x_lo, y_lo, dx, dy, nx, ny,
                        rmax, idw_exponent);
                    std::size_t idx = (static_cast<std::size_t>(k) * ny_cap + j) * nx_cap + i;
                    vel_u_h[idx] = ux_interp;
                    vel_v_h[idx] = uy_interp;
                    vel_w_h[idx] = uz_interp;
                }
            }
        }

        d_vel_u.resize(vel_u_h.size());
        d_vel_v.resize(vel_v_h.size());
        d_vel_w.resize(vel_w_h.size());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, vel_u_h.begin(), vel_u_h.end(), d_vel_u.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, vel_v_h.begin(), vel_v_h.end(), d_vel_v.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, vel_w_h.begin(), vel_w_h.end(), d_vel_w.begin());
        d_vel_u_ptr = d_vel_u.data();
        d_vel_v_ptr = d_vel_v.data();
        d_vel_w_ptr = d_vel_w.data();

        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real z_physical = z_lo_cap + (k + Real(0.5)) * dz_cap;
                Real z_agl      = z_physical - d_terr_ptr[j * nx_cap + i];

                if (z_agl <= Real(0.0)) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                } else {
                    std::size_t idx = (static_cast<std::size_t>(k) * ny_cap + j) * nx_cap + i;
                    vel(i, j, k, 0) = d_vel_u_ptr[idx];
                    vel(i, j, k, 1) = d_vel_v_ptr[idx];
                    vel(i, j, k, 2) = d_vel_w_ptr[idx];
                }
            });
        }
    }

    vel0_ptr->FillBoundary(geom_ptr->periodicity());

    if (enable_wake && !building_xmin.empty()) {
        amrex::Print() << "wind_solver: applying wake model (Röckle formulation)\n";
        
        WakeParams wake_params;
        wake_params.enabled = true;
        wake_params.c1 = wake_c1;
        wake_params.c2 = wake_c2;
        wake_params.separation_length = wake_separation_length;
        
        // Wake model enhancement parameters
        wake_params.enable_oblique_scaling = enable_oblique_scaling;
        wake_params.enable_tall_building_correction = enable_tall_building_correction;
        wake_params.enable_gaussian_profile = enable_gaussian_profile;
        wake_params.enable_upwind_recirculation = enable_upwind_recirculation;
        wake_params.enable_reference_correction = enable_reference_correction;
        wake_params.enable_corner_acceleration = enable_corner_acceleration;
        wake_params.enable_variance_correction = enable_variance_correction;
        wake_params.enable_horseshoe_vortex = enable_horseshoe_vortex;
        wake_params.enable_extended_farwake = enable_extended_farwake;
        wake_params.enable_yoshie_two_layer = enable_yoshie_two_layer;
        wake_params.yoshie_decay_beta = yoshie_decay_beta;
        wake_params.enable_rodi_entrainment = enable_rodi_entrainment;
        wake_params.rodi_ce_coefficient = rodi_ce_coefficient;
        wake_params.enable_lopes_comfort = enable_lopes_comfort;
        wake_params.lopes_comfort_threshold = lopes_comfort_threshold;
        wake_params.lopes_assessment_height = lopes_assessment_height;
        wake_params.lopes_reference_frequency = lopes_reference_frequency;
        wake_params.enable_oikonomou_aspect = enable_oikonomou_aspect;
        wake_params.oikonomou_beta_aspect = oikonomou_beta_aspect;
        wake_params.enable_britter_hanna_urban = enable_britter_hanna_urban;
        wake_params.britter_hanna_alpha = britter_hanna_alpha;
        
        if (wake_model_type == "aermod_prime" || wake_model_type == "aermod-prime" ||
            wake_model_type == "AERMOD_PRIME" || wake_model_type == "AERMOD-PRIME" ||
            wake_model_type == "prime" || wake_model_type == "PRIME") {
            wake_params.model_type = WakeModelType::AERMOD_PRIME;
        } else if (wake_model_type == "huber_snyder" || wake_model_type == "huber-snyder" || 
            wake_model_type == "HUBER_SNYDER" || wake_model_type == "HUBER-SNYDER") {
            wake_params.model_type = WakeModelType::HUBER_SNYDER;
        } else {
            wake_params.model_type = WakeModelType::ROCKLE;
        }
        
        int n_buildings = static_cast<int>(building_xmin.size());
        Gpu::DeviceVector<Real> d_bldg_xmin(n_buildings);
        Gpu::DeviceVector<Real> d_bldg_xmax(n_buildings);
        Gpu::DeviceVector<Real> d_bldg_ymin(n_buildings);
        Gpu::DeviceVector<Real> d_bldg_ymax(n_buildings);
        Gpu::DeviceVector<Real> d_bldg_zmin(n_buildings);
        Gpu::DeviceVector<Real> d_bldg_zmax(n_buildings);
        Gpu::DeviceVector<Real> d_bldg_rotation(n_buildings);
        Gpu::DeviceVector<int> d_bldg_shape(n_buildings);
        Gpu::DeviceVector<Real> d_bldg_pitch_or_radius(n_buildings);
        Gpu::DeviceVector<Real> d_bldg_pitch_direction(n_buildings);
        
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, building_xmin.begin(), building_xmin.end(), d_bldg_xmin.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, building_xmax.begin(), building_xmax.end(), d_bldg_xmax.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, building_ymin.begin(), building_ymin.end(), d_bldg_ymin.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, building_ymax.begin(), building_ymax.end(), d_bldg_ymax.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, building_zmin.begin(), building_zmin.end(), d_bldg_zmin.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, building_zmax.begin(), building_zmax.end(), d_bldg_zmax.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, building_rotation.begin(), building_rotation.end(), d_bldg_rotation.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, building_shape.begin(), building_shape.end(), d_bldg_shape.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, building_pitch_or_radius.begin(), building_pitch_or_radius.end(), d_bldg_pitch_or_radius.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, building_pitch_direction.begin(), building_pitch_direction.end(), d_bldg_pitch_direction.begin());
        
        // GPU memory for polygon buildings: flatten nested vectors into single arrays
        std::vector<Real> polygon_x_flat, polygon_y_flat;
        std::vector<int> polygon_vertex_start(n_buildings);  // Start index for each building
        std::vector<int> polygon_vertex_count(n_buildings);  // Vertex count for each building
        std::vector<int> d_geom_type_host(n_buildings);      // Geometry type for each building
        
        for (int b = 0; b < n_buildings; ++b) {
            polygon_vertex_start[b] = static_cast<int>(polygon_x_flat.size());
            d_geom_type_host[b] = building_geom_type[b];
            
            if (building_geom_type[b] > 0 && b < static_cast<int>(building_polygon_x.size())) {
                // Polygon or void building
                polygon_vertex_count[b] = static_cast<int>(building_polygon_x[b].size());
                for (int v = 0; v < polygon_vertex_count[b]; ++v) {
                    polygon_x_flat.push_back(building_polygon_x[b][v]);
                    polygon_y_flat.push_back(building_polygon_y[b][v]);
                }
            } else {
                // Rectangular building
                polygon_vertex_count[b] = 0;
            }
        }
        
        // Allocate GPU device vectors for polygon data
        Gpu::DeviceVector<Real> d_polygon_x(polygon_x_flat.size());
        Gpu::DeviceVector<Real> d_polygon_y(polygon_y_flat.size());
        Gpu::DeviceVector<int> d_polygon_start(n_buildings);
        Gpu::DeviceVector<int> d_polygon_count(n_buildings);
        Gpu::DeviceVector<int> d_geom_type(n_buildings);
        
        // Copy polygon data to device
        if (!polygon_x_flat.empty()) {
            amrex::Gpu::copy(amrex::Gpu::hostToDevice, polygon_x_flat.begin(), polygon_x_flat.end(), d_polygon_x.begin());
            amrex::Gpu::copy(amrex::Gpu::hostToDevice, polygon_y_flat.begin(), polygon_y_flat.end(), d_polygon_y.begin());
        }
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, polygon_vertex_start.begin(), polygon_vertex_start.end(), d_polygon_start.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, polygon_vertex_count.begin(), polygon_vertex_count.end(), d_polygon_count.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, d_geom_type_host.begin(), d_geom_type_host.end(), d_geom_type.begin());
        
        Real const* d_bldg_xmin_ptr = d_bldg_xmin.data();
        Real const* d_bldg_xmax_ptr = d_bldg_xmax.data();
        Real const* d_bldg_ymin_ptr = d_bldg_ymin.data();
        Real const* d_bldg_ymax_ptr = d_bldg_ymax.data();
        Real const* d_bldg_zmin_ptr = d_bldg_zmin.data();
        Real const* d_bldg_zmax_ptr = d_bldg_zmax.data();
        Real const* d_bldg_rotation_ptr = d_bldg_rotation.data();
        int const* d_bldg_shape_ptr = d_bldg_shape.data();
        Real const* d_bldg_pitch_or_radius_ptr = d_bldg_pitch_or_radius.data();
        Real const* d_bldg_pitch_direction_ptr = d_bldg_pitch_direction.data();
        
        // Polygon building device pointers
        Real const* d_polygon_x_ptr = d_polygon_x.data();
        Real const* d_polygon_y_ptr = d_polygon_y.data();
        int const* d_polygon_start_ptr = d_polygon_start.data();
        int const* d_polygon_count_ptr = d_polygon_count.data();
        int const* d_geom_type_ptr = d_geom_type.data();
        
        const int n_bldg_cap = n_buildings;
        const Real dx_wake = dx;
        const Real dy_wake = dy;
        const Real dz_wake = dz;
        const Real x_lo_wake = x_lo;
        const Real y_lo_wake = y_lo;
        const Real z_lo_wake = zs_min;
        const bool use_superposition = wake_superposition;
        const bool use_street_canyon = enable_street_canyon;
        const Real canyon_reduction = street_canyon_reduction;
        const Real U_ref_wake = U_ref;
        const Real V_ref_wake = V_ref;
        
        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);
            
            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real x = x_lo_wake + (i + Real(0.5)) * dx_wake;
                Real y = y_lo_wake + (j + Real(0.5)) * dy_wake;
                Real z = z_lo_wake + (k + Real(0.5)) * dz_wake;
                
                Real u = vel(i, j, k, 0);
                Real v = vel(i, j, k, 1);
                Real w = vel(i, j, k, 2);
                
                if (use_superposition && n_bldg_cap > 1) {
                    apply_wake_superposition(
                        x, y, z, u, v, w,
                        d_bldg_xmin_ptr, d_bldg_xmax_ptr,
                        d_bldg_ymin_ptr, d_bldg_ymax_ptr,
                        d_bldg_zmin_ptr, d_bldg_zmax_ptr,
                        d_bldg_rotation_ptr,
                        d_bldg_shape_ptr,
                        d_bldg_pitch_or_radius_ptr,
                        d_bldg_pitch_direction_ptr,
                        n_bldg_cap, wake_params);
                } else {
                    for (int b = 0; b < n_bldg_cap; ++b) {
                        // Check if this is a polygon/void building or rectangular building
                        if (d_geom_type_ptr[b] > 0 && d_polygon_count_ptr[b] > 0) {
                            // Polygon or void building - dispatch to polygon wake function
                            int v_start = d_polygon_start_ptr[b];
                            int n_verts = d_polygon_count_ptr[b];
                            Real poly_height = d_bldg_zmax_ptr[b] - d_bldg_zmin_ptr[b];
                            Real poly_zmin = d_bldg_zmin_ptr[b];
                            
                            // Wind direction (normalize U_ref and V_ref)
                            Real U_mag = std::sqrt(U_ref_wake * U_ref_wake + V_ref_wake * V_ref_wake);
                            Real wd_x = 1.0;
                            Real wd_y = 0.0;
                            if (U_mag > 1.0e-10) {
                                wd_x = U_ref_wake / U_mag;
                                wd_y = V_ref_wake / U_mag;
                            }
                            
                            // Skip void zones (geom_type == 2)
                            if (d_geom_type_ptr[b] == 2) {
                                continue;
                            }
                            
                            Real du = 0.0, dv = 0.0, dw = 0.0;
                            bool hit = false;
                            
                            // Dispatch based on wake model type
                            if (wake_params.model_type == WakeModelType::AERMOD_PRIME) {
                                hit = polygon_aermod_prime_wake_deficit(
                                    x, y, z,
                                    d_polygon_x_ptr + v_start, d_polygon_y_ptr + v_start,
                                    n_verts, poly_zmin, poly_height,
                                    U_mag, wd_x, wd_y, wake_params,
                                    du, dv, dw);
                            } else if (wake_params.model_type == WakeModelType::HUBER_SNYDER) {
                                hit = polygon_huber_snyder_wake_deficit(
                                    x, y, z,
                                    d_polygon_x_ptr + v_start, d_polygon_y_ptr + v_start,
                                    n_verts, poly_zmin, poly_height,
                                    U_mag, wd_x, wd_y, wake_params,
                                    du, dv, dw);
                            } else {
                                // Default: Röckle model
                                hit = polygon_rockle_wake_deficit(
                                    x, y, z,
                                    d_polygon_x_ptr + v_start, d_polygon_y_ptr + v_start,
                                    n_verts, poly_zmin, poly_height,
                                    U_mag, wd_x, wd_y, wake_params,
                                    du, dv, dw);
                            }
                            
                            if (hit) {
                                u -= du;  // Apply deficit by subtraction (same as rectangular buildings)
                                v -= dv;
                                w -= dw;
                            }
                        } else {
                            // Rectangular building - use existing code path
                            Building bldg = compute_building_dimensions(
                                d_bldg_xmin_ptr[b], d_bldg_xmax_ptr[b],
                                d_bldg_ymin_ptr[b], d_bldg_ymax_ptr[b],
                                d_bldg_zmin_ptr[b], d_bldg_zmax_ptr[b]);
                            bldg.rotation = d_bldg_rotation_ptr[b];
                            bldg.shape = static_cast<BuildingShape>(d_bldg_shape_ptr[b]);
                            bldg.pitch_or_radius = d_bldg_pitch_or_radius_ptr[b];
                            bldg.pitch_direction = d_bldg_pitch_direction_ptr[b];
                            
                            apply_single_building_wake(x, y, z, u, v, w, bldg, wake_params);
                        }
                    }
                }
                
                if (use_street_canyon && n_bldg_cap > 1) {
                    Real avg_height = Real(0.0);
                    for (int b = 0; b < n_bldg_cap; ++b) {
                        avg_height += (d_bldg_zmax_ptr[b] - d_bldg_zmin_ptr[b]);
                    }
                    avg_height /= Real(n_bldg_cap);
                    Real street_width = Real(2.0) * dx_wake;
                    
                    apply_street_canyon_effect(
                        z, u, v, w,
                        avg_height, street_width, canyon_reduction);
                }
                
                vel(i, j, k, 0) = u;
                vel(i, j, k, 1) = v;
                vel(i, j, k, 2) = w;
            });
        }
        vel0_ptr->FillBoundary(geom_ptr->periodicity());
    }

    if (enable_building_porosity && !porous_building_xmin.empty()) {
        amrex::Print() << "wind_solver: applying building porosity model\n";
        
        PorosityParams porosity_params;
        porosity_params.enabled = true;
        porosity_params.default_porosity = default_building_porosity;
        porosity_params.drag_coefficient = porosity_drag_coefficient;
        
        int n_porous = static_cast<int>(porous_building_xmin.size());
        Gpu::DeviceVector<Real> d_porous_xmin(n_porous);
        Gpu::DeviceVector<Real> d_porous_xmax(n_porous);
        Gpu::DeviceVector<Real> d_porous_ymin(n_porous);
        Gpu::DeviceVector<Real> d_porous_ymax(n_porous);
        Gpu::DeviceVector<Real> d_porous_zmin(n_porous);
        Gpu::DeviceVector<Real> d_porous_zmax(n_porous);
        Gpu::DeviceVector<Real> d_porous_porosity(n_porous);
        Gpu::DeviceVector<Real> d_porous_rotation(n_porous);
        
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, porous_building_xmin.begin(), porous_building_xmin.end(), d_porous_xmin.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, porous_building_xmax.begin(), porous_building_xmax.end(), d_porous_xmax.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, porous_building_ymin.begin(), porous_building_ymin.end(), d_porous_ymin.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, porous_building_ymax.begin(), porous_building_ymax.end(), d_porous_ymax.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, porous_building_zmin.begin(), porous_building_zmin.end(), d_porous_zmin.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, porous_building_zmax.begin(), porous_building_zmax.end(), d_porous_zmax.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, porous_building_porosity.begin(), porous_building_porosity.end(), d_porous_porosity.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, porous_building_rotation.begin(), porous_building_rotation.end(), d_porous_rotation.begin());
        
        Real const* d_porous_xmin_ptr = d_porous_xmin.data();
        Real const* d_porous_xmax_ptr = d_porous_xmax.data();
        Real const* d_porous_ymin_ptr = d_porous_ymin.data();
        Real const* d_porous_ymax_ptr = d_porous_ymax.data();
        Real const* d_porous_zmin_ptr = d_porous_zmin.data();
        Real const* d_porous_zmax_ptr = d_porous_zmax.data();
        Real const* d_porous_porosity_ptr = d_porous_porosity.data();
        Real const* d_porous_rotation_ptr = d_porous_rotation.data();
        
        const int n_porous_cap = n_porous;
        const Real dx_porous = dx;
        const Real dy_porous = dy;
        const Real dz_porous = dz;
        const Real x_lo_porous = x_lo;
        const Real y_lo_porous = y_lo;
        const Real z_lo_porous = zs_min;
        const Real drag_coeff = porosity_drag_coefficient;
        
        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);
            
            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real x = x_lo_porous + (i + Real(0.5)) * dx_porous;
                Real y = y_lo_porous + (j + Real(0.5)) * dy_porous;
                Real z = z_lo_porous + (k + Real(0.5)) * dz_porous;
                
                Real porosity = Real(1.0);
                
                for (int b = 0; b < n_porous_cap; ++b) {
                    PorousBuilding pb;
                    pb.xmin = d_porous_xmin_ptr[b];
                    pb.xmax = d_porous_xmax_ptr[b];
                    pb.ymin = d_porous_ymin_ptr[b];
                    pb.ymax = d_porous_ymax_ptr[b];
                    pb.zmin = d_porous_zmin_ptr[b];
                    pb.zmax = d_porous_zmax_ptr[b];
                    pb.porosity = d_porous_porosity_ptr[b];
                    pb.rotation = d_porous_rotation_ptr[b];
                    
                    Real p = point_in_porous_building(x, y, z, pb);
                    if (p < porosity) {
                        porosity = p;
                    }
                }
                
                if (porosity < Real(0.999)) {
                    Real u = vel(i, j, k, 0);
                    Real v = vel(i, j, k, 1);
                    Real w = vel(i, j, k, 2);
                    
                    apply_porosity_drag(u, v, w, porosity, drag_coeff, 
                                      dx_porous, dy_porous, dz_porous);
                    
                    vel(i, j, k, 0) = u;
                    vel(i, j, k, 1) = v;
                    vel(i, j, k, 2) = w;
                }
            });
        }
        vel0_ptr->FillBoundary(geom_ptr->periodicity());
    }

    if (enable_windbreaks && !windbreak_x1.empty()) {
        amrex::Print() << "wind_solver: applying sub-grid windbreaks model\n";
        
        int n_windbreaks = static_cast<int>(windbreak_x1.size());
        Gpu::DeviceVector<Real> d_wb_x1(n_windbreaks);
        Gpu::DeviceVector<Real> d_wb_y1(n_windbreaks);
        Gpu::DeviceVector<Real> d_wb_x2(n_windbreaks);
        Gpu::DeviceVector<Real> d_wb_y2(n_windbreaks);
        Gpu::DeviceVector<Real> d_wb_height(n_windbreaks);
        Gpu::DeviceVector<Real> d_wb_blockage(n_windbreaks);
        Gpu::DeviceVector<Real> d_wb_drag_coeff(n_windbreaks);
        
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, windbreak_x1.begin(), windbreak_x1.end(), d_wb_x1.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, windbreak_y1.begin(), windbreak_y1.end(), d_wb_y1.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, windbreak_x2.begin(), windbreak_x2.end(), d_wb_x2.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, windbreak_y2.begin(), windbreak_y2.end(), d_wb_y2.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, windbreak_height.begin(), windbreak_height.end(), d_wb_height.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, windbreak_blockage.begin(), windbreak_blockage.end(), d_wb_blockage.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, windbreak_drag_coeff.begin(), windbreak_drag_coeff.end(), d_wb_drag_coeff.begin());
        
        Real const* d_wb_x1_ptr = d_wb_x1.data();
        Real const* d_wb_y1_ptr = d_wb_y1.data();
        Real const* d_wb_x2_ptr = d_wb_x2.data();
        Real const* d_wb_y2_ptr = d_wb_y2.data();
        Real const* d_wb_height_ptr = d_wb_height.data();
        Real const* d_wb_blockage_ptr = d_wb_blockage.data();
        Real const* d_wb_drag_coeff_ptr = d_wb_drag_coeff.data();
        
        const int n_wb_cap = n_windbreaks;
        const Real dx_wb = dx;
        const Real dy_wb = dy;
        const Real x_lo_wb = x_lo;
        const Real y_lo_wb = y_lo;
        const Real z_lo_wb = zs_min;
        const Real max_cell_spacing = std::max(dx, dy);
        
        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);
            
            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real x = x_lo_wb + (i + Real(0.5)) * dx_wb;
                Real y = y_lo_wb + (j + Real(0.5)) * dy_wb;
                Real z = z_lo_wb + (k + Real(0.5)) * dz_cap;
                
                Real terrain_elev = d_terr_ptr[j * nx_cap + i];
                Real z_agl = z - terrain_elev;
                
                int closest_wb = -1;
                Real closest_dist = max_cell_spacing;
                
                for (int b = 0; b < n_wb_cap; ++b) {
                    Real wb_height = d_wb_height_ptr[b];
                    if (z_agl > Real(0.0) && z_agl <= wb_height) {
                        Real t_clamped;
                        Real dist = distance_to_segment(x, y, 
                                                        d_wb_x1_ptr[b], d_wb_y1_ptr[b],
                                                        d_wb_x2_ptr[b], d_wb_y2_ptr[b],
                                                        t_clamped);
                        if (dist <= Real(0.5) * max_cell_spacing && dist < closest_dist) {
                            closest_dist = dist;
                            closest_wb = b;
                        }
                    }
                }
                
                if (closest_wb >= 0) {
                    Real u = vel(i, j, k, 0);
                    Real v = vel(i, j, k, 1);
                    
                    apply_windbreak_drag(u, v, 
                                         d_wb_blockage_ptr[closest_wb],
                                         d_wb_drag_coeff_ptr[closest_wb],
                                         d_wb_x1_ptr[closest_wb], d_wb_y1_ptr[closest_wb],
                                         d_wb_x2_ptr[closest_wb], d_wb_y2_ptr[closest_wb],
                                         dx_wb, dy_wb);
                    
                    vel(i, j, k, 0) = u;
                    vel(i, j, k, 1) = v;
                }
            });
        }
        vel0_ptr->FillBoundary(geom_ptr->periodicity());
    }

    if (enable_turbine_wake && !turbines.empty()) {
        TurbineWake::TurbineWakeModelType tw_model_type = TurbineWake::TurbineWakeModelType::JENSEN;
        if (turbine_wake_model_type == "bastankhah_gaussian" || turbine_wake_model_type == "gaussian") {
            tw_model_type = TurbineWake::TurbineWakeModelType::BASTANKHAH_GAUSSIAN;
        } else if (turbine_wake_model_type == "turbopark") {
            tw_model_type = TurbineWake::TurbineWakeModelType::TURBOPARK;
        } else if (turbine_wake_model_type == "gch" || turbine_wake_model_type == "gauss_curl_hybrid") {
            tw_model_type = TurbineWake::TurbineWakeModelType::GAUSS_CURL_HYBRID;
        }
        TurbineWake::SuperpositionType tw_superposition = TurbineWake::SuperpositionType::QUADRATIC;
        if (turbine_wake_superposition == "linear") {
            tw_superposition = TurbineWake::SuperpositionType::LINEAR;
        } else if (turbine_wake_superposition == "max") {
            tw_superposition = TurbineWake::SuperpositionType::MAX;
        }
        TurbineWake::WakeAddedTurbulenceModelType added_turb_model = TurbineWake::WakeAddedTurbulenceModelType::NONE;
        if (wake_added_turbulence_model == "crespo_hernandez") {
            added_turb_model = TurbineWake::WakeAddedTurbulenceModelType::CRESPO_HERNANDEZ;
        } else if (wake_added_turbulence_model == "frandsen" || wake_added_turbulence_model == "stf") {
            added_turb_model = TurbineWake::WakeAddedTurbulenceModelType::FRANDSEN;
        }
        
        TurbineWake::apply_turbine_wakes_to_multifab(
            *vel0_ptr,
            terrain_h,
            turbines,
            tw_model_type,
            tw_superposition,
            jensen_kw,
            gaussian_ka,
            enable_stability_correction,
            stability_length,
            x_lo, y_lo, zs_min,
            dx, dy, dz,
            nx, ny, nz,
            turbopark_c1,
            ambient_ti,
            enable_jimenez_deflection,
            jimenez_kd,
            enable_bastankhah_deflection,
            added_turb_model,
            time_step,
            enable_wake_ground_interaction,
            wake_ground_damping_scale,
            surface_sensible_heat_flux,
            buoyant_wake_destruction_coeff
        );
    }

    if (enable_capping_lid) {
        amrex::Print() << "wind_solver: enforcing capping lid boundary condition (w = 0) using spatially-varying boundary layer depth from z_bl_diag_ptr (fallback: " << capping_lid_height << " m)\n";
        const Real fallback_lid_height = capping_lid_height;
        const Real z_lo_cap_lid = zs_min;
        const Real dz_cap_lid = dz;
        const int nx_cap_lid = nx;
        const Real* d_terr_ptr = d_obstacle_h.data();

        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);
            const auto z_bl_arr = z_bl_diag_ptr->const_array(mfi);

            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                Real z_physical = z_lo_cap_lid + (k + Real(0.5)) * dz_cap_lid;
                Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_lid + i];
                Real lid_height = (z_bl_arr(i, j, k) > Real(0.0)) ? z_bl_arr(i, j, k) : fallback_lid_height;
                if (z_agl >= lid_height) {
                    vel(i, j, k, 2) = Real(0.0);
                }
            });
        }
        amrex::Gpu::streamSynchronize();
    }

    if (enable_street_canyon && !building_xmin.empty()) {
        std::vector<StreetCanyon> host_canyons = detect_street_canyons(
            building_xmin, building_xmax, building_ymin, building_ymax,
            building_zmin, building_zmax, building_rotation);
        
        if (!host_canyons.empty()) {
            amrex::Print() << "wind_solver: applying Building Street Canyon Vortex Parameterization to initial wind field\n";
            for (size_t c = 0; c < host_canyons.size(); ++c) {
                amrex::Print() << "  Canyon " << c << ": direction=" << (host_canyons[c].direction == 0 ? "Y-aligned" : "X-aligned")
                               << ", width=" << host_canyons[c].w_canyon << " m, height=" << host_canyons[c].h_canyon << " m"
                               << ", aspect_ratio=" << host_canyons[c].aspect_ratio << "\n";
            }
            int n_canyons = static_cast<int>(host_canyons.size());
            Gpu::DeviceVector<StreetCanyon> d_canyons(n_canyons);
            amrex::Gpu::copy(amrex::Gpu::hostToDevice, host_canyons.begin(), host_canyons.end(), d_canyons.begin());
            StreetCanyon const* d_canyons_ptr = d_canyons.data();
            
            const Real dx_c = dx;
            const Real dy_c = dy;
            const Real dz_c = dz;
            const Real x_lo_c = x_lo;
            const Real y_lo_c = y_lo;
            const Real z_lo_c = zs_min;
            const int nz_c = nz;
            
            for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                auto vel = vel0_ptr->array(mfi);
                
                amrex::ParallelFor(bx,
                    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
                {
                    Real x = x_lo_c + (i + Real(0.5)) * dx_c;
                    Real y = y_lo_c + (j + Real(0.5)) * dy_c;
                    Real z = z_lo_c + (k + Real(0.5)) * dz_c;
                    
                    for (int c = 0; c < n_canyons; ++c) {
                        const auto& canyon = d_canyons_ptr[c];
                        
                        // Check if point is inside this canyon
                        if (x >= canyon.xmin && x <= canyon.xmax &&
                            y >= canyon.ymin && y <= canyon.ymax &&
                            z >= canyon.zmin && z <= canyon.zmax)
                        {
                            // Calculate aspect ratio
                            Real aspect = canyon.aspect_ratio;
                            
                            // Calculate vortex strength factor
                            Real vortex_strength_factor = Real(0.0);
                            if (aspect > Real(0.7)) {
                                vortex_strength_factor = Real(0.25);
                            } else if (aspect > Real(0.3)) {
                                vortex_strength_factor = Real(0.25) * (aspect - Real(0.3)) / Real(0.4);
                            }
                            
                            if (vortex_strength_factor > Real(1.0e-6)) {
                                // Find local ambient velocity at canyon top
                                int k_top = std::min(std::max(0, static_cast<int>(std::round((canyon.zmax - z_lo_c)/dz_c - Real(0.5)))), nz_c - 1);
                                Real U_amb = vel(i, j, k_top, 0);
                                Real V_amb = vel(i, j, k_top, 1);
                                
                                Real z_ratio = (z - canyon.zmin) / canyon.h_canyon;
                                
                                if (canyon.direction == 0) { // Y-aligned (separated in X)
                                    Real x_ratio = (x - canyon.xmin) / canyon.w_canyon;
                                    
                                    // Overwrite initial wind field (u0) with parameterized vortex profile
                                    vel(i, j, k, 0) = -vortex_strength_factor * U_amb * std::cos(MathConstants::pi * z_ratio) * std::sin(MathConstants::pi * x_ratio);
                                    vel(i, j, k, 2) = vortex_strength_factor * U_amb * (canyon.h_canyon / canyon.w_canyon) * std::sin(MathConstants::pi * z_ratio) * std::cos(MathConstants::pi * x_ratio);
                                    vel(i, j, k, 1) = Real(0.0); // Suppressed cross-wind
                                } else { // X-aligned (separated in Y)
                                    Real y_ratio = (y - canyon.ymin) / canyon.w_canyon;
                                    
                                    // Overwrite initial wind field (v0) with parameterized vortex profile
                                    vel(i, j, k, 1) = -vortex_strength_factor * V_amb * std::cos(MathConstants::pi * z_ratio) * std::sin(MathConstants::pi * y_ratio);
                                    vel(i, j, k, 2) = vortex_strength_factor * V_amb * (canyon.h_canyon / canyon.w_canyon) * std::sin(MathConstants::pi * z_ratio) * std::cos(MathConstants::pi * y_ratio);
                                    vel(i, j, k, 0) = Real(0.0); // Suppressed cross-wind
                                }
                            }
                            // Stop checking other canyons once we've processed this cell (canyons are disjoint in this model)
                            break; 
                        }
                    }
                });
            }
            amrex::Gpu::streamSynchronize();
            vel0_ptr->FillBoundary(geom_ptr->periodicity());
        }
    }

    if (enable_eb) {
        amrex::Print() << "wind_solver: masking initial velocities using Embedded Boundary (vfrac < " << eb_threshold << ")\n";
        const Real eb_thresh = eb_threshold;
        const amrex::MultiFab& vfrac = eb_factory->getVolFrac();
        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);
            const auto vfrac_arr = vfrac.const_array(mfi);
            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                if (vfrac_arr(i, j, k) < eb_thresh) {
                    vel(i, j, k, 0) = Real(0.0);
                    vel(i, j, k, 1) = Real(0.0);
                    vel(i, j, k, 2) = Real(0.0);
                }
            });
        }
        vel0_ptr->FillBoundary(geom_ptr->periodicity());
        amrex::Print() << "wind_solver: EB velocity masking complete\n";
    }

    amrex::Print() << "wind_solver: wind initialization time = " 
                   << (amrex::second() - t_phase) << " s\n";
}

void WindSolverApp::execute_poisson_solve(int time_step) {
    amrex::ignore_unused(time_step);
    t_phase = amrex::second();
    const Box domain = geom_ptr->Domain();
    const IntVect glo = domain.smallEnd();
    const IntVect ghi = domain.bigEnd();
    const int ilo = glo[0], ihi = ghi[0];
    const int jlo = glo[1], jhi = ghi[1];
    const int klo = glo[2], khi = ghi[2];
    const Real inv2dx = Real(0.5) / dx;
    const Real inv2dy = Real(0.5) / dy;
    const Real inv2dz = Real(0.5) / dz;
    const Real inv1dx = Real(1.0) / dx;
    const Real inv1dy = Real(1.0) / dy;
    const Real inv1dz = Real(1.0) / dz;
    
    const int deriv_method_cap = deriv_method_int;
    const Real dx_cap = dx;
    const Real dy_cap = dy;
    const Real dz_cap_div = dz;
    const Real z_lo_cap_div = zs_min;
    const int  nx_cap_div   = nx;
    const Real* d_terr_ptr = d_obstacle_h.data();

    const bool use_eb = enable_eb;
    const Real eb_thresh = eb_threshold;
    const amrex::MultiFab* vfrac_ptr = enable_eb ? &(eb_factory->getVolFrac()) : nullptr;

    if (enable_obrien_w_adjustment) {
        amrex::Print() << "wind_solver: Applying O'Brien Vertical Velocity Adjustment Procedure\n";
        for (MFIter mfi(*vel0_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto vel = vel0_ptr->array(mfi);
            const auto vfrac_arr = use_eb ? vfrac_ptr->const_array(mfi) : Array4<Real const>{};
            Box bx_2d(IntVect(bx.smallEnd(0), bx.smallEnd(1), 0),
                      IntVect(bx.bigEnd(0),   bx.bigEnd(1),   0));
            amrex::ParallelFor(bx_2d,
                [=] AMREX_GPU_DEVICE (int i, int j, int) noexcept
            {
                Real terrain_elev = d_terr_ptr[j * nx_cap_div + i];
                int k_start = klo;
                while (k_start <= khi) {
                    Real cell_center_height = z_lo_cap_div + (Real(k_start) + Real(0.5)) * dz_cap_div;
                    bool is_solid = (cell_center_height - terrain_elev <= Real(0.0));
                    if (use_eb) {
                        is_solid = is_solid || (vfrac_arr(i, j, k_start) < eb_thresh);
                    }
                    if (is_solid) {
                        k_start++;
                    } else {
                        break;
                    }
                }
                if (k_start < khi) {
                    // C++ device lambda for horizontal divergence Dh at level k
                    auto get_Dh = [=] AMREX_GPU_DEVICE (int k) noexcept -> Real {
                        Real du = 0.0, dv = 0.0;
                        if (deriv_method_cap == 0) {
                            if (i == ilo)
                                du = (vel(i+1,j,k,0) - vel(i,j,k,0)) * inv1dx;
                            else if (i == ihi)
                                du = (vel(i,j,k,0) - vel(i-1,j,k,0)) * inv1dx;
                            else
                                du = (vel(i+1,j,k,0) - vel(i-1,j,k,0)) * inv2dx;
                        } else if (deriv_method_cap == 1) {
                            if (i == ilo)
                                du = (vel(i+1,j,k,0) - vel(i,j,k,0)) * inv1dx;
                            else if (i == ihi)
                                du = (vel(i,j,k,0) - vel(i-1,j,k,0)) * inv1dx;
                            else
                                du = NumericalDerivatives::weno3_deriv(vel(i-1,j,k,0), vel(i,j,k,0), vel(i+1,j,k,0), dx_cap);
                        } else {
                            if (i <= ilo+1)
                                du = (vel(i+1,j,k,0) - vel(i,j,k,0)) * inv1dx;
                            else if (i >= ihi-1)
                                du = (vel(i,j,k,0) - vel(i-1,j,k,0)) * inv1dx;
                            else
                                du = NumericalDerivatives::weno5_deriv(vel(i-2,j,k,0), vel(i-1,j,k,0), vel(i,j,k,0), 
                                                vel(i+1,j,k,0), vel(i+2,j,k,0), dx_cap);
                        }
                        if (deriv_method_cap == 0) {
                            if (j == jlo)
                                dv = (vel(i,j+1,k,1) - vel(i,j,k,1)) * inv1dy;
                            else if (j == jhi)
                                dv = (vel(i,j,k,1) - vel(i,j-1,k,1)) * inv1dy;
                            else
                                dv = (vel(i,j+1,k,1) - vel(i,j-1,k,1)) * inv2dy;
                        } else if (deriv_method_cap == 1) {
                            if (j == jlo)
                                dv = (vel(i,j+1,k,1) - vel(i,j,k,1)) * inv1dy;
                            else if (j == jhi)
                                dv = (vel(i,j,k,1) - vel(i,j-1,k,1)) * inv1dy;
                            else
                                dv = NumericalDerivatives::weno3_deriv(vel(i,j-1,k,1), vel(i,j,k,1), vel(i,j+1,k,1), dy_cap);
                        } else {
                            if (j <= jlo+1)
                                dv = (vel(i,j+1,k,1) - vel(i,j,k,1)) * inv1dy;
                            else if (j >= jhi-1)
                                dv = (vel(i,j,k,1) - vel(i,j-1,k,1)) * inv1dy;
                            else
                                dv = NumericalDerivatives::weno5_deriv(vel(i,j-2,k,1), vel(i,j-1,k,1), vel(i,j,k,1), 
                                                vel(i,j+1,k,1), vel(i,j+2,k,1), dy_cap);
                        }
                        return du + dv;
                    };

                    // Pass 1: Integrate up to domain top to get the vertical velocity residual E
                    Real w_top_val = vel(i, j, k_start, 2);
                    for (int k = k_start + 1; k <= khi; ++k) {
                        w_top_val -= get_Dh(k) * dz_cap_div;
                    }
                    Real E = w_top_val;

                    // Pass 2: Apply the polynomial adjustment on the fly, integrating sequentially
                    Real w_current = vel(i, j, k_start, 2);
                    for (int k = k_start + 1; k <= khi; ++k) {
                        w_current -= get_Dh(k) * dz_cap_div;
                        Real frac = Real(k - k_start) / Real(khi - k_start);
                        Real adjustment = frac * frac * E;
                        vel(i, j, k, 2) = w_current - adjustment;
                    }
                }
            });
        }
        amrex::Gpu::streamSynchronize();
        vel0_ptr->FillBoundary(geom_ptr->periodicity());
    }
    
    const bool use_buoyancy_rhs = enable_buoyancy_stratification && (buoyancy_method == "rhs");
    const Real T_ref_rhs = temperature_reference;
    const Real buoy_coeff_rhs = buoyancy_coefficient;
    const int n_temp_pts_rhs = z_temp.size();
    
    Gpu::DeviceVector<Real> d_temp_z, d_temp_T;
    Real const* d_temp_z_ptr = nullptr;
    Real const* d_temp_T_ptr = nullptr;
    if (use_buoyancy_rhs && !z_temp.empty()) {
        d_temp_z.resize(z_temp.size());
        d_temp_T.resize(T_temp.size());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, z_temp.begin(), z_temp.end(), d_temp_z.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice, T_temp.begin(), T_temp.end(), d_temp_T.begin());
        d_temp_z_ptr = d_temp_z.data();
        d_temp_T_ptr = d_temp_T.data();
    }
    
    const bool use_div_source = enable_divergence_source;
    const Real div_source_const = divergence_source_constant;

    for (MFIter mfi(*rhs_ptr); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        const auto vel = vel0_ptr->const_array(mfi);
        auto rh = rhs_ptr->array(mfi);
        const auto vfrac_arr = use_eb ? vfrac_ptr->const_array(mfi) : Array4<Real const>{};

        amrex::ParallelFor(bx,
            [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
        {
            Real z_physical = z_lo_cap_div + (k + Real(0.5)) * dz_cap_div;
            Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_div + i];
            
            bool is_solid = (z_agl <= Real(0.0));
            if (use_eb) {
                is_solid = is_solid || (vfrac_arr(i, j, k) < eb_thresh);
            }
            if (is_solid) { rh(i, j, k) = Real(0.0); return; }

            Real du, dv, dw;
            
            if (deriv_method_cap == 0) {
                if (i == ilo)
                    du = (vel(i+1,j,k,0) - vel(i,j,k,0)) * inv1dx;
                else if (i == ihi)
                    du = (vel(i,j,k,0) - vel(i-1,j,k,0)) * inv1dx;
                else
                    du = (vel(i+1,j,k,0) - vel(i-1,j,k,0)) * inv2dx;
            } else if (deriv_method_cap == 1) {
                if (i == ilo)
                    du = (vel(i+1,j,k,0) - vel(i,j,k,0)) * inv1dx;
                else if (i == ihi)
                    du = (vel(i,j,k,0) - vel(i-1,j,k,0)) * inv1dx;
                else
                    du = NumericalDerivatives::weno3_deriv(vel(i-1,j,k,0), vel(i,j,k,0), vel(i+1,j,k,0), dx_cap);
            } else {
                if (i <= ilo+1)
                    du = (vel(i+1,j,k,0) - vel(i,j,k,0)) * inv1dx;
                else if (i >= ihi-1)
                    du = (vel(i,j,k,0) - vel(i-1,j,k,0)) * inv1dx;
                else
                    du = NumericalDerivatives::weno5_deriv(vel(i-2,j,k,0), vel(i-1,j,k,0), vel(i,j,k,0), 
                                    vel(i+1,j,k,0), vel(i+2,j,k,0), dx_cap);
            }

            if (deriv_method_cap == 0) {
                if (j == jlo)
                    dv = (vel(i,j+1,k,1) - vel(i,j,k,1)) * inv1dy;
                else if (j == jhi)
                    dv = (vel(i,j,k,1) - vel(i,j-1,k,1)) * inv1dy;
                else
                    dv = (vel(i,j+1,k,1) - vel(i,j-1,k,1)) * inv2dy;
            } else if (deriv_method_cap == 1) {
                if (j == jlo)
                    dv = (vel(i,j+1,k,1) - vel(i,j,k,1)) * inv1dy;
                else if (j == jhi)
                    dv = (vel(i,j,k,1) - vel(i,j-1,k,1)) * inv1dy;
                else
                    dv = NumericalDerivatives::weno3_deriv(vel(i,j-1,k,1), vel(i,j,k,1), vel(i,j+1,k,1), dy_cap);
            } else {
                if (j <= jlo+1)
                    dv = (vel(i,j+1,k,1) - vel(i,j,k,1)) * inv1dy;
                else if (j >= jhi-1)
                    dv = (vel(i,j,k,1) - vel(i,j-1,k,1)) * inv1dy;
                else
                    dv = NumericalDerivatives::weno5_deriv(vel(i,j-2,k,1), vel(i,j-1,k,1), vel(i,j,k,1), 
                                    vel(i,j+1,k,1), vel(i,j+2,k,1), dy_cap);
            }

            if (deriv_method_cap == 0) {
                if (k == klo)
                    dw = (vel(i,j,k+1,2) - vel(i,j,k,2)) * inv1dz;
                else if (k == khi)
                    dw = (vel(i,j,k,2) - vel(i,j,k-1,2)) * inv1dz;
                else
                    dw = (vel(i,j,k+1,2) - vel(i,j,k-1,2)) * inv2dz;
            } else if (deriv_method_cap == 1) {
                if (k == klo)
                    dw = (vel(i,j,k+1,2) - vel(i,j,k,2)) * inv1dz;
                else if (k == khi)
                    dw = (vel(i,j,k,2) - vel(i,j,k-1,2)) * inv1dz;
                else
                    dw = NumericalDerivatives::weno3_deriv(vel(i,j,k-1,2), vel(i,j,k,2), vel(i,j,k+1,2), dz_cap_div);
            } else {
                if (k <= klo+1)
                    dw = (vel(i,j,k+1,2) - vel(i,j,k,2)) * inv1dz;
                else if (k >= khi-1)
                    dw = (vel(i,j,k,2) - vel(i,j,k-1,2)) * inv1dz;
                else
                    dw = NumericalDerivatives::weno5_deriv(vel(i,j,k-2,2), vel(i,j,k-1,2), vel(i,j,k,2), 
                                    vel(i,j,k+1,2), vel(i,j,k+2,2), dz_cap_div);
            }

            rh(i, j, k) = -(du + dv + dw);
            
            if (use_div_source) {
                rh(i, j, k) += div_source_const;
            }
            
            if (use_buoyancy_rhs && n_temp_pts_rhs > 0) {
                Real T_local = T_ref_rhs;
                if (n_temp_pts_rhs == 1) {
                    T_local = d_temp_T_ptr[0];
                } else if (z_physical <= d_temp_z_ptr[0]) {
                    T_local = d_temp_T_ptr[0];
                } else if (z_physical >= d_temp_z_ptr[n_temp_pts_rhs - 1]) {
                    T_local = d_temp_T_ptr[n_temp_pts_rhs - 1];
                } else {
                    for (int m = 0; m < n_temp_pts_rhs - 1; ++m) {
                        if (z_physical >= d_temp_z_ptr[m] && 
                            z_physical <= d_temp_z_ptr[m + 1]) {
                            T_local = temperature_linear_interp(
                                z_physical,
                                d_temp_z_ptr[m], d_temp_T_ptr[m],
                                d_temp_z_ptr[m + 1], d_temp_T_ptr[m + 1]);
                            break;
                        }
                    }
                }
                rh(i, j, k) += buoyancy_rhs_term(T_local, T_ref_rhs, buoy_coeff_rhs);
            }
        });
    }

    amrex::Print() << "wind_solver: RHS computation time = " 
                   << (amrex::second() - t_phase) << " s\n";

    t_phase = amrex::second();
    LPInfo info;
    info.setAgglomeration(true);
    info.setConsolidation(true);

    MLABecLaplacian mlabec({*geom_ptr}, {*ba_ptr}, {*dm_ptr}, info);
    mlabec.setMaxOrder(2);

    Array<LinOpBCType, AMREX_SPACEDIM> lo_bc, hi_bc;
    lo_bc[0] = LinOpBCType::Dirichlet;
    hi_bc[0] = LinOpBCType::Dirichlet;
    lo_bc[1] = LinOpBCType::Neumann;
    hi_bc[1] = LinOpBCType::Neumann;
    lo_bc[2] = LinOpBCType::Neumann;
    hi_bc[2] = LinOpBCType::Neumann;
    mlabec.setDomainBC(lo_bc, hi_bc);

    mlabec.setScalars(0.0, 1.0);

    MultiFab acoef(*ba_ptr, *dm_ptr, 1, 0);
    acoef.setVal(0.0);
    mlabec.setACoeffs(0, acoef);

    const Real bh = alpha_h * alpha_h;
    const Real bv = alpha_v * alpha_v;
    Array<MultiFab, AMREX_SPACEDIM> bcoef;
    bcoef[0].define(convert(*ba_ptr, IntVect(1, 0, 0)), *dm_ptr, 1, 0);
    bcoef[1].define(convert(*ba_ptr, IntVect(0, 1, 0)), *dm_ptr, 1, 0);
    bcoef[2].define(convert(*ba_ptr, IntVect(0, 0, 1)), *dm_ptr, 1, 0);
    
    if (enable_cell_local_anisotropy) {
        if (z_temp.empty()) {
            amrex::Print() << "wind_solver: WARNING: Cell-local anisotropy is enabled but no temperature profile is provided. Skipping/disabling cell-local anisotropy.\n";
            enable_cell_local_anisotropy = false;
            if (alpha_coefficients_file.empty()) {
                use_spatial_alpha_coefficients = false;
            }
        } else {
            amrex::Print() << "wind_solver: computing cell-local spatially-varying variational anisotropy\n";
            CellLocalAnisotropy::compute_cell_local_anisotropy_fields(
                *alpha_h_field_ptr,
                *alpha_v_field_ptr,
                *vel0_ptr,
                *temp_ptr,
                d_obstacle_h.data(),
                nx, ny, nz,
                dx, dy, dz,
                alpha_h,
                alpha_v,
                zs_min,
                enable_cell_local_anisotropy,
                anisotropy_source,
                anisotropy_slope_scale,
                anisotropy_decay_height,
                anisotropy_ri_gamma,
                anisotropy_ri_beta,
                anisotropy_fr_min
            );
            
            alpha_h_field_ptr->FillBoundary(geom_ptr->periodicity());
            alpha_v_field_ptr->FillBoundary(geom_ptr->periodicity());
        }
    }

    if (use_spatial_alpha_coefficients && (!alpha_h_data.empty() || enable_cell_local_anisotropy)) {
        amrex::Print() << "wind_solver: using spatially-varying Lagrange coefficients in Poisson solver\n";
        
        const int nx_val = nx;
        const int ny_val = ny;
        const int nz_val = nz;
        for (MFIter mfi(bcoef[0]); mfi.isValid(); ++mfi) {
            const Box& bx_x = mfi.validbox();
            const Box& bx_y = convert(mfi.validbox(), IntVect(0, 1, 0));
            const Box& bx_z = convert(mfi.validbox(), IntVect(0, 0, 1));
            
            auto bx_arr = bcoef[0].array(mfi);
            auto by_arr = bcoef[1].array(mfi);
            auto bz_arr = bcoef[2].array(mfi);
            auto ah_arr = alpha_h_field_ptr->const_array(mfi);
            auto av_arr = alpha_v_field_ptr->const_array(mfi);
            
            amrex::ParallelFor(bx_x,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                int left_idx = std::max(0, i - 1);
                int right_idx = std::min(nx_val - 1, i);
                Real ah_left = ah_arr(left_idx, j, k);
                Real ah_right = ah_arr(right_idx, j, k);
                Real ah_avg = Real(0.5) * (ah_left + ah_right);
                bx_arr(i, j, k) = ah_avg * ah_avg;
            });
            
            amrex::ParallelFor(bx_y,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                int bottom_idx = std::max(0, j - 1);
                int top_idx = std::min(ny_val - 1, j);
                Real ah_bottom = ah_arr(i, bottom_idx, k);
                Real ah_top = ah_arr(i, top_idx, k);
                Real ah_avg = Real(0.5) * (ah_bottom + ah_top);
                by_arr(i, j, k) = ah_avg * ah_avg;
            });
            
            amrex::ParallelFor(bx_z,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                int below_idx = std::max(0, k - 1);
                int above_idx = std::min(nz_val - 1, k);
                Real av_below = av_arr(i, j, below_idx);
                Real av_above = av_arr(i, j, above_idx);
                Real av_avg = Real(0.5) * (av_below + av_above);
                bz_arr(i, j, k) = av_avg * av_avg;
            });
        }
    } else {
        bcoef[0].setVal(bh);
        bcoef[1].setVal(bh);
        
        if (use_height_dependent_alpha_v) {
            amrex::Print() << "wind_solver: using height-dependent alpha_v\n";
            amrex::Print() << "  alpha_v_surface = " << alpha_v_surface << "\n";
            amrex::Print() << "  alpha_v_top = " << alpha_v_top << "\n";
            
            const Real alpha_v_surf_sq = alpha_v_surface * alpha_v_surface;
            const Real alpha_v_top_sq = alpha_v_top * alpha_v_top;
            const Real z_lo_alphav = zs_min;
            const Real z_hi_alphav = obs_max + domain_height;
            const Real dz_alphav = dz;
            
            for (MFIter mfi(bcoef[2]); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                auto bz_arr = bcoef[2].array(mfi);
                
                amrex::ParallelFor(bx,
                    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
                {
                    Real z_face = z_lo_alphav + k * dz_alphav;
                    Real z_frac = (z_face - z_lo_alphav) / (z_hi_alphav - z_lo_alphav);
                    z_frac = std::max(Real(0.0), std::min(Real(1.0), z_frac));
                    
                    Real alpha_v_sq = alpha_v_surf_sq + (alpha_v_top_sq - alpha_v_surf_sq) * z_frac;
                    bz_arr(i, j, k) = alpha_v_sq;
                });
            }
        } else {
            bcoef[2].setVal(bv);
        }
    }
    mlabec.setBCoeffs(0, GetArrOfConstPtrs(bcoef));

    mlabec.setLevelBC(0, nullptr);

    amrex::Print() << "wind_solver: Poisson operator setup time = " 
                   << (amrex::second() - t_phase) << " s\n";

    t_phase = amrex::second();
    MLMG mlmg(mlabec);
    mlmg.setMaxIter(mlmg_max_iter);
    mlmg.setMaxFmgIter(mlmg_max_fmg_iter);
    mlmg.setVerbose(mlmg_verbose);
    mlmg.setBottomVerbose(0);
    mlmg.setPreSmooth(mlmg_pre_smooth);
    mlmg.setPostSmooth(mlmg_post_smooth);
    
    if (mlmg_bottom_solver == "bicgstab") {
        mlmg.setBottomSolver(MLMG::BottomSolver::bicgstab);
        amrex::Print() << "wind_solver: using BiCGStab bottom solver\n";
    } else if (mlmg_bottom_solver == "cg") {
        mlmg.setBottomSolver(MLMG::BottomSolver::cg);
        amrex::Print() << "wind_solver: using CG bottom solver\n";
    } else if (mlmg_bottom_solver == "smoother") {
        mlmg.setBottomSolver(MLMG::BottomSolver::smoother);
        amrex::Print() << "wind_solver: using smoother-only bottom solver\n";
    }

    lam_ptr->setVal(0.0);

    amrex::Print() << "wind_solver: starting MLMG Poisson solve...\n";
    mlmg.solve({lam_ptr.get()}, {rhs_ptr.get()}, tol_rel, Real(0.0));
    amrex::Print() << "wind_solver: MLMG solve complete.\n";
    amrex::Print() << "wind_solver: Poisson solve time = " 
                   << (amrex::second() - t_phase) << " s\n";

    lam_ptr->FillBoundary(geom_ptr->periodicity());

    if (enable_divergence_damping) {
        amrex::Print() << "wind_solver: applying divergence damping filter...\n";
        t_phase = amrex::second();
        
        Real damp_coeff_h = damping_coefficient_h;
        Real damp_coeff_v = damping_coefficient_v;
        
        if (damp_coeff_h < Real(0.0)) {
            if (damping_coefficient >= Real(0.0)) {
                damp_coeff_h = damping_coefficient;
            } else {
                Real h_spacing = std::min(dx, dy);
                damp_coeff_h = Real(0.05) * h_spacing * h_spacing;
            }
        }
        if (damp_coeff_v < Real(0.0)) {
            if (damping_coefficient >= Real(0.0)) {
                damp_coeff_v = damping_coefficient;
            } else {
                damp_coeff_v = Real(0.05) * dz * dz;
            }
        }
        
        amrex::Print() << "  damping_coefficient_h = " << damp_coeff_h << " m^2/s\n";
        amrex::Print() << "  damping_coefficient_v = " << damp_coeff_v << " m^2/s\n";
        amrex::Print() << "  damping_iterations = " << damping_iterations << "\n";
        
        const Real inv_dx2 = Real(1.0) / (dx * dx);
        const Real inv_dy2 = Real(1.0) / (dy * dy);
        const Real inv_dz2 = Real(1.0) / (dz * dz);
        
        for (int iter = 0; iter < damping_iterations; ++iter) {
            for (MFIter mfi(*lam_ptr); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                const auto lam_arr = lam_ptr->const_array(mfi);
                auto lambda_damp_arr = lambda_damped_ptr->array(mfi);
                
                amrex::ParallelFor(bx,
                    [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
                {
                    Real lambda_val = lam_arr(i, j, k);
                    
                    Real f_xx = (lam_arr(i+1, j, k) - Real(2.0) * lambda_val + lam_arr(i-1, j, k)) * inv_dx2;
                    Real f_yy = (lam_arr(i, j+1, k) - Real(2.0) * lambda_val + lam_arr(i, j-1, k)) * inv_dy2;
                    Real f_zz = (lam_arr(i, j, k+1) - Real(2.0) * lambda_val + lam_arr(i, j, k-1)) * inv_dz2;
                    
                    lambda_damp_arr(i, j, k) = lambda_val - damp_coeff_h * (f_xx + f_yy) - damp_coeff_v * f_zz;
                });
            }
            
            MultiFab::Copy(*lam_ptr, *lambda_damped_ptr, 0, 0, 1, 0);
            lam_ptr->FillBoundary(geom_ptr->periodicity());
        }
        
        amrex::Print() << "wind_solver: divergence damping time = " 
                       << (amrex::second() - t_phase) << " s\n";
    }

    if (enable_perturbation_pressure) {
        amrex::Print() << "wind_solver: setting up perturbation pressure solve...\n";
        t_phase = amrex::second();
        p_prime_ptr->setVal(0.0);
        amrex::Print() << "wind_solver: perturbation pressure initialization time = " 
                       << (amrex::second() - t_phase) << " s\n";
    }
}

void WindSolverApp::apply_divergence_corrections(int time_step) {
    t_phase = amrex::second();
    const Box domain = geom_ptr->Domain();
    const IntVect glo = domain.smallEnd();
    const IntVect ghi = domain.bigEnd();
    const int ilo = glo[0], ihi = ghi[0];
    const int jlo = glo[1], jhi = ghi[1];
    const int klo = glo[2], khi = ghi[2];
    const Real inv2dx = Real(0.5) / dx;
    const Real inv2dy = Real(0.5) / dy;
    const Real inv2dz = Real(0.5) / dz;
    const Real inv1dx = Real(1.0) / dx;
    const Real inv1dy = Real(1.0) / dy;
    const Real inv1dz = Real(1.0) / dz;

    const int deriv_method_cap = deriv_method_int;
    const Real dx_cap = dx;
    const Real dy_cap = dy;
    const Real dz_cap_div = dz;
    const Real z_lo_cap_div = zs_min;
    const int  nx_cap_div   = nx;
    const Real* d_terr_ptr = d_obstacle_h.data();
    
    const Real bh = alpha_h * alpha_h;
    const Real bv = alpha_v * alpha_v;

    const bool cap_enable_capping_lid = enable_capping_lid;
    const Real cap_capping_lid_height = capping_lid_height;
    const bool local_use_spatial_alpha_coefficients = use_spatial_alpha_coefficients;

    const bool use_eb = enable_eb;
    const Real eb_thresh = eb_threshold;
    const amrex::MultiFab* vfrac_ptr = enable_eb ? &(eb_factory->getVolFrac()) : nullptr;

    for (MFIter mfi(*vel_c_ptr); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        const auto v0  = vel0_ptr->const_array(mfi);
        const auto la  = lam_ptr->const_array(mfi);
        auto       vc  = vel_c_ptr->array(mfi);
        const auto z_bl_arr = z_bl_diag_ptr->const_array(mfi);
        const auto ah_arr = alpha_h_field_ptr->const_array(mfi);
        const auto av_arr = alpha_v_field_ptr->const_array(mfi);
        const auto vfrac_arr = use_eb ? vfrac_ptr->const_array(mfi) : Array4<Real const>{};

        amrex::ParallelFor(bx,
            [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
        {
            Real z_physical = z_lo_cap_div + (k + Real(0.5)) * dz_cap_div;
            Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_div + i];
            
            bool is_solid = (z_agl <= Real(0.0));
            if (use_eb) {
                is_solid = is_solid || (vfrac_arr(i, j, k) < eb_thresh);
            }
            if (is_solid) {
                vc(i, j, k, 0) = Real(0.0);
                vc(i, j, k, 1) = Real(0.0);
                vc(i, j, k, 2) = Real(0.0);
                return;
            }

            Real dlx, dly, dlz;
            
            if (deriv_method_cap == 0) {
                if (i == ilo)
                    dlx = (la(i+1,j,k) - la(i,j,k)) * inv1dx;
                else if (i == ihi)
                    dlx = (la(i,j,k) - la(i-1,j,k)) * inv1dx;
                else
                    dlx = (la(i+1,j,k) - la(i-1,j,k)) * inv2dx;
            } else if (deriv_method_cap == 1) {
                if (i == ilo)
                    dlx = (la(i+1,j,k) - la(i,j,k)) * inv1dx;
                else if (i == ihi)
                    dlx = (la(i,j,k) - la(i-1,j,k)) * inv1dx;
                else
                    dlx = NumericalDerivatives::weno3_deriv(la(i-1,j,k), la(i,j,k), la(i+1,j,k), dx_cap);
            } else {
                if (i <= ilo+1)
                    dlx = (la(i+1,j,k) - la(i,j,k)) * inv1dx;
                else if (i >= ihi-1)
                    dlx = (la(i,j,k) - la(i-1,j,k)) * inv1dx;
                else
                    dlx = NumericalDerivatives::weno5_deriv(la(i-2,j,k), la(i-1,j,k), la(i,j,k), 
                                     la(i+1,j,k), la(i+2,j,k), dx_cap);
            }

            if (deriv_method_cap == 0) {
                if (j == jlo)
                    dly = (la(i,j+1,k) - la(i,j,k)) * inv1dy;
                else if (j == jhi)
                    dly = (la(i,j,k) - la(i,j-1,k)) * inv1dy;
                else
                    dly = (la(i,j+1,k) - la(i,j-1,k)) * inv2dy;
            } else if (deriv_method_cap == 1) {
                if (j == jlo)
                    dly = (la(i,j+1,k) - la(i,j,k)) * inv1dy;
                else if (j == jhi)
                    dly = (la(i,j,k) - la(i,j-1,k)) * inv1dy;
                else
                    dly = NumericalDerivatives::weno3_deriv(la(i,j-1,k), la(i,j,k), la(i,j+1,k), dy_cap);
            } else {
                if (j <= jlo+1)
                    dly = (la(i,j+1,k) - la(i,j,k)) * inv1dy;
                else if (j >= jhi-1)
                    dly = (la(i,j,k) - la(i,j-1,k)) * inv1dy;
                else
                    dly = NumericalDerivatives::weno5_deriv(la(i,j-2,k), la(i,j-1,k), la(i,j,k), 
                                     la(i,j+1,k), la(i,j+2,k), dy_cap);
            }

            if (deriv_method_cap == 0) {
                if (k == klo)
                    dlz = (la(i,j,k+1) - la(i,j,k)) * inv1dz;
                else if (k == khi)
                    dlz = (la(i,j,k) - la(i,j,k-1)) * inv1dz;
                else
                    dlz = (la(i,j,k+1) - la(i,j,k-1)) * inv2dz;
            } else if (deriv_method_cap == 1) {
                if (k == klo)
                    dlz = (la(i,j,k+1) - la(i,j,k)) * inv1dz;
                else if (k == khi)
                    dlz = (la(i,j,k) - la(i,j,k-1)) * inv1dz;
                else
                    dlz = NumericalDerivatives::weno3_deriv(la(i,j,k-1), la(i,j,k), la(i,j,k+1), dz_cap_div);
            } else {
                if (k <= klo+1)
                    dlz = (la(i,j,k+1) - la(i,j,k)) * inv1dz;
                else if (k >= khi-1)
                    dlz = (la(i,j,k) - la(i,j,k-1)) * inv1dz;
                else
                    dlz = NumericalDerivatives::weno5_deriv(la(i,j,k-2), la(i,j,k-1), la(i,j,k), 
                                     la(i,j,k+1), la(i,j,k+2), dz_cap_div);
            }

            Real local_bh = bh;
            Real local_bv = bv;
            if (local_use_spatial_alpha_coefficients) {
                Real local_ah = ah_arr(i, j, k);
                Real local_av = av_arr(i, j, k);
                local_bh = local_ah * local_ah;
                local_bv = local_av * local_av;
            }

            vc(i, j, k, 0) = v0(i, j, k, 0) - local_bh * dlx;
            vc(i, j, k, 1) = v0(i, j, k, 1) - local_bh * dly;
            vc(i, j, k, 2) = v0(i, j, k, 2) - local_bv * dlz;
            
            Real local_capping_lid_height = (z_bl_arr(i, j, k) > Real(0.0)) ? z_bl_arr(i, j, k) : cap_capping_lid_height;
            if (cap_enable_capping_lid && z_agl >= local_capping_lid_height) {
                vc(i, j, k, 2) = Real(0.0);
            }
        });
    }

    amrex::Print() << "wind_solver: velocity correction time = " 
                   << (amrex::second() - t_phase) << " s\n";

    synthetic_turbulence_fluc_ptr = std::make_unique<MultiFab>(*ba_ptr, *dm_ptr, 3, 0);
    synthetic_turbulence_fluc_ptr->setVal(0.0);
    has_synthetic_turbulence = false;

    if (enable_synthetic_turbulence) {
        amrex::Print() << "wind_solver: generating synthetic turbulence field...\n";
        amrex::Real t_turb_start = amrex::second();

        std::string turbulence_output_file_ts = turbulence_output_file;
        if (num_time_steps > 1) {
            size_t dot_pos = turbulence_output_file.find_last_of('.');
            std::string base = (dot_pos != std::string::npos) ? 
                               turbulence_output_file.substr(0, dot_pos) : turbulence_output_file;
            std::string ext = (dot_pos != std::string::npos) ? 
                              turbulence_output_file.substr(dot_pos) : ".bts";
            std::ostringstream fname;
            fname << base << "_t" << time_step << ext;
            turbulence_output_file_ts = fname.str();
        }

        const auto& turb_ba = vel_c_ptr->boxArray();
        const Box turb_domain = amrex::grow(turb_ba.minimalBox(), 0) & geom_ptr->Domain();

        const int turb_nx = turb_domain.length(0);
        const int turb_ny = turb_domain.length(1);
        const int turb_nz = turb_domain.length(2);

        const Real z_agl_ref = std::max(z_ref, SyntheticTurbulence::Constants::z_min);
        const Real U_mean = std::max(std::hypot(U_ref, V_ref), Real(0.1));
        const Real total_duration = TemporalSynthesis::DEFAULT_DURATION;
        const Real custom_dt = Real(0.0);
        const unsigned int seed = turb_params.random_seed;

        SyntheticTurbulence::TurbulenceGenerator turb_gen(turb_params);
        RandomFieldSynthesis::SpectralAmplitudeEngine spectral_engine;
        RandomFieldSynthesis::RandomFieldGenerator field_gen(seed);
        auto spectrum = spectral_engine.BuildAmplitudeSpectrum(turb_gen, z_agl_ref, U_mean);
        auto random_fields = field_gen.Generate3DField(
            spectrum, turb_nx, turb_ny, turb_nz, dx, dy, dz, true, turb_gen);

        TemporalSynthesis::TimeSeriesGenerator ts_gen;
        auto time_series = ts_gen.GenerateTimeSeries(
            random_fields.u_prime, random_fields.v_prime, random_fields.w_prime,
            turb_nx, turb_ny, turb_nz, U_mean, turb_gen, total_duration, custom_dt, seed);

        Gpu::DeviceVector<Real> d_u_prime(random_fields.u_prime.size());
        Gpu::DeviceVector<Real> d_v_prime(random_fields.v_prime.size());
        Gpu::DeviceVector<Real> d_w_prime(random_fields.w_prime.size());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                         random_fields.u_prime.begin(), random_fields.u_prime.end(), d_u_prime.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                         random_fields.v_prime.begin(), random_fields.v_prime.end(), d_v_prime.begin());
        amrex::Gpu::copy(amrex::Gpu::hostToDevice,
                         random_fields.w_prime.begin(), random_fields.w_prime.end(), d_w_prime.begin());
        Real const* d_u_prime_ptr = d_u_prime.data();
        Real const* d_v_prime_ptr = d_v_prime.data();
        Real const* d_w_prime_ptr = d_w_prime.data();
        const int n_fluc = static_cast<int>(random_fields.u_prime.size());

        const bool cap_enable_terrain_aware_masking = enable_terrain_aware_masking;
        const Real cap_terrain_mask_transition_height = terrain_mask_transition_height;
        const Real cap_dz = dz;
        const Real cap_zs_min = zs_min;
        const int cap_nx = nx;
        const Real* d_terr_ptr_loc = d_obstacle_h.data();

        const bool use_eb = enable_eb;
        const Real eb_thresh = eb_threshold;
        const amrex::MultiFab* vfrac_ptr = enable_eb ? &(eb_factory->getVolFrac()) : nullptr;

        for (MFIter mfi(*synthetic_turbulence_fluc_ptr); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto fluc = synthetic_turbulence_fluc_ptr->array(mfi);
            const auto vfrac_arr = use_eb ? vfrac_ptr->const_array(mfi) : Array4<Real const>{};
            
            amrex::ParallelFor(bx,
                [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
            {
                int idx_1d = (k * turb_ny + j) * turb_nx + i;
                if (idx_1d >= 0 && idx_1d < n_fluc) {
                    Real u_val = d_u_prime_ptr[idx_1d];
                    Real v_val = d_v_prime_ptr[idx_1d];
                    Real w_val = d_w_prime_ptr[idx_1d];

                    if (cap_enable_terrain_aware_masking) {
                        Real z_physical = cap_zs_min + (k + Real(0.5)) * cap_dz;
                        Real z_agl      = z_physical - d_terr_ptr_loc[j * cap_nx + i];
                        Real mask;
                        bool is_solid = (z_agl <= Real(0.0));
                        if (use_eb) {
                            is_solid = is_solid || (vfrac_arr(i, j, k) < eb_thresh);
                        }
                        if (is_solid) {
                            mask = Real(0.0);
                        } else if (z_agl >= cap_terrain_mask_transition_height) {
                            mask = Real(1.0);
                        } else {
                            Real transition_angle = MathConstants::pi * z_agl / cap_terrain_mask_transition_height;
                            mask = (Real(1.0) - std::cos(transition_angle)) / Real(2.0);
                        }
                        u_val *= mask;
                        v_val *= mask;
                        w_val *= mask;
                    }

                    fluc(i,j,k,0) = u_val;
                    fluc(i,j,k,1) = v_val;
                    fluc(i,j,k,2) = w_val;
                } else {
                    fluc(i,j,k,0) = 0.0;
                    fluc(i,j,k,1) = 0.0;
                    fluc(i,j,k,2) = 0.0;
                }
            });
        }
        has_synthetic_turbulence = true;

        const int nt = time_series.num_time_steps;
        const Real dt_turb = time_series.metadata.dt;
        const Real z_hub = z_ref;
        const Real intensity_u = turb_gen.ComputeIntensity(z_agl_ref);
        const Real expected_u_rms = turb_gen.ComputeVelocityRmsU(z_agl_ref, U_mean);
        const Real expected_v_rms = turb_gen.ComputeVelocityRmsV(z_agl_ref, U_mean);
        const Real expected_w_rms = turb_gen.ComputeVelocityRmsW(z_agl_ref, U_mean);

        auto normalize_component = [] (std::vector<Real>& component, Real target_rms) {
            const Real actual_rms = Phase3Validation::ComputeRMS(component);
            if (actual_rms > Real(1.0e-12)) {
                const Real scale = target_rms / actual_rms;
                for (auto& value : component) {
                    value *= scale;
                }
            }
        };

        normalize_component(time_series.u_prime_time_series, expected_u_rms);
        normalize_component(time_series.v_prime_time_series, expected_v_rms);
        normalize_component(time_series.w_prime_time_series, expected_w_rms);
        for (std::size_t idx = 0; idx < time_series.u_prime_time_series.size(); ++idx) {
            time_series.v_prime_time_series[idx] =
                time_series.u_prime_time_series[idx] * turb_params.anisotropy_ratio_v;
            time_series.w_prime_time_series[idx] =
                time_series.u_prime_time_series[idx] * turb_params.anisotropy_ratio_w;
        }

        TurbSimExport::TurbSimBTSWriter bts_writer;
        bts_writer.Initialize(nt, turb_nx, turb_ny, turb_nz, dt_turb, U_mean,
                              dx, dy, dz, z_hub, intensity_u, seed);
        bool success = bts_writer.ExportTimeSeries(
            turbulence_output_file_ts,
            time_series.u_prime_time_series,
            time_series.v_prime_time_series,
            time_series.w_prime_time_series,
            turb_nx, turb_ny, turb_nz, nt);

        if (!success) {
            amrex::Print() << "WARNING: BTS export failed for "
                           << turbulence_output_file_ts << "\n";
        }

        const Real expected_timescale =
            TemporalSynthesis::ComputeIntegralTimescale(turb_params.length_scale_u, U_mean);

        Phase3Validation::ValidationSuite validation_suite;
        bool validation_passed = validation_suite.RunFullValidation(
            time_series.u_prime_time_series,
            time_series.v_prime_time_series,
            time_series.w_prime_time_series,
            turb_nx, turb_ny, turb_nz, nt, dt_turb,
            expected_u_rms, expected_v_rms, expected_w_rms, expected_timescale,
            turb_params.anisotropy_ratio_v, turb_params.anisotropy_ratio_w);

        amrex::Print() << "wind_solver: turbulence validation summary\n"
                       << validation_suite.GetSummary();
        if (!validation_passed) {
            amrex::Print() << "WARNING: Validation failed:\n";
            amrex::Print() << validation_suite.GetSummary();
        }

        amrex::Real t_turb_end = amrex::second();
        amrex::Print() << "wind_solver: turbulence generation time = "
                       << (t_turb_end - t_turb_start) << " s\n";
    }
}

void WindSolverApp::compute_diagnostics_and_output(int time_step) {
    t_phase = amrex::second();
    const Box domain = geom_ptr->Domain();
    const IntVect glo = domain.smallEnd();
    const IntVect ghi = domain.bigEnd();
    const int ilo = glo[0], ihi = ghi[0];
    const int jlo = glo[1], jhi = ghi[1];
    const int klo = glo[2], khi = ghi[2];
    const Real inv2dx = Real(0.5) / dx;
    const Real inv2dy = Real(0.5) / dy;
    const Real inv2dz = Real(0.5) / dz;
    const Real inv1dx = Real(1.0) / dx;
    const Real inv1dy = Real(1.0) / dy;
    const Real inv1dz = Real(1.0) / dz;
    
    const int deriv_method_cap = deriv_method_int;
    const Real dx_cap = dx;
    const Real dy_cap = dy;
    const Real dz_cap_div = dz;
    const Real z_lo_cap_div = zs_min;
    const int  nx_cap_div   = nx;
    const Real* d_terr_ptr = d_obstacle_h.data();

    MultiFab div_before(*ba_ptr, *dm_ptr, 1, 0);
    MultiFab div_after (*ba_ptr, *dm_ptr, 1, 0);

    MultiFab vel_c_g(*ba_ptr, *dm_ptr, 3, 1);
    MultiFab::Copy(vel_c_g, *vel_c_ptr, 0, 0, 3, 0);
    vel_c_g.FillBoundary(geom_ptr->periodicity());

    const bool use_eb = enable_eb;
    const Real eb_thresh = eb_threshold;
    const amrex::MultiFab* vfrac_ptr = enable_eb ? &(eb_factory->getVolFrac()) : nullptr;

    for (MFIter mfi(div_before); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        const auto v0b = vel0_ptr->const_array(mfi);
        const auto vcg = vel_c_g.const_array(mfi);
        auto db = div_before.array(mfi);
        auto da = div_after .array(mfi);
        const auto vfrac_arr = use_eb ? vfrac_ptr->const_array(mfi) : Array4<Real const>{};

        amrex::ParallelFor(bx,
            [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
        {
            Real z_physical = z_lo_cap_div + (k + Real(0.5)) * dz_cap_div;
            Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_div + i];

            // --- divergence before ---
            Real du_b, dv_b, dw_b;
            
            if (deriv_method_cap == 0) {
                if (i == ilo) du_b = (v0b(i+1,j,k,0)-v0b(i,j,k,0))*inv1dx;
                else if (i == ihi) du_b = (v0b(i,j,k,0)-v0b(i-1,j,k,0))*inv1dx;
                else du_b = (v0b(i+1,j,k,0)-v0b(i-1,j,k,0))*inv2dx;

                if (j == jlo) dv_b = (v0b(i,j+1,k,1)-v0b(i,j,k,1))*inv1dy;
                else if (j == jhi) dv_b = (v0b(i,j,k,1)-v0b(i,j-1,k,1))*inv1dy;
                else dv_b = (v0b(i,j+1,k,1)-v0b(i,j-1,k,1))*inv2dy;

                if (k == klo) dw_b = (v0b(i,j,k+1,2)-v0b(i,j,k,2))*inv1dz;
                else if (k == khi) dw_b = (v0b(i,j,k,2)-v0b(i,j,k-1,2))*inv1dz;
                else dw_b = (v0b(i,j,k+1,2)-v0b(i,j,k-1,2))*inv2dz;
            } else if (deriv_method_cap == 1) {
                if (i == ilo) du_b = (v0b(i+1,j,k,0)-v0b(i,j,k,0))*inv1dx;
                else if (i == ihi) du_b = (v0b(i,j,k,0)-v0b(i-1,j,k,0))*inv1dx;
                else du_b = NumericalDerivatives::weno3_deriv(v0b(i-1,j,k,0), v0b(i,j,k,0), v0b(i+1,j,k,0), dx_cap);

                if (j == jlo) dv_b = (v0b(i,j+1,k,1)-v0b(i,j,k,1))*inv1dy;
                else if (j == jhi) dv_b = (v0b(i,j,k,1)-v0b(i,j-1,k,1))*inv1dy;
                else dv_b = NumericalDerivatives::weno3_deriv(v0b(i,j-1,k,1), v0b(i,j,k,1), v0b(i,j+1,k,1), dy_cap);

                if (k == klo) dw_b = (v0b(i,j,k+1,2)-v0b(i,j,k,2))*inv1dz;
                else if (k == khi) dw_b = (v0b(i,j,k,2)-v0b(i,j,k-1,2))*inv1dz;
                else dw_b = NumericalDerivatives::weno3_deriv(v0b(i,j,k-1,2), v0b(i,j,k,2), v0b(i,j,k+1,2), dz_cap_div);
            } else {
                if (i <= ilo+1) du_b = (v0b(i+1,j,k,0)-v0b(i,j,k,0))*inv1dx;
                else if (i >= ihi-1) du_b = (v0b(i,j,k,0)-v0b(i-1,j,k,0))*inv1dx;
                else du_b = NumericalDerivatives::weno5_deriv(v0b(i-2,j,k,0), v0b(i-1,j,k,0), v0b(i,j,k,0),
                                       v0b(i+1,j,k,0), v0b(i+2,j,k,0), dx_cap);

                if (j <= jlo+1) dv_b = (v0b(i,j+1,k,1)-v0b(i,j,k,1))*inv1dy;
                else if (j >= jhi-1) dv_b = (v0b(i,j,k,1)-v0b(i,j-1,k,1))*inv1dy;
                else dv_b = NumericalDerivatives::weno5_deriv(v0b(i,j-2,k,1), v0b(i,j-1,k,1), v0b(i,j,k,1),
                                       v0b(i,j+1,k,1), v0b(i,j+2,k,1), dy_cap);

                if (k <= klo+1) dw_b = (v0b(i,j,k+1,2)-v0b(i,j,k,2))*inv1dz;
                else if (k >= khi-1) dw_b = (v0b(i,j,k,2)-v0b(i,j,k-1,2))*inv1dz;
                else dw_b = NumericalDerivatives::weno5_deriv(v0b(i,j,k-2,2), v0b(i,j,k-1,2), v0b(i,j,k,2),
                                       v0b(i,j,k+1,2), v0b(i,j,k+2,2), dz_cap_div);
            }

            bool is_solid = (z_agl <= Real(0.0));
            if (use_eb) {
                is_solid = is_solid || (vfrac_arr(i, j, k) < eb_thresh);
            }
            db(i,j,k) = is_solid ? Real(0.0) : (du_b+dv_b+dw_b);

            // --- divergence after ---
            Real du_a, dv_a, dw_a;
            
            if (deriv_method_cap == 0) {
                if (i == ilo) du_a = (vcg(i+1,j,k,0)-vcg(i,j,k,0))*inv1dx;
                else if (i == ihi) du_a = (vcg(i,j,k,0)-vcg(i-1,j,k,0))*inv1dx;
                else du_a = (vcg(i+1,j,k,0)-vcg(i-1,j,k,0))*inv2dx;

                if (j == jlo) dv_a = (vcg(i,j+1,k,1)-vcg(i,j,k,1))*inv1dy;
                else if (j == jhi) dv_a = (vcg(i,j,k,1)-vcg(i,j-1,k,1))*inv1dy;
                else dv_a = (vcg(i,j+1,k,1)-vcg(i,j-1,k,1))*inv2dy;

                if (k == klo) dw_a = (vcg(i,j,k+1,2)-vcg(i,j,k,2))*inv1dz;
                else if (k == khi) dw_a = (vcg(i,j,k,2)-vcg(i,j,k-1,2))*inv1dz;
                else dw_a = (vcg(i,j,k+1,2)-vcg(i,j,k-1,2))*inv2dz;
            } else if (deriv_method_cap == 1) {
                if (i == ilo) du_a = (vcg(i+1,j,k,0)-vcg(i,j,k,0))*inv1dx;
                else if (i == ihi) du_a = (vcg(i,j,k,0)-vcg(i-1,j,k,0))*inv1dx;
                else du_a = NumericalDerivatives::weno3_deriv(vcg(i-1,j,k,0), vcg(i,j,k,0), vcg(i+1,j,k,0), dx_cap);

                if (j == jlo) dv_a = (vcg(i,j+1,k,1)-vcg(i,j,k,1))*inv1dy;
                else if (j == jhi) dv_a = (vcg(i,j,k,1)-vcg(i,j-1,k,1))*inv1dy;
                else dv_a = NumericalDerivatives::weno3_deriv(vcg(i,j-1,k,1), vcg(i,j,k,1), vcg(i,j+1,k,1), dy_cap);

                if (k == klo) dw_a = (vcg(i,j,k+1,2)-vcg(i,j,k,2))*inv1dz;
                else if (k == khi) dw_a = (vcg(i,j,k,2)-vcg(i,j,k-1,2))*inv1dz;
                else dw_a = NumericalDerivatives::weno3_deriv(vcg(i,j,k-1,2), vcg(i,j,k,2), vcg(i,j,k+1,2), dz_cap_div);
            } else {
                if (i <= ilo+1) du_a = (vcg(i+1,j,k,0)-vcg(i,j,k,0))*inv1dx;
                else if (i >= ihi-1) du_a = (vcg(i,j,k,0)-vcg(i-1,j,k,0))*inv1dx;
                else du_a = NumericalDerivatives::weno5_deriv(vcg(i-2,j,k,0), vcg(i-1,j,k,0), vcg(i,j,k,0),
                                       vcg(i+1,j,k,0), vcg(i+2,j,k,0), dx_cap);

                if (j <= jlo+1) dv_a = (vcg(i,j+1,k,1)-vcg(i,j,k,1))*inv1dy;
                else if (j >= jhi-1) dv_a = (vcg(i,j,k,1)-vcg(i,j-1,k,1))*inv1dy;
                else dv_a = NumericalDerivatives::weno5_deriv(vcg(i,j-2,k,1), vcg(i,j-1,k,1), vcg(i,j,k,1),
                                       vcg(i,j+1,k,1), vcg(i,j+2,k,1), dy_cap);

                if (k <= klo+1) dw_a = (vcg(i,j,k+1,2)-vcg(i,j,k,2))*inv1dz;
                else if (k >= khi-1) dw_a = (vcg(i,j,k,2)-vcg(i,j,k-1,2))*inv1dz;
                else dw_a = NumericalDerivatives::weno5_deriv(vcg(i,j,k-2,2), vcg(i,j,k-1,2), vcg(i,j,k,2),
                                       vcg(i,j,k+1,2), vcg(i,j,k+2,2), dz_cap_div);
            }

            da(i,j,k) = is_solid ? Real(0.0) : (du_a+dv_a+dw_a);
        });
    }

    Real div_b_max = div_before.norm0();
    Real div_a_max = div_after .norm0();
    amrex::Print() << "wind_solver: max |div(u)| before correction = "
                   << div_b_max << " s⁻¹\n";
    amrex::Print() << "wind_solver: max |div(u)| after  correction = "
                   << div_a_max << " s⁻¹\n";

    std::vector<Real> u_host;
    std::vector<Real> v_host;
    std::vector<Real> w_host;
    std::vector<Real> T_host;

    if (enable_wire_loading) {
        u_host.assign(static_cast<std::size_t>(nx) * ny * nz, 0.0);
        v_host.assign(static_cast<std::size_t>(nx) * ny * nz, 0.0);
        w_host.assign(static_cast<std::size_t>(nx) * ny * nz, 0.0);
        T_host.assign(static_cast<std::size_t>(nx) * ny * nz, temperature_reference);

        for (amrex::MFIter mfi(*vel_c_ptr, false); mfi.isValid(); ++mfi) {
            const amrex::Box& bx = mfi.validbox();
#ifdef AMREX_USE_GPU
            amrex::FArrayBox host_fab(bx, vel_c_ptr->nComp(), amrex::The_Pinned_Arena());
            host_fab.copy<amrex::RunOn::Device>((*vel_c_ptr)[mfi], bx);
            amrex::Gpu::streamSynchronize();
            auto const& arr = host_fab.const_array();
#else
            auto const& arr = vel_c_ptr->const_array(mfi);
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

        if (!wires.empty()) {
            WireLoading::process_wire_loading_pregathered(
                wires, u_host, v_host, w_host, T_host,
                solar_radiation,
                x_lo, y_lo, zs_min,
                dx, dy, dz,
                nx, ny, nz
            );
            WireLoading::write_wire_output_file(wire_output_file, wires, time_step);
        }
    }

    // Process bridge loading if enabled
    if (enable_bridge_loading) {
        // Reuse or create host arrays for bridge processing
        if (u_host.empty()) {
            u_host.assign(static_cast<std::size_t>(nx) * ny * nz, 0.0);
            v_host.assign(static_cast<std::size_t>(nx) * ny * nz, 0.0);
            w_host.assign(static_cast<std::size_t>(nx) * ny * nz, 0.0);

            for (amrex::MFIter mfi(*vel_c_ptr, false); mfi.isValid(); ++mfi) {
                const amrex::Box& bx = mfi.validbox();
#ifdef AMREX_USE_GPU
                amrex::FArrayBox host_fab(bx, vel_c_ptr->nComp(), amrex::The_Pinned_Arena());
                host_fab.copy<amrex::RunOn::Device>((*vel_c_ptr)[mfi], bx);
                amrex::Gpu::streamSynchronize();
                auto const& arr = host_fab.const_array();
#else
                auto const& arr = vel_c_ptr->const_array(mfi);
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
        }

        if (!bridges.empty()) {
            BridgeLoading::process_bridge_loading_pregathered(
                bridges, u_host, v_host, w_host,
                x_lo, y_lo, zs_min,
                dx, dy, dz,
                nx, ny, nz
            );
            BridgeLoading::write_bridge_output_file(bridge_output_file, bridges, time_step);
        }
    }

    // Process general structure loading if enabled
    if (enable_structure_loading) {
        // Reuse or create host arrays for structure processing
        if (u_host.empty()) {
            u_host.assign(static_cast<std::size_t>(nx) * ny * nz, 0.0);
            v_host.assign(static_cast<std::size_t>(nx) * ny * nz, 0.0);
            w_host.assign(static_cast<std::size_t>(nx) * ny * nz, 0.0);

            for (amrex::MFIter mfi(*vel_c_ptr, false); mfi.isValid(); ++mfi) {
                const amrex::Box& bx = mfi.validbox();
#ifdef AMREX_USE_GPU
                amrex::FArrayBox host_fab(bx, vel_c_ptr->nComp(), amrex::The_Pinned_Arena());
                host_fab.copy<amrex::RunOn::Device>((*vel_c_ptr)[mfi], bx);
                amrex::Gpu::streamSynchronize();
                auto const& arr = host_fab.const_array();
#else
                auto const& arr = vel_c_ptr->const_array(mfi);
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
        }

        if (!structures.empty()) {
            StructureLoading::process_structure_loading_pregathered(
                structures, u_host, v_host, w_host,
                x_lo, y_lo, zs_min,
                dx, dy, dz,
                nx, ny, nz
            );
            StructureLoading::write_structure_output_file(structure_output_file, structures, time_step);
        }
    }

    int nout_val = 21;
    if (has_synthetic_turbulence) nout_val += 3;
    if (enable_coriolis_latitude) nout_val += 3;
    if (enable_wire_loading) nout_val += 2;
    const int nout = nout_val;
    const int nx_cap_out = nx;
    MultiFab output(*ba_ptr, *dm_ptr, nout, 0);

    const bool cap_enable_coriolis_latitude = enable_coriolis_latitude;
    const Real cap_domain_latitude = domain_latitude;
    const Real cap_y_lo = y_lo;
    const Real cap_dy = dy;
    const Real cap_y_center = y_lo + Real(0.5) * (y_hi - y_lo);
    const bool cap_has_turb = has_synthetic_turbulence;
    const bool cap_enable_terrain_analysis = enable_terrain_analysis;
    const bool use_pos_z0 = use_z0_file;
    const Real z0_cap = z0;
    const Real* d_z0_pos_ptr_diag = d_z0_pos.data();
    const bool enable_morph_val = enable_morphometric_models;
    const Real* d_morph_z0_ptr = d_morphometric_z0.data();

    int wire_idx_start = 21;
    if (has_synthetic_turbulence) wire_idx_start += 3;
    if (enable_coriolis_latitude) wire_idx_start += 3;
    const int cap_wire_idx_start = wire_idx_start;
    const bool cap_enable_wire_loading = enable_wire_loading;
    
    const Real rho_air = 1.225;
    const Real cp_air = 1005.0;
    const Real theta_star = 0.1;
    const Real kappa_diag = 0.41;
    const bool cap_enable_bl_depth_diagnostic = enable_bl_depth_diagnostic;
    const Real cap_bl_depth_param = bl_depth_param;
    const Real richardson_critical = this->richardson_critical;

    const bool enable_landuse_roughness_val = enable_landuse_roughness;
    const Real* d_landuse_pos_ptr_diag = d_landuse_pos.data();
    const Real charnock_alpha_val = charnock_alpha;

    const bool enable_marine_bl_val = enable_marine_bl;
    const Real marine_sst_val = marine_sst;
    const Real marine_air_sea_dt_val = marine_air_sea_dt;

    const bool cap_enable_flux_diagnostics = enable_flux_diagnostics;
    const Real cap_solar_radiation = solar_radiation;
    const Real cap_cloud_cover = cloud_cover;
    const Real cap_hour_of_day = (hour_of_day >= 0.0) ? hour_of_day : 12.0;  // Default to noon if not set
    const Real cap_day_of_year = (day_of_year > 0.0) ? day_of_year : 172.0;  // Default to summer solstice
    const Real cap_latitude = latitude_degrees;
    const Real cap_flux_theta_star = flux_theta_star;
    const Real cap_flux_q_star = flux_q_star;
    const Real cap_heat_flux_scale = heat_flux_scale;
    const Real cap_relative_humidity = relative_humidity;
    const Real cap_lv_water = FluxConstants::lv_water;

    for (MFIter mfi(output); mfi.isValid(); ++mfi) {
        const Box& bx = mfi.validbox();
        const int k_lo_box = bx.smallEnd(2);
        const int k_hi_box = bx.bigEnd(2) + 1;
        const auto vc   = vel_c_ptr->const_array(mfi);
        const auto v0a  = vel0_ptr->const_array(mfi);
        const auto la   = lam_ptr->const_array(mfi);
        const auto dib  = div_before.const_array(mfi);
        const auto dia  = div_after.const_array(mfi);
        
        const auto ttype_arr = terrain_type_ptr->const_array(mfi);
        const auto tslope_arr = terrain_slope_ptr->const_array(mfi);
        const auto adap_rough_arr = adaptive_roughness_ptr->const_array(mfi);
        const auto temp_arr = temp_ptr->const_array(mfi);
        const auto z_bl_arr = z_bl_diag_ptr->array(mfi);
        
        auto out = output.array(mfi);
        auto shf_arr = shf_ptr->array(mfi);
        auto lhf_arr = lhf_ptr->array(mfi);
        auto cd_arr = cd_ptr->array(mfi);
        auto ustar_arr = u_star_ptr->array(mfi);
        auto tau_arr = tau_flux_ptr->array(mfi);
        
        amrex::Array4<amrex::Real> turb_fluc;
        if (has_synthetic_turbulence && synthetic_turbulence_fluc_ptr) {
            turb_fluc = synthetic_turbulence_fluc_ptr->array(mfi);
        }

        amrex::ParallelFor(bx,
            [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
        {
            Real u = vc(i,j,k,0), v = vc(i,j,k,1), w = vc(i,j,k,2);
            out(i,j,k, 0) = u;
            out(i,j,k, 1) = v;
            out(i,j,k, 2) = w;
            out(i,j,k, 3) = std::sqrt(u*u + v*v + w*w);
            out(i,j,k, 4) = v0a(i,j,k,0);
            out(i,j,k, 5) = v0a(i,j,k,1);
            out(i,j,k, 6) = v0a(i,j,k,2);
            out(i,j,k, 7) = la(i,j,k);
            out(i,j,k, 8) = dib(i,j,k);
            out(i,j,k, 9) = dia(i,j,k);
            out(i,j,k,10) = d_terr_ptr[j * nx_cap_out + i];
            
            Real z_physical = z_lo_cap_div + (k + Real(0.5)) * dz_cap_div;
            Real z_agl      = z_physical - d_terr_ptr[j * nx_cap_out + i];
            Real u_mag = std::sqrt(u*u + v*v);
            Real ustar_local = Real(0.0);
            Real heat_flux = Real(0.0);
            Real Cd = Real(0.0);
            Real tau_x = Real(0.0);
            Real tau_y = Real(0.0);
            
            Real z0_local = enable_morph_val ? d_morph_z0_ptr[j * nx_cap_out + i] : (use_pos_z0 ? d_z0_pos_ptr_diag[j * nx_cap_out + i] : z0_cap);

            if (z_agl > Real(0.0) && u_mag > Real(1.0e-6)) {
                z0_local = std::max(z0_local, Real(1.0e-6));
                
                Real log_term = std::log((z_agl + z0_local) / z0_local);
                if (log_term > Real(0.1)) {
                    ustar_local = kappa_diag * u_mag / log_term;
                }
                
                if (enable_landuse_roughness_val && d_landuse_pos_ptr_diag) {
                    int lu_type = static_cast<int>(std::round(d_landuse_pos_ptr_diag[j * nx_cap_out + i]));
                    if (lu_type == static_cast<int>(LandUseCategory::WATER)) {
                        Real z0_water = compute_charnock_roughness(charnock_alpha_val, ustar_local);
                        z0_local = z0_water;
                        z0_local = std::max(z0_local, Real(1.0e-6));
                        log_term = std::log((z_agl + z0_local) / z0_local);
                        if (log_term > Real(0.1)) {
                            ustar_local = kappa_diag * u_mag / log_term;
                        }
                    }
                }
                
                if (log_term > Real(0.1)) {
                    heat_flux = rho_air * cp_air * ustar_local * theta_star;
                    Cd = (kappa_diag / log_term) * (kappa_diag / log_term);
                    
                    Real tau_magnitude = rho_air * ustar_local * ustar_local;
                    tau_x = tau_magnitude * (u / u_mag);
                    tau_y = tau_magnitude * (v / u_mag);
                }
            }

            Real shf_val = Real(0.0);
            Real lhf_val = Real(0.0);
            if (cap_enable_flux_diagnostics && z_agl > Real(0.0) && u_mag > Real(1.0e-6)) {
                if (enable_landuse_roughness_val && d_landuse_pos_ptr_diag) {
                    int lu_type = static_cast<int>(std::round(d_landuse_pos_ptr_diag[j * nx_cap_out + i]));
                    Real albedo = get_albedo_from_landuse(lu_type);
                    Real bowen = get_bowen_ratio_from_landuse(lu_type);
                    
                    // Apply cloud transmittance to solar radiation
                    Real solar_with_clouds = sky_view_factor::apply_cloud_cover_to_radiation(
                        cap_solar_radiation, cap_cloud_cover, cap_hour_of_day, cap_day_of_year, cap_latitude);
                    
                    Real net_rad = (Real(1.0) - albedo) * solar_with_clouds;
                    if (bowen > Real(1.0e-5)) {
                        constexpr Real partitioning_factor = Real(0.9); // 90% of net radiation is partitioned into turbulent fluxes
                        shf_val = partitioning_factor * net_rad / (Real(1.0) + Real(1.0) / bowen);
                        lhf_val = shf_val / bowen;
                    }
                } else {
                    shf_val = rho_air * cp_air * ustar_local * cap_flux_theta_star * cap_heat_flux_scale;
                    lhf_val = rho_air * cap_lv_water * ustar_local * cap_flux_q_star * cap_relative_humidity;
                }
                heat_flux = shf_val;
            }

            shf_arr(i, j, k) = shf_val;
            lhf_arr(i, j, k) = lhf_val;
            cd_arr(i, j, k) = Cd;
            ustar_arr(i, j, k) = ustar_local;
            tau_arr(i, j, k, 0) = tau_x;
            tau_arr(i, j, k, 1) = tau_y;

            Real richardson_no = Real(0.0);
            Real bl_depth = cap_bl_depth_param;
            Real terrain_elev = d_terr_ptr[j * nx_cap_out + i];
            
            int k_start = k_lo_box;
            while (k_start < k_hi_box && (z_lo_cap_div + (Real(k_start) + Real(0.5)) * dz_cap_div - terrain_elev <= Real(0.0))) {
                k_start++;
            }

            if (k_start < k_hi_box && z_agl > Real(0.0)) {
                Real theta_s = temp_arr(i, j, k_start);
                richardson_no = compute_bulk_richardson_number(theta_s, temp_arr(i, j, k), z_agl, u_mag, theta_s);

                if (cap_enable_bl_depth_diagnostic) {
                    Real diagnosed_bl_depth = RichardsonNumberConstants::MAX_BL_DEPTH;
                    bool found = false;
                    for (int kp = k_start + 1; kp < k_hi_box; ++kp) {
                        Real z_agl_kp = z_lo_cap_div + (Real(kp) + Real(0.5)) * dz_cap_div - terrain_elev;
                        Real u_kp = vc(i, j, kp, 0);
                        Real v_kp = vc(i, j, kp, 1);
                        Real u_mag_kp = std::sqrt(u_kp * u_kp + v_kp * v_kp);
                        Real ri_b_kp = compute_bulk_richardson_number(theta_s, temp_arr(i, j, kp), z_agl_kp, u_mag_kp, theta_s);

                        if (ri_b_kp > richardson_critical) {
                            Real z_agl_kpm1 = z_lo_cap_div + (Real(kp - 1) + Real(0.5)) * dz_cap_div - terrain_elev;
                            Real u_kpm1 = vc(i, j, kp - 1, 0);
                            Real v_kpm1 = vc(i, j, kp - 1, 1);
                            Real u_mag_kpm1 = std::sqrt(u_kpm1 * u_kpm1 + v_kpm1 * v_kpm1);
                            Real ri_b_kpm1 = compute_bulk_richardson_number(theta_s, temp_arr(i, j, kp - 1), z_agl_kpm1, u_mag_kpm1, theta_s);

                            if (std::abs(ri_b_kp - ri_b_kpm1) > Real(1.0e-10)) {
                                Real frac = (richardson_critical - ri_b_kpm1) / (ri_b_kp - ri_b_kpm1);
                                diagnosed_bl_depth = z_agl_kpm1 + frac * (z_agl_kp - z_agl_kpm1);
                            } else {
                                diagnosed_bl_depth = z_agl_kpm1;
                            }
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        diagnosed_bl_depth = RichardsonNumberConstants::MAX_BL_DEPTH;
                    }
                    const Real min_bl_depth_local = RichardsonNumberConstants::MIN_BL_DEPTH;
                    const Real max_bl_depth_local = RichardsonNumberConstants::MAX_BL_DEPTH;
                    bl_depth = std::max(min_bl_depth_local,
                                        std::min(diagnosed_bl_depth, max_bl_depth_local));
                }
            } else {
                richardson_no = Real(0.0);
                bl_depth = cap_bl_depth_param;
            }

            if (enable_marine_bl_val) {
                int lu_type = (enable_landuse_roughness_val && d_landuse_pos_ptr_diag) ? static_cast<int>(std::round(d_landuse_pos_ptr_diag[j * nx_cap_out + i])) : -1;
                if (lu_type == 11) { // WATER
                    Real y_coord = cap_y_lo + (j + Real(0.5)) * cap_dy;
                    Real f_param = compute_latitude_dependent_coriolis(y_coord, cap_y_center, cap_domain_latitude);
                    Real abs_f = std::max(std::abs(f_param), Real(1.0e-5));
                    
                    // Marine boundary layer parameters/empirical constants
                    constexpr Real BULK_HEAT_TRANSFER_COEFF_WATER = 0.0014;
                    constexpr Real MARINE_MECHANICAL_MIXING_COEFF = 0.2;
                    constexpr Real MARINE_CONVECTIVE_MIXING_COEFF = 0.3;
                    
                    // Mechanical mixing height
                    Real h_mech = MARINE_MECHANICAL_MIXING_COEFF * ustar_local / abs_f;
                    
                    // Convective mixing height
                    Real h_conv = Real(0.0);
                    if (marine_air_sea_dt_val < Real(0.0)) { // convective over water
                        Real g_val = Real(9.81);
                        Real num = g_val * BULK_HEAT_TRANSFER_COEFF_WATER * u_mag * (-marine_air_sea_dt_val);
                        Real den = marine_sst_val * abs_f * abs_f * abs_f;
                        h_conv = MARINE_CONVECTIVE_MIXING_COEFF * std::sqrt(num / den);
                    }
                    Real h_marine = std::sqrt(h_mech * h_mech + h_conv * h_conv);
                    
                    const Real min_bl_depth_local = RichardsonNumberConstants::MIN_BL_DEPTH;
                    const Real max_bl_depth_local = RichardsonNumberConstants::MAX_BL_DEPTH;
                    bl_depth = std::max(min_bl_depth_local, std::min(h_marine, max_bl_depth_local));
                }
            }
            
            out(i,j,k,11) = heat_flux;
            out(i,j,k,12) = Cd;
            out(i,j,k,13) = tau_x;
            out(i,j,k,14) = tau_y;
            out(i,j,k,15) = ustar_local;
            out(i,j,k,16) = richardson_no;
            out(i,j,k,17) = bl_depth;
            z_bl_arr(i, j, k) = bl_depth;
            
            out(i,j,k,18) = cap_enable_terrain_analysis ? Real(ttype_arr(i,j,k)) : Real(0.0);
            out(i,j,k,19) = cap_enable_terrain_analysis ? tslope_arr(i,j,k) : Real(0.0);
            out(i,j,k,20) = cap_enable_terrain_analysis ? adap_rough_arr(i,j,k) : z0_local;
            
            if (cap_has_turb) {
                Real u_openfast = u + turb_fluc(i,j,k,0);
                Real v_openfast = v + turb_fluc(i,j,k,1);
                Real w_openfast = w + turb_fluc(i,j,k,2);
                out(i,j,k,21) = u_openfast;
                out(i,j,k,22) = v_openfast;
                out(i,j,k,23) = w_openfast;
            }

            if (cap_enable_coriolis_latitude) {
                Real y_coord = cap_y_lo + (j + Real(0.5)) * cap_dy;
                Real f = compute_latitude_dependent_coriolis(y_coord, cap_y_center, cap_domain_latitude);
                Real U_mag = std::sqrt(u*u + v*v + w*w);
                Real Ro = compute_rossby_number(U_mag, f, Real(1000.0));
                Real T_i = compute_inertial_period(f);
                
                int idx_offset = cap_has_turb ? 24 : 21;
                out(i,j,k, idx_offset) = f;
                out(i,j,k, idx_offset + 1) = Ro;
                out(i,j,k, idx_offset + 2) = T_i;
            }

            if (cap_enable_wire_loading) {
                out(i, j, k, cap_wire_idx_start) = Real(0.0);
                out(i, j, k, cap_wire_idx_start + 1) = Real(0.0);
            }
        });
    }

    if (enable_wire_loading && !wires.empty()) {
        const Real xmin = x_lo;
        const Real ymin = y_lo;
        const Real zmin = zs_min;
        const Real dx_val = dx;
        const Real dy_val = dy;
        const Real dz_val = dz;
        const int nx_val = nx;
        const int ny_val = ny;
        const int nz_val = nz;

#ifdef AMREX_USE_GPU
        amrex::Gpu::streamSynchronize();
#endif
        for (MFIter mfi(output, false); mfi.isValid(); ++mfi) {
            const Box& bx = mfi.validbox();
            auto out = output.array(mfi);
            
            for (const auto& w : wires) {
                Real dx_span = w.x2 - w.x1;
                Real dy_span = w.y2 - w.y1;
                Real dz_span = w.z2 - w.z1;
                Real L = std::sqrt(dx_span*dx_span + dy_span*dy_span + dz_span*dz_span);
                if (L <= 1.0e-6) continue;

                Real tx = dx_span / L;
                Real ty = dy_span / L;
                Real tz = dz_span / L;

                int num_segs = std::max(1, static_cast<int>(std::ceil(L / std::min(dx_val, dy_val))));
                Real ds = L / num_segs;

                for (int s = 0; s < num_segs; ++s) {
                    Real xc = w.x1 + (s + Real(0.5)) * ds * tx;
                    Real yc = w.y1 + (s + Real(0.5)) * ds * ty;
                    Real zc = w.z1 + (s + Real(0.5)) * ds * tz;

                    int i = static_cast<int>(std::floor((xc - xmin) / dx_val));
                    int j = static_cast<int>(std::floor((yc - ymin) / dy_val));
                    int k = static_cast<int>(std::floor((zc - zmin) / dz_val));

                    if (bx.contains(IntVect(i, j, k))) {
                        Real u = TurbineWake::interpolate_3d(u_host, xc, yc, zc, xmin, ymin, zmin, dx_val, dy_val, dz_val, nx_val, ny_val, nz_val);
                        Real v = TurbineWake::interpolate_3d(v_host, xc, yc, zc, xmin, ymin, zmin, dx_val, dy_val, dz_val, nx_val, ny_val, nz_val);
                        Real w_vel = TurbineWake::interpolate_3d(w_host, xc, yc, zc, xmin, ymin, zmin, dx_val, dy_val, dz_val, nx_val, ny_val, nz_val);
                        Real Ta = TurbineWake::interpolate_3d(T_host, xc, yc, zc, xmin, ymin, zmin, dx_val, dy_val, dz_val, nx_val, ny_val, nz_val);

                        Real speed = std::sqrt(u*u + v*v + w_vel*w_vel);
                        Real u_p = u * tx + v * ty + w_vel * tz;
                        Real u_n_sq = std::max(Real(0.0), speed*speed - u_p*u_p);
                        Real u_n = std::sqrt(u_n_sq);

                        Real Fd = Real(0.5) * Real(1.225) * w.drag_coeff * w.diameter * u_n_sq;
                        Real Ts = WireLoading::solve_conductor_temp(Ta, u_n, w.diameter, w.resistance, w.emissivity, w.absorptivity, solar_radiation, w.current);

                        out(i, j, k, cap_wire_idx_start) += Fd;
                        out(i, j, k, cap_wire_idx_start + 1) = std::max(out(i, j, k, cap_wire_idx_start + 1), Ts);
                    }
                }
            }
        }
#ifdef AMREX_USE_GPU
        amrex::Gpu::streamSynchronize();
#endif
    }

    Vector<std::string> var_names = {
        "u", "v", "w", "vel_magnitude",
        "u0", "v0", "w0",
        "lambda",
        "div_before", "div_after",
        "terrain_z",
        "heat_flux", "drag_coeff",
        "tau_x", "tau_y", "u_star",
        "richardson_no", "bl_depth",
        "terrain_type", "terrain_slope", "adaptive_z0"
    };
    
    if (has_synthetic_turbulence) {
        var_names.push_back("u_openfast");
        var_names.push_back("v_openfast");
        var_names.push_back("w_openfast");
    }

    if (enable_coriolis_latitude) {
        var_names.push_back("coriolis_f");
        var_names.push_back("rossby_number");
        var_names.push_back("inertial_period");
    }

    if (enable_wire_loading) {
        var_names.push_back("wire_drag_force_per_m");
        var_names.push_back("wire_conductor_temp");
    }

    amrex::Print() << "wind_solver: divergence computation time = " 
                   << (amrex::second() - t_phase) << " s\n";

    t_phase = amrex::second();
    std::string indexed_plot_file = amrex::Concatenate(plot_file, time_step);
    WriteSingleLevelPlotfile(indexed_plot_file, output, var_names, *geom_ptr, 0.0, 0);
    amrex::Print() << "wind_solver: plotfile written to " << indexed_plot_file << "\n";
    amrex::Print() << "wind_solver: output writing time = " 
                   << (amrex::second() - t_phase) << " s\n";

    const bool do_extract = !extract_agl_list.empty() || !extract_k_list.empty();

    if (do_extract) {
        std::vector<std::pair<int, Real>> extraction_levels;
        
        if (!extract_agl_list.empty()) {
            for (Real agl_req : extract_agl_list) {
                int k_ext = static_cast<int>(std::floor(agl_req / dz));
                k_ext = std::max(0, std::min(nz - 1, k_ext));
                extraction_levels.push_back({k_ext, agl_req});
            }
        }
        else if (!extract_k_list.empty()) {
            for (int k_req : extract_k_list) {
                int k_ext = std::max(0, std::min(nz - 1, k_req));
                Real agl_est = (k_ext + Real(0.5)) * dz;
                extraction_levels.push_back({k_ext, agl_est});
            }
        }
        
        for (size_t level_idx = 0; level_idx < extraction_levels.size(); ++level_idx) {
            int k_ext = extraction_levels[level_idx].first;
            Real agl_target = extraction_levels[level_idx].second;
            Real z_phys_ext = zs_min + (k_ext + Real(0.5)) * dz;
            
            amrex::Print() << "wind_solver: terrain-aligned extraction " << (level_idx + 1)
                           << "/" << extraction_levels.size() << " at AGL = "
                           << agl_target << " m  →  k = " << k_ext
                           << "  (physical z = " << z_phys_ext << " m)\n";

            amrex::Gpu::streamSynchronize();

            struct ExtPt {
                Real x, y, z_terrain, z_phys, z_agl_val;
                Real u, v, w, speed;
                int gi, gj;
            };
            
            std::vector<ExtPt> local_pts;
            local_pts.reserve(static_cast<std::size_t>(nx) * ny / 4 + 1);

            for (MFIter mfi(*vel_c_ptr, false); mfi.isValid(); ++mfi) {
                const Box& bx = mfi.validbox();
                if (k_ext < bx.smallEnd(2) || k_ext > bx.bigEnd(2)) continue;

#ifdef AMREX_USE_GPU
                Box slice_bx(IntVect(bx.smallEnd(0), bx.smallEnd(1), k_ext),
                              IntVect(bx.bigEnd(0),   bx.bigEnd(1),   k_ext));
                FArrayBox slice_fab(slice_bx, 3, The_Pinned_Arena());
                slice_fab.copy<RunOn::Device>((*vel_c_ptr)[mfi], slice_bx);
                amrex::Gpu::streamSynchronize();
                auto const& vc = slice_fab.const_array();
#else
                auto const& vc = vel_c_ptr->const_array(mfi);
#endif

                for (int j = bx.smallEnd(1); j <= bx.bigEnd(1); ++j) {
                    for (int i = bx.smallEnd(0); i <= bx.bigEnd(0); ++i) {
                        Real zs      = terrain_h[static_cast<std::size_t>(j) * nx + i];
                        Real xc      = x_lo + (i + Real(0.5)) * dx;
                        Real yc      = y_lo + (j + Real(0.5)) * dy;
                        Real z_agl_c = z_phys_ext - zs;
                        Real u_  = vc(i, j, k_ext, 0);
                        Real v_  = vc(i, j, k_ext, 1);
                        Real w_  = vc(i, j, k_ext, 2);
                        Real spd = std::sqrt(u_*u_ + v_*v_ + w_*w_);
                        local_pts.push_back({xc, yc, zs, z_phys_ext,
                                             z_agl_c, u_, v_, w_, spd,
                                             i, j});
                    }
                }
            }

            std::sort(local_pts.begin(), local_pts.end(),
                      [](const ExtPt& a, const ExtPt& b) {
                          return (a.gj != b.gj) ? (a.gj < b.gj) : (a.gi < b.gi);
                      });

            std::string output_file;
            if (extraction_levels.size() == 1 && num_time_steps == 1) {
                output_file = extract_file;
            } else {
                size_t dot_pos = extract_file.find_last_of('.');
                std::string base = (dot_pos != std::string::npos) ? 
                                   extract_file.substr(0, dot_pos) : extract_file;
                std::string ext = (dot_pos != std::string::npos) ? 
                                  extract_file.substr(dot_pos) : ".csv";
                std::ostringstream fname;
                fname << base;
                if (extraction_levels.size() > 1) {
                    fname << "_" << static_cast<int>(agl_target) << "m";
                }
                if (num_time_steps > 1) {
                    fname << "_t" << time_step;
                }
                fname << ext;
                output_file = fname.str();
            }

            const int nranks = amrex::ParallelDescriptor::NProcs();
            const int myrank = amrex::ParallelDescriptor::MyProc();

            auto write_pts = [&](bool write_header) {
                std::ofstream outf(output_file,
                                   write_header ? std::ios::out
                                                : std::ios::app);
                    outf << std::scientific << std::setprecision(6);
                if (write_header) {
                    outf << "x,y,z_terrain,z_physical,z_agl,u,v,w,speed\n";
                }
                for (const auto& p : local_pts) {
                    outf << p.x       << ","
                         << p.y       << ","
                         << p.z_terrain << ","
                         << p.z_phys  << ","
                         << p.z_agl_val << ","
                         << p.u       << ","
                         << p.v       << ","
                         << p.w       << ","
                         << p.speed   << "\n";
                }
            };

            if (myrank == 0) {
                write_pts(true);
            }
            for (int r = 1; r < nranks; ++r) {
                amrex::ParallelDescriptor::Barrier();
                if (myrank == r) {
                    write_pts(false);
                }
            }
            amrex::ParallelDescriptor::Barrier();

            amrex::Print() << "wind_solver: terrain-aligned extraction written to "
                           << output_file << "  (" << (nx * ny) << " points)\n";
        }
    }
}

std::vector<StreetCanyon> WindSolverApp::detect_street_canyons(
    const std::vector<amrex::Real>& b_xmin, const std::vector<amrex::Real>& b_xmax,
    const std::vector<amrex::Real>& b_ymin, const std::vector<amrex::Real>& b_ymax,
    const std::vector<amrex::Real>& b_zmin, const std::vector<amrex::Real>& b_zmax,
    const std::vector<amrex::Real>& b_rotation)
{
    amrex::ignore_unused(b_rotation);
    std::vector<StreetCanyon> canyons;
    int n_bldgs = static_cast<int>(b_xmin.size());
    if (n_bldgs < 2) return canyons;

    // Detect Y-aligned canyons (separated in X, overlap in Y)
    for (int i = 0; i < n_bldgs; ++i) {
        for (int j = 0; j < n_bldgs; ++j) {
            if (i == j) continue;

            // Check if building i is to the left of building j in X
            if (b_xmax[i] <= b_xmin[j]) {
                // Check if they overlap in Y
                amrex::Real y_overlap_min = std::max(b_ymin[i], b_ymin[j]);
                amrex::Real y_overlap_max = std::min(b_ymax[i], b_ymax[j]);
                if (y_overlap_max > y_overlap_min) {
                    // Check if there is any building k in between them
                    bool blocked = false;
                    for (int k = 0; k < n_bldgs; ++k) {
                        if (k == i || k == j) continue;
                        // Does k overlap in Y with the overlap region?
                        amrex::Real k_overlap_min = std::max(b_ymin[k], y_overlap_min);
                        amrex::Real k_overlap_max = std::min(b_ymax[k], y_overlap_max);
                        if (k_overlap_max > k_overlap_min) {
                            // Is k strictly between i and j in X?
                            if (b_xmax[i] <= b_xmin[k] && b_xmax[k] <= b_xmin[j]) {
                                blocked = true;
                                break;
                            }
                        }
                    }
                    if (!blocked) {
                        StreetCanyon canyon;
                        canyon.xmin = b_xmax[i];
                        canyon.xmax = b_xmin[j];
                        canyon.ymin = y_overlap_min;
                        canyon.ymax = y_overlap_max;
                        canyon.zmin = 0.0; // canyons start from ground
                        canyon.h_canyon = 0.5 * ((b_zmax[i] - b_zmin[i]) + (b_zmax[j] - b_zmin[j]));
                        canyon.zmax = canyon.h_canyon;
                        canyon.w_canyon = canyon.xmax - canyon.xmin;
                        canyon.aspect_ratio = canyon.h_canyon / canyon.w_canyon;
                        canyon.direction = 0; // Y-aligned
                        canyons.push_back(canyon);
                    }
                }
            }

            // Check if building i is below building j in Y (X-aligned canyon)
            if (b_ymax[i] <= b_ymin[j]) {
                // Check if they overlap in X
                amrex::Real x_overlap_min = std::max(b_xmin[i], b_xmin[j]);
                amrex::Real x_overlap_max = std::min(b_xmax[i], b_xmax[j]);
                if (x_overlap_max > x_overlap_min) {
                    // Check if there is any building k in between them
                    bool blocked = false;
                    for (int k = 0; k < n_bldgs; ++k) {
                        if (k == i || k == j) continue;
                        // Does k overlap in X with the overlap region?
                        amrex::Real k_overlap_min = std::max(b_xmin[k], x_overlap_min);
                        amrex::Real k_overlap_max = std::min(b_xmax[k], x_overlap_max);
                        if (k_overlap_max > k_overlap_min) {
                            // Is k strictly between i and j in Y?
                            if (b_ymax[i] <= b_ymin[k] && b_ymax[k] <= b_ymin[j]) {
                                blocked = true;
                                break;
                            }
                        }
                    }
                    if (!blocked) {
                        StreetCanyon canyon;
                        canyon.xmin = x_overlap_min;
                        canyon.xmax = x_overlap_max;
                        canyon.ymin = b_ymax[i];
                        canyon.ymax = b_ymin[j];
                        canyon.zmin = 0.0;
                        canyon.h_canyon = 0.5 * ((b_zmax[i] - b_zmin[i]) + (b_zmax[j] - b_zmin[j]));
                        canyon.zmax = canyon.h_canyon;
                        canyon.w_canyon = canyon.ymax - canyon.ymin;
                        canyon.aspect_ratio = canyon.h_canyon / canyon.w_canyon;
                        canyon.direction = 1; // X-aligned
                        canyons.push_back(canyon);
                    }
                }
            }
        }
    }
    return canyons;
}

// ============================================================================
// 3D Scalar Transport Implementation
// ============================================================================

amrex::Real WindSolverApp::compute_adaptive_dt_transport() {
    // Compute adaptive time step based on CFL criterion
    // dt_cfl = CFL * min(dx, dy, dz) / max(u, v, w)
    
    if (scalar_dt > 0.0) {
       return scalar_dt;  // Use user-specified time step if provided
    }
    
    amrex::Real u_max = 0.0;
    
    // Find maximum velocity magnitude
    amrex::MultiFab temp(vel_c_ptr->boxArray(), vel_c_ptr->DistributionMap(), 1, 0);
    temp.setVal(0.0);
    
    for (amrex::MFIter mfi(*vel_c_ptr); mfi.isValid(); ++mfi) {
       const auto& box = mfi.validbox();
       const auto& vel_arr = vel_c_ptr->array(mfi);
       auto temp_arr = temp.array(mfi);
        
       amrex::ParallelFor(box, [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
           amrex::Real u = vel_arr(i, j, k, 0);
           amrex::Real v = vel_arr(i, j, k, 1);
           amrex::Real w = vel_arr(i, j, k, 2);
           amrex::Real vmag = std::sqrt(u*u + v*v + w*w);
           temp_arr(i, j, k) = vmag;
       });
    }
    
    u_max = temp.max(0);
    
    // Global max
    amrex::ParallelDescriptor::ReduceRealMax(u_max);
    
    // Avoid division by zero
    if (u_max < 1.0e-10) {
       u_max = 1.0e-10;
    }
    
    amrex::Real dz_min = std::min(dx, std::min(dy, dz));
    amrex::Real dt_cfl = scalar_cfl * dz_min / u_max;
    
    amrex::Print() << "wind_solver: computed adaptive transport dt = " << dt_cfl << " s (u_max = " << u_max << " m/s)\n";
    
    return dt_cfl;
}

void WindSolverApp::compute_eddy_diffusivity_mixing_length(amrex::MultiFab& kappa_eddy) {
    // Compute eddy diffusivity using mixing length model
    // K_eddy = (l_m)^2 * |∇u|
    // where l_m = mixing length (based on von Karman and height)
    
    kappa_eddy.setVal(0.0);
    
    if (!enable_mixing_length_turbulence) {
       return;
    }
    
    for (amrex::MFIter mfi(kappa_eddy); mfi.isValid(); ++mfi) {
       const auto& box = mfi.validbox();
       auto kappa_arr = kappa_eddy.array(mfi);
       auto vel_arr = vel_c_ptr->array(mfi);
       auto terrain_arr = terrain_type_ptr->array(mfi);
        
       amrex::ParallelFor(box, [this, kappa_arr, vel_arr, terrain_arr] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
           // Get cell-center height
           amrex::Real z_cell = zs_min + (amrex::Real(k) + 0.5) * this->dz;
            
           // Compute mixing length: l_m = κ * (z + z0) for z > 0
           amrex::Real z_eff = std::max(z_cell - terrain_arr(i, j, 0), 1.0e-3);
           amrex::Real l_m = von_karman * (z_eff + zground) * mixing_length_coefficient;
            
           // Compute velocity gradient magnitude (simplified: vertical shear)
           amrex::Real du_dz = 0.0;
           amrex::Real dv_dz = 0.0;
            
           if (k < nz - 1) {
               du_dz = (vel_arr(i, j, k+1, 0) - vel_arr(i, j, k, 0)) / this->dz;
               dv_dz = (vel_arr(i, j, k+1, 1) - vel_arr(i, j, k, 1)) / this->dz;
           }
            
           amrex::Real shear_mag = std::sqrt(du_dz*du_dz + dv_dz*dv_dz);
            
           // K_eddy = (l_m)^2 * |∇u|
           kappa_arr(i, j, k) = l_m * l_m * shear_mag;
       });
    }
}

void WindSolverApp::solve_transport_equations(int time_step, amrex::Real dt_transport) {
    // Solve scalar transport equations for temperature and moisture
    // ∂ϕ/∂t + u·∇ϕ = ∇·(K_eff ∇ϕ)
    // where K_eff = K_mol + K_eddy
    
    amrex::Print() << "wind_solver: solving scalar transport equations with dt = " << dt_transport << " s\n";
    
    if (enable_temperature_transport && temp_3d_ptr) {
       solve_scalar_transport(*temp_3d_ptr, *temp_3d_old_ptr, *vel_c_ptr, 
                             temperature_diffusivity, dt_transport, "temperature");
      amrex::MultiFab::Copy(*temp_3d_old_ptr, *temp_3d_ptr, 0, 0, 1, temp_3d_old_ptr->nGrow());
    }
    
    if (enable_moisture_transport && moisture_3d_ptr) {
      solve_scalar_transport(*moisture_3d_ptr, *moisture_3d_old_ptr, *vel_c_ptr,
                            moisture_diffusivity, dt_transport, "moisture");
      amrex::MultiFab::Copy(*moisture_3d_old_ptr, *moisture_3d_ptr, 0, 0, 1, moisture_3d_old_ptr->nGrow());
    }
}

void WindSolverApp::solve_scalar_transport(
    amrex::MultiFab& scalar_new,
    const amrex::MultiFab& scalar_old,
    const amrex::MultiFab& vel,
    amrex::Real diffusivity,
    amrex::Real dt,
    const std::string& scalar_name)
{
    // Simple explicit forward Euler scheme with diffusion
    // scalar_new = scalar_old - dt * u·∇scalar + dt * ∇·(K_eff ∇scalar)
    
    using namespace amrex;
    
    // Temporary field for intermediate calculations
    MultiFab scalar_adv(scalar_old.boxArray(), scalar_old.DistributionMap(), 1, 1);
    MultiFab kappa_eddy(scalar_old.boxArray(), scalar_old.DistributionMap(), 1, 0);
    
    // Compute eddy diffusivity with mixing length model
    compute_eddy_diffusivity_mixing_length(kappa_eddy);
    
    // Copy old scalar values
    amrex::MultiFab::Copy(scalar_adv, scalar_old, 0, 0, 1, 1);
    
    // Step 1: Advection (semi-Lagrangian or upstream differencing)
    for (MFIter mfi(scalar_adv); mfi.isValid(); ++mfi) {
       const auto& bx = mfi.validbox();
       auto adv_arr = scalar_adv.array(mfi);
       auto scalar_arr = scalar_old.array(mfi);
       auto vel_arr = vel.array(mfi);
        
       amrex::ParallelFor(bx, [this, vel_arr, scalar_arr, adv_arr, bx, dt] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
           amrex::Real u = vel_arr(i, j, k, 0);
           amrex::Real v = vel_arr(i, j, k, 1);
           amrex::Real w = vel_arr(i, j, k, 2);
            
           // Upstream differencing for advection
           amrex::Real dscalar_dx = 0.0, dscalar_dy = 0.0, dscalar_dz = 0.0;
            
           if (u > 0.0 && i > bx.smallEnd(0)) {
               dscalar_dx = (scalar_arr(i, j, k) - scalar_arr(i-1, j, k)) / this->dx;
           } else if (u < 0.0 && i < bx.bigEnd(0)) {
               dscalar_dx = (scalar_arr(i+1, j, k) - scalar_arr(i, j, k)) / this->dx;
           }
            
           if (v > 0.0 && j > bx.smallEnd(1)) {
               dscalar_dy = (scalar_arr(i, j, k) - scalar_arr(i, j-1, k)) / this->dy;
           } else if (v < 0.0 && j < bx.bigEnd(1)) {
               dscalar_dy = (scalar_arr(i, j+1, k) - scalar_arr(i, j, k)) / this->dy;
           }
            
           if (w > 0.0 && k > bx.smallEnd(2)) {
               dscalar_dz = (scalar_arr(i, j, k) - scalar_arr(i, j, k-1)) / this->dz;
           } else if (w < 0.0 && k < bx.bigEnd(2)) {
               dscalar_dz = (scalar_arr(i, j, k+1) - scalar_arr(i, j, k)) / this->dz;
           }
            
           // Update with advection
           adv_arr(i, j, k) -= dt * (u * dscalar_dx + v * dscalar_dy + w * dscalar_dz);
       });
    }
    
    // Step 2: Diffusion using finite differences
    scalar_new.setVal(0.0);
    
    for (MFIter mfi(scalar_new); mfi.isValid(); ++mfi) {
       const auto& bx = mfi.validbox();
       auto new_arr = scalar_new.array(mfi);
       auto adv_arr = scalar_adv.array(mfi);
       auto kappa_arr = kappa_eddy.array(mfi);
        
       amrex::ParallelFor(bx, [this, adv_arr, kappa_arr, new_arr, bx, diffusivity, dt] AMREX_GPU_DEVICE (int i, int j, int k) noexcept {
           amrex::Real K_eff_x = 0.0, K_eff_y = 0.0, K_eff_z = 0.0;
           amrex::Real d2scalar_dx2 = 0.0, d2scalar_dy2 = 0.0, d2scalar_dz2 = 0.0;
            
           // Total effective diffusivity (molecular + eddy)
           amrex::Real K_eddy_cell = kappa_arr(i, j, k);
            
           // X-direction
           if (i > bx.smallEnd(0) && i < bx.bigEnd(0)) {
               K_eff_x = diffusivity + K_eddy_cell;
               d2scalar_dx2 = (adv_arr(i+1, j, k) - 2.0*adv_arr(i, j, k) + adv_arr(i-1, j, k)) / (this->dx*this->dx);
           }
            
           // Y-direction
           if (j > bx.smallEnd(1) && j < bx.bigEnd(1)) {
               K_eff_y = diffusivity + K_eddy_cell;
               d2scalar_dy2 = (adv_arr(i, j+1, k) - 2.0*adv_arr(i, j, k) + adv_arr(i, j-1, k)) / (this->dy*this->dy);
           }
            
           // Z-direction
           if (k > bx.smallEnd(2) && k < bx.bigEnd(2)) {
               K_eff_z = diffusivity + K_eddy_cell;
               d2scalar_dz2 = (adv_arr(i, j, k+1) - 2.0*adv_arr(i, j, k) + adv_arr(i, j, k-1)) / (this->dz*this->dz);
           }
            
           // Final update
           new_arr(i, j, k) = adv_arr(i, j, k) + dt * (
               K_eff_x * d2scalar_dx2 + 
               K_eff_y * d2scalar_dy2 + 
               K_eff_z * d2scalar_dz2
           );
       });
    }
    
    // Copy boundary conditions from old field
    amrex::MultiFab::Copy(scalar_new, scalar_old, 0, 0, 1, 0);  // Copy ghost cells
    scalar_new.FillBoundary(geom_ptr->periodicity());
}
