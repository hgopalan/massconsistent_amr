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

Advanced Stability Physics (Neutral, Stable, Unstable)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The SCM now supports comprehensive physics for three atmospheric boundary layer stability classes through the following enhancements:

**Physical Constants Used**

- Air density: ρ = 1.225 kg/m³ (standard sea level value at 15°C; may require adjustment for different altitudes or temperatures)
- Specific heat of air: c_p = 1005 J/(kg·K)
- Gravitational acceleration: g = 9.81 m/s²
- von Kármán constant: κ = 0.41

**1. Richardson Number and Stratification Metrics**

The Richardson number (Ri) is computed at each level to quantify static stability:

.. math::

   Ri = \frac{N^2}{(dU/dz)^2}

where the Brunt-Väisälä frequency squared is:

.. math::

   N^2 = \frac{g}{T}\frac{\partial T}{\partial z}

Positive Ri indicates stable stratification (TKE suppressed), while negative Ri indicates unstable stratification (TKE enhanced).

**2. Stability-Dependent Mixing Length**

The mixing length varies with stability class:

- **Stable (z/L > 0.01)**: Reduced mixing length (Holtslag & Boville 1993)

  .. math::

     l_s = \frac{l_m}{\sqrt{1 + 5(z/L)}}

  Suppresses turbulent mixing and vertical transport.

- **Unstable (z/L < -0.01)**: Enhanced mixing length (Deardorff 1966)

  .. math::

     l_u = l_m \cdot (1 - 8(z/L))^{1/3}

  Promotes vigorous convective mixing and thermals.

- **Neutral (|z/L| ≤ 0.01)**: Standard Blackadar length scale

  .. math::

     l_m = \frac{1}{\sqrt{1/l_{\text{shear}}^2 + 1/l_{\text{max}}^2}}

**3. Stability-Dependent Prandtl Number**

The turbulent Prandtl number (σ_t) varies with stability based on Högström (1988) and Beljaars & Holtslag (1989):

- **Stable**: :math:`\sigma_t = 1.0 \cdot (1 + 2.0 \cdot z/L)` → Reduced heat transfer
- **Unstable**: :math:`\sigma_t = 1.0 / (1 + 2.0 \cdot |z/L|)` → Enhanced heat transfer
- **Neutral**: :math:`\sigma_t = 1.0`

**4. Stability-Dependent TKE Coefficient**

The model coefficient (c_μ) for eddy viscosity is modified by Richardson number:

- **Stable (Ri > 0)**: :math:`c_\mu = 0.1 \cdot \frac{1}{1 + 10 \cdot Ri}` → Reduced turbulence production
- **Unstable (Ri < 0)**: :math:`c_\mu = 0.1 \cdot \sqrt{1 - 5 \cdot Ri}` → Enhanced turbulence production
- **Neutral (Ri ≈ 0)**: :math:`c_\mu = 0.1`

**5. Buoyancy Production in TKE**

The TKE equation now includes explicit buoyancy production term:

.. math::

   \frac{\partial \text{tke}}{\partial t} = P + B + D - \varepsilon

where the buoyancy production is:

.. math::

   B = -\frac{g}{T} \cdot \frac{\nu_t}{\sigma_t} \cdot \frac{\partial T}{\partial z}

- Positive (B > 0) in unstable conditions → TKE enhancement via convection
- Negative (B < 0) in stable conditions → TKE suppression via stratification

**6. Dynamic Monin-Obukhov Length**

When heat flux is specified, the Monin-Obukhov length is computed dynamically:

.. math::

   L = -\frac{\rho c_p T u_*^3}{\kappa g Q_h}

where:
- ρ = air density (1.225 kg/m³)
- c_p = specific heat of air (1005 J/(kg·K))
- u_* = friction velocity [m/s]
- Q_h = sensible heat flux [W/m²]
- κ = von Kármán constant (0.41)
- g = gravitational acceleration (9.81 m/s²)

This enables automatic stability classification and application of appropriate physics.

**Physical Mechanism Summary**

