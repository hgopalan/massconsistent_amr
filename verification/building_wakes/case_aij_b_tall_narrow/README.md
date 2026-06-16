# AIJ Case B: Tall Narrow Building Wake with Aspect-Ratio Effects

## Overview
This case verifies building wake physics for an isolated tall, narrow building (3:1:1 aspect ratio) against theoretical formulations from the **Yoshie et al. (2007) Cooperative project on CFD prediction of pedestrian wind environment** (AIJ - Architectural Institute of Japan collaborative benchmark) and **Gowardhan et al. (2011)** aspect-ratio effects.

## Building Geometry
- **Height (H):** 30 m (tall)
- **Width (W):** 10 m (narrow)
- **Length (L):** 10 m (narrow)
- **Aspect Ratio:** H/W = 3.0 (triggers aspect-ratio correction)
- **Location:** Centered at (x=150m, y=100m)
- **Bounding box:** xmin=145m, xmax=155m, ymin=95m, ymax=105m

## Domain
- **Terrain:** Flat (z=0 for all x, y)
- **Domain size:** 400 m (x) × 200 m (y) × 150 m (z)
- **Grid spacing:** Δx = Δy = Δz = 5 m
- **Reference wind:** U_ref = 10 m/s at z_ref = 10 m

## Configurations

### Baseline (`inputs_baseline.i`)
Standard 3H linear wake model without aspect-ratio correction.

**Enhancements disabled:**
- No tall building aspect-ratio correction
- No corner flow acceleration
- Cavity length Lr = 0.9 × H = 27 m (standard)

**Expected results:**
- Cavity length: 27 m (standard without correction)
- Centerline deficit recovers at x ≈ 210 m (3H point)
- Upstream stagnation: Present at 0.5×W = 5m upstream
- Velocity at x=165m (cavity): ≈ 9.0 m/s (reduced)
- Velocity at x=230m (4H downstream): ≈ 9.7+ m/s (recovered)

### Enhanced (`inputs_enhanced.i`)
Tall building physics with aspect-ratio correction and corner acceleration.

**Enhancements enabled:**
- `enable_tall_building_correction = true` - Applies H/W scaling
- `enable_corner_acceleration = true` - Models sideward flow speedup

**Expected results:**
- Cavity length scaled: Lr = 0.9 × H × f(H/W)
- H/W = 3.0 triggers cavity length modification
- Centerline velocity at x=165m: < 9.0 m/s (deeper deficit)
- Corner acceleration: Peak ~20% speedup at building corners
- Velocity at x=230m (4H): < 8.0 m/s (persistent deficit)

## Verification Metrics

### 1. Upstream Stagnation Zone Length
**Location:** x = 140-145 m (upstream of building)

- Baseline U ≈ 9.95 m/s (0.5% reduction)
- Enhanced U ≈ 9.90 m/s (1% reduction with stagnation zone)
- **Expected:** Stagnation zone extends ~0.5×min(H,W) = 5m upstream
- **PASS if:** Upstream deficit present and proportional to building dimensions

### 2. Cavity/Top Wake Length
**Location:** x = 155m to x = 210m (cavity zone)

- Baseline cavity length: Lr = 0.9 × H = 27 m (x=155 to x=182)
- Enhanced cavity length: Scaled by aspect ratio f(3.0)
- **Expected cavity velocity reduction:** 20-25% at centerline (taller building)
- **PASS if:** 
  - Baseline cavity: Lr ≈ 27m
  - Enhanced cavity: Lr = 27 × f(H/W) where f(3.0) typically 0.8-1.0

### 3. Downstream Wake Length
**Location:** x > 210m (far-wake zone)

- Baseline wake recovery: Complete by x ≈ 210m (3H)
- Enhanced aspect-ratio correction: Reduces cavity zone slightly
- **Expected downstream recovery rate:** Linear decay in 3H zone
- **PASS if:** 
  - Baseline velocity > 9.7 m/s by x=230m
  - Enhanced shows appropriate aspect-ratio scaling effect

## Physical Characteristics of Tall Narrow Building

