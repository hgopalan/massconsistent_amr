# Fire Coupling Tests

This directory contains regression tests for coupling massconsistent_amr wind solver with wildfire_levelset fire solver.

## Coupling Modes

### One-way Coupling (`one_way/`)
Wind field is computed independently and provided to the fire solver. Fire spread responds to wind dynamics, but fire does NOT affect wind.

**Test:** `regtest/fire_coupling/one_way/test.py`
- Verifies wind solver initializes and solves correctly
- Verifies fire solver accepts 3D wind field from wind solver
- Runs coupled simulation for 5 timesteps
- Confirms wind field is unaffected by fire

**Input files:**
- `wind_inputs.i`: Wind solver configuration (32×32×16 grid, flat terrain)
- `fire_inputs.i`: Fire solver configuration (32×32 grid, matching domain)

### Two-way Coupling (`two_way/`)
Wind field is computed with fire heating effects. Fire heating is extracted and fed back to the wind solver for fire-induced wind changes.

**Test:** `regtest/fire_coupling/two_way/test.py`
- Verifies both solvers initialize with compatible domains
- Verifies wind solver accepts heat sources via `add_heat_source()` method
- Runs coupled simulation for 5 timesteps with two-way feedback
- Confirms heat source extraction and passing works

**Input files:**
- `wind_inputs.i`: Wind solver configuration (32×32×16 grid, flat terrain)
- `fire_inputs.i`: Fire solver configuration (32×32 grid, matching domain)

## Running Tests

### Prerequisites
Both solvers must be built with Python bindings enabled:
```bash
# massconsistent_amr
cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON

# wildfire_levelset
cmake -S . -B build -DLEVELSET_BUILD_PYTHON_BINDINGS=ON
```

### Run Individual Tests
```bash
# One-way coupling
cd regtest/fire_coupling/one_way/
python3 test.py

# Two-way coupling
cd regtest/fire_coupling/two_way/
python3 test.py
```

### Run via CTest
```bash
cd build
ctest -R "fire_coupling" -V
```

## Domain Compatibility

Both tests use matching domains:
- Horizontal extent: 0-1000 m in both x and y directions
- Grid resolution: 32×32 in horizontal direction
- Wind solver: 32×32×16 vertical levels
- Fire solver: 32×32 horizontal (2D)
- Terrain: Flat at 100 m elevation

This ensures that grid indices align properly during coupling:
- `i_x, i_y` correspond to the same physical location in both solvers

## Expected Behavior

### One-way Coupling
1. Wind solver solves independently for each timestep
2. 3D velocity field (u, v, w) extracted and passed to fire solver
3. Fire spreads according to wind field
4. Fire state (phi, ros, intensity) advances
5. Wind field remains unchanged across all steps

### Two-way Coupling
1. Wind solver solves with any stored heat source from previous fire step
2. 3D velocity field extracted and passed to fire solver
3. Fire advances and heat sources are extracted (if supported)
4. Heat source is passed back to wind solver via `add_heat_source()`
5. Next wind solve includes fire heating effects

## Dependencies

- `massconsistent_amr`: Wind solver with Python bindings
- `wildfire_levelset`: Fire solver with Python bindings
- Python 3.6+
- NumPy

## Troubleshooting

### ImportError for pyWindSolver or pyWildfire
- Ensure both solvers are built with Python bindings: `-DBUILD_PYTHON_BINDINGS=ON`
- Check that build directories' `python/` subdirectories are in PYTHONPATH

### Domain compatibility warnings
- Ensure wind and fire input files have matching domain extents
- Check that grid spacings (dx, dy) are consistent

### Heat source not being added in two-way mode
- Verify fire solver supports `get_surface_fluxes()` method
- Check that wind solver `add_heat_source()` method is properly implemented
- Review wind and fire solver compatibility

## References

- massconsistent_amr: https://github.com/hgopalan/massconsistent_amr
- wildfire_levelset: https://github.com/hgopalan/wildfire_levelset
