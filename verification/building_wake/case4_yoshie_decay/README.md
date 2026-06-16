# Case 4: Yoshie Above-Roof Exponential Decay Verification

## Overview
This case verifies advanced building wake physics using the **Yoshie two-layer model** against theoretical formulations from **Yoshie et al. (2007)** and empirical observations from the **QUIC-URB wind solver**.

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

## Physics: Yoshie Two-Layer Model

The Yoshie two-layer model divides the wake into two regions with different deficit recovery dynamics:

### Layer 1: Below-Roof Zone (z ≤ H)
- **Region:** Inside and just above building footprint
- **Behavior:** Standard cavity recirculation with linear recovery
- **Cavity length:** Lr = 0.9 × H = 27 m
- **Recovery profile:** Linear from cavity edge to 3H

### Layer 2: Above-Roof Zone (z > H)
- **Region:** Above building roofline
- **Behavior:** Exponential deficit decay with height
- **Decay model:** Deficit(z) = Deficit(H) × exp(-β × (z - H) / H)
- **Decay coefficient (β):** Tunable parameter (typical: 1.5-2.0)
- **Physical interpretation:** Wake rapidly dissipates above building height due to wake expansion

## Configurations

### Baseline (`inputs_baseline_*.i`)
Standard wake model without Yoshie two-layer physics.

**Two extractions:**
1. `inputs_baseline_below.i` - Extraction at AGL 15m (below-roof: z=15m < H=30m)
2. `inputs_baseline_above.i` - Extraction at AGL 32m (above-roof: z=32m > H=30m)

**Behavior:**
- Below-roof: Standard cavity deficit, Lr = 27m, velocity ≈ 12.04 m/s
- Above-roof: No exponential decay, deficit maintained similar to below-roof

**Expected results:**
- Below-roof velocity at x=127.5m: ≈ 12.04 m/s
- Above-roof velocity at x=157.5m: ≈ 11.53 m/s (similar deficit as below-roof)

### Enhanced (`inputs_yoshie_*.i`)
Yoshie two-layer model with exponential above-roof decay.

**Two extractions:**
1. `inputs_yoshie_below.i` - Extraction at AGL 15m (below-roof)
2. `inputs_yoshie_above.i` - Extraction at AGL 32m (above-roof)

**Yoshie parameters:**
- `enable_yoshie_two_layer = true`
- `yoshie_decay_beta = 1.75` - Exponential decay coefficient

**Behavior:**
- Below-roof: Identical to baseline (backward-compatibility)
- Above-roof: Exponential decay reduces deficit with height

**Expected results:**
- Below-roof velocity at x=127.5m: ≈ 12.04 m/s (same as baseline)
- Above-roof velocity at x=157.5m: ≈ 11.53 m/s → 11.94+ m/s (deficit reduced)

## Height-Dependent Physics

### Below-Roof Layer (z = 15m)
Cavity recirculation zone dominates. Results should be identical between baseline and Yoshie:
- Deep deficit zone directly downstream of building
- Slow recovery by 3H (cavity length)
- Velocity at cavity edge: U ≈ 12 m/s (low)

### Above-Roof Layer (z = 32m = 1.067H)
Wake deficit exponentially decays with height. Yoshie model predicts faster recovery:

**Deficit decay formula:**
```
Deficit(z) = Deficit(H) × exp(-β × (z - H) / H)
Deficit(32m) = Deficit(30m) × exp(-1.75 × (32-30) / 30)
            = Deficit(30m) × exp(-0.1167)
            ≈ 0.89 × Deficit(30m)
```

- Baseline: Deficit remains ~same as below-roof
- Yoshie: Deficit reduced to ~89% of below-roof value at z=32m

## Verification Metrics

### Metric 4A: Below-Roof Cavity Deficit (Backward Compatibility)
**Location:** x = 127.5 m (cavity zone), y = 97.5 m, z = 15 m (below-roof)

- Baseline U ≈ 12.04 m/s
- Yoshie U ≈ 12.04 m/s
- **PASS if:** |Baseline - Yoshie| < 0.1 m/s (identical behavior)

### Metric 4B: Above-Roof Exponential Decay
**Location:** x = 157.5 m (far-wake), y = 97.5 m, z = 32 m (above-roof)

