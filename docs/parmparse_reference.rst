ParmParse Input Reference
===========================

Comprehensive reference for all 449+ configuration parameters available through ParmParse input files for the massconsistent_amr wind solver. This document organizes parameters by functional category with descriptions of purpose and typical usage patterns.

.. contents::
   :depth: 2
   :local:

Overview
--------

The wind solver is configured through ParmParse input files (typically `inputs` or `inputs_*`). Parameters control:

- **Domain geometry**: Grid dimensions, spacing, simulation bounds
- **Initialization modes**: Constant profiles, log-law, Ekman spiral, Mann boxes, soundings
- **Physics models**: Turbulence, thermal effects, buoyancy, wind shear
- **Terrain & obstacles**: Terrain elevation, roughness, buildings, canopy
- **Turbine models**: Wake effects, yaw control, power extraction
- **Atmospheric forcing**: Pressure gradients, surface fluxes, geostrophic wind
- **Numerical methods**: Time stepping, solver configuration, AMR settings
- **I/O & diagnostics**: Output frequency, file formats, data extraction
- **Dispersion modeling**: Puff/LPDM particle transport, deposition

Quick Reference: Common Parameter Patterns
--------------------------------------------

**Boolean Parameters** (enable_*, use_*, apply_*, etc.):

::

    enable_adaptive_time_stepping = 1
    use_terrain_roughness = true
    apply_terrain_following_grid = 0


**Numeric Parameters** (floats, integers):

::

    dx = 10.0                    # Horizontal grid spacing [m]
    dz = 5.0                     # Vertical grid spacing [m]
    dt = 0.01                    # Time step [s]
    domain_height = 1000.0       # Domain top [m]


**Array Parameters**:

::

    sounding_files = sounding1.txt sounding2.txt sounding3.txt
    extract_k = 10 20 50 100
    synthetic_centers_x = 100.0 200.0 300.0


**String/Enum Parameters**:

::

    initialization_mode = log_law
    turbulence_model = mann_box
    velocity_profile_type = power_law


Domain Configuration
--------------------

**Grid Geometry**

- ``dx`` (Real): Horizontal grid spacing in x-direction [m]
- ``dy`` (Real): Horizontal grid spacing in y-direction [m]
- ``dz`` (Real): Vertical grid spacing [m]
- ``domain_height`` (Real): Height of domain top [m AGL]
- ``domain_width_x`` (Real): Total domain extent in x [m]
- ``domain_width_y`` (Real): Total domain extent in y [m]
- ``ncells_x`` (Integer): Number of grid cells in x-direction
- ``ncells_y`` (Integer): Number of grid cells in y-direction
- ``ncells_z`` (Integer): Number of grid cells in z-direction

**Domain Position & Orientation**

- ``domain_latitude`` (Real): Latitude of domain center [degrees N]
- ``domain_longitude`` (Real): Longitude of domain center [degrees E]
- ``domain_origin_x`` (Real): Origin x-coordinate [m]
- ``domain_origin_y`` (Real): Origin y-coordinate [m]
- ``domain_offset_x`` (Real): X-offset for terrain alignment [m]
- ``domain_offset_y`` (Real): Y-offset for terrain alignment [m]

Initialization Modes
--------------------

The solver supports multiple modes for initial velocity field setup.

**Log-Law Initialization**

- ``initialization_mode = log_law``
- ``aerodynamic_roughness`` (Real): Surface roughness length z0 [m]
- ``log_law_uref`` (Real): Reference velocity [m/s]
- ``log_law_uref_height`` (Real): Height of reference velocity [m]
- ``use_monin_obukhov_similarity`` (Bool): Apply M-O stability corrections

**Power-Law Initialization**

- ``initialization_mode = power_law``
- ``power_law_exponent`` (Real): Shear exponent (typically 0.1-0.3)
- ``power_law_uref`` (Real): Reference velocity [m/s]
- ``power_law_reference_height`` (Real): Height of reference [m]

**Ekman Spiral Initialization**

- ``initialization_mode = ekman_spiral``
- ``ekman_ug`` (Real): Geostrophic u-wind [m/s]
- ``ekman_vg`` (Real): Geostrophic v-wind [m/s]
- ``ekman_latitude`` (Real): Latitude for Coriolis [degrees N]
- ``ekman_veer_total`` (Real): Total wind veer [degrees]
- ``ekman_veer_height`` (Real): Height of veer maximum [m]

**Uniform Initialization**

- ``initialization_mode = uniform``
- ``U_wind`` (Real): Constant u-velocity [m/s]
- ``V_wind`` (Real): Constant v-velocity [m/s]
- ``W_wind`` (Real): Constant w-velocity [m/s]

**Mann Box Initialization**

- ``initialization_mode = mann_box``
- ``mann_box_file`` (String): Input Mann box file path
- ``mann_box_restart_time`` (Real): Time offset in Mann box [s]
- ``mann_box_scaling_factor`` (Real): Velocity scaling multiplier
- ``mann_box_zref`` (Real): Reference height for Mann box [m]

**Sounding-Based Initialization**

- ``initialization_mode = sounding``
- ``sounding_files`` (StringArray): List of sounding file paths
- ``sounding_x`` (RealArray): X-coordinates for sounding locations [m]
- ``sounding_y`` (RealArray): Y-coordinates for sounding locations [m]
- ``sounding_interpolation_type`` (String): Interpolation method
- ``sounding_extrapolation_above`` (Bool): Extrapolate above max height

**Raw Binary Field Initialization**

- ``initialization_mode = raws``
- ``raws_file`` (String): Path to raw velocity field binary file
- ``raws_format`` (String): Format specification (AMR-Wind standard)
- ``raws_velocity_scale`` (Real): Velocity scaling factor

Atmospheric Profile Models
---------------------------

**Log-Law Wind Profile**

- ``K_profile`` (String): "log_law" or similar
- ``K_reference_height`` (Real): Height where K value specified [m]
- ``K_h`` (Real): Horizontal eddy diffusivity [m²/s]
- ``K_v`` (Real): Vertical eddy diffusivity [m²/s]
- ``aerodynamic_roughness`` (Real): Surface roughness z0 [m]
- ``use_similarity_theory`` (Bool): Apply Monin-Obukhov

