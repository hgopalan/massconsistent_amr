# Building Wake Physics Verification Suite

Complete standalone C++ test cases for verifying advanced building wake physics enhancements in the mass-consistent wind solver.

## Overview

This verification suite contains **four independent test cases** designed to validate building wake modeling against theoretical formulations and empirical observations from the QUIC-URB wind solver ecosystem.

All test cases use standalone input files (`.i`) that can be run directly with the C++ `wind_solver` executable without requiring Python scripts.

## Case Structure

Each case is organized in its own directory with the following files:

```
case_name/
├── README.md                    # Detailed case documentation
├── terrain.csv                  # Terrain/topography input
├── buildings.csv                # Building geometry definitions
├── inputs_baseline.i            # Baseline configuration
├── inputs_enhanced.i            # Enhanced model configuration (or variants)
└── [other input files]
```

## The Four Cases

### 1. Case 1: Isolated Building Wake
**Directory:** `case1_isolated_wake/`

Verifies cavity length, far-wake extension, and Gaussian lateral profile for an isolated rectangular building.

- **Building:** H=30m, W=20m, L=40m
- **Tests:** Baseline (3H linear) vs Enhanced (15H far-wake + Gaussian profile)
- **Key enhancements:** Extended far-wake, Gaussian lateral spreading
- **Reference:** Pardyjak & Brown (2001)

**Run:**
```bash
cd case1_isolated_wake
../../../build/wind_solver inputs_baseline.i
../../../build/wind_solver inputs_enhanced.i
```

### 2. Case 2: Tall Building with Aspect-Ratio Effects
**Directory:** `case2_tall_building/`

Verifies tall building aspect-ratio correction and corner flow acceleration.

- **Building:** H=50m, W=15m, L=20m (tall, narrow)
- **Aspect ratio:** H/W = 3.33
- **Tests:** Baseline vs Enhanced (with aspect-ratio correction + corner acceleration)
- **Key enhancements:** Tall building correction, corner speedup
- **Reference:** Gowardhan et al. (2011), Yoshie et al. (2007)

**Run:**
```bash
cd case2_tall_building
../../../build/wind_solver inputs_baseline.i
../../../build/wind_solver inputs_enhanced.i
```

### 3. Case 3: 2D Building Array & Street Canyon
**Directory:** `case3_street_canyon/`

Verifies upwind recirculation zone and canyon wind speed attenuation for building arrays.

- **Buildings:** Two H=20m buildings with 30m canyon gap
- **Aspect ratio:** H/W = 20/30 = 0.67 (shallow canyon)
- **Tests:** Baseline vs Enhanced (with upwind recirculation)
- **Key enhancements:** Upwind stagnation zone, recirculation modeling
- **Reference:** Brown et al. (2000), MUST Experiment

**Run:**
```bash
cd case3_street_canyon
../../../build/wind_solver inputs_baseline.i
../../../build/wind_solver inputs_enhanced.i
```

### 4. Case 4: Yoshie Above-Roof Exponential Decay
**Directory:** `case4_yoshie_decay/`

Verifies two-layer model with exponential above-roof deficit decay.

- **Building:** H=30m, W=20m, L=40m (same as Case 1)
- **Tests:** Multiple simulations at below-roof and above-roof heights
  - `inputs_baseline_below.i` - Baseline at z=15m (below-roof)
  - `inputs_baseline_above.i` - Baseline at z=32m (above-roof)
  - `inputs_yoshie_below.i` - Yoshie at z=15m (below-roof)
  - `inputs_yoshie_above.i` - Yoshie at z=32m (above-roof)
- **Key enhancements:** Yoshie two-layer model, exponential decay
- **Yoshie parameter:** `yoshie_decay_beta = 1.75`
- **Reference:** Yoshie et al. (2007), Pardyjak & Brown (2001)

**Run:**
```bash
cd case4_yoshie_decay
../../../build/wind_solver inputs_baseline_below.i
../../../build/wind_solver inputs_baseline_above.i
../../../build/wind_solver inputs_yoshie_below.i
../../../build/wind_solver inputs_yoshie_above.i
```

