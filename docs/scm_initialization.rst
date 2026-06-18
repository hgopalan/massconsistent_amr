.. _scm_initialization:

Single Column Model (SCM) Wind Profile Initialization
======================================================

Overview
--------

The Single Column Model (SCM) is an initialization mode for the mass-consistent wind solver that enables direct specification of wind speed at a reference height (e.g., meteorological mast height). Instead of using log-law assumptions, the SCM performs a time-dependent 1D simulation to determine the geostrophic wind required to produce the specified wind speed at the reference height.

Motivation
----------

Traditional initialization using log-law is based on strong assumptions about atmospheric stability and may not be accurate in complex terrain. The SCM approach is more physics-based and handles:

- Coriolis forcing and geostrophic balance
- Turbulent diffusion with variable eddy viscosity  
- Temperature profiles with user-defined lapse rates
- Turbulent kinetic energy evolution
- Stratification effects on mixing

The implementation is based on the `hrrr_1dsolver_terrain.py <https://github.com/hgopalan/onedterrainsolver>`_ solver, which provides a physics-based 1D column model that solves wind momentum equations with Coriolis and turbulent diffusion, evolves temperature with diffusive mixing, and computes eddy viscosity from TKE and mixing length using Monin-Obukhov similarity theory for the surface layer.

Algorithm
---------

Geostrophic Wind Recursion
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The algorithm iteratively finds geostrophic wind components :math:`(U_g, V_g)` such that a specified wind speed is achieved at the reference height:

1. **Initial Guess**: Convert target wind speed and direction to :math:`(U_g, V_g)`
2. **Run 1D SCM**: Execute time-dependent simulation with current :math:`(U_g, V_g)`
3. **Extract Wind Speed**: Get wind speed at reference height from converged profile
4. **Scale Update**: Adjust :math:`(U_g, V_g)` to reduce error:

   .. math::
   
      U_{g,new} = U_g \sqrt{\frac{U_{target}}{U_{current}}}

5. **Iterate**: Repeat until convergence (typical: 10-20 iterations)

1D Profile Evolution
~~~~~~~~~~~~~~~~~~~~

The 1D SCM performs time-dependent simulation for each vertical level:

**Initialization**:
- Uniform horizontal winds: :math:`(u, v) = (U_g, V_g)` everywhere
- Linear temperature profile: :math:`T(z) = T_{ref} - \Gamma \cdot z`
- Small initial TKE: :math:`\text{tke} = 0.1 \text{ m}^2/\text{s}^2`

**Time-stepping** (for each interior level :math:`i`):

.. math::

   \frac{\partial u}{\partial t} &= \nu_t \frac{\partial^2 u}{\partial z^2} + \frac{1}{2\Delta z}\frac{\partial \nu_t}{\partial z}\frac{\partial u}{\partial z} + f v - f V_g + c(U_g - u)/20

   \frac{\partial v}{\partial t} &= \nu_t \frac{\partial^2 v}{\partial z^2} + \frac{1}{2\Delta z}\frac{\partial \nu_t}{\partial z}\frac{\partial v}{\partial z} - f u + f U_g + c(V_g - v)/20

   \frac{\partial T}{\partial t} &= \frac{\partial}{\partial z}\left(\frac{\nu_t}{\sigma_t}\frac{\partial T}{\partial z}\right)

   \frac{\partial e}{\partial t} &= P - \varepsilon + D

Where:
- :math:`f` = Coriolis parameter (depends on latitude)
- :math:`\nu_t` = eddy viscosity (m²/s)
- :math:`c` = height-dependent damping coefficient (0 at top, 1 near surface)
- :math:`P` = shear production of TKE
- :math:`\varepsilon` = TKE dissipation
- :math:`D` = diffusive transport

**Adaptive Time-stepping**:

.. math::

   \Delta t = 0.8 \frac{\Delta z}{\max(|U|)}

**Convergence Check**: Stop when wind field changes by less than 0.01 m/s

3D Mapping
~~~~~~~~~~

The converged 1D profile is mapped to 3D terrain-aligned coordinates:

For each grid point :math:`(i, j, k)`:

1. Compute height above local terrain: :math:`z_{\text{agl}} = z_{\text{phys}} - h_{\text{terrain}}(i,j)`
2. Find nearest level in 1D profile
3. Assign velocity and temperature from 1D profile

Boundary Conditions
-------------------

Wind Momentum Equations
~~~~~~~~~~~~~~~~~~~~~~~

The momentum equations include the following physical terms:

.. math::

   \text{Diffusion} &: \nu_t \frac{\partial^2 u}{\partial z^2} + \frac{1}{2\Delta z}\frac{\partial \nu_t}{\partial z}\frac{\partial u}{\partial z}

   \text{Coriolis} &: f v \quad \text{(and} \, -f u \text{ for } v\text{)}

   \text{Geostrophic Balance} &: -f V_g \quad \text{(and} \, f U_g \text{ for } v\text{)}

   \text{Relaxation Damping} &: c(U_g - u)/20

Height-dependent Damping Coefficient
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The relaxation coefficient varies smoothly with height to damp oscillations near the domain top:

.. math::

   c(z) = \begin{cases}
     0.0 & \text{if } z_{\text{top}} - z > 150 \text{ m} \\
     0.5\cos\left(\frac{\pi(z_{\text{top}} - 100 - z)}{50}\right) + 0.5 & \text{if } 100 \leq z_{\text{top}} - z \leq 150 \text{ m} \\
     1.0 & \text{if } z_{\text{top}} - z < 100 \text{ m}
   \end{cases}

Surface Layer (Monin-Obukhov)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

At the lowest active level:

.. math::

   u_* &= \frac{\kappa M_1}{\ln\left(\frac{z_1 + z_0}{z_0}\right)}

   \nu_t &= \frac{u_* \kappa z_0}{\phi_m}

