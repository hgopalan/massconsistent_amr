# Claude Agent Instructions

You are assisting with the **massconsistent_amr** project - a high-performance, GPU-accelerated mass-consistent wind diagnostic solver built on the AMReX framework.

## Project Overview

**massconsistent_amr** is a specialized computational tool for:
- High-performance 3D mass-consistent wind diagnostics
- Complex terrain-following coordinate transformations
- Urban building/canopy drag modeling
- Wind turbine wake analytics
- Atmospheric dispersion modeling (Lagrangian Puff and LPDM)
- Reactive chemistry simulations
- Integration with external tools (FLORIS, PyWake, PHREEQC)

## Core Capabilities

1. **Wind Diagnostics**: Mass-consistent solving on complex terrain with spatially-varying anisotropy
2. **Turbine Modeling**: Analytical wake deflection and deficit calculations
3. **Urban Modeling**: Building drag and street canyon effects
4. **Dispersion**: Advanced atmospheric transport with reactive chemistry
5. **Parallel Computing**: GPU acceleration (CUDA/HIP/SYCL) and MPI parallelism

## Repository Structure

```
.
├── src/                 # C++ source code (main solver)
├── tests_and_examples/  # Test cases and examples
├── docs/                # Documentation
├── tools/               # Utility tools
├── external/            # External dependencies (AMReX submodule)
└── regtest/             # Regression tests
```

## Development Guidelines

### Code Style
- Follow existing C++ conventions in the codebase
- Use AMReX API patterns and conventions
- Maintain consistency with GPU/MPI code patterns

### Testing
- Add tests in `tests_and_examples/` for new features
- Run existing tests to verify no regressions
- Use CMake for building and testing

### Documentation
- Update markdown files in `docs/` for architectural changes
- Add README files for new features or major subsystems
- Include examples in `tests_and_examples/` when introducing new features

## Common Tasks

- **Building**: Use CMake with appropriate compiler flags for GPU support
- **Testing**: Run regression tests in `regtest/` to verify functionality
- **Python Integration**: Check `src/python/` for Python-specific implementations
- **Dependencies**: Review `environment.yml` and `requirements.txt` for dependencies

## Key Files to Know

- `CMakeLists.txt` - Build configuration
- `pyproject.toml` - Python package configuration
- `GETTING_STARTED_TUTORIAL.md` - Project setup and tutorial
- `environment.yml` - Conda environment specification
- `.github/workflows/` - CI/CD pipeline definitions

## When Helping

1. **Understand the context**: Ask clarifying questions about the specific feature or issue
2. **Reference the codebase**: Look at existing patterns and implementations
3. **Consider performance**: This is a high-performance code; be mindful of optimization and parallelism
4. **Test thoroughly**: Always verify changes don't break existing functionality
5. **Document changes**: Update relevant markdown files and comments

## Default Development Guidelines

When making code changes, adhere to these requirements:

1. **Documentation Organization**
   - Do not create stray .MD files
   - Always integrate documentation into proper existing sections (`docs/`, inline comments, existing README files)
   - Do not create top-level documentation unless explicitly specified

2. **Regression Tests**
   - Check if regtests exist for the feature/area being modified
   - If regtests don't exist, create them
   - All regtests must pass before submitting PR

3. **Code Comments & Documentation**
   - Include detailed comments explaining changes
   - Add citations where applicable (papers, references, issues)
   - Include date of code addition in comments
   - Reference related issues/PRs in commit messages and comments

4. **Professional Documentation**
   - Keep all documentation and comments professional
   - Avoid informal agent conversations (e.g., "Feature 1", "Case 1", "Phase 1")
   - Use clear, technical language appropriate for code maintenance

5. **Build & Testing Requirements**
   - Code must compile without warnings/errors
   - All regtests must pass on Ubuntu and macOS
   - Verify no regression in existing functionality
   - Run validation before creating PR

6. **No Stray Content**
   - Don't include temporary exploration notes in final code
   - Keep commit messages and documentation focused on the actual change
   - Remove debug code before committing

## Additional Resources

- See `README.md` for scenario gallery and project highlights
- See `GETTING_STARTED_TUTORIAL.md` for setup instructions
- See `INSTALL.md` for detailed installation procedures
