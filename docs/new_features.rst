.. _new_features:

Recent Features
===============

This page documents recently added features to the massconsistent_amr wind solver.

Multi-Height Wind Field Extraction
-----------------------------------

**Added:** May 2026  
**Use Case:** Meteorology, wind energy, aviation applications

The wind solver can now extract wind fields at multiple heights above ground level (AGL)
in a single simulation run. This is useful for:

- Standard meteorological heights (10 m, 100 m, 200 m)
- Wind turbine hub heights (80 m, 120 m, 150 m)
- Aviation analysis at multiple flight levels
- Multi-level validation against weather station data

Configuration
~~~~~~~~~~~~~

In your input file, specify multiple extraction heights using space-separated values::

    # Extract wind at 10m, 50m, 100m, and 200m AGL
    extract_agl  = 10.0 50.0 100.0 200.0
    extract_file = wind_extract.csv

The solver will create separate CSV files for each height:

- ``wind_extract_10m.csv``
- ``wind_extract_50m.csv``
- ``wind_extract_100m.csv``
- ``wind_extract_200m.csv``

Each file contains the standard columns::

    x, y, z_terrain, z_physical, z_agl, u, v, w, speed

Output columns:

- **x, y**: Horizontal coordinates [m]
- **z_terrain**: Local terrain elevation [m above sea level]
- **z_physical**: Physical height of the extraction plane [m above sea level]
- **z_agl**: Height above ground level for this grid column [m]
- **u, v, w**: Velocity components [m/s]
- **speed**: Total velocity magnitude [m/s]

Example
~~~~~~~

See the regression test ``regtest/multiheight_extraction/inputs.i`` for a complete
working example that extracts wind at 10 m, 50 m, and 100 m AGL over a Gaussian hill terrain.

Backward Compatibility
~~~~~~~~~~~~~~~~~~~~~~

Single-height extraction still works::

    # Single height extraction
    extract_agl  = 15.0
    extract_file = wind_15m.csv

This creates a single file ``wind_15m.csv`` (or ``wind_extract.csv`` if only one height is specified).

Implementation
~~~~~~~~~~~~~~

- Uses AMReX ``ParmParse::getarr()`` to read array values
- Automatic filename generation with height suffix
- Extraction loop processes all heights efficiently
- GPU-safe implementation with proper synchronization
- MPI-parallel output with sequential file writes

Performance
~~~~~~~~~~~

Extracting multiple heights adds minimal overhead:

- Each extraction is a single k-plane copy operation
- Total runtime increase: ~1-2% per additional height
- I/O is the primary cost (not computation)

For example, extracting 4 heights instead of 1 adds approximately 3-6% to total runtime.

Phase 1 Physics Features (June 2026)
-------------------------------------

**Added:** June 2026
**Use Case:** Enhanced wind modeling, atmospheric stability, diagnostics

Four new physics features were added in Phase 1 to extend the solver capabilities:

1. Power-Law Wind Profile
~~~~~~~~~~~~~~~~~~~~~~~~~

Alternative initialization mode using power-law profile instead of log-law::

    init_mode = powerlaw
    U_ref = 10.0
    V_ref = 0.0
    z_ref = 10.0
    powerlaw_exponent = 0.143  # ~1/7 for neutral conditions

The power-law profile is: u(z) = U_ref × (z/z_ref)^α

This is commonly used in wind energy and atmospheric applications, particularly for
neutral atmospheric conditions. Typical exponent values: 0.1-0.4 (neutral ≈ 1/7 = 0.143).

2. Height-Dependent Anisotropy
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Vertical anisotropy coefficient α_v can now vary linearly with height::

    use_height_dependent_alpha_v = true
    alpha_v_surface = 0.5   # Strong vertical adjustment near surface
    alpha_v_top = 2.0       # Weaker vertical adjustment aloft

This allows finer control over mass-consistency adjustment behavior:
- Lower α_v near surface → stronger vertical velocity adjustment (preserves horizontal winds)
- Higher α_v aloft → weaker vertical adjustment (allows more horizontal wind adjustment)

Useful for complex terrain where near-surface flow should closely match terrain-following
log-law profiles while allowing more freedom aloft.

3. Surface Heat Flux Diagnostic
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sensible heat flux Q_H is now computed and output for each grid cell::

    Q_H = ρ c_p u* θ*  [W/m²]

Where:
- ρ = air density (1.225 kg/m³ at sea level)
- c_p = specific heat at constant pressure (1005 J/(kg·K))
- u* = friction velocity (diagnosed from local wind and roughness)
- θ* = characteristic temperature scale (0.1 K for neutral conditions)

Available in plotfile output as ``heat_flux`` variable. Useful for coupling with
fire spread models and atmospheric boundary layer studies.

4. Drag Coefficient Diagnostic
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Drag coefficient C_d is computed for each grid cell::

    C_d = (κ / ln(z/z0))²

Where:
- κ = von Kármán constant (0.41)
- z = height above ground
- z0 = aerodynamic roughness length

Available in plotfile output as ``drag_coeff`` variable. Useful for surface flux
parameterizations and coupling with atmospheric models.

Implementation Notes
~~~~~~~~~~~~~~~~~~~~

- All four features work with existing terrain, building, and canopy models
- Power-law mode compatible with all output and extraction options
- Height-dependent α_v uses linear interpolation between surface and top values
- Diagnostics computed from corrected wind field (after mass-consistency)
- Minimal performance overhead: <1% for power-law, ~2% for height-dependent α_v

Planned Features
----------------

The following features are planned for future releases:

Checkpoint/Restart
~~~~~~~~~~~~~~~~~~

- Save solver state for faster parameter sweeps
- Resume interrupted simulations
- Expected: Q3 2026

Position-Dependent Roughness
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Specify spatially-varying surface roughness
- Support land-use transitions (forest/urban/water)
- Expected: Q4 2026

Thermal Stratification
~~~~~~~~~~~~~~~~~~~~~~~

- Monin-Obukhov stability corrections
- Stable/unstable atmospheric conditions
- Expected: Q4 2026
