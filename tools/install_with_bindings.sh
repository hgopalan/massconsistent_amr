#!/bin/bash
#
# Unix/Linux/macOS installation script for massconsistent_amr with Python bindings
# This script automatically detects Python and configures the build environment
#
# Usage: ./tools/install_with_bindings.sh [options]
# For help: ./tools/install_with_bindings.sh --help
#
# Author: massconsistent_amr development team
# Date: June 2026

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

# Change to repository root
cd "$REPO_ROOT"

# Find Python executable
find_python() {
    # Try various python executables
    for python_cmd in python3 python python3.11 python3.10 python3.9; do
        if command -v "$python_cmd" &> /dev/null; then
            # Verify Python version is 3.8+
            version_output=$("$python_cmd" --version 2>&1)
            version_string=$(echo "$version_output" | awk '{print $2}')
            major=$(echo "$version_string" | cut -d. -f1)
            minor=$(echo "$version_string" | cut -d. -f2)

            if [ "$major" -ge 3 ] && [ "$minor" -ge 8 ]; then
                echo "$python_cmd"
                return 0
            fi
        fi
    done

    return 1
}

# Print colored output
print_header() {
    echo ""
    echo "============================================================"
    echo "  $1"
    echo "============================================================"
    echo ""
}

print_info() {
    echo "[INFO] $1"
}

print_success() {
    echo "[SUCCESS] $1"
}

print_warning() {
    echo "[WARNING] $1"
}

print_error() {
    echo "[ERROR] $1" >&2
}

# Parse command line arguments
PYTHON_EXECUTABLE=""
BUILD_DIR="build"
GPU_BACKEND="NONE"
ENABLE_MPI=false
SKIP_TESTS=false
JOBS=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --python)
            PYTHON_EXECUTABLE="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --gpu-backend)
            GPU_BACKEND="$2"
            shift 2
            ;;
        --enable-mpi)
            ENABLE_MPI=true
            shift
            ;;
        --skip-tests)
            SKIP_TESTS=true
            shift
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --help)
            cat << EOF
Usage: $0 [OPTIONS]

Options:
    --python PATH           Path to Python executable (auto-detected if not specified)
    --build-dir DIR         Build directory path (default: build)
    --gpu-backend BACKEND   GPU backend to use: NONE, CUDA, HIP, SYCL (default: NONE)
    --enable-mpi            Enable MPI support
    --skip-tests            Skip running regression tests
    --jobs N                Number of parallel build jobs (default: auto)
    --help                  Show this help message

Examples:
    # Standard installation (CPU only)
    ./tools/install_with_bindings.sh

    # With CUDA support
    ./tools/install_with_bindings.sh --gpu-backend CUDA

    # With MPI and CUDA
    ./tools/install_with_bindings.sh --gpu-backend CUDA --enable-mpi

    # Specify Python explicitly
    ./tools/install_with_bindings.sh --python /usr/bin/python3.10

    # Skip tests
    ./tools/install_with_bindings.sh --skip-tests

EOF
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Step 1: Print welcome
print_header "massconsistent_amr Installation with Python Bindings"
print_info "Platform: $(uname -s)"
print_info "Architecture: $(uname -m)"

# Step 2: Find Python
if [ -z "$PYTHON_EXECUTABLE" ]; then
    print_header "Detecting Python Installation"
    PYTHON_EXECUTABLE=$(find_python)
    if [ -z "$PYTHON_EXECUTABLE" ]; then
        print_error "Could not find Python. Please install Python 3.8+ or specify with --python"
        exit 1
    fi
else
    if [ ! -x "$PYTHON_EXECUTABLE" ]; then
        print_error "Specified Python path is not executable: $PYTHON_EXECUTABLE"
        exit 1
    fi
fi

PYTHON_VERSION=$("$PYTHON_EXECUTABLE" --version 2>&1 | awk '{print $2}')
print_success "Found Python: $PYTHON_EXECUTABLE ($PYTHON_VERSION)"

# Step 3: Check CMake
print_header "Checking CMake Installation"
if ! command -v cmake &> /dev/null; then
    print_error "CMake not found. Please install CMake 3.20+"
    exit 1
fi
CMAKE_VERSION=$(cmake --version | head -n1)
print_success "CMake found: $CMAKE_VERSION"

# Step 4: Check Git and submodules
print_header "Checking Git and Submodules"
if command -v git &> /dev/null; then
    if [ ! -f "external/amrex/CMakeLists.txt" ]; then
        print_warning "AMReX submodule not initialized. Initializing..."
        git submodule update --init --recursive
    else
        print_success "AMReX submodule is initialized"
    fi
else
    print_warning "Git not found, assuming submodules are already initialized"
fi

# Step 5: Check Python packages
print_header "Checking Python Packages"

# Check NumPy
if "$PYTHON_EXECUTABLE" -c "import numpy" 2>/dev/null; then
    print_success "NumPy: Available"
else
    print_error "NumPy: NOT FOUND (required for Python bindings)"
    exit 1
fi

# Check pybind11 (optional, can be fetched during build)
if "$PYTHON_EXECUTABLE" -c "import pybind11" 2>/dev/null; then
    print_success "pybind11: Available"
else
    print_warning "pybind11: NOT FOUND (will be fetched during build)"
fi

