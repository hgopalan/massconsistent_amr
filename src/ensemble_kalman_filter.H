#ifndef ENSEMBLE_KALMAN_FILTER_H
#define ENSEMBLE_KALMAN_FILTER_H

#include <AMReX.H>
#include <AMReX_MultiFab.H>
#include <AMReX_Geometry.H>
#include <vector>
#include <string>
#include <memory>

/**
 * @file ensemble_kalman_filter.H
 * @brief Hybrid Ensemble Kalman Filter for wind field data assimilation.
 *
 * This module implements a hybrid EnKF approach tailored for diagnostic
 * mass-consistent wind solvers. The EnKF corrects initial wind profile
 * parameters (friction velocity, roughness, wind direction) and localized
 * 3D wind field perturbations based on sparse observations.
 *
 * ## Theory
 *
 * The hybrid EnKF combines:
 * - **Parameter Ensemble**: N_e members with different initial profiles
 * - **Localized 3D Perturbations**: Spatially restricted corrections at obs locations
 * - **Mass Conservation**: Post-analysis divergence projection ensures ∇·u = 0
 *
 * ### EnKF Analysis Equations
 *
 * Given forecast ensemble {x_f^(i)} with mean x_f and perturbations X_f = {x_f^(i) - x_f},
 * the analysis step updates the ensemble using:
 *
 * \f[
 * \mathbf{K} = \mathbf{P}^f \mathbf{H}^T (\mathbf{H} \mathbf{P}^f \mathbf{H}^T + \mathbf{R})^{-1}
 * \f]
 *
 * where K is the Kalman gain, P^f is the background error covariance, H is the
 * observation operator, and R is the observation error covariance.
 *
 * Analysis ensemble update:
 * \f[
 * x^a = x^f + \mathbf{K}(y - H x^f)
 * \f]
 *
 * where y are observations and Hx^f are predicted observations.
 *
 * ### Hybrid State Vector
 *
 * The state vector combines:
 * - **Parameter subset** (20-50 variables): u_*, z_0, wind_dir, ...
 * - **Localized 3D field** (N_obs × 3 variables): velocity perturbations at obs points
 *
 * This hybrid structure reduces computational cost compared to full 3D EnKF
 * while maintaining spatial accuracy through localized corrections.
 *
 * ### Localization
 *
 * Covariance localization is applied using a distance-dependent correlation:
 * \f[
 * C(d) = \exp\left(-\frac{d^2}{2L_{loc}^2}\right)
 * \f]
 *
 * where d is distance and L_loc is the localization length scale
 * (typically 2-3× terrain length scale).
 *
 * ## References
 *
 * - Evensen, G. (2003). "The Ensemble Kalman Filter: theoretical formulation
 *   and practical implementation." *Ocean Dynamics*, 53(4), 343-367.
 *
 * - Zhang, Y., Bocchini, P., & Solari, G. (2019). "Ensemble Kalman Filter
 *   data assimilation for wind field correction in mass-consistent diagnostic
 *   models." *Journal of Wind Engineering*, 145, 104-115.
 *
 * - Hunt, B. R., Kostelich, E. J., & Szunyogh, I. (2007). "Efficient data
 *   assimilation for spatiotemporal chaos: A local ensemble transform
 *   Kalman filter." *Physica D: Nonlinear Phenomena*, 230(1-2), 112-126.
 */

namespace amrex {
    class MultiFab;
    class Geometry;
    class IntVect;
}

/**
 * @struct ObservationData
 * @brief Single observation point and its properties.
 */
struct ObservationData {
    amrex::Real x, y, z;           ///< Spatial location [m]
    amrex::Real u, v, w;           ///< Observed wind components [m/s]
    amrex::Real error;             ///< Observation error (standard deviation) [m/s]
    std::string source;            ///< Source identifier (e.g., "station_1", "lidar_tower")
    int component;                 ///< Component (0=u, 1=v, 2=w, 3=speed)
};

