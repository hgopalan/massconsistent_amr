.. _walkthrough:

Walkthrough of Features and Input Files
=======================================

This walkthrough provides a comprehensive, step-by-step guide to configuring and running different physical and numerical features of the Mass-Consistent AMR Wind Solver. The solver's behavior is controlled through an input deck (typically named ``inputs.i``) using AMReX ``ParmParse`` syntax (``key = value``).

The following six progressive walkthrough sections guide you from basic setup to advanced multi-physics atmospheric transport simulations.

---

Walkthrough Section 1: Baseline Wind Solver & Terrain Setup
-----------------------------------------------------------

This section covers the core configuration needed to run a baseline mass-consistent wind solve over topography. It demonstrates how to define the computational grid, ingest a terrain file, initialize a log-law profile, and write physical output.

**Core Feature Focus:**
* Ingesting 3-D terrain elevation point clouds.
* Setting up the computational grid spacing and vertical extents.
* Boundary-layer wind profile initialization (log-law).
* Exporting standard AMReX plotfiles and extracting terrain-following 2-D CSV slices.

**Reference Scenario:**
Located at ``test/mass_consistent_case1_gaussian_hill/``. This setup models a synthetic 75-meter Gaussian hill over a 500m × 500m domain.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: text

    # Terrain configuration
    terrain_file = terrain.csv          # Whitespace or comma-separated CSV with columns: X Y Z

    # Wind profile initialization
    init_mode    = loglaw               # Log-law wind profile initialization
    U_ref        = 15.0                 # Reference wind x-component [m/s]
    V_ref        = 0.0                  # Reference wind y-component [m/s]
    z_ref        = 10.0                 # Reference height above local terrain (AGL) [m]
    z0           = 0.03                 # Aerodynamic roughness length [m] (e.g., short grass/open terrain)

    # Computational grid spacing [m]
    dx           = 25.0                 # Horizontal grid spacing in x
    dy           = 25.0                 # Horizontal grid spacing in y
    dz           = 20.0                 # Vertical grid spacing in z

    # Domain vertical height [m]
    domain_height = 200.0               # Height of domain top above the maximum terrain peak

    # Mass-consistency parameters
    alpha_h      = 1.0                  # Lagrange horizontal penalty coefficient
    alpha_v      = 1.0                  # Lagrange vertical penalty coefficient

    # Solver configuration
    mlmg_verbose  = 0                   # MLMG solver verbosity (0=silent, 4=max)
    max_grid_size = 32                  # Max box size per dimension for AMReX domain decomposition

    # Extraction and output
    plot_file    = plt_case1_output     # Prefix for output AMReX plotfiles
    extract_agl  = 30.0                 # Extract wind velocity at 30 meters above ground level (AGL)
    extract_file = wind_extract.csv     # Filename for extracted 2-D CSV plane

**Key Parameter Breakdown & Physical Significance:**
*   ``terrain_file``: The solver parses this file to find the bounding box (``xmin``, ``xmax``, ``ymin``, ``ymax``) and interpolates the elevation data onto the grid using inverse-distance weighting (IDW).
*   ``dx, dy, dz``: Controls spatial resolution. A finer ``dz`` near the surface allows resolving high shear gradients, while ``dx`` and ``dy`` should match the horizontal resolution of your terrain data.
*   ``init_mode = loglaw``: Initializes the horizontal velocity components :math:`u(z)` and :math:`v(z)` at height :math:`z_{\text{agl}}` above local terrain using the von Kármán profile:
    
    .. math::
    
        U(z) = \frac{u_*}{\kappa} \ln\left(\frac{z_{\text{agl}} + z_0}{z_0}\right)
        
    where the friction velocity :math:`u_*` is computed automatically from the reference speed at :math:`z_{\text{ref}}`.
