.. _usage:

Usage Guide
===========

This section describes how to configure and run the Mass-Consistent AMR Wind Solver, including details on inputs, parameter settings, advanced physical features (including wind turbine wake models), and complete step-by-step tutorials.

.. contents:: Topics
   :local:
   :depth: 2

Running the Solver
------------------

To run the wind solver, pass an input file to the ``wind_solver`` executable::

    ./build/wind_solver inputs.i

Or supply parameters directly on the command line, which will override values inside the input file::

    ./build/wind_solver terrain_file=terrain.csv U_ref=8.0 z0=0.05

Input File Format
-----------------

The input file uses AMReX ``ParmParse`` syntax — one ``key = value`` pair per line. Lines beginning with ``#`` are comments.

Example ``inputs.i``::

    # Mass-consistent wind solver input file
    terrain_file  = terrain.csv   # X Y Z point cloud
    init_mode     = loglaw        # Wind initialization method
    U_ref         = 8.0           # reference x-wind [m/s]
    V_ref         = 0.0           # reference y-wind [m/s]
    z_ref         = 10.0          # reference height above local terrain [m]
    z0            = 0.05          # aerodynamic roughness length [m]

    dx            = 30.0          # grid spacing x [m]
    dy            = 30.0          # grid spacing y [m]
    dz            = 10.0          # grid spacing z [m]
    domain_height = 300.0         # vertical extent above max terrain [m]

    alpha_h       = 1.0           # horizontal anisotropy coefficient
    alpha_v       = 1.0           # vertical anisotropy coefficient

    mlmg_verbose  = 1             # MLMG verbosity (0=silent, 4=max)
    tol_rel       = 1.e-8         # relative convergence tolerance
    max_grid_size = 32            # max AMReX box size per dimension

    deriv_method  = central       # derivative method (central, weno3, weno5)
    plot_file     = plt_wind      # output plotfile prefix

    extract_agl   = 15.0          # extract CSV at 15 m AGL
    extract_file  = wind_extract.csv

Parameter Reference
-------------------

.. list-table::
   :header-rows: 1
   :widths: 22 15 63

   * - Parameter
     - Default
     - Description
   * - ``terrain_file``
     - ``terrain.csv``
     - Path to terrain point-cloud file (X Y Z, whitespace or comma separated). Set to ``synthetic`` (EXPERIMENTAL) to generate synthetic terrain programmatically.
   * - **Synthetic Terrain Parameters (EXPERIMENTAL)**
     -
     -
   * - ``synthetic_type``
     - ``multi_gaussian_hill``
     - Synthetic terrain model: ``gaussian_hill`` (single hill) or ``multi_gaussian_hill`` (sum of multiple hills).
   * - ``synthetic_xmin`` / ``synthetic_xmax``
     - ``0.0`` / ``300.0``
     - Spatial bounds in X dimension [m] for the generated synthetic terrain.
   * - ``synthetic_ymin`` / ``synthetic_ymax``
     - ``0.0`` / ``300.0``
     - Spatial bounds in Y dimension [m] for the generated synthetic terrain.
   * - ``synthetic_nx`` / ``synthetic_ny``
     - ``11`` / ``11``
     - Number of grid points in X and Y directions for generating the synthetic terrain.
   * - ``synthetic_peak`` / ``synthetic_sigma``
     - ``50.0`` / ``60.0``
     - Single peak elevation [m] and Gaussian width [m] (used when ``synthetic_type = gaussian_hill``).
   * - ``synthetic_center_x`` / ``synthetic_center_y``
     - center of domain
     - Coordinates of the single peak center (used when ``synthetic_type = gaussian_hill``).
   * - ``synthetic_peaks``
     - ``[50.0, 30.0]``
     - List of peak elevations [m] for each hill (used when ``synthetic_type = multi_gaussian_hill``). Space-separated.
   * - ``synthetic_sigmas``
     - ``[60.0, 40.0]``
     - List of Gaussian width parameters [m] for each hill (used when ``synthetic_type = multi_gaussian_hill``). Space-separated.
   * - ``synthetic_centers_x`` / ``synthetic_centers_y``
     - ``[100.0, 200.0]`` / ``[150.0, 150.0]``
     - List of peak center coordinates [m] in X and Y (used when ``synthetic_type = multi_gaussian_hill``). Space-separated.
   * - **Wind Initialization Mode**
     -
     -
   * - ``init_mode``
     - ``loglaw``
     - Wind field initialization method: ``loglaw`` (log-law profile), ``uniform`` (constant wind), ``raws`` (interpolate from RAWS file), ``surface_data`` (HRRR surface parameters), ``powerlaw`` (power-law profile), ``windfield`` (reads pre-mapped CSV data), ``ekman_spiral`` (analytical classical Ekman spiral profile).
   * - ``U_ref``
     - ``10.0``
     - Reference wind x-component [m/s] at height ``z_ref``.
   * - ``V_ref``
     - ``0.0``
     - Reference wind y-component [m/s] at height ``z_ref``.
   * - ``z_ref``
     - ``10.0``
     - Reference height above the local terrain surface [m].
   * - ``z0``
     - ``0.1``
     - Aerodynamic roughness length [m].
   * - ``powerlaw_exponent``
     - ``0.143``
     - Power-law exponent for ``powerlaw`` mode.
   * - ``enable_topographic_shielding``
     - ``false``
     - Enable topographic barrier shielding for meteorological station interpolations (e.g. in ``raws`` or ``windfield`` modes).
   * - **Grid Parameters**
     -
     -
   * - ``dx``
     - ``30.0``
     - Horizontal grid spacing in x [m].
   * - ``dy``
     - ``30.0``
     - Horizontal grid spacing in y [m].
   * - ``dz``
     - ``30.0``
     - Vertical grid spacing [m].
   * - ``domain_height``
     - ``300.0``
     - Vertical domain extent above the maximum terrain elevation [m].
   * - **Mass-Consistency Parameters**
     -
     -
   * - ``alpha_h``
     - ``1.0``
     - Horizontal Lagrange anisotropy coefficient.
   * - ``alpha_v``
     - ``1.0``
     - Vertical Lagrange anisotropy coefficient (constant).
   * - ``use_height_dependent_alpha_v``
     - ``false``
     - Enable height-dependent vertical anisotropy (linear variation).
   * - ``alpha_v_surface``
     - ``alpha_v``
     - Vertical anisotropy coefficient at ground level (z=z_lo).
   * - ``alpha_v_top``
     - ``alpha_v``
     - Vertical anisotropy coefficient at domain top (z=z_hi).
   * - **IDW Meteorological Interpolation Parameters**
     -
     -
   * - ``idw_gamma``
     - ``1.0``
     - Anisotropic vertical scaling parameter :math:`\gamma` for 3D meteorological interpolation. A value :math:`\gamma \gg 1` penalizes vertical distances to preserve atmospheric profile stratification.
   * - ``idw_exponent``
     - ``2.0``
     - User-configurable exponent parameter for Inverse Distance Weighting (IDW) interpolation. Higher values control the smoothness and local influence of interpolated station observations.
   * - ``idw_rmax1``
     - ``-1.0``
     - CALMET-style maximum horizontal radius of influence [m] for surface layer meteorological station interpolation (value <= 0 ignores the limit).
   * - ``idw_rmax2``
     - ``-1.0``
     - CALMET-style maximum horizontal radius of influence [m] for upper-air layers meteorological station interpolation (value <= 0 ignores the limit).
   * - ``idw_r1``
     - ``-1.0``
     - Blending weighting parameter [m] for surface layer Step 1 vs. Step 2 weighting (value <= 0 disables blending).
   * - ``idw_r2``
     - ``-1.0``
     - Blending weighting parameter [m] for upper-air layers Step 1 vs. Step 2 weighting (value <= 0 disables blending).
   * - **Analytical Ekman Spiral Initialization Parameters**
     -
     -
   * - ``ekman_latitude``
     - ``45.0``
     - Latitude [degrees] for analytical Ekman spiral profile initialization (defaults to ``latitude``).
   * - ``ekman_ug``
     - ``10.0``
     - Geostrophic wind x-component [m/s] for analytical Ekman spiral profile initialization (defaults to ``U_ref``).
   * - ``ekman_vg``
     - ``0.0``
     - Geostrophic wind y-component [m/s] for analytical Ekman spiral profile initialization (defaults to ``V_ref``).
   * - ``ekman_Km``
     - ``5.0``
     - Vertical eddy viscosity coefficient [m^2/s] for analytical Ekman spiral profile initialization.
   * - **MLMG Solver Parameters**
     -
     -
   * - ``mlmg_verbose``
     - ``1``
     - MLMG solver verbosity (0 = silent, 4 = maximum).
   * - ``tol_rel``
     - ``1.0e-8``
     - MLMG relative convergence tolerance.
   * - ``mlmg_max_iter``
     - ``200``
     - Maximum MLMG iterations.
   * - ``mlmg_bottom_solver``
     - ``default``
     - Bottom solver method: ``default``, ``bicgstab``, ``cg``, or ``smoother``.
   * - ``max_grid_size``
     - ``32``
     - Maximum AMReX box size per spatial dimension (increase for GPUs).
   * - **Numerical Methods**
     -
     -
   * - ``deriv_method``
     - ``central``
     - Derivative scheme: ``central`` (2nd order), ``weno3`` (3rd order), or ``weno5`` (5th order).
   * - ``enable_obrien_w_adjustment``
     - ``false``
     - Enables the O'Brien (1970) vertical velocity adjustment procedure to redistribute vertical divergence residuals column-wise and force vertical velocity :math:`w = 0` precisely at the domain top.
   * - **Output Parameters**
     -
     -
   * - ``plot_file``
     - ``plt_wind``
     - Output plotfile prefix.
   * - ``extract_agl``
     - ``-1.0``
     - Sample heights AGL [m] for 2-D CSV extraction. Space-separated for multiple heights.
   * - ``extract_file``
     - ``wind_extract.csv``
     - Output filename for terrain-aligned CSV extraction.
   * - **Wind Turbine Wake Parameters**
     -
     -
   * - ``enable_turbine_wake``
     - ``false``
     - Enable analytical turbine wake modeling.
   * - ``turbine_file``
     - (none)
     - Path to turbines CSV layout file.
   * - ``turbine_wake_model_type``
     - ``jensen``
     - Turbine wake model: ``jensen`` (classic linear), ``bastankhah_gaussian`` (Gaussian deficit), or ``turbopark`` (self-similar Gaussian with local TI).
   * - ``turbine_wake_superposition``
     - ``quadratic``
     - Wake deficit superposition method: ``quadratic`` (RSS), ``linear``, or ``max`` (maximum deficit).
   * - ``jensen_kw``
     - ``0.075``
     - Jensen wake decay constant (typically 0.05 for offshore, 0.075 for onshore).
   * - ``gaussian_ka``
     - ``0.05``
     - Bastankhah wake expansion coefficient.
   * - ``turbopark_c1``
     - ``0.38``
     - TurbOPark expansion coefficient.
   * - ``ambient_ti``
     - ``0.075``
     - Ambient turbulence intensity.
   * - ``enable_jimenez_deflection``
     - ``false``
     - Enable Jimenez wake centerline deflection model.
   * - ``jimenez_kd``
     - ``0.05``
     - Jimenez deflection calibration constant.
   * - ``enable_bastankhah_deflection``
     - ``false``
     - Enable Bastankhah & Porté-Agel (2016) wake deflection model.
   * - ``wake_added_turbulence_model``
     - ``none``
     - Analytical wake-added turbulence model: ``none``, ``crespo_hernandez``, or ``frandsen`` (STF).
   * - ``surface_sensible_heat_flux``
     - ``0.0``
     - Surface sensible heat flux in W/m² used for buoyant wake destruction (only applied when > 0).
   * - ``buoyant_wake_destruction_coeff``
    - ``0.005``
    - Buoyant wake destruction proportionality constant in m²/W.

Input Variables Sectioned by Physics
------------------------------------

This section lists all possible configuration variables that can be provided in the input file, organized by their respective physical and computational categories.

Core Domain & Solver Settings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **dx**, **dy**, **dz** (Real, Default: ``30.0``): Computational grid spacing in the X, Y, and Z directions [m].
* **domain_height** (Real, Default: ``300.0``): Computational domain vertical height above maximum terrain [m].
* **max_grid_size** (Integer, Default: ``32``): Maximum AMReX box size per dimension.
* **mlmg_verbose** (Integer, Default: ``1``): Multigrid solver verbosity level (0 to 4).
* **tol_rel** (Real, Default: ``1.e-8``): Relative convergence tolerance for multigrid solver.
* **mlmg_max_iter** (Integer, Default: ``200``): Maximum number of multigrid solver iterations.
* **mlmg_max_fmg_iter** (Integer, Default: ``0``): Maximum Full Multigrid iterations.
* **mlmg_pre_smooth** (Integer, Default: ``2``): Pre-smoothing iterations in multigrid.
* **mlmg_post_smooth** (Integer, Default: ``2``): Post-smoothing iterations in multigrid.
* **mlmg_bottom_solver** (String, Default: ``default``): Bottom solver type (``default``, ``bicgstab``, ``cg``, ``smoother``).
* **deriv_method** (String, Default: ``central``): Spatial derivative discretization scheme (``central``, ``weno3``, ``weno5``).
* **plot_file** (String, Default: ``plt_wind``): Prefix of the output AMReX plotfile.
* **extract_agl** (List of Reals, Default: none): Sampling heights above ground level [m] for 2D CSV extraction.
* **extract_k** (List of Integers, Default: none): Grid vertical indices to extract data at.
* **extract_file** (String, Default: ``wind_extract.csv``): Filename for terrain-aligned CSV extraction output.