Where :math:`M_1` is the wind speed at the first level and :math:`\phi_m` is the Monin-Obukhov stability function (currently simplified to neutral).

Eddy Viscosity and Mixing Length
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. math::

   \nu_t = c_\mu \sqrt{\text{tke}} \cdot l_{\text{scale}}

   l_{\text{scale}} = \frac{1}{\sqrt{1/l_{\text{shear}}^2 + 1/l_{\text{max}}^2}} \quad \text{(neutral/unstable)}

   l_{\text{shear}} = \kappa(z - z_{\text{lower}})

   l_{\text{max}} = \frac{0.00027\sqrt{U_g^2 + V_g^2}}{|f|} \quad \text{(Blackadar length scale)}

Temperature Equation
~~~~~~~~~~~~~~~~~~~~~

.. math::

   \frac{\partial T}{\partial t} = \frac{\partial}{\partial z}\left(\frac{\nu_t}{\sigma_t}\frac{\partial T}{\partial z}\right)

with :math:`\sigma_t = 1.0` (Prandtl number for heat).

TKE (1-equation model)
~~~~~~~~~~~~~~~~~~~~~~

.. math::

   \frac{\partial \text{tke}}{\partial t} = \underbrace{\nu_t\left[\left(\frac{\partial u}{\partial z}\right)^2 + \left(\frac{\partial v}{\partial z}\right)^2\right]}_{\text{production}} - \underbrace{C_\varepsilon \frac{\text{tke}^{3/2}}{l_{\text{scale}}}}_{\text{dissipation}} + \underbrace{\frac{\partial}{\partial z}\left(\nu_t\frac{\partial \text{tke}}{\partial z}\right)}_{\text{diffusion}}

where :math:`C_\varepsilon \approx 1.92`.

Usage
-----

Configuration Parameters
~~~~~~~~~~~~~~~~~~~~~~~~

Add the following parameters to your inputs file:

.. code-block:: bash

   # Use SCM mode
   init_mode = scm

   # Wind specification at reference height
   scm_wind_speed = 10.0           # Wind speed [m/s]
   scm_wind_direction = 270.0      # Direction [degrees, 0=N, 90=E, 180=S, 270=W]
   scm_ref_height = 10.0           # Reference height [m AGL]

   # Temperature profile
   scm_ref_temperature = 288.15    # Surface temperature [K]
   scm_lapse_rate = 0.0065         # Lapse rate [K/m]

   # 1D SCM domain
   scm_domain_height = 4000.0      # Domain height [m]
   scm_dz = 4.0                    # Grid spacing [m]

C++ API
~~~~~~~

After initialization, the computed geostrophic wind components are stored in the solver state:

.. code-block:: cpp

   wind_solver.initialize("scm_inputs.i");
   wind_solver.solve();
   
   // Access geostrophic wind
   Real ug = wind_solver.get_scm_ug();
   Real vg = wind_solver.get_scm_vg();

Python API
~~~~~~~~~~

.. code-block:: python

   from wind_solver import WindSolver

   # Initialize solver with SCM mode
   wind = WindSolver("scm_inputs.i")

   # Solve for wind field
   wind.solve()

   # Extract velocity at specified height
   vel_10m = wind.get_velocity_at_agl(10.0)

   # Save results
   wind.write_plotfile("plt_scm")
   wind.finalize()

Parameters
----------

.. list-table:: SCM Configuration Parameters
   :header-rows: 1
   :widths: 25 10 15 50

   * - Parameter
     - Type
     - Default
     - Description
   * - ``scm_wind_speed``
     - Real
     - 10.0
     - Wind speed at reference height [m/s]
   * - ``scm_wind_direction``
     - Real
     - 270.0
     - Wind direction [degrees, 0=N, 90=E, 180=S, 270=W]
   * - ``scm_ref_height``
     - Real
     - 10.0
     - Height where wind speed is specified [m AGL]
   * - ``scm_ref_temperature``
     - Real
     - 288.15
     - Reference temperature at surface [K]
   * - ``scm_lapse_rate``
     - Real
     - 0.0065
     - Temperature lapse rate [K/m]
   * - ``scm_domain_height``
     - Real
     - 4000.0
     - Domain height for 1D SCM [m]
   * - ``scm_dz``
     - Real
     - 4.0
     - Vertical grid spacing for 1D SCM [m]

Output
------

After convergence, the following geostrophic wind components are computed and stored:

- ``scm_ug`` — Geostrophic u-component [m/s]
- ``scm_vg`` — Geostrophic v-component [m/s]

These values can be accessed via the C++ API or examined in the solver output.

Performance Considerations
--------------------------

1. **1D SCM Runtime**: Completes in seconds to minutes depending on convergence
2. **No Additional Cost to 3D Solve**: 1D profile is simply mapped to 3D MultiFab
3. **Recommended Parameters**:
   - Vertical resolution: :math:`\Delta z = 4` m
   - Domain height: 4 km (typical PBL height + buffer)
   - Reference height: 10-100 m AGL

Example Configuration
---------------------

See the ``regtest/physics/scm_initialization/inputs.i`` file in the repository for a complete example configuration.

References
----------

1. Högström, U. "Review of some basic characteristics of the atmospheric surface layer." *Boundary-Layer Meteorology* 78.3 (1996): 215-246.

2. Blackadar, A. K. "The vertical distribution of wind and turbulent exchange in a neutral atmosphere." *Journal of Geophysical Research* 67.8 (1962): 3095-3102.

3. Businger, J. A., et al. "Flux-profile relationships in the atmospheric surface layer." *Journal of the Atmospheric Sciences* 28.2 (1971): 181-189.
