Comprehensive Turbulence Models Analysis for massconsistent_amr
===============================================================


Executive Summary
-----------------


This analysis examines synthetic turbulence models currently implemented in massconsistent_amr compared to NREL's TurbSim and industry standards, and identifies models that could enhance the solver's capabilities.

Current Implementation Status
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


**Implemented Models:**
- 2 Spectrum Models (Von Kármán, Kaimal)
- 3 Intensity Models (PowerLaw, Logarithmic, Constant)
- 2 Coherence Models (Gaussian, Exponential)
- 1 Export Format (BTS/OpenFAST)

**Total: 8 model variants implemented**

----


Detailed Findings
-----------------


Spectrum Models Comparison
~~~~~~~~~~~~~~~~~~~~~~~~~~


.. list-table::
   :header-rows: 1

   * - Model
     - Current
     - TurbSim
     - Complexity
     - Use Case
   * - Von Kármán
     - ✓
     - ✓
     - Low
     - Isotropic turbulence
   * - Kaimal
     - ✓
     - ✓
     - Low
     - IEC 61400-1 standard
   * - Mann Box
     - ✗
     - —
     - HIGH
     - Wind farms, complex terrain
   * - GP_LLJ
     - ✗
     - ✓
     - Medium
     - Great Plains, nocturnal LLJ
   * - NWTC
     - ✗
     - ✓
     - Medium
     - US locations (NREL)
   * - IEC 61400-1
     - ✗
     - ✓
     - Medium
     - Wind turbine design standards
   * - HIT
     - ✗
     - —
     - Low
     - Research/CFD validation


Critical Missing Models
~~~~~~~~~~~~~~~~~~~~~~~


1. Mann Box Model (HIGHEST PRIORITY)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Status**: Not in TurbSim core; separate preprocessing tool
- **Importance**: Critical for professional wind farm simulations
- **Physics**: 3D anisotropic spectral tensor (superior to isotropic models)
- **Industry Use**: Very high (Vestas, Siemens Gamesa, etc.)
- **Implementation Effort**: 1-2 weeks (complex, tensor math)
- **References**: 
  - Mann, J. (1994). J. Fluid Mech., 273, 141-168
  - Mann, J. (1998). Prob. Eng. Mech., 13(4), 269-282

2. IEC 61400-1:2019 Models (HIGH PRIORITY)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Types**: NTM, ETM, EOG, EWS, ECG
- **Importance**: Regulatory requirement for wind turbine design
- **Implementation Effort**: 3-5 days (lookup tables + gust profiles)
- **Industry Use**: Mandatory for certification

3. GP_LLJ Model (MEDIUM PRIORITY)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Status**: In TurbSim, not in massconsistent_amr
- **Importance**: Critical for Great Plains region
- **Implementation Effort**: 2-3 days

4. NWTC Model (MEDIUM PRIORITY)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Status**: In TurbSim, not in massconsistent_amr
- **Importance**: NREL-developed model for US applications
- **Implementation Effort**: 2-3 days

----


3-Phase Implementation Roadmap
------------------------------


PHASE 1: Quick Wins (1-2 weeks)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* [ ] IEC 61400-1 intensity model (lookup tables)
* [ ] Additional coherence model variants
* [ ] Smooth/user-defined intensity profiles

PHASE 2: Industry Alignment (2-4 weeks)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* [ ] **Mann Box spectrum model** (Priority 1)
* [ ] GP_LLJ spectrum model
* [ ] NWTC spectrum model
* [ ] IEC deterministic gusts (EOG, EWS, ECG)

PHASE 3: Enhanced Capabilities (1-2 months)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* [ ] Mann binary format export (.u, .v, .w)
* [ ] HDF5/NetCDF export support
* [ ] GPU optimization for tensor operations
* [ ] Extended BTS metadata

----


Technical Implementation Details
--------------------------------


Files to Modify
~~~~~~~~~~~~~~~


1. **``src/synthetic_turbulence.H``**
   - Add new TurbulenceModel enum values
   - Implement new spectrum functions
   - Extend TurbulenceParams structure

2. **``src/wind_solver.cpp``**
   - Add parameter parsing for new models
   - Route to appropriate spectrum implementations

3. **``src/turbsim_bts_export.H``**
   - Add format handlers for new export types

