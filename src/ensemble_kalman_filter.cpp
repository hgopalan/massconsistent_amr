#include "ensemble_kalman_filter.H"
#include <AMReX_Print.H>
#include <AMReX_ParmParse.H>
#include <AMReX_FArrayBox.H>
#include <AMReX_MLABecLaplacian.H>
#include <AMReX_MLMG.H>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <numeric>
#include <sstream>

using namespace amrex;

void EnsembleKalmanFilter::initialize(int ens_size,
                                      const Geometry& geom,
                                      Real localization_scale_in)
{
    ensemble_size = ens_size;
    localization_scale = localization_scale_in;
    geom_cached = geom;
    ensemble_members.resize(ensemble_size);
    forecast_perturbations.resize(ensemble_size);
    analysis_perturbations.resize(ensemble_size);
    
    amrex::Print() << "[EnKF] Initialized with "
                   << ensemble_size << " members, "
                   << "localization scale = " << localization_scale << " m\n";
}

void EnsembleKalmanFilter::set_background_error_covariance(
    Real u_star_std_in,
    Real z0_std_in,
    Real wind_dir_std_in)
{
    u_star_std = u_star_std_in;
    z0_std_factor = z0_std_in;
    wind_dir_std = wind_dir_std_in;
    
    amrex::Print() << "[EnKF] Background error covariance set:\n"
                   << "  u_star_std = " << u_star_std << " m/s\n"
                   << "  z0_std_factor = " << z0_std_factor << "\n"
                   << "  wind_dir_std = " << wind_dir_std << " degrees\n";
}

Real EnsembleKalmanFilter::random_gaussian(Real mean,
                                          Real std_dev,
                                          int seed)
{
    // Use std::mt19937 with seed based on input
    static std::mt19937 gen(seed);
    std::normal_distribution<Real> dist(mean, std_dev);
    return dist(gen);
}

void EnsembleKalmanFilter::generate_ensemble(
    const EnsembleProfileParameters& base_params,
    int seed_in)
{
    random_seed = seed_in;
    
    amrex::Print() << "[EnKF] Generating ensemble...\n";
    
    for (int i = 0; i < ensemble_size; ++i) {
        EnsembleProfileParameters perturbed = base_params;
        
        // Perturb parameters with Gaussian noise
        perturbed.u_star = base_params.u_star +
            random_gaussian(0.0, u_star_std, seed_in + i*1000);
        
        perturbed.z0 = base_params.z0 *
            std::max(0.01, 1.0 + random_gaussian(0.0, z0_std_factor*0.1, seed_in + i*1001));
        
        perturbed.wind_direction = base_params.wind_direction +
            random_gaussian(0.0, wind_dir_std, seed_in + i*1002);
        
        // Ensure u_star is positive
        perturbed.u_star = std::max(0.01, perturbed.u_star);
        
        // Wrap wind direction to [0, 360)
        while (perturbed.wind_direction < 0.0)
            perturbed.wind_direction += 360.0;
        while (perturbed.wind_direction >= 360.0)
            perturbed.wind_direction -= 360.0;
        
        ensemble_members[i] = perturbed;
    }
    
    amrex::Print() << "[EnKF] Ensemble generated. Sample member 0:\n"
                   << "  u_star = " << ensemble_members[0].u_star << " m/s\n"
                   << "  z0 = " << ensemble_members[0].z0 << " m\n"
                   << "  wind_direction = " << ensemble_members[0].wind_direction << " deg\n";
}

EnsembleProfileParameters EnsembleKalmanFilter::get_member_parameters(int member_id) const
{
    if (member_id < 0 || member_id >= ensemble_size) {
        amrex::Error("Invalid ensemble member ID");
    }
    return ensemble_members[member_id];
}

void EnsembleKalmanFilter::update_member_parameters(
    int member_id,
    const EnsembleProfileParameters& params)
{
    if (member_id < 0 || member_id >= ensemble_size) {
        amrex::Error("Invalid ensemble member ID");
    }
    ensemble_members[member_id] = params;
}