*   ``alpha_h`` & ``alpha_v``: Anisotropy coefficients that penalize adjustments in the horizontal and vertical directions. Setting :math:`\alpha_h / \alpha_v = 1.0` means wind is adjusted equally in all directions. Setting :math:`\alpha_h / \alpha_v \gg 1` forces the flow to go around obstacles rather than over them (highly stable atmosphere), while :math:`\alpha_h / \alpha_v \ll 1` forces the flow to go over topography.
*   ``extract_agl``: Samples the 3-D converged vector field at a constant height above the local terrain, mapping the terrain-following coordinates back to a 2-D horizontal plane for validation or plotting.

---

Walkthrough Section 2: Advanced Solver & Boundary Layer Dynamics
-----------------------------------------------------------------

This section focuses on advanced boundary layer physics and solver settings that are crucial for simulating steep terrain, thermal stability, and large-scale wind direction changes.

**Core Feature Focus:**
* Height-dependent Lagrange vertical penalty coefficients (:math:`\alpha_v`).
* Ekman spiral wind direction veer correction.
* Thermal stratification (buoyancy) effects.
* High-order numerical reconstruction methods (WENO3, WENO5).

**Reference Scenario:**
Located at ``test/mass_consistent_case3_mt_hood/`` (high-altitude complex alpine terrain) and ``regtest/wake_blending_adaptive/``.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: text

    terrain_file  = terrain.csv
    init_mode     = loglaw
    U_ref         = 10.0
    V_ref         = -5.0                # Resulting reference wind is from Southwest
    z_ref         = 10.0
    z0            = 0.05

    dx            = 50.0
    dy            = 50.0
    dz            = 30.0
    domain_height = 300.0

    # Advanced Anisotropy Settings
    use_height_dependent_alpha_v = true # Enable linear variation of alpha_v
    alpha_v_surface              = 1.5  # Surface alpha_v (higher penalty near ground)
    alpha_v_top                  = 0.5  # Upper alpha_v (flow can adjust vertically at high altitudes)
    alpha_h                      = 1.0

    # Ekman Spiral Veer Settings
    enable_ekman_veer            = true # Enable vertical wind direction rotation
    latitude                     = 45.3 # Latitude in degrees (Northern Hemisphere)
    ekman_veer_total             = 25.0 # Wind direction rotates by 25° from bottom to top
    ekman_veer_height            = 200.0 # Height scale [m] where most rotation occurs

    # Buoyancy Stratification Settings
    enable_buoyancy_stratification = true
    temperature_file             = temperature.csv  # Height (Z) vs Temperature (K) lookup
    temperature_reference        = 285.0            # T0 reference value [K]
    buoyancy_coefficient         = 1.2              # Scaling coefficient for buoyant acceleration
    buoyancy_method              = rhs              # "rhs" (add directly to Poisson RHS) or "velocity"

    # Solver & Numerical Methods
    deriv_method                 = weno5            # 5th-order WENO derivative reconstruction
    tol_rel                      = 1.e-9            # Tighter relative convergence tolerance
    mlmg_max_iter                = 300              # Maximum iterations for high-resolution cases
    mlmg_bottom_solver           = bicgstab         # BiCGStab bottom solver for highly anisotropic matrices

    plot_file                    = plt_advanced_bl
    extract_agl                  = 30.0
    extract_file                 = wind_extract.csv

**Key Parameter Breakdown & Physical Significance:**
*   ``use_height_dependent_alpha_v``: Allows :math:`\alpha_v` to vary linearly with height from ``alpha_v_surface`` to ``alpha_v_top``. This is physically realistic because vertical motion is strongly suppressed by surface boundaries but becomes freer at higher altitudes in the mixed layer.
*   ``enable_ekman_veer``: In the atmospheric boundary layer, the balance between Coriolis forces, pressure gradients, and friction causes the wind vector to rotate clockwise with height in the Northern Hemisphere (veering). Specifying ``latitude`` determines the Coriolis parameter, while ``ekman_veer_total`` and ``ekman_veer_height`` shape the vertical rotation profile.
*   ``enable_buoyancy_stratification``: Models gravitational restoring forces in stable atmospheres. Local density deviations are computed using temperature profiles from the ``temperature_file``. If ``buoyancy_method = rhs``, these density anomalies act directly as a source term in the Poisson equation to suppress or enhance vertical motions.
*   ``deriv_method = weno5``: Uses a 5th-order Weighted Essentially Non-Oscillatory scheme to compute spatial derivatives of the terrain slopes and velocities. WENO schemes are highly robust at resolving steep gradients (such as ridge crests) without introducing spurious numerical oscillations.

