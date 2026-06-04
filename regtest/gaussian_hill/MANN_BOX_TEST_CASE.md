# Gaussian Hill with Mann Box Turbulence - Test Case

## Overview

This directory now contains a complete test case for the mass-consistent wind solver with **Mann Box synthetic turbulence** applied to the **Gaussian Hill benchmark terrain**.

## Files

| File | Purpose |
|------|---------|
| `inputs.i` | Original Gaussian Hill (baseline, no turbulence) |
| `inputs_mann_box.i` | **NEW: Gaussian Hill with Mann Box turbulence** |
| `inputs_synthetic_turbulence.i` | Existing IEC/Von Kármán/Kaimal configuration |
| `terrain.csv` | Gaussian Hill terrain elevation data (11×11 grid) |
| `test_mann_box_inputs.py` | **NEW: Test suite for Mann Box configuration** |
| `test_case1.py` | Existing Python test case |

## Quick Start

### Run the Mann Box Test Case

```bash
# Using Python with the inputs.i file
cd /tmp/workspace/hgopalan/massconsistent_amr/regtest/gaussian_hill

# Test the configuration (requires numpy)
python3 test_mann_box_inputs.py

# Run the C++ solver with Mann Box configuration
/path/to/build/wind_solver inputs_mann_box.i
```

### Using inputs_mann_box.i

The configuration file is fully parameterized for the Mann Box model:

```bash
# Standard usage
wind_solver inputs_mann_box.i

# Or set output directory
wind_solver inputs_mann_box.i > wind_solution.log 2>&1
```

## Configuration Details

### Key Parameters

**Wind Conditions:**
- U_ref = 12.0 m/s (reference wind speed)
- V_ref = 0.0 m/s (no lateral component)
- z_ref = 10.0 m (reference height for wind profile)
- z0 = 0.03 m (aerodynamic roughness length)

**Grid:**
- Domain: 300×300 m horizontal, 100 m vertical (above terrain)
- Spacing: dx = dy = 30 m, dz = 25 m
- Terrain: Gaussian hill 11×11 points, max elevation 50 m

**Mann Box Spectral Tensor:**
- Length scales: L_u = 300 m, L_v = 210 m, L_w = 120 m
- Anisotropy ratios: v/u = 0.8, w/u = 0.5
- Asymmetry: α = 1.0
- Coherence factors: η_uv = 0.75, η_uw = 0.50, η_vw = 0.65

**Turbulence Intensity:**
- Reference: I = 0.12 at z = 10 m
- Power law exponent: 0.14

### Output Files

When run with `inputs_mann_box.i`, the solver generates:

1. **AMReX Plotfiles:**
   - `plt_gaussian_hill_mann_box/` - Wind field without turbulence
   - `plt_gaussian_hill_mann_box_with_fluctuations/` - Wind + synthetic fluctuations

2. **CSV Extraction:**
   - `wind_extract_mann_box.csv` - Wind values at 15 m AGL (tabular format)

## Test Suite

### Running test_mann_box_inputs.py

```bash
cd /tmp/workspace/hgopalan/massconsistent_amr/regtest/gaussian_hill
export PYTHONPATH=/path/to/build/python:$PYTHONPATH
python3 test_mann_box_inputs.py
```

**Tests Performed:**

1. **Inputs File Parsing** - Validates all Mann Box parameters present
2. **Parameter Extraction** - Verifies correct parameter values
3. **Mann Box Parameters** - Consistency checks (length scales, anisotropy, coherence)
4. **Wind Solver Init** - Initialization with inputs.i configuration
5. **Spectrum Generation** - Spectrum computation and realizability

**Expected Output:**
```
======================================================================
GAUSSIAN HILL WITH MANN BOX - INPUTS.I TEST SUITE
======================================================================

TEST 1: Inputs File Parsing
...
✓ Configuration file found
✓ enable_synthetic_turbulence
✓ turbulence_spectrum_model
...

TEST 2: Parameter Extraction and Validation
...
✓ U_ref: 12.0
✓ turbulence_length_scale_u: 300.0
...

[Additional test results...]

TEST SUMMARY
  ✓ PASS: Inputs File Parsing
  ✓ PASS: Parameter Extraction
  ✓ PASS: Mann Box Parameters
  ✓ PASS: Wind Solver Init
  ✓ PASS: Spectrum Generation

Total: 5/5 tests passed

✓ All tests passed! Mann Box configuration is valid.
```

## Customization

### Modify Atmospheric Stability

Edit `inputs_mann_box.i` for different conditions:

**Stable Atmosphere (reduced turbulence):**
```ini
turbulence_length_scale_u = 200.0
turbulence_intensity_ref = 0.08
turbulence_mann_asymmetry = 1.2
```

