Phase 4.2: Output Standardization
==================================

Overview
--------

Phase 4.2 extends CSV output to include new fields conditionally based on enabled features. This enables flexible output that scales from simple single-species dispersion to full multi-species chemistry with visibility impacts.

Design Goals
~~~~~~~~~~~~

- **Conditional Fields**: Add fields only when features are enabled
- **Backward Compatibility**: Old output fields always present
- **Metadata**: Include feature flags in output headers for reproducibility
- **CALPUFF Parity**: Support same output capabilities as CALPUFF model

Architecture
------------

OutputSpecification Class
~~~~~~~~~~~~~~~~~~~~~~~~~

The new ``OutputSpecification`` class (``OutputSpecification.H``) manages dynamic output fields::

    class OutputSpecification {
        bool enable_wind_components;     // u, v, w components
        bool enable_wind_speed;          // |U| = sqrt(u² + v²)
        bool enable_pressure;            // lambda, divergence fields
        bool enable_terrain;             // terrain_z, slope, z0
        bool enable_chemistry;           // Species concentrations
        bool enable_visibility;          // Visibility metrics
        bool enable_deposition;          // Deposition flux
        bool enable_quality_flags;       // QA/QC metrics
        
        std::vector<std::string> chemistry_species;
        std::vector<std::string> deposition_species;
    };

Output Fields
~~~~~~~~~~~~~~

**Always Output (Backward Compatible)**

- ``name`` - Receptor label
- ``x, y, z`` - Position coordinates
- ``C_total`` - Total concentration

**Conditional Fields**

Wind Components (if ``enable_wind_components = true``)::

    u, v, w, wind_speed

Chemistry Species (if ``enable_chemistry = true``)::

    SO2, Sulfate, NOx, HNO3, Nitrate, ...
    (user-configurable list)

Visibility Metrics (if ``enable_visibility = true``)::

    b_ext              # Extinction coefficient [1/Mm]
    visual_range       # Visual range [km]
    deciview           # Deciview [dv]
    fog_probability    # Fog occurrence probability [-]
    icing_probability  # Icing occurrence probability [-]

Deposition Flux (if ``enable_deposition = true``)::

    dry_flux_<species>  # Dry deposition flux [µg/(m²·s)]
    wet_flux_<species>  # Wet deposition flux [µg/(m²·s)]

Configuration
-------------

inputs.i Parameters
~~~~~~~~~~~~~~~~~~~

::

    # Phase 4.2: Output Specification
    puff_model {
        # Core feature flags
        enable_chemistry = true
        enable_visibility = true
        enable_deposition = false
        
        # Output field selection
        output_enable_wind_components = false
        output_enable_wind_speed = true
        output_enable_pressure = false
        output_enable_terrain = false
        
        # Visibility metric selection
        output_b_ext = true
        output_visual_range = true
        output_deciview = true
        output_fog_prob = false
        output_icing_prob = false
        
        # Deposition flux selection
        output_dry_flux = true
        output_wet_flux = true
    }

Metadata Header
~~~~~~~~~~~~~~~

Each output CSV file includes metadata indicating enabled features::

    # === Output Specification Metadata ===
    # enable_wind_components: true
    # enable_pressure: false
    # enable_chemistry: true
    # chemistry_species: SO2,Sulfate,NOx,HNO3,Nitrate
    # enable_visibility: true
    #   b_ext: yes
    #   visual_range: yes
    #   deciview: yes
    #   fog_probability: no
    #   icing_probability: no
    # enable_deposition: false
    # enable_quality_flags: false
    # === End Metadata ===
    name,x,y,z,C_total,SO2,Sulfate,NOx,HNO3,Nitrate,b_ext,visual_range,deciview

Implementation
---------------

Code Changes
~~~~~~~~~~~~

1. **OutputSpecification.H** - New header file
   - Defines OutputSpecification class
   - Generates CSV headers dynamically
   - Generates metadata headers

2. **puff_models.H** - Updated PuffParams struct
   - Added ``OutputSpec::OutputSpecification output_spec`` member
   - Configuration parsed from inputs.i

3. **puff_solver.cpp** - Updated receptor/grid output
   - Parse output specification from inputs
   - Use dynamic header generation
   - Write metadata header to output files
   - Dynamically write output fields based on specification

Example Usage
~~~~~~~~~~~~~

**Simple dispersion (backward compatible)**::

    # Default behavior: only concentration
    puff_model.enabled = true
    # Output: name,x,y,z,C_total

**With visibility**::

    puff_model.enable_visibility = true
    # Output: name,x,y,z,C_total,b_ext,visual_range,deciview

**Full CALPUFF-equivalent**::

    puff_model.enable_chemistry = true
    puff_model.enable_visibility = true
    puff_model.enable_deposition = true
    # Output: name,x,y,z,C_total,SO2,Sulfate,NOx,HNO3,Nitrate,
    #         b_ext,visual_range,deciview,dry_flux_SO2,dry_flux_Sulfate,...

Testing
-------

Verification Methods
~~~~~~~~~~~~~~~~~~~~~

1. **Output Format Verification**
   - Check CSV headers match specification
   - Verify metadata is consistent with actual data
   - Ensure no missing/extra columns

2. **Backward Compatibility**
   - Old input files produce valid output
   - Base fields (x, y, z, C_total) always present
   - No crashes on legacy configurations

3. **Value Range Checks**
   - Concentrations ≥ 0
   - Visibility metrics within physical bounds
   - Deposition fluxes have correct sign

Phase 5 Tests
~~~~~~~~~~~~~

Phase 5 includes regression tests verifying Phase 4.2 functionality:
- Multi-source with chemistry output
- Time-varying emissions with output specification
- Chemistry reactions with full output specification
- Backwards compatibility of output format

References
----------

- CALPUFF User's Guide (EPA/600/8-88/009L)
- Scire et al. (2000): CALPUFF Model Documentation
- Pitchford et al. (2007): IMPROVE Visibility Algorithm
