MANN BOX BEST PRACTICES GUIDE
=============================


**Version**: 1.0  
**Target Audience**: Wind energy engineers, atmospheric modelers  
**Scope**: Phases 1-7 implementation

----


TABLE OF CONTENTS
-----------------


1. `Model Selection <#model-selection>`_
2. `Parameter Tuning <#parameter-tuning>`_
3. `Validation Workflow <#validation-workflow>`_
4. `Performance Optimization <#performance-optimization>`_
5. `Common Mistakes <#common-mistakes>`_
6. `Troubleshooting <#troubleshooting>`_
7. `Production Deployment <#production-deployment>`_

----


MODEL SELECTION
---------------


When to Use Mann Box Turbulence Model
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


**Ideal Use Cases**:
- Wind farm siting and micrositing studies
- Turbine load calculations
- Complex terrain simulations
- Time-resolved fluctuation requirements
- Integration with OpenFAST/FLORIS

**Not Recommended For**:
- Real-time operational forecasting (requires too much computation)
- Ensemble probabilistic analysis (single realization only)
- Very large domains (>10 km) without MPI
- Highly stratified conditions (stable layer > 500 m)

Phase Selection
~~~~~~~~~~~~~~~


.. list-table::
   :header-rows: 1

   * - Phase
     - Use Case
     - Recommended
   * - 2
     - Basic log-law profiles
     - ✓ All cases
   * - 3
     - Full spectral tensor
     - ✓ If time-resolved output needed
   * - 4
     - Temporal synthesis
     - ✓ If stability matters
   * - 5
     - Terrain adaptation
     - ✓ Complex topography
   * - 6
     - Roughness/veer
     - ✓ Non-neutral conditions
   * - 7
     - Validation & export
     - ✓ Always (free diagnostics)


----


PARAMETER TUNING
----------------


Coherence Decay Rate
~~~~~~~~~~~~~~~~~~~~


**Recommended Range**: 5–10 m⁻¹

.. code-block:: ini

    # Typical by surface
    water            : 2.0–3.0    ! High coherence
    grassland        : 4.0–6.0    ! Moderate decay
    forest           : 6.0–8.0    ! Rapid decay
    urban            : 8.0–12.0   ! Very rapid decay


**Tuning Strategy**:
1. Start with 5.0 for unknown sites
2. Increase (6–8) if model underpredicts variance
3. Decrease (3–5) if model overpredicts variance
4. Validate against site measurements

Surface Roughness Length (z₀)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: cpp

    // Roughness classes (Phase 6)
    0: Water          z₀ = 0.0002 m
    1: Snow/ice       z₀ = 0.003 m
    2: Grassland      z₀ = 0.05–0.1 m
    3: Crops          z₀ = 0.1–0.2 m
    4: Shrubs         z₀ = 0.2–0.5 m
    5: Forest edges   z₀ = 0.5–1.0 m
    6: Dense forest   z₀ = 1.0–2.0 m
    7: Urban          z₀ = 1.0–2.0 m
    8: Mountains      z₀ = 2.0–4.0 m


**Site-Specific Tuning**:
- Compare z₀ with IEC 61400-1 tables
- Use satellite imagery for classification
- Validate against 10 m wind speed observations

Veer Gradient
~~~~~~~~~~~~~


.. code-block:: ini

    # Wind direction rotation with height (Phase 6)
    water            : 0°–5°/100m
    grassland        : 5°–10°/100m
    forest           : 10°–20°/100m
    urban            : 15°–30°/100m
    complex terrain  : 20°–40°/100m


**Tuning Method**:
1. Use IEC 61400-1 default: 10°/100m
2. Increase if observations show stronger veer
3. Decrease if observations show weaker veer

Stability Classification
~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: ini

    # Use Richardson number (Phase 4)
    Ri_b < -0.1  : Unstable (buoyancy-driven)
    -0.1 ≤ Ri_b ≤ 0.1  : Neutral (default)
    Ri_b > 0.1   : Stable (suppressed vertical mixing)


**Guidance**:
- Auto-compute from bulk Richardson number
- Only override if site-specific data available
- Stable regime requires careful tuning

----


VALIDATION WORKFLOW
-------------------


Pre-Simulation Checklist
~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: text

    □ Input file syntax correct
    □ Domain size > 5 × terrain height
    □ Grid resolution adequate (Δx < L_u/10)
    □ Time step stable (CFL < 0.5)
    □ Boundary conditions realistic
    □ Initial conditions physically reasonable


Post-Simulation Validation
~~~~~~~~~~~~~~~~~~~~~~~~~~


**1. Check Output Files**

.. code-block:: bash

    # Verify plotfile exists and has data
    ls -lah plt_wind/Header
    # Check for NaN/Inf values
    python3 -c "
    import numpy as np
    # Load AMReX data and check statistics
    "