Terrain & Initialization
~~~~~~~~~~~~~~~~~~~~~~~~

* **terrain_file** (String, Default: ``terrain.csv``): Path to CSV containing terrain X Y Z points, or ``synthetic``.
* **init_mode** (String, Default: ``loglaw``): Wind initialization mode (``loglaw``, ``uniform``, ``raws``, ``surface_data``, ``powerlaw``, ``windfield``, ``sounding``).
* **sounding_files** (Array of Strings, Default: none): Paths to sounding data files (FSL or UP.DAT/custom formats).
* **sounding_file** (String, Default: none): Path to single sounding data file.
* **sounding_x**, **sounding_y** (Array of Reals, Default: none): Projected X and Y coordinates [m] of the sounding stations.
* **sounding_vertical_interp** (String, Default: ``spline``): Method for 1D vertical interpolation (``spline`` or ``log_linear``).
* **sounding_wind_in_knots** (Boolean, Default: ``true``): Flag to convert FSL wind speeds from knots to m/s.
* **U_ref**, **V_ref** (Real, Default: ``10.0`` / ``0.0``): Reference wind components [m/s] at reference height ``z_ref``.
* **z_ref** (Real, Default: ``10.0``): Reference height above local terrain [m].
* **z0** (Real, Default: ``0.1``): Default aerodynamic roughness length [m].
* **uniform_U**, **uniform_V** (Real, Default: none): Uniform wind components [m/s] used in ``uniform`` mode.
* **powerlaw_exponent** (Real, Default: ``0.143``): Exponent coefficient for power-law profiles.
* **landuse_file** (String, Default: none): Path to land-use classification CSV file.
* **velocity_file** (String, Default: none): Path to sparse station wind observations CSV file.
* **surface_data_file** (String, Default: none): Path to HRRR-style surface observations CSV file.
* **windfield_file** (String, Default: none): Path to pre-mapped 3D windfield CSV data.
* **z0_file** (String, Default: none): Path to spatially-varying aerodynamic roughness CSV file.
* **use_spatial_alpha_coefficients** (Boolean, Default: ``false``): Flag to enable spatially-varying alpha weighting.
* **alpha_coefficients_file** (String, Default: none): Path to spatially-varying alpha coefficients CSV file.
* **alpha_h**, **alpha_v** (Real, Default: ``1.0``): Global horizontal and vertical weighting scaling parameters.
* **use_height_dependent_alpha_v** (Boolean, Default: ``false``): Flag to enable linear variation of vertical anisotropy with height.
* **alpha_v_surface**, **alpha_v_top** (Real, Default: ``alpha_v``): Vertical anisotropy weighting at ground and top boundaries.
* **idw_gamma** (Real, Default: ``1.0``): Vertical distance penalty factor for 3D meteorological interpolation.
* **idw_exponent** (Real, Default: ``2.0``): Exponent parameter for Inverse Distance Weighting (IDW) interpolation.
* **synthetic_type** (String, Default: ``multi_gaussian_hill``): Model type for synthetic terrain (``gaussian_hill``, ``multi_gaussian_hill``).
* **synthetic_xmin**, **synthetic_xmax**, **synthetic_ymin**, **synthetic_ymax** (Real, Default: ``0.0`` / ``300.0``): Bounds of the generated synthetic terrain [m].
* **synthetic_nx**, **synthetic_ny** (Integer, Default: ``11``): Horizontal grid resolution for synthetic terrain generation.
* **synthetic_peak**, **synthetic_sigma**, **synthetic_center_x**, **synthetic_center_y** (Real, Default: none): Structural parameters for single Gaussian hill synthetic terrain.
* **synthetic_peaks**, **synthetic_sigmas**, **synthetic_centers_x**, **synthetic_centers_y** (List of Reals, Default: none): Structural parameters lists for multi-Gaussian hill synthetic terrain.
* **enable_terrain_analysis** (Boolean, Default: ``false``): Enable automatic slope and roughness terrain classification.
* **slope_threshold_moderate**, **slope_threshold_steep** (Real, Default: ``15.0`` / ``30.0``): Terrain analysis slope angle classification thresholds [degrees].
* **roughness_factor_moderate**, **roughness_factor_steep** (Real, Default: ``1.5`` / ``2.5``): Aerodynamic roughness scale multipliers based on slope.
* **transition_zone_width** (Real, Default: ``100.0``): Spatial transition zone width [m].
* **enable_transition_smoothing** (Boolean, Default: ``false``): Enable vertical smoothing across transition boundaries.
* **transition_height_scale** (Real, Default: ``50.0``): Vertical transition height decay scale [m].
* **transition_layer_height** (Real, Default: ``100.0``): Base height of transition layer [m].

Atmospheric Physics & Boundary Layer Dynamics
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **enable_stability_correction** (Boolean, Default: ``false``): Enable Monin-Obukhov atmospheric stability profile corrections.
* **stability_length** (Real, Default: ``1.e10``): Obukhov length [m] (positive for stable, negative for unstable).
* **use_holtslag_stability** (Boolean, Default: ``false``): Enable Holtslag stability functions for stable conditions.
* **enable_pg_stability** (Boolean, Default: ``false``): Enable Pasquill-Gifford atmospheric stability classification scheme.
* **solar_radiation** (Real, Default: ``0.0``): Incoming solar radiation [W/m²] for PG stability estimation.
* **is_nighttime** (Boolean, Default: ``false``): Nighttime flag for PG stability.
* **cloud_cover** (Real, Default: ``0.0``): Cloud cover fraction (0.0 to 1.0) for PG stability.
* **enable_capping_lid** (Boolean, Default: ``false``): Enable capping lid boundary layer top constraint.
* **capping_lid_height** (Real, Default: ``500.0``): Boundary layer height for capping lid [m].
* **enable_elevation_scaling** (Boolean, Default: ``false``): Enable terrain-elevation-dependent wind speed scaling.
* **elevation_scaling_factor** (Real, Default: ``0.1``): Intensity factor of the elevation scaling effect.
* **elevation_height_scale** (Real, Default: ``100.0``): Height decay scale of the elevation scaling effect [m].
* **enable_ekman_veer** (Boolean, Default: ``false``): Enable Coriolis Ekman spiral wind direction veer.
* **latitude** (Real, Default: ``45.0``): Geographical latitude [degrees] for Coriolis force computation.
* **ekman_veer_total** (Real, Default: ``30.0``): Total direction veer angle across the boundary layer [degrees].
* **ekman_veer_height** (Real, Default: ``500.0``): Vertical extent height of the Ekman veer [m].
* **enable_wind_direction_gradient** (Boolean, Default: ``false``): Enable linear vertical shear of the wind direction.
* **wind_direction_shear_rate** (Real, Default: ``0.05``): Direction shear rate [deg/m].
* **enable_fetch_roughness_transition** (Boolean, Default: ``false``): Enable downwind internal boundary layer development at roughness transitions.
* **fetch_transition_blending_height** (Real, Default: ``50.0``): Height scale of internal boundary layer blending [m].
* **enable_diurnal_roughness** (Boolean, Default: ``false``): Enable time-dependent sinusoidal modulation of roughness length z0(t).
* **roughness_amplitude** (Real, Default: ``0.3``): Diurnal oscillation amplitude.
* **roughness_phase_offset** (Real, Default: ``0.0``): Diurnal oscillation phase offset [hours].
* **enable_bl_decay** (Boolean, Default: ``false``): Enable exponential wind speed decay above the boundary layer top.
* **bl_depth_param** (Real, Default: ``200.0``): Boundary layer depth parameter [m].
* **decay_height_scale** (Real, Default: ``100.0``): Vertical decay height scale above the boundary layer [m].
* **bl_transition_height** (Real, Default: ``50.0``): Boundary layer transition height zone [m].
* **enable_bl_depth_diagnostic** (Boolean, Default: ``false``): Enable Richardson-number-based boundary layer depth diagnostic.
* **enable_marine_bl** (Boolean, Default: ``false``): Enable CALMET-style diagnostic overwater boundary layer mixing height model over water cells (landuse category 11).
* **marine_sst** (Real, Default: ``288.15``): Sea-surface temperature (SST) [K].
* **marine_air_sea_dt** (Real, Default: ``0.0``): Air-sea temperature difference (T_air - T_sea) [K]. Negative difference indicates convective unstable boundary layer.
* **richardson_critical** (Real, Default: ``0.25``): Critical Richardson number threshold for boundary layer top.
* **richardson_min_wind_shear** (Real, Default: ``0.001``): Minimum wind shear threshold for Richardson calculation.
* **enable_froude_height_scaling** (Boolean, Default: ``false``): Scale terrain blocking intensity using local Froude height scaling.
* **enable_ageostrophic_balance** (Boolean, Default: ``false``): Enable ageostrophic wind balance boundary conditions.
* **ageostrophic_latitude** (Real, Default: ``45.0``): Latitude for ageostrophic force balance [degrees].
* **ageostrophic_pressure_grad_x**, **ageostrophic_pressure_grad_y** (Real, Default: ``0.0``): Prescribed horizontal pressure gradients [Pa/m].
* **ageostrophic_air_density** (Real, Default: ``1.225``): Reference air density [kg/m³].
* **ageostrophic_fraction** (Real, Default: ``1.0``): Inclusion fraction of ageostrophic components.
* **enable_flux_diagnostics** (Boolean, Default: ``false``): Enable surface layer energy and momentum flux computations.
* **flux_compute_sensible_heat**, **flux_compute_latent_heat** (Boolean, Default: ``false``): Flags to calculate sensible and latent heat flux components.
* **flux_theta_star** (Real, Default: ``0.1``): Prescribed scale temperature parameter for surface energy flux.
* **flux_q_star** (Real, Default: ``0.001``): Prescribed scale moisture parameter for latent heat flux.
* **surface_temperature** (Real, Default: ``288.15``): Reference temperature [K] for flux calculations.
* **heat_flux_scale** (Real, Default: ``1.0``): Scaling factor for sensible heat flux.
* **relative_humidity** (Real, Default: ``0.5``): Relative humidity for latent heat flux calculation.
* **charnock_alpha** (Real, Default: ``0.011``): Charnock parameter used for wind-speed-dependent overwater roughness.
* **precipitation_file** (String, Default: none): Path to precipitation rate spatial CSV file.
* **precipitation_stability_threshold** (Real, Default: ``1.0``): Rainfall rate threshold [mm/hr] for eroding stable layers.
* **enable_directional_bias** (Boolean, Default: ``false``): Enable systematic direction and speed bias correction.
* **bias_direction_offset** (Real, Default: ``0.0``): Wind direction bias correction offset [degrees].
* **bias_speed_factor** (Real, Default: ``1.0``): Wind speed bias scale factor.
* **bias_periodic_enabled** (Boolean, Default: ``false``): Enable time-varying periodic bias offset.
* **bias_periodic_amplitude** (Real, Default: ``5.0``): Amplitude of periodic bias oscillations [degrees].
* **enable_simplified_richardson** (Boolean, Default: ``false``): Enable bulk Richardson method for conditional stability selection.
* **use_golder_curves** (Boolean, Default: ``true``): Map stability class and roughness length to Obukhov length using standard empirical Golder (1972) curves when using bulk Richardson method.
* **enable_mosaic_roughness** (Boolean, Default: ``false``): Compute effective roughness length via area-weighted logarithmic averaging for fractional land use cover databases.
* **enable_coriolis_latitude** (Boolean, Default: ``false``): Enable geographical latitude-based scaling of Coriolis parameter.
* **domain_latitude** (Real, Default: ``45.0``): Latitude of computational domain [degrees].
* **enable_power_law_profile** (Boolean, Default: ``false``): Enable power-law profile wind initialization.
* **enable_heat_flux_diagnostics** (Boolean, Default: ``false``): Enable diagnostic calculation of surface sensible and latent heat fluxes.
* **heat_flux_theta_star** (Real, Default: ``0.0``): Temperature scaling parameter for heat flux diagnostic.
* **enable_divergence_damping** (Boolean, Default: ``false``): Enable spatial divergence damping filtering.
* **damping_coefficient** (Real, Default: ``0.1``): Divergence damping filter coefficient.
* **damping_coefficient_h** (Real, Default: uses damping_coefficient): Horizontal divergence damping filter coefficient.
* **damping_coefficient_v** (Real, Default: uses damping_coefficient): Vertical divergence damping filter coefficient.
* **damping_iterations** (Integer, Default: ``5``): Number of divergence damping smoothing iterations.
* **enable_perturbation_pressure** (Boolean, Default: ``false``): Enable full perturbation pressure equation solve.
* **pressure_tol_rel** (Real, Default: ``1.e-6``): Relative convergence tolerance for perturbation pressure.
* **pressure_max_iter** (Integer, Default: ``100``): Maximum iterations for pressure multigrid solver.
* **pressure_scale** (Real, Default: ``1.0``): Hydrostatic scaling parameter for pressure.
* **enable_cell_local_anisotropy** (Boolean, Default: ``false``): Enable spatially-varying cell-local anisotropy coefficients.
* **anisotropy_source** (String, Default: ``slope``): Driving mechanism for local anisotropy variation (``slope``, ``stability``).
* **anisotropy_slope_scale** (Real, Default: ``1.0``): Sensitivity coefficient of slope-driven anisotropy.
* **anisotropy_decay_height** (Real, Default: ``100.0``): Height decay scale of local anisotropy variations [m].
* **anisotropy_ri_gamma**, **anisotropy_ri_beta** (Real, Default: ``2.0`` / ``0.5``): Scaling exponents for stability-driven anisotropy.
* **anisotropy_fr_min** (Real, Default: ``0.1``): Minimum Froude number limit for stability anisotropy.

