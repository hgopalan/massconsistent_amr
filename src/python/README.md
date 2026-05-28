# Python API for massconsistent_amr

This directory contains Python bindings for the mass-consistent wind solver.

## Files

- `pyWindSolver.cpp` - C++ pybind11 bindings (generated module)
- `wind_solver.py` - High-level Python wrapper class
- `__init__.py` - Package initialization
- `test_wind_solver_api.py` - API tests
- `coupled_wind_fire_example.py` - Example coupled wind-fire simulation
- `CMakeLists.txt` - Build configuration for Python bindings

## Build

```bash
cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
cmake --build build --parallel
export PYTHONPATH=$PWD/build/python:$PYTHONPATH
```

## Usage

### Low-level API

```python
import pyWindSolver

# Initialize
result = pyWindSolver.initialize("inputs.i")
print(f"Grid: {result['nx']} x {result['ny']} x {result['nz']}")

# Solve
pyWindSolver.solve()

# Extract velocity
vel = pyWindSolver.get_velocity()
u, v, w = vel['u'], vel['v'], vel['w']

# Cleanup
pyWindSolver.finalize()
```

### High-level API

```python
from wind_solver import WindSolver

# Context manager ensures finalization
with WindSolver("inputs.i") as wind:
    wind.solve()
    
    # Extract velocity at 10m AGL
    vel_agl = wind.get_velocity_at_agl(10.0)
    
    # Write output
    wind.write_plotfile("plt_wind")
    wind.write_extract("wind_10m.csv", agl_height=10.0)
```

## Testing

```bash
python3 src/python/test_wind_solver_api.py
```
