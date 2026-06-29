# Wildfire Coupling with massconsistent_amr

## Overview

This guide describes the comprehensive Python interface for coupling the massconsistent_amr wind solver with wildfire_levelset fire solver. The interface supports:

1. **One-way coupling**: Wind field drives fire spread (fire does not affect wind)
2. **Two-way coupling**: Fire heating creates updrafts that feed back to wind solver
3. **Multiple ROS models**: Rothermel (1972), Richards (1990), hybrid, or custom level-set methods
4. **Advanced diagnostics**: Sensitivity analysis, statistics, and visualization

## Architecture

### Core Components

```
massconsistent_amr/
├── src/python/
│   ├── wind_solver.py                    # Wind solver wrapper
│   ├── wildfire_solver_interface.py       # Abstract base class for fire solver
│   ├── rothermel_ros.py                  # Rothermel ROS calculation (Rothermel 1972)
│   ├── richards_ros.py                   # Richards ROS calculation (Richards 1990)
│   ├── levelset_coupling.py               # Basic coupling (existing)
│   ├── levelset_coupling_enhanced.py      # Enhanced coupling with diagnostics
│   └── example_rothermel_richards_coupling.py  # Complete examples
├── regtest/
│   └── fire_coupling/
│       ├── one_way/                      # One-way coupling test
│       └── two_way/                      # Two-way coupling test
└── docs/
    └── WILDFIRE_COUPLING_GUIDE.md        # This file
```

### Interface Hierarchy

```
WildfireSolver (abstract base)
  ├── Domain properties
  │   ├── nx, ny, dx, dy
  │   ├── xmin, xmax, ymin, ymax
  │   └── get_domain_info()
  ├── Wind-fire coupling
  │   ├── update_wind_3d(u, v, w, nz, zmin, zmax)  [REQUIRED]
  │   └── get_surface_fluxes()                       [REQUIRED for 2-way]
  ├── ROS calculations
  │   ├── compute_rothermel_ros(...)
  │   ├── compute_richards_ros(...)
  │   └── compute_hybrid_ros(...)
  ├── State management
  │   ├── get_state()              [REQUIRED]
  │   ├── get_fuel_data()
  │   └── get_ros_components()
  ├── Time integration
  │   ├── step(dt)                 [REQUIRED]
  │   └── advance_to_time(t_target)
  ├── Fuel/environment I/O
  │   ├── set_fuel_model_map()
  │   ├── set_fuel_moisture_field()
  │   ├── set_slope_field()
  │   ├── set_aspect_field()
  │   └── set_elevation_field()
  ├── Ignition & initial conditions
  │   ├── set_ignition_point(x, y, time)
  │   ├── set_ignition_polygon(vertices, time)
  │   └── set_initial_fire_state(phi, time)
  ├── Model configuration
  │   ├── configure_rothermel(model_number, preference)
  │   ├── configure_richards(coefficients)
  │   └── set_ros_calculation_method(method)
  ├── Domain handling
  │   ├── set_domain_bounds()
  │   ├── set_periodic_boundaries()
  │   └── get_domain_info()
  ├── Output
  │   ├── write_plotfile(filename)
  │   ├── export_csv(filename, fields)
  │   └── export_geotiff(filename, field, georeference)
  ├── Diagnostics
  │   ├── compute_fire_perimeter()
  │   ├── compute_burned_area_fraction()
  │   ├── compute_fire_statistics()
  │   └── compute_ros_sensitivity(parameter, delta)
  └── Cleanup
      └── finalize()
```

## Rothermel (1972) Model

### Reference
Rothermel, R. C. (1972). "A mathematical model for predicting fire spread in wildland fuels."
USDA Forest Service Research Paper INT-115.