Complex Topographic Physics
~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **enable_topographic_shielding** (Boolean, Default: ``false``): Enable topographic barrier sheltering IDW interpolation penalty.
* **enable_thermal_circulation** (Boolean, Default: ``false``): Enable coastline-thermal sea-breeze circulation parameterization.
* **thermal_temperature_contrast** (Real, Default: ``10.0``): Land-sea temperature contrast [K].
* **thermal_reference_temperature** (Real, Default: ``293.0``): Reference air temperature [K].
* **thermal_coefficient** (Real, Default: ``0.5``): Coastline sea-breeze flow intensity scaling parameter.
* **thermal_vertical_decay_height** (Real, Default: ``200.0``): Vertical extent limit height of thermal flow [m].
* **thermal_distance_scale** (Real, Default: ``2000.0``): Horizontal width of thermal sea-breeze transition zone [m].
* **thermal_coastline_x**, **thermal_coastline_y** (Real, Default: ``0.0``): Center point coordinates representing the coastline.
* **thermal_coast_normal_x**, **thermal_coast_normal_y** (Real, Default: ``1.0`` / ``0.0``): Unit direction vector normal to the coastline pointing from water to land.
* **land_sea_mask_file** (String, Default: none): Path to binary land-sea mask CSV representing sea=0, land=1.
* **enable_terrain_blocking** (Boolean, Default: ``false``): Enable stable flow terrain blocking parameterization.
* **terrain_blocking_brunt_vaisala_frequency** (Real, Default: ``0.01``): Brunt-Väisälä frequency [rad/s] for stable layering.
* **terrain_blocking_reduction_factor** (Real, Default: ``0.8``): Reference wind reduction scale inside blocked region.
* **terrain_blocking_transition_froude** (Real, Default: ``1.0``): Critical Froude number below which terrain blocking triggers.
* **terrain_blocking_flank_enhancement** (Real, Default: ``0.2``): Speedup enhancement factor of deflected flow bypassing terrain flanks.
* **terrain_blocking_lapse_rate** (Real, Default: ``-0.0065``): Temperature vertical lapse rate [K/m].
* **terrain_blocking_reference_temperature** (Real, Default: ``288.15``): Ground surface reference temperature [K].
* **enable_slope_flows** (Boolean, Default: ``false``): Enable mountain katabatic (night downslope) or anabatic (day upslope) flow parameterization.
* **slope_flow_temperature_diff** (Real, Default: ``-5.0``): Temperature deficit between surface and ambient air [K] (negative for katabatic, positive for anabatic).
* **slope_flow_reference_temperature** (Real, Default: ``288.0``): Reference background temperature [K].
* **slope_flow_empirical_coefficient** (Real, Default: ``0.1``): Slope flow speed velocity scaling parameter.
* **slope_flow_vertical_decay_height** (Real, Default: ``50.0``): Vertical extent decay scale of slope flow jet [m].
* **slope_flow_min_slope** (Real, Default: ``5.0``): Minimum slope angle [degrees] required to activate slope flow jet.
* **enable_valley_channeling** (Boolean, Default: ``false``): Enable subgrid mountain valley wind channeling parameterization.
* **valley_axis_angle_deg** (Real, Default: ``0.0``): Angle of the valley centerline axis relative to North [degrees].
* **valley_width** (Real, Default: ``500.0``): Valley crosswind width scale [m].
* **valley_depth** (Real, Default: ``200.0``): Valley depth scale [m].
* **valley_channeling_strength_max** (Real, Default: ``0.5``): Maximum wind direction steering strength coefficient.
* **valley_speedup_factor_narrow** (Real, Default: ``0.3``): Speedup acceleration multiplier inside valley narrows.
* **valley_slowdown_factor_wide** (Real, Default: ``0.2``): Flow deceleration scale inside valley wide basins.
* **enable_gap_flow** (Boolean, Default: ``false``): Enable wind speedup channeling inside mountain passes and gaps.
* **gap_flow_orientation** (Real, Default: ``0.0``): Angle of gap axis relative to North [degrees].
* **gap_flow_width**, **gap_flow_depth** (Real, Default: ``100.0`` / ``50.0``): Structural width and depth parameters of the gap [m].
* **gap_flow_pressure_coefficient** (Real, Default: ``0.2``): Pressure gradient acceleration scaling factor inside gap.
* **gap_flow_speedup_max** (Real, Default: ``1.5``): Maximum speedup multiplier.
* **gap_flow_center_x**, **gap_flow_center_y** (Real, Default: ``0.0``): Spatial coordinate center of the gap [m].
* **gap_flow_transition_width** (Real, Default: ``200.0``): Axial length scale of gap entrance/exit transition zone [m].
* **gap_flow_vertical_extent** (Real, Default: ``100.0``): Vertical extent height limit of gap flow speedup [m].

Canopy, Vegetation & Shelter Models
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **enable_canopy** (Boolean, Default: ``false``): Enable forest canopy momentum sink drag parameterization.
* **canopy_file** (String, Default: none): Path to spatially varying canopy parameters CSV file.
* **canopy_height** (Real, Default: ``15.0``): Uniform forest canopy top height [m].
* **frontal_area_index** (Real, Default: ``0.1``): Frontal leaf area index used for drag density calculation.
* **plan_area_index** (Real, Default: ``0.1``): Plan area index used for zero-plane displacement height.
* **canopy_drag_coeff** (Real, Default: ``0.2``): Canopy drag coefficient (typical range 0.1 to 0.3).
* **canopy_attenuation** (Real, Default: ``2.0``): Exponential wind speed decay scale parameter inside canopy.
* **use_exponential_profile** (Boolean, Default: ``false``): Enable Shaw-Pereira exponential velocity decay profiling inside vegetation.
* **canopy_profile_type** (String, Default: ``forest``): Type of canopy structure (``forest``, ``crop``, ``grass``).
* **enable_windbreaks** (Boolean, Default: ``false``): Enable subgrid shelterbelt and windbreak line-segment drag.
* **windbreaks_file** (String, Default: none): Path to windbreaks definition CSV file.
* **enable_vegetation_roughness** (Boolean, Default: ``false``): Enable vegetation leaf density attenuation of surface roughness.
* **vegetation_state** (Real, Default: ``1.0``): Vegetation seasonal state index (e.g. green leaf-area index fraction).
* **vegetation_state_type** (Integer, Default: ``0``): Mapping code for vegetation seasonal types.

Obstacles & Buildings
~~~~~~~~~~~~~~~~~~~~~

* **building_file** (String, Default: none): Path to buildings CSV specification file.
* **enable_wake** (Boolean, Default: ``false``): Enable building wake parameterization (e.g. Röckle building model).
* **wake_model_type** (String, Default: ``rockle``): Building wake parameterization (``rockle``, ``huber_snyder``, ``aermod_prime``).
* **wake_c1**, **wake_c2** (Real, Default: ``0.9`` / ``0.3``): Cavity recirculation and downwind wake length parameters.
* **wake_separation_length** (Real, Default: ``3.0``): Multiplier for downwind wake separation length scale.
* **wake_superposition** (String, Default: ``rss``): Multi-building wake deficit superposition method (``rss``, ``linear``, ``max``).
* **enable_street_canyon** (Boolean, Default: ``false``): Enable street canyon wind speed reduction for parallel building arrays.
* **street_canyon_reduction** (Real, Default: ``0.5``): Speed reduction factor inside canyon cavities.
* **enable_eb** (Boolean, Default: ``false``): Enable AMReX Embedded Boundary (EB) capability as an alternative geometry representation for arbitrary 3D shapes (such as boxes, cylinders, spheres, and STL geometries), marking solid cells via fluid volume fraction.
* **eb_threshold** (Real, Default: ``0.5``): Fluid volume fraction threshold (cells with fluid volume fraction below this threshold are marked as solid obstacles).
* **enable_building_porosity** (Boolean, Default: ``false``): Enable porosity drag model for porous building structures.
* **building_porosity_file** (String, Default: none): Path to building-by-building porosity properties CSV file.
* **default_building_porosity** (Real, Default: ``0.0``): Default building porosity fraction (0.0 represents solid, 1.0 is open air).
* **porosity_drag_coefficient** (Real, Default: ``2.0``): Drag scaling coefficient of the porous structure.
* **enable_wall_functions** (Boolean, Default: ``false``): Enable wall function boundary conditions.
* **enable_terrain_wall_function** (Boolean, Default: ``false``): Enable wall functions on terrain surface.
* **enable_flat_surface_wall_function** (Boolean, Default: ``false``): Enable wall functions on flat bottom boundary.
* **enable_building_wall_function** (Boolean, Default: ``false``): Enable wall functions on building solid walls.
* **wall_function_z0_building**, **wall_function_z0_flat** (Real, Default: ``0.1`` / ``0.05``): Roughness lengths used inside wall functions [m].
* **wall_function_blend_height** (Real, Default: ``10.0``): Vertical blending height of wall functions [m].
* **wall_function_max_distance** (Real, Default: ``50.0``): Maximum spatial distance of wall function influence [m].
* **wall_function_flat_surface_elevation** (Real, Default: ``0.0``): Elevation coordinate of flat bottom boundary [m].
* **wall_function_enable_flat_surface** (Boolean, Default: ``false``): Enable presence of flat bottom surface.
* **wall_function_min_wall_distance** (Real, Default: ``0.1``): Minimum wall distance cutoff [m] to prevent singularity.
* **wall_function_enable_stability** (Boolean, Default: ``false``): Enable non-neutral Monin-Obukhov corrections inside wall functions.
* **wall_function_stability_length** (Real, Default: ``1.e10``): Obukhov length inside wall functions [m].
* **wall_function_enable_adaptive** (Boolean, Default: ``false``): Enable adaptive wall function activation based on local grid resolution.
* **wall_function_adaptive_threshold** (Real, Default: ``0.05``): Relative resolution threshold for wall functions.
* **wall_function_adaptive_min_cells** (Integer, Default: ``3``): Minimum grid cells inside boundary layer.

Infrastructure Vulnerability Assessment (Power Lines & Bridges)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **enable_wire_loading** (Boolean, Default: ``false``): Enable assessment of wind loading on overhead electrical conductors and power transmission lines.
* **wire_file** (String, Default: ``wires.csv``): Path to CSV file containing electrical wire span definitions and properties.
* **wire_output_file** (String, Default: ``wire_output.csv``): Output CSV filename where wire loading results (drag forces, temperatures, ampacity ratings) are written.
* **enable_bridge_loading** (Boolean, Default: ``false``): Enable assessment of wind loading on bridge structures (future extension).
* **bridge_file** (String, Default: ``bridges.csv``): Path to CSV file containing bridge span definitions (future extension).
* **bridge_output_file** (String, Default: ``bridge_output.csv``): Output CSV filename for bridge loading results (future extension).

