.. _physics:

Advanced Physics Models
========================

This page documents the advanced physics parameterizations available in
``massconsistent_amr`` for realistic atmospheric boundary layer simulations.

.. contents:: Topics
   :local:
   :depth: 2

Atmospheric Stability
---------------------

Non-Neutral Boundary Layer (Monin-Obukhov Similarity Theory)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Physical Motivation**

The neutral log-law wind profile is only accurate when buoyancy effects are
negligible (near-neutral conditions). In reality, atmospheric boundary layers
exhibit different wind profiles depending on thermal stratification:

* **Stable conditions** (nighttime cooling): Suppressed turbulent mixing,
  steeper velocity gradients
* **Unstable conditions** (daytime heating): Enhanced turbulent mixing,
  gentler velocity gradients

**Implementation**

The solver implements Businger-Dyer (1971) stability corrections to the
log-law profile via Monin-Obukhov similarity theory.

The non-neutral wind profile is:

.. math::

   u(z) = \frac{u_*}{\kappa}\left[\ln\left(\frac{z+z_0}{z_0}\right) - \psi_m\left(\frac{z}{L}\right) + \psi_m\left(\frac{z_0}{L}\right)\right]

where:

* *u*\ :sub:`*` = friction velocity [m/s]
* κ = 0.41 (von Kármán constant)
* *z* = height above ground level [m]
* *z*\ :sub:`0` = aerodynamic roughness length [m]
* *L* = Obukhov length [m] (stability parameter)
* ψ\ :sub:`m` = dimensionless stability function

**Obukhov Length** *L*

The Obukhov length characterizes atmospheric stability:

* **L > 0**: Stable (surface cooling)
* **L < 0**: Unstable (surface heating)
* **|L| → ∞**: Neutral (standard log-law)

Typical values:

* Stable nocturnal boundary layer: *L* = 50–200 m
* Unstable daytime boundary layer: *L* = −50 to −300 m
* Neutral conditions: *L* > 10,000 m (effectively infinite)

**Stability Functions**