### Features
- **13 standard NFDRS fuel models**: Supports all standard fuel types
- **Physics-based parameters**: Fuel load, moisture of extinction, heat content, surface area-to-volume ratio
- **Moisture damping**: Exponential response to fuel moisture
- **Slope enhancement**: Accounts for terrain slope effects
- **Wind enhancement**: Directional wind effects on fire spread
- **Outputs**: Base ROS, slope-enhanced ROS, wind-enhanced ROS, flame length, intensity

### Fuel Models (NFDRS Standard 1-13)
1. Short grass (cured)
2. Timber-grass-shrub
3. Tall grass (cured)
4. Chaparral
5. Timber litter
6. Conifer plantation litter
7. Ponderosa pine/mixed conifer litter
8. Closed timber litter
9. Hardwood litter
10. Timber-shrub (black spruce-lichen)
11. Timber-shrub (light conifer-lichen)
12. Closed shelterwood
13. Palm-grass-shrub

### Usage Example
```python
from rothermel_ros import compute_rothermel_ros
import numpy as np

# Domain
ny, nx = 32, 32

# Fuel and environment
fuel_model = 1  # Short grass
moisture = 10.0 * np.ones((ny, nx))  # 10% moisture
slope = 15.0 * np.ones((ny, nx))     # 15° average slope
wind_speed = 5.0 * np.ones((ny, nx)) # 5 m/s wind
wind_direction = 180.0 * np.ones((ny, nx))  # From north

# Compute ROS
result = compute_rothermel_ros(
    fuel_model, moisture, slope, wind_speed, wind_direction
)

# Extract results
ros_base = result['ros_no_wind_slope']  # Base ROS (m/min)
ros_final = result['ros_with_wind']     # Final ROS (m/min)
intensity = result['fireline_intensity']  # kW/m
flame_length = result['flame_length']    # meters
```

### Typical ROS Values
- Grass fires: 0.5-5 m/min depending on fuel and conditions
- Shrub fires: 1-10 m/min
- Forest fires: 0.1-3 m/min
- Wind-driven fires: 5-30+ m/min
- High-elevation fires: 0.05-1 m/min due to thin fuels

## Richards (1990) Model

### Reference
Richards, G. D. (1990). "An elliptical growth model of forest fire fronts and its applications
to fire management." International Journal of Wildland Fire, 1(2):91-101.

### Features
- **Explicit ROS components**: Separate u and v components
- **Energy-balance basis**: Connects to thermodynamics
- **Fuel-independent**: Works with any fuel load/moisture data
- **Elliptical front geometry**: Supports anisotropic fire spread
- **Flexible parameterization**: Easy to customize coefficients

### Usage Example
```python
from richards_ros import compute_richards_ros
import numpy as np

# Domain
ny, nx = 32, 32

# Fuel and environment
fuel_load = 50.0 * np.ones((ny, nx))    # kg/m²
fuel_moisture = 10.0 * np.ones((ny, nx)) # %
wind_speed = 5.0 * np.ones((ny, nx))    # m/s
slope = 15.0 * np.ones((ny, nx))        # degrees

# Compute ROS
result = compute_richards_ros(
    fuel_load, fuel_moisture, wind_speed, slope,
    ros_0=0.1,                    # Base ROS coefficient
    wind_factor=2.0,              # Wind sensitivity
    slope_factor=1.5,             # Slope sensitivity
    moisture_response='exponential'  # Moisture damping model
)

# Extract results
ros_total = result['ros']  # Total ROS (m/min)
u_component = result['ros_components']['u_component']  # x-component
v_component = result['ros_components']['v_component']  # y-component
energy = result['energy_release']  # kJ/m²
```

## One-Way Coupling Example

Wind field is computed independently and provided to fire solver.

