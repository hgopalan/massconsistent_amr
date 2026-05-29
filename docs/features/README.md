# Advanced Features Documentation

This directory contains detailed documentation for advanced features of the mass-consistent wind solver.

## Features

### Canopy Model Implementation
- **[Canopy Model](canopy_model.md)** - Complete documentation of the vegetation canopy parameterization
- **[Canopy Implementation Summary](canopy_implementation_summary.txt)** - Detailed implementation summary with validation results

The canopy model implementation adds support for vegetation effects using QUIC-URB methodology, including:
- MacDonald et al. (2000) displacement height model
- Shaw & Pereira (1982) exponential decay model
- GPU-portable implementation

### Python API
- **[Python API](python_api.md)** - Complete Python bindings documentation for coupled wind-fire simulations

The Python API enables:
- Full solver control from Python
- Coupled wind-fire simulations
- Integration with external fire solvers like wildfire_levelset
- Data exchange via numpy arrays

### Implementation Details
- **[Implementation Summary](implementation_summary.txt)** - General implementation notes and summary

## Usage

Refer to the individual feature documentation files for detailed usage instructions, API references, and examples.