**Power-Law Profile**

- ``K_power_law_exponent`` (Real): Shear exponent
- ``K_power_law_uref`` (Real): Reference velocity [m/s]

**Boundary Layer Depth**

- ``bl_depth_param`` (Real): Boundary layer depth [m]
- ``bl_depth_diagnostic`` (Bool): Compute depth dynamically
- ``bl_transition_height`` (Real): Height of profile transition [m]

Terrain & Elevation Modeling
-----------------------------

**Terrain Input**

- ``use_terrain`` (Bool): Enable terrain elevation field
- ``terrain_file`` (String): Path to terrain DEM or point cloud
- ``terrain_format`` (String): File format (GeoTIFF, ASCII, NetCDF, etc.)
- ``terrain_scaling_factor`` (Real): Elevation multiplier
- ``terrain_height_reference`` (String): Vertical datum reference
- ``apply_terrain_following_grid`` (Bool): Use terrain-following mesh

**Terrain Processing**

- ``elevation_scaling_factor`` (Real): Scale applied to elevation
- ``elevation_height_scale`` (Real): Reference height for scaling
- ``smooth_terrain`` (Bool): Apply smoothing filter
- ``terrain_smoothness_iterations`` (Integer): Number of smoothing passes
- ``terrain_smoothness_radius`` (Real): Kernel radius for smoothing [m]

**Roughness Modeling**

- ``roughness_file`` (String): Path to roughness length map
- ``roughness_scaling_factor`` (Real): Roughness multiplier
- ``use_terrain_roughness`` (Bool): Apply spatial roughness variation
- ``building_roughness_factor`` (Real): Additional roughness from buildings
- ``canopy_roughness_factor`` (Real): Canopy roughness contribution [m]

Building & Structure Loading
-----------------------------

**Building Input & Geometry**

- ``building_file`` (String): Path to building database/shapefile
- ``building_format`` (String): File format (OSM, GIS shapefile, ASCII, etc.)
- ``building_height_source`` (String): Data source for heights
- ``building_height_scaling`` (Real): Height adjustment factor
- ``default_building_height`` (Real): Default height for missing data [m]
- ``default_building_porosity`` (Real): Default drag coefficient

**Building Aerodynamic Effects**

- ``use_buildings`` (Bool): Enable building-induced drag
- ``building_drag_model`` (String): Model type (exponential, directional, etc.)
- ``building_porosity_file`` (String): Spatial porosity map
- ``building_porosity_scaling`` (Real): Porosity adjustment factor
- ``building_height_uncertainty`` (Real): Height uncertainty for statistics

**Building Output**

- ``building_output_file`` (String): Path for building-resolved output
- ``extract_building_statistics`` (Bool): Compute building-scale stats

Building Geometry Formats
--------------------------

Buildings can be defined using rectangular, cylindrical, pitched-roof, and polygon geometries in CSV format.
The ``building_file`` parameter specifies the path to the buildings CSV file.

**Rectangular Buildings (Standard Format)**

Format: ``x1 x2 y1 y2 z1 z2 [rotation shape pitch_or_radius pitch_direction]``

- ``x1, x2``: X-coordinates of building extent [m]
- ``y1, y2``: Y-coordinates of building extent [m]
- ``z1, z2``: Z-coordinates (base to roof) [m]
- ``rotation`` (optional): Building orientation [degrees], default 0
- ``shape`` (optional): Building shape - 0 (rectangular), 1 (cylindrical), 2 (pitched roof), default 0
- ``pitch_or_radius`` (optional): Roof pitch [degrees] or cylinder radius [m], default 0
- ``pitch_direction`` (optional): Roof ridge direction [degrees], default 0

Example rectangular building:
::

    100 200 300 400 0 30

**Polygon Buildings**

Format: ``POLYGON: x1 y1 x2 y2 ... xn yn | z1 z2``

Arbitrary polygon footprints with internal vertices. Vertices should be provided in order (clockwise or counter-clockwise).

- ``x_i, y_i``: Polygon vertex coordinates [m], minimum 3 vertices
- ``z1, z2``: Z-coordinates (base to roof) [m]

Polygon buildings enable modeling of complex urban shapes:
- L-shaped, T-shaped, U-shaped building footprints
- Non-convex geometries
- Irregular building outlines

Example L-shaped building (two rectangles joined):
::

    POLYGON: 0 0 100 0 100 50 200 50 200 100 0 100 | 0 35

**Internal Courtyards and Void Zones**

Format: ``VOID: x1 y1 x2 y2 ... xn yn | z1 z2``

Void zones exclude interior regions from wake calculations, enabling modeling of:
- Internal courtyards and atriums
- Roof-level cavity spaces
- Complex multi-building compounds

Void zones are treated as exclusion zones and do not generate wakes. The superposition wake model automatically accounts for shadowing from adjacent buildings.

Example courtyard with void zone:
::

    POLYGON: 0 0 200 0 200 200 0 200 | 0 40
    VOID: 50 50 150 50 150 150 50 150 | 0 40

**Notes on Polygon Support**

- Polygon buildings inherit all three wake models (Röckle, Huber-Snyder, AERMOD PRIME) from rectangular buildings
- Wake calculations use the effective polygon dimensions and orientation relative to wind direction
- Wake zone transitions (near/cavity/far-wake) are computed for the polygon footprint
- Complex shapes are handled through geometric decomposition and superposition
- Polygon vertices are assumed to define a simple (non-self-intersecting) polygon


-----------------------------

**Canopy Input & Geometry**

- ``canopy_file`` (String): Path to canopy height/LAI data
- ``canopy_height`` (Real): Canopy top height [m AGL]
- ``canopy_profile_type`` (String): Profile shape (uniform, gaussian, exponential)
- ``use_canopy`` (Bool): Enable canopy drag and flow modification

**Canopy Aerodynamic Effects**