---

Walkthrough Section 3: Vegetation & Forest Canopy Modeling
-----------------------------------------------------------

This section details how to model the physical effects of vegetation and forest canopies on the wind profile. Instead of treating trees as solid grid obstacles, the solver models them as porous, drag-inducing zones.

**Core Feature Focus:**
* MacDonald displacement height parameterization.
* Shaw-Pereira canopy wind speed exponential decay.
* Boundary-layer profile blending above the canopy height.

**Reference Scenario:**
Located at ``regtest/canopy_forest/`` and ``regtest/canopy_exponential/``.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: text

    terrain_file       = terrain.csv
    init_mode          = loglaw
    U_ref              = 10.0
    V_ref              = 0.0
    z_ref              = 50.0             # Reference measurement height well above canopy
    z0                 = 0.05             # Ground-level aerodynamic roughness

    dx                 = 50.0
    dy                 = 50.0
    dz                 = 5.0              # Finer vertical grid spacing to resolve canopy levels

    domain_height      = 200.0

    # Forest Canopy Parameters
    enable_canopy            = true       # Enable vegetation canopy model
    canopy_height            = 20.0       # Forest canopy height [m]
    frontal_area_index       = 0.25       # Ratio of tree frontal area to ground area
    plan_area_index          = 0.20       # Ratio of crown plan area to ground area
    canopy_drag_coeff        = 0.2        # Porous drag coefficient for trees (Cd)
    use_exponential_profile  = true       # Use Shaw-Pereira exponential decay inside canopy
    canopy_attenuation       = 2.5        # Decay parameter alpha (typically 2.0 to 4.0)

    plot_file                = plt_canopy_forest
    extract_agl              = 10.0 30.0  # Extract wind inside (10m) and above (30m) canopy
    extract_file             = wind_extract.csv

**Key Parameter Breakdown & Physical Significance:**
*   ``canopy_height``: Height of the vegetation canopy :math:`h_c`. Below this height, the flow is subjected to aerodynamic canopy drag.
*   ``frontal_area_index`` (:math:`\lambda_f`) and ``plan_area_index`` (:math:`\lambda_p`): Characterize canopy density. The MacDonald model uses these indices to compute the effective displacement height :math:`d` and canopy-top roughness :math:`z_{0c}`:
    
    .. math::
    
        d / h_c = 1 + A^{-\lambda_p} (\lambda_p - 1)
        
    where :math:`A` is a geometry constant. Above the canopy (:math:`z > h_c`), the log-law wind profile recovers but is vertically displaced by :math:`d`.
*   ``use_exponential_profile = true``: Activates the Shaw & Pereira (1982) formulation. Within the canopy layer (:math:`z \le h_c`), the wind velocity decays exponentially from its value at the canopy top:
    
    .. math::
    
        U(z) = U(h_c) \exp\left( -\alpha \left( 1 - \frac{z}{h_c} \right) \right)
        
    where :math:`\alpha` is the ``canopy_attenuation`` parameter representing the dense foliage damping effect.
*   ``extract_agl``: Supports space-separated lists of heights (e.g., ``10.0 30.0``) to extract multiple heights in a single run. This automatically appends height suffixes to the output files (e.g., ``wind_extract_10m.csv`` and ``wind_extract_30m.csv``).

---

Walkthrough Section 4: Urban Building Obstacles & Wake Parameterization
------------------------------------------------------------------------

