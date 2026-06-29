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
#include <AMReX_Real.H>

using namespace amrex;

/**
 * @struct SCMSimilarityParams
 * @brief Parameters for similarity theory calculations
 */
struct SCMSimilarityParams {
    int heat_flux_mode = 1;        ///< 1: heat flux, 2: surface temp, 3: heating rate, 4: MOL
    Real heat_flux_value = 0.0;    ///< Value for specified heat flux mode
    Real temperature_surface = 0.0; ///< Surface temperature (K) - for mode 2
    Real temperature_reference = 300.0; ///< Reference temperature (K)
    Real latitude = 45.0;          ///< Latitude (degrees) for Coriolis
    Real z0 = 0.1;                 ///< Roughness length (m)
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
    SCM1DSolver(Real scm_height, Real scm_dz, const SCMSimilarityParams& params);

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
        Real target_u_ref,
        Real target_v_ref,
        Real z_ref,
        Real tolerance = 0.1,
        int max_iterations = 100,
        Real end_time = 20000.0
    );

    /**
     * @brief Get interpolated wind speed at a given height
     * @param z Height above ground (m)
     * @param u Output u-component
     * @param v Output v-component
     */
    void get_wind_at_height(Real z, Real& u, Real& v) const;

    /**
     * @brief Get interpolated temperature at a given height
     * @param z Height above ground (m)
     * @return Temperature (K)
     */
    Real get_temperature_at_height(Real z) const;

    /**
     * @brief Get interpolated eddy viscosity at a given height
     * @param z Height above ground (m)
     * @return Eddy viscosity (m^2/s)
     */
    Real get_eddy_viscosity_at_height(Real z) const;

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
        std::vector<Real>& z,
        std::vector<Real>& u,
        std::vector<Real>& v,
        std::vector<Real>& temp,
        std::vector<Real>& tke,
        std::vector<Real>& nut
    ) const;

    /**
     * @brief Get Monin-Obukhov length (for diagnostics)
     * @return MOL (m)
     */
    Real get_monin_obukhov_length() const { return mo_length_; }

    /**
     * @brief Get friction velocity (for diagnostics)
     * @return u* (m/s)
     */
    Real get_friction_velocity() const { return ustar_; }

    /**
     * @brief Get sensible heat flux (for diagnostics)
     * @return Qh (W/m^2)
     */
    Real get_sensible_heat_flux() const { return Qh_; }

private:
    // Grid and domain parameters
    Real scm_height_;              ///< Max height of 1D domain (m)
    Real scm_dz_;                  ///< Grid spacing in 1D domain (m)
    int nz_;                        ///< Number of grid points
    std::vector<Real> z_;          ///< Height grid points (m)

    // Physical fields
    std::vector<Real> ux_;         ///< u-velocity (m/s)
    std::vector<Real> uy_;         ///< v-velocity (m/s)
    std::vector<Real> temperature_; ///< Temperature (K)
    std::vector<Real> tke_;        ///< Turbulent kinetic energy (m^2/s^2)
    std::vector<Real> nut_;        ///< Eddy viscosity (m^2/s)
    std::vector<Real> lscale_;     ///< Turbulent length scale (m)
    std::vector<Real> nutPrime_;   ///< Turbulent viscosity parameter
    std::vector<Real> sigmaT_;     ///< Prandtl number for temperature
    std::vector<Real> Rt_;         ///< Richardson number

    // Parameters
    SCMSimilarityParams params_;
    Real ug_;                      ///< Geostrophic u-wind (m/s)
    Real vg_;                      ///< Geostrophic v-wind (m/s)
    Real t_ref_;                   ///< Reference temperature (K)
    Real tke_init_;                ///< Initial TKE (m^2/s^2)
    Real pblh_;                    ///< PBL height (m)

    // Physical constants
    Real coriolis_f_;              ///< Coriolis parameter (1/s)
    Real ustar_;                   ///< Friction velocity (m/s)
    Real thetastar_;               ///< Temperature scale (K)
    Real mo_length_;               ///< Monin-Obukhov length (m)
    Real Qh_;                      ///< Sensible heat flux (W/m^2)
    Real Qb_;                      ///< Buoyancy flux (K m/s^2)

    // Simulation control
    Real start_time_;              ///< Current simulation time (s)
    Real end_time_;                ///< End time for simulation (s)
    Real dt_;                      ///< Adaptive time step (s)
    int lower_;                    ///< Index of terrain surface (usually 0)

    // Temperature inversion parameters
    Real inversion_height_;        ///< Height of inversion (m)
    Real inversion_width_;         ///< Width of inversion region (m)
    Real delta_inversion_;         ///< Inversion strength (K)
    Real lapse_rate_;              ///< Environmental lapse rate (K/m)

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
    void update_windspeed_x(int i, Real dt);

    /**
     * @brief Update v-momentum at grid point i
     * @param i Grid index
     * @param dt Time step (s)
     */
    void update_windspeed_y(int i, Real dt);

    /**
     * @brief Update temperature at grid point i
     * @param i Grid index
     * @param dt Time step (s)
     */
    void update_temperature(int i, Real dt);

    /**
     * @brief Update TKE and eddy viscosity at grid point i
     * @param i Grid index
     * @param dt Time step (s)
     */
    void update_turbulence(int i, Real dt);

    /**
     * @brief Interpolate field at height z
     * @param z Height (m)
     * @param field The field to interpolate
     * @return Interpolated value
     */
    Real interpolate_field(Real z, const std::vector<Real>& field) const;

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
    bool check_convergence(Real u_current, Real v_current, Real u_target, Real v_target, Real tol) const;

    /**
     * @brief Update geostrophic wind using simple bisection-like adjustment
     * @param u_current Current wind speed u at z_ref
     * @param v_current Current wind speed v at z_ref
     * @param u_target Target wind speed u at z_ref
     * @param v_target Target wind speed v at z_ref
     */
    void adjust_geostrophic_wind(Real u_current, Real v_current, Real u_target, Real v_target);
};

#endif // SCM_1D_SOLVER_H