- ``canopy_drag_coeff`` (Real): Canopy drag coefficient (typically 0.1-0.2)
- ``canopy_attenuation`` (Real): Wind attenuation factor
- ``canopy_enhancement_factor`` (Real): Velocity enhancement above canopy
- ``canopy_sheltering_factor`` (Real): Sheltering effectiveness
- ``canopy_clumping_index`` (Real): Non-uniform spacing factor

**Canopy Radiation & Heat**

- ``canopy_albedo`` (Real): Surface albedo (typically 0.15-0.25)
- ``canopy_emissivity`` (Real): Thermal emissivity
- ``canopy_leaf_area_index`` (Real): LAI value

Building Wake Models
--------------------

**Core Wake Model Parameters**

- ``enable_wake`` (Bool): Enable building wake deficit calculations
- ``wake_model_type`` (String): Model selection (Röckle, Huber-Snyder, AERMOD PRIME, etc.)
- ``wake_c1`` (Real): Cavity zone length coefficient (L_r = c1 × H), default 0.9
- ``wake_c2`` (Real): Wake deficit intensity coefficient, default 0.3
- ``wake_separation_length`` (Real): Far-wake extent as multiple of building height H, default 3.0

**Wake Enhancement Flags**

All wake enhancements are backward-compatible and can be independently enabled/disabled:

- ``enable_oblique_scaling`` (Bool): Scale cavity length based on wind direction obliquity, default true
- ``enable_tall_building_correction`` (Bool): Apply correction for tall buildings (H > 100 m), default true
- ``enable_gaussian_profile`` (Bool): Use Gaussian lateral deficit profile instead of cosine, default false
- ``enable_upwind_recirculation`` (Bool): Include upwind recirculation zone, default true
- ``enable_reference_correction`` (Bool): Apply reference height stability correction, default false
- ``enable_corner_acceleration`` (Bool): Amplify deficit at building corners, default true
- ``enable_variance_correction`` (Bool): Scale turbulent kinetic energy recovery, default false
- ``enable_horseshoe_vortex`` (Bool): Model horseshoe vortex formation at building base, default true
- ``enable_extended_farwake`` (Bool): Extend far-wake zone to 15H instead of 5H, default true
- ``enable_yoshie_two_layer`` (Bool): Two-layer height-dependent deficit model (Yoshie et al., 2007), default true

**Yoshie Two-Layer Model Parameters**

- ``yoshie_decay_beta`` (Real): Exponential decay coefficient for above-roof zone, default 1.75, valid range [1.5, 2.0]
  
  The decay coefficient controls the rate of deficit reduction above building height. Physically, β ∈ [1.5, 2.0] corresponds to wind tunnel and field study observations. The model transitions smoothly at z = H between cavity zone (unchanged) and above-roof zone (exponentially decaying).

**Rodi Entrainment Model Parameters**

- ``enable_rodi_entrainment`` (Bool): Entrainment-based far-wake decay model (Rodi et al., 2003), default true

- ``rodi_ce_coefficient`` (Real): Entrainment coefficient, default 1.0, valid range [0.5, 1.5]
  
  Controls ambient fluid entrainment strength into the wake. Ce = 1.0 represents typical field observations. Higher values increase entrainment and accelerate deficit recovery in the 2–5H range.

**Lopes Pedestrian Wind Comfort Assessment Parameters**

- ``enable_lopes_comfort`` (Bool): Pedestrian wind comfort classification (Lopes et al., 2006), default true

- ``lopes_comfort_threshold`` (Real): Critical discomfort velocity [m/s], default 5.0, valid range 3.0–7.0
  
  Wind speed threshold above which conditions become uncomfortable. Default 5.0 m/s for general walking pedestrians; range depends on activity type (seated ≈3–4 m/s, standing ≈4–5 m/s, walking ≈5–7 m/s).

- ``lopes_assessment_height`` (Real): Evaluation height [m AGL], default 1.5, typical range [1.1, 2.0]
  
  Height at which comfort is assessed. Default 1.5 m corresponds to standing pedestrian head height; can be adjusted to 1.1 m (seated eye level) or 2.0 m (tall person standing).

**Lopes Pedestrian Wind Comfort Assessment Parameters**

- ``enable_lopes_comfort`` (Bool): Pedestrian wind comfort classification (Lopes et al., 2006), default true

- ``lopes_comfort_threshold`` (Real): Critical discomfort velocity [m/s], default 5.0, valid range 3.0–7.0
  
  Wind speed threshold above which conditions become uncomfortable. Default 5.0 m/s for general walking pedestrians; range depends on activity type (seated ≈3–4 m/s, standing ≈4–5 m/s, walking ≈5–7 m/s).

- ``lopes_assessment_height`` (Real): Evaluation height [m AGL], default 1.5, typical range [1.1, 2.0]
  
  Height at which comfort is assessed. Default 1.5 m corresponds to standing pedestrian head height; can be adjusted to 1.1 m (seated eye level) or 2.0 m (tall person standing).

- ``lopes_reference_frequency`` (Real): Reference discomfort frequency, default 0.02, range [0.0, 1.0]
  
  Used for diagnostic output and frequency scaling. Represents baseline discomfort fraction (0.02 = 2%, ~175 hours/year). Full implementation requires historical wind statistics; this parameter enables simplified comfort assessment estimates.

**Oikonomou Aspect-Ratio Correction Parameters**

- ``enable_oikonomou_aspect`` (Bool): Aspect-ratio dependent cavity zone correction (Oikonomou et al., 2011), default true

- ``oikonomou_beta_aspect`` (Real): Aspect-ratio correction coefficient, default 0.25, valid range [0.15, 0.35]
  
  Controls magnitude of cavity length adjustment based on building elongation (L/W ratio). β = 0.25 produces ~5–10% cavity length increase for moderately elongated buildings (L/W ≈ 2) and ~15–20% for highly elongated buildings (L/W ≈ 4). Square or near-square buildings (L/W ≤ 1) are unaffected. Correction factor clamped to [1.0, 1.5].

**Britter-Hanna Urban Canyon Attenuation Parameters**

- ``enable_britter_hanna_urban`` (Bool): Urban canyon wind speed attenuation model (Britter and Hanna, 2003), default true

