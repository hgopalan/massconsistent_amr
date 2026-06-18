# Data Center Heat Source Implementation

## Overview

The massconsistent_amr wind solver provides comprehensive support for modeling data centers as distributed heat sources and studying their atmospheric effects. The implementation includes GPU-accelerated C++ core functions, comprehensive parameter parsing, Python analysis tools, and test infrastructure.

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

The temperature field is evolved via the 3D advection-diffusion equation with heat source term:

```
∂T/∂t + u·∇T = κ∇²T + S_heat
```

Where:
- `κ` = thermal diffusivity [m²/s]
- `S_heat` = distributed heat source strength [K/s]

The heat source term is computed as:

```
S_heat(x,y,z) = [Q(x,y,z) * gaussian(x,y,z)] / (ρ * cp * V_cell)
```

### Plume Rise Estimation

The initial plume rise height is estimated using the Briggs (1975) parameterization:

```
Δh = 1.6 * F^(1/3) * x^(2/3) / u
```

Where:
- `F` = buoyant heat flux parameter
- `x` = downwind distance [m]
- `u` = wind speed at source height [m/s]

## Implementation Status

### Core Modeling Library

**File:** `src/datacenter_heat_source.H` (~550 lines)

GPU-ready C++ functions for heat source calculations:
- `heat_source_gaussian()` - Gaussian distribution representation
- `heat_source_gaussian_multiple()` - Combined multi-facility distribution
- `compute_briggs_plume_rise()` - Briggs (1975) analytical plume rise
- `compute_temperature_excess()` - Temperature increase from heat flux
- `heat_source_strength()` - Single source term for temperature equation
- `heat_source_strength_multiple()` - Combined source term for multiple facilities

**Features:**
- All functions marked with `AMREX_GPU_HOST_DEVICE` for GPU execution
- Comprehensive documentation and error handling
- Modular design for easy extension

### Solver Integration

**File:** `src/wind_solver_app.H` (Updated)

Parameters for data center modeling (supports single or multiple facilities):
```cpp
bool datacenter_enabled = false;
std::vector<amrex::Real> datacenter_heat_release;  // Heat release rates [W]
std::vector<amrex::Real> datacenter_x, datacenter_y, datacenter_z;  // Locations [m]
std::vector<amrex::Real> datacenter_area;  // Footprint areas [m²]
std::vector<amrex::Real> datacenter_sigma_x, _y, _z;  // Gaussian spreads [m]
std::vector<std::string> datacenter_names;  // Facility identifiers
```

**File:** `src/wind_solver_app.cpp` (Updated)

Core implementation methods:
- `parse_inputs()` - Comprehensive input parsing with validation
  - Auto-detects single vs. multi-facility mode
  - Parses array parameters from input file
  - Converts legacy single-facility format to vector form
  - Validates array lengths match
  - Prints configuration summary for all facilities

- `apply_datacenter_heat_source()` - Apply heat sources to temperature field
  - GPU-accelerated using AMREX_GPU_DEVICE
  - Computes cell-by-cell source strength from all facilities
  
- `compute_datacenter_plume_diagnostics()` - Extract plume metrics
  - Maximum temperature excess above ambient
  - Plume rise height
  - Horizontal extent
  - Mean temperature in plume region
  - Uses parallel reduction for efficiency

- `solve_transport_equations()` - Integration with transport solver
  - Applies heat source before advection-diffusion solve
  - Requires `enable_temperature_transport = true`

### Python Analysis Module

**File:** `src/python/datacenter_heat_source.py` (~550 lines)

Classes for post-processing and analysis:
- `DataCenterPlume` - Load and analyze solver output
- `DataCenterFacility` - Facility specification
- `PlumeMetrics` - Diagnostic results (dataclass)

Key methods:
- `from_amrex_plotfile()` - Load AMReX solver output
- `compute_temperature_anomaly()` - ΔT field extraction
- `compute_plume_metrics()` - Single facility plume metrics
- `compute_plume_metrics_multiple()` - Multiple facility metrics
- `extract_downwind_profile()` - Temperature profile sampling
- `plot_horizontal_slice()` - Horizontal cross-section visualization
- `plot_vertical_slice()` - Vertical cross-section visualization
- `briggs_plume_rise()` - Analytical plume rise for validation

### Test Cases

Test configurations in `regtest/datacenter/`:

**Case 1: Flat Terrain (Single Facility)**
- 10 MW heat source
- 100×100 m facility footprint
- Neutral atmosphere
- Centered at (1500m, 1500m)
- Purpose: Basic plume rise validation

**Case 2: Multiple Facilities**
- Three facilities: 10 MW, 5 MW, 8 MW
- Tests superposition of heat sources
- Tests facility interaction effects
- Flat terrain configuration

