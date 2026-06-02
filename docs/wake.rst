.. _wake:

Wake Models
===========

The solver supports two building wake parameterizations:

1. **Röckle (1990)** — Default cavity + far-wake formulation
2. **Huber-Snyder (EPA)** — Alternative aspect-ratio dependent model

Both models parameterize velocity deficits in the cavity and far-wake zones
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

Enabling the Wake Model
------------------------

To enable wake modeling, set ``enable_wake = true`` in the input file and
specify building geometry via ``building_file``:

.. code-block:: text

   # Enable wake model
   enable_wake = true
   wake_model_type = rockle              # or "huber_snyder"
   
   # Wake model parameters (optional, these are defaults)
   wake_c1 = 0.9                    # Cavity length coefficient (Röckle only)
   wake_c2 = 0.3                    # Wake deficit coefficient  
   wake_separation_length = 3.0     # Far-wake extent (× building height, Röckle only)
   
   # Building geometry
   building_file = buildings.csv

**Selecting Wake Model Type**

Use ``wake_model_type`` to choose between models::

   wake_model_type = rockle         # Default: Röckle (1990)
   wake_model_type = huber_snyder   # Alternative: Huber-Snyder (EPA)

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
     - Enable wake model (Röckle or Huber-Snyder)
   * - ``wake_model_type``
     - ``rockle``
     - Wake model selection: "rockle" or "huber_snyder"
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

Future Extensions
-----------------

* Improved rooftop vortex models based on building aspect ratio
* Two-counter-rotating vortex pair (CFD-based parameterizations)