```python
from wind_solver import WindSolver
from levelset_coupling import CoupledWindFireSimulation

# Create coupled solver in one-way mode
coupled = CoupledWindFireSimulation(
    wind_inputs="wind_inputs.i",
    fire_inputs="fire_inputs.i",
    coupling_mode='one_way'
)

# Run simulation for 5 timesteps
result = coupled.run(num_steps=5)

# Get final state
status = coupled.get_status()
print(f"Fire time: {status['fire_time']:.1f}s")
print(f"Burned area: {status.get('burned_fraction', 0)*100:.1f}%")

# Finalize
coupled.finalize()
```

### Workflow
1. Wind solver initializes with terrain and reference wind
2. Wind solver solves for mass-consistent 3D velocity field
3. 3D velocity extracted and passed to fire solver via `update_wind_3d()`
4. Fire solver computes ROS using provided wind field
5. Fire front advances based on computed ROS
6. Repeat for next timestep (no feedback to wind)

## Two-Way Coupling Example

Fire heating creates updrafts that feed back to wind solver.

```python
from wind_solver import WindSolver
from levelset_coupling import CoupledWindFireSimulation

# Create coupled solver in two-way mode
coupled = CoupledWindFireSimulation(
    wind_inputs="wind_inputs.i",
    fire_inputs="fire_inputs.i",
    coupling_mode='two_way'
)

# Run simulation with feedback callback
def track_coupling(step, result):
    if result.get('heat_source_added', False):
        print(f"Step {step}: Heat feedback added to wind solver")

result = coupled.run(
    num_steps=50,
    wind_update_interval=1,  # Update wind every step
    callback=track_coupling
)

coupled.finalize()
```

### Workflow
1. Wind solver initializes with terrain and reference wind
2. **First iteration**:
   - Wind solver solves for 3D velocity (no fire heating yet)
   - 3D velocity passed to fire solver
   - Fire solver computes ROS and flame properties
   - Heat flux extracted via `get_surface_fluxes()`
   - Heat source added to wind solver via `wind.add_heat_source()`
3. **Subsequent iterations**:
   - Wind solver includes fire heating in momentum/energy equations
   - Fire-induced updrafts develop (buoyancy forcing)
   - New wind field with fire effects passed to fire solver
   - ROS adjusted based on new wind (feedback loop)
   - Heat flux updated and fed back to wind
4. Repeat until final time

### Heat Flux Interface

Fire solver extracts surface fluxes:
```python
heat_data = fire.get_surface_fluxes()
# Returns: {
#     'heat_flux': ndarray (ny, nx) kW/m²,
#     'sensible_heat': ndarray (ny, nx),
#     'latent_heat': ndarray (ny, nx),
#     'flame_height': ndarray (ny, nx),
#     'fireline_intensity': ndarray (ny, nx),
#     'surface_temp': ndarray (ny, nx),
#     'smoke_emission': ndarray (ny, nx),
# }
```

Wind solver adds heat source:
```python
wind.add_heat_source(
    heat_data['heat_flux'],
    grid_info={
        'xmin': wind.xmin,
        'xmax': wind.xmax,
        'ymin': wind.ymin,
        'ymax': wind.ymax,
        'dx': wind.dx,
        'dy': wind.dy,
        'scaling_factor': 1.0,  # Convert units if needed
        'temporal_decay': 0.95,  # Optional time decay
    }
)
```

## Input File Format

### Wind Solver (wind_inputs.i)
```ini
# Domain
amr.max_level = 0
amr.n_cell = 32 32 16
geometry.prob_lo = 0.0 0.0 0.0
geometry.prob_hi = 1000.0 1000.0 400.0

# Reference wind
wind.U_ref = 10.0
wind.V_ref = 0.0
wind.z_ref = 10.0

# Solver settings
wind.alpha_h = 1.0
wind.alpha_v = 1.0
wind.tol_rel = 1.e-8
```

