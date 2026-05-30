.. _puff:

Gaussian Puff Model
===================

The Gaussian puff model is a passive dispersion parameterization that can be coupled with the mass-consistent wind solver. It models pollutant dispersion using discrete Gaussian-shaped puffs that are emitted from a source, drift with the wind, and grow due to diffusion.

Physical Model
--------------

The Gaussian puff model represents the concentration field as a superposition of Gaussian puffs:

.. math::

    C(\mathbf{r}, t) = \sum_i C_i(\mathbf{r}, t)

where each puff has a concentration profile:

.. math::

    C_i(x, y, z, t) = \frac{m_i}{(2\pi)^{3/2} \sigma_{x,i} \sigma_{y,i} \sigma_{z,i}} \exp\left(-\frac{(x-x_i)^2}{2\sigma_{x,i}^2} - \frac{(y-y_i)^2}{2\sigma_{y,i}^2} - \frac{(z-z_i)^2}{2\sigma_{z,i}^2}\right)

where:
- :math:`m_i` is the mass (or strength) of puff :math:`i`
- :math:`(x_i, y_i, z_i)` is the puff center position
- :math:`\sigma_{x,i}, \sigma_{y,i}, \sigma_{z,i}` are the puff standard deviations

Puff Evolution
---------------

Each puff evolves according to three processes:

**1. Advection (Drift with Wind)**

The puff center drifts with the local wind velocity using simple Euler time stepping:

.. math::

    \frac{d\mathbf{r}_i}{dt} = \mathbf{u}(\mathbf{r}_i, t)

In practice, for a uniform wind field:

.. math::

    x_i^{n+1} = x_i^n + u \Delta t

**2. Diffusion (Growth)**

The puff grows over time due to turbulent diffusion. The growth rate follows:

.. math::

    \sigma(t) = \sqrt{\sigma_0^2 + 2K \cdot t}

where :math:`K` is the diffusivity (horizontal :math:`K_h` or vertical :math:`K_v`).

**3. Mass Conservation**

The total mass in each puff is conserved:

.. math::

    m_i(t) = m_i(0)

Implementation Details
----------------------

**Data Structures**

The model uses two main structures:

.. code-block:: cpp

    struct Puff {
        Real x, y, z;            // Puff center position [m]
        Real sigma_y, sigma_z;   // Gaussian standard deviations [m]
        Real mass;               // Mass/strength of puff [units]
        Real age;                // Current age of puff [s]
        bool active;             // Whether puff is still modeled
    };

    struct PuffParams {
        bool enabled;            // Enable puff model
        Real source_x, y, z;     // Source location [m]
        Real emission_rate;      // [units/s]
        Real emission_duration;  // [s]
        Real K_h, K_v;           // Diffusivity [m²/s]
        Real sigma_y0, z0;       // Initial puff size [m]
    };

**Time Stepping**

The model uses a simple explicit time-stepping loop:

.. code-block:: cpp

    for each time step:
        if (still emitting):
            create new puff at source
        
        for each puff:
            advect with wind (x += u*dt)
            grow due to diffusion (σ = √(σ0² + 2K*t))
            update age and deactivate if necessary
        
        compute concentration field (superposition of all puffs)
        write output

**Concentration Gridding**

Concentrations are computed at grid points by evaluating the 3D Gaussian formula for each puff and summing:

.. code-block:: cpp

    C_grid(i,j,k) = Σ_puff gaussian_puff_concentration(x_ijk, y_ijk, z_ijk, puff)

Input Parameters
----------------

Core Parameters
^^^^^^^^^^^^^^^

.. code-block:: ini

    # Enable/disable the puff model
    enable_puff = true / false

    # Source location [m]
    source_x = 150.0
    source_y = 150.0
    source_z = 10.0

    # Emission parameters
    emission_rate = 1.0           # [units/s]
    emission_duration = 50.0      # [s]

