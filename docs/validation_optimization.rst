.. _validation_optimization:

Validation & Optimization Layer
================================

**Overview**

The Validation & Optimization Layer (Layer 3) provides comprehensive tools for ensuring physical correctness, performance assessment, and production readiness of advanced solver features. This layer builds upon the Foundation Layer (parameters and headers) and enables systematic validation against reference data, performance profiling on CPU/GPU architectures, and parameter sensitivity analysis.

**Key Components**

Performance Profiling
~~~~~~~~~~~~~~~~~~~~~

The performance profiling system provides detailed timing and resource utilization metrics:

- **Wall-clock timing**: Total elapsed time for computation
- **CPU timing**: CPU time accounting for parallelization
- **Memory profiling**: Peak memory usage and allocation tracking
- **Convergence metrics**: Convergence rate and iteration count
- **Throughput analysis**: Computational operations per second

Example usage::

    ValidationOptimization::PerformanceProfiler profiler;
    profiler.StartTimer();
    
    // ... perform computation ...
    
    double elapsed_ms = profiler.StopTimer();
    double memory_mb = ValidationOptimization::PerformanceProfiler::GetMultiFabMemoryMB(mf);

Performance Metrics Structure::

    struct PerformanceMetrics {
        double wall_time_ms;              // Total wall time in milliseconds
        double cpu_time_ms;               // CPU time in milliseconds
        double peak_memory_mb;            // Peak memory usage in MB
        int    total_iterations;          // Total number of iterations
        double convergence_rate;          // Convergence rate metric
        double computational_throughput;  // Operations per second
    };

Physical Correctness Validation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The validation framework checks that computed solutions meet physical constraints:

**Wind Field Validation**

Compares computed wind fields against reference data with configurable tolerance::

    ValidationResult result = ValidationOptimization::PhysicalValidator::ValidateWindField(
        computed_field,      // Computed MultiFab
        reference_field,     // Reference MultiFab
        tolerance);          // Tolerance (default 0.01 = 1%)

Computes error metrics:
- **Max Error**: Maximum absolute error across all cells
- **Mean Error**: Average absolute error
- **RMS Error**: Root mean square error
- **Failed Cells**: Count of cells exceeding tolerance

**Physical Bounds Checking**

Validates that solution respects physical constraints::

    bool valid = ValidationOptimization::PhysicalValidator::CheckPhysicalBounds(
        field,              // Input MultiFab
        min_value,          // Physical minimum
        max_value,          // Physical maximum
        component);         // Component index

Example constraints:
- Wind speed: u ≥ 0, v can be any sign, w can be any sign
- Temperature: T ≥ absolute zero (depends on model)
- Richardson number: Ri ≥ 0 (always non-negative)

**Mass Conservation**

Verifies that continuity equation is satisfied::

    double max_div = ValidationOptimization::PhysicalValidator::GetMaxDivergence(
        velocity_x,         // u-component
        velocity_y,         // v-component
        velocity_z);        // w-component

For proper mass-consistent solver: ∇·u should be near machine precision.

Parameter Sensitivity Analysis
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Systematic parameter variation to identify which parameters significantly affect output.

**Batch Parameter Sweep Tool (Phase 5)**

The ``tools/parameter_sensitivity.py`` utility provides command-line access to systematic
sensitivity studies:

**Single Parameter Sweep:**

.. code-block:: bash

    python3 tools/parameter_sensitivity.py --inputs regtest/gaussian_hill/inputs.i \
        --param z0 --range 0.001 0.1 --steps 10 \
        --output sensitivity_z0.csv

This varies roughness length z₀ logarithmically from 0.001 m to 0.1 m in 10 steps,
running the solver for each value and recording convergence metrics.

**Output:**

CSV file with columns:
- ``step``: Sequential step number
- ``parameter``: Parameter name
- ``value``: Parameter value tested
- ``success``: Solver convergence (true/false)
- ``elapsed_s``: Wall-clock time [s]
- ``max_div``: Maximum divergence of final field [1/s]
- ``mean_div``: Mean divergence [1/s]

**Multi-Parameter Sweep:**

For factorial parameter combinations:

.. code-block:: bash

    python3 tools/parameter_sensitivity.py --inputs regtest/gaussian_hill/inputs.i \
        --multi-param z0 alpha_v \
        --ranges 0.001 0.1 0.5 2.0 \
        --steps 5 5 \
        --output sensitivity_multi.csv

This creates 5×5 = 25 solver runs exploring the (z₀, α_v) parameter space.

**Available Parameters:**

.. list-table::
   :header-rows: 1
   :widths: 20 30 50

   * - Parameter
     - Typical Range
     - Physical Interpretation
   * - ``z0``
     - [0.001, 1.0] m
     - Aerodynamic roughness length; higher values increase surface drag
   * - ``alpha_h``
     - [0.5, 2.0]
     - Horizontal mass-consistent correction weight; affects horizontal divergence damping
   * - ``alpha_v``
     - [0.5, 2.0]
     - Vertical mass-consistent correction weight; affects vertical velocity magnitude
   * - ``z_ref``
     - [5, 50] m
     - Reference height for log-law initialization
   * - ``domain_height``
     - [50, 500] m
     - Vertical domain extent above terrain
   * - ``U_ref``, ``V_ref``
     - Various m/s
     - Reference wind components

**Physical Insight:**

