.. _regtests:

Regression Tests
================

The regression test suite is located in ``regtest/`` and uses CMake's
``ctest`` infrastructure.  Each test runs the ``wind_solver`` executable with
a pre-prepared ``inputs.i`` file and terrain CSV, verifying that the solver
completes without error.

Running the Tests
-----------------

After building, run all regression tests from the build directory::

    ctest --test-dir build -L regtest --output-on-failure

Or use the custom CMake target::

    cmake --build build --target regtest

To run a single named test::

    ctest --test-dir build -R flat_terrain --output-on-failure

Synthetic Turbulence and Export Coverage
----------------------------------------

The turbulence/export regression coverage is split between a solver run and
Python-side file validation:

* ``synthetic_turbulence_full`` runs ``wind_solver`` with synthetic turbulence
  enabled on the Gaussian hill terrain and verifies end-to-end execution.
* ``synthetic_turbulence_full_validation`` inspects the generated BTS and
  metadata files and optionally exercises BTS-to-VTK conversion.
* ``regtest/turbulence/openfast_export_regression/test_openfast_export.py`` validates BTS
  header layout, metadata consistency, and physical parameter ranges for the
  standalone export path.

Run the focused turbulence checks with::

    ctest --test-dir build -R synthetic_turbulence_full --output-on-failure

Test Descriptions
-----------------

flat_terrain
^^^^^^^^^^^^

**Location:** ``regtest/terrain/flat_terrain/``

**Purpose:** Verifies that the MLMG Poisson solver converges on the simplest
possible geometry — a flat domain with all terrain elevations set to z = 0.

**Terrain:** A 3 × 3 grid of points spanning 0–100 m in x and y, all at z = 0 m.

**Grid:** 2 × 2 × 2 cells (dx = dy = dz = 50 m, domain_height = 100 m).

**Wind:** U_ref = 5 m/s (westerly), z_ref = 10 m, z₀ = 0.1 m.

**Expected behaviour:** On a flat domain the initial log-law field is
horizontally uniform, so ∇·\ **u**₀ ≈ 0 everywhere.  The MLMG solve
converges immediately and the post-correction divergence should be negligible.

**Key input parameters:**

.. code-block:: text

    terrain_file  = terrain.csv
    U_ref         = 5.0
    V_ref         = 0.0
    z_ref         = 10.0
    z0            = 0.1
    dx            = 50.0
    dy            = 50.0
    dz            = 50.0
    domain_height = 100.0
    mlmg_verbose  = 0
    plot_file     = plt_flat_terrain

gaussian_hill
^^^^^^^^^^^^^

**Location:** ``regtest/terrain/gaussian_hill/``

**Purpose:** Verifies terrain-following wind initialisation and mass-consistent
correction on a realistic hill geometry.

**Terrain:** An 11 × 11 point cloud over a 300 × 300 m domain with a Gaussian
hill of peak elevation 50 m at the domain centre (σ = 60 m).

**Grid:** 10 × 10 × 6 cells (dx = dy = 30 m, dz = 25 m, domain_height = 100 m).

**Wind:** U_ref = 10 m/s (westerly), z_ref = 10 m, z₀ = 0.03 m (short grass).

**Expected behaviour:** The log-law profile accelerates over the hill crest and
decelerates in the lee.  The mass-consistent correction adjusts the vertical
velocity to enforce ∇·\ **u** = 0, producing a physically consistent flow.
The test also writes a terrain-aligned CSV extract at 15 m AGL.

**Key input parameters:**

.. code-block:: text

    terrain_file  = terrain.csv
    U_ref         = 10.0
    V_ref         = 0.0
    z_ref         = 10.0
    z0            = 0.03
    dx            = 30.0
    dy            = 30.0
    dz            = 25.0
    domain_height = 100.0
    alpha_h       = 1.0
    alpha_v       = 1.0
    mlmg_verbose  = 0
    extract_agl   = 15.0
    extract_file  = wind_extract.csv
    plot_file     = plt_gaussian_hill

