# Gemini Agent Instructions

You are assisting with the **massconsistent_amr** project - a high-performance, GPU-accelerated mass-consistent wind diagnostic solver built on the AMReX framework.

## Project Overview

**massconsistent_amr** is an advanced computational framework for:
- 3D mass-consistent wind field diagnostics on complex terrain
- GPU-accelerated parallel computing (CUDA/HIP/SYCL)
- Integrated building and canopy drag models
- Wind turbine wake characterization and prediction
- Atmospheric dispersion with Lagrangian particle methods
- Reactive chemistry transport simulation
- External integration (FLORIS, PyWake, PHREEQC)

## Key Technical Features

1. **Solver**: Mass-consistent wind diagnostic with terrain-following coordinates
2. **Turbulence**: Spatially-varying anisotropy parameterization
3. **Aerodynamics**: Building wakes, canopy effects, turbine wake modeling
4. **Dispersion**: Lagrangian Puff, LPDM with reactive chemistry
5. **Parallelism**: MPI distributed + GPU acceleration
6. **Framework**: Built on AMReX for structured adaptive mesh refinement

## Repository Layout

```
massconsistent_amr/
├── src/                 # Primary C++ implementation
│   ├── cpp/            # Core solver code
│   └── python/         # Python bindings and utilities
├── tests_and_examples/  # Test suite and demonstrations
├── docs/                # Technical documentation
├── tools/               # Auxiliary utilities
├── external/            # AMReX submodule and dependencies
├── regtest/             # Regression test suite
├── CMakeLists.txt       # Build configuration
└── environment.yml      # Conda dependency specification
```

## Build & Development

### Prerequisites
- C++17 compiler or later
- CUDA/HIP/SYCL toolkit (for GPU support)
- MPI library (for parallel execution)
- Python 3.8+ (for Python integration)

### Build System
- CMake-based build system
- Environment files: `environment.yml`, `environment-dev.yml`, `environment-full.yml`
- Automated CI/CD via GitHub Actions (`.github/workflows/`)

### Testing Infrastructure
- Regression tests in `regtest/`
- Example scenarios in `tests_and_examples/`
- CMake-integrated test execution

## Development Best Practices

1. **Code Organization**: Follow AMReX patterns and existing code conventions
2. **GPU/Parallel Code**: Maintain efficiency for both CPU and GPU backends
3. **Testing**: Add tests for new features; verify existing tests pass
4. **Version Control**: Reference PR/issue numbers in commit messages
5. **Documentation**: Update markdown files for architectural or user-facing changes

## Critical Subsystems

### Wind Solver
- Mass-consistent diagnostic algorithm
- Terrain-following coordinate transformation
- Boundary condition handling

### Turbine/Building Wakes
- Bastankhah wake model for turbines
- Building drag parameterizations
- Urban canopy effects

### Dispersion Module
- Lagrangian particle tracking
- Puff model implementation
- Reactive chemistry integration (PHREEQC coupling)

## When Assisting

1. Understand the specific problem domain (wind, dispersion, structures)
2. Reference existing implementations for patterns
3. Consider performance implications of proposed changes
4. Ensure compatibility across CPU/GPU backends
5. Validate against regression test suite

## Documentation References

- `README.md` - Project overview and scenario descriptions
- `GETTING_STARTED_TUTORIAL.md` - Setup and quick-start guide
- `INSTALL.md` - Installation procedures
- `docs/` - Technical deep-dives on specific features
