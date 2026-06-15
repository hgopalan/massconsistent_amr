#include "data_assimilation.H"
#include <AMReX_ParmParse.H>
#include <AMReX_Print.H>
#include <algorithm>

using namespace amrex;

// Global singleton instance
static std::unique_ptr<DataAssimilationManager> g_data_assimilation_manager;

DataAssimilationManager& get_data_assimilation_manager()
{
    if (!g_data_assimilation_manager) {
        g_data_assimilation_manager = std::make_unique<DataAssimilationManager>();
    }
    return *g_data_assimilation_manager;
}

void DataAssimilationManager::initialize_from_parmparse(const Geometry& geom)
{
    ParmParse pp;
    
    // Check if data assimilation is enabled
    pp.query("enable_data_assimilation", enabled);
    
    if (!enabled) {
        amrex::Print() << "[DA] Data assimilation disabled (enable_data_assimilation = false)\n";
        return;
    }
    
    amrex::Print() << "[DA] Initializing data assimilation module...\n";
    
    // Read EnKF parameters
    pp.query("enkf_ensemble_size", ensemble_size);
    pp.query("enkf_localization_scale", localization_scale);
    pp.query("enkf_u_star_std", u_star_std);
    pp.query("enkf_z0_std_factor", z0_std_factor);
    pp.query("enkf_wind_dir_std", wind_dir_std);
    pp.query("enkf_obs_file_station", obs_file_station);
    pp.query("enkf_obs_file_lidar", obs_file_lidar);
    pp.query("enkf_poisson_tolerance", poisson_tolerance);
    pp.query("enkf_max_iterations", max_iterations);
    
    // Validate parameters
    if (ensemble_size < 1) {
        amrex::Warning("[DA] Invalid ensemble_size; setting to 1");
        ensemble_size = 1;
    }
    
    if (localization_scale <= 0.0) {
        amrex::Warning("[DA] Invalid localization_scale; setting to 5000 m");
        localization_scale = 5000.0;
    }
    
    // Create EnKF instance
    enkf = std::make_unique<EnsembleKalmanFilter>();
    enkf->initialize(ensemble_size, geom, localization_scale);
    enkf->set_background_error_covariance(u_star_std, z0_std_factor, wind_dir_std);
    
    amrex::Print() << "[DA] Data assimilation initialized:\n"
                   << "  ensemble_size = " << ensemble_size << "\n"
                   << "  localization_scale = " << localization_scale << " m\n"
                   << "  u_star_std = " << u_star_std << " m/s\n"
                   << "  z0_std_factor = " << z0_std_factor << "\n"
                   << "  wind_dir_std = " << wind_dir_std << " degrees\n";
    
    if (!obs_file_station.empty()) {
        amrex::Print() << "  obs_file_station = " << obs_file_station << "\n";
    }
    if (!obs_file_lidar.empty()) {
        amrex::Print() << "  obs_file_lidar = " << obs_file_lidar << "\n";
    }
}

void DataAssimilationManager::forecast_ensemble_member(
    int member_id,
    const std::function<void(const EnsembleProfileParameters&, amrex::MultiFab&)>& solve_func,
    amrex::MultiFab& wind_field)
{
    if (!enabled || !enkf) {
        amrex::Error("[DA] Data assimilation not enabled");
    }
    
    if (member_id < 0 || member_id >= ensemble_size) {
        amrex::Error("[DA] Invalid ensemble member ID");
    }
    
    // Get perturbed parameters for this member
    EnsembleProfileParameters params = enkf->get_member_parameters(member_id);
    
    amrex::Print() << "[DA] Solving member " << member_id << " with u_* = "
                   << params.u_star << " m/s, z0 = " << params.z0 << " m\n";
    
    // Call solver callback
    solve_func(params, wind_field);
}

void DataAssimilationManager::forecast_ensemble(
    const std::function<void(const EnsembleProfileParameters&, amrex::MultiFab&)>& solve_func,
    std::vector<amrex::MultiFab>& ensemble_wind_fields)
{
    if (!enabled || !enkf) {
        amrex::Error("[DA] Data assimilation not enabled");
    }
    
    ensemble_wind_fields.clear();
    ensemble_wind_fields.resize(ensemble_size);
    
    amrex::Print() << "[DA] Starting ensemble forecast with " << ensemble_size 
                   << " members...\n";
    
    for (int i = 0; i < ensemble_size; ++i) {
        forecast_ensemble_member(i, solve_func, ensemble_wind_fields[i]);
    }
    
    amrex::Print() << "[DA] Ensemble forecast completed\n";
}