gaussian_hill_weno5
^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/terrain/gaussian_hill_weno5/``

**Purpose:** Verifies that WENO-5 derivative computations produce consistent results
compared to the standard central-difference method on the same terrain geometry.

**Terrain:** Identical to the Gaussian hill test — an 11 × 11 point cloud over a 
300 × 300 m domain with a Gaussian hill of peak elevation 50 m.

**Grid:** Identical to Gaussian hill test (10 × 10 × 6 cells).

**Wind:** Identical to Gaussian hill test (U_ref = 10 m/s, z_ref = 10 m, z₀ = 0.03 m).

**Expected behaviour:** The solution should be qualitatively similar to the central
method but with potential improvements in smoothness at discontinuities or near steep
gradients. The test verifies that the WENO-5 method converges without errors and
produces physically consistent results.

**Key input parameters:**

.. code-block:: text

    terrain_file  = terrain.csv
    U_ref         = 10.0
    V_ref         = 0.0
    z_ref         = 10.0
    z0            = 0.03
    dx            = 30.0
    dy            = 30.0
    dz            = 25.0
    domain_height = 100.0
    alpha_h       = 1.0
    alpha_v       = 1.0
    deriv_method  = weno5
    mlmg_verbose  = 0
    extract_agl   = 15.0
    extract_file  = wind_extract.csv
    plot_file     = plt_gaussian_hill_weno5

wake_single_building
^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/wakes/wake_single_building/``

**Purpose:** Verifies the Röckle (1990) building wake model for a single rectangular
building on flat terrain.

**Terrain:** Flat domain (300 × 200 m, z = 0 everywhere).

**Building:** Single rectangular building (40m × 20m × 30m tall) centered at
x=100m, y=100m.

**Grid:** 60 × 40 × 30 cells (dx = dy = dz = 5 m, domain_height = 150 m).

**Wind:** U_ref = 10 m/s (westerly, along +x), z_ref = 10 m, z₀ = 0.1 m.

**Wake model:** Röckle formulation enabled with default parameters
(c1=0.9, c2=0.3, separation_length=3.0).

**Expected behaviour:** The wake model creates:

* **Cavity zone**: Recirculation region extending ~27m (0.9 × 30m) downwind of
  the building with reduced/negative velocity
* **Far-wake zone**: Velocity deficit region extending to ~90m (3 × 30m) downwind
  with gradual recovery

The mass-consistency solver then adjusts the flow to ensure ∇·\ **u** = 0.

**Key input parameters:**

.. code-block:: text

    terrain_file  = terrain.csv
    building_file = buildings.csv
    enable_wake   = true
    wake_c1       = 0.9
    wake_c2       = 0.3
    wake_separation_length = 3.0
    U_ref         = 10.0
    V_ref         = 0.0
    z_ref         = 10.0
    z0            = 0.1
    dx            = 5.0
    dy            = 5.0
    dz            = 5.0
    domain_height = 150.0
    extract_agl   = 10.0
    extract_file  = wind_wake_10m.csv
    plot_file     = plt_wake_single_building

wake_polygon_shapes
^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/wakes/wake_polygon_shapes/``

**Purpose:** Validates polygon building support with complex shapes (L, T, U-shaped
buildings) using the Röckle wake model.

**Terrain:** Flat domain (300 × 200 m, z = 0 everywhere).

**Buildings:** Three polygon buildings:

* L-shaped building: 40m × 20m + 20m × 60m sections, 35m tall
* T-shaped building: Vertical stem (40m × 80m) with horizontal cap (100m × 30m), 35m tall
* U-shaped building: 100m × 60m outer perimeter, 35m tall

**Grid:** 60 × 40 × 30 cells (dx = dy = dz = 5 m, domain_height = 300 m).

**Wind:** U_ref = 10 m/s (westerly, along +x), z_ref = 10 m, z₀ = 0.1 m.

**Wake model:** Röckle formulation with polygon vertex processing
(c1=0.9, c2=0.3, separation_length=3.0).

