# Phase 5: OpenFAST Export Tool - Regression Tests

This directory contains regression tests for the OpenFAST/TurbSim export tool (Phase 5).

## Test Cases

### 1. BTS Format Compliance (`test_openfast_export.py`)

Unit tests for the TurbSim binary format (`.bts`) writer:

- **TestBTSFormat**: Core BTS format validation
  - Header validity checks (format identifiers, grid dimensions, time steps)
  - Metadata initialization (turbulence intensity, integral scales, physical parameters)
  - File creation and binary format verification
  - Metadata file generation (`.meta` files)

- **TestOpenFASTIntegration**: OpenFAST compatibility tests
  - Physical parameter ranges (wind speed, turbulence intensity, hub height)
  - Turbulence intensity profile relationships
  - Integral length scale parameters

- **TestDataFormatting**: Data handling tests
  - Float precision preservation
  - Data type conversion (float32, float64, int)

### 2. Gaussian Hill Test (`test_phase5_openfast_gaussian_hill.py`)

Integration test using a Gaussian hill terrain:

- **Test 1: BTS Format Compliance**
  - Creates BTS file with Gaussian hill parameters
  - Verifies binary header format
  - Checks file size and structure
  - Validates metadata file generation

- **Test 2: Metadata Parameters**
  - Tests terrain-specific turbulence parameters
  - Validates intensity relationships (v ≈ 0.8u, w ≈ 0.5u)
  - Verifies integral length scales
  - Checks surface roughness settings

- **Test 3: Physical Parameter Ranges**
  - Tests multiple atmospheric conditions (neutral, strong wind, low wind, high hub)
  - Validates physical ranges for all parameters
  - Ensures parameter consistency

## Input Files

- `gaussian_hill_inputs.i`: Wind solver configuration for Gaussian hill
- `regtest_terrain.csv`: Gaussian hill terrain elevation data (500m × 500m × 300m domain)

## Running the Tests

### Run all BTS format tests:
```bash
cd /tmp/workspace/hgopalan/massconsistent_amr
python3 regtest/phase5_openfast_export/test_openfast_export.py
```

### Run Gaussian hill regression test:
```bash
cd /tmp/workspace/hgopalan/massconsistent_amr
python3 regtest/phase5_openfast_export/test_phase5_openfast_gaussian_hill.py
```

### Run specific test class:
```bash
python3 -m unittest regtest.phase5_openfast_export.test_openfast_export.TestBTSFormat
```

## Expected Output

When all tests pass:
```
======================================================================
Phase 5: OpenFAST Export - Gaussian Hill Regression Test
======================================================================

======================================================================
Test 1: BTS Format Compliance
======================================================================
✓ BTS header valid
  Grid: 20 × 20 × 15
  Time steps: 1
  Hub height: 90.0 m
  Mean wind: 10.5 m/s
✓ BTS file created: ...
✓ BTS header format verified
✓ Metadata file verified

✓ Test 1 PASSED

... [Test 2 and 3 output] ...

======================================================================
Test Summary
======================================================================
BTS Format Compliance.............................✓ PASSED
Metadata Parameters...............................✓ PASSED
Physical Parameter Ranges..........................✓ PASSED

Total: 3/3 tests passed

✓ All regression tests PASSED
```

## Test Parameters

### Gaussian Hill Domain
- **Horizontal extent**: 500m × 500m (21 × 21 grid points with 25m spacing)
- **Vertical extent**: 300m (15 levels with 20m spacing)
- **Peak elevation**: 115m (15m above base)
- **Base elevation**: 100m

### Wind Parameters
- **Reference wind speed**: 10 m/s at 10m height
- **Hub height**: 90 m AGL
- **Surface roughness**: 0.1 m (grass/low vegetation)
- **Expected speed-up**: 5-10% over peak

### Turbulence Parameters
- **Model**: Von Kármán spectrum
- **Intensity**: 14% (neutral atmospheric conditions)
- **Integral scales**: u=100m, v=100m, w=50m
- **Coherence model**: Gaussian

## Dependencies

- NumPy (optional, for some tests)
- Python 3.6+

## Troubleshooting

### "ModuleNotFoundError: No module named 'numpy'"

This is expected if NumPy is not installed. The tool works without NumPy,
but some validation tests will be skipped. To use NumPy-dependent tests:

```bash
pip install numpy
```

### BTS file not created

Check that:
1. Output directory exists and is writable
2. Sufficient disk space available
3. Wind solver is properly initialized

### Metadata file not found

The tool should automatically generate `.meta` files alongside `.bts` files.
If missing, check write permissions on output directory.

## References

- NREL TurbSim User's Guide (v1.06.00+)
- OpenFAST Documentation: ExternalInputs module
- massconsistent_amr Phase 3: `turbsim_bts_export.H`