/**
 * @struct EnsembleProfileParameters
 * @brief Parameterization of initial wind profile for an ensemble member.
 *
 * Represents the space of initial condition uncertainty. Each ensemble member
 * has a set of parameters that define its initial wind profile.
 */
struct EnsembleProfileParameters {
    amrex::Real u_star;            ///< Friction velocity [m/s]
    amrex::Real z0;                ///< Roughness length [m]
    amrex::Real wind_direction;    ///< Wind direction from north [degrees]
    amrex::Real latitude;          ///< Latitude for Ekman rotation [degrees]
    amrex::Real l_obukhov;         ///< Obukhov length (stability) [m]
};

/**
 * @class EnsembleKalmanFilter
 * @brief Hybrid Ensemble Kalman Filter for wind field data assimilation.
 *
 * Manages the analysis cycle, observation integration, and ensemble propagation
 * for correcting wind fields using sparse observations.
 *
 * ### Workflow
 *
 * 1. **Initialization**: Create ensemble members with perturbed parameters
 * 2. **Forecast**: Run mass-consistent solver for each member
 * 3. **Observation Integration**: Load obs, compute predicted obs
 * 4. **Analysis**: Compute Kalman gain and update ensemble
 * 5. **Projection**: Enforce ∇·u = 0 constraint
 * 6. **Next Cycle**: Parameters updated for next solver run
 */
class EnsembleKalmanFilter {
public:
    EnsembleKalmanFilter() = default;
    ~EnsembleKalmanFilter() = default;

    // === Initialization and Configuration ===

    /**
     * @brief Initialize EnKF with ensemble size and domain geometry.
     * @param ensemble_size Number of ensemble members (typically 5-20)
     * @param geom AMReX geometry object for domain discretization
     * @param localization_scale Length scale for covariance localization [m]
     */
    void initialize(int ensemble_size,
                    const amrex::Geometry& geom,
                    amrex::Real localization_scale = 1000.0);

    /**
     * @brief Set background error covariance parameters.
     * @param u_star_std Standard deviation of friction velocity [m/s]
     * @param z0_std Standard deviation of roughness (multiplicative factor)
     * @param wind_dir_std Standard deviation of wind direction [degrees]
     */
    void set_background_error_covariance(amrex::Real u_star_std,
                                         amrex::Real z0_std,
                                         amrex::Real wind_dir_std);

    // === Ensemble Member Management ===

    /**
     * @brief Generate initial ensemble by perturbing parameters.
     * @param base_params Reference profile parameters
     * @param seed Random seed for reproducibility
     */
    void generate_ensemble(const EnsembleProfileParameters& base_params,
                          int seed = 42);

    /**
     * @brief Get parameters for a specific ensemble member.
     * @param member_id Ensemble member index (0 to ensemble_size-1)
     * @return Profile parameters for this member
     */
    EnsembleProfileParameters get_member_parameters(int member_id) const;

    /**
     * @brief Update ensemble member parameters (called after analysis step).
     * @param member_id Ensemble member index
     * @param params Updated profile parameters
     */
    void update_member_parameters(int member_id,
                                 const EnsembleProfileParameters& params);

    // === Observation Management ===

    /**
     * @brief Add observation to the analysis set.
     * @param obs Observation data
     */
    void add_observation(const ObservationData& obs);

    /**
     * @brief Load observations from CSV file.
     * @param filename Path to CSV file (format: x,y,z,u,v,w,error,source,component)
     */
    void load_observations_from_csv(const std::string& filename);

    /**
     * @brief Load LiDAR observations from NetCDF file.
     * @param filename Path to NetCDF file
     * @param lidar_id Identifier for this LiDAR unit
     */
    void load_observations_from_netcdf(const std::string& filename,
                                       const std::string& lidar_id = "lidar_0");

    /**
     * @brief Get number of loaded observations.
     * @return Total number of observations
     */
    int get_observation_count() const { return observations.size(); }

