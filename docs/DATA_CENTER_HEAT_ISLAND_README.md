# Data Center Heat Island Effects - Phase 1 Documentation

## Overview

This module provides functionality to model data centers as heat sources and study their atmospheric effects using the mass-consistent wind solver. Phase 1 focuses on basic heat source modeling and plume dispersion analysis.

## Physical Model

### Heat Source Representation

A data center is modeled as a distributed heat source using a Gaussian distribution:

```
Q(x,y,z) = Q_total * exp(-(dx²/2σx² + dy²/2σy² + dz²/2σz²))
```

Where:
- `Q_total` = total heat release rate [W]
- `(x,y,z)` = facility center location [m]
- `σx, σy, σz` = Gaussian spread parameters [m]

### Temperature Field Evolution

The temperature field is evolved via the 3D advection-diffusion equation:

```
∂T/∂t + u·∇T = κ∇²T + S_heat
```

Where:
- `κ` = thermal diffusivity [m²/s]
- `S_heat` = distributed heat source strength [K/s]

The heat source term is computed as:

```
S_heat(x,y,z,t) = [Q(x,y,z) * gaussian(x,y,z)] / (ρ * cp * V_cell)
```

### Plume Rise Estimation

The initial plume rise height is estimated using the Briggs (1975) parameterization:

```
Δh = 1.6 * F^(1/3) * x^(2/3) / u
```

Where:
- `F` = buoyant heat flux parameter [dimensionless]
- `x` = downwind distance [m]
- `u` = ambient wind speed [m/s]

## Configuration Parameters

### Input File Keywords

All data center parameters use the prefix `datacenter.`:

```ini
# Enable/disable data center module
datacenter.enabled = true

# Heat source specification [W]
datacenter.heat_release = 1.0e7

# Facility location [m]
datacenter.x = 1500.0
datacenter.y = 1500.0
datacenter.z = 10.0

# Facility footprint [m²]
datacenter.area = 10000.0

# Gaussian distribution spreads [m]
datacenter.sigma_x = 100.0
datacenter.sigma_y = 100.0
datacenter.sigma_z = 10.0
```

### Required Companion Parameters

To use the data center module, ensure these are enabled:

```ini
# Enable 3D scalar transport
enable_3d_scalars = true
enable_temperature_transport = true

# Set appropriate diffusivity
temperature_diffusivity = 2.5e-5  # [m²/s]
scalar_cfl = 0.8
```

## Physical Parameters

### Default Values

| Parameter | Default | Units | Notes |
|-----------|---------|-------|-------|
| `heat_release` | 0.0 | W | Set to desired heat output |
| `x, y` | 0.0 | m | Facility center in domain coordinates |
| `z` | 10.0 | m | Height above ground level |
| `area` | 1000.0 | m² | Facility footprint |
| `sigma_x, sigma_y` | 50.0 | m | Horizontal spread; ~2×√(area/π) typical |
| `sigma_z` | 5.0 | m | Vertical spread; small value for surface source |

### Typical Data Center Properties

| Property | Range | Notes |
|----------|-------|-------|
| Heat Release | 10-100 MW | ~10 kW per rack; 1000-10000 racks typical |
| Footprint | 5,000-100,000 m² | Small hyperscale centers; larger regional centers |
| Effective Radius | √(Area/π) | ~40-180 m for typical centers |
| Height | 10-30 m | Ground-level discharge; elevated sources in Phase 2 |

### Example Configurations

**Small Data Center (10 MW)**
```ini
datacenter.heat_release = 1.0e7
datacenter.area = 5000.0
datacenter.sigma_x = 63.0
datacenter.sigma_y = 63.0
datacenter.sigma_z = 8.0
```

**Hyperscale Facility (50 MW)**
```ini
datacenter.heat_release = 5.0e7
datacenter.area = 30000.0
datacenter.sigma_x = 155.0
datacenter.sigma_y = 155.0
datacenter.sigma_z = 10.0
```

## Output and Analysis

### Temperature Fields

The solver outputs 3D temperature fields in plotfiles:
- `temp` field: Absolute temperature [K]
- Thermal anomaly ΔT computed as: T - T_reference

### Diagnostics

Key metrics extracted from output:

1. **Plume Rise Height**: Maximum height with ΔT > 0.5 K
2. **Plume Extent**: Horizontal/vertical distance of thermal signature
3. **Temperature Excess**: Max and mean ΔT within plume region
4. **Downwind Profile**: Temperature decay with distance from facility

### Python Analysis Tools

Use `datacenter_heat_source.py` module for post-processing:

```python
from datacenter_heat_source import DataCenterPlume, DataCenterFacility

# Load solver output
plume = DataCenterPlume.from_amrex_plotfile("plt_datacenter_00050")

# Define facility
facility = DataCenterFacility(
    x=1500.0, y=1500.0, z=10.0,
    area=10000.0, heat_release=1.0e7,
    name="TestCenter"
)

# Compute metrics
metrics = plume.compute_plume_metrics(facility)
print(f"Plume rise: {metrics.plume_rise_height:.1f} m")
print(f"Max ΔT: {metrics.max_temperature_excess:.2f} K")

# Extract downwind profile
profile = plume.extract_downwind_profile(
    facility, 
    wind_direction=270.0,  # From west
    height_agl=50.0
)
print(profile.head(10))

# Visualize
plume.plot_horizontal_slice(100.0, facility, "plume_100m.png")
plume.plot_vertical_slice(y_coord=1500, facility, "plume_vertical.png")
```

## Test Cases

### Case 1: Flat Terrain (regtest/datacenter/flat_terrain_inputs.i)

**Purpose**: Validate basic plume rise in idealized conditions

**Configuration**:
- Flat terrain at z=100 m
- Neutral atmosphere (constant lapse rate)
- 10 m/s westerly wind
- 10 MW heat source

**Expected Results**:
- Plume rises ~150-200 m above source
- Thermal anomaly decays exponentially with distance
- Gaussian horizontal extent ~300-500 m at 1 km downwind
- Comparison with Briggs formula: <10% error

### Case 2: Valley Terrain (regtest/datacenter/valley_terrain_inputs.i)

**Purpose**: Study interaction with complex terrain

**Configuration**:
- Valley geometry: 350 m walls, 150 m floor
- Facility located on valley floor
- Stable atmosphere above valley
- 50 MW heat source (larger facility)

**Expected Results**:
- Plume confined by valley walls initially
- Thermal circulation effects visible
- Enhanced mixing on windward slopes
- Interaction with terrain-forced vertical motion

## Validation Strategy

### Briggs Formula Comparison

Compare simulated plume rise with analytical Briggs model:

```python
from datacenter_heat_source import briggs_plume_rise

# For 10 MW source at 10 m/s wind, 1 km downwind
dh_briggs = briggs_plume_rise(heat_flux=1.0e7, 
                               wind_speed=10.0,
                               downwind_distance=1000.0)
```

### Sensitivity Analysis

Test parameter variations:
1. Heat release rate: 1-100 MW
2. Wind speed: 2-20 m/s
3. Atmospheric stability: neutral, stable, unstable
4. Facility size: 5,000-50,000 m²

### Physical Constraints

Verify solution properties:
- ✓ Temperature excess ≥ 0 everywhere
- ✓ Maximum ΔT ≤ source energy / (ρ·cp·V_domain)
- ✓ Plume extent increases with downwind distance
- ✓ Wind-dependent plume asymmetry

## Limitations and Future Work

### Phase 1 Limitations

1. **Point source approximation**: Facility modeled at single height; no multi-level discharge
2. **Steady-state analysis**: Time-dependent operational loads not yet supported
3. **No recirculation**: Plume doesn't re-enter intake (studied in Phase 2)
4. **Surface-level release**: Elevated cooling towers not yet modeled
5. **No detailed facility geometry**: Complex building arrangements approximated as Gaussian

### Phase 2 Enhancements

- Air-cooled vs. water-cooled facility distinction
- Elevated cooling tower stacks with buoyant discharge
- Time-varying operational cycles (peak vs. idle loads)
- Facility intake recirculation modeling
- Real-time PUE sensitivity analysis

### Phase 3+ Features

- Multi-facility cluster interactions
- Regional cumulative heat island effect
- Integration with air quality modeling
- Measurement framework (satellite, UAV, station data)
- Facility siting optimization

## References

1. **Briggs, G.A.** (1975). Plume rise predictions. In Lectures on air pollution modeling. American Meteorological Society.

2. **Skamarock, W.C., et al.** (2008). A description of the Advanced Research WRF version 3. NCAR/TN-475+STR.

3. **Simpson, J.E.** (1994). Sea Breeze and Local Winds. Cambridge University Press.

4. **Building Research Establishment** (2002). The Building Environment Modeling Framework (BEM).

## Contacts and Support

For questions about data center heat island modeling, see:
- Main README.md
- Source code headers: `datacenter_heat_source.H`, `datacenter_heat_source.py`
- Test case documentation: `regtest/datacenter/`

## License

This module is part of the massconsistent_amr project and follows the same license terms.