Turbine Wakes & Deflection / Steering
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **enable_turbine_wake** (Boolean, Default: ``false``): Enable analytical turbine wake modeling.
* **turbine_file** (String, Default: none): Path to turbines CSV layout file.
* **turbine_wake_model_type** (String, Default: ``jensen``): Wake model type (``jensen``, ``bastankhah_gaussian``, ``turbopark``, ``gch``).
* **turbine_wake_superposition** (String, Default: ``quadratic``): Deficit superposition method (``quadratic``, ``linear``, ``max``).
* **jensen_kw** (Real, Default: ``0.075``): Jensen wake expansion decay constant.
* **gaussian_ka** (Real, Default: ``0.05``): Bastankhah Gaussian wake expansion decay constant.
* **turbopark_c1** (Real, Default: ``0.38``): TurbOPark expansion scaling parameter.
* **ambient_ti** (Real, Default: ``0.075``): Ambient turbulence intensity parameter.
* **enable_jimenez_deflection** (Boolean, Default: ``false``): Enable Jimenez wake centerline deflection model.
* **jimenez_kd** (Real, Default: ``0.05``): Jimenez yaw deflection decay parameter.
* **enable_bastankhah_deflection** (Boolean, Default: ``false``): Enable Bastankhah & Porté-Agel (2016) wake deflection model.
* **wake_added_turbulence_model** (String, Default: ``none``): Analytical wake-added TI model (``none``, ``crespo_hernandez``, ``frandsen``).
* **enable_wake_ground_interaction** (Boolean, Default: ``false``): Enable ground reflection boundary condition for wakes.
* **wake_ground_damping_scale** (Real, Default: ``1.0``): Ground reflection damping factor.
* **surface_sensible_heat_flux** (Real, Default: ``0.0``): Sensible heat flux [W/m²] for buoyant wake destruction.
* **buoyant_wake_destruction_coeff** (Real, Default: ``0.005``): Buoyant wake destruction scale parameter [m²/W].

Puff Dispersion & LPDM Models
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **enable_puff** (Boolean, Default: ``false``): Enable Lagrangian Puff Dispersion Model solver.
* **enable_lpdm** (Boolean, Default: ``false``): Enable Lagrangian Particle Dispersion Model solver.
* **particles_per_step** (Integer, Default: ``100``): Number of particles emitted per step in LPDM.
* **lpdm_random_seed** (Integer, Default: ``12345``): Random seed integer for particle random walk generator.
* **source_x**, **source_y**, **source_z** (Real, Default: ``0.0``): Computational coordinate coordinates of release source [m].
* **emission_rate** (Real, Default: ``1.0``): Pollutant mass emission rate [kg/s].
* **emission_duration** (Real, Default: ``10.0``): Mass emission duration [seconds].
* **enable_indoor_infiltration** (Boolean, Default: ``false``): Enable indoor building infiltration concentration calculation.
* **ach** (Real, Default: ``0.5``): Air changes per hour for indoor building infiltration calculation.
* **emissions_file** (String, Default: none): Path to CSV file containing time-varying emission rates.
* **threshold_red**, **threshold_orange**, **threshold_yellow**, **threshold_lfl** (Real, Default: none): Threat zone concentration thresholds for ALOHA threat analysis.
* **threat_zones_output** (String, Default: none): Filename to write threat zone boundaries to.
* **K_h**, **K_v** (Real, Default: ``1.0`` / ``0.1``): Horizontal and vertical turbulent diffusivities [m²/s].
* **sigma_y0**, **sigma_z0** (Real, Default: ``1.0``): Initial puff sizing parameters [m].
* **enable_height_dependent_K** (Boolean, Default: ``false``): Enable vertical variation of turbulent diffusivity.
* **K_profile** (String, Default: ``uniform``): Diffusivity profile model (``uniform``, ``powerlaw``).
* **K_power_law_exponent**, **K_reference_height** (Real, Default: ``0.15`` / ``10.0``): Height-dependent diffusivity power-law parameter.
* **enable_decay** (Boolean, Default: ``false``): Enable first-order pollutant concentration decay.
* **decay_constant** (Real, Default: ``1.e-4``): Chemical decay rate coefficient [s⁻¹].
* **enable_plume_rise** (Boolean, Default: ``false``): Enable buoyant/momentum plume rise.
* **heat_flux** (Real, Default: ``0.0``): Heat emission rate of source [W] for plume rise.
* **dt_puff** (Real, Default: ``1.0``): Time step size for dispersion integration [seconds].
* **n_steps_puff** (Integer, Default: ``100``): Number of integration steps to execute.
* **output_freq_puff** (Integer, Default: ``10``): Plot file output frequency in steps.
* **enable_adaptive_time_stepping** (Boolean, Default: ``false``): Enable CFL-limited adaptive time stepping.
* **cfl_limit** (Real, Default: ``0.5``): Maximum allowable CFL number for dispersion stability.
* **U_wind**, **V_wind**, **W_wind** (Real, Default: ``0.0``): Prescribed constant transport wind components [m/s] in standalone puff solver.
* **enable_terrain_reflection** (Boolean, Default: ``true``): Enable perfect reflection at terrain surface.
* **use_image_source** (Boolean, Default: ``false``): Use image source reflection method on flat ground.
* **capping_lid_file** (String, Default: none): Path to CSV file containing spatially-varying capping lid heights.
* **enable_building_masking** (Boolean, Default: ``false``): Zero out concentration inside solid building volumes.
* **enable_wake_diffusivity** (Boolean, Default: ``false``): Enable enhanced diffusivity inside building wakes and cavity zones.
* **wake_enhancement_cavity**, **wake_enhancement_far** (Real, Default: ``5.0`` / ``2.0``): Diffusivity enhancement factors inside building wakes.
* **enable_cavity_trapping** (Boolean, Default: ``false``): Enable cavity trapping and slow release of pollutants downwind of buildings.
* **enable_plume_deformation** (Boolean, Default: ``false``): Enable wind-shear-driven deformation of puff ellipsoid.
* **aermod_prime_cavity_factor** (Real, Default: ``0.5``): AERMOD PRIME cavity trapping calibration coefficient.
* **cavity_recirculation_strength** (Real, Default: ``0.5``): Recirculation wind speed scaling factor.
* **enable_turbine_wake_diffusivity** (Boolean, Default: ``false``): Enable enhanced diffusivity inside wind turbine wakes.
* **turbine_wake_diffusivity_factor** (Real, Default: ``3.0``): Diffusivity multiplier scale inside turbine wakes.
* **enable_canopy_effects** (Boolean, Default: ``false``): Enable canopy wind sheltering and enhanced diffusivity inside canopy.
* **canopy_enhancement_factor**, **canopy_sheltering_factor** (Real, Default: ``2.0`` / ``0.5``): Canopy diffusivity enhancement and sheltering coefficients.
* **enable_canopy_deposition** (Boolean, Default: ``false``): Enable leaf surface pollutant deposition.
* **enable_settling** (Boolean, Default: ``false``): Enable gravitational settling of heavy aerosol particles.
* **particle_density**, **particle_diameter** (Real, Default: ``1000.0`` / ``1.e-5``): Particle density [kg/m³] and physical diameter [m].
* **air_viscosity**, **gravity** (Real, Default: ``1.8e-5`` / ``9.81``): Physical dynamic viscosity of air [Pa·s] and gravity [m/s²].
* **enable_puff_deposition** (Boolean, Default: ``false``): Enable dry deposition loss on ground surface.
* **enable_wet_deposition** (Boolean, Default: ``false``): Enable precipitation scavenging washout of puff.
* **scavenging_coeff_base** (Real, Default: ``1.e-4``): Base scavenging coefficient [s⁻¹].
* **precipitation_rate** (Real, Default: ``0.0``): Rain precipitation intensity rate [mm/hr].
* **scavenging_exponent** (Real, Default: ``0.8``): Washout precipitation intensity exponent scale.
* **enable_dynamic_decay** (Boolean, Default: ``false``): Enable weather-dependent chemical decay.
* **temp_ref**, **temp_coeff**, **rh_ref**, **rh_coeff**, **solar_ref**, **solar_coeff** (Real, Default: none): Meteorological decay coefficients.
* **ambient_temp**, **ambient_rh**, **ambient_solar** (Real, Default: none): Prescribed ambient temperature [K], relative humidity, and solar radiation.
* **coupled_mode** (Boolean, Default: ``false``): Enable reading wind fields from 3D NetCDF or solver plotfiles.
* **unsteady_wind** (Boolean, Default: ``false``): Enable time-dependent reading of consecutive wind plotfiles.
* **wind_plotfile_prefix** (String, Default: none): Path prefix to solver velocity plotfiles.
* **dispersion_scheme** (String, Default: ``constant``): Growth/dispersion scheme (``constant``, ``pasquill_gifford``, ``mcelroy_pooler``, ``turbulence``).
* **is_urban** (Boolean, Default: ``false``): Use urban McElroy-Pooler formulas for analytical dispersion.
* **pg_stability_class** (Integer, Default: ``3``): Pasquill-Gifford stability class (0=A, 1=B, 2=C, 3=D, 4=E, 5=F).
* **enable_pg_stability** (Boolean, Default: ``false``): Automatically estimate Pasquill-Gifford stability from reference wind, solar radiation, cloud cover, and nighttime conditions.
* **roughness** (Real, Default: ``0.1``): Ground surface roughness length [m] for Wesely dry deposition.
* **u_star** (Real, Default: ``0.4``): Friction velocity [m/s] for Wesely dry deposition.
* **L_obukhov** (Real, Default: ``1.0e10``): Monin-Obukhov stability length [m] for Wesely dry deposition.
* **base_surface_resistance** (Real, Default: ``100.0``): Wesely dry deposition base surface resistance [s/m].
* **molecular_diffusivity** (Real, Default: ``1.5e-5``): Pollutant molecular diffusivity in air [m²/s].
* **is_snow** (Boolean, Default: ``false``): Enable snow scavenging coefficients for wet deposition.
* **source_type** (String, Default: ``point``): Source geometry category (``point``, ``line``, ``area``, ``volume``).
* **receptor_file** (String, Default: none): Path to input file containing discrete x,y,z receptor coordinates.
* **receptor_output** (String, Default: ``receptor_concentration.csv``): Filename prefix for discrete receptors concentration and visibility output.
* **enable_visibility** (Boolean, Default: ``false``): Enable calculation of b_ext, visual range, and deciview under IMPROVE algorithm.
* **enable_chemistry** (Boolean, Default: ``false``): Enable MESOPUFF II chemical transformation of SO2/NOx to Sulfate/Nitrate aerosols.

Synthetic Turbulence & Spectral Models
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **enable_synthetic_turbulence** (Boolean, Default: ``false``): Enable terrain-aware synthetic turbulence pipeline.
* **turbulence_spectrum_model** (String, Default: ``VonKarman``): Turbulent spectrum (``VonKarman``, ``Kaimal``, ``Mann``, ``Davenport``, ``Harris``).
* **turbulence_intensity_model** (String, Default: ``PowerLaw``): Turbulence intensity profile (``PowerLaw``, ``IEC``, ``Uniform``).
* **turbulence_coherence_model** (String, Default: ``Gaussian``): Spatial coherence model (``Gaussian``, ``PowerLaw``, ``QuadraticExponential``, ``none``).
* **turbulence_intensity_ref** (Real, Default: ``0.12``): Reference turbulence intensity at hub height.
* **turbulence_z_intensity_ref** (Real, Default: ``90.0``): Reference height for turbulence intensity profile [m].
* **turbulence_intensity_exponent** (Real, Default: ``0.143``): Vertical exponent factor for PowerLaw intensity profile.
* **turbulence_hub_height** (Real, Default: ``90.0``): Hub height for IEC spectrum parameters [m].
* **turbulence_iec_category** (String, Default: ``B``): IEC 61400-1 turbulence category (``A``, ``B``, ``C``).
* **turbulence_coherence_powerlaw_exponent** (Real, Default: ``0.5``): Coherence decay exponent for powerlaw model.
* **turbulence_length_scale_u**, **turbulence_length_scale_v**, **turbulence_length_scale_w** (Real, Default: ``340.2`` / ``113.4`` / ``27.7``): Integral turbulence scales [m].
* **turbulence_coherence_decay_vertical**, **turbulence_coherence_decay_lateral** (Real, Default: ``12.0`` / ``12.0``): Spatial decay factors of coherence.
* **turbulence_anisotropy_ratio_v**, **turbulence_anisotropy_ratio_w** (Real, Default: ``0.8`` / ``0.5``): Variance ratios relative to u-component.
* **mann_length_scale_u**, **mann_length_scale_v**, **mann_length_scale_w** (Real, Default: none): Integral scales for Mann model.
* **mann_variance_u**, **mann_variance_v**, **mann_variance_w** (Real, Default: none): Component variances for Mann model.
* **mann_asymmetry_parameter** (Real, Default: ``3.9``): Shear distortion anisotropy parameter for Mann model.
* **mann_eddy_lifetime** (Real, Default: ``0.1``): Eddy lifetime parameter for Mann model.
* **mann_terrain_adaptation_factor** (Real, Default: ``1.0``): Terrain adaptation scaling factor for Mann model.
* **turbulence_random_seed** (Integer, Default: ``12345``): Random seed integer for spectral phase synthesis.
* **turbulence_enable_stability_correction** (Boolean, Default: ``false``): Enable Monin-Obukhov atmospheric stability spectrum corrections.
* **turbulence_monin_obukhov_length** (Real, Default: ``1.e10``): Obukhov length for turbulence spectrum [m].
* **turbulence_stability_parameterization** (String, Default: ``MoninObukhov``): Stability correction scheme (``MoninObukhov``, ``Richardson``).
* **turbulence_export_format** (String, Default: ``bts``): Export format for synthetic turbulence output (``bts``, ``Bladed``, ``vtk``, ``none``).
* **turbulence_output_file** (String, Default: ``turbulence.bts``): Output binary filename for synthetic turbulence export.
* **enable_terrain_aware_masking** (Boolean, Default: ``false``): Enable terrain-aware masking below ground level.
* **terrain_mask_transition_height** (Real, Default: ``50.0``): Blending transition layer height above terrain [m].