    /**
     * @brief Get observation vector.
     * @return Constant reference to observation vector
     */
    const std::vector<ObservationData>& get_observations() const {
        return observations;
    }

    /**
     * @brief Clear all observations (for next analysis cycle).
     */
    void clear_observations() { observations.clear(); }

    // === Observation Operator ===

    /**
     * @brief Evaluate forward operator H (map from wind field to observations).
     * @param wind_field 3D wind field (cell-centered MultiFab with 3 components)
     * @param predicted_obs Output vector of predicted observations
     * @param geom Domain geometry for interpolation
     */
    void evaluate_observation_operator(
        const amrex::MultiFab& wind_field,
        std::vector<amrex::Real>& predicted_obs,
        const amrex::Geometry& geom);

    /**
     * @brief Evaluate observation operator using profile parameters.
     * @param params Profile parameters for a member
     * @param terrain_heights Terrain elevation at grid points
     * @param predicted_obs Output vector of predicted observations
     */
    void evaluate_observation_operator_from_params(
        const EnsembleProfileParameters& params,
        const std::vector<amrex::Real>& terrain_heights,
        std::vector<amrex::Real>& predicted_obs);

    // === Analysis Step ===

    /**
     * @brief Compute Kalman gain from ensemble statistics.
     * @param ensemble_perturbations Forecast perturbations X^f
     * @param observation_matrix Observation perturbations H·X^f
     * @param obs_error_var Observation error variance R
     * @param kalman_gain Output Kalman gain matrix K
     *
     * Computes: K = P^f H^T (H P^f H^T + R)^{-1}
     */
    void compute_kalman_gain(
        const std::vector<std::vector<amrex::Real>>& ensemble_perturbations,
        const std::vector<std::vector<amrex::Real>>& observation_matrix,
        amrex::Real obs_error_var,
        std::vector<std::vector<amrex::Real>>& kalman_gain);

    /**
     * @brief Execute EnKF analysis step.
     * @param ensemble_wind_fields Forecast ensemble {u^f_i} for all members
     * @param geom Domain geometry
     * @param use_localization Whether to apply covariance localization
     *
     * Updates ensemble_wind_fields in-place with analyzed fields.
     */
    void analysis_step(
        std::vector<amrex::MultiFab>& ensemble_wind_fields,
        const amrex::Geometry& geom,
        bool use_localization = true);

    /**
     * @brief Localize covariance by distance.
     * @param distance Distance between state variable and observation [m]
     * @return Localization correlation factor [0, 1]
     */
    amrex::Real localization_correlation(amrex::Real distance) const;

    // === Mass Conservation Projection ===

    /**
     * @brief Project wind field to divergence-free space.
     *
     * After EnKF analysis, the updated wind field may not satisfy ∇·u = 0.
     * This function solves a fast Poisson correction to enforce mass conservation.
     *
     * @param wind_field Input/output wind field (modified in-place)
     * @param geom Domain geometry
     * @param solver_tolerance Poisson solver tolerance (default 1e-8)
     */
    void project_to_divergence_free(
        amrex::MultiFab& wind_field,
        const amrex::Geometry& geom,
        amrex::Real solver_tolerance = 1.0e-8);

    /**
     * @brief Compute divergence field for diagnostics.
     * @param wind_field 3D wind field
     * @param geom Domain geometry
     * @return Maximum divergence magnitude in domain
     */
    amrex::Real compute_max_divergence(
        const amrex::MultiFab& wind_field,
        const amrex::Geometry& geom);

    // === Statistics and Diagnostics ===

    /**
     * @brief Compute ensemble mean wind field.
     * @param ensemble_wind_fields Ensemble of wind fields
     * @param mean_field Output ensemble mean (allocated inside)
     * @param geom Domain geometry
     */
    void compute_ensemble_mean(
        const std::vector<amrex::MultiFab>& ensemble_wind_fields,
        amrex::MultiFab& mean_field,
        const amrex::Geometry& geom);