**Expected behaviour:** The solver correctly computes wake deficits for non-rectangular
building footprints using point-in-polygon testing and effective polygon dimensions.
Polygon centroids and effective widths are calculated for each building orientation.

**Key input parameters:**

.. code-block:: text

    building_file = buildings.csv
    enable_wake = true
    wake_model = rockle
    wake_c1 = 0.9
    wake_c2 = 0.3
    wake_separation_length = 3.0
    U_ref = 10.0
    V_ref = 0.0
    z_ref = 10.0
    z0 = 0.1
    dx = 5.0
    dy = 5.0
    dz = 5.0
    domain_height = 300
    extract_file = wind_wake_10m.csv

wake_polygon_huber_snyder
^^^^^^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/wakes/wake_polygon_huber_snyder/``

**Purpose:** Validates polygon building support with EPA Huber-Snyder wake model,
verifying proper cavity/far-wake zone transitions for complex shapes.

**Terrain:** Flat domain (300 × 200 m, z = 0 everywhere).

**Buildings:** Same three polygon buildings as wake_polygon_shapes test.

**Wake model:** Huber-Snyder EPA model with extended cavity zone
(c1=1.5, c2=0.4, separation_length=4.0).

**Expected behaviour:** Polygon buildings compute wake deficits using Huber-Snyder
cavity entrance effects and horizontal turbulence parameterization. Wake zone
transitions (near-cavity/far-wake) follow EPA formulation for effective dimensions.

**Key input parameters:**

.. code-block:: text

    enable_wake = true
    wake_model = huber_snyder
    wake_c1 = 1.5
    wake_c2 = 0.4
    wake_separation_length = 4.0
    C_h = 0.5
    C_v = 0.3

wake_polygon_aermod_prime
^^^^^^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/wakes/wake_polygon_aermod_prime/``

**Purpose:** Validates polygon building support with AERMOD PRIME wake model,
verifying strong cavity deficit and proper wake growth for complex geometries.

**Terrain:** Flat domain (300 × 200 m, z = 0 everywhere).

**Buildings:** Same three polygon buildings as other polygon tests.

**Wake model:** AERMOD PRIME cavity parameterization for polygon footprints.

**Expected behaviour:** Polygon buildings produce larger wake deficits consistent
with AERMOD PRIME formulation. Interior cavity wind speeds reduced significantly
with gradual far-wake recovery following Gaussian plume assumptions.

wake_courtyard_modeling
^^^^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/wakes/wake_courtyard_modeling/``

**Purpose:** Validates internal void zone (courtyard/atrium) exclusion from wake
calculations for polygon buildings with complex internal geometry.

**Terrain:** Flat domain (300 × 300 m, z = 0 everywhere).

**Buildings:** 

* Outer polygon: 200m × 200m perimeter building at z=0-35m
* Internal void zone: 100m × 100m courtyard at z=0-35m (excluded from wakes)
* Separate structure: 40m × 40m polygon at z=0-25m

**Expected behaviour:** Wind speeds inside the void zone are preserved (≥95% of
reference) since void zones do not generate wakes. Superposition correctly
combines wakes from multiple polygon and void structures. Exterior regions show
wake deficits from surrounding buildings.

**Key input parameters:**

.. code-block:: text

    enable_wake = true
    wake_model = rockle
    enable_superposition = true
    building_file = buildings.csv

raws_synthetic
^^^^^^^^^^^^^^

**Location:** ``regtest/terrain/raws_synthetic/``

**Purpose:** Validates the RAWS (Remote Automated Weather Station) initialization
mode by reading sparse velocity observations from a CSV file and using
inverse-distance weighting (IDW) to interpolate them to the grid.

**Terrain:** Gaussian hill (same as gaussian_hill test).

**Grid:** 10 × 10 × 5 cells (dx = dy = 30 m, dz = 25 m).

**Wind:** Three synthetic observation points with varying wind components.

**Expected behaviour:** The IDW interpolation smoothly blends the three wind
observations across the domain. The mass-consistency solver then enforces
divergence-free flow.