File Formats
------------

Terrain File Format
~~~~~~~~~~~~~~~~~~~
The terrain CSV file must contain one data point per line with columns **X  Y  Z** (in meters, UTM or local coordinates). Lines beginning with ``#`` are comments::

   # X [m]  Y [m]  Z [m]
   0.0      0.0    5.2
   30.0     0.0    8.1
   ...

Building File Format
~~~~~~~~~~~~~~~~~~~~
Buildings are specified in a CSV file with one building box per line. The optional 7th column specifies building rotation in degrees counter-clockwise from the x-axis::

   # xmin  xmax  ymin  ymax  zmin  zmax  [rotation]
   40.0    60.0  40.0  60.0  0.0   30.0
   100.0   140.0 60.0  80.0  0.0   50.0  45.0

Wind Turbine File Format
~~~~~~~~~~~~~~~~~~~~~~~~
Turbines are defined in a CSV layout file. Columns represent coordinate locations, dimensions, and operational properties in the following order:

1. **x** (required): Easting or local x-coordinate [m].
2. **y** (required): Northing or local y-coordinate [m].
3. **hub_height** (required): Turbine hub height above ground level [m].
4. **rotor_diameter** (required): Rotor diameter [m].
5. **default_ct** (required): Default thrust coefficient.
6. **yaw** (optional): Wake deflection angle relative to incoming wind [degrees].
7. **orientation** (optional): Turbine rotor alignment angle relative to grid x-axis [degrees].
8. **tilt** (optional): Rotor tilt angle [degrees], used to calculate vertical wake deflection.
9. **power_curve_file** (optional): Filename of the CSV or JSON power curve.

**Yaw** defines the active aerodynamic misalignment of the rotor disk relative to the incoming wind direction. This is used to calculate lateral wake deflection and secondary steering. In contrast, **orientation** specifies the fixed absolute physical heading of the rotor face relative to the grid coordinate system. **Tilt** defines the backward/upward rotation of the rotor disk around the crosswind axis, used to model vertical wake deflection.

Example::

   # x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, tilt, power_curve_file
   100.0, 200.0, 90.0, 120.0, 0.8, 15.0, 45.0, 10.0, test_turbine.json

Power Curve Format (CSV/JSON)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Discrete electrical power outputs and thrust coefficients are mapped in an optional power curve file, which can be either a flat CSV or a standard JSON specification file (facilitating interoperability with FLORIS and PyWake):

* **CSV Format**::

     # wind_speed, power_kw, ct
     3.0, 0.0, 0.8
     5.0, 1000.0, 0.78
     10.0, 5000.0, 0.5
     25.0, 5000.0, 0.1

* **JSON Format** (FLORIS and PyWake compatible)::

     {
       "power_thrust_table": {
         "wind_speed": [3.0, 5.0, 10.0, 25.0],
         "power": [0.0, 1000.0, 5000.0, 5000.0],
         "thrust_coefficient": [0.8, 0.78, 0.5, 0.1]
       }
     }

Electrical Wire Loading (Power Lines & Infrastructure)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Overhead electrical power transmission lines, telecommunications cables, and other wire-based infrastructure can be assessed for wind-induced mechanical loading and thermal rating limits. The solver computes:

- **Aerodynamic drag force** on conductor spans (perpendicular wind component)
- **Steady-state conductor temperature** (heat balance: convection + radiation - solar heating - Joule heating)
- **Dynamic Line Rating (DLR) / Ampacity limit** (maximum allowable current before overheating)
- **Conductor sway angle** (mechanical deflection from vertical)

Wire spans are specified in a CSV file with one conductor per line. Each row contains geometric, electrical, and thermal properties::

    # [optional_id], x1, y1, z1, x2, y2, z2, diameter, mass_density, drag_coeff, resistance, emissivity, absorptivity, current
    0, 10.0, 50.0, 40.0, 90.0, 50.0, 40.0, 0.0286, 1.628, 1.0, 0.0000728, 0.5, 0.5, 500.0

**Column Definitions**:

- **x1, y1, z1** (Real): Start point of the conductor span [m] (projected domain coordinates).
- **x2, y2, z2** (Real): End point of the conductor span [m].
- **diameter** (Real): Conductor outer diameter [m].
- **mass_density** (Real): Linear mass density [kg/m].
- **drag_coeff** (Real): Aerodynamic drag coefficient [dimensionless], typically 1.0 for cylinders.
- **resistance** (Real): AC electrical resistance per unit length [ohm/m].
- **emissivity** (Real): Thermal surface emissivity [0.0 to 1.0], typically 0.5 for bare conductors.
- **absorptivity** (Real): Solar absorptivity [0.0 to 1.0], typically 0.5 for bare conductors.
- **current** (Real): Steady-state operating electrical current [A].

**Configuration Parameters** (in ``inputs.i``):

- **enable_wire_loading** (Boolean, Default: ``false``): Activate power line infrastructure assessment.
- **wire_file** (String, Default: ``wires.csv``): Path to CSV file containing wire span definitions.
- **wire_output_file** (String, Default: ``wire_output.csv``): Output CSV filename for wire loading results.
- **solar_radiation** (Real, Default: ``0.0``): Solar radiation intensity [W/m²]. Used in conductor temperature balance.

**Output CSV Format** (written to ``wire_output_file``):

::

    wire_id,x1,y1,z1,x2,y2,z2,diameter,mass_density,drag_coeff,resistance,emissivity,absorptivity,current,avg_wind_speed,max_wind_speed,total_drag_force,max_drag_force_per_m,conductor_temp_K,max_capacity_A,sway_angle_deg,time_step

Fields include:
- **avg_wind_speed**, **max_wind_speed** [m/s]: Average and peak wind speed along the span.
- **total_drag_force** [N]: Integrated wind drag force on the entire span.
- **max_drag_force_per_m** [N/m]: Peak specific drag force per unit length.
- **conductor_temp_K** [K]: Average steady-state conductor temperature.
- **max_capacity_A** [A]: Maximum allowable current (DLR limit) before reaching critical temperature (373.15 K / 100°C).
- **sway_angle_deg** [degrees]: Average mechanical sway angle from vertical.

**Example Configuration**::

    terrain_file = terrain.csv
    enable_wire_loading = true
    wire_file = wires.csv
    wire_output_file = wire_results.csv
    solar_radiation = 500.0
    U_ref = 10.0
    z_ref = 10.0
    z0 = 0.1
    dx = 30.0
    dy = 30.0
    dz = 10.0
    plot_file = plt_wind

Run the solver::

    ./build/wind_solver inputs.i

3D Meteorological Ingestion (NetCDF)
------------------------------------

The C++ solver can ingest pre-processed 3D wind fields from larger-scale weather prediction models (such as WRF or GFS outputs) through the `tools/netcdf_to_windfield.py` utility.

1. **Interpolation and Parsing**:
   
   .. code-block:: bash

      python3 tools/netcdf_to_windfield.py \
        --nc-files wrf_t1.nc wrf_t2.nc \
        --inputs inputs.i \
        --output windfield.csv \
        --time 50.0

2. **Solver Configuration**:
   Configure the solver in ``inputs.i`` to load this wind field:

   .. code-block:: ini

      init_mode = windfield
      windfield_file = windfield.csv

FLORIS Integration
------------------

The solver can export wind speeds in formats compatible with NREL's FLORIS (Wind Farm Simulation Software) without requiring a local FLORIS installation:

.. code-block:: bash

    python3 tools/floris_export.py \
        --solver inputs.i \
        --turbines turbines.csv \
        --hub-height 90.0 \
        --reference-speed 10.0 \
        --output wind_data.csv

Wind Farm & Coupling Integration
--------------------------------

PyWake Integration and Site Export
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The solver integrates directly with DTU's PyWake wind farm simulation library. This integration allows users to format and extract mass-consistent wind fields as native PyWake ``Site`` or ``WAsPGridSite`` structures. Additionally, grid maps of terrain, roughness, wind speed, and wind direction can be exported to Surfer ASCII ``.grd`` formats compatible with WAsP.

.. code-block:: python

    import sys
    from wind_solver import WindSolver
    from pywake_coupling import MassConsistentSite, to_wasp_grid_site

    # 1. Initialize and execute the mass-consistent wind solver
    wind = WindSolver("inputs.i")
    wind.solve()

    # 2. Extract resolved fields as a standard PyWake Site object
    site = MassConsistentSite(wind)
    
    # Query local wind properties at multiple coordinates and heights
    local_wind = site.local_wind(x=[100.0, 500.0], y=[200.0, 200.0], h=[90.0, 90.0])
    print("Local wind directions:", local_wind.WD_ilk)
    print("Local wind speeds:", local_wind.WS_ilk)

    # 3. Export resolved wind fields to WAsP Surfer GRD files and instantiate a WAsPGridSite
    wasp_site = to_wasp_grid_site(wind, height_agl=90.0, output_dir="wasp_grids")
    print("WAsP Surfer GRD files successfully generated in 'wasp_grids/'")

    # 4. Clean up resources
    wind.finalize()

FLORIS Integration and Export
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The solver provides seamless export of resolved wind fields to formats required by NREL's FLORIS (Wind Farm Simulation Software). Using the ``floris_coupling`` module, users can extract local wind speed and direction profiles at turbine hub heights, save to JSON or CSV formats, or obtain speed-up ratios relative to free-stream reference wind.

.. code-block:: python

    from wind_solver import WindSolver
    from floris_coupling import FLORISWindMap, quick_export

    # 1. Initialize and execute the mass-consistent wind solver
    wind = WindSolver("inputs.i")
    wind.solve()

    # 2. Extract resolved fields as a FLORISWindMap
    wind_map = FLORISWindMap(wind)

    # 3. Interpolate wind properties to turbine positions
    turbines = [(150.0, 250.0), (350.0, 450.0)]
    wind_map.export_to_csv(turbines, hub_height=90.0, output_file="floris_wind.csv")

    # 4. Clean up resources
    wind.finalize()

Wind Turbine Yaw Setup & Wake Steering
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To configure wake deflection and secondary steering via yaw, specify the active wake parameterizations in ``inputs.i`` and define specific yaw angles in ``turbines.csv``:

1. **Enable models in inputs.i**:

   .. code-block:: ini

       enable_turbine_wake = true
       turbine_file = turbines.csv
       turbine_wake_model_type = gch            # Gauss-Curl Hybrid (secondary steering)
       enable_jimenez_deflection = false        # Jimenez model for yawed turbines
       enable_bastankhah_deflection = true      # Bastankhah & Porté-Agel (2016) deflection model
       jimenez_kd = 0.05                        # Jimenez deflection decay rate
       wake_added_turbulence_model = frandsen   # Wake-added turbulence model

2. **Specify layout and yaw values in turbines.csv**:

   .. code-block:: csv

       # x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, [power_curve_file]
       200.0, 500.0, 80.0, 110.0, 0.8, 20.0, 0.0, nrel_5mw.csv
       600.0, 500.0, 80.0, 110.0, 0.8, 0.0, 0.0, nrel_5mw.csv

Coupling Integration with External Solvers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Programmatic coupling allows passing dynamic 3D wind velocity arrays to external solvers (e.g., wild-land fire spread solvers such as ``wildfire_levelset``) in a tight-coupling framework.

.. code-block:: python

    import time
    from wind_solver import WindSolver
    from wildfire_solver import WildfireSolver

    # Initialize the wind and fire solvers
    wind = WindSolver("wind_inputs.i")
    fire = WildfireSolver("fire_inputs.i")

    # Coupling loop
    step = 0
    while fire.time < 3600.0:
        # Step A: Update and solve mass-consistent wind field
        wind.solve()
        vel_3d = wind.get_velocity()
        
        # Step B: Pass 3D wind arrays (nz, ny, nx) to the fire spread solver
        # The fire solver maps these onto its internal grid using vertical interpolation
        fire.update_wind_3d(
            vel_3d['u'], 
            vel_3d['v'], 
            vel_3d['w'],
            wind.nz, 
            wind.zmin, 
            wind.zmax
        )

        # Step C: Advance the wildfire levelset front
        fire.step()
        print(f"Step {step}: advanced fire simulation time to {fire.time:.1f} s")
        step += 1

    wind.finalize()
    fire.finalize()

