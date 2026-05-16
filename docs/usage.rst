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
   * - ``alpha_h``
     - ``1.0``
     - Horizontal Lagrange anisotropy coefficient α_h.
   * - ``alpha_v``
     - ``1.0``
     - Vertical Lagrange anisotropy coefficient α_v.
   * - ``mlmg_verbose``
     - ``1``
     - MLMG solver verbosity (0 = silent, 4 = maximum).
   * - ``tol_rel``
     - ``1.0e-8``
     - MLMG relative convergence tolerance.
   * - ``max_grid_size``
     - ``32``
     - Maximum AMReX box size per spatial dimension.
   * - ``deriv_method``
     - ``central``
     - Method for computing derivatives: ``central`` (2nd order, one-sided at boundaries),
       ``weno3`` (3rd order WENO), or ``weno5`` (5th order WENO).
   * - ``plot_file``
     - ``plt_wind``
     - Output plotfile prefix.
   * - ``extract_agl``
     - ``-1.0``
     - Sample the corrected wind at this AGL height [m] and write a CSV
       slice.  Negative value disables extraction.
   * - ``extract_k``
     - ``-1``
     - Alternative: sample at explicit k-index (0 = lowest level).
       ``extract_agl`` takes priority when both are set.
   * - ``extract_file``
     - ``wind_extract.csv``
     - Output filename for the terrain-aligned CSV slice.

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