**Key input parameters:**

.. code-block:: text

    init_mode     = raws
    velocity_file = velocity.csv
    terrain_file  = terrain.csv
    dx            = 30.0
    dy            = 30.0
    dz            = 25.0
    extract_agl   = 15.0

surface_data_synthetic
^^^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/terrain/surface_data_synthetic/``

**Purpose:** Validates the ``surface_data`` initialization mode designed for
HRRR-style inputs. Reads surface parameters (friction velocity, roughness length,
10m winds) from a CSV file, interpolates them to each grid column, and constructs
per-column vertical log-law profiles with spatially-varying surface properties.

**Terrain:** Gaussian hill (same as gaussian_hill test).

**Grid:** 10 × 10 × 5 cells (dx = dy = 30 m, dz = 25 m).

**Surface data:** Three synthetic observation points with varying USTAR (0.35-0.40 m/s)
and Z0 (0.05-0.10 m) values.

**Expected behaviour:** Each grid column gets its own friction velocity and roughness
from IDW interpolation, creating inhomogeneous vertical profiles. This enables
realistic simulation of spatially-varying surface conditions from HRRR or similar
model output.

**Key input parameters:**

.. code-block:: text

    init_mode          = surface_data
    surface_data_file  = surface_data.csv
    terrain_file       = terrain.csv
    dx                 = 30.0
    dy                 = 30.0
    dz                 = 25.0
    extract_agl        = 15.0

Advanced Features
^^^^^^^^^^^^^^^^^

The following tests validate advanced boundary condition and wind profile features:

diurnal_roughness
~~~~~~~~~~~~~~~~~

**Location:** ``regtest/physics/diurnal_roughness/``

**Purpose:** Tests time-dependent variation of aerodynamic roughness length z₀(t) 
following a diurnal cycle. Validates that the sinusoidal modulation is correctly 
applied to the initial log-law profiles.

**Physics:** z₀(t) = z₀_base × [1 + A·sin(πt/12 + φ)]

**Key input parameters:**

.. code-block:: text

    enable_diurnal_roughness = true
    roughness_amplitude      = 0.3
    diurnal_time_of_day      = 14.0

bl_decay
~~~~~~~~

**Location:** ``regtest/physics/bl_decay/``

**Purpose:** Verifies exponential wind decay above the boundary layer depth. 
Tests that wind speed decays correctly in the region above z_BL.

**Physics:** u(z) = u_BL · exp[-(z - z_BL) / H_decay] for z > z_BL

**Key input parameters:**

.. code-block:: text

    enable_bl_decay         = true
    bl_depth_param          = 80.0
    decay_height_scale      = 20.0
    bl_transition_height    = 10.0

momentum_flux
~~~~~~~~~~~~~

**Location:** ``regtest/physics/momentum_flux/``

**Purpose:** Validates computation and output of momentum flux diagnostic fields 
(τ_x, τ_y, u*). Ensures correct friction velocity and shear stress calculation 
and output in plotfile.

**Output fields validated:**

- Index 13: τ_x [Pa] - Shear stress x-component
- Index 14: τ_y [Pa] - Shear stress y-component
- Index 15: u* [m/s] - Friction velocity

richardson_diagnostic
~~~~~~~~~~~~~~~~~~~~~~

**Location:** ``regtest/physics/richardson_diagnostic/``

**Purpose:** Tests Richardson number computation and boundary layer depth diagnosis. 
Validates that the critical Richardson number (Ri ≈ 0.25) correctly identifies 
the boundary layer top.

**Physics:** Ri = (g/θ)·(dθ/dz) / [(du/dz)² + (dv/dz)²]

**Output fields validated:**

- Index 16: richardson_no - Richardson number diagnostic
- Index 17: bl_depth [m] - Diagnosed boundary layer depth

**Key input parameters:**

.. code-block:: text

    enable_bl_depth_diagnostic  = true
    richardson_critical        = 0.25
    richardson_min_wind_shear  = 0.001

