#ifndef SCM_1D_SOLVER_H
#define SCM_1D_SOLVER_H

/**
 * @file scm_1d_solver.H
 * @brief Single Column Model (SCM) 1D solver for atmospheric boundary layer
 *
 * Implements a simplified 1D column model based on the physics from:
 * https://github.com/hgopalan/onedterrainsolver/blob/main/hrrr_1dsolver_terrain.py
 *
 * The solver operates on a 1D grid (z=0 to scm_height with uniform dz) and solves:
 * - Wind field (u, v) momentum equations with Coriolis forcing and turbulent mixing
 * - Temperature field with stratification
 * - Turbulent kinetic energy (TKE) and eddy viscosity using 1-equation turbulence model
 *
 * The geostrophic wind is iteratively adjusted until the wind speed at a reference
 * height matches the target wind speed within tolerance.
 *
 * Date Added: 2026-06-29
 */

#include <vector>
#include <cmath>
#include <algorithm>
#include <AMReX.H>

using namespace amrex;

/**
 * @struct SCMSimilarityParams
 * @brief Parameters for similarity theory calculations
 */
struct SCMSimilarityParams {
    int heat_flux_mode = 1;        ///< 1: heat flux, 2: surface temp, 3: heating rate, 4: MOL
    double heat_flux_value = 0.0;    ///< Value for specified heat flux mode
    double temperature_surface = 0.0; ///< Surface temperature (K) - for mode 2
    double temperature_reference = 300.0; ///< Reference temperature (K)
    double latitude = 45.0;          ///< Latitude (degrees) for Coriolis
    double z0 = 0.1;                 ///< Roughness length (m)
};

/**
 * @class SCM1DSolver
 * @brief 1D Single Column Model for atmospheric boundary layer simulation
 *
 * Simulates a 1D column atmosphere with:
 * - Configurable 1D grid (typically z=0-4000m with dz=4m)
 * - Independent of 3D domain grid spacing
 * - Closure: 1-equation TKE turbulence model with stability-dependent mixing length
 * - Boundary conditions: Monin-Obukhov similarity at surface
 * - Time stepping: Forward Euler with adaptive CFL criterion
 */
class SCM1DSolver {
public:
    /**
     * @brief Constructor
     * @param scm_height Maximum height for 1D solver (m)
     * @param scm_dz Grid spacing in 1D solver (m)
     * @param params Similarity theory parameters
     */
    SCM1DSolver(double scm_height, double scm_dz, const SCMSimilarityParams& params);

    /**
     * @brief Destructor
     */
    ~SCM1DSolver() = default;

    /**
     * @brief Run the SCM until convergence
     *
     * Iteratively adjusts geostrophic wind to match target wind speed at reference height.
     * Convergence criteria: |u_cfd - u_target| < tolerance and |v_cfd - v_target| < tolerance
     *
     * @param target_u_ref Target u wind speed at reference height (m/s)
     * @param target_v_ref Target v wind speed at reference height (m/s)
     * @param z_ref Reference height (m)
     * @param tolerance Convergence tolerance for geostrophic wind iteration (m/s)
     * @param max_iterations Maximum number of geostrophic wind updates
     * @param end_time Simulation end time for averaging (s)
     */
    void run_to_convergence(
        double target_u_ref,
        double target_v_ref,
        double z_ref,
        double tolerance = 0.1,
        int max_iterations = 100,
        double end_time = 20000.0
    );

    /**
     * @brief Get interpolated wind speed at a given height
     * @param z Height above ground (m)
     * @param u Output u-component
     * @param v Output v-component
     */
    void get_wind_at_height(double z, double& u, double& v) const;

    /**
     * @brief Get interpolated temperature at a given height
     * @param z Height above ground (m)
     * @return Temperature (K)
     */
    double get_temperature_at_height(double z) const;

    /**
     * @brief Get interpolated eddy viscosity at a given height
     * @param z Height above ground (m)
     * @return Eddy viscosity (m^2/s)
     */
    double get_eddy_viscosity_at_height(double z) const;

    /**
     * @brief Get full 1D profile
     * @param z Output heights (m)
     * @param u Output u-velocity (m/s)
     * @param v Output v-velocity (m/s)
     * @param temp Output temperature (K)
     * @param tke Output turbulent kinetic energy (m^2/s^2)
     * @param nut Output eddy viscosity (m^2/s)
     */
    void get_profile(
        std::vector<double>& z,
        std::vector<double>& u,
        std::vector<double>& v,
        std::vector<double>& temp,
        std::vector<double>& tke,
        std::vector<double>& nut
    ) const;

    /**
     * @brief Get Monin-Obukhov length (for diagnostics)
     * @return MOL (m)
     */
    double get_monin_obukhov_length() const { return mo_length_; }