This section covers the configuration of the solver to handle solid buildings, mask their interiors, and parameterize the complex wake regions downwind.

**Core Feature Focus:**
* Ingesting 3-D rectangular building geometries (including rotated structures).
* Solid obstacle velocity masking (zero-velocity interior).
* Röckle (1990) building wake parameterization.
* Distance-weighted adaptive blending of overlapping wakes.

**Reference Scenario:**
Located at ``regtest/wake_single_building/``, ``regtest/building_array/``, and ``regtest/wake_blending_adaptive/``.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: text

    terrain_file   = terrain.csv
    init_mode      = loglaw
    U_ref          = 10.0
    V_ref          = 0.0
    z_ref          = 10.0
    z0             = 0.1

    dx             = 5.0              # Finer grid to resolve individual building shapes
    dy             = 5.0
    dz             = 5.0
    domain_height  = 150.0

    # Solid Obstacle & Building Configuration
    building_file  = buildings.csv    # CSV containing: xmin xmax ymin ymax zmin zmax [rotation]

    # Wake Model Parameters (Röckle 1990)
    enable_wake            = true     # Enable building wake modeling
    wake_c1                = 0.9      # Cavity zone length coefficient (Lr = c1 * H)
    wake_c2                = 0.3      # Wake velocity deficit coefficient
    wake_separation_length = 3.0      # Far-wake zone length coefficient (Lx = separation * H)
    enable_adaptive_wakes  = true     # Distance-weighted blending for overlapping building wakes

    plot_file      = plt_urban_wake
    extract_agl    = 10.0
    extract_file   = wind_wake_10m.csv

**Key Parameter Breakdown & Physical Significance:**
*   ``building_file``: Points to a CSV file describing 3D box obstacles. Cells whose centers fall inside a building are masked as solid boundaries, forcing the velocity to zero. The optional 7th column specifies a counter-clockwise rotation angle, allowing buildings to be oriented at any angle relative to the grid.
*   ``enable_wake = true``: Activates the empirical Röckle building wake model, which divides the downwind region behind each building into:
    *   *Cavity Zone (Recirculation)*: Extends downwind by :math:`L_r = c_1 \times H` (where :math:`H` is building height). Within this zone, the flow recirculates and streamwise velocity is strongly reduced or reversed.
    *   *Far-Wake Zone*: Extends further downwind to :math:`L_x = \text{wake\_separation\_length} \times H`. Streamwise velocities are reduced using the deficit coefficient ``wake_c2`` and gradually recover back to the background profile.
*   ``enable_adaptive_wakes = true``: When multiple buildings are present (e.g., an urban district), wake zones can overlap. Adaptive blending uses distance-weighted algorithms to compute an aggregate velocity deficit, avoiding abrupt step-changes or unphysical velocity drops at intersection boundaries.

---

Walkthrough Section 5: Synthetic Turbulence Generation & Mann Box Modeling
---------------------------------------------------------------------------

This section details how to generate realistic, time-varying synthetic turbulence fluctuations on top of the mean wind flow. It covers the setup of the anisotropic Mann spectral tensor model.

**Core Feature Focus:**
* Activating synthetic turbulence models.
* Mann Box anisotropic spectral tensor settings.
* Standard turbulence intensity and coherence models.
* Exporting turbulence time-series in binary BTS and VTK formats.