### Fire Solver (fire_inputs.i)
```ini
# Domain (must match wind solver horizontal domain)
n_cell_x = 32
n_cell_y = 32
plo_x = 0.0
plo_y = 0.0
phi_x = 1000.0
phi_y = 1000.0

# Spread Model
spread_model = rothermel              # or "richards", "hybrid", "levelset"
rothermel.fuel_model = 1              # 1-13 standard fuel models
rothermel.max_ros_direction = maximum_spread

# Fuel Data (NEW)
fuel.map_file = fuel_model_map.csv    # Spatial fuel model distribution
fuel.dead_moisture = 5.0              # Dead fuel moisture (%)
fuel.live_moisture = 100.0            # Live fuel moisture (%)

# Terrain (NEW)
terrain.slope_file = slope.csv        # Slope (degrees)
terrain.aspect_file = aspect.csv      # Aspect (degrees)
terrain.elevation_file = elevation.csv # Elevation (m)

# Ignition
ignition.type = circle
ignition.x0 = 250.0
ignition.y0 = 250.0
ignition.radius = 50.0
ignition.time = 0.0

# Time Control
cfl = 0.5
nsteps = 100
max_time = 3600.0

# Output
plot_interval = 100
write_plotfile = 1
```

## Data Files

### Fuel Model Map (fuel_model_map.csv)
```csv
# Space-separated or CSV format
# nx ny columns/rows
32 32
1 1 3 3 1 1 ...  # Row 1
1 1 3 3 1 1 ...  # Row 2
...
```

### Slope (slope.csv)
```csv
# Terrain slope in degrees
# nx ny columns/rows
32 32
0.0 1.5 2.5 3.0 ...
...
```

### Aspect (aspect.csv)
```csv
# Terrain aspect (degrees from N)
# 0° = North, 90° = East, 180° = South, 270° = West
32 32
180.0 175.0 170.0 ...
...
```

## Diagnostic Methods

### Fire Statistics
```python
# Get comprehensive fire statistics
stats = coupled.compute_fire_statistics(fire_state)
# Returns: {
#     'max_ros': float,  # m/min
#     'mean_ros': float,
#     'std_ros': float,
#     'ros_percentiles': {'10th', '25th', '50th', '75th', '90th'},
#     'perimeter_length': float,  # meters
#     'burned_area': float,  # m²
#     'max_intensity': float,  # kW/m
#     'mean_intensity': float,
# }
```

### ROS Sensitivity Analysis
```python
# Compute sensitivity to moisture changes
sensitivity = coupled.compute_ros_sensitivity(
    parameter='moisture',
    delta=0.1  # ±10%
)
# Returns: {
#     'low': ros_field_at_-10%,
#     'base': ros_field_at_nominal,
#     'high': ros_field_at_+10%,
#     'sensitivity': (high - low) / (2 * delta),
#     'parameter': 'moisture',
# }
```

### Perimeter and Burned Area
```python
# Get active fire perimeter
perimeter = fire.compute_fire_perimeter()  # meters

# Get burned fraction
burned_frac = fire.compute_burned_area_fraction()  # [0, 1]

# Detailed state information
state = fire.get_state()
# {
#     'phi': level_set_field,
#     'ros': ros_field,
#     'intensity': intensity_field,
#     'fuel_consumed': consumed_fraction,
#     'burned_area_fraction': float,
#     'time': float,
#     'step': int,
#     'active_perimeter': float,
# }
```

## Output and Visualization

### AMReX Plotfiles
```python
# Write AMReX-format plotfile for VisIt/ParaView
wind.write_plotfile("plt_wind_00100")
fire.write_plotfile("plt_fire_00100")
```

### CSV Export
```python
# Export fire state to CSV
fire.export_csv(
    "fire_state.csv",
    fields=['phi', 'ros', 'intensity', 'fuel_consumed']
)
```

### GeoTIFF Export
```python
# Export with georeferencing
fire.export_geotiff(
    "ros_distribution.tif",
    field="ros",
    georeference={
        'crs': 'EPSG:4326',
        'origin_x': wind.xmin,
        'origin_y': wind.ymin,
        'pixel_size': wind.dx,
    }
)
```

