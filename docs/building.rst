.. _building:

Building
========

Prerequisites
-------------

* **CMake** ≥ 3.20
* **C++17** compiler — GCC ≥ 9, Clang ≥ 10, MSVC ≥ 19.20, or Intel icpx
* **Git** (for submodule checkout)
* **Ninja** (recommended; optional — the default generator also works)

Optional:

* **MPI** — for multi-process parallel runs
* **CUDA Toolkit** ≥ 11 — for NVIDIA GPU execution
* **AMD ROCm/HIP** ≥ 5 — for AMD GPU execution
* **Intel oneAPI DPC++** — for Intel GPU/SYCL execution

Cloning
-------

Always clone with ``--recurse-submodules`` to fetch the bundled AMReX library::

    git clone --recurse-submodules https://github.com/hgopalan/massconsistent_amr.git
    cd massconsistent_amr

If you already cloned without submodules::

    git submodule update --init --recursive

CPU Build (Linux / macOS)
--------------------------

.. code-block:: bash

    cmake -S . -B build \
          -G Ninja \
          -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel

The ``wind_solver`` executable is placed in ``build/``.

CPU Build (Windows)
--------------------

.. code-block:: bat

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release -j 4

CUDA Build
----------

.. code-block:: bash

    cmake -S . -B build \
          -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DMASSCONSISTENT_GPU_BACKEND=CUDA \
          -DAMReX_CUDA_ARCH="8.0"
    cmake --build build --parallel

Replace ``8.0`` with the compute capability of your GPU (e.g. ``7.5`` for
Turing, ``9.0`` for Hopper).

HIP / ROCm Build
-----------------

.. code-block:: bash

    cmake -S . -B build \
          -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_C_COMPILER=clang \
          -DCMAKE_CXX_COMPILER=clang++ \
          -DMASSCONSISTENT_GPU_BACKEND=HIP \
          -DAMReX_AMD_ARCH="gfx90a"
    cmake --build build --parallel

SYCL / Intel oneAPI Build
--------------------------

.. code-block:: bash

    source /opt/intel/oneapi/setvars.sh
    cmake -S . -B build \
          -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_C_COMPILER=icx \
          -DCMAKE_CXX_COMPILER=icpx \
          -DMASSCONSISTENT_GPU_BACKEND=SYCL
    cmake --build build --parallel

MPI Build
---------

Append ``-DMASSCONSISTENT_ENABLE_MPI=ON`` to any of the above CMake commands::

    cmake -S . -B build -DMASSCONSISTENT_ENABLE_MPI=ON
    cmake --build build --parallel

CMake Options Reference
-----------------------

.. list-table::
   :header-rows: 1
   :widths: 35 15 50

   * - Option
     - Default
     - Description
   * - ``MASSCONSISTENT_USE_VENDORED_AMREX``
     - ``ON``
     - Use the bundled AMReX git submodule.  Set ``OFF`` to supply an
       installed AMReX via ``-DAMReX_DIR=…``.
   * - ``MASSCONSISTENT_ENABLE_MPI``
     - ``OFF``
     - Enable MPI for distributed-memory parallelism.
   * - ``MASSCONSISTENT_GPU_BACKEND``
     - ``NONE``
     - GPU back-end: ``NONE`` (CPU-only), ``CUDA``, ``HIP``, or ``SYCL``.
   * - ``MASSCONSISTENT_BUILD_DOCS``
     - ``OFF``
     - Enable the ``docs`` CMake target (requires ``sphinx-build``).

Building Documentation Locally
--------------------------------

Install the Python dependencies and run Sphinx::

    pip install -r docs/requirements.txt
    cd docs && make html

Open ``docs/_build/html/index.html`` in a browser.

Alternatively, enable the CMake docs target::

    cmake -S . -B build -DMASSCONSISTENT_BUILD_DOCS=ON
    cmake --build build --target docs
