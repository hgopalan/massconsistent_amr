# Installation Guide

This guide provides detailed instructions for installing **massconsistent_amr** and its Python dependencies using Conda, Anaconda, or other package managers.

## Table of Contents

- [Quick Start](#quick-start)
- [Environment Options](#environment-options)
- [Installation Methods](#installation-methods)
- [Python-Only Installation](#python-only-installation)
- [Full Build Setup](#full-build-setup)
- [Verifying Your Installation](#verifying-your-installation)
- [Troubleshooting](#troubleshooting)
- [Optional Packages](#optional-packages)

## Quick Start

### For Python Tools Only (No Compilation)

```bash
# Clone the repository
git clone --recurse-submodules https://github.com/hgopalan/massconsistent_amr.git
cd massconsistent_amr

# Create and activate the conda environment
conda env create -f environment.yml
conda activate massconsistent_amr

# Now you can use Python tools and utilities
python src/python/example_floris_export.py
```

### For Development/Building C++

```bash
# Create the development environment (includes compilers and build tools)
conda env create -f environment-dev.yml
conda activate massconsistent_amr-dev

# Build the project
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
cmake --build build --parallel

# Run tests
ctest -j 4
```

### For Everything (Full Suite)

```bash
# Create environment with all optional packages (FLORIS, PyWake, etc.)
conda env create -f environment-full.yml
conda activate massconsistent_amr-full

# Build and run with full capabilities
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
cmake --build build --parallel
```

## Environment Options

The repository includes three pre-configured conda environment files to suit different needs:

### 1. `environment.yml` - Minimal/Runtime Environment
**Use this for:**
- Running Python tools and utilities
- Using wind field data from pre-built binaries
- Analyzing results with post-processing tools

**Includes:**
- Python 3.9+
- NumPy, SciPy, Matplotlib
- NetCDF4 support
- Scientific Python ecosystem

**Installation:**
```bash
conda env create -f environment.yml
conda activate massconsistent_amr
```

### 2. `environment-dev.yml` - Development Environment
**Use this for:**
- Building the C++ solver from source
- Contributing to the codebase
- Running full regression test suite
- Building Python bindings

**Includes:**
- Everything from `environment.yml`
- C/C++ compilers
- CMake and build tools
- Testing frameworks (pytest)
- Debugging tools (gdb, lldb)
- Documentation tools (Sphinx)

**Installation:**
```bash
conda env create -f environment-dev.yml
conda activate massconsistent_amr-dev
```

### 3. `environment-full.yml` - Complete Suite with Optional Packages
**Use this for:**
- Wind farm analysis with FLORIS or PyWake
- Geochemical coupling with PHREEQC
- Advanced visualization and analysis
- Complete development workflow

**Includes:**
- Everything from both `environment.yml` and `environment-dev.yml`
- FLORIS (wind farm simulator)
- PyWake (wake modeling)
- Additional data tools (Pandas, Xarray, Dask)
- Advanced visualization (Seaborn)

**Installation:**
```bash
conda env create -f environment-full.yml
conda activate massconsistent_amr-full
```

## Installation Methods

### Using Conda (Recommended)

**Option 1: Using conda-forge channel**
```bash
# Install base packages
conda install -c conda-forge numpy scipy matplotlib netcdf4

# Activate environment with environment.yml
conda env create -f environment.yml
conda activate massconsistent_amr
```

**Option 2: Using Anaconda default channels**
```bash
# Install from defaults (compatible but may have older versions)
conda env create -f environment.yml -c defaults
conda activate massconsistent_amr
```

### Using Mamba (Faster Alternative to Conda)

Mamba is a faster conda implementation:

```bash
# Install mamba if not already available
conda install -c conda-forge mamba

# Create environment using mamba (faster)
mamba env create -f environment.yml
conda activate massconsistent_amr
```

### Using Miniforge or Miniconda

**Miniforge** (conda-forge focused):
```bash
# Miniforge comes with conda-forge as default channel
# Simply install from environment.yml
conda env create -f environment.yml
conda activate massconsistent_amr
```

**Miniconda** (lightweight Anaconda):
```bash
# Miniconda requires specifying conda-forge channel
conda env create -f environment.yml -c conda-forge
conda activate massconsistent_amr
```

## Python-Only Installation

If you prefer not to use conda and want to install via pip:

### Create Virtual Environment

```bash
# Create Python virtual environment
python3 -m venv massconsistent_amr_env
source massconsistent_amr_env/bin/activate  # On Windows: massconsistent_amr_env\Scripts\activate

# Install required packages
pip install numpy scipy matplotlib netcdf4 requests pyyaml
```

### Install Optional Packages (pip)

```bash
# Wind farm simulation tools
pip install floris py-wake

# Data analysis tools
pip install pandas xarray dask seaborn scikit-learn

# Visualization
pip install plotly bokeh
```

**Note:** The C++ solver requires compilation. If you want Python bindings, use conda with `environment-dev.yml` and compile as shown in the [Build Options](#build-options) section.

## Full Build Setup

Once your conda environment is activated, build the complete solver:

```bash
# Activate development environment
conda activate massconsistent_amr-dev

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON

# Build (use -j for parallel builds)
cmake --build . --parallel 4

# Run regression tests
ctest -j 4
```

### Build Options

```bash
# CPU-only build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMASSCONSISTENT_GPU_BACKEND=NONE

# CUDA GPU acceleration
cmake .. -DCMAKE_BUILD_TYPE=Release -DMASSCONSISTENT_GPU_BACKEND=CUDA

# HIP/ROCm GPU acceleration
cmake .. -DCMAKE_BUILD_TYPE=Release -DMASSCONSISTENT_GPU_BACKEND=HIP

# SYCL/oneAPI GPU acceleration
cmake .. -DCMAKE_BUILD_TYPE=Release -DMASSCONSISTENT_GPU_BACKEND=SYCL

# With MPI support
cmake .. -DMASSCONSISTENT_ENABLE_MPI=ON

# Full configuration (all features)
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DMASSCONSISTENT_GPU_BACKEND=CUDA \
  -DMASSCONSISTENT_ENABLE_MPI=ON \
  -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
```

## Verifying Your Installation

### Test Python Environment

```bash
# Activate environment
conda activate massconsistent_amr

# Check Python and key packages
python --version
python -c "import numpy, scipy, matplotlib, netCDF4; print('All core packages OK')"

# Test optional packages (if installed)
python -c "import floris; print('FLORIS available')" 2>/dev/null || echo "FLORIS not installed"
python -c "import py_wake; print('PyWake available')" 2>/dev/null || echo "PyWake not installed"
```

### Test C++ Build (if compiled)

```bash
# Run a simple regression test
cd build
ctest -R gaussian_hill --output-on-failure

# Or run the wind solver directly
./wind_solver ../regtest/gaussian_hill/inputs.i
```

### Verify Python Bindings

```bash
# If compiled with Python bindings
python -c "import pyWindSolver; print('Python bindings OK')"
```

## Troubleshooting

### Issue: "ModuleNotFoundError: No module named 'numpy'"

**Solution:** Make sure your conda environment is activated:
```bash
conda activate massconsistent_amr
python -c "import numpy"
```

### Issue: "Could not find CMake executable"

**Solution:** Install CMake via conda:
```bash
conda activate massconsistent_amr-dev
conda install cmake
```

### Issue: "gcc: command not found" (on macOS)

**Solution:** Install Xcode Command Line Tools:
```bash
xcode-select --install
```

Or use conda compilers:
```bash
conda install -c conda-forge cxx-compiler
```

### Issue: "CUDA not found" for GPU builds

**Solution:** Ensure CUDA toolkit is installed and environment is set correctly:
```bash
# Check CUDA installation
nvcc --version

# If using conda CUDA:
conda install -c conda-forge cuda-toolkit
```

### Issue: Build fails with "netCDF4 not found"

**Solution:** Reinstall netCDF4:
```bash
conda install -c conda-forge netcdf4
```

### Issue: Permission denied when creating environments

**Solution:** Check conda configuration and disk space:
```bash
# Check conda info
conda info

# Clear conda cache if needed
conda clean --all

# Try creating in a different location
conda create -p /custom/path/massconsistent_amr python=3.10
```

## Optional Packages

### FLORIS - Wind Farm Simulation

For wind farm layout optimization and power output simulation:

```bash
# Install via pip (if environment-full.yml not used)
pip install floris>=3.0

# Example usage
python src/python/example_floris_export.py
```

Documentation: https://floris.nrel.gov/

### PyWake - Wake Modeling

For detailed wake deficit analysis:

```bash
# Install via pip
pip install py-wake>=2.1

# Example usage
from py_wake.deficit_models.noj import NOJ
from massconsistent_amr import MassConsistentSite
```

Documentation: https://pywake.readthedocs.io/

### PHREEQC - Geochemical Modeling

For reactive transport modeling and geochemical analysis:

```bash
# Install PHREEQC (typically standalone executable)
# Follow https://www.usgs.gov/software/phreeqc-version-3

# For Python coupling, the code provides fallback implementations
# See src/python/phreeqc_coupling/ for details
```

Documentation: https://www.usgs.gov/software/phreeqc-version-3

## Platform-Specific Notes

### macOS

- If using Apple Silicon (M1/M2), ensure conda-forge packages are compatible:
  ```bash
  conda install -c conda-forge numpy scipy --force-reinstall
  ```

- For building with native compilers:
  ```bash
  conda install -c conda-forge llvm-openmp
  ```

### Windows

- Use `conda activate massconsistent_amr` (not `source activate`)
- For Python bindings, use Visual Studio Build Tools or MSVC compiler
- CMake should auto-detect MSVC if installed

### Linux

- Most tools work out-of-the-box
- For GPU support, ensure NVIDIA drivers are installed:
  ```bash
  nvidia-smi
  ```

## Additional Resources

- [Official Documentation](https://hgopalan.github.io/massconsistent_amr/)
- [GitHub Repository](https://github.com/hgopalan/massconsistent_amr)
- [CMake Documentation](https://cmake.org/documentation/)
- [Conda Documentation](https://docs.conda.io/)

## Getting Help

If you encounter issues:

1. Check [GitHub Issues](https://github.com/hgopalan/massconsistent_amr/issues)
2. Review this guide's [Troubleshooting](#troubleshooting) section
3. Run diagnostics:
   ```bash
   conda info
   cmake --version
   python --version
   ```
4. Create a new GitHub issue with your environment details
