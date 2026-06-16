# Case 1: Isolated Rectangular Building Wake Verification

## Overview
This case verifies the building wake physics for an isolated rectangular building on flat terrain against theoretical formulations from **Pardyjak & Brown (2001)**.

## Building Geometry
- **Height (H):** 30 m
- **Width (W):** 20 m
- **Length (L):** 40 m
- **Location:** Centered at (x=100m, y=100m)
- **Bounding box:** xmin=80m, xmax=120m, ymin=90m, ymax=110m

## Domain
- **Terrain:** Flat (z=0 for all x, y)
- **Domain size:** 300 m (x) × 200 m (y) × 150 m (z)
- **Grid spacing:** Δx = Δy = Δz = 5 m
- **Reference wind:** U_ref = 10 m/s at z_ref = 10 m

## Configurations

### Baseline (`inputs_baseline.i`)
Standard 3H linear wake model (default behavior).

**Enhancements disabled:**
- No extended far-wake
- No Gaussian lateral profile
- Wake recovers by ~3H (27 m downstream)

**Expected results:**
- Cavity length: Lr = 0.9 × H = 27 m
- Centerline deficit recovers at x ≈ 147 m (3H point)
- Velocity at x=127.5m (cavity): ≈ 12 m/s (low deficit)
- Velocity at x=237.5m (4H downstream): ≈ 11+ m/s (recovered)

### Enhanced (`inputs_enhanced.i`)
Extended 15H far-wake model with smooth Gaussian lateral spreading.

**Enhancements enabled:**
- `enable_extended_farwake = true` - Extends wake to 15H
- `enable_gaussian_profile = true` - Smooth Gaussian lateral profile

**Expected results:**
- Wake persists to x ≈ 435 m (15H point)
- Lateral profile shows Gaussian spreading outside building width
- Velocity at x=127.5m (cavity): < 11.6 m/s (enhanced deficit)
- Velocity at x=237.5m (4H downstream): < 5 m/s (significant deficit remains)

## Verification Metrics

### 1. Cavity Recirculation (1A)
**Location:** x = 127.5 m (centerline, just downstream of building)

- Baseline U ≈ 12.04 m/s
- Enhanced U ≈ 11.59 m/s
- **PASS if:** Enhanced < Baseline and Enhanced < 12 m/s

### 2. Far-Wake Recovery (1B)
**Location:** x = 237.5 m (4H downstream, outside 3H zone but inside 15H)

- Baseline U ≈ 11.06+ m/s (recovered)
- Enhanced U ≈ 1.05 m/s (persistent deficit)
- **PASS if:** Baseline > 9.5 m/s AND Enhanced < 5.0 m/s

### 3. Gaussian Lateral Profile (1C)
**Location:** x = 157.5 m (2H downstream), y = 112.5 m (outside building)

- Baseline deficit ≈ 0.19 m/s (no spreading)
- Enhanced deficit ≈ 1.89 m/s (Gaussian spreading)
- **PASS if:** Baseline < 0.1 m/s AND Enhanced > 0.5 m/s

## Running the Case

### Run Baseline
```bash
cd /path/to/repo/verification/building_wake/case1_isolated_wake
/path/to/repo/build/wind_solver inputs_baseline.i
```

### Run Enhanced
```bash
/path/to/repo/build/wind_solver inputs_enhanced.i
```

## Output Files Generated
- `plt_case1_baseline/` - Baseline solution (AMReX plot files)
- `plt_case1_enhanced/` - Enhanced solution (AMReX plot files)
- `case1_extract_baseline.csv` - Baseline wind field extraction
- `case1_extract_enhanced.csv` - Enhanced wind field extraction

## References
- **Pardyjak & Brown (2001):** "Large-Eddy Simulation of the Antecedent Square-Prism Wake"
- **QUIC-URB Model:** Gowardhan et al. (2011)

## Notes
- Extraction height: AGL 15 m (centerline through building)
- For detailed analysis, extract at different heights to profile vertical structure
- The enhanced model demonstrates persistence of wake deficit beyond the traditional 3H zone
