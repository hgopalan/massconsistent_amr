# Case 2: Tall Building with Aspect-Ratio & Corner Effects Verification

## Overview
This case verifies building wake physics for a tall, narrow building against theoretical formulations from **Gowardhan et al. (2011)** and **Yoshie et al. (2007)**.

## Building Geometry
- **Height (H):** 50 m (tall)
- **Width (W):** 15 m (narrow)
- **Length (L):** 20 m
- **Aspect Ratio:** H/W = 3.33 (ratio >2 triggers aspect-ratio correction)
- **Location:** Centered at (x=100m, y=100m)
- **Bounding box:** xmin=90m, xmax=110m, ymin=92.5m, ymax=107.5m

## Domain
- **Terrain:** Flat (z=0 for all x, y)
- **Domain size:** 300 m (x) × 200 m (y) × 200 m (z)
- **Grid spacing:** Δx = Δy = Δz = 5 m
- **Reference wind:** U_ref = 10 m/s at z_ref = 10 m

## Configurations

### Baseline (`inputs_baseline.i`)
Standard wake model without aspect-ratio correction or corner effects.

**Enhancements disabled:**
- No tall building aspect-ratio correction
- No corner/side velocity acceleration
- Uses standard cavity length Lr = 0.9 × H = 45 m

**Expected results:**
- Centerline velocity at x=142.5m (cavity edge): ≈ 13.95 m/s
- Side velocity at x=112.5m (corner): ≈ 12.58 m/s
- Wake recovers by 3H with standard linear profile

### Enhanced (`inputs_enhanced.i`)
Tall building physics with aspect-ratio correction and corner flow speedup.

**Enhancements enabled:**
- `enable_tall_building_correction = true` - Applies H/W scaling to cavity length
- `enable_corner_acceleration = true` - Models sideward flow acceleration at corners

**Expected results:**
- Cavity length scaled by aspect ratio: Lr = 0.9 × H × f(H/W)
- Centerline velocity at x=142.5m: ≈ 13.87 m/s (slightly recovered)
- Side velocity at x=112.5m: ≈ 12.57 m/s (corner acceleration active)
- Aspect-ratio correction restricts cavity zone on tall buildings

## Verification Metrics

### 1. Corner/Side Velocity Acceleration (2A)
**Location:** x = 112.5 m (corner region), y = 107.5 m (building edge)

- Baseline U ≈ 12.58 m/s
- Enhanced U ≈ 12.57 m/s
- **PASS if:** Both baseline and enhanced show corner speedup, Enhanced ≈ Baseline

### 2. Tall Building Centerline Wake (2B)
**Location:** x = 142.5 m (cavity region, downwind)

- Baseline U ≈ 13.95 m/s
- Enhanced U ≈ 13.87 m/s
- **PASS if:** Enhanced < Baseline (aspect-ratio correction reduces cavity size)

## Height Dependence
This case is best analyzed at multiple heights to observe:
- **Below roofline (z < 50m):** Primary cavity zone
- **Above roofline (z > 50m):** Reduced wake effect with tall building
- **Far field (z > 100m):** Nearly ambient conditions

## Running the Case

### Run Baseline
```bash
cd /path/to/repo/verification/building_wake/case2_tall_building
/path/to/repo/build/wind_solver inputs_baseline.i
```

### Run Enhanced
```bash
/path/to/repo/build/wind_solver inputs_enhanced.i
```

## Output Files Generated
- `plt_case2_baseline/` - Baseline solution (AMReX plot files)
- `plt_case2_enhanced/` - Enhanced solution (AMReX plot files)
- `case2_extract_baseline.csv` - Baseline wind field extraction (AGL 25m)
- `case2_extract_enhanced.csv` - Enhanced wind field extraction (AGL 25m)

## References
- **Gowardhan et al. (2011):** *Evaluation of a Fast and Simple Obstruction Modeling Approach for Use in Urban Wind Resource Estimation*.
- **Yoshie et al. (2007):** *Cooperative project on CFD prediction of pedestrian wind environment in the built environment*. *Journal of Wind Engineering and Industrial Aerodynamics*, 95(12), 1551–1578.

## Notes
- Extraction height: AGL 25 m (midway between roof and building top)
- For aspect-ratio studies, use buildings with H/W > 2.0
- Corner acceleration is most pronounced at building corners and sides
- Tall building correction can significantly alter wake extent downstream
