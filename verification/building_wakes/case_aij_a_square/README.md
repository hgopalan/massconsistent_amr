# AIJ Case A: Isolated Square Building Wake Verification

## Overview
This case verifies building wake physics for an isolated square building (1:1:1 aspect ratio) against theoretical formulations from the **Yoshie et al. (2007) Cooperative project on CFD prediction of pedestrian wind environment** (AIJ - Architectural Institute of Japan collaborative benchmark).

## Building Geometry
- **Height (H):** 20 m
- **Width (W):** 20 m (square footprint)
- **Length (L):** 20 m
- **Location:** Centered at (x=150m, y=100m)
- **Bounding box:** xmin=140m, xmax=160m, ymin=90m, ymax=110m

## Domain
- **Terrain:** Flat (z=0 for all x, y)
- **Domain size:** 400 m (x) × 200 m (y) × 100 m (z)
- **Grid spacing:** Δx = Δy = Δz = 5 m
- **Reference wind:** U_ref = 10 m/s at z_ref = 10 m

## Configurations

### Baseline (`inputs_baseline.i`)
Standard 3H linear wake model (default behavior).

**Enhancements disabled:**
- No extended far-wake
- No Gaussian lateral profile
- Wake recovers by ~3H (60 m downstream)

**Expected results:**
- Cavity length: Lr = 0.9 × H = 18 m
- Centerline deficit recovers at x ≈ 210 m (3H point)
- Upstream stagnation: Minimal (square geometry)
- Velocity at x=170m (cavity): ≈ 9.5 m/s (reduced)
- Velocity at x=230m (4H downstream): ≈ 9.8+ m/s (recovered)

### Enhanced (`inputs_enhanced.i`)
Extended 15H far-wake model with smooth Gaussian lateral spreading.

**Enhancements enabled:**
- `enable_extended_farwake = true` - Extends wake to 15H (300m)
- `enable_gaussian_profile = true` - Smooth Gaussian lateral profile

**Expected results:**
- Wake persists to x ≈ 450 m (15H point)
- Lateral profile shows Gaussian spreading outside building width
- Velocity at x=170m (cavity): < 9.5 m/s (enhanced deficit)
- Velocity at x=230m (4H downstream): < 8.0 m/s (persistent deficit)

## Verification Metrics

### 1. Upstream Stagnation Zone Length
**Location:** x < 140 m (upstream of building)

- Baseline U ≈ 10.0 m/s (minimal reduction)
- Enhanced U ≈ 10.0 m/s (square geometry shows no upwind effects)
- **Expected:** No significant upstream effects for square buildings
- **Literature reference:** Yoshie et al. (2007), symmetric square geometry

### 2. Cavity/Top Wake Length (Upstream to Downstream)
**Location:** x = 140m to x = 210m (cavity zone = 3H = 60m)

- Baseline cavity length: Lr = 0.9 × H = 18 m
- Recovery distance: ~3H = 60 m
- **Expected cavity velocity reduction:** 15-20% at centerline
- **PASS if:** Baseline cavity length ≈ 18-20m

### 3. Downstream Wake Length
**Location:** x > 210m (far-wake zone)

- Baseline wake recovery: Complete by x ≈ 210m (3H)
- Enhanced wake persistence: Extends to x ≈ 450m (15H)
- **Expected downstream recovery rate:** Gradual 1/x-type decay for enhanced
- **PASS if:** 
  - Baseline velocity > 9.8 m/s by x=230m
  - Enhanced velocity < 8.0 m/s at x=230m

## Physical Characteristics of Square Building

### Symmetry
- **Lateral symmetry:** Perfect symmetry about building centerline (y=100m)
- **No preferential flow direction:** Square geometry → isotropic wake
- **Comparable top and bottom corners:** No aspect-ratio effects

### Wind Profile at Reference Height (z_ref = 10m)
- Upstream (x < 140m): U ≈ 10.0 m/s (log-law profile)
- At building edge (x = 160m): Strong shear and acceleration
- In cavity (x = 160-180m): Recirculation and low velocity
- Far-wake (x > 210m): Gradual recovery

## Running the Case

### Run Baseline
```bash
cd /path/to/repo/verification/building_wakes/case_aij_a_square
/path/to/repo/build/wind_solver inputs_baseline.i
```

### Run Enhanced
```bash
/path/to/repo/build/wind_solver inputs_enhanced.i
```

## Output Files Generated
- `plt_aij_case_a_baseline/` - Baseline solution (AMReX plot files)
- `plt_aij_case_a_enhanced/` - Enhanced solution (AMReX plot files)
- `case_aij_a_extract_baseline.csv` - Baseline wind field extraction
- `case_aij_a_extract_enhanced.csv` - Enhanced wind field extraction

## Expected CSV Data Structure
Each extract CSV file should contain columns:
- `x` - Downwind distance (m)
- `y` - Lateral position (m)
- `z` - Height above ground (m)
- `u` - Streamwise wind component (m/s)
- `v` - Lateral wind component (m/s)
- `w` - Vertical wind component (m/s)

## Analysis Points for Verification

### Upstream Region (x = 130-140m)
- Extract velocity profile to verify no upstream stagnation
- For square building: U_upstream ≈ U_ref = 10.0 m/s

### Cavity Zone (x = 160-180m)
- Peak deficit location
- Baseline: U ≈ 9.5 m/s (5% deficit)
- Enhanced: U ≈ 9.2 m/s (8% deficit due to broader profile)

### Recovery Zone (x = 210-230m)
- Transition from cavity to far-wake
- Baseline: U ≈ 9.8 m/s (nearly recovered by 3H)
- Enhanced: U ≈ 8.5 m/s (still in wake)

### Far-Wake Zone (x = 300m+)
- Baseline: W ≈ 10.0 m/s (fully recovered)
- Enhanced: U ≈ 9.5 m/s (still elevated deficit from 15H extension)

## References
- **Yoshie, R., Mochida, A., Tominaga, Y., Kataoka, H., Harimoto, K., Nozu, T., & Shirasawa, T. (2007)**. Cooperative project on CFD prediction of pedestrian wind environment in the built environment. *Journal of Wind Engineering and Industrial Aerodynamics*, 95(12), 1551–1578.
- **Pardyjak, E. R., & Brown, M. J. (2001)**. *QUIC-URB v. 1.1: Theory and User's Guide*. Los Alamos National Laboratory, LA-UR-01-4228.

## Notes
- Extraction height: AGL 10 m (representative height for pedestrian wind comfort assessment)
- For detailed analysis, extract at multiple heights to profile vertical structure
- The square geometry serves as an ideal testbed for wake model symmetry validation
- Compare lateral (y-direction) profiles to verify Gaussian spreading in enhanced case
