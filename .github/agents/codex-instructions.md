# Codex Agent Instructions

You are assisting with the **massconsistent_amr** project - a high-performance, GPU-accelerated mass-consistent wind diagnostic solver built on the AMReX framework.

## Project Summary

**massconsistent_amr** is a C++ computational solver for atmospheric modeling featuring:
- Mass-consistent 3D wind field diagnostics
- Terrain-following coordinate systems with complex geometry support
- Building/canopy aerodynamic drag modeling
- Wind turbine wake deflection and deficit modeling (Bastankhah model)
- Atmospheric dispersion (Lagrangian Puff, LPDM) with reactive chemistry
- GPU acceleration (CUDA/HIP/SYCL) and MPI parallelism
- AMReX framework for structured adaptive mesh refinement

## Code Structure

```
.
├── src/cpp/              # Core C++ solver implementation
├── src/python/           # Python wrappers and utilities
├── tests_and_examples/   # Test cases and usage examples
├── docs/                 # Documentation (markdown)
├── tools/                # Supporting utilities
├── external/             # Dependencies (AMReEx submodule)
├── regtest/              # Regression test suite
├── CMakeLists.txt        # Build configuration
└── environment.yml       # Conda environment spec
```

## Language & Frameworks

- **Primary**: C++17 (AMReX-based)
- **Parallel**: MPI, CUDA/HIP/SYCL
- **Secondary**: Python 3.8+
- **Build**: CMake

## Common Development Tasks

### Adding New Features
1. Implement in C++ in `src/cpp/`
2. Add Python bindings if user-facing in `src/python/`
3. Create test case in `tests_and_examples/`
4. Add regression test in `regtest/`
5. Document in `docs/` or update relevant README files

### Modifying Physics/Algorithms
- Wind solver: Affects mass-consistency and terrain handling
- Wake model: Impacts turbine and building aerodynamics
- Dispersion: Changes atmospheric transport behavior
- Chemistry: Modifies reactive transport solver

### Performance Optimization
- Profile GPU/MPI scaling
- Review AMReX optimization patterns
- Consider memory bandwidth and compute density
- Test on supported accelerators

## Build & Testing

```bash
# Build with GPU support
cmake -DENABLE_CUDA=ON ...

# Run tests
ctest

# Run regression tests
./regtest/...
```

## Key Implementation Patterns

- Use AMReX MultiFab for distributed arrays
- MPI calls via AMReEx utilities
- GPU kernel launching via AMReEx macros
- Python/C++ integration via pybind11 (if applicable)

## Common Files to Reference

| File | Purpose |
|------|---------|
| CMakeLists.txt | Build system configuration |
| GETTING_STARTED_TUTORIAL.md | Quick start guide |
| INSTALL.md | Installation details |
| src/python/README.md | Python integration guide |
| docs/*.md | Technical documentation |

## Quality Checklist

- [ ] Code compiles without warnings
- [ ] Existing regression tests pass
- [ ] New features have test cases
- [ ] Documentation updated if needed
- [ ] GPU/MPI compatibility verified
- [ ] Performance impact assessed

## Contact & References

- See README.md for project overview and capabilities
- See GETTING_STARTED_TUTORIAL.md for environment setup
- Check .github/workflows/ for CI/CD pipeline details