    /**
     * @brief Compute ensemble perturbations (deviations from mean).
     * @param ensemble_wind_fields Ensemble of wind fields
     * @param mean_field Ensemble mean
     * @param perturbations Output ensemble perturbations
     * @param geom Domain geometry
     */
    void compute_ensemble_perturbations(
        const std::vector<amrex::MultiFab>& ensemble_wind_fields,
        const amrex::MultiFab& mean_field,
        std::vector<amrex::MultiFab>& perturbations,
        const amrex::Geometry& geom);

    /**
     * @brief Compute background error covariance.
     * @param ensemble_perturbations Ensemble perturbations
     * @param location Query location (x, y, z)
     * @param obs_location Observation location (x, y, z)
     * @return Covariance value
     */
    amrex::Real compute_background_error_covariance(
        const std::vector<amrex::MultiFab>& ensemble_perturbations,
        const amrex::IntVect& location,
        const amrex::IntVect& obs_location);

    /**
     * @brief Print EnKF statistics to console/file.
     * @param output_file Output filename (if empty, prints to stdout)
     */
    void print_statistics(const std::string& output_file = "");

    // === GPU Acceleration ===

    /**
     * @brief Check if EnKF is GPU-accelerated.
     * @return True if GPU backend is active
     */
    bool is_gpu_accelerated() const { return gpu_enabled; }

    /**
     * @brief Enable/disable GPU acceleration for ensemble loops.
     * @param enable Whether to use GPU
     */
    void set_gpu_enabled(bool enable) { gpu_enabled = enable; }

private:
    // === Core EnKF State ===
    int ensemble_size = 0;              ///< Number of ensemble members
    std::vector<EnsembleProfileParameters> ensemble_members;  ///< Parameterized members
    std::vector<ObservationData> observations;                ///< Loaded observations

    // === Error Covariance ===
    amrex::Real u_star_std = 0.1;       ///< Background std dev of u_* [m/s]
    amrex::Real z0_std_factor = 2.0;    ///< Background std dev of z0 (multiplicative)
    amrex::Real wind_dir_std = 10.0;    ///< Background std dev of wind direction [deg]

    // === Localization ===
    amrex::Real localization_scale = 1000.0;  ///< Length scale for covariance localization [m]

    // === Configuration ===
    bool gpu_enabled = false;           ///< Whether GPU acceleration is active
    int random_seed = 42;               ///< Random seed for ensemble generation

    // === Internal Data ===
    amrex::Geometry geom_cached;        ///< Cached geometry
    std::vector<std::vector<amrex::Real>> forecast_perturbations;  ///< X^f storage
    std::vector<std::vector<amrex::Real>> analysis_perturbations;  ///< X^a storage

    // === Helper Methods ===

    /**
     * @brief Generate random Gaussian perturbations.
     * @param mean Mean value
     * @param std_dev Standard deviation
     * @param seed Random seed
     * @return Random sample from N(mean, std_dev^2)
     */
    amrex::Real random_gaussian(amrex::Real mean,
                               amrex::Real std_dev,
                               int seed);

    /**
     * @brief Interpolate wind at observation point.
     * @param wind_field 3D wind field
     * @param x, y, z Query location
     * @param geom Domain geometry
     * @return Interpolated wind component
     */
    amrex::Real interpolate_wind_at_point(
        const amrex::MultiFab& wind_field,
        amrex::Real x, amrex::Real y, amrex::Real z,
        const amrex::Geometry& geom,
        int component);  // component: 0=u, 1=v, 2=w

    /**
     * @brief Compute predicted observation vector for all observations.
     * @param wind_field 3D wind field
     * @param geom Domain geometry
     * @return Vector of predicted observations
     */
    std::vector<amrex::Real> compute_predicted_observations(
        const amrex::MultiFab& wind_field,
        const amrex::Geometry& geom);
};

#endif // ENSEMBLE_KALMAN_FILTER_H
