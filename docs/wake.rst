.. _wake:

Wake Models
===========

The solver supports three building wake parameterizations:

1. **Röckle (1990)** — Default cavity + far-wake formulation
2. **Huber-Snyder (EPA)** — Alternative aspect-ratio dependent model
3. **AERMOD PRIME (EPA)** — Regulatory model with Projected Building Area method

All models parameterize velocity deficits in the cavity and far-wake zones
behind rectangular buildings.

.. note::

   The wake model is applied **after** the initial velocity field is constructed
   (log-law, uniform, or RAWS mode) and **before** the mass-consistency solver
   is called. This allows wake deficits to be incorporated into the divergence-free
   wind field.

Overview
--------

Building wakes create recirculation zones (cavity) and velocity deficits (far-wake)
downwind of obstacles. Both wake models divide the wake into two zones:

1. **Cavity zone**: Immediate recirculation region with negative velocity
2. **Far-wake zone**: Downstream displacement region with reduced velocity

The wake model modifies the initial velocity field ``u₀`` by applying velocity
deficits based on the building geometry and wind direction.

Röckle (1990) Wake Model
-------------------------

The Röckle formulation parameterizes wake zones based on building height ``H``,
width ``W``, and length ``L``:

Cavity Zone
^^^^^^^^^^^

* **Length**: ``Lr = c1 × H`` (default: ``c1 = 0.9``)
* **Height**: ``Hr = 0.67 × H``
* **Width**: ``Wr = W``
* **Velocity deficit**: ``u_deficit = c2 × U_H`` (default: ``c2 = 0.3``)
* **Vertical circulation**: Rooftop vortex with characteristic up-down pattern

The cavity exhibits recirculation (negative velocity in the wind direction) combined with
vertical circulation due to rooftop vortex formation.

Far-Wake Zone
^^^^^^^^^^^^^

* **Starts at**: ``x = x_building + Lr``
* **Extends to**: ``Lf = separation_length × H`` (default: 3H)
* **Lateral spreading**: Wake width increases linearly from ``Wr`` to ``~2Wr``
* **Velocity deficit**: Decreases linearly from cavity edge to zero at ``Lf``

The far-wake velocity deficit is:

.. math::

   u_{deficit}(x) = c_2 \cdot U_H \cdot \left(1 - \frac{x - L_r}{L_f - L_r}\right)

where ``x`` is the downwind distance from the building back face, and ``U_H``
is the reference velocity magnitude at building height.

Huber-Snyder (EPA) Wake Model
------------------------------

The Huber-Snyder model is an alternative wake parameterization from EPA wind tunnel
studies, used in EPA dispersion models (ISC, AERMOD precursors). Key differences from
Röckle:

Cavity Zone
^^^^^^^^^^^

* **Length**: ``Lc = 0.5 × H × sqrt(W/H)`` (aspect ratio dependent)
* **Height**: ``Hc = 0.67 × H`` (same as Röckle)
* **Width**: ``Wc = W``
* **Velocity deficit**: ``u_deficit = c2 × U_H`` (same coefficient)

The cavity length depends on building aspect ratio, making it more suitable for certain
building geometries.

Far-Wake Zone
^^^^^^^^^^^^^

