Phase 5: Testing & Validation
==============================

Overview
--------

Phase 5 implements comprehensive testing and validation of all model capabilities against analytical solutions, legacy code, and CALPUFF where applicable. The goal is to ensure that the model capabilities are comparable to CALPUFF while maintaining backward compatibility.

Testing Strategy
----------------

Phase 5 is divided into three components:

1. **5.1: Regression Tests** - Verify each major capability
2. **5.2: Backwards Compatibility** - Ensure legacy support
3. **5.3: CALPUFF Validation** - Compare against reference model

Phase 5.1: Regression Tests
----------------------------

Purpose
~~~~~~~

Verify that new capabilities work correctly and produce physically reasonable results.

Test Coverage
~~~~~~~~~~~~~

**5.1a: Multi-source Dispersion**

Located in ``regtest/dispersion/puff_multisource_three_stacks/``

Scenario: Three industrial stacks at different locations and heights

- Stack 1: 100 m high, 1.0 units/s
- Stack 2: 80 m high, 0.8 units/s  
- Stack 3: 120 m high, 1.2 units/s

Verification:
  ✓ Three sources load correctly
  ✓ Concentrations computed at receptors
  ✓ Superposition principle verified
  ✓ No negative concentrations
  ✓ Output includes all configured fields

Expected Result: Sum of three Gaussian puff contributions

**5.1b: Time-varying Emissions**

Located in ``regtest/dispersion/puff_timevary_emissions/``

Scenario: Single source with time-varying emission rate

- Simulates traffic rush hour pattern
- Low morning, peak midday, evening decay
- 7 time points over 3600 seconds

Verification:
  ✓ Emission rates interpolated correctly
  ✓ Receptor concentrations follow emission pattern
  ✓ Temporal variation is smooth
  ✓ No physically unrealistic spikes

Expected Result: Concentration peaks correspond to high emission periods

**5.1c: Reactive Chemistry**

Located in ``regtest/dispersion/puff_chemistry_reactions/``

Scenario: SO₂ emissions with chemical transformation to sulfate

- SO₂ source at 50 m height
- First-order decay: SO₂ → Sulfate
- 3600 second simulation

Verification:
  ✓ Chemistry output fields present
  ✓ SO₂ concentration decreases downwind
  ✓ Sulfate concentration increases (formation from decay)
  ✓ Total SO₂-equivalent remains roughly conserved
  ✓ Output includes chemistry species

Expected Result: SO₂ decay with Sulfate formation

**5.1d: Enhanced Deposition**

Location: ``regtest/dispersion/puff_deposition/`` (existing test)

Scenario: Particle settling and dry/wet deposition

Verification:
  ✓ Deposition reduces puff mass over time
  ✓ Deposition flux computed correctly
  ✓ Particles settle to ground
  ✓ Deposition fields in output

Expected Result: Puff mass decreases, ground-level deposition increases

Running Regression Tests
~~~~~~~~~~~~~~~~~~~~~~~~

Run all Phase 5 tests::

    cd regtest
    python3 run_phase5_tests.py

Run individual test::

    cd regtest/dispersion/puff_multisource_three_stacks
    python3 test_multisource.py

Expected Output::

    ✓ Multi-source test passed
      - 25 receptors
      - Concentration range: 1.23e-05 to 8.45e-03
      - Non-zero receptors: 24/25

Phase 5.2: Backwards Compatibility
-----------------------------------

Purpose
~~~~~~~

Ensure that all legacy input files still work with the new code and produce compatible output.

Test Strategy
~~~~~~~~~~~~~

1. **Input File Validation**
   - Parse all existing inputs_*.i files
   - Verify no deprecated parameters
   - Check for warnings/errors

2. **Output Format Compatibility**
   - Verify base fields always present (x, y, z, C_total)
   - Check metadata header format
   - Ensure CSV can be read by old post-processing scripts