Walk-through Tutorials
----------------------

This section provides comprehensive, hands-on tutorials progressing from simple to advanced scenarios. Each tutorial is organized by primary physics modeling focus, with complete example configurations and step-by-step explanations of each parameter.

Mass Consistent Solver Fundamentals
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Tutorial 1.1: Simple Baseline Setup (Flat Terrain)**

This basic tutorial demonstrates the essential parameters needed to run a simple mass-consistent wind solver on flat terrain.

**Key Concepts:**

* Reference wind velocity and height
* Aerodynamic roughness (surface roughness length)
* Computational grid spacing
* Domain vertical extent
* Basic output configuration

**Annotated Input File:**

.. code-block:: ini

    # === ESSENTIAL PARAMETERS FOR BASIC RUNS ===
    # Terrain configuration - use "synthetic" for flat terrain generation
    terrain_file = terrain.csv      # Path to terrain point-cloud (X Y Z format)
    
    # Wind profile initialization 
    init_mode    = loglaw           # Use logarithmic profile (van Kármán)
    U_ref        = 10.0             # Reference wind speed in x-direction [m/s]
    V_ref        = 0.0              # Reference wind speed in y-direction [m/s]
    z_ref        = 10.0             # Height where U_ref/V_ref are defined [m AGL]
    z0           = 0.1              # Aerodynamic roughness length [m] - 0.05-0.1 for grass, 0.5-1.5 for urban
    
    # Computational domain and grid
    dx           = 30.0             # Horizontal grid spacing in x-direction [m]
    dy           = 30.0             # Horizontal grid spacing in y-direction [m]
    dz           = 15.0             # Vertical grid spacing [m]
    domain_height = 300.0           # Total domain height above terrain [m]
    
    # Mass-consistency: Poisson equation anisotropy coefficients
    alpha_h      = 1.0              # Horizontal penalty coefficient (1.0 = isotropic)
    alpha_v      = 1.0              # Vertical penalty coefficient
    
    # Linear solver settings (MLMG: Multi-Level Multi-Grid)
    mlmg_verbose = 1                # Verbosity: 0=silent, 1=basic, 4=detailed
    tol_rel      = 1.e-8            # Relative convergence tolerance
    max_grid_size = 32              # Maximum AMReX box size per dimension
    
    # Output configuration
    plot_file    = plt_baseline     # Output plotfile prefix (creates plt_baseline00000/)
    extract_agl  = 20.0             # Extract 2D wind field at 20m AGL
    extract_file = wind_extract.csv # CSV output filename

**Example Execution:**

.. code-block:: bash

    ./build/wind_solver inputs.i
    # Output: AMReX plotfile (plt_baseline00000/) + CSV extraction (wind_extract.csv)

**Expected Results:**

* A 3D AMReX plotfile containing (u, v, w) velocity components
* A CSV file with 2D wind field at specified AGL height
* Logarithmic velocity profile vertically, mass-consistent horizontally

**Common Modifications:**

- Change ``z0`` to model different surface types (0.001 for ocean, 0.3 for trees)
- Adjust ``dx/dy/dz`` for finer/coarser resolution vs. computational cost
- Set ``extract_agl`` to multiple heights for vertical profile analysis
- Increase ``domain_height`` for taller domains (e.g., 600m for complex terrain)

**Tutorial 1.2: Gaussian Hill with Advanced Parameters**

This tutorial adds a realistic synthetic terrain and introduces height-dependent anisotropy, Ekman spiral effects, and buoyancy stratification.

**Key Concepts:**

* Synthetic terrain generation
* Height-dependent anisotropy coefficients
* Ekman spiral veer corrections
* Atmospheric stability and buoyancy effects
* High-order derivative schemes (WENO-5)

**Annotated Input File:**

.. code-block:: ini

    # === SYNTHETIC TERRAIN SETUP ===
    # Generate Gaussian hill terrain programmatically instead of reading file
    terrain_file = synthetic
    synthetic_type = gaussian_hill
    synthetic_xmin = 0.0
    synthetic_xmax = 1000.0
    synthetic_ymin = 0.0
    synthetic_ymax = 1000.0
    synthetic_nx = 21                   # Grid points for terrain interpolation
    synthetic_ny = 21
    synthetic_peak = 150.0              # Hill peak elevation [m]
    synthetic_sigma = 200.0             # Gaussian width [m]
    synthetic_center_x = 500.0          # Hill center X coordinate
    synthetic_center_y = 500.0          # Hill center Y coordinate
    
    # === WIND INITIALIZATION ===
    init_mode    = loglaw
    U_ref        = 12.0
    V_ref        = -3.0               # Non-zero cross-wind (oblique flow)
    z_ref        = 10.0
    z0           = 0.08
    
    # === GRID CONFIGURATION ===
    dx           = 40.0
    dy           = 40.0
    dz           = 20.0
    domain_height = 400.0             # Tall enough for complex terrain (2-3× max elevation)
    
    # === HEIGHT-DEPENDENT ANISOTROPY (Advanced) ===
    # Penalty coefficients vary with height to capture boundary layer effects
    use_height_dependent_alpha_v = true
    alpha_v_surface = 1.5             # Stronger vertical coupling near ground
    alpha_v_top     = 0.5             # Weaker coupling aloft
    alpha_h         = 1.0             # Constant horizontal coupling
    
    # === EKMAN SPIRAL VEER (Geostrophic effect) ===
    # Wind direction rotates with height due to Coriolis force
    enable_ekman_veer = true
    latitude = 45.0                   # Latitude [degrees N]
    ekman_veer_total = 30.0           # Total veer from surface to top [degrees]
    ekman_veer_height = 300.0         # Height over which veer occurs [m]
    
    # === BUOYANCY STRATIFICATION (Atmospheric Stability) ===
    # Include thermal effects (density variations)
    enable_buoyancy_stratification = true
    temperature_file = temperature.csv  # Vertical temperature profile
    temperature_reference = 288.0       # Reference temperature [K]
    buoyancy_coefficient = 0.5         # Strength of thermal coupling
    buoyancy_method = velocity         # Method: "velocity", "rhs"
    
    # === NUMERICAL METHOD SELECTION ===
    deriv_method = weno5              # High-order derivatives: weno3, weno5
    tol_rel = 1.e-9                   # Tighter tolerance for complex physics
    mlmg_max_iter = 300               # Max iterations (increased for complex cases)
    mlmg_bottom_solver = bicgstab     # Bottom solver: bicgstab, cg
    
    # === STABILITY CORRECTIONS ===
    enable_stability_correction = true
    stability_length = -100.0         # Monin-Obukhov length (negative=unstable)
    
    # === OUTPUT ===
    plot_file = plt_gaussian_hill_advanced
    extract_agl = 25.0
    extract_file = wind_extract_advanced.csv

**Example Execution:**

.. code-block:: bash

    # Requires temperature.csv in same directory (vertical T profile)
    ./build/wind_solver inputs.i

**Expected Results:**

* Directional shear (Ekman veer) visible in horizontal velocity field
* Enhanced wind acceleration over hill crest
* Stability effects modifying vertical mixing
* Smoother velocity gradients from WENO-5 derivatives

**Exhaustive Input File Reference for Mass Consistent Solver**

Below is a comprehensive input file containing all possible mass-consistent solver parameters with detailed comments:

.. code-block:: ini

    # ================================================================================
    # COMPLETE MASS-CONSISTENT AMR WIND SOLVER INPUT FILE
    # ================================================================================
    # This file documents ALL parameters for configuring the mass-consistent solver.
    # Only parameters with non-default values need to be specified in practice.
    # ================================================================================

    # --- TERRAIN & DOMAIN SETUP ---
    terrain_file = terrain.csv                # Terrain point-cloud or "synthetic"
    
    # Synthetic terrain generation (if terrain_file = synthetic)
    synthetic_type = multi_gaussian_hill      # gaussian_hill, multi_gaussian_hill
    synthetic_xmin = 0.0
    synthetic_xmax = 1000.0
    synthetic_ymin = 0.0
    synthetic_ymax = 1000.0
    synthetic_nx = 21
    synthetic_ny = 21
    synthetic_peak = 200.0                    # Single hill peak [m] (gaussian_hill only)
    synthetic_sigma = 150.0                   # Gaussian width [m]
    synthetic_center_x = 500.0                # Hill center X (gaussian_hill only)
    synthetic_center_y = 500.0                # Hill center Y (gaussian_hill only)
    synthetic_peaks = 200.0 150.0             # Multiple peaks [m] (multi_gaussian_hill)
    synthetic_sigmas = 150.0 120.0            # Multiple widths [m]
    synthetic_centers_x = 300.0 700.0         # Multiple centers X [m]
    synthetic_centers_y = 400.0 600.0         # Multiple centers Y [m]

    # --- WIND INITIALIZATION ---
    init_mode = loglaw                        # Log-law profile initialization
    U_ref = 10.0                              # Reference x-wind [m/s]
    V_ref = 0.0                               # Reference y-wind [m/s]
    z_ref = 10.0                              # Reference height [m AGL]
    z0 = 0.1                                  # Aerodynamic roughness [m]
    
    # Ekman spiral veer (Coriolis effect)
    enable_ekman_veer = false
    latitude = 45.0                           # Latitude [degrees N]
    ekman_veer_total = 30.0                   # Total veer [degrees]
    ekman_veer_height = 300.0                 # Veer transition height [m]

    # --- GRID & DOMAIN GEOMETRY ---
    dx = 30.0                                 # Grid spacing x [m]
    dy = 30.0                                 # Grid spacing y [m]
    dz = 15.0                                 # Grid spacing z [m]
    domain_height = 300.0                     # Domain top above terrain [m]
    
    # Optional: Direct cell count specification (overrides spacing)
    # ncells_x = 64
    # ncells_y = 64
    # ncells_z = 32

    # --- MASS CONSISTENCY (Poisson Equation) ---
    alpha_h = 1.0                             # Horizontal penalty coefficient
    alpha_v = 1.0                             # Vertical penalty coefficient
    
    # Height-dependent anisotropy
    use_height_dependent_alpha_v = false
    alpha_v_surface = 1.5                     # Surface value
    alpha_v_top = 0.5                         # Top value
    
    # Cell-local anisotropy based on local terrain slope
    enable_cell_local_anisotropy = false
    anisotropy_source = all                   # all, terrain, buildings

    # --- ATMOSPHERIC PHYSICS ---
    # Stability and stratification
    enable_stability_correction = false
    stability_length = -100.0                 # Monin-Obukhov length [m]
    
    # Buoyancy stratification
    enable_buoyancy_stratification = false
    temperature_file = temperature.csv
    temperature_reference = 288.0             # Reference T [K]
    buoyancy_coefficient = 1.0
    buoyancy_method = velocity                # velocity, rhs
    enable_diurnal_temperature = false
    
    # Thermal effects and circulation
    enable_thermal_circulation = false
    enable_slope_flows = false
    enable_katabatic_flow = false             # Downslope flows
    enable_valley_channeling = false          # Valley wind channeling
    enable_gap_flow = false                   # Gap wind acceleration

    # Capping inversion lid
    enable_capping_lid = false
    capping_lid_height = 1000.0               # Inversion height [m]
    capping_lid_strength = 0.01               # Strength parameter

    # --- NUMERICAL METHODS ---
    deriv_method = central                    # central, weno3, weno5
    tol_rel = 1.e-8                           # MLMG relative tolerance
    tol_abs = 1.e-12                          # MLMG absolute tolerance
    mlmg_verbose = 1                          # Verbosity: 0-4
    mlmg_max_iter = 200
    mlmg_bottom_solver = bicgstab             # bicgstab, cg, ilu
    max_grid_size = 32                        # Max AMReX box size

    # --- TEMPORAL SETTINGS ---
    enable_time_varying = false
    time_series_file = time_series.csv        # Transient forcing

    # --- I/O & DIAGNOSTICS ---
    plot_file = plt_wind                      # Output plotfile prefix
    extract_agl = 20.0                        # Extraction height [m AGL]
    extract_file = wind_extract.csv           # CSV output file
    diagnostic_output_interval = 1            # Output frequency

Building Wake Physics Modeling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Tutorial 2.1: Simple Building Wake (Single Building)**

This tutorial introduces basic building wake modeling using the Röckle (1990) parameterization with a single rectangular building.

**Key Concepts:**

* Building file format (CSV with position, dimensions)
* Wake deficit zones (cavity, far-wake)
* Building wake enhancement options
* Wind field modification in urban areas

**Annotated Input File:**

