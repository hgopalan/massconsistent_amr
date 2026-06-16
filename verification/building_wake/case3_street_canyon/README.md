# Case 3: 2D Building Array & Street Canyon Verification

## Overview
This case verifies building wake physics for an array of buildings forming an urban canyon configuration against theoretical formulations from **Brown et al. (2000)** and empirical observations from the **MUST Experiment**.

## Building Geometry
### Building 1
- **Height (H):** 20 m
- **Width (W):** 20 m
- **Length (L):** 40 m
- **Location:** xmin=80m, xmax=100m, ymin=80m, ymax=120m

### Building 2 (Downwind)
- **Height (H):** 20 m
- **Width (W):** 20 m
- **Length (L):** 40 m
- **Location:** xmin=130m, xmax=150m, ymin=80m, ymax=120m

### Canyon Geometry
- **Canyon width:** 30 m (between buildings)
- **Canyon aspect ratio:** H/W = 20m / 30m = 0.67 (shallow canyon)
- **Canyon centerline:** y = 100 m (between building rows)
- **Along-canyon flow:** Parallel to y-axis

## Domain
- **Terrain:** Flat (z=0 for all x, y)
- **Domain size:** 300 m (x) × 200 m (y) × 150 m (z)
- **Grid spacing:** Δx = Δy = Δz = 5 m
- **Reference wind:** U_ref = 10 m/s at z_ref = 10 m
- **Wind direction:** Perpendicular to canyon (along x-axis)

## Configurations

### Baseline (`inputs_baseline.i`)
Standard wake model without upwind recirculation zone.

**Enhancements disabled:**
- No upwind recirculation zone
- Flow may penetrate fully into upwind cavity
- Simple cavity pressure recovery

**Expected results:**
- Upwind velocity (x=77.5m): ≈ 9.85 m/s (minimal reduction)
- Canyon velocity average: ≈ varies with cavity dynamics
- Downwind building wake: Modulated by upstream building wake

### Enhanced (`inputs_enhanced.i`)
Includes upwind stagnation/recirculation zone modeling.

**Enhancements enabled:**
- `enable_upwind_recirculation = true` - Models stagnation ~0.5×min(H,W) upstream
- Creates horse-shoe vortex zone
- Modulates upwind pressure field

**Expected results:**
- Upwind stagnation distance: x_stag ≈ 0.5 × min(H,W) = 10 m
- Upwind velocity at x=77.5m (2.5m upstream): ≈ 9.36 m/s (reduced)
- Canyon velocity average: ≈ 5.95 m/s (sheltered)
- More realistic pressure recovery field

## Physical Zones

### Upwind Zone (x < 80m)
- **Baseline:** Gradual pressure buildup upstream
- **Enhanced:** Sharp recirculation zone at x ≈ 70m (0.5×H upstream)
- Reverse flow observed just upstream of building

### Canyon Zone (100m < x < 130m)
- **Baseline:** Flow funneling through gap, some acceleration possible
- **Enhanced:** Sheltering effect from both upwind and downwind buildings
- Wind speed attenuation due to pressure shadow

### Downwind Zone (x > 150m)
- **Building 2 wake:** Extends 3H ≈ 60m
- Recovery to ambient by x ≈ 210m
- Possible interaction with upstream cavity if buildings close enough

## Verification Metrics

### 1. Upwind Stagnation Zone (3A)
**Location:** x = 77.5 m (2.5m upstream of Building 1, y = 100m)

- Baseline U ≈ 9.85 m/s (minimal reduction)
- Enhanced U ≈ 9.36 m/s (stagnation zone active)
- **PASS if:** Enhanced < Baseline AND Enhanced < 9.5 m/s

### 2. Canyon Wind Speed Attenuation (3B)
**Locations:** x = 102.5, 107.5, 112.5, 117.5, 122.5, 127.5 m (canyon centerline, y = 100m)

- Baseline canyon average: ≈ varies
- Enhanced canyon average: ≈ 5.95 m/s (strong sheltering)
- **PASS if:** Both Baseline and Enhanced < 7.0 m/s (expected canyon sheltering)

## Running the Case

### Run Baseline
```bash
cd /path/to/repo/verification/building_wake/case3_street_canyon
/path/to/repo/build/wind_solver inputs_baseline.i
```

### Run Enhanced
```bash
/path/to/repo/build/wind_solver inputs_enhanced.i
```

## Output Files Generated
- `plt_case3_baseline/` - Baseline solution (AMReX plot files)
- `plt_case3_enhanced/` - Enhanced solution (AMReX plot files)
- `case3_extract_baseline.csv` - Baseline wind field extraction (AGL 5m)
- `case3_extract_enhanced.csv` - Enhanced wind field extraction (AGL 5m)

## Analysis Recommendations
1. **Velocity profiles** along x-axis through canyon centerline
2. **Lateral profiles** (y-direction) at various x-positions
3. **Height-dependent analysis** at z = 5m (street level), z = 10m, z = 20m
4. **Pressure field comparison** to verify stagnation zone

## References
- **Brown et al. (2000):** "Street Canyon Generation and Tracking Algorithms"
- **Britter & Hanna (2003):** "Flow and Dispersion in Urban Areas"
- **MUST Experiment:** Dinar et al. (2000)

## Notes
- Extraction height: AGL 5 m (street level near ground)
- Two-building array represents idealized urban canyon
- Results scale with H/W ratio and building separation
- Wind direction perpendicular to canyon for maximum effect
