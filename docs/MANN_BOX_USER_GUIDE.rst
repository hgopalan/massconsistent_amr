MANN BOX USER GUIDE: Complete Reference
=======================================


**Last Updated**: June 4, 2026  
**Status**: Complete Implementation (Production Ready)

----


TABLE OF CONTENTS
-----------------


1. `Quick Start <#quick-start>`_
2. `Building & Installation <#building--installation>`_
3. `Configuration <#configuration>`_
4. `Running Simulations <#running-simulations>`_
5. `Input Parameters <#input-parameters>`_
6. `Output Files <#output-files>`_
7. `Validation & Diagnostics <#validation--diagnostics>`_
8. `Integration with External Tools <#integration-with-external-tools>`_
9. `Troubleshooting <#troubleshooting>`_
10. `Best Practices <#best-practices>`_

----


QUICK START
-----------


Minimal Configuration
~~~~~~~~~~~~~~~~~~~~~


.. code-block:: bash

    # 1. Clone repository
    git clone --recurse-submodules https://github.com/hgopalan/massconsistent_amr.git
    cd massconsistent_amr

    # 2. Build (CPU-only)
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel

    # 3. Run example
    ./build/wind_solver regtest/gaussian_hill/inputs.i


Verify Installation
~~~~~~~~~~~~~~~~~~~


.. code-block:: bash

    # Run basic tests
    python3 test/mann_box_phase3_test.py       # Spectral Tensor Tests: ✓
    python3 test/mann_box_phase4_test.py       # Stability Physics Tests: ✓
    python3 test/mann_box_phase6_test.py       # Advanced Features Tests: ✓
    python3 test/mann_box_phase7_validation.py # Validation Diagnostics Tests: ✓


----


BUILDING & INSTALLATION
-----------------------


System Requirements
~~~~~~~~~~~~~~~~~~~


**Compiler**: GCC 9+, Clang 10+, or MSVC 2019+  
**CMake**: 3.20+  
**C++17**: Required

CPU Build (Default)
~~~~~~~~~~~~~~~~~~~


.. code-block:: bash

    cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++
    cmake --build build --parallel 4


GPU Build (CUDA)
~~~~~~~~~~~~~~~~


.. code-block:: bash

    cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DMASSCONSISTENT_GPU_BACKEND=CUDA
    cmake --build build --parallel 4


Python Bindings
~~~~~~~~~~~~~~~


.. code-block:: bash

    cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
    cd build && make -j4
    pip install -e .


----


CONFIGURATION
-------------


Input File Format
~~~~~~~~~~~~~~~~~


Create ``inputs.i``:

.. code-block:: ini

    # Domain settings
    geometry.prob_lo = 0 0 0
    geometry.prob_hi = 500 500 300
    amr.n_cell = 50 50 30

    # Mann Box turbulence model
    wind_solver.enable_mann_box = 1
    wind_solver.mann_box_model = "spectral_tensor"

    # Spectral tensor features
    wind_solver.mann_box_coherence_decay = 5.0
    wind_solver.mann_box_phase3_enabled = 1

    # Temporal synthesis & stability features
    wind_solver.mann_box_temporal_enabled = 1
    wind_solver.mann_box_stability_model = "richardson"

    # Roughness & Wind Veer features
    wind_solver.mann_box_roughness_class = 2
    wind_solver.mann_box_veer_model = "power_law"

    # Validation & Diagnostics features
    wind_solver.enable_validation_diagnostics = 1
    wind_solver.export_csv = 1
    wind_solver.export_bts = 1

    # Output
    amr.plot_file = "plt_wind"
    amr.check_file = "chk_wind"


Parameter Reference
~~~~~~~~~~~~~~~~~~~


.. list-table::
   :header-rows: 1

   * - Parameter
     - Type
     - Default
     - Range
     - Description
   * - ``mann_box_model``
     - string
     - "spectral_tensor"
     - 
     - Turbulence model
   * - ``coherence_decay``
     - real
     - 5.0
     - 1–10
     - Spatial coherence decay [m⁻¹]
   * - ``stability_model``
     - string
     - "richardson"
     - 
     - Stability classification
   * - ``roughness_class``
     - int
     - 2
     - 0–8
     - Land use class (0=water, 8=mountains)
   * - ``veer_model``
     - string
     - "power_law"
     - 
     - Wind direction veer model
   * - ``enable_validation_diagnostics``
     - bool
     - 0
     - 
     - Enable validation diagnostics
   * - ``export_csv``
     - bool
     - 0
     - 
     - Export CSV files
   * - ``export_bts``
     - bool
     - 0
     - 
     - Export OpenFAST BTS format


----


RUNNING SIMULATIONS
-------------------


Single Case
~~~~~~~~~~~