**2. Run Phase 7 Diagnostics**

.. code-block:: bash

    # Enable in inputs.i:
    wind_solver.enable_validation_diagnostics = 1

    # Inspect outputs:
    ls diagnostics/
    cat diagnostics/statistics_summary.csv


**3. Check Key Metrics**

.. code-block:: csv

    # Expected ranges:
    Turbulence_Intensity: 0.08–0.25 (fraction)
    v/u_RMS_Ratio: 0.6–0.9 (fraction)
    w/u_RMS_Ratio: 0.3–0.7 (fraction)
    Peak_Frequency: 0.01–0.2 Hz
    Integral_Length_Scale: 50–1000 m


**4. Compare to Standards**

.. code-block:: python

    import pandas as pd

    # Load diagnostics
    stats = pd.read_csv('diagnostics/statistics_summary.csv', index_col=0)
    ti = stats.loc['Turbulence_Intensity', 'Value']

    # IEC 61400-1:2019 check
    iec_ti = 0.16  # Normal turbulence, 10 m/s
    error = abs(ti - iec_ti) / iec_ti
    print(f"TI error vs IEC: {error*100:.1f}%")
    assert error < 0.3, "TI outside acceptable range"


----


PERFORMANCE OPTIMIZATION
------------------------


Grid Resolution Strategy
~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: ini

    # Start coarse, refine critical regions
    amr.n_cell = 32 32 16         ! Coarse (fast)
    amr.max_level = 1              ! AMR level

    # Production:
    amr.n_cell = 64 64 32          ! Medium
    amr.max_level = 2              ! AMR levels 0-1

    # High precision:
    amr.n_cell = 128 128 64        ! Fine (slow)
    amr.max_level = 3              ! AMR levels 0-2


**Rule of Thumb**:
.. code-block:: text

    Computational cost ∝ n_cell³
    2× resolution → 8× runtime


Multigrid Solver Tuning
~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: ini

    # Reduce iterations (faster, less accurate)
    amr.mg_max_iter = 50           ! Default 100

    # Increase for better accuracy
    amr.mg_max_iter = 200

    # Bottleneck solver choice
    amr.mg_bottom_solver = "bicg"  ! Default
    # Options: "bi-cgstab", "cg", "bicg"


Parallel Scaling
~~~~~~~~~~~~~~~~


.. code-block:: bash

    # MPI scaling (weak scaling expected)
    for np in 1 2 4 8 16; do
        time mpirun -np $np ./build/wind_solver inputs.i
    done
    # Plot: Runtime vs number of processes


**Expected**:
- Good scaling up to 10–100 cores
- Diminishing returns beyond 100 cores
- I/O becomes bottleneck at many cores

GPU Acceleration
~~~~~~~~~~~~~~~~


.. code-block:: bash

    # CUDA backend selection
    cmake -DMASSCONSISTENT_GPU_BACKEND=CUDA

    # Expected speedup: 5–10× vs CPU
    # Best for large grids (> 10⁶ cells)


----


COMMON MISTAKES
---------------


Mistake 1: Using Neutral Stability Everywhere
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


❌ **Wrong**:
.. code-block:: ini

    wind_solver.stability_model = "neutral"  # Always


✓ **Correct**:
.. code-block:: ini

    wind_solver.stability_model = "richardson"  # Auto-detect
    # Verify: check z_imbalance, wind_veer outputs


Mistake 2: Ignoring Time Step Stability
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


❌ **Wrong**:
.. code-block:: ini

    amr.dt = 1.0  # 1 second per step (too large!)


✓ **Correct**:
.. code-block:: ini

    # Set based on grid and wind speed
    # CFL < 0.5: dt < (Δx / max_wind_speed) × 0.5
    # Typically 0.01–0.1 seconds


Mistake 3: Over-Tuning Coherence Decay
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


❌ **Wrong**:
.. code-block:: ini

    wind_solver.coherence_decay = 0.5  # Too low → no correlation decay
    wind_solver.coherence_decay = 50.0  # Too high → instant decay


✓ **Correct**:
.. code-block:: ini

    wind_solver.coherence_decay = 5.0   # Start here
    # Fine-tune ±2 based on site data


Mistake 4: Domain Too Small
~~~~~~~~~~~~~~~~~~~~~~~~~~~


❌ **Wrong**:
.. code-block:: ini

    geometry.prob_hi = 200 200 100    # Only 2× terrain height


✓ **Correct**:
.. code-block:: ini

    geometry.prob_hi = 1000 1000 300  # At least 5× terrain height
    # Allows proper wind adjustment


----


TROUBLESHOOTING
---------------


Negative Pressure Values
~~~~~~~~~~~~~~~~~~~~~~~~


**Symptom**: Pressure fluctuations > ±50 Pa