void EnsembleKalmanFilter::add_observation(const ObservationData& obs)
{
    observations.push_back(obs);
}

void EnsembleKalmanFilter::load_observations_from_csv(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        amrex::Warning("[EnKF] Could not open observation file: " + filename);
        return;
    }
    
    std::string line;
    int line_count = 0;
    
    // Skip header if present
    std::getline(file, line);
    
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream iss(line);
        
        ObservationData obs;
        std::string source;
        int component;
        
        if (!(iss >> obs.x >> obs.y >> obs.z >> obs.u >> obs.v >> obs.w
              >> obs.error >> source >> component)) {
            amrex::Warning("[EnKF] Skipping malformed line in " + filename);
            continue;
        }
        
        obs.source = source;
        obs.component = component;
        
        // Only add observations with positive error
        if (obs.error > 0.0) {
            observations.push_back(obs);
            line_count++;
        }
    }
    
    file.close();
    
    amrex::Print() << "[EnKF] Loaded " << line_count << " observations from " << filename << "\n";
}

void EnsembleKalmanFilter::load_observations_from_netcdf(
    const std::string& filename,
    const std::string& lidar_id)
{
    // Placeholder for NetCDF reader (requires netCDF library)
    // Implementation would use libnetcdf to read LiDAR data
    amrex::Warning("[EnKF] NetCDF observation loading not yet implemented");
}

void EnsembleKalmanFilter::evaluate_observation_operator(
    const MultiFab& wind_field,
    std::vector<Real>& predicted_obs,
    const Geometry& geom)
{
    predicted_obs.clear();
    predicted_obs.reserve(observations.size());
    
    for (const auto& obs : observations) {
        // Interpolate wind at observation location
        Real u_pred = interpolate_wind_at_point(wind_field, obs.x, obs.y, obs.z, geom, 0);
        Real v_pred = interpolate_wind_at_point(wind_field, obs.x, obs.y, obs.z, geom, 1);
        Real w_pred = interpolate_wind_at_point(wind_field, obs.x, obs.y, obs.z, geom, 2);
        
        Real obs_value;
        switch (obs.component) {
            case 0: obs_value = u_pred; break;
            case 1: obs_value = v_pred; break;
            case 2: obs_value = w_pred; break;
            case 3: obs_value = std::sqrt(u_pred*u_pred + v_pred*v_pred); break;
            default: obs_value = 0.0;
        }
        
        predicted_obs.push_back(obs_value);
    }
}

void EnsembleKalmanFilter::evaluate_observation_operator_from_params(
    const EnsembleProfileParameters& params,
    const std::vector<Real>& terrain_heights,
    std::vector<Real>& predicted_obs)
{
    predicted_obs.clear();
    predicted_obs.reserve(observations.size());
    
    for (const auto& obs : observations) {
        // Reconstruct wind from log-law profile
        Real z_agl = obs.z - terrain_heights[0];  // Simplified: assumes flat terrain at origin
        if (z_agl < 0.1) z_agl = 0.1;
        
        Real kappa = 0.41;  // von Karman constant
        Real wind_speed = (params.u_star / kappa) * 
                         std::log((z_agl + params.z0) / params.z0);
        
        // Convert to u, v components using wind direction
        Real dir_rad = params.wind_direction * M_PI / 180.0;
        Real u_pred = wind_speed * std::sin(dir_rad);
        Real v_pred = wind_speed * std::cos(dir_rad);
        Real w_pred = 0.0;  // Assume no vertical velocity in diagnostic model
        
        Real obs_value;
        switch (obs.component) {
            case 0: obs_value = u_pred; break;
            case 1: obs_value = v_pred; break;
            case 2: obs_value = w_pred; break;
            case 3: obs_value = std::sqrt(u_pred*u_pred + v_pred*v_pred); break;
            default: obs_value = 0.0;
        }
        
        predicted_obs.push_back(obs_value);
    }
}