Diffusivity and Growth
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: ini

    # Turbulent diffusion parameters
    K_h = 1.0                     # Horizontal [m²/s]
    K_v = 0.5                     # Vertical [m²/s]

    # Initial puff size (σ0)
    sigma_y0 = 1.0                # Lateral [m]
    sigma_z0 = 1.0                # Vertical [m]

Wind Field
^^^^^^^^^^

For uniform wind tests:

.. code-block:: ini

    U_wind = 10.0      # x-component [m/s]
    V_wind = 0.0       # y-component [m/s]
    W_wind = 0.0       # z-component [m/s]

Domain and Grid
^^^^^^^^^^^^^^^

.. code-block:: ini

    # Domain extent [m]
    xmin = 0.0,    xmax = 300.0
    ymin = 0.0,    ymax = 300.0
    zmin = 0.0,    zmax = 100.0

    # Concentration grid resolution [m]
    dx = 10.0
    dy = 10.0
    dz = 10.0

Time Stepping
^^^^^^^^^^^^^

.. code-block:: ini

    # Time integration
    dt_puff = 0.5              # Time step [s]
    n_steps_puff = 100         # Total steps
    output_freq_puff = 10      # Output every N steps

Output
^^^^^^

.. code-block:: ini

    puff_output = puff_concentration.csv   # Output file prefix

Usage
-----

**Running the Puff Solver Standalone**

After building:

.. code-block:: bash

    # Build
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel

    # Run puff solver
    ./build/puff_solver regtest/puff_gaussian/inputs.i

**Example Input File**

See ``regtest/puff_gaussian/inputs.i`` for a complete example:

.. code-block:: ini

    enable_puff = true
    source_x = 150.0
    source_y = 150.0
    source_z = 10.0
    emission_rate = 1.0
    emission_duration = 50.0
    K_h = 1.0
    K_v = 0.5
    sigma_y0 = 1.0
    sigma_z0 = 1.0
    U_wind = 10.0
    V_wind = 0.0
    W_wind = 0.0
    dt_puff = 0.5
    n_steps_puff = 100
    output_freq_puff = 10

Output Files
^^^^^^^^^^^^

The puff solver writes concentration snapshots in CSV format:

::

    puff_concentration.csv_step0
    puff_concentration.csv_step10
    puff_concentration.csv_step20
    ...

Each file contains columns: ``x, y, z, C`` (x, y, z coordinates in meters and concentration in units/m³)

Validation and Testing
----------------------

**Analytical Solution (Steady Plume)**

For a continuous point source in a uniform wind field, the steady-state Gaussian plume solution is:

.. math::

    C(x, y, z) = \frac{Q}{2\pi u \sigma_y \sigma_z} \exp\left(-\frac{y^2}{2\sigma_y^2}\right) 
                 \left[\exp\left(-\frac{z^2}{2\sigma_z^2}\right) + \exp\left(-\frac{(2H-z)^2}{2\sigma_z^2}\right)\right]

where :math:`Q` is the emission rate, :math:`u` is the wind speed, and :math:`H` is the source height.

The puff model should asymptotically approach this solution for long simulation times when using:

.. code-block:: ini

    emission_rate = Q              # Total emission
    emission_duration = very long  # Simulate continuous source

**Expected Behavior**

1. **Plume Asymmetry**: The plume should drift downwind (positive x direction in this example)
2. **Lateral Spreading**: The plume should spread equally in y-direction (isotropic lateral diffusion)
3. **Vertical Asymmetry**: If :math:`K_v < K_h`, vertical spreading should be slower than lateral
4. **Reflection**: The plume should reflect off the ground (z=0) - currently NOT implemented

Terrain, Building, and Canopy Integration
------------------------------------------

**NEW**: The puff model now includes comprehensive support for terrain, buildings, and tree canopy effects.

Terrain Awareness
~~~~~~~~~~~~~~~~~

The model implements ground reflection using the image source method:

* **Terrain data**: Reads elevation from CSV file (same format as wind solver)
* **Ground reflection**: Puffs reflect when hitting terrain surface
* **Image source method**: Mirror puffs below ground satisfy zero-flux boundary condition

