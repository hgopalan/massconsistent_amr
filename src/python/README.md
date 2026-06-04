# Python Bindings for massconsistent_amr

This directory contains Python bindings for the mass-consistent wind solver, providing a convenient high-level interface to control the solver from Python.

## Usage

```python
from wind_solver import WindSolver

# Create solver instance
wind = WindSolver()

# Initialize from inputs file
wind.initialize("inputs.i")

# Solve for wind field
result = wind.solve()

# Extract velocity field
vel = wind.get_velocity()

# Extract velocity at specific height
vel_30m = wind.get_velocity_at_agl(30.0)

# Cleanup
wind.finalize()
```

## Building

To build with Python bindings enabled:

```bash
cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
cmake --build build
```

## Installation

After building, the module is available in `build/python/`. To use it, add the path to `PYTHONPATH`:

```bash
export PYTHONPATH=${PWD}/build/python:$PYTHONPATH
```
