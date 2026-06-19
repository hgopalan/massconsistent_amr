# Data Center Heat Source - Test Cases

This directory contains test cases for validating the data center heat source implementation in the mass-consistent wind solver.

## Test Cases

### 1. Flat Terrain (Single Facility)

**File:** `flat_terrain_inputs.i`

A single 10 MW data center on flat ground with neutral atmospheric stratification.

**Configuration:**
- Heat release: 10 MW
- Location: (1500 m, 1500 m) 
- Height: 10 m AGL
- Domain: 3000 m × 3000 m × 300 m
- Grid spacing: 25 m × 25 m × 20 m
- Wind: 10 m/s (log-law profile, z0 = 0.05 m)

**Physics:**
- Gaussian heat distribution (100 m spreads)
- Neutral stratification
- Temperature transport enabled

**Expected Results:**
- Thermal plume rises ~10-20 m (Briggs formula)
- Maximum temperature excess: 2-4 K at facility
- Plume disperses downwind

**Validation:**
- Compare computed plume rise to Briggs formula
- Check temperature excess against energy balance
- Verify plume advection with wind

### 2. Multiple Facilities

**File:** `multi_facility_inputs.i`

Three data centers with different heat releases to test superposition.

**Configuration:**
- Facility A: 10 MW at (1000 m, 1000 m)
- Facility B: 5 MW at (1500 m, 2000 m)
- Facility C: 8 MW at (2500 m, 1500 m)
- Domain: Same as flat terrain case

**Physics:**
- Multiple Gaussian sources
- Linear superposition principle
- Neutral atmosphere

**Expected Results:**
- Individual plumes visible
- Interaction effects where plumes merge
- Combined heating effects downstream

**Validation:**
- Check superposition property: T_total ≈ T_A + T_B + T_C in near field
- Verify facility-level diagnostics
- Confirm inter-facility distance effects

### 3. Valley Terrain

**File:** `valley_terrain_inputs.i`

Data center in a valley with complex terrain and stable stratification.

**Configuration:**
- Heat release: 10 MW
- Location: Valley bottom
- Domain: 3000 m × 3000 m × 500 m (taller domain for terrain)
- Grid spacing: 25 m × 25 m × 20 m
- Wind: 5 m/s (reduced by terrain blockage)
- Stratification: Stable (N² > 0)

**Physics:**
- Terrain-induced flow distortion
- Stable atmospheric layer
- Reduced plume rise
- Potential channeling effects

**Expected Results:**
- Plume confined to lower levels
- Reduced vertical rise (stable conditions)
- Potential recirculation in valley

**Validation:**
- Compare to stable atmosphere plume rise formula
- Check terrain-wind interaction
- Verify Brunt-Väisälä frequency effects

## Running Tests

### Prerequisites

- Compiled massconsistent_amr executable (wind_solver_app)
- Python environment for post-processing (numpy, matplotlib optional)
- Test data files in this directory

### Basic Execution

```bash
# Single test case
wind_solver_app regtest/datacenter/flat_terrain_inputs.i

# Generates output:
# - plt_datacenter_flat/     (AMReX plotfile)
# - wind_extract.csv         (extracted wind data)
```

### Post-Processing

```python
import sys
sys.path.insert(0, 'src/python')
from datacenter_heat_source import DataCenterPlume

# Load solver output
plume = DataCenterPlume.from_amrex_plotfile('plt_datacenter_flat')

# Compute metrics
metrics = plume.compute_plume_metrics('DataCenter')
print(f"Max temperature: {metrics.T_max:.2f} K")
print(f"Plume height: {metrics.plume_height:.1f} m")

# Generate visualizations
plume.plot_horizontal_slice(z=100.0)
plume.plot_vertical_slice(x=1500.0)
```

## Validation Metrics

### 1. Temperature Excess

**Formula:** ΔT = Q / (ρ * cp * V_eff)

Where:
- Q = heat release rate [W]
- ρ = air density [kg/m³]
- cp = specific heat [J/(kg·K)]
- V_eff = effective mixing volume [m³]

**For 10 MW flat case:**
- Q = 1.0e7 W
- ρ = 1.225 kg/m³
- cp = 1005 J/(kg·K)
- V_eff ≈ 25 × 25 × 50 = 31,250 m³ (at center)
- ΔT ≈ 2.6 K

### 2. Plume Rise (Briggs Formula)

**Formula:** Δh = 1.6 * F^(1/3) * x^(2/3) / u

Where:
- F = buoyancy parameter
- x = downwind distance [m]
- u = wind speed [m/s]

**For flat case at 500 m downwind:**
- u = 10 m/s
- ΔT ≈ 2.6 K
- F ≈ (g/T_ref) * ΔT * 10 ≈ 0.084
- Δh ≈ 1.6 * (0.084)^(1/3) * (500)^(2/3) / 10 ≈ 8-10 m

### 3. Energy Conservation

**Check:** ∫∫∫ (dT/dt) * ρ * cp dV = Q

Over the domain, the integrated rate of temperature increase times the heat capacity should equal the heat release rate.

## Input File Parameters

### Datacenter Configuration

```ini
# Enable data center modeling
datacenter.enabled = true

# Single facility format (legacy)
datacenter.heat_release = 1.0e7          # [W]
datacenter.x = 1500.0                    # [m]
datacenter.y = 1500.0                    # [m]
datacenter.z = 10.0                      # [m] height AGL
datacenter.area = 10000.0                # [m²]
datacenter.sigma_x = 100.0               # [m] Gaussian spread
datacenter.sigma_y = 100.0               # [m]
datacenter.sigma_z = 10.0                # [m]

# Multiple facilities format (array)
datacenter.heat_release = 1.0e7 5.0e6    # Multiple values
datacenter.x = 1000.0 1500.0
datacenter.y = 1000.0 2000.0
# ... etc for all parameters
```

### Required Scalar Transport Parameters

```ini
enable_3d_scalars = true                 # REQUIRED
enable_temperature_transport = true      # REQUIRED with datacenter
temperature_diffusivity = 2.5e-5         # [m²/s]
scalar_cfl = 0.8                         # CFL for transport
```

## Troubleshooting

### Issue: "datacenter heat source requires enable_temperature_transport = true"

**Solution:** Add `enable_temperature_transport = true` to input file when using datacenter.

### Issue: Plume not forming

**Possible causes:**
- `enable_3d_scalars = false` - enable 3D scalars
- Grid resolution too coarse - decrease dx, dy, dz
- Heat release too small - increase datacenter.heat_release
- Wind speed too high - heat is advected away too quickly

### Issue: Temperature oscillations

**Possible causes:**
- CFL number too large - reduce scalar_cfl < 0.8
- Time step too large - solver will reduce dt_transport automatically
- Diffusivity too small - increase temperature_diffusivity

## Future Test Cases

Planned additions:
- Urban heat island (multiple distributed sources)
- Time-varying heat release (diurnal cycle)
- Complex terrain with buildings
- High-resolution DNS-like case for validation
- Comparison with WRF/CFD results
- Recirculation scenarios

## References

- Briggs, G.A. (1975). Plume rise predictions
- Test case documentation in docs/DATACENTER_IMPLEMENTATION.md
- Parameter reference in docs/MULTI_FACILITY_DATACENTER.md