- Baseline velocity: ≈ 11.53 m/s (deficit ≈ 0.47 m/s)
- Yoshie velocity: ≈ 11.94+ m/s (deficit ≈ 0.06 m/s)
- **PASS if:** Yoshie deficit < Baseline deficit AND Yoshie U > Baseline U

**Deficit ratio check:**
- Expected reduction factor: exp(-1.75 × (32-30)/30) ≈ 0.89
- Yoshie deficit ≈ 0.89 × Baseline deficit

## Running the Case

### Run All Four Simulations
```bash
cd /path/to/repo/verification/building_wake/case4_yoshie_decay

# Baseline simulations
/path/to/repo/build/wind_solver inputs_baseline_below.i
/path/to/repo/build/wind_solver inputs_baseline_above.i

# Yoshie simulations
/path/to/repo/build/wind_solver inputs_yoshie_below.i
/path/to/repo/build/wind_solver inputs_yoshie_above.i
```

### Comparison Script (Optional)
To generate a vertical profile plot:
```python
import numpy as np

# Load results
baseline_below = np.genfromtxt('case4_baseline_below.csv', delimiter=',', names=True)
baseline_above = np.genfromtxt('case4_baseline_above.csv', delimiter=',', names=True)
yoshie_below = np.genfromtxt('case4_yoshie_below.csv', delimiter=',', names=True)
yoshie_above = np.genfromtxt('case4_yoshie_above.csv', delimiter=',', names=True)

# Extract at x=157.5m, y=97.5m
def query_point(data, x, y):
    dist = (data['x'] - x)**2 + (data['y'] - y)**2
    return data[np.argmin(dist)]

u_bl_below = query_point(baseline_below, 157.5, 97.5)['u']
u_bl_above = query_point(baseline_above, 157.5, 97.5)['u']
u_yo_below = query_point(yoshie_below, 157.5, 97.5)['u']
u_yo_above = query_point(yoshie_above, 157.5, 97.5)['u']

print(f"Baseline vertical profile at x=157.5m, y=97.5m:")
print(f"  Below-roof (z=15m): U = {u_bl_below:.2f} m/s")
print(f"  Above-roof (z=32m): U = {u_bl_above:.2f} m/s")
print(f"\nYoshie vertical profile at x=157.5m, y=97.5m:")
print(f"  Below-roof (z=15m): U = {u_yo_below:.2f} m/s")
print(f"  Above-roof (z=32m): U = {u_yo_above:.2f} m/s")
```

## Output Files Generated
- `plt_case4_baseline_below/` - Baseline below-roof solution
- `plt_case4_baseline_above/` - Baseline above-roof solution
- `plt_case4_yoshie_below/` - Yoshie below-roof solution
- `plt_case4_yoshie_above/` - Yoshie above-roof solution
- `case4_baseline_below.csv` - Baseline extraction at z=15m
- `case4_baseline_above.csv` - Baseline extraction at z=32m
- `case4_yoshie_below.csv` - Yoshie extraction at z=15m
- `case4_yoshie_above.csv` - Yoshie extraction at z=32m

## Analysis Recommendations
1. **Vertical profile analysis** at different x-positions
2. **Deficit decay rate** verification (check exponential form)
3. **Height-dependent velocity recovery** downstream
4. **Backward compatibility** verification (below-roof identical)
5. **Sensitivity analysis** on decay coefficient β

## References
- **Yoshie et al. (2007):** *Cooperative project on CFD prediction of pedestrian wind environment in the built environment*. *Journal of Wind Engineering and Industrial Aerodynamics*, 95(12), 1551–1578.
- **Pardyjak & Brown (2001):** *QUIC-URB v. 1.1: Theory and User's Guide*. Los Alamos National Laboratory, LA-UR-01-4228.

## Notes
- Extraction heights: 15m (below-roof, z < H) and 32m (above-roof, z > H)
- Yoshie decay coefficient (β = 1.75) is tunable based on empirical data
- Below-roof behavior should be identical to baseline (backward compatibility)
- Above-roof exponential decay is the key physics addition
- This model is particularly important for tall buildings and boundary layer modeling