froude_scaling
~~~~~~~~~~~~~~

**Location:** ``regtest/physics/froude_scaling/``

**Purpose:** Validates height-dependent terrain blocking intensity modification through 
Froude number scaling. Tests that blocking intensity varies realistically with height 
and wind speed.

**Physics:** Fr(z) = U(z) / (N·h), where blocking effect ∝ 1/Fr(z)

**Key input parameters:**

.. code-block:: text

    enable_terrain_blocking                    = true
    enable_froude_height_scaling               = true
    terrain_blocking_brunt_vaisala_frequency   = 0.01

ageostrophic_balance
~~~~~~~~~~~~~~~~~~~~

**Location:** ``regtest/physics/ageostrophic_balance/``

**Purpose:** Tests geostrophic wind balance at domain boundaries with proper 
Coriolis parameter computation. Validates that ageostrophic wind components 
are correctly computed from pressure gradients and latitude.

**Physics:** U_geo = -(1/ρf)·∂p/∂y, V_geo = +(1/ρf)·∂p/∂x

**Key input parameters:**

.. code-block:: text

    enable_ageostrophic_balance         = true
    ageostrophic_latitude               = 45.0
    ageostrophic_pressure_grad_y        = -1.0
    ageostrophic_air_density            = 1.225
    ageostrophic_fraction               = 0.15

thermodynamic_lid
~~~~~~~~~~~~~~~~~

**Location:** ``regtest/physics/thermodynamic_lid/``

**Purpose:** Validates 1-D thermodynamic convective boundary layer (CBL) mixing height growth models.

**Physics:** Integrates surface heat flux over time to calculate mixing height development.

**Key input parameters:**

.. code-block:: text

    enable_thermodynamic_lid   = true
    heat_flux_file             = flux.csv
    time_series_file           = time_series.csv

obrien_adjustment
~~~~~~~~~~~~~~~~~

**Location:** ``regtest/physics/obrien_adjustment/``

**Purpose:** Validates O'Brien column-wise vertical velocity adjustment procedure.

**Physics:** Redistributes vertical velocity errors proportionally across the column.

**Key input parameters:**

.. code-block:: text

    enable_obrien_adjustment   = true

Surface Flux Diagnostics and Refinement Features
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

flux_diagnostics_feature
^^^^^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/physics/flux_diagnostics_feature/``

**Purpose:** Verifies that surface flux diagnostic fields are computed correctly,
including sensible heat flux (SHF), latent heat flux (LHF), momentum flux (τ), and
drag coefficient (C_d). These diagnostics are critical for fire-atmosphere coupling,
dust emission parameterization, and surface energy balance calculations.

**Terrain:** Flat 3 × 3 grid, simplified geometry to isolate flux computations
(domain 0–100 m in x, y; all z = 0).

**Grid:** 2 × 2 × 2 cells (dx = dy = 50 m, dz = 25 m, domain_height = 100 m).

**Wind:** U_ref = 10 m/s (westerly), z_ref = 10 m, z₀ = 0.1 m.

**Expected behaviour:** On flat terrain with uniform roughness, surface fluxes should be
spatially uniform. The solver computes friction velocity (u*), heat fluxes, and drag
coefficients using the logarithmic profile at the surface layer.

**Key input parameters:**

.. code-block:: text

    enable_flux_diagnostics = true
    surface_temperature = 300.0
    heat_flux_scale = 1.0
    relative_humidity = 0.5
    U_ref = 10.0
    V_ref = 0.0
    z_ref = 10.0
    z0 = 0.1
    dx = 50.0
    dy = 50.0
    dz = 25.0
    domain_height = 100.0
    extract_agl = 10.0

landuse_classification
^^^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/physics/landuse_classification/``

**Purpose:** Verifies that NLCD (National Land Cover Database)-compatible land-use
classification correctly maps land-use categories to aerodynamic roughness lengths (z₀).
This feature enables spatially-varying surface properties based on land-use type, critical
for realistic wind modelling over heterogeneous terrain.