## Common Input Parameters

All input files share common parameters:

### Domain Configuration
```
dx = 5.0                  # Grid spacing (m)
dy = 5.0
dz = 5.0
domain_height = 150.0     # Total domain height (m)
```

### Reference Wind
```
U_ref = 10.0             # Reference wind speed (m/s)
V_ref = 0.0              # Lateral wind speed
z_ref = 10.0             # Reference height (m)
z0 = 0.1                 # Surface roughness (m)
```

### Wake Parameters
```
enable_wake = true       # Enable building wake model
wake_c1 = 0.9            # Cavity length coefficient
wake_c2 = 0.3            # Wake recovery coefficient
wake_separation_length = 3.0  # Wake separation zone (H units)
```

### Solver Parameters
```
mlmg_verbose = 0         # MLMG verbosity level
max_grid_size = 32       # Maximum AMReX grid size
tol_rel = 1.e-8          # Relative solver tolerance
```

### Output Configuration
```
plot_file = plt_case1_baseline    # Output plot file prefix
extract_agl = 15.0                # Extraction height above ground (m)
extract_file = case1_extract.csv  # CSV output filename
```

## Enhancement Flags

Each case uses different enhancement flags to test specific physics:

### Available Enhancements
```
enable_oblique_scaling = true/false          # Oblique wind angle scaling
enable_tall_building_correction = true/false # Aspect-ratio correction (H/W)
enable_gaussian_profile = true/false         # Gaussian lateral spreading
enable_upwind_recirculation = true/false     # Upwind stagnation zone
enable_corner_acceleration = true/false      # Corner flow speedup
enable_horseshoe_vortex = true/false         # Horseshoe vortex modeling
enable_extended_farwake = true/false         # Extended 15H far-wake
enable_variance_correction = true/false      # Variance correction
enable_yoshie_two_layer = true/false         # Yoshie two-layer model
yoshie_decay_beta = 1.75                     # Yoshie decay coefficient
```

## Running All Cases

To run all four verification cases:

```bash
#!/bin/bash
WIND_SOLVER="../../../build/wind_solver"
REPO_ROOT="../../../"

echo "Running Case 1: Isolated Building Wake"
cd case1_isolated_wake
$WIND_SOLVER inputs_baseline.i
$WIND_SOLVER inputs_enhanced.i
cd ..

echo "Running Case 2: Tall Building"
cd case2_tall_building
$WIND_SOLVER inputs_baseline.i
$WIND_SOLVER inputs_enhanced.i
cd ..

echo "Running Case 3: Street Canyon"
cd case3_street_canyon
$WIND_SOLVER inputs_baseline.i
$WIND_SOLVER inputs_enhanced.i
cd ..

echo "Running Case 4: Yoshie Decay"
cd case4_yoshie_decay
$WIND_SOLVER inputs_baseline_below.i
$WIND_SOLVER inputs_baseline_above.i
$WIND_SOLVER inputs_yoshie_below.i
$WIND_SOLVER inputs_yoshie_above.i
cd ..

echo "All verification cases completed!"
```

## Expected Output Files

After running each case, the following files are generated:

### Plot Files (AMReX native format)
- `plt_case*_*/` - Directories containing full 3D solution fields

### Extraction CSV Files
- `case*_extract*.csv` - Point/line extracted data for analysis
  - Columns: `x`, `y`, `z`, `u`, `v`, `w` (velocity components)

### Configuration Documentation
- `README.md` - Detailed case description and verification metrics

## Verification Workflow

### For Each Case:

1. **Read the README.md** in the case directory for detailed documentation
2. **Review the input files** to understand parameters and enhancements
3. **Run the baseline simulation** with `inputs_baseline.i`
4. **Run the enhanced simulation** with `inputs_enhanced.i` (or variants)
5. **Extract data** from the CSV files for comparison
6. **Verify metrics** specified in the README (e.g., velocity at specific points)
7. **Compare results** between baseline and enhanced models