**Cause**: Multigrid solver not converged

**Solution**:
.. code-block:: ini

    amr.mg_bottom_solver = "bicg"
    amr.mg_max_iter = 150           ! Increase from 100
    amr.mg_verbose = 1              ! Diagnose convergence


Divergent Solution
~~~~~~~~~~~~~~~~~~


**Symptom**: NaN values in output

**Causes**:
- Time step too large
- Boundary condition unstable
- Grid too coarse

**Solution**:
.. code-block:: bash

    # Reduce time step
    dt = 0.5 * dt_original

    # Check stability criterion
    CFL = max_wind_speed * dt / dx
    # Must satisfy: CFL < 0.5


Slow Convergence
~~~~~~~~~~~~~~~~


**Symptom**: > 1000 multigrid iterations per step

**Causes**:
- Ill-conditioned pressure equation
- Terrain too steep
- Grid too anisotropic

**Solution**:
.. code-block:: ini

    # Increase relaxation parameter
    amr.mg_smoother_scale = 1.5

    # Use faster bottom solver
    amr.mg_bottom_solver = "cg"


Memory Exhaustion
~~~~~~~~~~~~~~~~~


**Symptom**: "Out of memory" error

**Causes**:
- Grid too fine
- Too many AMR levels
- Checkpoint frequency too high

**Solution**:
.. code-block:: ini

    # Reduce grid size
    amr.n_cell = 64 64 32  # From 128 128 64

    # Reduce checkpoints
    amr.check_int = 100000  # Less frequent

    # Reduce diagnostic output
    wind_solver.diagnostic_frequency = 50  # Every 50 steps


----


PRODUCTION DEPLOYMENT
---------------------


Simulation Checklist
~~~~~~~~~~~~~~~~~~~~


.. code-block:: bash

    □ Code built with Release mode (-O3)
    □ All tests passing (28/28 Phase 7 tests)
    □ Validation report generated
    □ Parameters documented in inputs.i
    □ Boundary conditions verified
    □ Output directory structure ready
    □ Disk space > 2× expected output size
    □ Wall-clock time estimate obtained


Documentation Protocol
~~~~~~~~~~~~~~~~~~~~~~


Create ``simulation_log.md`` for every run:

.. code-block:: markdown

    # Simulation Log

    ## Metadata
    - Date: 2026-06-04
    - Domain: 1000×1000×300 m
    - Grid: 64×64×32 cells
    - Phases: 3-7 enabled

    ## Parameters
    - Surface roughness: z₀ = 0.1 m
    - Coherence decay: 5.0 m⁻¹
    - Stability: Richardson (auto)

    ## Validation Results
    - TI: 0.12 (±0.02 IEC 61400-1)
    - Anisotropy: v/u = 0.80, w/u = 0.60 ✓
    - Energy conservation: ±2% ✓
    - Peak frequency: 0.033 Hz ✓

    ## Output Files
    - plt_wind/ → Corrected wind field
    - diagnostics/spectral_data.csv → Phase 7 export
    - diagnostics/turbulence.bts → OpenFAST format


Archive & Backup
~~~~~~~~~~~~~~~~


.. code-block:: bash

    # After successful run
    tar czf simulation_2026-06-04.tar.gz \
        inputs.i \
        plt_wind/ \
        diagnostics/ \
        simulation_log.md

    # Verify integrity
    tar tzf simulation_2026-06-04.tar.gz | wc -l


----


QUALITY ASSURANCE
-----------------


Code Review Before Production
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: cpp

    // Check for:
    1. GPU device memory management ✓
    2. Boundary condition consistency ✓
    3. Phase integration completeness ✓
    4. Error handling robustness ✓
    5. Physical realizability ✓


Regression Testing
~~~~~~~~~~~~~~~~~~


.. code-block:: bash

    # Run full test suite
    python3 test/mann_box_phase3_test.py
    python3 test/mann_box_phase4_test.py
    python3 test/mann_box_phase6_test.py
    python3 test/mann_box_phase7_validation.py

    # Expected: 90+/93 tests passing


Performance Profiling
~~~~~~~~~~~~~~~~~~~~~


.. code-block:: bash

    # Time each phase
    time ./build/wind_solver inputs.i 2>&1 | grep Timing

    # Profile hotspots
    perf record -F 99 ./build/wind_solver inputs.i
    perf report


----


SUPPORT RESOURCES
-----------------


- **Documentation**: https://hgopalan.github.io/massconsistent_amr/
- **GitHub Repo**: https://github.com/hgopalan/massconsistent_amr
- **Test Cases**: ``test/mann_box_phase*.py``
- **Examples**: ``regtest/*/inputs.i``

----


**Best Practices Guide Version**: 1.0  
**Last Updated**: June 4, 2026
