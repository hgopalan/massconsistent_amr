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
