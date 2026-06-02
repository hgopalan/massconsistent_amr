.. _usage:

Usage
=====

Running the Solver
------------------

Pass an input file to the ``wind_solver`` executable::

    ./build/wind_solver inputs.i

Or supply parameters directly on the command line::

    ./build/wind_solver terrain_file=terrain.csv U_ref=8.0 z0=0.05

Command-line key=value pairs override values in the input file.

Input File Format
-----------------

The input file uses AMReX ``ParmParse`` syntax — one ``key = value`` pair per
line.  Lines beginning with ``#`` are comments.

Example ``inputs.i``::

    # Mass-consistent wind solver input file

    terrain_file  = terrain.csv   # X Y Z point cloud

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
     - Path to terrain point-cloud file (X Y Z, whitespace or comma
       separated; ``#`` comments supported).
   * - **Wind Initialization Mode**
     -
     -
   * - ``init_mode``
     - ``loglaw``
     - Wind field initialization method. Options: ``loglaw`` (log-law profile),
       ``uniform`` (constant wind), ``raws`` (interpolate from velocity file),
       ``surface_data`` (HRRR-style surface parameters), ``powerlaw`` (power-law profile).
   * - ``U_ref``
     - ``10.0``
     - Reference wind x-component [m/s] at height ``z_ref``. Used for ``loglaw`` and ``powerlaw`` modes.
   * - ``V_ref``
     - ``0.0``
     - Reference wind y-component [m/s] at height ``z_ref``. Used for ``loglaw`` and ``powerlaw`` modes.
   * - ``z_ref``
     - ``10.0``
     - Reference height above the local terrain surface [m]. Used for ``loglaw`` and ``powerlaw`` modes.
   * - ``z0``
     - ``0.1``
     - Aerodynamic roughness length [m]. Used for ``loglaw`` mode.
   * - ``powerlaw_exponent``
     - ``0.143``
     - Power-law exponent α for ``powerlaw`` mode: u(z) = U_ref * (z/z_ref)^α. 
       Typical value: 0.143 (~1/7) for neutral conditions. Range: 0.1-0.4.
   * - ``uniform_U``
     - ``U_ref``
     - Constant x-wind [m/s] for ``uniform`` mode.
   * - ``uniform_V``
     - ``V_ref``
     - Constant y-wind [m/s] for ``uniform`` mode.
   * - ``velocity_file``
     - ``velocity.csv``
     - Path to velocity data file for ``raws`` mode (format: X Y Z Ux Uy).
   * - ``surface_data_file``
     - ``surface_data.csv``
     - Path to surface parameter file for ``surface_data`` mode (format: X Y Z USTAR Z0 U10 V10).
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
     - Vertical grid spacing [m].  Reduce for finer near-surface resolution.
   * - ``domain_height``
     - ``300.0``
     - Vertical domain extent above the maximum terrain elevation [m].
   * - **Mass-Consistency Parameters**
     -
     -
   * - ``alpha_h``
     - ``1.0``
     - Horizontal Lagrange anisotropy coefficient α_h.
   * - ``alpha_v``
     - ``1.0``
     - Vertical Lagrange anisotropy coefficient α_v (constant if height-dependent mode is disabled).
   * - ``use_height_dependent_alpha_v``
     - ``false``
     - Enable height-dependent vertical anisotropy: α_v(z) varies linearly from surface to top.
   * - ``alpha_v_surface``
     - ``alpha_v``
     - Vertical anisotropy coefficient at domain surface (z=z_lo). Only used when 
       ``use_height_dependent_alpha_v = true``.
   * - ``alpha_v_top``
     - ``alpha_v``
     - Vertical anisotropy coefficient at domain top (z=z_hi). Only used when 
       ``use_height_dependent_alpha_v = true``.
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
     - Maximum MLMG iterations. Reduce for faster (less accurate) solves.
   * - ``mlmg_max_fmg_iter``
     - ``20``
     - Maximum Full Multigrid (FMG) iterations. Reduce for faster convergence.
   * - ``mlmg_pre_smooth``
     - ``16``
     - Pre-smoothing iterations per V-cycle. Reduce to 8-12 for well-conditioned problems.
   * - ``mlmg_post_smooth``
     - ``16``
     - Post-smoothing iterations per V-cycle. Reduce to 8-12 for well-conditioned problems.
   * - ``mlmg_bottom_solver``
     - ``default``
     - Bottom solver method: ``default`` (auto-select), ``bicgstab`` (BiCGStab iterative),
       ``cg`` (Conjugate Gradient), or ``smoother`` (smoother-only, fastest but may diverge).
       Use ``bicgstab`` or ``cg`` for highly anisotropic problems.
   * - ``max_grid_size``
     - ``32``
     - Maximum AMReX box size per spatial dimension. Increase to 64-256 for GPU acceleration
       to improve cache utilization and GPU occupancy.
   * - **Numerical Methods**
     -
     -
   * - ``deriv_method``
     - ``central``
     - Method for computing derivatives: ``central`` (2nd order, one-sided at boundaries),
       ``weno3`` (3rd order WENO), or ``weno5`` (5th order WENO).
   * - **Output Parameters**
     -
     -
   * - ``plot_file``
     - ``plt_wind``
     - Output plotfile prefix.
   * - ``extract_agl``
     - ``-1.0``
     - Sample the corrected wind at this AGL height [m] and write a CSV
       slice. Can be a single value or space-separated list (e.g., ``10.0 50.0 100.0``).
       Negative value disables extraction.
   * - ``extract_k``
     - ``-1``
     - Alternative: sample at explicit k-index (0 = lowest level).
       Can be a single value or space-separated list.
       ``extract_agl`` takes priority when both are set.
   * - ``extract_file``
     - ``wind_extract.csv``
     - Output filename for terrain-aligned CSV extraction. For multi-height extraction,
       height suffix is automatically appended (e.g., ``wind_extract_10m.csv``).
     - Output filename for the terrain-aligned CSV slice.
   * - **Canopy Model Parameters**
     -
     -
   * - ``enable_canopy``
     - ``false``
     - Enable vegetation canopy parameterization (MacDonald et al. 2000 and Shaw-Pereira).
   * - ``canopy_height``
     - ``0.0``
     - Canopy height [m]. Used to compute displacement height and wind profile.
   * - ``frontal_area_index``
     - ``0.0``
     - Frontal area index λ_f (frontal area / ground area). Typical values: 0.2-0.4 for forests.
   * - ``plan_area_index``
     - ``0.0``
     - Plan area index λ_p (plan area / ground area). Typical values: 0.15-0.3 for forests.
   * - ``canopy_drag_coeff``
     - ``0.2``
     - Canopy drag coefficient C_d. Typical range: 0.15-0.3.
   * - ``use_exponential_profile``
     - ``false``
     - Use Shaw-Pereira (1982) exponential decay within canopy instead of log-law.
   * - ``canopy_attenuation``
     - ``2.5``
     - Exponential attenuation coefficient α for Shaw-Pereira profile. Typical range: 2-4.
   * - **Ekman Spiral Veer Parameters**
    -
    -
   * - ``enable_ekman_veer``
    - ``false``
    - Enable Ekman spiral wind veer correction. Wind direction rotates (veers) with height
      due to Coriolis effects and surface friction balance.
   * - ``latitude``
    - ``45.0``
    - Latitude in degrees (positive = North, negative = South). Used to compute Coriolis
      parameter. Affects veer direction: clockwise in Northern Hemisphere, counter-clockwise
      in Southern Hemisphere.
   * - ``ekman_veer_total``
    - ``20.0``
    - Total wind veer from surface to domain top [degrees]. Typical values: 10-30° for
      mid-latitudes. Higher values for stable conditions, lower for unstable conditions.
   * - ``ekman_veer_height``
    - ``200.0``
    - Height scale for veer profile [m]. Most veer occurs within this height. Typical values:
      100-200 m for boundary layer depth scale.
   * - **Buoyancy Stratification Parameters**
    -
    -
   * - ``enable_buoyancy_stratification``
    - ``false``
    - Enable buoyancy effects from thermal stratification. Requires ``temperature_file`` to be specified.
   * - ``temperature_file``
    - ``temperature.csv``
    - Path to temperature profile CSV file. Format: ``Z T`` (height [m above sea level], temperature [K]).
      Lines starting with ``#`` are comments. Temperature is interpolated to grid cells.
   * - ``temperature_reference``
    - ``300.0``
    - Reference temperature T₀ [K] for buoyancy computation. Typically set to mean atmospheric temperature.
   * - ``buoyancy_coefficient``
    - ``1.0``
    - Tuning coefficient for buoyancy strength. Typical range: 0.1-2.0. Higher values strengthen
      buoyancy effects.
   * - ``buoyancy_method``
    - ``velocity``
    - Method for applying buoyancy: ``velocity`` (add to vertical velocity before mass-consistency)
      or ``rhs`` (add source term directly to lambda equation RHS). Both are physically consistent;
      ``rhs`` method avoids the need for ``buoyancy_timescale`` parameter.
   * - ``buoyancy_timescale``
    - ``10.0``
    - Characteristic time scale Δt [s] for integrating buoyancy acceleration to velocity.
      Only used when ``buoyancy_method = velocity``. Typical range: 5-20 s.
   * - **Building Parameters**
    -
    -
   * - ``building_file``
    - (none)
    - Path to building CSV file (optional). Each line defines a building box with
      columns: ``xmin xmax ymin ymax zmin zmax`` [m]. Lines starting with ``#`` are
      comments. Buildings are masked as solid obstacles (zero velocity inside).
   * - **Wake Model Parameters**
    -
    -
   * - ``enable_wake``
    - ``false``
    - Enable Röckle (1990) building wake parameterization. Requires ``building_file`` to be specified.
   * - ``wake_c1``
    - ``0.9``
    - Cavity length coefficient. Cavity extends ``c1 × H`` downwind (H = building height).
   * - ``wake_c2``
    - ``0.3``
    - Wake deficit coefficient. Controls velocity reduction magnitude in wake zones.
   * - ``wake_separation_length``
    - ``3.0``
    - Wake extent factor. Far-wake extends to ``factor × H`` downwind from building.

Terrain File Format
-------------------

The terrain file must contain one data point per line with columns
**X  Y  Z** (in metres, UTM or local coordinates).  Both whitespace and
comma-separated formats are accepted.  Lines beginning with ``#`` are comments::

   # X [m]  Y [m]  Z [m]
   0.0      0.0    5.2
   30.0     0.0    8.1
   60.0     0.0   12.7
   ...

The horizontal domain extents (x_lo, x_hi, y_lo, y_hi) are derived
automatically from the min/max of the terrain data.  The grid dimensions are:

.. code-block:: text

   nx = round((x_hi - x_lo) / dx)
   ny = round((y_hi - y_lo) / dy)
   nz = round((z_hi - z_lo) / dz)

where z_lo = min terrain elevation and z_hi = max terrain elevation +
``domain_height``.

Surface Data File Format (for ``init_mode = surface_data``)
------------------------------------------------------------

When using ``surface_data`` initialization mode (for HRRR-style inputs), the
surface data file must contain one observation per line with columns
**X  Y  Z  USTAR  Z0  U10  V10** (whitespace or comma-separated)::

   # X[m]  Y[m]  Z[m]  USTAR[m/s]  Z0[m]   U10[m/s]  V10[m/s]
   0.0     0.0   0.0   0.35        0.05    8.0       2.0
   300.0   0.0   0.0   0.40        0.10    9.0       1.0
   150.0   260.0 0.0   0.38        0.08    8.5       2.5

where:

* **X, Y, Z**: Spatial coordinates [m] of the observation point
* **USTAR**: Friction velocity u* [m/s] from surface layer similarity theory
* **Z0**: Aerodynamic roughness length [m]
* **U10, V10**: 10-meter wind components [m/s] (eastward, northward)

The solver uses inverse-distance weighting (IDW) to interpolate these parameters
to each grid column, then constructs a vertical log-law profile at each (i,j)
using the local friction velocity and roughness:

.. math::

   u(z) = \frac{u_*}{\kappa}\ln\!\left(\frac{z_\text{agl} + z_0}{z_0}\right)

The wind direction is taken from the U10/V10 components. This mode is designed
for ingesting HRRR (High-Resolution Rapid Refresh) model surface output or
similar gridded analysis products that provide spatially-varying surface
parameters.

Building File Format
--------------------

Buildings are specified in a CSV file with one building per line. Each line
contains six required values and one optional value:

**xmin xmax ymin ymax zmin zmax [rotation_degrees]** (in metres)

Lines beginning with ``#`` are comments::

   # xmin  xmax  ymin  ymax  zmin  zmax  [rotation]
   40.0    60.0  40.0  60.0  0.0   30.0           # Building 1: grid-aligned
   100.0   140.0 60.0  80.0  0.0   50.0  45.0     # Building 2: rotated 45°

**Rotation angle**:

The optional 7th column specifies the building rotation angle in degrees,
counter-clockwise from the x-axis. If omitted, the building is assumed to be
grid-aligned (0°). The rotation angle affects how the effective building
dimensions are projected onto the wind direction for wake modeling.

Buildings are treated as solid obstacles — cells where the cell center falls
inside a building (x in [xmin, xmax] and y in [ymin, ymax]) and below the
building top (z_phys < zmax) are masked (velocity set to zero). The vertical
domain automatically extends to accommodate the tallest building.

Terrain-Aligned Extraction
---------------------------

The solver can extract wind data at specific heights above ground level (AGL) and write
them to CSV files for post-processing.

Single Height Extraction
~~~~~~~~~~~~~~~~~~~~~~~~~

To extract wind at a single height (e.g., 10 m AGL)::

    extract_agl  = 10.0
    extract_file = wind_10m.csv

To extract at a specific k-index (vertical cell)::

    extract_k    = 5
    extract_file = wind_k5.csv

Multi-Height Extraction
~~~~~~~~~~~~~~~~~~~~~~~

You can extract multiple heights in a single run by providing space-separated values::

    extract_agl  = 10.0 50.0 100.0 200.0
    extract_file = wind_extract.csv

This will create four files:

- ``wind_extract_10m.csv``   — wind at 10 m AGL
- ``wind_extract_50m.csv``   — wind at 50 m AGL  
- ``wind_extract_100m.csv``  — wind at 100 m AGL
- ``wind_extract_200m.csv``  — wind at 200 m AGL

Each CSV file contains columns::

    x, y, z_terrain, z_physical, z_agl, u, v, w, speed

Where:

- ``x, y`` = horizontal coordinates [m]
- ``z_terrain`` = local terrain elevation [m]
- ``z_physical`` = physical height of the extraction plane [m]
- ``z_agl`` = height above ground level for this column [m]
- ``u, v, w`` = velocity components [m/s]
- ``speed`` = total velocity magnitude [m/s]

**Use Cases:**

- Standard meteorology heights (10 m, 100 m, 200 m)
- Wind turbine hub heights (80 m, 120 m, 150 m)
- Aviation heights (50 m, 100 m, 150 m, 300 m)
- Multi-level validation against weather station data

**Example:**

See ``regtest/multiheight_extraction/inputs.i`` for a complete example.

Output Files
------------

**AMReX plotfile** (``plt_wind``)

The output plotfile contains the following cell-centred components:

.. list-table::
   :header-rows: 1
   :widths: 22 78

   * - Variable
     - Description
   * - ``u``
     - Corrected x-wind [m/s]
   * - ``v``
     - Corrected y-wind [m/s]
   * - ``w``
     - Corrected z-wind [m/s]
   * - ``vel_magnitude``
     - Wind speed |**u**| [m/s]
   * - ``u0``
     - Initial log-law x-wind [m/s]
   * - ``v0``
     - Initial log-law y-wind [m/s]
   * - ``w0``
     - Initial log-law z-wind [m/s]
   * - ``lambda``
     - Lagrange multiplier λ [m²/s]
   * - ``div_before``
     - ∇·\ **u**₀ before correction [s⁻¹]
   * - ``div_after``
     - ∇·\ **u** after correction [s⁻¹]
   * - ``terrain_z``
     - Terrain elevation at the column centre [m]

Plotfiles can be visualised with VisIt or ParaView (AMReX reader plugin).

**Terrain-aligned CSV slice** (``wind_extract.csv``)

When ``extract_agl`` or ``extract_k`` is set, a 2-D CSV slice is written with
columns::

    x, y, z_terrain, z_physical, z_agl, u, v, w, speed

Typical Workflow
----------------

1. **Prepare terrain** — create a CSV from a DEM or generate synthetically::

       # Gaussian hill example (already provided in regtest/)
       cat regtest/gaussian_hill/terrain.csv

2. **Write an inputs file** (or reuse a regtest one):

   .. code-block:: text

       terrain_file = terrain.csv
       U_ref = 8.0
       z0    = 0.05
       dx    = 30.0
       dy    = 30.0
       dz    = 10.0
       domain_height = 300.0
       extract_agl   = 10.0
       extract_file  = wind_10m.csv
       plot_file     = plt_wind

3. **Run the solver**::

       ./build/wind_solver inputs.i

4. **Check convergence** — MLMG prints iteration residuals when
   ``mlmg_verbose ≥ 1``.

5. **Visualise** — open the plotfile in VisIt or ParaView, or load the CSV
   extract in Python::

       import pandas as pd
       import matplotlib.pyplot as plt
       df = pd.read_csv("wind_10m.csv")
       plt.quiver(df.x, df.y, df.u, df.v)
       plt.show()

Performance Tuning
------------------

The solver reports detailed timing for each phase to help identify bottlenecks.

**MLMG Solver Tuning**

For faster solves on well-conditioned problems (relatively flat terrain, moderate anisotropy)::

    mlmg_max_iter = 100
    mlmg_max_fmg_iter = 10
    mlmg_pre_smooth = 8
    mlmg_post_smooth = 8
    mlmg_bottom_solver = bicgstab

For highly anisotropic problems (α_h/α_v > 100, strong vertical stratification)::

    mlmg_bottom_solver = bicgstab  # or cg
    mlmg_pre_smooth = 12
    mlmg_post_smooth = 12

**GPU Optimization**

When running on GPUs (CUDA, HIP, or SYCL), increase box size for better cache utilization::

    max_grid_size = 128  # or 64, 256 depending on GPU memory

**Parallel Scaling**

For large MPI runs, adjust box size based on the number of ranks to ensure good load balance.
Use ``mlmg_verbose = 2`` to see per-rank box distribution.