.. code-block:: bash

    ./build/wind_solver regtest/gaussian_hill/inputs.i


With Parameter Override
~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: bash

    ./build/wind_solver regtest/gaussian_hill/inputs.i \
      wind_solver.coherence_decay=7.0 \
      wind_solver.roughness_class=3


MPI Parallelization
~~~~~~~~~~~~~~~~~~~


.. code-block:: bash

    mpirun -np 4 ./build/wind_solver regtest/gaussian_hill/inputs.i


GPU Execution
~~~~~~~~~~~~~


.. code-block:: bash

    CUDA_VISIBLE_DEVICES=0 ./build/wind_solver inputs.i


----


INPUT PARAMETERS
----------------


Domain Parameters
~~~~~~~~~~~~~~~~~


.. code-block:: ini

    # Computational domain
    geometry.prob_lo = 0 0 0          # Lower corner [m]
    geometry.prob_hi = 500 500 300    # Upper corner [m]
    amr.n_cell = 50 50 30             # Grid cells per dimension


Boundary Conditions
~~~~~~~~~~~~~~~~~~~


.. code-block:: ini

    # Wind profile initialization
    wind_solver.init_profile = "log_law"      # log_law | uniform | raws | hrrr
    wind_solver.reference_height = 10.0       # Reference height [m]
    wind_solver.reference_wind_speed = 10.0   # Reference wind speed [m/s]
    wind_solver.surface_roughness = 0.1       # Roughness length [m]


Spectral Tensor Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: ini

    wind_solver.mann_box_phase3_enabled = 1
    wind_solver.mann_coherence_decay = 5.0    # Coherence decay [m⁻¹]
    wind_solver.mann_spectrum_model = "von_karman"  # von_karman | kaimal


Temporal Synthesis
~~~~~~~~~~~~~~~~~~


.. code-block:: ini

    wind_solver.mann_box_temporal_enabled = 1
    wind_solver.mann_stability_model = "richardson"
    wind_solver.mann_richardson_number = auto  # Auto-compute from Ri_b


Roughness & Veer
~~~~~~~~~~~~~~~~


.. code-block:: ini

    wind_solver.mann_box_roughness_class = 2
    wind_solver.mann_box_veer_model = "power_law"
    wind_solver.mann_box_veer_gradient = 15.0  # °/100m


Diagnostics
~~~~~~~~~~~


.. code-block:: ini

    wind_solver.enable_validation_diagnostics = 1
    wind_solver.export_csv = 1
    wind_solver.export_bts = 1
    wind_solver.export_netcdf = 0
    wind_solver.diagnostic_output_dir = "diagnostics"


----


OUTPUT FILES
------------


Plot Files (AMReX Format)
~~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: text

    plt_wind/
    ├── Header
    ├── Level_0/
    │   ├── Cell_H
    │   ├── Cell_data_0
    │   └── Cell_data_1
    └── ...


**Variables**:
- ``velocity_x``, ``velocity_y``, ``velocity_z`` — Velocity components [m/s]
- ``pressure`` — Corrected pressure [Pa]
- ``density`` — Air density [kg/m³]

CSV Export
~~~~~~~~~~


.. code-block:: text

    diagnostics/
    ├── spectral_data.csv       # PSD vs frequency
    ├── coherence_function.csv  # Coherence vs separation
    ├── statistics_summary.csv  # Integral statistics
    └── autocorrelation.csv     # Temporal correlation


BTS Export (OpenFAST)
~~~~~~~~~~~~~~~~~~~~~


.. code-block:: text

    diagnostics/
    ├── turbulence.bts          # Binary time series (OpenFAST format)
    └── turbulence.meta         # Metadata


----


VALIDATION & DIAGNOSTICS
------------------------


Validation Metrics
~~~~~~~~~~~~~~~~~~


.. code-block:: text

    Spectral validation:
      ├─ Energy conservation: |∫S(f)df - σ²| < 5%
      ├─ Peak frequency: f_peak in expected range
      └─ Spectral decay: S(f) ∝ f^(-5/3) at high frequencies

    Statistical validation:
      ├─ Turbulence intensity: TI = σ/U_mean
      ├─ Anisotropy: v/u ∈ [0.6, 0.9], w/u ∈ [0.3, 0.7]
      └─ TKE: = 0.5·(σ_u² + σ_v² + σ_w²)

    Energy conservation:
      ├─ Kinetic energy: Consistent across components
      ├─ Budget: Production ≈ Dissipation
      └─ Cross-component: Preserved throughout analysis


Running Validation Suite
~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: bash

    # Run all tests
    python3 test/mann_box_phase7_validation.py

    # Expected output:
    # ✓ 28/28 tests passing (100%)
    # ✓ Spectral power density diagnostics
    # ✓ Energy balance validation
    # ✓ Coherence function analysis
    # ✓ Export utilities verification


