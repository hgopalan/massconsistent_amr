.. _wake:

Wake Models
===========

The solver supports building wake parameterization using the **Röckle (1990)**
formulation, which models velocity deficits in the cavity and far-wake zones
behind rectangular buildings.

.. note::

   The wake model is applied **after** the initial velocity field is constructed
   (log-law, uniform, or RAWS mode) and **before** the mass-consistency solver
   is called. This allows wake deficits to be incorporated into the divergence-free
   wind field.

Overview
--------

Building wakes create recirculation zones (cavity) and velocity deficits (far-wake)
downwind of obstacles. The Röckle model divides the wake into two zones:

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

The cavity exhibits recirculation (negative velocity in the wind direction).

Far-Wake Zone
^^^^^^^^^^^^^

* **Starts at**: ``x = x_building + Lr``
* **Extends to**: ``Lf = separation_length × H`` (default: 3H)
* **Lateral spreading**: Wake width increases linearly from ``Wr`` to ``~2Wr``
* **Velocity deficit**: Decreases linearly from cavity edge to zero at ``Lf``

The far-wake velocity deficit is:

.. math::

   u_{deficit}(x) = c_2 \cdot U_H \cdot \left(1 - \frac{x - L_r}{L_f - L_r}\right)

where ``x`` is the downwind distance from the building back face.

Enabling the Wake Model
------------------------

To enable wake modeling, set ``enable_wake = true`` in the input file and
specify building geometry via ``building_file``:

.. code-block:: text

   # Enable wake model
   enable_wake = true
   
   # Wake model parameters (optional, these are defaults)
   wake_c1 = 0.9                    # Cavity length coefficient
   wake_c2 = 0.3                    # Wake deficit coefficient  
   wake_separation_length = 3.0     # Far-wake extent (× building height)
   
   # Building geometry
   building_file = buildings.csv

The buildings CSV file should contain one building per line:

.. code-block:: text

   # Format: xmin xmax ymin ymax zmin zmax [m]
   80.0  120.0  90.0  110.0  0.0  30.0

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
     - Enable Röckle wake model
   * - ``wake_c1``
     - ``0.9``
     - Cavity length coefficient (Lr = c1 × H)
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

References
----------

* Röckle, R. (1990). *Bestimmung der Strömungsverhältnisse im Bereich komplexer
  Bebauungsstrukturen*. Dissertation, Vom Fachbereich Mechanik, der Technischen
  Hochschule Darmstadt.

* Kaplan, H., & Dinar, N. (1996). A Lagrangian dispersion model for calculating
  concentration distribution within a built-up domain. *Atmospheric Environment*,
  30(24), 4197-4207.

Implementation Details
----------------------

The wake model is implemented in ``src/wake_models.H`` and integrated into
the velocity initialization in ``src/wind_solver.cpp``. The model:

1. Computes the wind-aligned coordinate system for each building
2. Determines if each grid cell falls within the cavity or far-wake zone
3. Applies velocity deficits to the initial wind field ``u₀``
4. The modified field is then passed to the mass-consistency solver

The wake calculations are performed on-device (GPU-compatible) using AMReX
GPU kernels for efficient parallel execution.

Limitations
-----------

* **Simplified geometry**: The model assumes rectangular buildings aligned with
  the domain axes. Arbitrary building orientations are not fully supported.
  
* **Single wake per building**: Each building's wake is computed independently.
  Wake-wake interactions and wake merging are handled by sequential application
  but may not capture complex interference patterns.
  
* **Steady-state**: The model assumes steady, uniform approach flow. Time-varying
  winds or turbulent fluctuations are not represented.

Future Extensions
-----------------

* Support for arbitrary building orientations
* Wake-wake interaction models for building arrays
* Vertical velocity components in cavity zone
* Street canyon models for closely-spaced buildings
