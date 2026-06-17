.. _building_wake_enhancements:

Building Wake Model Enhancements
=================================

This section provides a quick reference to the advanced building wake model enhancements implemented in the mass-consistent wind solver. Detailed information is located in the appropriate documentation sections below.

Overview
--------

The mass-consistent solver includes nine advanced building wake physics enhancements that improve prediction accuracy for urban wind fields:

1. **Far-Wake Extension to 15H** — Extends far-wake influence from 3–5H to 15 building heights downstream
2. **Oblique Angle Cavity Scaling** — Scales cavity length based on wind approach angle
3. **Tall-Building Aspect-Ratio Correction** — Applies aspect-ratio dependent correction for non-cubic buildings
4. **Gaussian Lateral Wake Profile** — Optional smooth Gaussian-profile deficit distribution
5. **Upwind Recirculation Zone** — Models reverse flow upstream of building
6. **Log-Law Reference Velocity Correction** — Extracts reference velocity from log-law profile
7. **Corner and Side Acceleration** — Adds velocity amplification at building edges
8. **Height-Dependent Velocity Variance Correction** — Modifies velocity variance profile for turbulence intensity
9. **Horseshoe Vortex Modeling** — Computes velocity perturbations from circulation at building-ground junction

Documentation Navigation
------------------------

**Mathematical Models**

For detailed mathematical formulations and physical equations:

- See :ref:`mathematical_models` section "Building Wake Models"
- Includes core Röckle model formulation
- All nine physics enhancement equations documented

**Numerical Implementation**

For implementation details and GPU optimization:

- See :ref:`numerical_methods` section "Building Wake Physics Implementation"
- Physical-to-numerical discretization
- GPU-compatible function implementations
- Integration into the mass-consistent solver

**Configuration Parameters**

For input file parameters controlling wake enhancements:

- See :ref:`parmparse_reference` section "Building Wake Physics Enhancements"
- All nine feature enable/disable flags
- Default parameter values and descriptions

**Testing Infrastructure**

For comprehensive test details and execution:

- See :ref:`regtests` section ``wake_enhancements``
- C++ unit test specifications
- Python integration test coverage
- Build and run commands

**Implementation Details**

For complete implementation status and project metrics:

- See :ref:`wake_physics_implementation`
- Detailed feature status for all 9 enhancements
- Code statistics and verification results

Backward Compatibility
-----------------------

All changes are backward compatible:

- Default configuration enables all enhancements for improved physics
- Each feature can be individually disabled via input parameters
- Disabling all flags recovers the original Röckle model behavior
- No API changes to public solver interface
- No data structure modifications breaking binary compatibility

Quick Configuration Example
----------------------------

To enable all building wake physics enhancements in an AMReX inputs file:

.. code-block:: text

    enable_extended_farwake = true
    enable_oblique_scaling = true
    enable_tall_building_correction = true
    enable_gaussian_profile = false
    enable_upwind_recirculation = true
    enable_reference_correction = false
    enable_corner_acceleration = true
    enable_variance_correction = false
    enable_horseshoe_vortex = true

References
----------

See :ref:`references` section for the complete bibliography. Key citations for building wake modeling include:

- Röckle (1990): Foundational urban canyon wake model
- Huber-Snyder (EPA): Empirical aspect-ratio dependent model
- Pardyjak & Brown (2001): QUIC-URB implementation guide
- Jensen (1979): Power-law wake recovery
- Rodi (1986): Entrainment-based wake modeling
- Blocken & Carmeliet (2004): Separable 3D deficit profiles
- Yoshie et al. (2007): Height-dependent canyon effects
- Oikonomou et al. (2017): Modern aspect-ratio refinements
- Murakami & Uehara (1983): Non-dimensional self-similar forms