4. **Documentation (``docs/*.rst``)**
   - Document new models and usage

5. **Tests (``regtest/synthetic_turbulence_full/``)**
   - Add regression tests for each model

Key Code Locations
~~~~~~~~~~~~~~~~~~


.. code-block:: text

    Spectrum implementations: src/synthetic_turbulence.H (lines 147-300+)
    Parameter parsing:       src/wind_solver.cpp (lines ~1700-1800)
    Export logic:           src/turbsim_bts_export.H
    Tests:                  regtest/synthetic_turbulence_full/test_synthetic_turbulence.py


----


Mann Box Model: Detailed Technical Overview
-------------------------------------------


What Makes Mann Box Special
~~~~~~~~~~~~~~~~~~~~~~~~~~~


- **Anisotropic spectral tensor** rather than isotropic spectra
- **3D spatial coherence** with proper tensor structure
- **Better physics** for complex terrain and wind farms
- **Industry standard** in European wind energy

Implementation Requirements
~~~~~~~~~~~~~~~~~~~~~~~~~~~


1. 3x3 spectral tensor computation: S_ij(k) from Mann (1994)
2. Anisotropy parameters and scaling
3. Coherence matrix calculations
4. Optional export format (.u, .v, .w binary files)

Complexity Factors
~~~~~~~~~~~~~~~~~~


- Requires understanding of spectral tensor theory
- More computationally intensive than isotropic models
- Well-documented in peer-reviewed literature
- Can leverage AMReX GPU tensor operations

Expected Impact
~~~~~~~~~~~~~~~


- Enables professional wind farm simulations
- Improves representation of complex terrain effects
- Aligns solver with industry-standard tools
- Opens market to European wind energy developers

----


Terrain Integration
-------------------


**Good News**: Recent work on terrain-aware fluctuations (IMPLEMENTATION_NOTES.rst) provides solid foundation:
- Masking approach turns off fluctuations inside terrain
- Smooth blending maintains mass conservation
- All new models can leverage existing masking

----


TurbSim Compatibility Status
----------------------------


Already Supported
~~~~~~~~~~~~~~~~~

✓ Von Kármán
✓ Kaimal

In TurbSim But Missing
~~~~~~~~~~~~~~~~~~~~~~

- GP_LLJ
- NWTC
- Smooth/User-defined
- USWTPP

Not in TurbSim (Separate Tool)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **Mann Box** (most important)

Why Mann Box Isn't in TurbSim
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Mann Box is a **standalone preprocessor**, not integrated into TurbSim core. However, it's widely used by professional wind developers who then feed its output into TurbSim for turbine simulations.

----


Validation Strategy
-------------------


For each new model, implement:
1. Spectral matching against published references
2. Intensity profile validation
3. Coherence structure verification
4. BTS export compatibility testing
5. Regression tests

----


Export Format Considerations
----------------------------


Current: BTS format only
Recommended additions:
- Mann binary format (.u, .v, .w files)
- HDF5/NetCDF for large datasets and metadata
- Extended BTS metadata for model information

----


Recommendations Summary
-----------------------


🔴 CRITICAL
~~~~~~~~~~

- **Mann Box Model should be priority**
  - Highest industry demand
  - Unlocks professional wind farm market
  - Well-documented implementation path

🟡 IMPORTANT
~~~~~~~~~~~

- **IEC 61400-1 compliance**
  - Regulatory requirement
  - Relatively easy to implement
  - Required for certification workflows

🟢 NICE-TO-HAVE
~~~~~~~~~~~~~~

- **GP_LLJ, NWTC models**
  - Improve geographic coverage
  - Good regional applicability
  - Moderate implementation effort

----


References
----------


1. Mann, J. (1994). The spatial structure of neutral atmospheric surface-layer turbulence. J. Fluid Mech., 273, 141-168.
2. Mann, J. (1998). Wind field simulation. Probabilistic Engineering Mechanics, 13(4), 269-282.
3. IEC 61400-1:2019. Wind turbines – Design requirements.
4. Kaimal, J.C., et al. (1972). Spectral characteristics of surface-layer turbulence. Quarterly Journal of the Royal Meteorological Society, 98(417), 563-589.
5. TurbSim documentation: https://nrel.github.io/TurbSim/
6. Panofsky, H.A., & Dutton, J.A. (1984). Atmospheric Turbulence: Models and Methods for Engineering Applications.

----


**Analysis Date**: June 2026  
**Scope**: massconsistent_amr synthetic turbulence models, NREL TurbSim reference  
**Status**: Comprehensive research completed, ready for implementation phase