- ``britter_hanna_alpha`` (Real): Urban canyon attenuation coefficient, default 0.15, valid range [0.10, 0.30]
  
  Controls rate of wind speed decay with urban building density (frontal area index φ_v). α = 0.15 produces ~7% reduction at moderate density (φ_v = 0.5) and ~14% at high density (φ_v = 1.0). Physically represents cumulative drag from distributed buildings and enhanced surface roughness in street canyons. Higher values increase attenuation in dense urban areas.
  
  The two-layer model separates cavity zone (z < H) and above-roof zone (z ≥ H), where the deficit decays exponentially:
  ΔU(z) = ΔU_cavity × exp(-β × (z - H) / H). The decay coefficient β controls the rate of wind speed recovery above building height.

Bridge & Obstacle Modeling
---------------------------

- ``bridge_file`` (String): Path to bridge geometry
- ``bridge_output_file`` (String): Output path for bridge forcing
- ``use_bridge_loading`` (Bool): Apply bridge aerodynamic loading

Wind Shear & Boundary Conditions
---------------------------------

**Reference Wind Vectors**

- ``U_ref`` (Real): Reference u-velocity [m/s]
- ``V_ref`` (Real): Reference v-velocity [m/s]
- ``U_wind`` (Real): Constant u-wind component [m/s]
- ``V_wind`` (Real): Constant v-wind component [m/s]
- ``W_wind`` (Real): Constant w-wind (typically 0) [m/s]

**Wind Direction & Veer**

- ``wind_direction`` (Real): Wind direction [degrees from North]
- ``wind_veer`` (Real): Total wind veer with height [degrees]
- ``wind_veer_height`` (Real): Height of maximum veer [m]

**Geostrophic Wind & Pressure Gradients**

- ``ageostrophic_fraction`` (Real): Non-geostrophic wind fraction
- ``ageostrophic_pressure_grad_x`` (Real): Pressure gradient x-component [Pa/m]
- ``ageostrophic_pressure_grad_y`` (Real): Pressure gradient y-component [Pa/m]
- ``ageostrophic_latitude`` (Real): Latitude for Coriolis [degrees N]
- ``ageostrophic_air_density`` (Real): Air density [kg/m³]

**Boundary Layer Configuration**

- ``enable_ageostrophic_balance`` (Bool): Enable geostrophic adjustment
- ``apply_geostrophic_wind`` (Bool): Force geostrophic balance
- ``apply_surface_forcing`` (Bool): Apply surface fluxes

Turbulence & Mixing
--------------------

**Turbulence Model Selection**

- ``turbulence_model`` (String): Model choice (mann_box, synthetic, sounding, etc.)
- ``use_turbulence_synthesis`` (Bool): Generate synthetic turbulence
- ``turbulence_intensity`` (Real): Turbulence intensity fraction (typically 0.05-0.15)

**Mann Box Model**

- ``mann_box_file`` (String): Input Mann turbulence box
- ``mann_box_restart_time`` (Real): Time offset in box [s]
- ``mann_box_scaling_factor`` (Real): Velocity scale factor
- ``mann_box_length_scale`` (Real): Correlation length [m]
- ``mann_box_reynolds_stress`` (RealArray): Six Reynolds stress components
- ``mann_box_zref`` (Real): Reference height [m]

**Synthetic Turbulence Generation**

- ``synthetic_turbulence_type`` (String): Method (gaussian, spectral, etc.)
- ``synthetic_peaks`` (RealArray): Spectral peak frequencies [Hz]
- ``synthetic_sigmas`` (RealArray): Velocity standard deviations [m/s]
- ``synthetic_centers_x`` (RealArray): Spectral peak x-wavenumbers
- ``synthetic_centers_y`` (RealArray): Spectral peak y-wavenumbers
- ``synthetic_anisotropy`` (Bool): Apply anisotropic spectrum

**Turbulence Anisotropy**

- ``anisotropy_source`` (String): Model for anisotropy
- ``anisotropy_ri_beta`` (Real): Richardson-number beta parameter
- ``anisotropy_ri_gamma`` (Real): Richardson-number gamma parameter
- ``anisotropy_fr_min`` (Real): Minimum Froude number
- ``anisotropy_slope_scale`` (Real): Slope length scale [m]
- ``anisotropy_decay_height`` (Real): Height of anisotropy decay [m]

**Eddy Viscosity & Diffusivity**

- ``K_h`` (Real): Horizontal eddy viscosity [m²/s]
- ``K_v`` (Real): Vertical eddy viscosity [m²/s]
- ``alpha_v`` (Real): Vertical diffusivity coefficient
- ``alpha_h`` (Real): Horizontal diffusivity coefficient
- ``alpha_v_surface`` (Real): Surface vertical diffusivity
- ``alpha_v_top`` (Real): Top domain vertical diffusivity
- ``use_constant_eddy_viscosity`` (Bool): Constant vs. dynamic viscosity

**Coherence & Decay**

- ``coherence_powerlaw_exponent`` (Real): Coherence decay exponent
- ``decay_constant`` (Real): Decay rate constant [1/s]
- ``decay_height_scale`` (Real): Height scale for decay [m]

Thermal & Buoyancy Effects
---------------------------

**Atmospheric Thermodynamics**

- ``enable_buoyancy`` (Bool): Include buoyancy forces
- ``buoyancy_method`` (String): Boussinesq or other formulation
- ``buoyancy_coefficient`` (Real): Buoyancy parameter
- ``buoyancy_timescale`` (Real): Thermal time scale [s]
- ``ambient_temp`` (Real): Background temperature [K]
- ``ambient_rh`` (Real): Relative humidity [%]

**Temperature Profile & Stratification**

- ``temperature_profile_type`` (String): Profile shape (neutral, stable, unstable)
- ``temperature_gradient`` (Real): Temperature lapse rate [K/m]
- ``temperature_inversion_height`` (Real): Inversion base [m]
- ``temperature_inversion_strength`` (Real): Inversion gradient [K/100m]
- ``stratification_parameter`` (Real): Richardson number or similar

**Diurnal Heating**