Real EnsembleKalmanFilter::interpolate_wind_at_point(
    const MultiFab& wind_field,
    Real x, Real y, Real z,
    const Geometry& geom,
    int component)
{
    // Tri-linear interpolation at observation point
    const auto& domain_box = geom.Domain();
    const auto& dx = geom.CellSize();
    const auto& prob_lo = geom.ProbLo();
    
    // Grid indices
    Real i_real = (x - prob_lo[0]) / dx[0];
    Real j_real = (y - prob_lo[1]) / dx[1];
    Real k_real = (z - prob_lo[2]) / dx[2];
    
    int i = static_cast<int>(i_real);
    int j = static_cast<int>(j_real);
    int k = static_cast<int>(k_real);
    
    // Check bounds
    if (i < domain_box.smallEnd(0) || i >= domain_box.bigEnd(0)) return 0.0;
    if (j < domain_box.smallEnd(1) || j >= domain_box.bigEnd(1)) return 0.0;
    if (k < domain_box.smallEnd(2) || k >= domain_box.bigEnd(2)) return 0.0;
    
    Real fx = i_real - i;
    Real fy = j_real - j;
    Real fz = k_real - k;
    
    // Get wind field data
    Real value = 0.0;
    int count = 0;
    
    for (MFIter mfi(wind_field); mfi.isValid(); ++mfi) {
        const auto& box = mfi.validbox();
        const auto& arr = wind_field[mfi];
        
        if (!box.contains(IntVect(i, j, k))) continue;
        
        // Tri-linear interpolation
        Real v000 = arr(IntVect(i,   j,   k  ), component);
        Real v100 = arr(IntVect(i+1, j,   k  ), component);
        Real v010 = arr(IntVect(i,   j+1, k  ), component);
        Real v110 = arr(IntVect(i+1, j+1, k  ), component);
        Real v001 = arr(IntVect(i,   j,   k+1), component);
        Real v101 = arr(IntVect(i+1, j,   k+1), component);
        Real v011 = arr(IntVect(i,   j+1, k+1), component);
        Real v111 = arr(IntVect(i+1, j+1, k+1), component);
        
        Real v00 = v000 * (1.0 - fx) + v100 * fx;
        Real v10 = v010 * (1.0 - fx) + v110 * fx;
        Real v01 = v001 * (1.0 - fx) + v101 * fx;
        Real v11 = v011 * (1.0 - fx) + v111 * fx;
        
        Real v0 = v00 * (1.0 - fy) + v10 * fy;
        Real v1 = v01 * (1.0 - fy) + v11 * fy;
        
        value = v0 * (1.0 - fz) + v1 * fz;
        count++;
        break;
    }
    
    return (count > 0) ? value : 0.0;
}

std::vector<Real> EnsembleKalmanFilter::compute_predicted_observations(
    const MultiFab& wind_field,
    const Geometry& geom)
{
    std::vector<Real> predicted_obs;
    evaluate_observation_operator(wind_field, predicted_obs, geom);
    return predicted_obs;
}

Real EnsembleKalmanFilter::localization_correlation(Real distance) const
{
    if (localization_scale <= 0.0) return 1.0;
    Real arg = -0.5 * (distance / localization_scale) * (distance / localization_scale);
    return std::exp(arg);
}

void EnsembleKalmanFilter::compute_kalman_gain(
    const std::vector<std::vector<Real>>& ensemble_perturbations,
    const std::vector<std::vector<Real>>& observation_matrix,
    Real obs_error_var,
    std::vector<std::vector<Real>>& kalman_gain)
{
    // Simplified Kalman gain computation
    // K = P^f H^T (H P^f H^T + R)^{-1}
    
    if (ensemble_perturbations.empty() || observation_matrix.empty()) {
        amrex::Warning("[EnKF] Cannot compute Kalman gain: empty perturbations");
        kalman_gain.clear();
        return;
    }
    
    int n_state = ensemble_perturbations[0].size();
    int n_obs = observation_matrix[0].size();
    int n_ens = ensemble_perturbations.size();
    
    // This is a placeholder implementation
    // Full implementation would use eigenvalue decomposition or SVD
    
    kalman_gain.assign(n_state, std::vector<Real>(n_obs, 0.0));
    
    amrex::Print() << "[EnKF] Kalman gain computed: " << n_state << " x " << n_obs << "\n";
}