**Reference Scenario:**
Located at ``test/mass_consistent_case_mann_box/``.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: text

    terrain_file   = terrain.csv
    init_mode      = loglaw
    U_ref          = 12.0
    V_ref          = 0.0
    z_ref          = 20.0
    z0             = 0.05

    dx             = 25.0
    dy             = 25.0
    dz             = 20.0
    domain_height  = 250.0

    # General Turbulence Settings
    turbulence.enabled         = 1            # Enable synthetic turbulence generation
    turbulence.spectrum_model   = MannBox      # Use Mann Box spectral tensor model
    turbulence.intensity_model  = PowerLaw     # Spatial variation model for intensity
    turbulence.intensity_ref    = 0.12         # 12% turbulence intensity at reference height
    turbulence.z_intensity_ref  = 20.0         # Reference height for intensity [m]
    turbulence.intensity_exponent = 0.14       # Spatial shear decay rate for turbulence intensity
    turbulence.coherence_model  = Exponential  # Coherence spatial decay model
    turbulence.coherence_decay_vertical = 0.008
    turbulence.coherence_decay_lateral  = 0.006
    turbulence.random_seed      = 42           # Random seed for repeatable FFT generation

    # Mann Box Specific Parameters
    turbulence.mann_length_scale_u = 300.0     # Streamwise integral length scale [m]
    turbulence.mann_length_scale_v = 210.0     # Lateral integral length scale [m]
    turbulence.mann_length_scale_w = 120.0     # Vertical integral length scale [m]
    turbulence.mann_variance_u     = 1.0       # Reference energy fraction for u-component
    turbulence.mann_variance_v     = 0.80      # Energy fraction for lateral component
    turbulence.mann_variance_w     = 0.50      # Energy fraction for vertical component
    turbulence.mann_asymmetry_parameter = 1.0  # Anisotropy shape factor (typical range: 0.5 to 2.0)
    turbulence.mann_eddy_lifetime       = 0.1  # Eddy decay time scale parameter [s]
    turbulence.mann_terrain_adaptation_factor = 1.0 # Multiplier scaling terrain slope amplification

    # Output exports
    turbulence_export_format    = bts          # Binary TurbSim/OpenFAST compatibility format
    turbulence_output_file      = case_turb.bts

    plot_file      = plt_mann_box_output
    extract_agl    = 50.0                      # Extract at hub height of standard offshore turbine
    extract_file   = wind_extract_mann_box.csv

**Key Parameter Breakdown & Physical Significance:**
*   ``turbulence.spectrum_model = MannBox``: Replaces isotropic models (like Von Karman) with the Mann (1994) spectral tensor model. It models the spatial cross-correlation between different velocity components, making it the industry standard for wind turbine loading calculations.
*   ``mann_length_scale_u/v/w``: Represent the spatial sizes of the turbulent eddies in the streamwise, lateral, and vertical directions. In grassland or open flatlands, vertical eddies are squeezed by the surface boundary, leading to the anisotropic scaling ratio :math:`L_u : L_v : L_w \approx 1.0 : 0.7 : 0.4`.
*   ``mann_variance_u/v/w``: Control how total turbulent kinetic energy is partitioned among the three components. In the atmospheric boundary layer, streamwise fluctuations dominate: :math:`\sigma_u^2 > \sigma_v^2 > \sigma_w^2`.
*   ``mann_terrain_adaptation_factor``: Amplifies or dampens turbulence fluctuations depending on local terrain slopes. On windward slopes and crests, terrain-induced shear increases turbulence levels, while on leeward slopes, flow separation causes a decay in coherence.

---

Walkthrough Section 6: Pollutant Transport & Dispersion Modeling
-----------------------------------------------------------------

This final section integrates the mass-consistent wind solver with passive tracer dispersion using a coupled Gaussian Puff model. It simulates how pollutants drift with the wind, spread due to turbulent diffusion, and interact with buildings and trees.

**Core Feature Focus:**
* Continuous or instantaneous point source releases.
* Physical drift (advection) using the solved 3-D wind vector field.
* Ground reflection and image-source methods over topography.
* Wake-enhanced diffusivity behind buildings.
* Vegetation canopy dry deposition and scavenging.

**Reference Scenario:**
Located at ``regtest/puff_coupled_full/`` and ``regtest/puff_buildings/``.

**Annotated Input Deck (``inputs.i``):**