- **z₀ sensitivity:** Low z₀ (0.001 m, smooth water) produces weak surface drag and rapid
  vertical wind increase. High z₀ (1.0 m, forest) produces strong surface drag. Typical
  applications use logarithmic spacing to explore this range.

- **α_h, α_v sensitivity:** These penalty coefficients control the stiffness of the
  mass-consistent correction. Higher values force stronger correction to divergence-free
  constraint but may increase computational cost. Default α_h = α_v = 1.0 is optimal for
  most applications.

- **Nonlinear effects:** Wind field often exhibits nonlinear sensitivity to z₀ and
  terrain-related parameters; factorial sweeps help identify interaction effects.

**Sensitivity Computation**

The framework also supports programmatic sensitivity analysis::

    std::vector<ParameterSensitivity> sensitivities;
    
    sensitivities.push_back(
        SensitivityAnalyzer::AnalyzeParameterSensitivity(
            "z0", 0.1, 20.0));  // ±20% variation
    
    sensitivities.push_back(
        SensitivityAnalyzer::AnalyzeParameterSensitivity(
            "alpha_v", 1.0, 50.0));  // ±50% variation
    
    SensitivityAnalyzer::GenerateSensitivityReport(
        sensitivities, "sensitivity_report.txt");

**Interpretation**

- **sensitivity_index > 0.5**: Parameter is significant; document well
- **sensitivity_index < 0.2**: Parameter has minimal effect; may be hard-coded
- **output_variation > 1.0**: Parameter has nonlinear effects

Optimization Suggestions
~~~~~~~~~~~~~~~~~~~~~~~~

Based on performance profiling results, automatic optimization suggestions are generated:

**GPU Optimization Recommendations**

Suggestions based on architecture profiling::

    std::vector<std::string> suggestions = 
        OptimizationHelper::SuggestGPUOptimizations(metrics);

Common suggestions include:
- **Memory bandwidth**: Increase block size or reduce memory traffic
- **Register pressure**: Simplify kernels if register spills detected
- **Occupancy**: Adjust block dimensions for better GPU occupancy
- **Load balancing**: Redistribute work if uneven distribution detected

**Feature Enablement Decision**

Determines if a feature justifies its computational cost::

    bool worth_enabling = OptimizationHelper::IsFeatureWorthEnabling(
        metrics_with_feature,
        metrics_without_feature,
        overhead_threshold);  // Default 0.05 = 5% overhead

Criteria:
- Feature overhead ≤ threshold: ENABLE
- Feature overhead > threshold: DISABLE or OPTIMIZE

**Validation Output Format**

The validation framework produces standardized output for integration into CI/CD systems:

Summary Report Structure::

    ===================================
    VALIDATION & OPTIMIZATION REPORT
    ===================================
    
    PERFORMANCE METRICS
    -------------------
    Average Wall Time:          125.3 ms
    Average CPU Time:           240.5 ms
    Peak Memory:                256.2 MB
    Computational Throughput:   1.2 GFLOPS
    
    PHYSICAL VALIDATION
    -------------------
    Wind Field Validation:      PASS ✓
      Max Error:                0.8%
      Mean Error:               0.3%
      RMS Error:                0.5%
    
    Bounds Checking:            PASS ✓
    Mass Conservation:          PASS ✓
      Max Divergence:           1.2e-14
    
    PARAMETER SENSITIVITY
    ---------------------
    damping_coefficient:        SENSITIVE (index 0.73)
      Output Variation:         1.8% per parameter %
    
    transition_height_scale:    INSENSITIVE (index 0.12)
      Output Variation:         0.1% per parameter %
    
    OPTIMIZATION SUGGESTIONS
    ------------------------
    1. GPU Occupancy: Increase block size from 128 to 256
    2. Feature overhead: diurnal_roughness costs 2.1% (acceptable)

**Integration with CI/CD**

Layer 3 validation integrates with continuous integration:

1. **Performance Regression Detection**: Flag features causing >5% slowdown
2. **Correctness Regression Detection**: Flag features with validation failures
3. **Sensitivity Analysis Tracking**: Monitor parameter impact changes
4. **GPU Compatibility**: Verify features work on all GPU backends

Example CI configuration::

    # Run validation for all features
    ctest --test-dir build -R validation --output-on-failure
    
    # Generate performance report
    ./build/wind_solver --validate-all --performance-report

**Validation Requirements for Production Release**

Before a feature is considered production-ready:

✅ **Validation Checklist**

- [ ] Physical bounds satisfied for all test cases
- [ ] Mass conservation error < 1e-10 (machine precision)
- [ ] Wind field validation against reference: RMS error < 2%
- [ ] Parameter sensitivity analysis complete
- [ ] All parameters documented with sensitivity index
- [ ] Performance overhead < 5% for disabled features
- [ ] GPU code verified on CUDA, HIP, and SYCL backends
- [ ] Edge cases tested (boundary layers, terrain singularities)
- [ ] Documentation updated with validation results

**Files and References**

- ``src/validation_optimization.H`` — Main validation/optimization framework
- ``docs/advanced_solver_features.rst`` — Feature specifications
- ``docs/implementation_status.rst`` — Overall implementation tracking
- Regression tests in ``regtest/`` — Automated validation suite

**Next Steps**

1. Implement ValidationResult computation for wind field comparison
2. Integrate bounds checking into wind solver main loop
3. Implement mass conservation diagnostics
4. Create parameter sensitivity test suite
5. Generate automated performance reports in CI/CD pipeline
6. Document best practices for feature developers
