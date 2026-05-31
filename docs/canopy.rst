.. _canopy:

Canopy Model
============

Overview
--------

This implementation adds vegetation canopy parameterization to the mass-consistent wind solver, following the QUIC-URB approach with empirical models from MacDonald et al. (2000) and Shaw & Pereira (1982).

Implementation Details
----------------------

Files Created/Modified
~~~~~~~~~~~~~~~~~~~~~~~

1. **src/canopy_models.H** (NEW)
   - Header-only library with GPU-portable canopy functions
   - MacDonald displacement height calculation
   - Shaw-Pereira exponential profile
   - Combined canopy wind profile function

2. **src/wind_solver_api.H** (MODIFIED)
   - Added canopy parameters to ``WindSolverState`` structure

3. **src/wind_solver_api.cpp** (MODIFIED)
   - Added canopy parameter parsing
   - Modified log-law initialization to use canopy model

4. **src/wind_solver.cpp** (MODIFIED)
   - Added canopy parameter parsing
   - Modified log-law initialization to use canopy model
   - Added canopy status output

5. **docs/usage.rst** (MODIFIED)
   - Added canopy parameter documentation

6. **docs/wind_solver.rst** (MODIFIED)
   - Added canopy model theory and usage section

7. **regtest/canopy_forest/** (NEW)
   - Regression test for MacDonald displacement height model

8. **regtest/canopy_exponential/** (NEW)
   - Regression test for Shaw-Pereira exponential profile

New Input Parameters
--------------------

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Parameter
     - Default
     - Description
   * - ``enable_canopy``
     - ``false``
     - Enable canopy parameterization
   * - ``canopy_height``
     - ``0.0``
     - Canopy height [m]
   * - ``frontal_area_index``
     - ``0.0``
     - Frontal area index λ_f
   * - ``plan_area_index``
     - ``0.0``
     - Plan area index λ_p
   * - ``canopy_drag_coeff``
     - ``0.2``
     - Drag coefficient C_d
   * - ``use_exponential_profile``
     - ``false``
     - Use Shaw-Pereira exponential decay
   * - ``canopy_attenuation``
     - ``2.5``
     - Exponential attenuation coefficient α

Typical Parameter Values
-------------------------

Forest Canopies
~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 15 15 20

   * - Type
     - λ_f
     - λ_p
     - Height (m)
   * - Sparse forest
     - 0.15-0.20
     - 0.10-0.15
     - 10-15
   * - Moderate forest
     - 0.25-0.30
     - 0.20-0.25
     - 15-20
   * - Dense forest
     - 0.35-0.45
     - 0.30-0.40
     - 20-30

Agricultural Canopies
~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 25 15 15 20

   * - Type
     - λ_f
     - λ_p
     - Height (m)
   * - Crops
     - 0.10-0.15
     - 0.05-0.10
     - 0.5-2.0
   * - Grassland
     - 0.05-0.10
     - 0.03-0.07
     - 0.1-0.5

Usage Example
-------------

Add the following to your ``inputs.i`` file::

    # Enable canopy model
    enable_canopy = true
    canopy_height = 15.0          # Forest canopy [m]
    frontal_area_index = 0.25     # Moderate density
    plan_area_index = 0.20
    canopy_drag_coeff = 0.2

    # Optional: enable exponential decay within canopy
    use_exponential_profile = true
    canopy_attenuation = 2.5

Model Validation
----------------

The implementation has been tested with:

* ✅ MacDonald displacement height model (regtest/canopy_forest)
* ✅ Shaw-Pereira exponential profile (regtest/canopy_exponential)
* ✅ Backward compatibility with existing tests (gaussian_hill, flat_terrain)

References
----------

1. MacDonald, R.W., Griffiths, R.F., Hall, D.J. (2000). A comparison of results from scaled field and wind tunnel modelling of dispersion in arrays of obstacles. *Atmospheric Environment*, 34(20), 3845-3862.

2. Shaw, R.H., Pereira, A.R. (1982). Aerodynamic roughness of a plant canopy: A numerical experiment. *Agricultural Meteorology*, 26, 51-65.

3. Cionco, R.M. (1965). A mathematical model for air flow in a vegetative canopy. *Journal of Applied Meteorology*, 4, 517-522.

Future Enhancements
-------------------

Potential additions for canopy modeling:

1. **Spatial variability**: Read canopy parameters from file (similar to terrain)
2. **Cionco drag-force model**: Add momentum source terms
3. **Canopy-induced turbulence**: Modify anisotropy factors within canopy
4. **Leaf area density profiles**: Vertical variation in canopy density
5. **Multiple canopy layers**: Different parameters for understory and overstory

Performance Notes
-----------------

The canopy model adds minimal computational overhead:

* GPU-portable inline functions
* No additional memory allocation
* Same MLMG solver convergence
* Compatible with all existing features (RAWS, uniform, terrain-following)