.. code-block:: ini

    # === TERRAIN & DOMAIN ===
    terrain_file = synthetic
    synthetic_type = gaussian_hill
    synthetic_peak = 100.0
    synthetic_sigma = 200.0
    synthetic_center_x = 500.0
    synthetic_center_y = 500.0
    
    # === WIND INITIALIZATION ===
    init_mode = loglaw
    U_ref = 10.0
    V_ref = 0.0
    z_ref = 10.0
    z0 = 0.2                               # Urban roughness
    
    # === GRID & DOMAIN ===
    dx = 20.0
    dy = 20.0
    dz = 10.0
    domain_height = 250.0
    
    # === MASS CONSISTENCY ===
    alpha_h = 1.0
    alpha_v = 1.0
    
    # === BUILDING WAKE MODELING (Basic) ===
    enable_wake = true                     # Enable building wake effects
    building_file = buildings.csv           # CSV: xmin,xmax,ymin,ymax,zmin,zmax [m]
    wake_model_type = rockle                # Röckle (1990) model
    
    # Use default enhancements (recommended for most applications)
    enable_oblique_scaling = true           # Adjust wake extent for non-perpendicular wind
    enable_tall_building_correction = true  # Correct for tall buildings
    enable_upwind_recirculation = true      # Model reverse flow upstream
    enable_corner_acceleration = true       # Velocity amplification at edges
    enable_extended_farwake = true          # Extend far-wake to 15H
    enable_horseshoe_vortex = true          # Secondary circulation at base
    
    # === OUTPUT ===
    plot_file = plt_building_wake_simple
    extract_agl = 20.0
    extract_file = wind_building_simple.csv

**Building CSV Format:**

.. code-block:: text

    xmin,xmax,ymin,ymax,zmin,zmax
    400.0,500.0,450.0,550.0,0.0,30.0

Each row defines one building: X, Y, Z bounds [m] (zmin is typically ground, zmax is roof height).

**Expected Results:**

* Cavity zone (0-1.5H downstream): ~30-50% velocity deficit
* Far-wake zone (1.5H-15H): Gradually recovering velocity
* Upwind recirculation zone (~0.5H upstream): Weak reverse flow
* Flow acceleration at building corners (~10-20% enhancement)

**Tutorial 2.2: Urban Building Array with Advanced Wake Physics**

This tutorial models multiple buildings in a street canyon configuration with advanced wake physics enhancements enabled selectively.

**Key Concepts:**

* Multiple building interactions (wake merging)
* Street canyon effects (Yoshie two-layer model)
* Wake superposition methods (quadratic, cubic)
* Advanced physics: Gaussian profiles, variance correction, log-law reference

**Annotated Input File:**

.. code-block:: ini

    # === COMPLEX TERRAIN & DOMAIN ===
    terrain_file = terrain.csv             # Real DEM or generated terrain
    
    # === WIND INITIALIZATION (Oblique Flow) ===
    init_mode = loglaw
    U_ref = 12.0
    V_ref = -4.0                           # 30° oblique approach
    z_ref = 10.0
    z0 = 0.3                               # Dense urban environment
    
    # === GRID & DOMAIN ===
    dx = 15.0
    dy = 15.0
    dz = 8.0
    domain_height = 300.0
    
    # === ANISOTROPY FOR URBAN FLOWS ===
    alpha_h = 1.2                          # Slightly higher horizontal coupling
    alpha_v = 0.8                          # Lower vertical coupling
    use_height_dependent_alpha_v = true
    alpha_v_surface = 1.5
    alpha_v_top = 0.5
    
    # === MULTIPLE BUILDINGS & ARRAYS ===
    enable_wake = true
    building_file = urban_blocks.csv       # Many buildings (100s)
    wake_model_type = rockle
    
    # Street canyon effects (Yoshie two-layer model)
    enable_yoshie_two_layer = true         # Two-layer recirculation in canyons
    
    # Advanced wake physics for detailed accuracy
    enable_oblique_scaling = true          # Critical for non-perpendicular flows
    enable_tall_building_correction = true # Many tall buildings
    enable_gaussian_profile = true         # Smooth lateral deficit (not rectangular)
    enable_upwind_recirculation = true
    enable_reference_correction = true     # Use log-law velocity at height
    enable_corner_acceleration = true
    enable_variance_correction = true      # Modify turbulence intensity
    enable_horseshoe_vortex = true
    enable_extended_farwake = true
    
    # Wake superposition (combining multiple deficits)
    turbine_wake_superposition = quadratic # quadratic or cubic
    
    # === OPTIONAL: STREET CANYON MODELING ===
    enable_street_canyon = true
    landuse_file = landuse.csv             # Urban classification
    
    # === OUTPUT ===
    plot_file = plt_urban_array_advanced
    extract_agl = 15.0
    extract_file = wind_urban_array.csv

**Expected Results:**

* Asymmetric flow patterns in street canyons (Yoshie two-layer response)
* Gaussian profiles providing smooth transitions between buildings
* Stronger upwind effects due to reference correction
* Variance correction affecting turbulence intensity estimates

**Exhaustive Input File for Building Wake Modeling**

.. code-block:: ini

    # ================================================================================
    # BUILDING WAKE PHYSICS CONFIGURATION REFERENCE
    # ================================================================================
    
    # === BASIC BUILDING SETUP ===
    enable_wake = false                    # Enable building wake effects
    building_file = buildings.csv          # CSV file: xmin,xmax,ymin,ymax,zmin,zmax [m]
    wake_model_type = rockle               # rockle (default) or huber_snyder
    
    # === BUILDING WAKE ENHANCEMENTS (9 Total) ===
    # Each can be independently enabled/disabled
    
    # 1. Far-wake extension to 15H (vs. standard 3-5H)
    enable_extended_farwake = true         # Default: true
    
    # 2. Oblique angle cavity scaling
    enable_oblique_scaling = true          # Default: true
    
    # 3. Tall-building aspect-ratio correction
    enable_tall_building_correction = true # Default: true
    
    # 4. Gaussian lateral wake profile
    enable_gaussian_profile = false        # Default: false (uses rectangular)
    
    # 5. Upwind recirculation zone
    enable_upwind_recirculation = true     # Default: true
    
    # 6. Log-law reference velocity correction
    enable_reference_correction = false    # Default: false
    
    # 7. Corner and side acceleration
    enable_corner_acceleration = true      # Default: true
    
    # 8. Height-dependent velocity variance
    enable_variance_correction = false     # Default: false
    
    # 9. Horseshoe vortex modeling
    enable_horseshoe_vortex = true         # Default: true
    
    # === ADVANCED URBAN PHYSICS ===
    enable_yoshie_two_layer = true         # Yoshie two-layer canyon model
    enable_rodi_entrainment = false        # Rodi entrainment model
    enable_lopes_comfort = false           # Lopes comfort index
    enable_oikonomou_aspect = false        # Oikonomou aspect ratio effects
    enable_britter_hanna_urban = false     # Britter-Hanna urban model
    
    # === WAKE SUPERPOSITION (Multiple Buildings) ===
    turbine_wake_superposition = quadratic # quadratic, cubic, or linear
    
    # === STREET CANYON SPECIFIC ===
    enable_street_canyon = false           # Street canyon detection and modeling
    landuse_file = ""                      # Urban/rural classification
    
    # === BUILDING POROSITY & PERMEABILITY ===
    enable_building_porosity = false       # Allow flow through buildings
    building_porosity_file = ""            # Porosity coefficients per building

Turbine Wake Physics Modeling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Tutorial 3.1: Single Wind Turbine Wake**

This tutorial demonstrates basic wind turbine wake modeling using analytical wake models with a single turbine.

**Key Concepts:**

* Turbine file format (CSV with position, rotor diameter, hub height, power curve)
* Wake deficit models (Jensen, Bastankhah, TurbOPark)
* Power output calculation
* Wake ground interaction effects

**Annotated Input File:**

.. code-block:: ini

    # === TERRAIN & DOMAIN ===
    terrain_file = synthetic
    synthetic_type = gaussian_hill
    synthetic_peak = 80.0
    synthetic_sigma = 250.0
    
    # === WIND INITIALIZATION ===
    init_mode = loglaw
    U_ref = 10.0
    V_ref = 0.0
    z_ref = 10.0
    z0 = 0.05
    
    # === GRID & DOMAIN ===
    dx = 25.0
    dy = 25.0
    dz = 15.0
    domain_height = 400.0                  # Tall for wind turbines (3-4× hub height)
    
    # === MASS CONSISTENCY ===
    alpha_h = 1.0
    alpha_v = 1.0
    
    # === TURBINE WAKE MODELING ===
    enable_turbine_wake = true             # Enable turbine wake effects
    turbine_file = turbines.csv            # CSV: x,y,z,D,H,power_curve.csv
    turbine_wake_model_type = jensen       # jensen, bastankhah, turbopark
    
    # Wake deficit superposition (how wakes combine)
    turbine_wake_superposition = quadratic # quadratic, cubic, or linear
    
    # Ground interaction: mirror turbine technique
    enable_wake_ground_interaction = true
    
    # Advanced wake deflection for yawed turbines
    enable_jimenez_deflection = false      # Jimenez yaw deflection model
    enable_bastankhah_deflection = false   # Bastankhah yaw deflection model
    
    # === OUTPUT ===
    plot_file = plt_turbine_wake_single
    extract_agl = 100.0                    # Extract at hub height
    extract_file = wind_turbine_single.csv

**Turbine CSV Format:**

.. code-block:: text

    x,y,z,D,H,power_curve_file
    500.0,500.0,0.0,120.0,100.0,power_curve.csv

Where:
- (x, y, z): Turbine location [m], z typically 0 (ground)
- D: Rotor diameter [m]
- H: Hub height above ground [m]
- power_curve_file: CSV with (U_wind, P_output) pairs

**Expected Results:**

* Velocity deficit cone extending 10-15 rotor diameters downstream
* Gaussian profile of deficit in lateral direction
* Power output varies with upstream wind speed (from power curve)
* Ground-mirrored wake effect (weaker reflection in lower half-space)

**Tutorial 3.2: Wind Farm with Multiple Turbines and Yaw Control**

This tutorial models a multi-turbine wind farm with optimized yaw angles for wake steering and includes advanced deflection models.

**Key Concepts:**

* Multiple turbine interactions (wake superposition)
* Yaw control and wake deflection (Jimenez, Bastankhah models)
* Wake-added turbulence (increased mixing downwind)
* Farm-scale power optimization
* Stability effects on wake recovery

**Annotated Input File:**

.. code-block:: ini

    # === REAL TERRAIN ===
    terrain_file = terrain.csv             # Actual DEM/point cloud
    
    # === WIND INITIALIZATION ===
    init_mode = loglaw
    U_ref = 11.0
    V_ref = 0.0
    z_ref = 10.0
    z0 = 0.1
    
    # === GRID (Finer for farm layout) ===
    dx = 20.0
    dy = 20.0
    dz = 10.0
    domain_height = 500.0
    
    # === ANISOTROPY FOR WIND FARM ===
    alpha_h = 0.9                          # Slightly reduced for farm effects
    alpha_v = 1.0
    use_height_dependent_alpha_v = true
    alpha_v_surface = 1.2
    alpha_v_top = 0.6
    
    # === MULTIPLE TURBINES WITH OPTIMIZATION ===
    enable_turbine_wake = true
    turbine_file = farm_layout.csv         # Many turbines (>10)
    turbine_wake_model_type = bastankhah   # Advanced model
    turbine_wake_superposition = cubic     # More accurate for overlapping wakes
    
    # === WAKE DEFLECTION (Yaw Steering) ===
    # Advanced models for optimizing power output via yaw angles
    enable_jimenez_deflection = true       # Jimenez (2010) deflection model
    enable_bastankhah_deflection = true    # Bastankhah (2016) counter-rotating vortex
    
    # === WAKE-ADDED TURBULENCE ===
    # Turbulence intensity increases in wakes, affecting wake recovery
    wake_added_turbulence_model = default  # Applies turbulence increase downwind
    
    # === GROUND INTERACTION ===
    enable_wake_ground_interaction = true
    
    # === ATMOSPHERIC STABILITY ===
    enable_stability_correction = true
    stability_length = 300.0               # Neutral (large L value)
    # Neutral conditions → faster wake recovery than stable
    # Unstable conditions (negative L) → slower wake recovery
    
    # === ADVANCED: BUOYANCY (Temperature Effects) ===
    enable_buoyancy_stratification = true
    temperature_file = temperature_profile.csv
    buoyancy_coefficient = 1.0
    
    # === OUTPUT WITH DETAILED DIAGNOSTICS ===
    plot_file = plt_farm_optimized
    extract_agl = 100.0
    extract_file = wind_farm_power.csv
    turbine_power_output_file = farm_power_output.csv
    turbine_energy_production_file = farm_annual_energy.csv

**Farm CSV Format:**

.. code-block:: text

    x,y,z,D,H,power_curve_file,yaw_angle
    0.0,0.0,0.0,120.0,100.0,power_5mw.csv,0.0
    500.0,0.0,0.0,120.0,100.0,power_5mw.csv,25.0
    1000.0,0.0,0.0,120.0,100.0,power_5mw.csv,0.0