- ``enable_diurnal_cycle`` (Bool): Include day-night heating
- ``diurnal_period`` (Real): Period of cycle [s] (86400 for 1 day)
- ``diurnal_time_of_day`` (Real): Current time in cycle [s]
- ``diurnal_phase_hour`` (Real): Peak heating hour [0-24]
- ``diurnal_temperature_amplitude`` (Real): Temperature swing [K]

**Radiative Forcing and Cloud Effects**

- ``solar_radiation`` (Real): Reference solar radiation [W/m²] (default: 400)
- ``cloud_cover`` (Real): Fractional cloud cover [0-1] (default: 0.5). Controls cloud transmittance of direct and diffuse radiation.
- ``is_nighttime`` (Bool): Nighttime flag (no solar radiation)

**Cloud Transmittance Model**

When ``cloud_cover`` > 0, the solar radiation is attenuated by:

- Direct beam transmittance: :math:`\tau_d = 0.8 \times (1 - c)` where :math:`c` is cloud cover
- Diffuse transmittance: :math:`\tau_{df} = 0.2 + 0.6 \times c`

This means under partly cloudy conditions (c=0.5), direct radiation is reduced to 40% while diffuse increases, providing more realistic radiation partitioning.

**Sky View Factor & Solar Shading**

Unified computation of radiation transmission and shadowing using combined terrain+building elevation field. Buildings are treated as elevation features (no special casing required).

- ``enable_sky_view_factor`` (Bool): Compute sky view factor from local topography
- ``enable_solar_shading`` (Bool): Compute solar shading based on sun position (takes precedence over SVF for determining unshaded regions)
- ``latitude_degrees`` (Real): Location latitude [degrees, -90 to +90]
- ``longitude_degrees`` (Real): Location longitude [degrees, -180 to +180]
- ``day_of_year`` (Real): Day of year [1-365] for solar geometry
- ``hour_of_day`` (Real): Time of day [0-24 hours] for solar position
- ``max_horizon_distance`` (Real): Maximum distance for horizon ray-casting [m]

**Sky View Factor Methodology:**

SVF is computed from local terrain+building slope using :math:`\text{SVF} = (1 + \cos(\theta))/2` where :math:`\theta = \arctan(|\nabla h|)`.
This unified approach naturally captures terrain slopes, building wall effects, and urban canyon geometry.

**Solar Shading Methodology:**

For each grid point, a ray is cast toward the solar position (altitude and azimuth). If terrain/building features block the ray, the point is shaded.
Shading varies throughout the day as the sun position changes.

**Typical Configuration:**

.. code-block:: bash

    # Enable terrain-only SVF
    enable_sky_view_factor = true
    latitude_degrees = 40.0
    day_of_year = 172.0

    # With buildings and solar shading
    enable_sky_view_factor = true
    enable_solar_shading = true
    hour_of_day = 14.0

**Buoyant Wake Effects**

- ``buoyant_wake_destruction_coeff`` (Real): Decay rate for buoyant wakes


Turbine & Wind Energy
----------------------

**Turbine Input & Positioning**

- ``turbine_file`` (String): Path to turbine database
- ``turbine_coordinates`` (RealArray): X, Y positions of turbines [m]
- ``turbine_hub_height`` (RealArray): Hub height for each turbine [m]
- ``turbine_rotor_diameter`` (RealArray): Rotor diameter [m]
- ``turbine_base_height`` (Real): Default base height [m]

**Turbine Aerodynamic Modeling**

- ``use_turbine_wake_model`` (Bool): Enable wake effects
- ``wake_model_type`` (String): Model (Bastankhah, FLORIS, analytical, etc.)
- ``enable_bastankhah_deflection`` (Bool): Lateral wake deflection
- ``wake_deflection_angle`` (Real): Deflection angle [degrees]
- ``wake_core_radius`` (Real): Wake core size [m]
- ``wake_expansion_rate`` (Real): Lateral growth rate
- ``wake_decay_constant`` (Real): Centerline decay rate [1/m]

**Turbine Power & Thrust**

- ``power_curve_file`` (String): Path to power curve data
- ``thrust_coefficient`` (Real): Ct value (typically 0.8-1.0)
- ``thrust_coefficient_file`` (String): Spatially-varying Ct data
- ``power_scaling_factor`` (Real): Power multiplier
- ``use_power_extraction`` (Bool): Extract power from flow

**Turbine Yaw Control**

- ``enable_yaw_control`` (Bool): Allow turbine yaw
- ``yaw_angle`` (RealArray): Yaw angles [degrees]
- ``yaw_strategy`` (String): Control strategy
- ``yaw_rate_limit`` (Real): Maximum yaw rate [degrees/s]

**Turbine Output & Monitoring**

- ``turbine_output_file`` (String): Path for turbine statistics
- ``turbine_statistics_frequency`` (Integer): Averaging interval [steps]

Wind Wake Modeling
-------------------

**Wake Characteristics**

- ``wake_model_type`` (String): Model selection
- ``wake_center_x`` (RealArray): Wake center x-offset [m]
- ``wake_center_y`` (RealArray): Wake center y-offset [m]
- ``wake_deficit_profile`` (String): Profile type
- ``wake_core_radius`` (Real): Core radius [m]
- ``wake_expansion_rate`` (Real): Lateral expansion [1/m]
- ``wake_decay_constant`` (Real): Decay rate [1/m]
- ``wake_baseline_deficit`` (Real): Maximum velocity deficit [fraction]

**Secondary Flow Effects**

- ``enable_counter_rotating_pair`` (Bool): Include counter-rotating vortex pair
- ``counter_rotating_pair_strength`` (Real): Circulation intensity
- ``counter_rotating_pair_spacing`` (Real): Vortex core spacing [m]

**Wake-Wake Interaction**

- ``enable_wake_merging`` (Bool): Allow wake combination
- ``wake_merge_distance`` (Real): Merging threshold [m]
- ``enable_wake_superposition`` (Bool): Use linear superposition

Atmospheric Forcing & Forcing
------------------------------

**Surface Heat Fluxes**

- ``sensible_heat_flux`` (Real): Sensible heat flux [W/m²]
- ``latent_heat_flux`` (Real): Latent heat flux [W/m²]
- ``momentum_flux`` (Real): Surface stress [Pa]
- ``apply_surface_forcing`` (Bool): Enable surface boundary layer forcing