**Terrain:** Gaussian hill with mixed land-use categories (11 × 11 point cloud over
300 × 300 m domain, peak 50 m).

**Land-use categories:** Grassland (code 71, z₀ = 0.05 m), Deciduous forest (code 41,
z₀ = 0.8 m), Developed open space (code 21, z₀ = 0.3 m).

**Grid:** 10 × 10 × 5 cells (dx = dy = 30 m, dz = 25 m).

**Wind:** U_ref = 10 m/s (westerly), z_ref = 10 m, spatially-varying z₀ from classification.

**Expected behaviour:** The vertical log-law profile varies horizontally based on local
land-use category. Forested regions show more wind shear (larger z₀), while grassland
shows weaker shear. IDW interpolation of land-use derived z₀ produces smooth transitions
between categories.

**Key input parameters:**

.. code-block:: text

    landuse_file = landuse.csv
    enable_landuse_classification = true
    landuse_interp_method = idw
    U_ref = 10.0
    V_ref = 0.0
    z_ref = 10.0
    z0 = 0.1
    dx = 30.0
    dy = 30.0
    dz = 25.0
    domain_height = 100.0
    extract_agl = 15.0

directional_bias_correction
^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/physics/directional_bias_correction/``

**Purpose:** Verifies that systematic directional and speed biases from NWP model output
are correctly applied to the initial wind field. This feature corrects common model errors
such as constant directional offset or speed-dependent biases before mass-consistent adjustment.

**Terrain:** Gaussian hill (11 × 11 point cloud, 300 × 300 m domain, peak 50 m).

**Grid:** 10 × 10 × 6 cells (dx = dy = 30 m, dz = 25 m, domain_height = 100 m).

**Wind:** U_ref = 10 m/s reference, with applied corrections:

* Constant direction bias: 30° (model wind rotated 30° counterclockwise)
* Speed bias factor: 1.05 (model wind speed multiplied by 5%)

**Expected behaviour:** The initial wind field is rotated and scaled before the
mass-consistent solver enforces divergence-free flow. The corrected wind should show
the specified rotation and speed adjustment relative to an uncorrected case.

**Key input parameters:**

.. code-block:: text

    enable_directional_bias_correction = true
    direction_bias_constant = 30.0
    speed_bias_factor = 1.05
    enable_periodic_bias = false
    direction_bias_amplitude = 15.0
    direction_bias_phase = 0.0
    U_ref = 10.0
    V_ref = 0.0
    z_ref = 10.0
    z0 = 0.03
    dx = 30.0
    dy = 30.0
    dz = 25.0
    domain_height = 100.0
    extract_agl = 15.0

cell_local_anisotropy
^^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/physics/cell_local_anisotropy/`` (all three sources combined)

**Purpose:** Verifies that cell-local spatially-varying anisotropic weighting tensor 
A(x,y,z) is correctly computed and applied. The anisotropy coefficients (α_h / α_v) 
adapt cell-locally based on terrain slope, Richardson number (atmospheric stability), 
and Froude number (orographic blocking). This advanced feature captures complex 
interactions between terrain, wind, and atmospheric conditions.

**Terrain:** Gaussian hill (11 × 11 point cloud over 300 × 300 m domain, peak 50 m).

**Grid:** 10 × 10 × 6 cells (dx = dy = 30 m, dz = 25 m, domain_height = 100 m).

**Wind:** U_ref = 10 m/s (westerly), z_ref = 10 m, z₀ = 0.03 m.

**Thermodynamics:** Stable potential temperature gradient (0.005 K/m) to enable 
Richardson number variations.

**Expected behaviour:** The mass-consistent solver converges, and alpha_v varies 
spatially according to local slope, stability, and Froude number. Steeper slopes, 
stable conditions, and strong wind blocking modulate the vertical anisotropy locally.

**Key input parameters:**

