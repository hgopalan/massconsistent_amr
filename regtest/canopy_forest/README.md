# Canopy Model Implementation

This directory contains regression tests for the canopy parameterization models.

## Models Implemented

### 1. MacDonald et al. (2000) - Displacement Height Model
- Computes effective displacement height `d` and roughness length `z0_eff`
- Based on canopy morphology parameters (frontal area index, plan area index, drag coefficient)
- Modifies the log-law profile: `u(z) = (u*/κ) * ln((z - d + z0) / z0)`

### 2. Shaw & Pereira (1982) - Exponential Decay Model
- Adds exponential wind speed decay within the canopy
- Profile: `u(z) = u(h) * exp(-α * (1 - z/h))` for z < h
- Above canopy (z ≥ h): standard log-law with displacement height

## Test Cases

### canopy_forest/
Tests the MacDonald displacement height model with typical forest parameters:
- Canopy height: 20 m
- Frontal area index: 0.25
- Plan area index: 0.20
- Drag coefficient: 0.2

### canopy_exponential/
Tests the combined MacDonald + Shaw-Pereira model with:
- Canopy height: 15 m
- Frontal area index: 0.30 (dense canopy)
- Plan area index: 0.25
- Exponential attenuation coefficient: 2.5

## Running Tests

From the repository root:

```bash
cd regtest/canopy_forest
../../build/wind_solver inputs.i

cd ../canopy_exponential
../../build/wind_solver inputs.i
```

## Expected Output

Both tests should:
1. Successfully run without errors
2. Show "canopy model enabled" in the output
3. Generate plotfiles and CSV extracts
4. Produce wind profiles modified by canopy effects

## References

- MacDonald, R.W., Griffiths, R.F., Hall, D.J. (2000). *Atmospheric Environment*, 34(20), 3845-3862.
- Shaw, R.H., Pereira, A.R. (1982). *Agricultural Meteorology*, 26, 51-65.
- Cionco, R.M. (1965). *Journal of Applied Meteorology*, 4, 517-522.