**Wind Farm Effects**

- ``farm_power_coefficient`` (Real): Farm efficiency factor
- ``farm_thrust_coefficient`` (Real): Farm thrust coefficient
- ``apply_farm_forcing`` (Bool): Apply wind farm body forces
- ``wind_farm_source_term_coefficient`` (Real): Source term scaling

**External Forcing Files**

- ``time_varying_wind_file`` (String): Path to time-varying wind input
- ``time_varying_temperature_file`` (String): Temperature forcing
- ``time_varying_pressure_file`` (String): Pressure forcing
- ``forcing_interpolation_type`` (String): Temporal interpolation method

Numerical Methods & Solver Configuration
-----------------------------------------

**Time Integration**

- ``dt`` (Real): Fixed time step [s]
- ``cfl_limit`` (Real): CFL number for stability
- ``enable_adaptive_time_stepping`` (Bool): Adapt dt based on CFL
- ``adaptive_dt_factor`` (Real): Factor for adaptive adjustment
- ``start_time`` (Real): Simulation start time [s]
- ``stop_time`` (Real): Simulation end time [s]

**Spatial Discretization**

- ``deriv_method`` (String): Derivative method (centered, upwind, etc.)
- ``use_fourth_order_derivatives`` (Bool): Fourth-order vs. second-order
- ``use_hyperbolic_damping`` (Bool): Apply hyperbolic damping

**Pressure Solver (MLMG)**

- ``pressure_solver_type`` (String): Solver method
- ``pressure_tolerance`` (Real): Convergence tolerance
- ``pressure_max_iterations`` (Integer): Maximum iterations
- ``use_multigrid`` (Bool): Enable multilevel solver
- ``multigrid_max_levels`` (Integer): Coarsening depth
- ``multigrid_pre_smooth`` (Integer): Pre-smoothing iterations
- ``multigrid_post_smooth`` (Integer): Post-smoothing iterations

**Advection & Interpolation**

- ``use_ppm`` (Bool): PPM (Piecewise Parabolic Method) advection
- ``use_muscl`` (Bool): MUSCL reconstruction
- ``advection_limiter`` (String): Limiter type (van_leer, minmod, etc.)

**Damping & Filtering**

- ``damping_coefficient`` (Real): General damping strength
- ``damping_coefficient_h`` (Real): Horizontal damping
- ``damping_coefficient_v`` (Real): Vertical damping
- ``damping_iterations`` (Integer): Number of damping passes
- ``use_sponge_layer`` (Bool): Apply sponge boundary layer
- ``sponge_layer_thickness`` (Real): Sponge thickness [m]

**Divergence Treatment**

- ``divergence_source_constant`` (Real): Divergence source term
- ``divergence_source_file`` (String): Spatially-varying divergence source

Adaptive Mesh Refinement (AMR)
------------------------------

**AMR Setup**

- ``amr_max_level`` (Integer): Maximum refinement level
- ``amr_ref_ratio`` (Integer): Refinement ratio between levels
- ``amr_blocking_factor`` (Integer): Box size multiple
- ``amr_error_buffer`` (Integer): Buffer around tagged cells

**Regridding & Tagging**

- ``amr_regrid_interval`` (IntegerArray): Regrid on these steps
- ``amr_tag_criteria`` (String): Criteria for tagging cells
- ``amr_tag_threshold`` (Real): Threshold for tagging
- ``amr_tag_variable`` (String): Variable for error estimation

Monin-Obukhov Similarity Theory
--------------------------------

**MOST Parameters**

- ``use_monin_obukhov_similarity`` (Bool): Enable MOST corrections
- ``obukhov_length`` (Real): Monin-Obukhov length [m]
- ``friction_velocity`` (Real): Friction velocity u* [m/s]
- ``surface_temperature`` (Real): Surface temperature [K]

**Stability Functions**

- ``stability_correction_type`` (String): MOST variant (Paulson, Beljaars, etc.)
- ``unstable_correction_factor`` (Real): Unstable regime factor
- ``stable_correction_factor`` (Real): Stable regime factor

Wall Functions & Boundary Conditions
-------------------------------------

**Wall Function Selection**

- ``use_wall_functions`` (Bool): Enable wall models
- ``wall_model_type`` (String): Model type (log-law, algebraic, etc.)

**Inflow Boundary Conditions**

- ``inflow_type`` (String): Type (uniform, profile, file-based, etc.)
- ``inflow_velocity`` (Real): Inflow magnitude [m/s]
- ``inflow_direction`` (Real): Inflow direction [degrees]

**Outflow Boundary Conditions**

- ``outflow_type`` (String): Condition type (zero-gradient, subsonic, etc.)

**Top Boundary Conditions**

- ``use_capping_lid`` (Bool): Apply pressure lid at top
- ``capping_lid_file`` (String): Lid pressure file
- ``capping_lid_height`` (Real): Lid height [m]
- ``damping_at_top`` (Bool): Damp motion near top

Puff/LPDM Dispersion Modeling
-----------------------------

**Puff Initialization**

- ``use_puff_model`` (Bool): Enable puff particle tracking
- ``puff_domain_height`` (Real): Top height for puffs [m]
- ``emission_rate`` (Real): Particle emission rate [particles/s]
- ``emission_duration`` (Real): Duration of emission [s]
- ``emissions_file`` (String): File with emission locations/times

**Puff Dynamics**

- ``puff_initial_radius`` (Real): Initial puff radius [m]
- ``puff_growth_rate`` (Real): Radius growth rate [m/s]
- ``dt_puff`` (Real): Puff time step [s]
- ``puff_tracking_interval`` (Integer): Steps between puff updates
- ``puff_integration_method`` (String): Integration scheme

**Deposition Processes**

- ``use_deposition`` (Bool): Enable particle deposition
- ``deposition_velocity`` (Real): Deposition settling [m/s]
- ``gravitational_settling`` (Bool): Include gravity
- ``brownian_diffusion`` (Bool): Include Brownian motion
- ``particle_diameter`` (Real): Particle size [µm]
- ``particle_density`` (Real): Particle density [kg/m³]