int DataAssimilationManager::load_observations_for_analysis()
{
    if (!enabled || !enkf) {
        return 0;
    }
    
    // Clear previous observations
    enkf->clear_observations();
    last_obs_count = 0;
    
    // Load from station file
    if (!obs_file_station.empty()) {
        enkf->load_observations_from_csv(obs_file_station);
        last_obs_count += enkf->get_observation_count();
    }
    
    // Load from LiDAR file (if implemented)
    if (!obs_file_lidar.empty()) {
        enkf->load_observations_from_netcdf(obs_file_lidar, "lidar");
        last_obs_count = enkf->get_observation_count();
    }
    
    amrex::Print() << "[DA] Loaded " << last_obs_count << " observations\n";
    
    return last_obs_count;
}

void DataAssimilationManager::execute_analysis_step(
    std::vector<amrex::MultiFab>& ensemble_wind_fields,
    const Geometry& geom)
{
    if (!enabled || !enkf) {
        amrex::Error("[DA] Data assimilation not enabled");
    }
    
    if (ensemble_wind_fields.size() != ensemble_size) {
        amrex::Error("[DA] Ensemble size mismatch in analysis step");
    }
    
    if (enkf->get_observation_count() == 0) {
        amrex::Warning("[DA] No observations available for analysis");
        return;
    }
    
    analysis_cycle_count++;
    
    amrex::Print() << "[DA] ========== Analysis Cycle " << analysis_cycle_count 
                   << " ==========\n";
    
    // Execute analysis step (updates ensemble in-place)
    enkf->analysis_step(ensemble_wind_fields, geom, true);  // with localization
    
    // Project to divergence-free for each member
    amrex::Print() << "[DA] Projecting ensemble members to divergence-free space...\n";
    for (int i = 0; i < ensemble_size; ++i) {
        enkf->project_to_divergence_free(ensemble_wind_fields[i], geom, poisson_tolerance);
    }
    
    amrex::Print() << "[DA] Analysis step completed\n";
}

void DataAssimilationManager::get_ensemble_mean(
    const std::vector<amrex::MultiFab>& ensemble_wind_fields,
    amrex::MultiFab& mean_field,
    const Geometry& geom)
{
    if (!enabled || !enkf) {
        amrex::Error("[DA] Data assimilation not enabled");
    }
    
    if (ensemble_wind_fields.size() != ensemble_size) {
        amrex::Error("[DA] Ensemble size mismatch in mean computation");
    }
    
    enkf->compute_ensemble_mean(ensemble_wind_fields, mean_field, geom);
}

void DataAssimilationManager::get_ensemble_uncertainty(
    const std::vector<amrex::MultiFab>& ensemble_wind_fields,
    amrex::MultiFab& std_dev_field,
    const Geometry& geom)
{
    if (!enabled || !enkf) {
        amrex::Error("[DA] Data assimilation not enabled");
    }
    
    if (ensemble_wind_fields.size() != ensemble_size) {
        amrex::Error("[DA] Ensemble size mismatch in uncertainty computation");
    }
    
    // Compute mean
    MultiFab mean_field;
    enkf->compute_ensemble_mean(ensemble_wind_fields, mean_field, geom);
    
    // Allocate std_dev field
    std_dev_field.define(ensemble_wind_fields[0].boxArray(),
                         ensemble_wind_fields[0].DistributionMap(),
                         3, 0);  // 3 components
    std_dev_field.setVal(0.0);
    
    // Compute variance: Var = E[(X - E[X])^2]
    for (const auto& member : ensemble_wind_fields) {
        MultiFab variance_term(ensemble_wind_fields[0].boxArray(),
                              ensemble_wind_fields[0].DistributionMap(),
                              3, 0);
        
        MultiFab::Copy(variance_term, member, 0, 0, 3, 0);
        MultiFab::Subtract(variance_term, mean_field, 0, 0, 3, 0);
        
        // Square each component
        for (MFIter mfi(variance_term); mfi.isValid(); ++mfi) {
            auto& arr = variance_term[mfi];
            auto& std_arr = std_dev_field[mfi];
            
            for (auto it = arr.begin(); it != arr.end(); ++it) {
                for (int n = 0; n < 3; ++n) {
                    std_arr(*it, n) += arr(*it, n) * arr(*it, n) / ensemble_size;
                }
            }
        }
    }
    
    // Take square root to get std dev
    for (MFIter mfi(std_dev_field); mfi.isValid(); ++mfi) {
        auto& arr = std_dev_field[mfi];
        
        for (auto it = arr.begin(); it != arr.end(); ++it) {
            for (int n = 0; n < 3; ++n) {
                arr(*it, n) = std::sqrt(std::max(0.0, arr(*it, n)));
            }
        }
    }
    
    amrex::Print() << "[DA] Ensemble uncertainty computed\n";
}

void DataAssimilationManager::write_diagnostics(const std::string& filename)
{
    if (!enabled || !enkf) {
        return;
    }
    
    enkf->print_statistics(filename);
    
    amrex::Print() << "[DA] Diagnostics written to " << filename << "\n";
}

int DataAssimilationManager::get_ensemble_size() const
{
    return ensemble_size;
}

Real DataAssimilationManager::get_localization_scale() const
{
    return localization_scale;
}
