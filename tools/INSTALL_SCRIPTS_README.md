# massconsistent_amr Installation Scripts

This directory contains cross-platform installation scripts for building **massconsistent_amr** with Python bindings. These scripts automatically detect Python, configure CMake, and handle platform-specific setup.

## Available Scripts

### 1. Python Installation Script (Universal)

**File:** `install_with_bindings.py`

**Platform:** Windows, Linux, macOS  
**Language:** Python 3.8+

The main installation script written in Python. Works on all platforms and provides the most control and detailed output.

**Usage:**
```bash
python tools/install_with_bindings.py [options]
```

**Advantages:**
- Works identically on all platforms
- Detailed colored output and progress reporting
- Configuration saved to `install_config.json`
- Most flexible with command-line options

### 2. Unix/Linux/macOS Installation Script

**File:** `install_with_bindings.sh`

**Platform:** Linux, macOS  
**Language:** Bash/Shell

Native bash script for Unix-like systems. Automatically generates setup scripts for easy PYTHONPATH configuration.

**Usage:**
```bash
./tools/install_with_bindings.sh [options]
```

**Advantages:**
- No Python required to run
- Fast and lightweight
- Auto-generates `setup_pythonpath.sh` for easy environment setup

### 3. Windows Installation Script

**File:** `install_with_bindings.bat`

**Platform:** Windows  
**Language:** Batch/CMD

Native batch script for Windows command prompt. Generates both `.bat` and `.ps1` setup scripts.

**Usage:**
```cmd
tools\install_with_bindings.bat [options]
```

**Advantages:**
- No Python required to run
- Generates setup scripts for both CMD and PowerShell
- Native Windows console integration

## Quick Start

### Linux/macOS

```bash
cd massconsistent_amr

# Standard installation
./tools/install_with_bindings.sh

# With CUDA GPU support
./tools/install_with_bindings.sh --gpu-backend CUDA

# Source the setup script
source build/setup_pythonpath.sh
```

### Windows

```cmd
cd massconsistent_amr

# Standard installation
tools\install_with_bindings.bat

# Run the setup script
call build\setup_pythonpath.bat
```

## Common Use Cases

### CPU-Only Build

```bash
# All platforms
python tools/install_with_bindings.py

# Or native scripts
./tools/install_with_bindings.sh              # Linux/macOS
tools\install_with_bindings.bat              # Windows
```

### Build with CUDA

```bash
# All platforms
python tools/install_with_bindings.py --gpu-backend CUDA

# Or
./tools/install_with_bindings.sh --gpu-backend CUDA
```

### Build with MPI Support

```bash
# All platforms
python tools/install_with_bindings.py --enable-mpi

# Or
./tools/install_with_bindings.sh --enable-mpi
```

### Build with Multiple Features

```bash
# CUDA + MPI with custom Python
./tools/install_with_bindings.sh \
  --python /usr/bin/python3.10 \
  --gpu-backend CUDA \
  --enable-mpi \
  --jobs 8
```

### Faster Builds (Skip Tests)

```bash
python tools/install_with_bindings.py --skip-tests
```

## Installation Steps

Each script performs the following steps:

1. **Python Detection** - Finds Python 3.8+ executable
2. **CMake Verification** - Checks for CMake 3.20+
3. **Git Submodule Setup** - Initializes AMReX submodule if needed
4. **Package Verification** - Checks for NumPy and pybind11
5. **Environment Setup** - Configures Python paths
6. **Build Directory** - Creates/cleans build directory
7. **CMake Configuration** - Runs CMake with appropriate flags
8. **Compilation** - Builds the project with all dependencies
9. **Testing** - Runs regression tests (optional)
10. **Python Setup** - Generates PYTHONPATH setup scripts

## Command-Line Options

All scripts support these options:

| Option | Description | Example |
|--------|-------------|---------|
| `--python PATH` | Python executable path | `--python /usr/bin/python3.10` |
| `--build-dir DIR` | Build directory | `--build-dir mybuild` |
| `--gpu-backend BACKEND` | GPU backend (NONE/CUDA/HIP/SYCL) | `--gpu-backend CUDA` |
| `--enable-mpi` | Enable MPI support | `--enable-mpi` |
| `--skip-tests` | Skip regression tests | `--skip-tests` |
| `--jobs N` | Parallel build jobs | `--jobs 16` |
| `--help` | Show help message | `--help` |

## Environment Variables

The scripts automatically detect and set:

- `PYTHON_EXECUTABLE` - Path to Python interpreter
- `Python3_INCLUDE_DIR` - Python include directory
- `Python3_LIBRARY` - Python library path
- `PYTHONPATH` - Updated to include built modules

## Output Files

After installation, the following files are created/updated:

| File | Purpose |
|------|---------|
| `build/` | Build directory with compiled binaries |
| `build/python/` | Python module and bindings |
| `build/setup_pythonpath.sh` | Linux/macOS PYTHONPATH setup (bash) |
| `build/setup_pythonpath.bat` | Windows PYTHONPATH setup (CMD) |
| `build/setup_pythonpath.ps1` | Windows PYTHONPATH setup (PowerShell) |
| `install_config.json` | Installation configuration saved |