.. code-block:: text

    enable_cell_local_anisotropy = true
    anisotropy_source = all
    anisotropy_slope_scale = 0.25
    anisotropy_decay_height = 100.0
    anisotropy_ri_gamma = 1.0
    anisotropy_ri_beta = 0.5
    anisotropy_fr_min = 0.1
    temperature_file = "temperature.csv"
    temperature_gradient = 0.005

cell_local_anisotropy_slope
^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/physics/cell_local_anisotropy_slope/``

**Purpose:** Isolates and validates the terrain slope contribution to cell-local 
anisotropy. Only the slope factor (f_slope) is active; Richardson and Froude 
contributions are disabled.

**Terrain:** Identical to cell_local_anisotropy (Gaussian hill).

**Expected behaviour:** Alpha_v is modulated primarily by local terrain slope. 
Steep regions show strongest anisotropy adjustment. Highest slope gradients occur 
on the hill sides, producing the most significant alpha_v variations there.

**Key input parameters:**

.. code-block:: text

    enable_cell_local_anisotropy = true
    anisotropy_source = slope
    anisotropy_slope_scale = 0.25
    anisotropy_decay_height = 100.0

cell_local_anisotropy_richardson
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/physics/cell_local_anisotropy_richardson/``

**Purpose:** Isolates and validates the Richardson number (stability) contribution 
to cell-local anisotropy. Only the Richardson factor (f_ri) is active; slope and 
Froude contributions are disabled.

**Terrain:** Identical to cell_local_anisotropy (Gaussian hill).

**Thermodynamics:** Stable potential temperature gradient (0.005 K/m).

**Expected behaviour:** Alpha_v is modulated by local atmospheric stability 
(Richardson number). Stable stratification (positive Ri) reduces alpha_v, while 
unstable conditions (negative Ri) enhance it. Height-dependent variations of 
Richardson number create vertical heterogeneity in anisotropy.

**Key input parameters:**

.. code-block:: text

    enable_cell_local_anisotropy = true
    anisotropy_source = richardson
    anisotropy_ri_gamma = 1.0
    anisotropy_ri_beta = 0.5
    temperature_file = "temperature.csv"
    temperature_gradient = 0.005

cell_local_anisotropy_froude
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/physics/cell_local_anisotropy_froude/``

**Purpose:** Isolates and validates the Froude number (orographic blocking) 
contribution to cell-local anisotropy. Only the Froude factor (f_fr) is active; 
slope and Richardson contributions are disabled.

**Terrain:** Identical to cell_local_anisotropy (Gaussian hill).

**Thermodynamics:** Stable potential temperature gradient (0.005 K/m) to enable 
Froude number variations via buoyancy frequency.

**Expected behaviour:** Alpha_v is modulated by local Froude number, which depends 
on wind speed, terrain height scale, and atmospheric stability (buoyancy 
frequency). Low Froude number (weak wind or steep terrain) indicates flow blocking, 
leading to larger anisotropy adjustments. High Froude number (strong wind) 
indicates flow acceleration over terrain.

**Key input parameters:**

.. code-block:: text

    enable_cell_local_anisotropy = true
    anisotropy_source = froude
    anisotropy_fr_min = 0.1
    temperature_file = "temperature.csv"
    temperature_gradient = 0.005

Adding New Tests

----------------

1. Create a new sub-directory under the appropriate category under ``regtest/``, e.g. ``regtest/physics/my_test/``.

2. Add a terrain file ``terrain.csv`` (X Y Z columns).

3. Write an ``inputs.i`` with the desired solver parameters.

4. Register the test in ``regtest/CMakeLists.txt``:

   .. code-block:: cmake

       add_regression_test(my_test physics/my_test)

5. Re-run CMake to pick up the new test::

       cmake -S . -B build
       ctest --test-dir build -R my_test --output-on-failure

CI Integration
--------------

The Linux and macOS builds in `.github/workflows/cmake_build.yml` run the full
regression suite automatically after each successful build::

    ctest --test-dir build -L regtest --output-on-failure

This ensures that every push and pull request verifies both compilation and
solver correctness on multiple operating systems.