### Expected Verification Metrics:

Each case has specific metrics to verify:

| Case | Metric 1 | Metric 2 | Metric 3 |
|------|----------|----------|----------|
| 1 | Cavity velocity | Far-wake recovery | Lateral profile |
| 2 | Corner speedup | Tall building wake | Aspect ratio effect |
| 3 | Upwind stagnation | Canyon attenuation | - |
| 4 | Below-roof backward-compat | Above-roof decay | Exponential profile |

## Physical Domains

### Case 1 & 4 (Isolated Building)
- Domain: 300m × 200m × 150m
- Grid: 5m × 5m × 5m (60 × 40 × 30 points)
- Wind direction: +x (along domain length)

### Case 2 (Tall Building)
- Domain: 300m × 200m × 200m
- Grid: 5m × 5m × 5m (60 × 40 × 40 points)
- Wind direction: +x (along domain length)

### Case 3 (Street Canyon)
- Domain: 300m × 200m × 150m
- Grid: 5m × 5m × 5m (60 × 40 × 30 points)
- Wind direction: +x (perpendicular to canyon)
- Buildings aligned along ±y direction

## Computational Requirements

Typical runtime per case (CPU):
- **Case 1:** ~5-10 minutes
- **Case 2:** ~10-15 minutes (larger domain)
- **Case 3:** ~5-10 minutes
- **Case 4:** ~20-30 minutes total (4 runs)

Memory requirements: ~100-500 MB per run depending on grid size

## References

### Academic Papers
- **Pardyjak & Brown (2001):** *QUIC-URB v. 1.1: Theory and User's Guide*. Los Alamos National Laboratory, LA-UR-01-4228.
- **Gowardhan et al. (2011):** *Evaluation of a Fast and Simple Obstruction Modeling Approach for Use in Urban Wind Resource Estimation*.
- **Yoshie et al. (2007):** *Cooperative project on CFD prediction of pedestrian wind environment in the built environment*. *Journal of Wind Engineering and Industrial Aerodynamics*, 95(12), 1551–1578.
- **Brown et al. (2000):** *Mean flow and turbulence measurements around a 2-D array of buildings in a wind tunnel*. *Journal of Applied Meteorology*, 40(10), 1882–1897.
- **Britter & Hanna (2003):** *Flow and dispersion in urban areas*. *Annual Review of Fluid Mechanics*, 35, 469–496.

### Related Models
- **QUIC-URB:** Fast diagnostic wind solver from University of Utah
- **MUST Experiment:** Mock Urban Setting Test - urban wind field experiments

## Troubleshooting

### Case fails to run
1. Check that `wind_solver` executable exists at correct path
2. Verify terrain.csv and buildings.csv files are readable
3. Check file paths in inputs_*.i files (use relative paths from case directory)
4. Review wind_solver error output

### Unexpected results
1. Verify enhancement flags match the intended case variant
2. Check extraction height (AGL parameter) against verification metrics
3. Compare velocity values to expected ranges in README
4. Examine full 3D solutions in plt_* directories for validation

### Output files missing
1. Check output file paths in inputs_*.i configuration
2. Verify write permissions in case directory
3. Check wind_solver stderr for I/O errors

## Future Extensions

These cases can be extended to study:
- Sensitivity to grid resolution (refine dx, dy, dz)
- Wind direction effects (varying wind angle)
- Terrain effects (add sloped terrain)
- Multiple buildings (array effects)
- Seasonal or atmospheric stability variations
- Coupling with turbulence models

## Author & Contributions

These verification cases were created as part of the mass-consistent AMR wind solver development.

**Based on:**
- Python verification script: `run_verification.py`
- QUIC-URB validation studies
- Academic literature cited above

## License

Same as parent repository (see LICENSE file)

---

**Last Updated:** 2026-06-16

For questions or issues, refer to individual case README files or main repository documentation.