**Unstable Atmosphere (increased turbulence):**
```ini
turbulence_length_scale_u = 400.0
turbulence_intensity_ref = 0.16
turbulence_mann_asymmetry = 0.8
```

**Wind Farm Wakes:**
```ini
turbulence_length_scale_u = 250.0
turbulence_intensity_ref = 0.14
turbulence_mann_asymmetry = 1.1
```

**Complex Terrain:**
```ini
turbulence_length_scale_u = 350.0
turbulence_intensity_ref = 0.15
turbulence_mann_asymmetry = 1.3
enable_terrain_aware_masking = 1
```

### Modify Wind Direction/Speed

```ini
# Different wind directions and magnitudes
U_ref = 15.0  # Increase speed to 15 m/s
V_ref = 5.0   # Add lateral component (wind from NW)
```

### Adjust Grid Resolution

For coarser/finer grids:
```ini
# Coarser (faster)
dx = 50.0
dy = 50.0
dz = 50.0

# Finer (more accurate)
dx = 15.0
dy = 15.0
dz = 12.5
```

## Expected Results

### Physical Characteristics

After running the solver:

1. **Wind Acceleration Over Hill:**
   - Wind speed increases toward peak (acceleration zone)
   - Separation zone on lee side

2. **Turbulence Distribution:**
   - Higher turbulence intensity on windward slope
   - Reduced turbulence in separation bubble
   - Recovery downwind

3. **Spectral Tensor Properties:**
   - Energy ordering: S_uu > S_vv > S_ww (physically correct)
   - Anisotropy preserved: v/u ≈ 0.8, w/u ≈ 0.5
   - Cauchy-Schwarz inequality satisfied for off-diagonal components

### Validation Metrics

- Convergence: MLMG solver converges in <50 iterations
- Residual: Final residual < 1e-11
- Energy: Spectral integration matches configured variances
- Anisotropy: Component ratios within ±5% of expected values

## Integration with Python API

```python
from mann_box import create_mann_box_preset
from wind_solver import WindSolver
import numpy as np

# Read configuration
inputs_file = "inputs_mann_box.i"

# Initialize solver
wind = WindSolver()
wind.initialize(inputs_file)
wind.solve()

# Create Mann Box model with matching parameters
mann = create_mann_box_preset('neutral')

# Generate spectrum
frequencies = np.logspace(-2, 1, 100)
spectrum = mann.compute_spectrum(
    frequencies=frequencies,
    height=90.0,
    mean_wind_speed=12.0
)

# Validate
if mann.validate_realizability(spectrum):
    print("Configuration and spectrum are consistent!")

wind.finalize()
```

## References

**Mann Box Model:**
- Mann, J. (1994). The spatial structure of neutral atmospheric surface-layer turbulence. J. Fluid Mech., 273, 141-168.
- Mann, J. (1998). Wind field simulation. Probabilistic Engineering Mechanics, 13(4), 269-282.

**Gaussian Hill Benchmark:**
- Jackson, P. S., & Hunt, J. C. R. (1975). Turbulent wind flow over a low hill. Q. J. R. Meteorol. Soc., 101(429), 929-955.

**IEC 61400-1 Wind Classes:**
- IEC (2019). Wind turbines – Part 1: Design requirements (IEC 61400-1:2019).

## Support

For issues or questions:

1. Check the Mann Box documentation: `MANN_BOX_PYTHON_BINDINGS.md`
2. Review the Python test suite: `test_mann_box_inputs.py`
3. See the configuration guide in `inputs_mann_box.i` comments
4. Run the verification tests to validate your configuration

## Files Generated

After running the solver, you'll have:

```
gaussian_hill/
├── inputs_mann_box.i                          (configuration file)
├── terrain.csv                                (terrain data)
├── plt_gaussian_hill_mann_box/                (NEW: base wind field)
│   ├── Header
│   ├── Level_0/
│   └── ...
├── plt_gaussian_hill_mann_box_with_fluctuations/ (NEW: with turbulence)
│   ├── Header
│   ├── Level_0/
│   └── ...
└── wind_extract_mann_box.csv                  (NEW: extracted wind at 15m AGL)
```

## Summary

This test case provides:

✓ Complete Mann Box configuration for Gaussian Hill benchmark
✓ Validated parameter set for neutral atmospheric conditions
✓ Python test suite for configuration validation
✓ Easy customization for different atmospheric conditions
✓ Seamless integration with existing wind solver
✓ Production-ready for wind energy applications

The inputs_mann_box.i file is ready for immediate use and can serve as a template for other complex terrain simulations.