    /**
     * @brief Get friction velocity (for diagnostics)
     * @return u* (m/s)
     */
    double get_friction_velocity() const { return ustar_; }

    /**
     * @brief Get sensible heat flux (for diagnostics)
     * @return Qh (W/m^2)
     */
    double get_sensible_heat_flux() const { return Qh_; }

private:
    // Grid and domain parameters
    double scm_height_;              ///< Max height of 1D domain (m)
    double scm_dz_;                  ///< Grid spacing in 1D domain (m)
    int nz_;                        ///< Number of grid points
    std::vector<double> z_;          ///< Height grid points (m)

    // Physical fields
    std::vector<double> ux_;         ///< u-velocity (m/s)
    std::vector<double> uy_;         ///< v-velocity (m/s)
    std::vector<double> temperature_; ///< Temperature (K)
    std::vector<double> tke_;        ///< Turbulent kinetic energy (m^2/s^2)
    std::vector<double> nut_;        ///< Eddy viscosity (m^2/s)
    std::vector<double> lscale_;     ///< Turbulent length scale (m)
    std::vector<double> nutPrime_;   ///< Turbulent viscosity parameter
    std::vector<double> sigmaT_;     ///< Prandtl number for temperature
    std::vector<double> Rt_;         ///< Richardson number

    // Parameters
    SCMSimilarityParams params_;
    double ug_;                      ///< Geostrophic u-wind (m/s)
    double vg_;                      ///< Geostrophic v-wind (m/s)
    double t_ref_;                   ///< Reference temperature (K)
    double tke_init_;                ///< Initial TKE (m^2/s^2)
    double pblh_;                    ///< PBL height (m)

    // Physical constants
    double coriolis_f_;              ///< Coriolis parameter (1/s)
    double ustar_;                   ///< Friction velocity (m/s)
    double thetastar_;               ///< Temperature scale (K)
    double mo_length_;               ///< Monin-Obukhov length (m)
    double Qh_;                      ///< Sensible heat flux (W/m^2)
    double Qb_;                      ///< Buoyancy flux (K m/s^2)

    // Simulation control
    double start_time_;              ///< Current simulation time (s)
    double end_time_;                ///< End time for simulation (s)
    double dt_;                      ///< Adaptive time step (s)
    int lower_;                    ///< Index of terrain surface (usually 0)

    // Temperature inversion parameters
    double inversion_height_;        ///< Height of inversion (m)
    double inversion_width_;         ///< Width of inversion region (m)
    double delta_inversion_;         ///< Inversion strength (K)
    double lapse_rate_;              ///< Environmental lapse rate (K/m)

    /**
     * @brief Initialize fields for simulation
     */
    void initialize_fields();

    /**
     * @brief Compute similarity theory corrections
     *
     * Calculates friction velocity, MOL, surface fluxes using specified heat flux mode
     */
    void compute_similarity();

    /**
     * @brief Update u-momentum at grid point i
     * @param i Grid index
     * @param dt Time step (s)
     */
    void update_windspeed_x(int i, double dt);

    /**
     * @brief Update v-momentum at grid point i
     * @param i Grid index
     * @param dt Time step (s)
     */
    void update_windspeed_y(int i, double dt);

    /**
     * @brief Update temperature at grid point i
     * @param i Grid index
     * @param dt Time step (s)
     */
    void update_temperature(int i, double dt);

    /**
     * @brief Update TKE and eddy viscosity at grid point i
     * @param i Grid index
     * @param dt Time step (s)
     */
    void update_turbulence(int i, double dt);

    /**
     * @brief Interpolate field at height z
     * @param z Height (m)
     * @param field The field to interpolate
     * @return Interpolated value
     */
    double interpolate_field(double z, const std::vector<double>& field) const;

    /**
     * @brief Apply similarity boundary conditions at surface
     */
    void apply_similarity_bc();

    /**
     * @brief Check convergence at reference height
     * @param u_current Current u at z_ref
     * @param v_current Current v at z_ref
     * @param u_target Target u at z_ref
     * @param v_target Target v at z_ref
     * @param tol Tolerance
     * @return true if converged
     */
    bool check_convergence(double u_current, double v_current, double u_target, double v_target, double tol) const;

    /**
     * @brief Update geostrophic wind using simple bisection-like adjustment
     * @param u_current Current wind speed u at z_ref
     * @param v_current Current wind speed v at z_ref
     * @param u_target Target wind speed u at z_ref
     * @param v_target Target wind speed v at z_ref
     */
    void adjust_geostrophic_wind(double u_current, double v_current, double u_target, double v_target);
};

#endif // SCM_1D_SOLVER_H