# Step 6: Setup Python environment
print_header "Setting Up Python Environment"

PYTHON_INCLUDE=$("$PYTHON_EXECUTABLE" -c "import sysconfig; print(sysconfig.get_path('include'))" 2>/dev/null || echo "")
PYTHON_LIB=$("$PYTHON_EXECUTABLE" -c "import sysconfig; print(sysconfig.get_path('purelib'))" 2>/dev/null || echo "")

print_info "Python executable: $PYTHON_EXECUTABLE"
[ -n "$PYTHON_INCLUDE" ] && print_info "Python include path: $PYTHON_INCLUDE"
[ -n "$PYTHON_LIB" ] && print_info "Python lib path: $PYTHON_LIB"

# Step 7: Create build directory
print_header "Preparing Build Directory"
if [ -d "$BUILD_DIR" ]; then
    print_warning "Build directory '$BUILD_DIR' already exists"
else
    mkdir -p "$BUILD_DIR"
    print_success "Created build directory: $BUILD_DIR"
fi

# Step 8: Configure with CMake
print_header "Configuring with CMake (GPU Backend: $GPU_BACKEND)"

CMAKE_ARGS=(
    "-DCMAKE_BUILD_TYPE=Release"
    "-DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON"
    "-DMASSCONSISTENT_GPU_BACKEND=$GPU_BACKEND"
    "-DPython3_EXECUTABLE=$PYTHON_EXECUTABLE"
)

if [ -n "$PYTHON_INCLUDE" ]; then
    CMAKE_ARGS+=("-DPython3_INCLUDE_DIR=$PYTHON_INCLUDE")
fi

if [ -n "$PYTHON_LIB" ]; then
    CMAKE_ARGS+=("-DPython3_LIBRARY=$PYTHON_LIB")
fi

if [ "$ENABLE_MPI" = true ]; then
    CMAKE_ARGS+=("-DMASSCONSISTENT_ENABLE_MPI=ON")
    print_info "MPI support enabled"
fi

cd "$BUILD_DIR"

print_info "CMake configuration command:"
echo "  cmake .. ${CMAKE_ARGS[*]}"

if cmake .. "${CMAKE_ARGS[@]}"; then
    print_success "CMake configuration completed successfully"
else
    print_error "CMake configuration failed"
    exit 1
fi

cd "$REPO_ROOT"

# Step 9: Build project
print_header "Building massconsistent_amr with Python Bindings"

if [ -z "$JOBS" ]; then
    # Use number of available CPU cores
    if command -v nproc &> /dev/null; then
        JOBS=$(nproc)
    elif [ "$(uname)" = "Darwin" ]; then
        JOBS=$(sysctl -n hw.ncpu)
    else
        JOBS=4
    fi
fi

print_info "Using $JOBS parallel jobs"

if cmake --build "$BUILD_DIR" --config Release --parallel "$JOBS"; then
    print_success "Build completed successfully"
else
    print_error "Build failed"
    exit 1
fi

# Step 10: Run tests (optional)
if [ "$SKIP_TESTS" = false ]; then
    print_header "Running Regression Tests"
    if command -v ctest &> /dev/null; then
        cd "$BUILD_DIR"
        if ctest -j 4 --output-on-failure; then
            print_success "All tests passed"
        else
            print_warning "Some tests failed"
        fi
        cd "$REPO_ROOT"
    else
        print_warning "ctest not found, skipping tests"
    fi
else
    print_info "Skipping test execution"
fi

# Step 11: Setup PYTHONPATH
print_header "Python Bindings Configuration"

PYTHON_MODULE_PATH=$(cd "$BUILD_DIR/python" 2>/dev/null && pwd || echo "$REPO_ROOT/$BUILD_DIR/python")

print_success "Python bindings have been built successfully!"
print_info "Module location: $PYTHON_MODULE_PATH"

print_header "Installation Summary"
print_success "Installation completed successfully!"
print_info "Build directory: $BUILD_DIR"
print_info "Python bindings: $PYTHON_MODULE_PATH"
print_info ""
print_info "Next steps:"
print_info "1. Add to PYTHONPATH:"
echo "   export PYTHONPATH=$PYTHON_MODULE_PATH:\$PYTHONPATH"
print_info "2. Verify installation:"
echo "   python -c 'import sys; sys.path.insert(0, \"$PYTHON_MODULE_PATH\"); import pyWindSolver'"
print_info "3. See INSTALL.md for usage examples"

# Save setup instructions to a file
cat > "${BUILD_DIR}/setup_pythonpath.sh" << EOF
#!/bin/bash
# Source this file to setup PYTHONPATH for massconsistent_amr Python bindings
# Usage: source $BUILD_DIR/setup_pythonpath.sh

export PYTHONPATH="$PYTHON_MODULE_PATH:\$PYTHONPATH"
echo "PYTHONPATH updated: \$PYTHONPATH"

# Verify bindings
python -c "import sys; sys.path.insert(0, '$PYTHON_MODULE_PATH'); import pyWindSolver; print('[SUCCESS] pyWindSolver module loaded')" || echo "[WARNING] Could not import pyWindSolver module"
EOF
chmod +x "${BUILD_DIR}/setup_pythonpath.sh"
print_success "Created setup script: ${BUILD_DIR}/setup_pythonpath.sh"

exit 0
