# Python Bindings for Mass-Consistent Wind Solver

This directory contains Python bindings for the mass-consistent AMR wind solver.

## Overview

The Python module `pyWindSolver` provides a Python interface to the C++ wind solver implementation, enabling:

- Wind field calculation on complex terrain
- Building and canopy drag effects
- Scalar transport and turbulence modeling
- Dispersion modeling
- Integration with other Python frameworks

## Building

The Python bindings are built as part of the CMake build process when `DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON` is enabled.

## Usage

After building, add the build directory to your Python path:

```bash
export PYTHONPATH=${PWD}/build/python:$PYTHONPATH
```

Then import the module:

```python
import pyWindSolver
```

## Features

The binding provides access to:

- Wind field solver with AMR capabilities
- Terrain-following coordinate systems
- Building and wake modeling
- Surface flux calculations
- Boundary condition management

For detailed usage examples, see the example scripts in this directory.