----


INTEGRATION WITH EXTERNAL TOOLS
-------------------------------


OpenFAST/TurbSim
~~~~~~~~~~~~~~~~


.. code-block:: bash

    # 1. Generate BTS file with Mann Box
    ./build/wind_solver inputs.i --export-bts

    # 2. Use in OpenFAST
    # In .fst file:
    CompIW = 4  ! IEC spectral turbulence
    FileName_Uni = "diagnostics/turbulence"  ! Without extension

    # 3. Run simulation
    openfast model.fst


Post-Processing with Python
~~~~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: python

    import pandas as pd
    import matplotlib.pyplot as plt

    # Load spectral data
    spec = pd.read_csv("diagnostics/spectral_data.csv")

    # Plot
    plt.loglog(spec['Frequency_Hz'], spec['PSD_u_m2s2Hz'])
    plt.xlabel('Frequency [Hz]')
    plt.ylabel('PSD [m²/s²/Hz]')
    plt.show()

    # Load statistics
    stats = pd.read_csv("diagnostics/statistics_summary.csv", index_col=0)
    print(f"Turbulence Intensity: {stats.loc['Turbulence_Intensity', 'Value']:.2%}")


FLORIS Wind Farm Simulation
~~~~~~~~~~~~~~~~~~~~~~~~~~~


.. code-block:: python

    from floris.core import Core

    # Initialize FLORIS with Mann Box winds
    floris = Core("floris_input.json")
    wind_data = np.genfromtxt("diagnostics/wind_extract_30m.csv", delimiter=',')
    floris.set_wind_speeds(wind_data[:, 0])
    floris.set_wind_directions(wind_data[:, 1])
    floris.wake_model_options['wind_speed'] = wind_data[:, 0]


----


TROUBLESHOOTING
---------------


Build Issues
~~~~~~~~~~~~


**Problem**: CMake cannot find AMReX  
**Solution**: 
.. code-block:: bash

    cmake -S . -B build \
      -DMASSCONSISTENT_USE_VENDORED_AMREX=ON


**Problem**: CUDA compilation error  
**Solution**:
.. code-block:: bash

    cmake --build build --verbose
    # Check CUDA toolkit version: nvcc --version
    # Should be 12.0 or higher


Runtime Issues
~~~~~~~~~~~~~~


**Problem**: Segmentation fault  
**Solution**:
.. code-block:: bash

    # Run with debug symbols
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    gdb ./build/wind_solver


**Problem**: Negative pressure values  
**Solution**:
.. code-block:: ini

    # Increase multigrid solver accuracy
    amr.mg_bottom_solver = "bicg"
    amr.mg_max_iter = 100
    amr.mg_verbose = 1


Performance Issues
~~~~~~~~~~~~~~~~~~


**Problem**: Slow execution  
**Solution**:
.. code-block:: bash

    # Compile with optimizations
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -march=native"

    # Profile execution
    ./build/wind_solver inputs.i --verbose 2>&1 | grep "Timing"


----


BEST PRACTICES
--------------


Simulation Setup
~~~~~~~~~~~~~~~~


1. **Start simple**: Use Gaussian hill test case first
2. **Verify convergence**: Check grid refinement
3. **Validate output**: Use Phase 7 diagnostics
4. **Archive configuration**: Save inputs.i with results

Parameter Selection
~~~~~~~~~~~~~~~~~~~


1. **Coherence decay**: Typical 5–10 m⁻¹
2. **Roughness class**: Match site characteristics
3. **Stability**: Auto-compute Richardson number
4. **Time step**: dt < L_u / (10·U_mean)

Validation Workflow
~~~~~~~~~~~~~~~~~~~


.. code-block:: text

    1. Run simulation with diagnostics enabled
       ↓
    2. Check Phase 7 output files
       ├─ PSD: Should match theory
       ├─ TI: Should be in 0.08–0.20 range
       └─ Statistics: Compare to IEC standards
       ↓
    3. Generate validation report
       ↓
    4. Compare with site measurements (if available)
       ↓
    5. Archive and document


Production Deployment
~~~~~~~~~~~~~~~~~~~~~


1. Create parameter template file
2. Version control inputs.i
3. Enable all diagnostics initially
4. Run overnight tests on target hardware
5. Profile and optimize
6. Deploy with monitoring

----


SUPPORT & RESOURCES
-------------------


- **Documentation**: https://hgopalan.github.io/massconsistent_amr/
- **GitHub Issues**: https://github.com/hgopalan/massconsistent_amr/issues
- **Test Cases**: ``test/mann_box_phase*_test.py``
- **Examples**: ``regtest/*/inputs.i``

----


**Guide Version**: 1.0  
**Last Updated**: June 4, 2026