## Troubleshooting

### "Could not find Python"

**Solution:** Install Python 3.8+ or specify explicitly:
```bash
./tools/install_with_bindings.sh --python /usr/bin/python3.10
```

### "CMake not found"

**Solution:** Install CMake:
```bash
# macOS
brew install cmake

# Ubuntu/Debian
sudo apt-get install cmake

# Windows
choco install cmake
```

### "NumPy not found"

**Solution:** Install NumPy before running the script:
```bash
python -m pip install numpy
# Or if using conda
conda install numpy
```

### "Build failed with compiler error"

**Solution:**
1. Check that you have a C++17 compiler installed
2. On macOS, ensure Xcode Command Line Tools are installed:
   ```bash
   xcode-select --install
   ```
3. On Windows, ensure Visual Studio or Build Tools are installed
4. Try with verbose output:
   ```bash
   cmake --build build --verbose
   ```

### Python bindings import fails

**Solution:** Ensure PYTHONPATH is set:
```bash
# macOS/Linux
export PYTHONPATH=$PWD/build/python:$PYTHONPATH

# Windows CMD
set PYTHONPATH=%cd%\build\python;%PYTHONPATH%

# Windows PowerShell
$env:PYTHONPATH = "$PWD\build\python;" + $env:PYTHONPATH

# Then test
python -c "import sys; sys.path.insert(0, 'build/python'); import pyWindSolver"
```

## Advanced Usage

### Custom CMake Configuration

If you need additional CMake options, you can manually run CMake after using the script:

```bash
cd build
cmake .. -DCUSTOM_OPTION=VALUE
cmake --build . --config Release --parallel
```

### Using Different Build Directories

```bash
# Build in 'mybuild' instead of 'build'
./tools/install_with_bindings.sh --build-dir mybuild

# Setup PYTHONPATH for custom directory
export PYTHONPATH=$PWD/mybuild/python:$PYTHONPATH
```

### Parallel Build Control

```bash
# Use all available cores (default)
./tools/install_with_bindings.sh

# Use specific number of cores
./tools/install_with_bindings.sh --jobs 4

# Use 1 core (for debugging)
./tools/install_with_bindings.sh --jobs 1
```

### Incremental Builds

For faster rebuilds during development:

```bash
# Initial build with script
./tools/install_with_bindings.sh

# Later, incrementally rebuild without running tests
cd build
cmake --build . --config Release --parallel
cd ..
```

## Verification

After installation, verify the setup:

```bash
# Check Python bindings
python -c "
import sys
sys.path.insert(0, 'build/python')
import pyWindSolver
print('✓ pyWindSolver module loaded successfully')
print('✓ Installation verified')
"
```

## Python Packages Required

The installation scripts require:

- **Python 3.8+** - Programming language
- **NumPy** - Scientific computing (required for Python bindings)
- **pybind11** - Python/C++ bindings (will be downloaded if not found)
- **CMake 3.20+** - Build system
- **C++17 Compiler** - For compilation (GCC, Clang, MSVC)

Optional:
- **CUDA Toolkit** - For GPU acceleration with CUDA
- **HIP** - For GPU acceleration with AMD/ROCm
- **oneAPI** - For GPU acceleration with SYCL
- **MPI** - For distributed computing support

## Platform-Specific Notes

### macOS

Ensure Xcode Command Line Tools are installed:
```bash
xcode-select --install
```

For Apple Silicon (M1/M2), all tools must be ARM64 compatible:
```bash
# Install ARM64 compatible tools via conda
conda install -c conda-forge cmake numpy
```

### Windows

Visual Studio or Build Tools must be installed:
- Visual Studio Community (recommended)
- Visual Studio Build Tools for Visual Studio 2022

CMake will auto-detect MSVC if installed.

### Linux

On Ubuntu/Debian:
```bash
sudo apt-get install build-essential cmake git python3 python3-dev python3-numpy
```

On RHEL/CentOS:
```bash
sudo yum install gcc-c++ cmake git python3 python3-devel python3-numpy
```

## Contributing

If you improve these scripts, please:

1. Test on all three platforms (Windows, Linux, macOS)
2. Update documentation
3. Ensure backward compatibility
4. Submit a pull request with detailed changes

## Support

For issues with the installation scripts:

1. Check the [Troubleshooting](#troubleshooting) section
2. Review build output in detail
3. Check `install_config.json` for configuration details
4. Open an issue on GitHub with:
   - Your platform and OS version
   - Python version and path
   - CMake version
   - Full output from the installation script
   - `install_config.json` contents

## Related Documentation

- [INSTALL.md](../INSTALL.md) - Main installation guide
- [GETTING_STARTED_TUTORIAL.md](../GETTING_STARTED_TUTORIAL.md) - Getting started guide
- [CMakeLists.txt](../CMakeLists.txt) - CMake build configuration
- [src/python/CMakeLists.txt](../src/python/CMakeLists.txt) - Python bindings configuration