For **stable conditions** (*ζ* = *z*/*L* > 0):

.. math::

   \psi_m(\zeta) = -5\zeta

For **unstable conditions** (*ζ* < 0):

.. math::

   \psi_m(\zeta) = 2\ln\left(\frac{1+x}{2}\right) + \ln\left(\frac{1+x^2}{2}\right) - 2\arctan(x) + \frac{\pi}{2}

where:

.. math::

   x = (1 - 16\zeta)^{1/4}

**Usage**

Enable stability corrections in your input file::

    # Enable non-neutral stability corrections
    enable_stability_correction = true
    stability_length = 100.0    # Obukhov length L [m]
                                # Positive = stable, negative = unstable

**Examples**

Stable nocturnal boundary layer::

    enable_stability_correction = true
    stability_length = 100.0    # L = 100 m (stable)

Unstable daytime convective boundary layer::

    enable_stability_correction = true
    stability_length = -150.0   # L = -150 m (unstable)

**References**

* Businger, J.A., et al. (1971). Flux-profile relationships in the atmospheric
  surface layer. *Journal of Atmospheric Sciences*, 28(2), 181–189.
* Dyer, A.J. (1974). A review of flux-profile relationships. *Boundary-Layer
  Meteorology*, 7(3), 363–372.
* Paulson, C.A. (1970). The mathematical representation of wind speed and
  temperature profiles in the unstable atmospheric surface layer. *Journal of
  Applied Meteorology*, 9(6), 857–861.

**Regression Tests**

* ``regtest/stability_stable/`` — stable atmospheric conditions (*L* = 100 m)
* ``regtest/stability_unstable/`` — unstable conditions (*L* = −150 m)

Pasquill-Gifford Stability Classes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Physical Motivation**

The Pasquill-Gifford (PG) stability classification system provides a simple,
widely-used method for categorizing atmospheric stability based on readily
available meteorological observations. Developed in the 1960s, it remains
essential for:

* Regulatory air quality modeling
* Quick stability assessments without detailed surface flux data
* Compatibility with legacy dispersion models (AERMOD, CALPUFF)
* Linking to standard dispersion parameters (σ_y, σ_z)

**Implementation**

The solver implements the classic Turner (1970) lookup table that maps wind
speed and solar radiation (daytime) or cloud cover (nighttime) to discrete
stability classes A-F:

* **Class A**: Very unstable (strong daytime heating)
* **Class B**: Moderately unstable
* **Class C**: Slightly unstable
* **Class D**: Neutral (overcast or moderate wind)
* **Class E**: Slightly stable
* **Class F**: Moderately stable (strong nighttime cooling)

**Daytime Classification** (based on solar radiation):

* Strong insolation (> 700 W/m²): Classes A-D depending on wind speed
* Moderate insolation (350-700 W/m²): Classes A-D
* Slight insolation (< 350 W/m²): Classes B-D

**Nighttime Classification** (based on cloud cover):

* Clear sky (< 40% cover): Classes F (low wind) to D (high wind)
* Partly cloudy (40-80%): Classes E to D
* Overcast (> 80%): Class D (neutral)

**Mapping to Obukhov Length**

PG classes are mapped to approximate Obukhov length values:

.. list-table::
   :header-rows: 1
   :widths: 20 40 40

   * - Class
     - Description
     - L (m)
   * - A
     - Very unstable
     - −50
   * - B
     - Moderately unstable
     - −100
   * - C
     - Slightly unstable
     - −200
   * - D
     - Neutral
     - 10,000
   * - E
     - Slightly stable
     - 100
   * - F
     - Moderately stable
     - 50

**Usage**

Enable PG stability classification in your input file::

    # Enable Pasquill-Gifford classification
    enable_pg_stability = true
    solar_radiation = 500.0    # W/m² (daytime)
    is_nighttime = false       # Daytime conditions
    
    # Alternatively for nighttime:
    enable_pg_stability = true
    is_nighttime = true
    cloud_cover = 0.3          # Clear sky (0-1)

**References**

* Pasquill, F. (1961). The estimation of dispersion of windborne material.
  *Meteorological Magazine*, 90, 33–49.
* Gifford, F.A. (1961). Use of routine meteorological observations for
  estimating atmospheric dispersion. *Nuclear Safety*, 2(4), 47–51.
* Turner, D.B. (1970). Workbook of atmospheric dispersion estimates.
  US EPA, AP-26.

**Regression Tests**

* ``regtest/pasquill_gifford/`` — PG classification with moderate insolation

Thermal Stratification and Buoyancy
------------------------------------

Boussinesq Approximation for Buoyancy-Driven Flow
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Physical Motivation**

Temperature variations in the atmosphere create buoyancy forces that drive
vertical motion. Warm air rises (positive buoyancy), cold air sinks (negative
buoyancy). This is critical for:

* Daytime convective boundary layer development
* Nighttime drainage flows and inversions
* Slope flows in mountainous terrain
* Fire plume dynamics

**Implementation**

The solver implements the **Boussinesq approximation** for buoyancy effects on
vertical velocity. The buoyancy-induced vertical velocity is:

.. math::

   w_{buoyancy} = \frac{g(T - T_0)}{T_0} \Delta t

where:

* *g* = 9.81 m/s² (gravitational acceleration)
* *T* = local temperature [K]
* *T*\ :sub:`0` = reference temperature [K]
* Δ*t* = characteristic time scale [s]

**Reduced Gravity**

The buoyancy force can be expressed as reduced gravity:

.. math::

   g' = g\frac{T - T_0}{T_0}

* *g'* > 0: Warm air, upward buoyancy force
* *g'* < 0: Cold air, downward buoyancy force
* *g'* = 0: Neutral buoyancy

**Brunt-Väisälä Frequency**

Atmospheric stability from temperature stratification is quantified by the
Brunt-Väisälä frequency squared:

.. math::

   N^2 = \frac{g}{T_0}\left(\frac{dT}{dz} + \Gamma_d\right)

where Γ\ :sub:`d` = 0.0098 K/m is the dry adiabatic lapse rate.

* *N*\ :sup:`2` > 0: **Stable** (oscillatory motion, gravity waves)
* *N*\ :sup:`2` < 0: **Unstable** (convective overturning)
* *N*\ :sup:`2` ≈ 0: **Neutral** (adiabatic)

**Usage**

Specify temperature profile for buoyancy::

    # Enable buoyancy effects
    enable_buoyancy = true
    temperature_file = temperature_profile.csv
    reference_temperature = 288.0  # T0 [K]
    buoyancy_coefficient = 1.0     # Tuning parameter

**Temperature Profile Format**

CSV or whitespace-separated file with columns ``z [m]  T [K]``::

    # Height [m]  Temperature [K]
    0.0           285.0
    100.0         287.0
    500.0         290.0
    1000.0        295.0

**Regression Tests**

* ``regtest/buoyancy_stratification/`` — thermal stratification with buoyancy
* ``regtest/combined_thermal_terrain/`` — combined buoyancy and kinematic BC

Kinematic Terrain-Following Boundary Condition
-----------------------------------------------

**Physical Motivation**

At the terrain surface, the flow must satisfy a **no-penetration condition**:
the normal component of velocity at the surface must equal zero. For sloping
terrain, this requires the vertical velocity to match the terrain slope.

**Mathematical Formulation**

The kinematic boundary condition at the terrain surface is:

.. math::

   w = \mathbf{u} \cdot \nabla h = u\frac{\partial h}{\partial x} + v\frac{\partial h}{\partial y}

where:

* *w* = vertical velocity [m/s]
* *u*, *v* = horizontal velocity components [m/s]
* *h*\ (*x*, *y*) = terrain elevation function [m]
* ∇*h* = terrain gradient vector

This ensures that flow follows the terrain contours without passing through
the surface.

**Implementation**

The solver computes terrain gradients using central differences (or WENO
schemes) and applies the kinematic condition at cells adjacent to the terrain
surface:

.. code-block:: c++

   w_surface = u * dh_dx + v * dh_dy

where:

* ``dh_dx`` = ∂\ *h*/∂\ *x* (terrain slope in x-direction)
* ``dh_dy`` = ∂\ *h*/∂\ *y* (terrain slope in y-direction)

**Relaxation Parameter**

A relaxation factor (0 < *α* ≤ 1) can be applied for numerical stability:

.. math::

   w = \alpha\left(u\frac{\partial h}{\partial x} + v\frac{\partial h}{\partial y}\right)

* *α* = 1.0: Strict kinematic condition (recommended)
* *α* < 1.0: Relaxed condition (for steep terrain)

**Usage**

Enable kinematic terrain BC::

    # Enable kinematic terrain-following boundary condition
    enable_kinematic_bc = true
    kinematic_bc_relaxation = 1.0  # Relaxation factor (default: 1.0)

**Benefits**

* Improved representation of terrain-following flow
* Reduces spurious vertical velocities near steep slopes
* More accurate flow separation and recirculation zones

**Regression Tests**

* ``regtest/terrain_kinematic_bc/`` — kinematic BC on Gaussian hill
* ``regtest/combined_thermal_terrain/`` — combined with buoyancy effects

Ekman Spiral and Wind Veer
---------------------------

**Physical Motivation**

In the atmospheric boundary layer, the balance between Coriolis force, pressure
gradient, and surface friction causes wind direction to **veer** (rotate) with
height. This is known as the Ekman spiral.

* **Northern Hemisphere**: Wind veers clockwise with height
* **Southern Hemisphere**: Wind veers counter-clockwise with height

Typical veer: 10–30° from surface to boundary layer top.

**Implementation**

The solver applies an exponential veer profile:

.. math::

   \theta(z) = \theta_{total}\left[1 - \exp\left(-\frac{z}{h_{veer}}\right)\right]

where:

* θ(*z*) = wind direction change at height *z* [degrees or radians]
* θ\ :sub:`total` = total veer from surface to top [degrees]
* *h*\ :sub:`veer` = characteristic veer height scale [m]

**Wind Rotation**

The horizontal wind components are rotated:

.. math::

   \begin{aligned}
   u_{veer} &= u\cos\theta - v\sin\theta \\
   v_{veer} &= u\sin\theta + v\cos\theta
   \end{aligned}

**Coriolis Parameter**

The Coriolis parameter is:

.. math::

   f = 2\Omega\sin(\phi)

where:

* Ω = 7.2921 × 10⁻⁵ rad/s (Earth's rotation rate)
* φ = latitude [radians]

**Usage**

Enable Ekman veer correction::

    # Enable Ekman spiral wind veer
    enable_ekman_veer = true
    latitude = 45.0              # Latitude [degrees], positive=North
    ekman_veer_total = 20.0      # Total veer [degrees] from surface to top
    ekman_veer_height = 200.0    # Veer height scale [m]

**Typical Values**

* Mid-latitudes (30–60°): Total veer = 15–30°
* Polar regions (> 60°): Total veer = 30–40°
* Tropics (< 30°): Total veer = 5–15°
* Veer height scale: 100–200 m (boundary layer depth scale)

**Regression Tests**

* ``regtest/ekman_veer/`` — Ekman spiral veer correction

Elevation-Dependent Wind Scaling
---------------------------------

**Physical Motivation**

In complex mountainous terrain, wind speed often varies with terrain elevation
due to:

* **Valley channeling**: Wind speed decreases at higher elevations (sheltering)
* **Ridge acceleration**: Wind speed increases on exposed ridges
* **Mountain-valley circulation**: Thermally-driven flows

**Implementation**

The reference wind speed is scaled based on terrain elevation:

.. math::

   U_{scaled} = U_{ref}\exp\left(-\alpha\frac{\Delta z}{H_{scale}}\right)

where:

* *U*\ :sub:`ref` = reference wind speed [m/s]
* Δ*z* = elevation above minimum terrain [m]
* *α* = elevation scaling factor (dimensionless)
* *H*\ :sub:`scale` = characteristic height scale [m]

**Scaling Factor Interpretation**

* **α > 0**: Wind decreases with elevation (valley effect)
* **α < 0**: Wind increases with elevation (ridge effect)
* **α = 0**: No elevation dependence

**Usage**

Enable elevation-dependent wind scaling::

    # Enable elevation-dependent wind speed scaling
    enable_elevation_scaling = true
    elevation_scaling_factor = 0.3     # Scaling strength
    elevation_height_scale = 1000.0    # Characteristic height [m]

**Example**

For a mountain valley with *α* = 0.3 and *H*\ :sub:`scale` = 1000 m:

* At Δ*z* = 0 m (valley floor): *U* = *U*\ :sub:`ref`
* At Δ*z* = 500 m: *U* = 0.86 *U*\ :sub:`ref` (14% reduction)
* At Δ*z* = 1000 m: *U* = 0.74 *U*\ :sub:`ref` (26% reduction)

**Regression Tests**

* ``regtest/elevation_scaling/`` — elevation-dependent wind scaling

Building Porosity Model
------------------------

**Physical Motivation**

Not all structures are solid obstacles. Many features allow partial flow:

* Forest canopies and tree stands
* Fences and barriers
* Lattice structures and screens
* Vegetated noise barriers
* Porous windbreaks

**Implementation**

The porosity model applies drag to velocity based on a porosity parameter:

.. math::

   \frac{du}{dt} = -\frac{1}{2}(1-\beta)\frac{C_d}{\Delta x}|u|u

where:

* β = porosity (0 = solid, 1 = fully open)
* *C*\ :sub:`d` = drag coefficient (typical: 0.2)
* Δ*x* = cell size (frontal area characteristic length)

**Porosity Parameter**

* **β = 0.0**: Solid building (zero velocity)
* **β = 0.3**: Moderately porous (dense trees, hedges)
* **β = 0.7**: Highly porous (fences, lattices)
* **β = 1.0**: Fully open (no obstruction)

**Drag Application**

For steady-state simulations, drag is applied as velocity reduction:

.. math::

   u_{new} = u_{old}\exp(-\alpha\Delta t)

where *α* is the drag factor based on porosity and drag coefficient.

**Porous Building File Format**

CSV or whitespace-separated file with columns:

``xmin xmax ymin ymax zmin zmax porosity [rotation_degrees]``

Example::

    # xmin xmax ymin ymax zmin zmax porosity [rotation]
    100 120 100 120 0 10 0.3 0     # 30% porous tree stand
    200 220 200 220 0 15 0.7 45    # 70% porous fence (rotated 45°)

**Usage**

Enable building porosity model::

    # Enable building porosity
    enable_building_porosity = true
    building_porosity_file = porous_buildings.csv
    porosity_drag_coefficient = 0.2

**References**

* Santiago, J.L., et al. (2007). CFD simulation of airflow over a regular
  array of cubes. Part I: Three-dimensional simulation of the flow and
  validation with wind-tunnel measurements. *Boundary-Layer Meteorology*,
  122(3), 609–634.
* Kanda, M., et al. (2013). A new aerodynamic parametrization for real urban
  surfaces. *Boundary-Layer Meteorology*, 148(2), 357–377.

**Regression Tests**

* ``regtest/porous_buildings/`` — building porosity model

Wall Functions
--------------

**Physical Motivation**

Traditional no-slip boundary conditions (zero velocity at walls) require very
fine near-wall grid resolution to resolve the viscous sublayer. Wall functions
provide an alternative by using empirical wall laws (log-law) to bridge between
the wall and the first grid cell.

Benefits:

* Coarser grids near boundaries
* Reduced computational cost
* Improved accuracy for boundary layer flows

**Log-Law Wall Function**

For horizontal surfaces, the log-law wall function is:

.. math::

   u_{parallel} = \frac{u_*}{\kappa}\ln\left(\frac{z + z_0}{z_0}\right)

where:

* *u*\ :sub:`parallel` = velocity parallel to wall [m/s]
* *u*\ :sub:`*` = friction velocity (computed iteratively) [m/s]
* κ = 0.41 (von Kármán constant)
* *z* = distance from wall [m]
* *z*\ :sub:`0` = wall roughness length [m]

**Friction Velocity Calculation**

The friction velocity *u*\ :sub:`*` is computed by iteratively solving:

.. math::

   u_{parallel} = \frac{u_*}{\kappa}\ln\left(\frac{z + z_0}{z_0}\right)

given *u*\ :sub:`parallel` at the first cell center.

**Blending Function**

Near the wall, the wall function is blended with the computed velocity:

.. math::

   u_{final} = \lambda u_{wall} + (1-\lambda)u_{computed}

where *λ*(*z*) is a blending function (0 at wall, 1 at blend height).

**Stability Corrections**

Wall functions can include Monin-Obukhov stability corrections:

.. math::

   u = \frac{u_*}{\kappa}\left[\ln\left(\frac{z+z_0}{z_0}\right) - \psi_m\left(\frac{z}{L}\right)\right]

**Adaptive Activation**

Wall functions are automatically activated/deactivated based on grid resolution:

* Activate when: *z*\ :sub:`0` < Δ*z* < 30*z*\ :sub:`0` (in logarithmic layer)
* Deactivate when: Δ*z* too small (viscous sublayer) or too large (outer layer)

**Usage**

Enable wall functions::

    # Enable wall functions
    enable_wall_functions = true
    enable_terrain_wall_function = true
    enable_flat_surface_wall_function = false
    enable_building_wall_function = false
    
    # Wall function parameters
    wall_function_z0_building = 0.001       # Building wall roughness [m]
    wall_function_z0_flat = 0.01            # Flat surface roughness [m]
    wall_function_blend_height = 2.0        # Blending height [cells]
    
    # Optional: stability corrections
    wall_function_enable_stability = true
    wall_function_stability_length = 100.0  # Obukhov length [m]
    
    # Optional: adaptive activation
    wall_function_enable_adaptive = true
    wall_function_adaptive_threshold = 30.0      # Max dz/z0 ratio
    wall_function_adaptive_min_cells = 3.0       # Min cells in log layer

**Regression Tests**

* ``regtest/wall_function_flat/`` — basic wall function on flat terrain
* ``regtest/wall_function_stable/`` — wall function with stable stability
* ``regtest/wall_function_unstable/`` — wall function with unstable stability
* ``regtest/wall_function_adaptive/`` — adaptive activation based on grid

**References**

* Blocken, B., et al. (2007). CFD simulation of the atmospheric boundary layer:
  wall function problems. *Atmospheric Environment*, 41(2), 238–252.
* Richards, P.J., & Hoxey, R.P. (1993). Appropriate boundary conditions for
  computational wind engineering models using the k-ε turbulence model.
  *Journal of Wind Engineering and Industrial Aerodynamics*, 46, 145–153.

Height-Dependent Anisotropy
----------------------------

**Physical Motivation**

In the mass-consistency formulation, anisotropy coefficients (*α*\ :sub:`h`,
*α*\ :sub:`v`) control the relative weight of horizontal versus vertical wind
adjustments. Near the surface, it's often desirable to:

* Preserve horizontal wind profiles (small *α*\ :sub:`v` → strong vertical
  adjustment penalty)
* Allow more horizontal adjustment aloft (larger *α*\ :sub:`v` → weaker vertical
  penalty)

**Implementation**

The vertical anisotropy coefficient varies linearly with height:

.. math::

   \alpha_v(z) = \alpha_{v,surface} + \left(\alpha_{v,top} - \alpha_{v,surface}\right)\frac{z - z_{min}}{z_{max} - z_{min}}

where:

* *α*\ :sub:`v,surface` = coefficient at domain bottom
* *α*\ :sub:`v,top` = coefficient at domain top
* *z*\ :sub:`min`, *z*\ :sub:`max` = vertical domain bounds

**Typical Configuration**

For terrain-following flow preservation::

    use_height_dependent_alpha_v = true
    alpha_h = 1.0
    alpha_v_surface = 0.5    # Strong vertical adjustment near surface
    alpha_v_top = 2.0        # Weaker vertical adjustment aloft

**Effect**

* **Near surface**: Small *α*\ :sub:`v` → horizontal winds preferentially
  adjusted → preserves log-law profile shape
* **Aloft**: Large *α*\ :sub:`v` → vertical winds preferentially adjusted →
  allows horizontal flow around obstacles

**Regression Tests**

* ``regtest/alphav_height/`` — height-dependent vertical anisotropy

Terrain-Adaptive Alpha Coefficients
------------------------------------

**Physical Motivation**

Optimal anisotropy coefficients (*α*\ :sub:`h`, *α*\ :sub:`v`) for mass-consistent
wind adjustment vary with terrain characteristics. Different terrain types require
different adjustment strategies:

* **Flat terrain**: Isotropic adjustment (*α*\ :sub:`h` ≈ *α*\ :sub:`v`) allows
  equal horizontal and vertical wind corrections
* **Steep slopes**: Preserve terrain-following flow with *α*\ :sub:`v` ≪ *α*\ :sub:`h`
  (minimal vertical adjustment)
* **Valley bottoms**: Allow more vertical adjustment (larger *α*\ :sub:`v`) to
  accommodate channeled flow
* **Ridge tops**: Constrain vertical motion (smaller *α*\ :sub:`v`) to prevent
  unrealistic flow separation

**Implementation**

The solver computes spatially-varying *α*\ :sub:`v` based on local terrain slope
and curvature using an exponential decay relationship:

.. math::

   \alpha_v = \alpha_{v,flat} \cdot \exp\left(-\frac{s}{s_{scale}}\right)

where:

* *s* = terrain slope magnitude (dimensionless, rise/run)
* *s*\ :sub:`scale` = characteristic slope for decay (typically 0.2-0.3)
* *α*\ :sub:`v,flat` = vertical coefficient for flat terrain (typically 1.0)

**Curvature Modulation**

Terrain curvature (∇²*z*) provides additional refinement:

* **Ridge tops** (positive curvature): Reduce *α*\ :sub:`v` by up to 30% to
  constrain vertical motion over crests
* **Valley bottoms** (negative curvature): Increase *α*\ :sub:`v` by up to 50%
  to allow enhanced vertical adjustment in channeled flow

**Slope Regimes**

.. list-table::
   :header-rows: 1
   :widths: 25 25 50

   * - Slope Range
     - Typical α_v
     - Behavior
   * - < 0.05 (flat)
     - 0.9-1.0
     - Nearly isotropic adjustment
   * - 0.1-0.2 (moderate)
     - 0.5-0.7
     - Balanced terrain-following
   * - 0.3-0.5 (steep)
     - 0.2-0.4
     - Strong terrain-following
   * - > 0.5 (very steep)
     - 0.1-0.2
     - Minimal vertical adjustment

**Usage**

Enable terrain-adaptive alpha coefficients::

    enable_terrain_adaptive_alpha = true
    alpha_h_base = 1.0              # Horizontal coefficient (constant)
    alpha_v_flat = 1.0              # Vertical coeff for flat terrain
    alpha_slope_scale = 0.25        # Slope decay parameter

**Benefits**

* **Automatic tuning**: No manual adjustment of *α* values for each terrain type
* **Improved mass conservation**: Better preservation of terrain-following flow
* **Reduced artifacts**: Smoother flow over complex topography
* **Physical consistency**: Adjustment strategy adapts to local terrain characteristics

**References**

* Ross, D.G., et al. (1988). Diagnostic wind field modeling for complex terrain.
  *J. Appl. Meteor.*, 27, 785-796.
* Sherman, C.A. (1978). A mass-consistent model for wind fields over complex terrain.
  *J. Appl. Meteor.*, 17, 312-319.

**Regression Tests**

* ``regtest/terrain_adaptive_alpha/`` — Gaussian hill with varying slope regimes

Katabatic/Anabatic Slope Flows
-------------------------------

**Physical Motivation**

In mountainous terrain, differential heating/cooling between the surface and air creates
thermally-driven flows parallel to terrain slopes:

* **Anabatic flows**: Daytime up-slope winds when surface is warmer than air
* **Katabatic flows**: Nighttime down-slope winds when surface is cooler than air (cold air drainage)

These flows are often the dominant feature in mountain meteorology, overriding synoptic winds.
Critical for:

* Valley cold air drainage and frost forecasting
* Fire spread on slopes (upslope winds accelerate fire)
* Pollutant transport in complex terrain
* Mountain-valley circulation systems

**Implementation**

The slope flow velocity is parameterized as:

.. math::

   V_{slope} = C \cdot g \cdot \frac{\Delta T}{T} \cdot \sin(\theta) \cdot \exp(-z/H)

where:

* *C* = empirical coefficient (1-5 m/s, typically 2.5)
* *g* = gravitational acceleration (9.81 m/s²)
* Δ*T* = surface - air temperature difference [K]
* *T* = reference temperature [K]
* θ = terrain slope angle
* *z* = height above ground [m]
* *H* = vertical decay height (50-200 m typical)

**Direction**: The flow is directed:

* **Upslope** when Δ*T* > 0 (anabatic)
* **Downslope** when Δ*T* < 0 (katabatic)

The slope aspect (upslope direction) is computed from terrain gradients.

**Usage**

Enable slope flows in your input file::

    # Katabatic/Anabatic Slope Flows
    enable_slope_flows = true
    slope_flow_temperature_diff = -5.0              # Surface 5K cooler (katabatic)
    slope_flow_reference_temperature = 300.0        # Reference temperature [K]
    slope_flow_empirical_coefficient = 2.5          # Empirical constant [m/s]
    slope_flow_vertical_decay_height = 50.0         # Vertical decay [m]
    slope_flow_min_slope = 0.05                     # Minimum slope (~3 degrees)

**Example: Nighttime Katabatic Flow**

Cold air drainage down mountain slopes::

    enable_slope_flows = true
    slope_flow_temperature_diff = -8.0      # 8K cooling (strong katabatic)
    slope_flow_empirical_coefficient = 3.0  # Moderate strength

**Example: Daytime Anabatic Flow**

Upslope winds from surface heating::

    enable_slope_flows = true
    slope_flow_temperature_diff = 5.0       # 5K heating (anabatic)
    slope_flow_empirical_coefficient = 2.0  # Weaker than katabatic

**Regression Tests**

* ``regtest/katabatic_flow/`` — nighttime downslope cold air drainage
* ``regtest/anabatic_flow/`` — daytime upslope heating flow

**References**

* Whiteman, C.D. (1990). Observations of thermally developed wind systems in
  mountainous terrain. *Atmospheric Processes over Complex Terrain*, Meteorological
  Monographs, 23, 5-42.
* Manins, P.C., & Sawford, B.L. (1979). A model of katabatic winds. *Journal of
  the Atmospheric Sciences*, 36(4), 619-630.
* Zardi, D., & Whiteman, C.D. (2013). Diurnal mountain wind systems. *Mountain
  Weather Research and Forecasting*, 35-119.

Valley Channeling Factor
--------------------------

**Physical Motivation**

Valleys strongly channel airflow regardless of the synoptic wind direction.
Observed effects include:

* Wind alignment within ±30° of valley axis
* Speed-up in narrow valleys (venturi effect)
* Speed reduction in wide valleys (increased friction)

The valley channeling model rotates wind vectors toward the valley axis and
adjusts wind speed based on valley geometry.

**Implementation**

The solver implements valley channeling with automatic valley detection:

1. **Valley axis detection**: Ridge-line detection from terrain gradients
2. **Channeling strength**: Based on valley depth, width, and wind angle
3. **Wind rotation**: Blends synoptic and valley-aligned directions
4. **Speed adjustment**: Venturi effect (narrow) or friction (wide valleys)

The channeling strength *C* depends on valley geometry:

.. math::

   C = C_{max} \cdot \tanh\left(\frac{H_v}{200}\right) \cdot 
       \exp\left(-\frac{W_v}{2000}\right) \cdot \cos^2(\theta_{diff})

where:

* *H*\ :sub:`v` = valley depth [m]
* *W*\ :sub:`v` = valley width [m]
* θ\ :sub:`diff` = angle between synoptic wind and valley axis
* *C*\ :sub:`max` = maximum channeling strength (typically 0.8)

**Wind Direction Rotation**

The wind direction is rotated toward the valley axis:

.. math::

   \theta_{new} = (1 - C) \cdot \theta_{synoptic} + C \cdot \theta_{valley}

**Speed Adjustment**

* **Narrow valleys** (*W*\ :sub:`v` < 500 m): Speed-up factor ~1.3 (venturi)
* **Wide valleys** (*W*\ :sub:`v` > 2000 m): Slowdown factor ~0.85 (friction)
* **Medium valleys**: No speed adjustment

**Usage**

Enable valley channeling in your input file::

    # Enable valley channeling
    enable_valley_channeling = true
    valley_axis_angle_deg = 90.0         # Valley axis direction [degrees]
                                         # (counter-clockwise from x-axis)
    valley_width = 1000.0                # Valley width [m]
    valley_depth = 300.0                 # Valley depth [m]
    valley_channeling_strength_max = 0.8 # Maximum channeling strength (0-1)
    valley_speedup_factor_narrow = 1.3   # Speed-up for narrow valleys
    valley_slowdown_factor_wide = 0.85   # Slowdown for wide valleys

**Automatic Valley Detection**

The solver can automatically detect valley orientation from terrain if the
parameters are not explicitly set. The detection algorithm:

1. Analyzes terrain gradients in the neighborhood
2. Identifies ridge directions (positive curvature)
3. Sets valley axis perpendicular to ridge lines
4. Estimates valley width and depth from terrain cross-section

**Examples**

North-south oriented valley with strong channeling::

    enable_valley_channeling = true
    valley_axis_angle_deg = 90.0    # N-S valley
    valley_width = 800.0            # Narrow valley
    valley_depth = 250.0            # Moderately deep
    valley_channeling_strength_max = 0.8

**Applications**

* Mountain wind forecasting
* Hydroelectric facility siting
* Aviation in mountainous terrain
* Wind resource assessment in valleys
* Pollutant dispersion in valley environments

**References**

* Whiteman, C.D. (2000). *Mountain Meteorology: Fundamentals and Applications*.
  Oxford University Press.
* Zardi, D., & Whiteman, C.D. (2013). Diurnal mountain wind systems. In
  *Mountain Weather Research and Forecasting* (pp. 35-119). Springer.
* Rampanelli, G., Zardi, D., & Rotunno, R. (2004). Mechanisms of up-valley
  winds. *Journal of the Atmospheric Sciences*, 61(24), 3097-3111.

Terrain-Following (Streamline) Coordinates
-------------------------------------------

**Physical Motivation**

Mass-consistent solvers work better when the computational coordinates align
with the flow, especially over steep terrain:

* **Reduces artificial divergence** induced by terrain in Cartesian grids
* **Improves numerical stability** on steep slopes (>30°)
* **Better represents boundary layers** that follow terrain contours
* **More accurate mass conservation** in complex topography

**Mathematical Formulation**

The terrain-following coordinate transformation follows Mason & King (1985)
sigma-coordinate approach, transforming from Cartesian coordinates (*x*, *y*, *z*)
to terrain-following coordinates (*x*, *y*, *s*):

.. math::

   s = z - z_{\text{terrain}}(x,y) \cdot f(z_{\text{agl}})

where *z*\ :sub:`terrain`\ (*x*, *y*) is the terrain elevation and *f*\ (*z*\ :sub:`agl`) is a
decay function that provides smooth transition from terrain-following at the
surface (*f* = 1) to flat coordinates aloft (*f* → 0).

**Decay Function**

An exponential decay function ensures smooth transition:

.. math::

   f(z_{\text{agl}}) = \exp\left(-\frac{z_{\text{agl}}}{H}\right)

where:

* *z*\ :sub:`agl` = *z* − *z*\ :sub:`terrain` is height above ground level [m]
* *H* is the decay height scale [m], typically *H* ≈ *domain_height* / 3

The decay function has these properties:

* At the surface (*z*\ :sub:`agl` = 0): *f* = 1 (fully terrain-following)
* At height *H*: *f* = 1/*e* ≈ 0.37 (partially terrain-following)
* Aloft (*z*\ :sub:`agl` ≫ *H*): *f* → 0 (Cartesian coordinates)

**Jacobian and Metric Terms**

The coordinate transformation introduces metric coefficients in the divergence
operator. The Jacobian of the transformation is:

.. math::

   J = \frac{\partial z}{\partial s} = \frac{1}{1 - z_{\text{terrain}} \cdot f'(z_{\text{agl}})}

where the derivative of the decay function is:

.. math::

   f'(z_{\text{agl}}) = \frac{\partial f}{\partial z} = -\frac{1}{H} \exp\left(-\frac{z_{\text{agl}}}{H}\right)

The horizontal metric coefficients are:

.. math::

   \frac{\partial s}{\partial x} &= -\frac{\partial z_{\text{terrain}}}{\partial x} \cdot f(z_{\text{agl}}) \\
   \frac{\partial s}{\partial y} &= -\frac{\partial z_{\text{terrain}}}{\partial y} \cdot f(z_{\text{agl}})

**Modified Divergence Operator**

In terrain-following coordinates, the divergence includes metric correction terms:

.. math::

   \nabla \cdot \mathbf{u} = \frac{\partial u}{\partial x} + \frac{\partial v}{\partial y} + \frac{1}{J}\frac{\partial(J w)}{\partial z}
   - \left(\frac{\partial s}{\partial x} \frac{\partial u}{\partial z} + \frac{\partial s}{\partial y} \frac{\partial v}{\partial z}\right)

The first three terms are the standard Cartesian divergence (computed on the
*z*-grid). The last term is the metric correction accounting for the terrain-following
transformation.

**Modified Poisson Equation**

The anisotropic Poisson equation for the Lagrange multiplier *λ* becomes:

.. math::

   -\nabla \cdot (\alpha^2 \nabla \lambda) = -\nabla \cdot \mathbf{u}_0

where the vertical diffusion coefficient is scaled by the Jacobian:

.. math::

   \alpha_v^2 \rightarrow \alpha_v^2 \cdot J^2

This accounts for the metric tensor in the terrain-following coordinate system.

**Implementation**

To enable terrain-following coordinates::

    # Enable terrain-following coordinate transformation
    enable_terrain_following = true
    
    # Decay height scale [m] (optional; defaults to domain_height / 3)
    terrain_decay_height = 100.0

**When to Use**

Terrain-following coordinates are beneficial for:

* **Very steep terrain**: slopes > 30° (1:2 slope ratio)
* **Deep valleys**: where boundary layer follows terrain closely
* **Research-grade simulations**: requiring high accuracy in complex terrain
* **Improved mass conservation**: when Cartesian grids show excessive divergence

**Limitations**

* Adds computational cost (~10-15% slower due to metric term calculations)
* Most effective on smooth terrain (not cliffs or sharp ridges)
* Requires sufficient vertical resolution to capture decay function

**References**

* Mason, P. J., & King, J. C. (1985). Measurements and predictions of flow
  and turbulence over an isolated hill of moderate slope. *Quarterly Journal
  of the Royal Meteorological Society*, 111(468), 617-640.
* Gal-Chen, T., & Somerville, R. C. (1975). On the use of a coordinate
  transformation for the solution of the Navier-Stokes equations. *Journal
  of Computational Physics*, 17(2), 209-228.

Summary of Physics Models
--------------------------

.. list-table::
   :header-rows: 1
   :widths: 30 50 20

   * - Feature
     - Description
     - Header File
   * - Atmospheric stability
     - Monin-Obukhov similarity theory (Businger-Dyer)
     - ``stability_models.H``
   * - Thermal buoyancy
     - Boussinesq approximation for temperature-driven flow
     - ``buoyancy_models.H``
   * - Kinematic terrain BC
     - No-flow-through condition at terrain surface
     - ``buoyancy_models.H``
   * - Ekman veer
     - Wind direction rotation with height (Coriolis)
     - ``stability_models.H``
   * - Elevation scaling
     - Wind speed variation with terrain elevation
     - ``stability_models.H``
   * - Building porosity
     - Partial flow through porous structures
     - ``porosity_models.H``
   * - Wall functions
     - Log-law boundary conditions for coarse grids
     - ``wall_functions.H``
   * - Canopy drag
     - Forest canopy parameterization
     - ``canopy_models.H``
   * - Building wakes
     - Röckle, Huber-Snyder, and AERMOD PRIME wake models
     - ``wake_models.H``
   * - Slope flows
     - Katabatic/anabatic thermally-driven flows
     - ``slope_flow_models.H``
   * - Valley channeling
     - Wind alignment and speed adjustment in valleys
     - ``valley_channeling_models.H``
   * - Terrain-following coordinates
     - Streamline coordinates for steep terrain
     - ``terrain_following_coords.H``
   * - Diurnal roughness
     - Time-dependent aerodynamic roughness variations
     - ``diurnal_roughness_models.H``
   * - Boundary layer decay
     - Exponential wind decay above boundary layer
     - ``boundary_layer_decay_models.H``
   * - Momentum flux diagnostics
     - Surface shear stress and friction velocity output
     - ``wind_solver.cpp``
   * - Richardson number diagnostics
     - Automatic boundary layer depth diagnosis
     - ``richardson_number_models.H``
   * - Froude number height scaling
     - Height-dependent terrain blocking intensity
     - ``terrain_blocking_models.H``
   * - Ageostrophic wind balance
     - Geostrophic boundary conditions with Coriolis
     - ``ageostrophic_models.H``

All physics models are:

* **GPU-portable** via AMReX GPU kernels
* **Optional** (disabled by default for backward compatibility)
* **Combinable** (e.g., stability + buoyancy + canopy)
* **Validated** via regression tests

Advanced Boundary Conditions & Wind Profile Features
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In addition to the fundamental physics models listed above, the solver includes
advanced boundary condition and wind profile refinement features for enhanced
realism in complex atmospheric scenarios:

**Diurnal Roughness (Feature 7)**

Aerodynamic roughness length z₀ varies sinusoidally with time of day to represent
diurnal cycles in canopy structure and surface properties.

Configuration: ``enable_diurnal_roughness = true``

**Boundary Layer Wind Decay (Feature 9)**

Wind speed decays exponentially above the boundary layer depth to represent the
transition from well-mixed PBL to stratified free atmosphere.

Configuration: ``enable_bl_decay = true``

**Momentum Flux Diagnostics (Feature 8)**

Computes and outputs surface momentum flux components (τ_x, τ_y) and friction
velocity u* for drag parameterization and land-atmosphere coupling analysis.

Output fields: indices 13-15 in plotfile

**Richardson Number Boundary Layer Depth (Feature 23)**

Diagnoses boundary layer depth automatically by finding where the Richardson
number exceeds a critical threshold (typically 0.25).

Configuration: ``enable_bl_depth_diagnostic = true``

Output fields: indices 16-17 in plotfile

**Froude Number Height Scaling (Feature 21)**

Terrain blocking intensity varies with height through a height-dependent Froude
number (Fr = U(z)/(N·h)), enabling realistic flow blocking at lower levels and
overtopping at upper levels in stably stratified conditions.

Configuration: ``enable_froude_height_scaling = true`` (requires terrain blocking)

**Ageostrophic Wind Balance (Feature 10)**

Applies lateral boundary conditions with geostrophic wind balance, computing the
wind components from pressure gradients and Coriolis parameter based on latitude.

Configuration: ``enable_ageostrophic_balance = true``

Detailed documentation on these features, including physics formulations,
configuration parameters, and validation tests, is available in the
:ref:`implementation_status` section.
