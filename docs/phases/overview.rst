Phase 4-6: Implementation Overview
==================================

Overview
--------

This documentation summarizes the Phase 4-6 enhancement implementation for the massconsistent_amr dispersion model.

- **Phase 4**: Data I/O and output standardization (Sections 4.1-4.3)
- **Phase 5**: Testing and validation (Sections 5.1-5.3)  
- **Phase 6**: Documentation and user guides

Key Goals
---------

- **Capability Parity**: Match CALPUFF model capabilities
- **Backward Compatibility**: Maintain support for legacy input files
- **Flexible Output**: Conditional output fields based on enabled features
- **Comprehensive Testing**: Full regression and validation test suite

.. toctree::
   :maxdepth: 2
   :caption: Phases
   
   phases/phase4
   phases/phase5
   features/index

Phase 4: Data I/O & Output Standardization
-------------------------------------------

Phase 4 implements a flexible CSV input/output infrastructure:

**4.1 CSV Input Infrastructure**
  - Multi-source dispersion (point, line, area, volume sources)
  - Time-varying emissions (hourly, daily, seasonal profiles)
  - Spatial meteorology profiles
  - Deposition parameters
  - Receptor grid definitions

**4.2 Output Standardization** (NEW)
  - Dynamic output fields based on enabled capabilities
  - Chemistry species output (SO₂, Sulfate, NOₓ, HNO₃, Nitrate, etc.)
  - Visibility metrics (b_ext, visual_range, deciview)
  - Deposition flux output (dry and wet)
  - Metadata header with feature flags for reproducibility

**4.3 Python Preprocessing & Postprocessing**
  - `chemistry_builder.py` - Interactive chemistry matrix builder
  - `emission_profile_generator.py` - Time-varying profile generation
  - `receptor_grid_generator.py` - Receptor grid creation
  - `visibility_postprocessor.py` - Visibility metric computation
  - `wind_field_converter.py` - Wind field format conversion

Phase 5: Testing & Validation
------------------------------

Phase 5 implements comprehensive regression and validation tests:

**5.1 Regression Tests**
  - Multi-source dispersion (3+ sources)
  - Time-varying emissions
  - Reactive chemistry (SO₂→SO₄ transformation)
  - Deposition effects (particle settling)
  - Wind shear and stability effects
  - Comparison with analytical solutions where available

**5.2 Backwards Compatibility**
  - Run all legacy input files through new code
  - Verify output fields remain compatible
  - Check for minimal floating-point differences
  - Document any parameter mapping changes

**5.3 CALPUFF Validation**
  - Create benchmark test scenarios
  - Compare results for overlapping capabilities
  - Document validation results and assumptions

Configuration Example
---------------------

Enable Phase 4.2 output standardization in inputs.i::

    puff_model {
        enable_chemistry = true
        output_enable_wind_components = false
        output_enable_pressure = false
        output_enable_terrain = false
        output_enable_visibility = true
        output_enable_deposition = false
        
        output_b_ext = true
        output_visual_range = true
        output_deciview = true
        output_fog_prob = false
    }

The output will include columns for all enabled features.

References
----------

- EPA CALPUFF Model Documentation
- Turner et al. (1994): Workbook of Atmospheric Dispersion Estimates
- Seinfeld & Pandis (2016): Atmospheric Chemistry and Physics
- Pitchford et al. (2007): IMPROVE Visibility Algorithm