## Testing and Validation

### Regression Tests
```bash
cd regtest/fire_coupling

# One-way coupling test
cd one_way
python3 test.py

# Two-way coupling test
cd two_way
python3 test.py
```

### Example Scripts
```bash
cd src/python

# Run Rothermel/Richards examples
python3 example_rothermel_richards_coupling.py

# Run coupled simulation example
python3 coupled_wind_fire_example.py
```

## Implementation Requirements for wildfire_levelset

To implement the WildfireSolver interface in wildfire_levelset:

1. **Inherit from WildfireSolver** abstract base class
2. **Implement required methods**:
   - `__init__(inputs_file, model_type)`
   - `update_wind_3d(u, v, w, nz, zmin, zmax)`
   - `get_surface_fluxes()`
   - `step(dt)`
   - `get_state()`
   - `finalize()`

3. **Integrate ROS models**:
   - Use `rothermel_ros.compute_rothermel_ros()` for Rothermel calculations
   - Use `richards_ros.compute_richards_ros()` for Richards calculations
   - Or implement your own level-set based propagation

4. **Support fuel data I/O**:
   - Read/write fuel model maps, moisture, slope, aspect
   - Parse input file format with new parameters
   - Validate grid alignment with wind solver

5. **Provide heat flux extraction**:
   - Compute intensity from ROS and fuel properties
   - Estimate flame height and surface temperature
   - Return as dictionary via `get_surface_fluxes()`

6. **Build with Python bindings**:
   ```bash
   cmake -S . -B build -DLEVELSET_BUILD_PYTHON_BINDINGS=ON
   cd build && make -j4
   ```

7. **Expose as importable module**:
   ```python
   # In Python
   from wildfire_solver import WildfireSolver  # Your implementation
   fire = WildfireSolver("fire_inputs.i", model_type="rothermel")
   ```

## References

### Primary References
- Rothermel, R. C. (1972). "A mathematical model for predicting fire spread in wildland fuels." USDA Forest Service Research Paper INT-115.
- Richards, G. D. (1990). "An elliptical growth model of forest fire fronts and its applications to fire management." International Journal of Wildland Fire, 1(2):91-101.
- Scott, J. H., & Reinhardt, E. D. (2001). "Assessing crown fire potential by linking models of surface and crown fire behavior." USDA Forest Service General Technical Report RMRS-GTR-87.

### Related Work
- Alexander, M. E., & Cruz, M. G. (2012). "Interdependencies between flame length and fireline intensity in model predictions of crown fire passive Crown fire initiation." International Journal of Wildland Fire, 21(2):95-113.
- Bastankhah, M., & Porté-Agel, F. (2016). "A new analytical model for wind farm power output." Renewable Energy, 70:116-123.

## Troubleshooting

### Domain Compatibility Issues
- Ensure wind and fire domains have matching horizontal extent
- Check that grid spacings (dx, dy) are consistent
- Verify fuel data arrays have correct shapes

### ROS Calculation Issues
- Check fuel moisture values (typically 0-100%)
- Ensure slope values are in degrees (0-90)
- Verify wind speeds are realistic (m/s)
- Check for NaN or Inf values in inputs

### Coupling Errors
- Verify wind solver and fire solver are initialized successfully
- Check that 3D wind field shapes match (nz, ny, nx)
- Ensure wind is updated before fire step
- Validate heat source grid info matches wind domain

### Performance Issues
- Reduce grid resolution for testing
- Use larger timesteps (higher CFL number)
- Decrease ROS model complexity (use richardsover Rothermel if computationally expensive)
- Profile to identify bottlenecks

## Contact and Support

For questions about the coupling interface:
- See CLAUDE.md for agent-specific instructions
- Check README.md for project overview
- Review GETTING_STARTED_TUTORIAL.md for setup
- Examine regtest/fire_coupling/ for working examples