### Aspect-Ratio Effects
- **H/W = 3.0:** High aspect ratio triggers correction
- **Cavity depth:** Reduced compared to standard 0.9H due to aspect ratio
- **Lateral confinement:** Narrow building (W=10m) induces strong lateral acceleration at sides

### Corner Acceleration
- **Peak acceleration:** ~20% speedup at building corners (y=95m and y=105m)
- **Location:** Maximum near building mid-height (z ≈ 15m)
- **Decay:** Decreases with distance from building

### Wind Profile at Reference Height (z_ref = 10m)
- Upstream (x < 145m): U ≈ 10.0 m/s with stagnation zone
- At building edge (x = 155m): Strong shear and corner acceleration
- In cavity (x = 155-182m): Deep recirculation (tall building)
- Far-wake (x > 210m): Gradual recovery
- Side acceleration (y < 95m, y > 105m): Corner speedup effect

## Running the Case

### Run Baseline
```bash
cd /path/to/repo/verification/building_wakes/case_aij_b_tall_narrow
/path/to/repo/build/wind_solver inputs_baseline.i
```

### Run Enhanced
```bash
/path/to/repo/build/wind_solver inputs_enhanced.i
```

## Output Files Generated
- `plt_aij_case_b_baseline/` - Baseline solution (AMReX plot files)
- `plt_aij_case_b_enhanced/` - Enhanced solution (AMReX plot files)
- `case_aij_b_extract_baseline.csv` - Baseline wind field extraction
- `case_aij_b_extract_enhanced.csv` - Enhanced wind field extraction

## Expected CSV Data Structure
Each extract CSV file should contain columns:
- `x` - Downwind distance (m)
- `y` - Lateral position (m)
- `z` - Height above ground (m)
- `u` - Streamwise wind component (m/s)
- `v` - Lateral wind component (m/s)
- `w` - Vertical wind component (m/s)

## Analysis Points for Verification

### Upstream Region (x = 140-145m, y = 100m)
- Verify stagnation zone presence
- Baseline stagnation: 0.5% velocity reduction
- Enhanced stagnation: 1-2% velocity reduction

### Corner Region (x = 155m, y = 95m and y = 105m)
- Verify corner acceleration
- Expected enhancement: 10-20% speedup compared to centerline
- Height dependent: Maximum near mid-height

### Cavity Zone (x = 160-180m, y = 100m)
- Peak deficit location
- Baseline: U ≈ 9.0 m/s (10% deficit)
- Enhanced: U ≈ 8.7 m/s (13% deficit, deeper for tall building)

### Recovery Zone (x = 210-230m, y = 100m)
- Transition from cavity to far-wake
- Baseline: U ≈ 9.7 m/s (nearly recovered by 3H)
- Enhanced: U ≈ 8.5 m/s (still influenced by aspect-ratio scaling)

### Far-Wake Zone (x = 300m, y = 100m)
- Baseline: U ≈ 10.0 m/s (fully recovered)
- Enhanced: U ≈ 9.8 m/s (nearly recovered)

## References
- **Yoshie, R., Mochida, A., Tominaga, Y., Kataoka, H., Harimoto, K., Nozu, T., & Shirasawa, T. (2007)**. Cooperative project on CFD prediction of pedestrian wind environment in the built environment. *Journal of Wind Engineering and Industrial Aerodynamics*, 95(12), 1551–1578.
- **Gowardhan, A., Brown, M. J., Pardyjak, E. R., & Norgren, H. (2011)**. Evaluation of a fast and simple obstruction modeling approach for use in urban wind resource estimation. *Wind Engineering*, 35(6), 697–713.
- **Pardyjak, E. R., & Brown, M. J. (2001)**. *QUIC-URB v. 1.1: Theory and User's Guide*. Los Alamos National Laboratory, LA-UR-01-4228.

## Notes
- Extraction height: AGL 15 m (below building height, within cavity zone)
- For detailed analysis, extract at multiple heights: 5m, 15m, 25m to profile vertical structure
- Tall narrow building serves as testbed for aspect-ratio correction and anisotropic wake effects
- Compare corner velocities to verify sideward acceleration model
- Height-dependent analysis is critical for tall building verification