**Deposition Output**

- ``save_deposition_field`` (Bool): Output deposition grid
- ``deposition_output_interval`` (Integer): Output frequency

Data I/O & Diagnostics
-----------------------

**Plot File Output**

- ``amr_plot_file`` (String): Prefix for plotfiles
- ``plot_int`` (Integer): Plotfile interval (steps)
- ``plot_time`` (Real): Plotfile interval (simulated seconds)
- ``enable_3d_scalars`` (Bool): Include all 3D fields

**Checkpoint Files**

- ``amr_checkpoint_file`` (String): Checkpoint file prefix
- ``chk_int`` (Integer): Checkpoint interval
- ``write_initial_checkpoint`` (Bool): Save initial state

**Data Extraction**

- ``extract_k`` (IntegerArray): Vertical levels to extract
- ``extract_agl`` (RealArray): Heights AGL to extract [m]
- ``extract_locations_file`` (String): File with extraction points
- ``extract_frequency`` (Integer): Extraction interval

**Statistical Output**

- ``compute_statistics`` (Bool): Calculate statistics
- ``statistics_interval`` (Integer): Averaging window
- ``statistics_start_time`` (Real): When to start averaging [s]

**Logging & Diagnostics**

- ``verbose`` (Integer): Verbosity level (0=silent, 3=maximum)
- ``diagnostic_output`` (Bool): Enable diagnostic output
- ``profile_output_interval`` (Integer): Profile dump frequency
- ``time_averaging_enabled`` (Bool): Accumulate time averages

**File Formats**

- ``output_format`` (String): Format (NetCDF, HDF5, etc.)
- ``compression_level`` (Integer): Compression (0-9)
- ``precision`` (String): Precision (single, double)

Miscellaneous Parameters
------------------------

**General Settings**

- ``ncells_x``, ``ncells_y``, ``ncells_z`` (Integer): Grid resolution
- ``air_viscosity`` (Real): Kinematic viscosity [m²/s] (typically 1.5e-5)
- ``gravity`` (Real): Gravitational acceleration [m/s²] (typically 9.81)
- ``ach`` (Real): Air changes per hour (ventilation model)

**Constants & Coefficients**

- ``charnock_alpha`` (Real): Charnock coefficient (typically 0.014-0.018)
- ``cavity_recirculation_strength`` (Real): Urban canyon recirculation
- ``aermod_prime_cavity_factor`` (Real): AERMOD cavity factor

**Advanced Options**

- ``alpha_coefficients_file`` (String): Custom alpha coefficient file
- ``enable_heterogeneous_roughness`` (Bool): Spatial roughness variation
- ``apply_topographic_blocking`` (Bool): Blocking from steep slopes
- ``apply_terrain_following_grid`` (Bool): Terrain-following grid coordinates

Atmospheric Processes and Meteorological Effects
-------------------------------------------------

Advanced atmospheric modeling features for capturing realistic flow physics in complex terrain.

**Coriolis Effects**

- ``domain_lat_deg`` (Real): Geographic latitude in degrees (north positive) (default: 0.0)
- ``domain_lon_deg`` (Real): Geographic longitude in degrees (east positive) (default: 0.0)

When latitude is specified (non-zero), Coriolis parameter is automatically computed and applied to the wind field.

**Directional Bias Correction**

- ``enable_directional_bias_correction`` (Bool): Enable systematic wind direction/speed bias correction (default: false)
- ``bias_correction_file`` (String): Path to bias correction table (CSV format)

Bias correction table format: ``wind_direction, speed_factor, direction_offset``

**Thermal Circulation**

- ``enable_thermal_circulation`` (Bool): Enable slope and valley flows from thermal heating (default: false)
- ``surface_sensible_heat_flux`` (Real): Sensible heat flux from surface [W/m²] (default: 0.0)
- ``enable_diurnal_roughness`` (Bool): Time-varying roughness due to heating (default: false)
- ``roughness_diurnal_file`` (String): Hourly roughness length variations (CSV: hour, z0_value)

**Terrain Blocking and Flow Deflection**

- ``enable_terrain_blocking`` (Bool): Parameterize flow blockage by mountain barriers (default: false)
- ``froude_critical`` (Real): Critical Froude number for flow regime detection (typically 1.0)
- ``blockage_max_fraction`` (Real): Maximum blockage fraction in subcritical regimes (typical: 0.8)

**Roughness Transitions**

- ``enable_roughness_transitions`` (Bool): Smooth interpolation across land-use changes (default: true)
- ``roughness_transition_scale`` (Real): Smoothing length scale [m] for roughness transitions (default: 100.0)

**Valley and Gap Flow Channeling**

- ``enable_valley_channeling`` (Bool): Enhance horizontal wind in valleys (default: false)
- ``channeling_enhancement_factor`` (Real): Multiplier for valley wind speed amplification (typical: 1.2–1.5)
- ``gap_flow_detection`` (Bool): Automatically detect and enhance gap flows (default: false)

**Surface Layer Transitions**

- ``enable_surface_layer_transition`` (Bool): Smooth blending between log-law and mixed-layer profiles (default: true)
- ``transition_height`` (Real): Height above ground for profile blending [m] (default: 100.0)

**Porosity and Drag Models**

- ``enable_porosity_model`` (Bool): Treat vegetation/buildings as porous media (default: false)
- ``leaf_area_index`` (Real): LAI for canopy drag parameterization (typical: 1.0–8.0 for forests)
- ``canopy_height`` (Real): Vegetation/canopy height [m] (default: 0.0)

**Morphometric Analysis**

- ``compute_terrain_curvature`` (Bool): Calculate terrain curvature metrics for parameterization (default: false)
- ``morphometric_adaptation`` (Bool): Adapt local parameters based on morphometry (default: false)

Data Assimilation (Ensemble Kalman Filter)
--------------------------------------------

The optional Hybrid Ensemble Kalman Filter module enables wind field correction using sparse observations. All parameters are optional and the feature is disabled by default for backward compatibility.

**Core EnKF Configuration**