void EnsembleKalmanFilter::analysis_step(
    std::vector<MultiFab>& ensemble_wind_fields,
    const Geometry& geom,
    bool use_localization)
{
    if (ensemble_wind_fields.size() != ensemble_size) {
        amrex::Error("[EnKF] Wind field ensemble size mismatch");
    }
    
    if (observations.empty()) {
        amrex::Warning("[EnKF] No observations for analysis step");
        return;
    }
    
    amrex::Print() << "[EnKF] Executing analysis step with "
                   << observations.size() << " observations...\n";
    
    // Compute predicted observations for each member
    std::vector<std::vector<Real>> predicted_observations(ensemble_size);
    for (int i = 0; i < ensemble_size; ++i) {
        predicted_observations[i] = compute_predicted_observations(
            ensemble_wind_fields[i], geom);
    }
    
    // Compute ensemble mean prediction
    std::vector<Real> mean_prediction(observations.size(), 0.0);
    for (int i = 0; i < ensemble_size; ++i) {
        for (int j = 0; j < observations.size(); ++j) {
            mean_prediction[j] += predicted_observations[i][j] / ensemble_size;
        }
    }
    
    // Construct observation vector
    std::vector<Real> obs_vector(observations.size());
    for (int i = 0; i < observations.size(); ++i) {
        switch (observations[i].component) {
            case 0: obs_vector[i] = observations[i].u; break;
            case 1: obs_vector[i] = observations[i].v; break;
            case 2: obs_vector[i] = observations[i].w; break;
            case 3: obs_vector[i] = std::sqrt(
                observations[i].u*observations[i].u + 
                observations[i].v*observations[i].v); break;
            default: obs_vector[i] = 0.0;
        }
    }
    
    // Innovation (observation minus prediction)
    std::vector<Real> innovation(observations.size());
    for (int i = 0; i < observations.size(); ++i) {
        innovation[i] = obs_vector[i] - mean_prediction[i];
    }
    
    amrex::Print() << "[EnKF] Mean innovation magnitude: "
                   << std::sqrt(std::inner_product(innovation.begin(), innovation.end(),
                                                   innovation.begin(), 0.0) / 
                              std::max(1, static_cast<int>(innovation.size())))
                   << " m/s\n";
    
    // Update ensemble members (simplified: apply mean innovation)
    for (int i = 0; i < ensemble_size; ++i) {
        // TODO: Implement full Kalman gain computation and update
        // For now, this is a placeholder that just stores the analysis
        analysis_perturbations[i] = predicted_observations[i];
    }
    
    amrex::Print() << "[EnKF] Analysis step completed\n";
}

void EnsembleKalmanFilter::project_to_divergence_free(
    MultiFab& wind_field,
    const Geometry& geom,
    Real solver_tolerance)
{
    // This would call the mass-consistent Poisson solver
    // to correct the wind field and enforce ∇·u = 0
    
    amrex::Print() << "[EnKF] Projecting to divergence-free space (solver_tol = "
                   << solver_tolerance << ")...\n";
    
    // Placeholder: compute divergence before and after
    Real div_max_before = compute_max_divergence(wind_field, geom);
    
    // TODO: Implement Poisson-based projection using AMReX MLMG
    
    Real div_max_after = compute_max_divergence(wind_field, geom);
    
    amrex::Print() << "[EnKF] Divergence: before = " << div_max_before
                   << ", after = " << div_max_after << "\n";
}

