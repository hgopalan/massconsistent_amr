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
* **init_mode** (String, Default: ``loglaw``): Wind initialization mode (``loglaw``, ``uniform``, ``raws``, ``surface_data``, ``powerlaw``, ``windfield``).
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
* **charnock_alpha** (Real, Default: ``0.011``): Charnock parameter used for wind-speed-dependent overwater roughness.
* **precipitation_file** (String, Default: none): Path to precipitation rate spatial CSV file.
* **precipitation_stability_threshold** (Real, Default: ``1.0``): Rainfall rate threshold [mm/hr] for eroding stable layers.
* **enable_directional_bias** (Boolean, Default: ``false``): Enable systematic direction and speed bias correction.
* **bias_direction_offset** (Real, Default: ``0.0``): Wind direction bias correction offset [degrees].
* **bias_speed_factor** (Real, Default: ``1.0``): Wind speed bias scale factor.
* **bias_periodic_enabled** (Boolean, Default: ``false``): Enable time-varying periodic bias offset.
* **bias_periodic_amplitude** (Real, Default: ``5.0``): Amplitude of periodic bias oscillations [degrees].
* **enable_simplified_richardson** (Boolean, Default: ``false``): Enable bulk Richardson method for conditional stability selection.
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

Step-by-Step Walkthrough Tutorials
----------------------------------

Walkthrough Section 1: Baseline Wind Solver & Terrain Setup
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This walkthrough demonstrates configuring a baseline mass-consistent wind solve over synthetic Gaussian topography.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: ini

    # Terrain configuration
    terrain_file = terrain.csv          # Point-cloud file

    # Wind profile initialization
    init_mode    = loglaw               # Log-law initialization
    U_ref        = 15.0                 # Reference wind x-component [m/s]
    V_ref        = 0.0                  # Reference wind y-component [m/s]
    z_ref        = 10.0                 # Reference height AGL [m]
    z0           = 0.03                 # Aerodynamic roughness length [m]

    # Computational grid spacing [m]
    dx           = 25.0
    dy           = 25.0
    dz           = 20.0

    # Domain vertical height [m]
    domain_height = 200.0               # Height above maximum terrain peak

    # Mass-consistency parameters
    alpha_h      = 1.0                  # Horizontal penalty coefficient
    alpha_v      = 1.0                  # Vertical penalty coefficient

    # Extraction and output
    plot_file    = plt_case1_output     # Output plotfile prefix
    extract_agl  = 30.0                 # Extract wind velocity at 30m AGL
    extract_file = wind_extract.csv

Run this baseline scenario located at ``test/test_case1_gaussian_hill/``::

    python3 test_case1.py

Walkthrough Section 2: Advanced Solver & Boundary Layer Dynamics
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This tutorial covers height-dependent anisotropy, Ekman spiral veer correction, buoyancy effects, and high-order derivative schemes (WENO-5).

**Annotated Input Deck (``inputs.i``):**

.. code-block:: ini

    terrain_file  = terrain.csv
    init_mode     = loglaw
    U_ref         = 10.0
    V_ref         = -5.0
    z_ref         = 10.0
    z0            = 0.05

    dx            = 50.0
    dy            = 50.0
    dz            = 30.0
    domain_height = 300.0

    # Height-Dependent Anisotropy
    use_height_dependent_alpha_v = true
    alpha_v_surface              = 1.5
    alpha_v_top                  = 0.5
    alpha_h                      = 1.0

    # Ekman Spiral Veer
    enable_ekman_veer            = true
    latitude                     = 45.3
    ekman_veer_total             = 25.0
    ekman_veer_height            = 200.0

    # Buoyancy Stratification
    enable_buoyancy_stratification = true
    temperature_file             = temperature.csv
    temperature_reference        = 285.0
    buoyancy_coefficient         = 1.2
    buoyancy_method              = rhs

    # Numerical Methods
    deriv_method                 = weno5
    tol_rel                      = 1.e-9
    mlmg_max_iter                = 300
    mlmg_bottom_solver           = bicgstab

    plot_file                    = plt_advanced_bl

Walkthrough Section 3: Vegetation & Forest Canopy Modeling
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This section configures forest canopies using MacDonald top-canopy adjustments and Shaw-Pereira exponential decay inside porous canopy layers.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: ini

    terrain_file       = terrain.csv
    init_mode          = loglaw
    U_ref              = 10.0
    V_ref              = 0.0
    z_ref              = 50.0
    z0                 = 0.05

    dx                 = 50.0
    dy                 = 50.0
    dz                 = 5.0

    domain_height      = 200.0

    # Forest Canopy Parameters
    enable_canopy            = true
    canopy_height            = 20.0
    frontal_area_index       = 0.25
    plan_area_index          = 0.20
    canopy_drag_coeff        = 0.2
    use_exponential_profile  = true
    canopy_attenuation       = 2.5

    plot_file                = plt_canopy_forest

Walkthrough Section 4: Atmospheric Stability and Slope Flows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This tutorial configures non-neutral atmospheric boundary layers and slope-driven thermal flows.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: ini

    # Enable stability corrections
    enable_stability_correction = true
    stability_length            = -150.0            # Unstable convective layer

    # Thermally-driven slope flows
    enable_katabatic_flow       = true              # Enable katabatic parameterization
    slope_flow_u_max            = 3.5               # Max slope flow velocity [m/s]
    slope_flow_z_max            = 15.0              # Height of peak velocity [m/s]

Walkthrough Section 5: Passive Gaussian Puff Dispersion
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This tutorial demonstrates coupling wind transport with Gaussian pollutant dispersion, first-order chemical decay, and deposition.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: ini

    enable_puff              = true
    source_x                 = 150.0
    source_y                 = 150.0
    source_z                 = 10.0
    emission_rate            = 1.0
    emission_duration        = 50.0

    K_h                      = 1.0
    K_v                      = 0.5
    sigma_y0                 = 1.0
    sigma_z0                 = 1.0

    enable_decay             = true
    decay_constant           = 0.001

    enable_puff_deposition   = true
    deposition_velocity      = 0.01

    # Adaptive (CFL-Limited) Time-Stepping
    enable_adaptive_time_stepping = true
    cfl_limit                = 0.5

Run the standalone puff solver::

    ./build/puff_solver regtest/puff_gaussian/inputs.i

Walkthrough Section 6: Synthetic Turbulence & BTS Generation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This walkthrough generates terrain-aware synthetic turbulence fluctuations and exports them in OpenFAST-compatible binary format.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: ini

    enable_synthetic_turbulence    = true
    turbulence_spectrum_model      = VonKarman
    turbulence_intensity_model     = PowerLaw
    turbulence_coherence_model     = Gaussian
    turbulence_intensity_ref       = 0.12
    turbulence_length_scale_u      = 300.0
    turbulence_length_scale_v      = 200.0
    turbulence_length_scale_w      = 120.0
    turbulence_random_seed         = 12345
    turbulence_export_format       = bts
    turbulence_output_file         = turbulence.bts

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
