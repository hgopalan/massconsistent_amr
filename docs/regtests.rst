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

Test Descriptions
-----------------

flat_terrain
^^^^^^^^^^^^

**Location:** ``regtest/flat_terrain/``

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

**Location:** ``regtest/gaussian_hill/``

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

**Location:** ``regtest/gaussian_hill_weno5/``

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

**Location:** ``regtest/wake_single_building/``

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

raws_synthetic
^^^^^^^^^^^^^^

**Location:** ``regtest/raws_synthetic/``

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

**Location:** ``regtest/surface_data_synthetic/``

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

Phase 1: Surface Flux Diagnostics and Refinement Features
----------------------------------------------------------

flux_diagnostics_feature
^^^^^^^^^^^^^^^^^^^^^^^^

**Location:** ``regtest/flux_diagnostics_feature/``

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

**Location:** ``regtest/landuse_classification/``

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

**Location:** ``regtest/directional_bias_correction/``

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

Adding New Tests
----------------

1. Create a new sub-directory under ``regtest/``, e.g. ``regtest/my_test/``.

2. Add a terrain file ``terrain.csv`` (X Y Z columns).

3. Write an ``inputs.i`` with the desired solver parameters.

4. Register the test in ``regtest/CMakeLists.txt``:

   .. code-block:: cmake

       add_regression_test(my_test my_test)

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