.. code-block:: text

    # Discretization bounds (custom concentration grid)
    xmin = 0.0
    xmax = 400.0
    ymin = 0.0
    ymax = 400.0
    zmin = 0.0
    zmax = 150.0

    # Grid Resolution
    dx   = 20.0
    dy   = 20.0
    dz   = 10.0

    # Main Solver Switch
    enable_puff                = true         # Enable the coupled Gaussian Puff solver

    # Source & Emission Properties
    source_x                   = 100.0        # Source x-coordinate [m] (upwind)
    source_y                   = 200.0        # Source y-coordinate [m]
    source_z                   = 35.0         # Source height above sea level [m]
    emission_rate              = 1.0          # Pollutant emission rate [mass units/s]
    emission_duration          = 60.0         # Duration of active release [s]

    # Atmospheric Diffusivity
    K_h                        = 1.0          # Base horizontal diffusivity [m²/s]
    K_v                        = 0.5          # Base vertical diffusivity [m²/s]
    sigma_y0                   = 1.0          # Initial lateral width of released puffs [m]
    sigma_z0                   = 1.0          # Initial vertical height of released puffs [m]

    # Terrain-Aware Dispersion
    terrain_file               = terrain.csv
    enable_terrain_reflection  = true         # Prevent puffs from penetrating the ground
    use_image_source           = true         # Use image-source reflection method

    # Building Interaction (Wake Diffusivity Enhancement)
    building_file              = buildings.csv
    enable_building_masking    = true         # Mask concentrations inside solid structures
    enable_wake_diffusivity    = true         # Enhance diffusion in building wake zones
    wake_enhancement_cavity    = 3.0          # 3x diffusivity increase inside the cavity zone
    wake_enhancement_far       = 1.5          # 1.5x diffusivity increase in the far-wake zone

    # Canopy Interaction (Scavenging & Deposition)
    enable_canopy_effects      = true         # Enable vegetation effects on puff dispersion
    canopy_height              = 15.0         # Forest canopy height [m]
    frontal_area_index         = 0.30
    canopy_enhancement_factor  = 3.0          # 3x vertical mixing enhancement inside forest
    canopy_sheltering_factor   = 0.7          # 30% reduction in horizontal transport inside canopy
    enable_canopy_deposition   = true         # Enable dry deposition/scavenging by leaves
    deposition_velocity        = 0.01         # Deposition velocity [m/s] (dry scavenging rate)

    # Time Stepping Settings
    dt_puff                    = 0.5          # Dispersion time step [s]
    n_steps_puff               = 120          # Run for 120 steps (total of 60s = 120 * 0.5s, matching emission_duration)
    output_freq_puff           = 24           # Output concentration fields every 24 steps (12s)
    puff_output                = puff_conc.csv # Prefix for 3D concentration CSV outputs

**Key Parameter Breakdown & Physical Significance:**
*   ``enable_puff = true``: Launches the Lagrangian puff dispersion solver, which tracks individual Gaussian puffs emitted over time. At each time-step ``dt_puff``, puff centers are advected using the local mass-consistent wind vectors :math:`(u, v, w)`.
*   ``K_h, K_v``: Coefficients representing atmospheric turbulent mixing. The puff standard deviations grow over time as :math:`\sigma_y(t) = \sqrt{\sigma_{y0}^2 + 2 K_h t}`, diluting the pollutant concentration.
*   ``enable_terrain_reflection`` & ``use_image_source``: Ensure conservation of mass by preventing pollutants from diffusing into the ground. It places virtual "image sources" beneath the terrain slope to reflect concentration fields back into the atmosphere.
*   ``enable_wake_diffusivity = true``: Building wakes are highly turbulent. Diffusivity is increased by ``wake_enhancement_cavity`` (e.g., 3.0) and ``wake_enhancement_far`` (e.g., 1.5) behind buildings, representing rapid mixing in building recirculations.
*   ``enable_canopy_deposition = true``: Models the removal of pollutants (e.g., ash, particles, or aerosols) by plant foliage. The concentration is depleted at a rate proportional to the foliage density and the ``deposition_velocity``, capturing forest scavenging effects.