- ``enable_data_assimilation`` (Bool): Activate EnKF data assimilation (default: false)
- ``enkf_ensemble_size`` (Integer): Number of ensemble members (default: 10, typical: 5-20)
- ``enkf_localization_scale`` (Real): Covariance localization length scale [m] (default: 5000.0)

**Background Error Covariance**

Controls the uncertainty in initial conditions that drives ensemble perturbations:

- ``enkf_u_star_std`` (Real): Standard deviation of friction velocity [m/s] (default: 0.1)
- ``enkf_z0_std_factor`` (Real): Multiplicative std dev of roughness length (default: 2.0)
- ``enkf_wind_dir_std`` (Real): Standard deviation of wind direction [degrees] (default: 10.0)

**Observation Configuration**

Specifies where observations are loaded from:

- ``enkf_obs_file_station`` (String): Path to weather station observations (CSV format)
- ``enkf_obs_file_lidar`` (String): Path to LiDAR observations (NetCDF format)

CSV station format: ``x, y, z, u, v, w, error, source, component``

where component is: 0=u, 1=v, 2=w, 3=wind_speed

**Solver Settings**

Fine-tune the analysis and projection steps:

- ``enkf_poisson_tolerance`` (Real): Divergence correction tolerance (default: 1.0e-8)
- ``enkf_max_iterations`` (Integer): Maximum Poisson solver iterations (default: 100)

**Example Configuration**

Enable EnKF with default settings::

    enable_data_assimilation = true

Enable with custom ensemble and observations::

    enable_data_assimilation = true
    enkf_ensemble_size = 10
    enkf_localization_scale = 5000.0
    enkf_u_star_std = 0.1
    enkf_z0_std_factor = 2.0
    enkf_wind_dir_std = 10.0
    enkf_obs_file_station = "observations/stations.csv"
    enkf_obs_file_lidar = "observations/lidar.nc"
    enkf_poisson_tolerance = 1.0e-8

**Expected Performance**

- Accuracy: 25-40% improvement in wind field prediction
- Computational cost: Linear in ensemble size (N_e × T_solve)
- Total cycle time: 3-10 minutes for N_e=10 on GPU (with ~100 observations)
- Uncertainty quantification: Ensemble spread provides confidence intervals

**References**

See :ref:`mathematical_models` section "Data Assimilation" for mathematical formulation.

Infrastructure and Structural Assessment
-----------------------------------------

Specialized modules for evaluating infrastructure vulnerability to wind loading.

**Transmission Line Assessment**

- ``enable_transmission_line`` (Bool): Compute transmission line dynamic response (default: false)
- ``transmission_line_file`` (String): Tower locations and line specifications (CSV format)
- ``conductor_diameter`` (Real): Conductor diameter [m] (typical: 0.02–0.04)
- ``conductor_weight`` (Real): Conductor weight per unit length [kg/m] (typical: 0.5–2.0)
- ``span_length`` (Real): Distance between towers [m]
- ``initial_tension`` (Real): Initial conductor tension [N]

**Bridge Assessment**

- ``enable_bridge_assessment`` (Bool): Compute bridge wind loading and pedestrian comfort (default: false)
- ``bridge_width`` (Real): Bridge deck width [m]
- ``bridge_length`` (Real): Span length [m]
- ``bridge_height`` (Real): Height above reference [m]

**Pedestrian Wind Comfort**

- ``enable_pedestrian_comfort`` (Bool): Evaluate ISO 23601 wind comfort (default: false)
- ``comfort_output_height`` (Real): Evaluation height for pedestrian comfort [m] (typical: 1.8)

Advanced Data Processing and Validation
----------------------------------------

Tools for validating wind field solutions and computing diagnostics.

**Continuity and Flux Checking**

- ``enable_continuity_check`` (Bool): Verify mass conservation post-solve (default: true)
- ``divergence_tolerance`` (Real): Maximum acceptable divergence magnitude (default: 1.0e-12)
- ``output_divergence_field`` (Bool): Write divergence field to output (default: false)

- ``enable_flux_diagnostics`` (Bool): Compute mass, heat, and momentum fluxes (default: false)
- ``flux_output_plane`` (String): Plane for flux computation ("xy", "xz", "yz") (default: "xy")
- ``flux_plane_height`` (Real): Height/location of flux plane [m]

**Numerical Methods**

- ``deriv_method`` (String): Spatial derivative scheme: "central", "weno3", or "weno5" (default: "central")
- ``enable_high_order_derivatives`` (Bool): Use WENO-5 for all derivatives (default: false)

**Numerical Optimization**

- ``enable_parameter_optimization`` (Bool): Iteratively refine friction velocity and roughness (default: false)
- ``optimization_max_iterations`` (Integer): Maximum optimization iterations (default: 50)
- ``optimization_tolerance`` (Real): Convergence tolerance for parameter fit (default: 1.0e-4)
- ``observation_data_file`` (String): Sparse observations for parameter fitting

Example Input File
-------------------

A complete working input example::

    # Domain
    amr.n_cell = 128 128 64
    amr.max_level = 1
    geometry.prob_lo = 0.0 0.0 0.0
    geometry.prob_hi = 1280.0 1280.0 640.0
    
    # Initial conditions
    initialization_mode = log_law
    log_law_uref = 10.0
    log_law_uref_height = 90.0
    aerodynamic_roughness = 0.1
    
    # Turbulence
    turbulence_model = mann_box
    mann_box_file = mann_box.dat
    
    # Terrain
    use_terrain = true
    terrain_file = terrain.tif
    
    # Turbines
    turbine_file = turbines.csv
    use_turbine_wake_model = true
    wake_model_type = Bastankhah
    
    # I/O
    amr_plot_file = plt
    plot_int = 100
    amr_checkpoint_file = chk
    chk_int = 500
    
    # Time integration
    stop_time = 3600.0
    dt = 0.1
    cfl_limit = 0.9

See Also
--------

- :ref:`external_coupling` — Reactive transport coupling
- :ref:`python_api` — API documentation
- `AMReX Documentation <https://amrex-codes.github.io>`_
- `AMR-Wind Project <https://github.com/Exawind/amr-wind>`_

**Last Updated:** 2026-06-10