.. code-block:: ini

    # Enable terrain awareness
    terrain_file = terrain.csv
    enable_terrain_reflection = true
    use_image_source = true

Building Awareness
~~~~~~~~~~~~~~~~~~

Buildings are handled through collision detection and wake-enhanced diffusivity:

* **Building masking**: Puffs inside buildings are deactivated
* **Wake zones**: Röckle (1990) model identifies cavity and far-wake regions
* **Enhanced mixing**: Turbulent diffusivity increased in building wakes

.. code-block:: ini

    # Enable building awareness
    building_file = buildings.csv
    enable_building_masking = true
    enable_wake_diffusivity = true
    wake_enhancement_cavity = 3.0    # 3x diffusivity in cavity
    wake_enhancement_far = 1.5       # 1.5x in far wake

Canopy Effects
~~~~~~~~~~~~~~

Tree canopy modifies dispersion through enhanced vertical mixing and deposition:

* **Enhanced diffusivity**: Vertical mixing increased due to canopy turbulence
* **Horizontal sheltering**: Reduced lateral diffusion within canopy
* **Dry deposition**: Optional mass removal for particles/aerosols

.. code-block:: ini

    # Enable canopy effects
    enable_canopy_effects = true
    canopy_height = 20.0
    frontal_area_index = 0.25
    canopy_enhancement_factor = 3.0  # Vertical K enhancement
    canopy_sheltering_factor = 0.7   # Horizontal K reduction
    
    # Optional deposition
    enable_canopy_deposition = true
    deposition_velocity = 0.01       # [m/s]

Test Cases
~~~~~~~~~~

New regression tests demonstrate terrain/building/canopy integration:

* **regtest/puff_terrain**: Ground reflection over Gaussian hill
* **regtest/puff_buildings**: Wake-enhanced dispersion around buildings
* **regtest/puff_canopy**: Canopy diffusivity and deposition
* **regtest/puff_coupled_full**: All features combined (terrain + buildings + canopy)

Remaining Limitations
---------------------

**Current Limitations**

1. **Nearest-neighbor velocity interpolation**: Should use trilinear interpolation for spatially-varying wind fields
2. **No chemical decay**: No radioactive or chemical decay modeled
3. **No plume rise**: No buoyancy effects for heated sources
4. **Constant canopy properties**: Canopy parameters are spatially uniform

**Future Extensions**

1. Add gravitational settling for particles (size-dependent)
2. Add chemical decay (first-order kinetics)
3. Add plume rise for heated/buoyant sources
4. Couple with wind field from massconsistent solver (read from plotfile)
5. Add Python API for coupling with fire/chemistry models
6. Support spatially-varying canopy properties

References
----------

* Pasquill, F., & Gifford, F.A. (1961). The estimation of the dispersion of wind-borne 
  material from industrial and other sources. *Meteorological Magazine*, 90(1066), 33-49.

* Hanna, S.R., Briggs, G.A., & Hosker, R.P. (1982). *Handbook on atmospheric diffusion*. 
  U.S. Department of Energy, DOE/TIC-11223.

* Röckle, R. (1990). Bestimmung der Strömungsverhältnisse im Bereich komplexer 
  Bebauungsstrukturen. Dissertation, Technischen Hochschule Darmstadt.

* Pardyjak, E.R., & Brown, M.J. (2001). QUIC-URB v. 1.1: Theory and User's Guide. 
  Los Alamos National Laboratory, LA-UR-01-4228.

* Stohl, A., et al. (2005). Technical note: The Lagrangian particle dispersion model 
  FLEXPART version 6.2. *Atmospheric Chemistry and Physics*, 5, 2461-2474.

* Shaw, R.H., & Pereira, A.R. (1982). Aerodynamic roughness of a plant canopy: 
  A numerical experiment. *Agricultural Meteorology*, 26, 51-65.

* MacDonald, R.W., Griffiths, R.F., & Hall, D.J. (2000). A comparison of results from 
  scaled field and wind tunnel modelling of dispersion in arrays of obstacles. 
  *Atmospheric Environment*, 34(20), 3845-3862.