3. **Numerical Results**
   - Compare with baseline calculations
   - Allow for small floating-point differences (< 0.1%)
   - Document any systematic differences

Legacy Input Files Tested
~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    inputs_single.i              - Single source, simple dispersion
    inputs_multi.i               - Multi-source dispersion
    inputs_phase1_simple.i       - Phase 1 baseline
    inputs_phase4_industrial.i   - Phase 4 industrial scenario
    inputs_phase4_complex_terrain.i - Complex terrain
    inputs_phase4_area_chemistry.i   - Area source with chemistry
    inputs_phase4_timevary.i     - Time-varying emissions
    inputs_scalar_transport.i    - Scalar transport mode
    (Plus all other inputs_*.i files)

Running Compatibility Tests
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    cd regtest/compatibility
    python3 test_backwards_compat.py

Expected Results::

    Found 12 input files to test
    
    Backwards compatibility test results:
      Passed: 12
      Failed: 0
    
    ✓ Output format compatibility check passed

Phase 5.3: CALPUFF Validation
------------------------------

Purpose
~~~~~~~

Compare model results against CALPUFF for overlapping capabilities to validate physical correctness.

Validation Approach
~~~~~~~~~~~~~~~~~~~

**Comparable Scenarios**

1. **Simple Gaussian Puff**
   - Single point source
   - Neutral stability
   - Uniform wind field
   - Compare analytical solution

2. **Multi-source Industrial Complex**
   - 3-5 stacks
   - Different stack parameters
   - Time-varying meteorology
   - Compare total impact zone

3. **Reactive Chemistry**
   - SO₂ → Sulfate transformation
   - Compare concentration profiles
   - Verify decay rates

Validation Metrics
~~~~~~~~~~~~~~~~~~

For each scenario, compute:

- **Spatial correlation**: Compare concentration at same locations
- **Peak concentration**: Compare maximum values
- **Integrated mass**: Verify conservation of mass
- **Plume centerline**: Compare trajectory alignment
- **Plume width**: Compare growth rates

Expected Correlations::

    Perfect agreement (r² > 0.95):
      - Gaussian puff analytical solution
      - Multi-source superposition
    
    Good agreement (r² > 0.80):
      - Reactive chemistry (same rates)
      - Deposition effects
    
    Reasonable agreement (r² > 0.70):
      - Complex terrain effects
      - Building downwash effects
      - CALPUFF comparison (accounting for model differences)

Running CALPUFF Comparison (if available)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    python3 tools/compare_with_calpuff.py \
        --scenario industrial_complex \
        --calpuff_results calpuff_output.csv \
        --massconsistent_results receptor_concentration.csv_step*

Test Coverage Summary
---------------------

.. table:: Phase 5 Test Coverage

    ==================  =============  ==================  ===========
    Feature             Test Name      Expected Result     Status
    ==================  =============  ==================  ===========
    Multi-source        5.1a           3 sources added     PASS
    Time-varying        5.1b           Emission variation  PASS
    Chemistry           5.1c           SO₂ decay+formation PASS
    Deposition          5.1d           Mass reduction      PASS
    Backward compat     5.2            Legacy files work   PASS
    CALPUFF validate    5.3            r² > 0.70           TBD*
    ==================  =============  ==================  ===========

    \* Pending CALPUFF license availability

Quality Assurance Checklist
---------------------------

- [x] All regression tests pass
- [x] No negative/infinite/NaN values in output
- [x] Output files readable and well-formed
- [x] Metadata headers correct and consistent
- [x] Backward compatibility verified
- [x] Performance acceptable (< 5% slowdown)
- [ ] CALPUFF comparison (pending license)
- [ ] Field documentation complete

References
----------

- Turner et al. (1994): Workbook of Atmospheric Dispersion Estimates
- Pasquill & Gifford (1961): Dispersion estimation methods
- EPA CALPUFF Documentation
- Hanna et al. (1982): Handbook on Atmospheric Diffusion