* **Starts at**: ``x = x_building + Lc``
* **Extends to**: ``Lw = 5 × H`` (longer than Röckle's typical 3H)
* **Lateral spreading**: Wake width increases linearly
* **Velocity deficit**: Decreases with power-law decay

The far-wake velocity deficit uses power-law decay:

.. math::

   u_{deficit}(x) = c_2 \cdot U_H \cdot \min\left(1, \frac{1}{\sqrt{x/L_c}}\right)

This creates a slower recovery than Röckle's linear decay, extending the wake influence
further downwind.

**When to use Huber-Snyder**:

* Buildings with non-square cross-sections (aspect ratio ≠ 1)
* Regulatory dispersion modeling (EPA model compatibility)
* Longer wake extent needed (5H vs 3H)

AERMOD PRIME (EPA) Wake Model
------------------------------

The AERMOD PRIME (Plume Rise Model Enhancements) is the wake model used in EPA's
AERMOD regulatory dispersion model. It is the industry standard for regulatory
compliance and well-validated against EPA wind tunnel data.

Key Features
^^^^^^^^^^^^

* **Projected Building Area (PBA) method**: Computes effective building cross-section
  perpendicular to wind direction
* **Streamline deflection**: Models flow over and around buildings with enhanced
  vertical velocity components
* **Enhanced turbulence**: Higher mixing in wake zones for realistic dispersion
* **Regulatory standard**: Used in EPA air quality modeling and permit applications

The PRIME algorithm is more complex than Röckle or Huber-Snyder, providing
improved accuracy for:

* Stack emissions and building downwash
* Regulatory air quality modeling
* Industrial facility design and permitting

Cavity Zone
^^^^^^^^^^^

The PRIME cavity zone dimensions depend on building aspect ratio (W/H):

* **Wide buildings** (W/H > 1): ``Lc = 0.9 × sqrt(PBA)``
* **Tall buildings** (W/H < 1): ``Lc = 0.5 × sqrt(PBA)``
* **Peak cavity height**: ``Hc_peak = 0.22 × sqrt(PBA)``
* **Cavity height limit**: ``Hc = min(1.5 × Hc_peak, H)``
* **Velocity deficit**: 50% of reference wind speed (higher than Röckle/Huber-Snyder)

The cavity zone includes enhanced turbulence and streamline deflection effects,
providing more realistic vertical velocity components.

Far-Wake Zone
^^^^^^^^^^^^^

* **Extends to**: ``10H`` downwind (longest of all three models)
* **Lateral spreading**: Enhanced lateral mixing with factor ``1 + 2x_norm``
* **Velocity deficit decay**: Exponential decay ``exp(-1.5 × x_norm)``
* **Vertical growth**: Wake height grows from ``Hc`` to ``H`` based on position

The PRIME far-wake uses exponential decay with enhanced mixing, creating a more
gradual recovery than the power-law (Huber-Snyder) or linear (Röckle) approaches.

.. math::

   u_{deficit}(x) = 0.3 \cdot U_H \cdot \exp\left(-1.5 \cdot \frac{x - L_c}{L_w - L_c}\right)

where ``Lw = 10H`` is the wake extent.

**When to use AERMOD PRIME**:

* Regulatory air quality modeling (EPA compliance)
* Stack emissions and building downwash analysis
* Industrial facility design and permitting
* Well-validated against wind tunnel data
* Need for enhanced turbulence and mixing

Enabling the Wake Model
------------------------

To enable wake modeling, set ``enable_wake = true`` in the input file and
specify building geometry via ``building_file``:

.. code-block:: text

   # Enable wake model
   enable_wake = true
   wake_model_type = rockle              # or "huber_snyder" or "aermod_prime"
   
   # Wake model parameters (optional, these are defaults)
   wake_c1 = 0.9                    # Cavity length coefficient (Röckle only)
   wake_c2 = 0.3                    # Wake deficit coefficient  
   wake_separation_length = 3.0     # Far-wake extent (× building height)
                                    # Röckle: 3H, Huber-Snyder: 5H, AERMOD: 10H
   
   # Building geometry
   building_file = buildings.csv

**Selecting Wake Model Type**

Use ``wake_model_type`` to choose between models::

   wake_model_type = rockle         # Default: Röckle (1990)
   wake_model_type = huber_snyder   # Alternative: Huber-Snyder (EPA)
   wake_model_type = aermod_prime   # EPA AERMOD PRIME (regulatory)

The buildings CSV file should contain one building per line with optional rotation angle:

.. code-block:: text

   # Format: xmin xmax ymin ymax zmin zmax [rotation_degrees]
   # Rotation angle (7th column, optional): degrees counter-clockwise from x-axis
   80.0  120.0  90.0  110.0  0.0  30.0       # Grid-aligned (rotation = 0°)
   200.0 240.0 150.0 180.0 0.0  25.0  45.0   # Rotated 45° counter-clockwise

Input Parameters
----------------

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Parameter
     - Default
     - Description
   * - ``enable_wake``
     - ``false``
     - Enable wake model (Röckle, Huber-Snyder, or AERMOD PRIME)
   * - ``wake_model_type``
     - ``rockle``
     - Wake model selection: "rockle", "huber_snyder", or "aermod_prime"
   * - ``wake_c1``
     - ``0.9``
     - Cavity length coefficient (Lr = c1 × H, Röckle only)
   * - ``wake_c2``
     - ``0.3``
     - Wake deficit coefficient (velocity reduction factor)
   * - ``wake_separation_length``
     - ``3.0``
     - Far-wake extent factor (Lf = factor × H)

Physical Interpretation
-----------------------

**Cavity zone**: Immediately behind a building, flow separates and creates a
recirculation bubble. The Röckle model approximates this as a region with negative
velocity (reverse flow) extending ``0.9H`` downwind and ``0.67H`` vertically.

**Far-wake zone**: Beyond the cavity, the wake gradually recovers as the flow
re-attaches. The wake width spreads laterally and the velocity deficit decreases
linearly until the flow returns to the undisturbed state at approximately ``3H``
downwind.

**Multiple buildings**: The model is applied independently for each building.
When wakes overlap, the velocity deficits are cumulative (applied sequentially).

Example: Single Building Wake
------------------------------

A simple test case with a single rectangular building (40m × 20m × 30m tall):

.. code-block:: text

   # inputs.i
   terrain_file = terrain.csv
   building_file = buildings.csv
   
   enable_wake = true
   wake_c1 = 0.9
   wake_c2 = 0.3
   wake_separation_length = 3.0
   
   U_ref = 10.0   # 10 m/s wind from west
   V_ref = 0.0
   z_ref = 10.0
   z0 = 0.1
   
   dx = 5.0
   dy = 5.0
   dz = 5.0
   
   plot_file = plt_wake

See ``regtest/wake_single_building/`` for a complete regression test.

Advanced Wake Features
----------------------

Rooftop Vortices
^^^^^^^^^^^^^^^^

**Physical basis**: When wind flows around a building, separation at the top edges
creates a rooftop vortex with vertical circulation inside the cavity zone. This
feature adds realistic vertical velocity components to the Röckle model.

**Implementation**: The rooftop vortex is parameterized as a parabolic circulation
pattern in both the vertical (z) and streamwise (x) directions:

.. math::

   w_{vortex} = C_v \cdot U_{ref} \cdot \left(\frac{H}{30}\right) \cdot 
                4\frac{x}{L_r}\left(1 - \frac{x}{L_r}\right) \cdot 4\frac{z}{H_r}\left(1 - \frac{z}{H_r}\right)

where ``C_v ≈ 0.15`` is the vortex strength coefficient. The vertical velocity is maximum
at mid-height and mid-length of the cavity, creating a characteristic up-down-up circulation
pattern.

**Validation**: See ``regtest/rooftop_vortex/`` for a test case that validates the vertical
velocity profiles in the building cavity zone.

**Reference**: Oke, T.R. (1988). Street design and urban canopy layer climate.
*Energy and Buildings*, 11(1-3), 103-113.

Building Orientation Effects
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Motivation**: Real buildings rarely align perfectly with the computational grid.
The solver supports arbitrary building orientations to improve wake modeling
for non-grid-aligned structures.

**Rotation parameter**: The buildings CSV file now accepts an optional 7th column
specifying rotation angle in degrees (counter-clockwise from the x-axis):

.. code-block:: text

    # xmin xmax ymin ymax zmin zmax [rotation_degrees]
    100.0 150.0 200.0 250.0 0.0 30.0         # Grid-aligned
    300.0 350.0 350.0 400.0 0.0 25.0 45.0    # Rotated 45°
    500.0 550.0 500.0 550.0 0.0 20.0 90.0    # Rotated 90°

**Wind-aligned dimensions**: When a building is rotated, the effective width and length
in the wind coordinate system are computed via projection onto the wind direction:

.. math::

   L_{wind} = |L_1 \cos\theta_{wind} + L_2 \sin\theta_{wind}|
   W_{wind} = |L_1 \sin\theta_{wind} - L_2 \cos\theta_{wind}|

where ``L_1`` and ``L_2`` are the rotated building edge vectors, and ``θ_wind`` is the
wind direction angle.

**Validation**: See ``regtest/building_oriented/`` for a test case with rotated buildings.

References
----------

* Röckle, R. (1990). *Bestimmung der Strömungsverhältnisse im Bereich komplexer
  Bebauungsstrukturen*. Dissertation, Vom Fachbereich Mechanik, der Technischen
  Hochschule Darmstadt.

* Kaplan, H., & Dinar, N. (1996). A Lagrangian dispersion model for calculating
  concentration distribution within a built-up domain. *Atmospheric Environment*,
  30(24), 4197-4207.

* Schulman, L.L., Strimaitis, D.G., & Scire, J.S. (2000). Development and
  Evaluation of the PRIME Plume Rise and Building Downwash Model. *Journal of
  the Air & Waste Management Association*, 50(3), 378-390.

* EPA (2004). *User's Guide for the AMS/EPA Regulatory Model - AERMOD*.
  EPA-454/B-03-001. U.S. Environmental Protection Agency.

* Oke, T.R. (1988). Street design and urban canopy layer climate. *Energy and
  Buildings*, 11(1-3), 103-113.

Implementation Details
----------------------

The wake model is implemented in ``src/wake_models.H`` and integrated into
the velocity initialization in ``src/wind_solver.cpp``. The model:

1. Computes the wind-aligned coordinate system for each building
2. Applies building rotation angle to get effective dimensions in wind frame
3. Determines if each grid cell falls within the cavity or far-wake zone
4. Applies velocity deficits and rooftop vortex circulation
5. The modified field is then passed to the mass-consistency solver

The wake calculations are performed on-device (GPU-compatible) using AMReX
GPU kernels for efficient parallel execution.

Limitations
-----------

* **Steady-state**: The model assumes steady, uniform approach flow. Time-varying
  winds or turbulent fluctuations are not represented.

* **Single wake per building**: Each building's wake is computed independently.
  Wake-wake interactions and wake merging are handled by sequential application
  but may not capture complex interference patterns.

* **Street Canyons**: The empirical Oke (1988) street canyon parameterization is a simplified flow-regime detector based purely on 2D height-to-width (``H/W``) ratios and grid cell dimensions. It does not resolve full 3D corner vortices, channeling angles, or asymmetric building heights, and assumes a homogeneous street canyon width represented by twice the cell spacing (``2Δx``).

Future Extensions
-----------------

* Improved rooftop vortex models based on building aspect ratio
* Two-counter-rotating vortex pair (CFD-based parameterizations)

Analytical Turbine Wake Models
==============================

The solver also supports analytical wind turbine wake deficit models applied directly within the initial wind field computation phase before the mass-consistent projection solver is executed. This is particularly useful for wind energy applications (such as coupled FLORIS-type microscale flow simulations).

Supported Turbine Wake Models
-----------------------------

1. **Jensen (Park) Wake Model**:
   Uses the classic linear expansion formulation to determine the wake radius as a function of downstream distance and a wake decay constant. A uniform wind speed reduction is applied across the wake cross-section.

   .. math::

      R_w(x_{down}) = R_0 + k_w \cdot x_{down}

      \Delta U = U_0 \cdot \frac{1 - \sqrt{1 - C_T}}{(1 + 2 \cdot k_w \cdot x_{down} / D)^2}

   where :math:`R_0 = D / 2` is the rotor radius, :math:`D` is the rotor diameter, :math:`C_T` is the thrust coefficient, :math:`k_w` is the wake decay constant, and :math:`U_0` is the inflow wind speed at the turbine hub.

2. **Bastankhah (Gaussian) Wake Model**:
   Implements a self-similar Gaussian velocity deficit profile, where the wake expansion scales linearly with downwind distance, providing a smoother, more physical radial deficit distribution.

   .. math::

      \sigma_w(x_{down}) = k_a \cdot x_{down} + \epsilon \cdot D

      \frac{\Delta U}{U_0} = \left( 1 - \sqrt{1 - \frac{C_T}{8 \cdot (\sigma_w / D)^2}} \right) \cdot \exp\left( - \frac{r^2}{2 \cdot \sigma_w^2} \right)

   where :math:`\epsilon = 0.2 \cdot \sqrt{\frac{1 + \sqrt{1-C_T}}{2\sqrt{1-C_T}}}` and :math:`k_a` is the wake expansion coefficient.

Terrain Awareness
-----------------

To make the wake models fully terrain-aware, the wake centerline is formulated to follow the local terrain profile perfectly. At any downwind coordinate, the local height of the centerline is computed relative to the local terrain height:

.. math::

   z_{vertical} = z_{agl} - H_T = (z - Z_{terrain}(x, y)) - H_T

where :math:`z_{agl}` is the height above local terrain, and :math:`H_T` is the turbine's hub height above local terrain. The radial distance :math:`r` is computed as:

.. math::

   r = \sqrt{y_{cross}^2 + z_{vertical}^2}

This ensures that the wake bends and conforms to arbitrary complex topography, maintaining a constant height above local ground level.

Atmospheric Stability Influence
--------------------------------

If atmospheric stability correction is enabled (using the Obukhov length :math:`L`), the turbine wake decay constant :math:`k_w` (for Jensen) or expansion rate :math:`k_a` (for Bastankhah) is scaled dynamically based on local stability:

.. math::

   F_{stability} = \begin{cases} 
      1 - 0.4 \cdot \tanh(\zeta_{hub}) & \text{if stable } (\zeta_{hub} > 0) \\
      1 + 0.6 \cdot \tanh(-\zeta_{hub}) & \text{if unstable } (\zeta_{hub} < 0) \\
      1 & \text{if neutral } (\zeta_{hub} = 0)
   \end{cases}

where :math:`\zeta_{hub} = H_{hub} / L`. In stable conditions, reduced ambient turbulence suppresses mixing, leading to a smaller wake expansion rate and a slower wake recovery. In unstable conditions, convective mixing accelerates wake recovery, resulting in a larger expansion rate.

Enabling Analytical Turbine Wakes
---------------------------------

To enable turbine wake modeling, configure the following parameters in your inputs file:

.. code-block:: text

   enable_turbine_wake = true
   turbine_file = turbines.csv
   turbine_wake_model_type = jensen           # or "bastankhah_gaussian"
   turbine_wake_superposition = quadratic     # or "linear"
   jensen_kw = 0.075                          # Base Jensen wake decay constant
   gaussian_ka = 0.05                         # Base Bastankhah expansion rate

The turbines CSV file should contain turbine locations, characteristics, and optional power/thrust curve file path:

.. code-block:: text

   # x, y, hub_height, rotor_diameter, default_ct, [power_curve_file]
   100.0, 200.0, 90.0, 120.0, 0.8, nrel_5mw.csv

Power Curve CSV Format
----------------------

The optional power curve CSV specifies discrete wind speeds, electrical power (kW), and thrust coefficient (:math:`C_T`):

.. code-block:: text

   # wind_speed, power_kw, ct
   3.0, 0.0, 0.8
   5.0, 1000.0, 0.78
   10.0, 5000.0, 0.5
   25.0, 5000.0, 0.1

Output Reporting
----------------

At each simulated time step, computed turbine states (including inflow hub wind speeds, meteorological directions, and power output) are logged in FLORIS-compatible format into a dedicated CSV file named ``turbine_power_output.csv``.

