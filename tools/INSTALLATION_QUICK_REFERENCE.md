# Installation Scripts Quick Reference

This quick reference shows the most common installation scenarios for massconsistent_amr with Python bindings.

## Simple Installation (No GPU)

### Linux/macOS
```bash
./tools/install_with_bindings.sh
source build/setup_pythonpath.sh
```

### Windows (CMD)
```cmd
tools\install_with_bindings.bat
call build\setup_pythonpath.bat
```

### Windows (PowerShell)
```powershell
& .\tools\install_with_bindings.bat
. .\build\setup_pythonpath.ps1
```

### Any Platform (Python)
```bash
python tools/install_with_bindings.py
# Add to PYTHONPATH
export PYTHONPATH=$PWD/build/python:$PYTHONPATH  # Linux/macOS
set PYTHONPATH=%cd%\build\python;%PYTHONPATH%    # Windows CMD
```

## Installation with CUDA Support

```bash
# Linux/macOS
./tools/install_with_bindings.sh --gpu-backend CUDA
source build/setup_pythonpath.sh

# Windows
tools\install_with_bindings.bat
REM (Note: GPU backend configured with CMake)

# Python (any platform)
python tools/install_with_bindings.py --gpu-backend CUDA
```

## Installation with MPI Support

```bash
# Linux/macOS
./tools/install_with_bindings.sh --enable-mpi
source build/setup_pythonpath.sh

# Python (any platform)
python tools/install_with_bindings.py --enable-mpi
```

## Installation with CUDA and MPI

```bash
./tools/install_with_bindings.sh --gpu-backend CUDA --enable-mpi
```

## Installation with Specific Python Version

```bash
# If you have multiple Python versions installed
./tools/install_with_bindings.sh --python /usr/bin/python3.10

# Or with Python script
python tools/install_with_bindings.py --python /usr/bin/python3.10
```

## Fast Build (Skip Tests)

```bash
./tools/install_with_bindings.sh --skip-tests

# Or with Python
python tools/install_with_bindings.py --skip-tests
```

## Control Build Parallelization

```bash
# Use 8 parallel jobs
./tools/install_with_bindings.sh --jobs 8

# Use only 1 job (useful for debugging)
./tools/install_with_bindings.sh --jobs 1
```

## Verify Installation

After installation and setting up PYTHONPATH:

```bash
# Verify Python bindings load
python -c "import sys; sys.path.insert(0, 'build/python'); import pyWindSolver; print('✓ pyWindSolver loaded')"

# List available functions
python -c "import sys; sys.path.insert(0, 'build/python'); import pyWindSolver; print(dir(pyWindSolver))"
```

## Troubleshooting Commands

```bash
# Check Python version
python --version

# Check if NumPy is available
python -c "import numpy; print('✓ NumPy available')"

# Check CMake version
cmake --version

# View installation configuration
cat install_config.json

# Check build directory contents
ls -la build/python/

# View build logs
cat build/CMakeFiles/CMakeOutput.log
```

## Important Notes

1. **Python Path**: The build scripts save the PYTHONPATH configuration. Always source/call the setup scripts before using pyWindSolver.

2. **Build Directory**: Each call to the installation script can reconfigure the same build directory. If you want a fresh build, remove the `build/` directory first:
   ```bash
   rm -rf build/
   ./tools/install_with_bindings.sh
   ```

3. **GPU Support**: GPU backend selection (CUDA, HIP, SYCL) requires the corresponding toolkit to be installed.

4. **MPI Support**: MPI support requires an MPI implementation (OpenMPI, MPICH, etc.) to be installed.

5. **Multiple Architectures**: If building for different architectures (CPU vs CUDA), use different build directories:
   ```bash
   ./tools/install_with_bindings.sh --build-dir build_cpu
   ./tools/install_with_bindings.sh --gpu-backend CUDA --build-dir build_cuda
   ```

## Advanced: Manual CMake Configuration

If you need more control, use CMake directly after the initial setup:

```bash
# Run automated setup once
./tools/install_with_bindings.sh --skip-tests

# Then manually configure or reconfigure
cd build
cmake .. -DCUSTOM_OPTION=VALUE
cmake --build . --config Release --parallel
cd ..
```

## See Also

- `tools/INSTALL_SCRIPTS_README.md` - Comprehensive documentation
- `INSTALL.md` - Main installation guide
- `GETTING_STARTED_TUTORIAL.md` - Getting started tutorial
- `install_config.json` - Configuration details saved after installation