+-----------+-------------------+-------------------+-----------------+
| Condition | z/L range         | Mixing Length     | TKE Production  |
+===========+===================+===================+=================+
| Unstable  | z/L < -0.01       | Enhanced (larger) | Positive (B>0)  |
+-----------+-------------------+-------------------+-----------------+
| Neutral   | -0.01 ≤ z/L ≤0.01 | Standard          | Near zero       |
+-----------+-------------------+-------------------+-----------------+
| Stable    | z/L > 0.01        | Reduced (smaller) | Negative (B<0)  |
+-----------+-------------------+-------------------+-----------------+

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

   # Optional stability parameters (default: neutral conditions)
   scm_heat_flux = 0.0             # Surface sensible heat flux [W/m^2] (optional)
   scm_monin_obukhov_length = -1e30    # Monin-Obukhov length [m] (optional, default=-1e30 for neutral)

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
   * - ``scm_heat_flux``
     - Real
     - 0.0
     - Surface sensible heat flux [W/m^2] (optional, 0=neutral)
   * - ``scm_monin_obukhov_length``
     - Real
     - -1e30
     - Monin-Obukhov length [m] (optional, -1e30=neutral)

Atmospheric Stability Models
----------------------------

The SCM now features comprehensive physics for three atmospheric stability regimes: **neutral, stable, and unstable**.

**Neutral ABL**

When ``scm_heat_flux`` = 0 and ``scm_monin_obukhov_length`` = -1e30:

- Standard log-law wind profile applies throughout
- Constant mixing length follows Blackadar scale
- Minimal buoyancy effects on turbulence
- Appropriate for weakly stratified conditions

**Stable ABL** (cold nocturnal conditions)

Specify positive ``scm_monin_obukhov_length`` (e.g., 100 m):

- Reduced mixing length suppresses turbulent mixing
- Increased Prandtl number reduces heat transfer
- Negative buoyancy production dampens TKE
- Wind profile deviates from log-law due to stability
- Results in stronger wind shear near surface

**Unstable ABL** (warm daytime convection)

Specify negative ``scm_monin_obukhov_length`` (e.g., -50 m):

- Enhanced mixing length promotes convective mixing
- Reduced Prandtl number enhances heat transfer
- Positive buoyancy production energizes TKE
- Vigorous vertical mixing and thermals develop
- Results in weaker wind shear, better mixed vertical profiles

**Automatic Stability Classification**

If ``scm_heat_flux`` is specified (non-zero), the model automatically:

1. Computes sensible heat flux from temperature gradients
2. Calculates Monin-Obukhov length: L = -ρ·c_p·T·u*³ / (κ·g·Q_h)
3. Classifies stability based on z/L: stable (z/L > 0.01), neutral (|z/L| ≤ 0.01), unstable (z/L < -0.01)
4. Applies appropriate mixing length and TKE modifications

Atmospheric Stability
---------------------

The SCM model supports optional atmospheric stability corrections via Monin-Obukhov similarity theory. By default, if neither ``scm_heat_flux`` nor ``scm_monin_obukhov_length`` are specified, the model assumes **neutral conditions**.

**Stability Functions**

When a prescribed Monin-Obukhov length is provided, the model applies stability corrections to surface layer parameters:

- **Stable regime** (z/L > 0): Φ_m = 1 + 5(z/L) (Högström 1988)
- **Unstable regime** (z/L < 0): Φ_m = (1 - 16|z/L|)^(-1/4) (Businger et al. 1971)
- **Neutral regime**: Φ_m = 1 (default when M-O length not specified)

**Heat Flux vs. Monin-Obukhov Length**

Users can specify either:

1. ``scm_heat_flux`` [W/m²]: The model uses this to drive buoyancy-driven turbulence
2. ``scm_monin_obukhov_length`` [m]: Direct specification of the stability length scale

If neither is provided, the model defaults to neutral stratification.

**3D-1D Grid Mapping**

The 1D SCM solves on a uniform grid with spacing ``scm_dz`` (typically 4 m). When mapping the 1D profile to the 3D domain with potentially different grid spacing (``dz``), the model now uses **linear interpolation** for accurate vertical mapping at each 3D grid point. This provides better accuracy compared to nearest-neighbor interpolation when the 1D and 3D grids have different resolutions.

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