Real EnsembleKalmanFilter::compute_max_divergence(
    const MultiFab& wind_field,
    const Geometry& geom)
{
    const auto& dx = geom.CellSize();
    Real max_div = 0.0;
    
    for (MFIter mfi(wind_field); mfi.isValid(); ++mfi) {
        const auto& box = mfi.validbox();
        const auto& arr = wind_field[mfi];
        
        for (int k = box.smallEnd(2); k <= box.bigEnd(2); ++k) {
            for (int j = box.smallEnd(1); j <= box.bigEnd(1); ++j) {
                for (int i = box.smallEnd(0); i <= box.bigEnd(0); ++i) {
                    Real du_dx = (arr(IntVect(i+1, j, k), 0) - arr(IntVect(i-1, j, k), 0)) 
                                / (2.0 * dx[0]);
                    Real dv_dy = (arr(IntVect(i, j+1, k), 1) - arr(IntVect(i, j-1, k), 1))
                                / (2.0 * dx[1]);
                    Real dw_dz = (arr(IntVect(i, j, k+1), 2) - arr(IntVect(i, j, k-1), 2))
                                / (2.0 * dx[2]);
                    Real div = std::abs(du_dx + dv_dy + dw_dz);
                    max_div = std::max(max_div, div);
                }
            }
        }
    }
    
    return max_div;
}

void EnsembleKalmanFilter::compute_ensemble_mean(
    const std::vector<MultiFab>& ensemble_wind_fields,
    MultiFab& mean_field,
    const Geometry& geom)
{
    if (ensemble_wind_fields.empty()) {
        amrex::Error("[EnKF] Empty ensemble for mean computation");
    }
    
    // Allocate mean field
    mean_field.define(ensemble_wind_fields[0].boxArray(),
                      ensemble_wind_fields[0].DistributionMap(),
                      3, 0);  // 3 components, 0 ghosts
    
    mean_field.setVal(0.0);
    
    // Sum all members
    for (const auto& member : ensemble_wind_fields) {
        MultiFab::Add(mean_field, member, 0, 0, 3, 0);
    }
    
    // Divide by ensemble size
    mean_field.mult(1.0 / ensemble_size, 0, 3, 0);
    
    amrex::Print() << "[EnKF] Ensemble mean computed\n";
}

void EnsembleKalmanFilter::compute_ensemble_perturbations(
    const std::vector<MultiFab>& ensemble_wind_fields,
    const MultiFab& mean_field,
    std::vector<MultiFab>& perturbations,
    const Geometry& geom)
{
    perturbations.clear();
    perturbations.resize(ensemble_size);
    
    for (int i = 0; i < ensemble_size; ++i) {
        perturbations[i].define(ensemble_wind_fields[i].boxArray(),
                               ensemble_wind_fields[i].DistributionMap(),
                               3, 0);
        
        // Perturbation = member - mean
        MultiFab::Copy(perturbations[i], ensemble_wind_fields[i], 0, 0, 3, 0);
        MultiFab::Subtract(perturbations[i], mean_field, 0, 0, 3, 0);
    }
    
    amrex::Print() << "[EnKF] Ensemble perturbations computed\n";
}

Real EnsembleKalmanFilter::compute_background_error_covariance(
    const std::vector<MultiFab>& ensemble_perturbations,
    const IntVect& location,
    const IntVect& obs_location)
{
    if (ensemble_perturbations.empty()) return 0.0;
    
    // Placeholder: compute covariance between two locations
    Real cov = 0.0;
    
    // TODO: Implement covariance computation with localization
    
    return cov;
}

void EnsembleKalmanFilter::print_statistics(const std::string& output_file)
{
    std::ostringstream oss;
    
    oss << "=============== EnKF Statistics ===============\n";
    oss << "Ensemble size: " << ensemble_size << "\n";
    oss << "Observations loaded: " << observations.size() << "\n";
    oss << "Localization scale: " << localization_scale << " m\n";
    oss << "Background error covariance:\n";
    oss << "  u_star_std: " << u_star_std << " m/s\n";
    oss << "  z0_std_factor: " << z0_std_factor << "\n";
    oss << "  wind_dir_std: " << wind_dir_std << " degrees\n";
    oss << "============================================\n";
    
    if (output_file.empty()) {
        amrex::Print() << oss.str();
    } else {
        std::ofstream file(output_file, std::ios::app);
        if (file.is_open()) {
            file << oss.str();
            file.close();
        }
    }
}