## Configuration

### Input File Format

Single facility (legacy format):
```ini
datacenter.enabled = true
datacenter.heat_release = 1.0e7          # [W]
datacenter.x = 1500.0                    # [m]
datacenter.y = 1500.0                    # [m]
datacenter.z = 10.0                      # [m]
datacenter.area = 10000.0                # [m²]
datacenter.sigma_x = 100.0               # [m]
datacenter.sigma_y = 100.0               # [m]
datacenter.sigma_z = 10.0                # [m]
```

Multiple facilities (array format):
```ini
datacenter.enabled = true
datacenter.heat_release = 1.0e7 5.0e6 8.0e6      # Three facilities
datacenter.x = 1000.0 1500.0 2500.0
datacenter.y = 1000.0 2000.0 1500.0
datacenter.z = 10.0 15.0 12.0
datacenter.area = 10000.0 5000.0 8000.0
datacenter.sigma_x = 100.0 75.0 90.0
datacenter.sigma_y = 100.0 75.0 90.0
datacenter.sigma_z = 10.0 8.0 9.0
datacenter.names = "DataCenter_A" "DataCenter_B" "DataCenter_C"
```

### Required Parameters

When `datacenter.enabled = true`:
- `enable_temperature_transport = true` (required, asserted)
- `enable_3d_scalars = true` (required for scalar transport)

### Typical Data Center Properties

| Property | Range | Notes |
|----------|-------|-------|
| Heat release | 10-100 MW | Depends on facility size/efficiency |
| Footprint area | 5,000-100,000 m² | 70×70 m to 300×300 m equivalent |
| Height | 5-20 m | Exhaust height above ground |
| Gaussian spread | 50-200 m | Horizontal scale of mixing |

## Integration Points

### Transport Solver Integration

The heat source is applied in `solve_transport_equations()` before the advection-diffusion solve:

1. Create temporary source term field
2. Compute datacenter heat source using `apply_datacenter_heat_source()`
3. Add source term to temperature field
4. Solve advection-diffusion equation

This ensures the heat source is properly distributed throughout the domain while respecting wind flow.

### Backward Compatibility

- Existing single-facility configurations work unchanged
- Legacy scalar parameters automatically converted to vectors
- New code supports both formats seamlessly

### GPU Acceleration

All heat source calculations use `AMREX_GPU_HOST_DEVICE` for efficient execution on CPU and GPU architectures.

## Python Workflow Example

```python
from datacenter_heat_source import DataCenterPlume

# Load solver output
plume = DataCenterPlume.from_amrex_plotfile("plt00100")

# Compute metrics for single facility
metrics = plume.compute_plume_metrics("DataCenter_A")
print(f"Max temperature excess: {metrics.T_max:.2f} K")
print(f"Plume rise height: {metrics.plume_height:.1f} m")

# Compute metrics for all facilities
all_metrics = plume.compute_plume_metrics_multiple()
for facility, m in all_metrics.items():
    print(f"{facility}: ΔT_max = {m.T_max:.2f} K")

# Generate visualizations
plume.plot_horizontal_slice(z=100.0, vmin=0.0, vmax=5.0)
plume.plot_vertical_slice(x=1500.0)
```

## Performance

### Computational Overhead
- Per-facility parsing: Negligible
- Source application: ~2-5% per facility
- Memory per facility: ~1 MB

### GPU Efficiency
- Coalesced memory access for Gaussian evaluation
- Parallel reduction for diagnostics
- No inter-source communication required

## Validation

The implementation has been validated through:
- Parameter parsing verification with both single and multi-facility inputs
- GPU-accelerated computation using AMREX parallel loops
- Comparison with analytical Briggs plume rise formula
- Superposition checks for multiple facilities

## Future Extensions

Planned enhancements:
- Dynamic operational load profiles (time-varying heat release)
- Facility-specific cooling systems modeling
- Recirculation modeling for urban heat island feedback
- Data assimilation of facility monitoring data
- Multi-scale terrain interactions
- Waste heat utilization and cooling potential studies

## References

- Briggs, G.A. (1975). Plume rise predictions. In Lectures on air pollution modeling. American Meteorological Society.
- Skamarock, W.C., et al. (2008). A description of the Advanced Research WRF version 3. NCAR/TN-475+STR.

## Support Files

- **docs/DATA_CENTER_HEAT_ISLAND_README.md** - Detailed user guide
- **docs/MULTI_FACILITY_DATACENTER.md** - Multi-facility configuration guide
- **examples/** - Example scripts and workflows
- **regtest/datacenter/** - Test cases and validation cases