Where yaw_angle is positive for clockwise rotation (when viewed from above).

**Expected Results:**

* Front-row turbines produce nominal power
* Wake deficits from upstream turbines reduce power of downwind turbines
* Yaw steering deflects wakes laterally, reducing downwind losses
* Wake recovery faster in stable conditions, slower in unstable
* Cubic superposition captures non-linear wake merging better than quadratic

**Important Note: Capability Conflicts**

.. note::

    **Do not combine building and turbine wakes in the same simulation** at this time. The solver includes validation that will issue a warning if both ``enable_wake=true`` and ``enable_turbine_wake=true``. These systems use different methodologies and mixing them may produce unphysical results. Choose one or the other for your application.

**Exhaustive Turbine Wake Configuration Reference**

.. code-block:: ini

    # ================================================================================
    # TURBINE WAKE PHYSICS CONFIGURATION REFERENCE
    # ================================================================================
    
    # === BASIC TURBINE SETUP ===
    enable_turbine_wake = false            # Enable turbine wake modeling
    turbine_file = turbines.csv            # CSV: x,y,z,D,H,power_curve.csv[,yaw]
    
    # === WAKE DEFICIT MODELS ===
    turbine_wake_model_type = jensen       # jensen, bastankhah, turbopark
    
    # Jensen (Park) Model: D_wake(x) = D + 2*k_w*x
    # - Simple, fast, good for small farms
    # - k_w ≈ 0.04-0.05
    
    # Bastankhah Model: Gaussian deficit with counter-rotating vortex pair
    # - More physics-based, better far-wake recovery
    # - Better for medium/large farms
    
    # TurbOPark Model: Dynamic wake expansion from local turbulence intensity
    # - Most advanced, best for complex physics
    # - Computationally intensive
    
    # === SUPERPOSITION METHOD ===
    turbine_wake_superposition = quadratic # quadratic, cubic, linear
    
    # quadratic: sqrt(D1^2 + D2^2 + ...) - default, conservative
    # cubic: (D1^3 + D2^3 + ...)^(1/3) - more realistic overlaps
    # linear: D1 + D2 + ... - least physical
    
    # === WAKE DEFLECTION (YAW STEERING) ===
    enable_jimenez_deflection = false      # Jimenez et al. (2010) model
    enable_bastankhah_deflection = false   # Bastankhah et al. (2016) model
    # Note: Bastankhah deflection only works with Bastankhah wake model
    
    # === WAKE-ADDED TURBULENCE ===
    wake_added_turbulence_model = none     # none, default, or custom
    # Increases turbulence intensity in wakes, affecting wake recovery
    
    # === GROUND INTERACTION ===
    enable_wake_ground_interaction = true  # Mirror turbine technique
    # F_damp factor applied for surface shear layer
    
    # === DIAGNOSTICS & OUTPUT ===
    turbine_power_output_file = ""         # Per-turbine power output
    turbine_energy_production_file = ""    # Time-integrated energy

Other Useful Walk-through Tutorials
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Tutorial 4.1: Forest Canopy Drag and Wind Reduction**

This tutorial demonstrates modeling wind attenuation through forest canopies using porous media drag parameterization.

**Key Concepts:**

* Canopy height and frontal area index (FAI)
* Plan area index (PAI) - fraction of ground covered
* Exponential velocity attenuation with height
* Drag coefficient calibration for different forest types

**Annotated Input File:**

.. code-block:: ini

    # === BASIC SETUP ===
    terrain_file = synthetic
    init_mode = loglaw
    U_ref = 12.0
    V_ref = 0.0
    z_ref = 10.0
    z0 = 0.1
    
    dx = 30.0
    dy = 30.0
    dz = 5.0                               # Finer vertical grid in canopy
    domain_height = 200.0
    
    alpha_h = 1.0
    alpha_v = 1.0
    
    # === FOREST CANOPY MODELING ===
    enable_canopy = true
    canopy_height = 25.0                   # Canopy top elevation [m]
    frontal_area_index = 0.3               # FAI - typical 0.2-0.5 for forests
    plan_area_index = 0.4                  # PAI - fraction of horizontal projection
    canopy_drag_coeff = 0.15               # Drag coefficient in canopy
    
    # Vertical structure of canopy
    use_exponential_profile = true
    canopy_attenuation = 2.5               # Decay rate: U(z) = U_top * exp(-attenuation*(h-z)/h)
    
    # === OUTPUT ===
    plot_file = plt_forest_canopy
    extract_agl = 5.0
    extract_file = wind_in_canopy.csv

**Expected Results:**

* Significant wind reduction within canopy (50-80% at base)
* Exponential recovery above canopy top
* Increased effective surface roughness perceived aloft
* Turbulence intensity increase within canopy

**Tutorial 4.2: Atmospheric Stability and Non-Neutral Boundary Layers**

This tutorial demonstrates the effects of atmospheric stratification (stable vs. unstable) on wind profiles and flow patterns.

**Key Concepts:**

* Monin-Obukhov stability length (L)
* Stable (L > 0): strong stratification, weak mixing
* Unstable (L < 0): convective overturning, strong mixing
* Temperature-wind coupling via buoyancy
* Stability effects on terrain-flow interactions

**Annotated Input File:**

.. code-block:: ini

    # === BASE CONFIGURATION ===
    terrain_file = synthetic
    synthetic_type = gaussian_hill
    synthetic_peak = 150.0
    
    init_mode = loglaw
    U_ref = 8.0                            # Lower wind speed in stable conditions
    V_ref = 0.0
    z_ref = 10.0
    z0 = 0.1
    
    dx = 40.0
    dy = 40.0
    dz = 15.0
    domain_height = 400.0
    
    # === STABILITY CONFIGURATION ===
    enable_stability_correction = true
    stability_length = 100.0               # Positive = Stable (strong strat)
    # Examples:
    # stability_length = 500.0     → Very stable (nocturnal)
    # stability_length = 100.0     → Moderately stable
    # stability_length = -100.0    → Unstable (daytime convection)
    # stability_length = -500.0    → Very unstable (strong heating)
    
    # === BUOYANCY & TEMPERATURE EFFECTS ===
    enable_buoyancy_stratification = true
    temperature_file = temperature.csv     # Vertical temperature profile
    temperature_reference = 288.0          # Reference temperature [K]
    buoyancy_coefficient = 1.0             # Coupling strength
    buoyancy_method = velocity             # velocity or rhs
    
    # === OUTPUT ===
    plot_file = plt_stability_effects
    extract_agl = 30.0
    extract_file = wind_stability.csv

**Expected Results:**

* Stable conditions: Stronger wind shear, weaker terrain acceleration
* Unstable conditions: Reduced shear, stronger thermal mixing
* Buoyancy effects visible in vertical velocity (w-component)
* Stability length directly affects velocity profile shape

**Tutorial 4.3: Data Assimilation and Observation Coupling**

This advanced tutorial demonstrates assimilating observational data (sounding profiles, point measurements) into the wind field to improve accuracy.

**Key Concepts:**

* Sounding data (vertical wind/temperature profiles)
* Observation nudging/relaxation
* Blending model predictions with observations
* Kalman-filter style corrections

**Annotated Input File:**

.. code-block:: ini

    # === BASE CONFIGURATION ===
    terrain_file = terrain.csv
    init_mode = loglaw
    U_ref = 10.0
    V_ref = 0.0
    z_ref = 10.0
    z0 = 0.1
    
    dx = 30.0
    dy = 30.0
    dz = 10.0
    domain_height = 300.0
    
    # === DATA ASSIMILATION ===
    enable_data_assimilation = true
    sounding_file = sounding_profile.txt   # Vertical wind profile (Z, U, V, T)
    observation_file = point_obs.csv       # Point measurements (X, Y, Z, U, V, W)
    
    # Assimilation parameters
    data_assimilation_method = optimal_interpolation  # oi or ensemble_kalman
    observation_error_variance = 0.5       # Measurement uncertainty [m/s]^2
    background_error_variance = 1.0        # Model uncertainty [m/s]^2
    assimilation_time_window = 3600.0      # Analysis window [s]
    
    # === OUTPUT ===
    plot_file = plt_assimilated_wind
    extract_agl = 20.0
    extract_file = wind_assimilated.csv

**Expected Results:**

* Wind field adjusted toward observations
* Improved accuracy in vicinity of observation points
* Smooth transition to model predictions away from obs
* Better representation of local phenomena not in coarse model

**Tutorial 4.4: Time-Varying Forcing and Transient Simulations**

This tutorial demonstrates running the solver with time-varying boundary conditions such as changing reference wind or rotating direction.

**Key Concepts:**

* Time series of reference wind speed/direction
* Diurnal cycles (day/night temperature variations)
* Transient response of wind field
* Output at multiple time steps

**Annotated Input File:**

.. code-block:: ini

    # === BASE CONFIGURATION ===
    terrain_file = synthetic
    synthetic_type = gaussian_hill
    
    init_mode = loglaw
    z_ref = 10.0
    z0 = 0.1
    
    dx = 30.0
    dy = 30.0
    dz = 15.0
    domain_height = 300.0
    
    # === TIME-VARYING FORCING ===
    enable_time_varying = true
    time_series_file = hourly_wind.csv     # Format: Time[s], U_ref, V_ref, T_ref
    
    # Example time_series_file content:
    #   0.0,10.0,0.0,288.0
    #   3600.0,11.5,1.0,289.0
    #   7200.0,13.0,2.5,290.5
    #   ...
    
    # === TEMPORAL INTEGRATION ===
    dt = 60.0                              # Time step [s]
    num_timesteps = 96                     # 96 steps × 60s = 5760s ~ 1.6 hours
    diagnostic_output_interval = 10        # Save every 10 timesteps
    
    # === DIURNAL TEMPERATURE CYCLE ===
    enable_diurnal_temperature = true
    diurnal_amplitude = 8.0                # +/- temperature variation [K]
    diurnal_phase = 14.0                   # Peak heating at 2 PM [hours]
    
    # === OUTPUT (Multiple Times) ===
    plot_file = plt_time_varying
    extract_agl = 20.0
    extract_file = wind_time_series.csv

**Expected Results:**

* Multiple output files (plt_time_varying00000, plt_time_varying00001, etc.)
* Wind field gradually adjusts as forcing changes
* Diurnal cycle visible in temperature and buoyancy effects
* CSV extraction at multiple times for time-series analysis

**Tutorial 4.5: Synthetic Turbulence and OpenFAST Export**

This tutorial demonstrates generating synthetic turbulence fluctuations and exporting in OpenFAST-compatible binary format.

**Key Concepts:**

* Von Kármán and Kaimal turbulence spectra
* Spatial coherence models
* Mann box approach for structured turbulence
* BTS (binary turbulence simulation) format for wind turbine tools

**Annotated Input File:**

.. code-block:: ini

    # === BASE WIND FIELD ===
    terrain_file = synthetic
    init_mode = loglaw
    U_ref = 10.0
    V_ref = 0.0
    z_ref = 10.0
    z0 = 0.1
    
    dx = 25.0
    dy = 25.0
    dz = 15.0
    domain_height = 300.0
    
    # === SYNTHETIC TURBULENCE GENERATION ===
    enable_synthetic_turbulence = true
    turbulence_spectrum_model = VonKarman  # VonKarman or Kaimal
    turbulence_intensity_model = PowerLaw  # PowerLaw (IEC std)
    turbulence_coherence_model = Gaussian  # Spatial coherence
    
    # Turbulence parameters
    turbulence_intensity_ref = 0.12        # 12% at reference height
    turbulence_length_scale_u = 300.0      # Integral length scale [m]
    turbulence_length_scale_v = 200.0
    turbulence_length_scale_w = 100.0
    turbulence_random_seed = 12345         # For reproducibility
    
    # === EXPORT FOR OPENFAST ===
    turbulence_export_format = bts         # bts (binary turbulence sim)
    turbulence_output_file = wind_field.bts
    turbulence_gridpoints_time = 100       # Number of time steps
    turbulence_gridpoints_y = 10           # Lateral grid points
    turbulence_gridpoints_z = 10           # Vertical grid points
    
    # === OUTPUT ===
    plot_file = plt_turbulence

**Expected Results:**

* AMReX plotfile with smooth mean wind field
* BTS file with spatially-correlated turbulence fluctuations
* BTS file importable by OpenFAST/TurbSim tools
* Turbulence intensity matches specified profile

Performance Tuning
------------------

MLMG Multigrid Optimization
~~~~~~~~~~~~~~~~~~~~~~~~~~~
For flat terrain, reduce V-cycle iterations per step::

    mlmg_max_iter = 100
    mlmg_pre_smooth = 8
    mlmg_post_smooth = 8

GPU Performance
~~~~~~~~~~~~~~~
When using CUDA, HIP, or SYCL, increase the box size per GPU rank to maximize occupancy::

    max_grid_size = 128
