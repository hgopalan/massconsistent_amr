# Automated Regression Tests (`regtest`)

This directory contains the automated, self-contained regression tests used for continuous integration (CI) and validation of the mass-consistent AMR wind solver.

## Directory Structure

Tests are organized into clean categories corresponding to specific areas of the solver's physics and functionality:

* **[buildings/](./buildings/)**: Verifies building wake models (Röckle, Huber-Snyder, AERMOD PRIME), standard shapes, polygon footprints, and void zones.
* **[datacenter/](./datacenter/)**: Tests data center heat source definitions, thermal plume rise, and buoyant convective flows.
* **[diagnostics/](./diagnostics/)**: Performs internal consistency checks, mass conservation metrics, and diagnostics validations.
* **[dispersion/](./dispersion/)**: Regresses particle/puff dispersion models, agricultural drone spray pathing, and PHREEQC reactive transport.
* **[physics/](./physics/)**: Tests boundary layer wall functions, cell-local anisotropy, ABL profile assimilation, and NWP coupling.
* **[terrain/](./terrain/)**: Validates grid initialization, masking, and solver convergence over real and synthetic topographies.
* **[turbulence/](./turbulence/)**: Regresses Mann Box anisotropic spectral tensor models, spectral realizations, and coherence checks.
* **[wakes/](./wakes/)**: Verifies analytical wake models (Jensen, Park), wake deflection under yaw, and turbine power curves.

## Execution

The regression tests are designed to be run automatically by CTest after compiling the solver:

```bash
# Build the solver
cmake -S . -B build -DMASSCONSISTENT_BUILD_TESTS=ON
cmake --build build -j$(nproc)

# Run regression tests
cd build && ctest --output-on-failure
```
