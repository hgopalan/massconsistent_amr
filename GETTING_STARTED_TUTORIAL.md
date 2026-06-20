# Getting Started with the Mass-Consistent Wind Solver

This tutorial provides a comprehensive guide for beginners and advanced users to configure and run the mass-consistent wind solver (`wind_solver`) and the puff dispersion model (`puff_solver`).

The mass-consistent wind solver takes an initial wind profile or observation data, interpolates it onto a 3D grid with complex terrain or building obstacles, and solves a Poisson equation to enforce mass conservation (continuity, $\nabla \cdot \vec{u} = 0$).

---

## 1. Wind Profile Types (`init_mode`)

The mass-consistent wind solver initializes the flow field using one of several mathematical formulations or experimental data sources defined by the `init_mode` parameter. Below are samples and explanations for each profile type.

### a. Log-Law (`loglaw`)
Initializes the horizontal wind components using the logarithmic wind profile (van Kármán law), ideal for neutral atmospheric boundary layers over flat or moderately complex terrain.
```ini
# inputs.i
init_mode = loglaw
U_ref     = 10.0      # Reference wind speed [m/s]
V_ref     = 0.0       # Reference cross-wind [m/s]
z_ref     = 10.0      # Height above terrain where U_ref/V_ref are specified [m]
z0        = 0.1       # Aerodynamic roughness length [m]
```

### b. Uniform (`uniform`)
Initializes a completely uniform wind field across the entire domain. Used for testing or highly simplified idealized scenarios.
```ini
# inputs.i
init_mode = uniform
uniform_U = 8.0       # Constant U-velocity [m/s]
uniform_V = 2.0       # Constant V-velocity [m/s]
```

### c. Power-Law (`powerlaw`)
Initializes the velocity using a standard power-law profile, widely used in wind resource assessment and engineering wind loading.
```ini
# inputs.i
init_mode = powerlaw
U_ref             = 12.0     # Reference wind speed [m/s]
V_ref             = 0.0      # Reference cross-wind [m/s]
z_ref             = 80.0     # Hub or reference height [m]
powerlaw_exponent = 0.143    # Power-law exponent (e.g., 1/7th law)
```

### d. Atmospheric Sounding (`sounding`)
Initializes the vertical structure using standard sounding data (e.g., FSL or UP.DAT formats) representing vertical profiles of wind speed, direction, temperature, and moisture.
```ini
# inputs.i
init_mode                 = sounding
sounding_file             = sounding_data.txt
sounding_vertical_interp  = spline    # Interpolation scheme: 'spline' or 'log_linear'
sounding_wind_in_knots    = false     # Set true if sounding speed is in knots
```

### e. Remote Automated Weather Stations (`raws`)
Initializes a 3D wind field by interpolating sparse weather station wind speed and direction observations using Inverse Distance Weighting (IDW).
```ini
# inputs.i
init_mode    = raws
velocity_file = raws_stations.csv  # CSV with Station X, Y, Z, Speed, Direction
idw_exponent = 2.0                 # Distance decay exponent for IDW
```

### f. Grid-Based Surface Data (`surface_data`)
Initializes wind fields based on 2D grids of surface metrics (e.g., $u_*$, $z_0$, 10m wind speed/direction) extracted from regional meteorological models like HRRR or ERA5.
```ini
# inputs.i
init_mode          = surface_data
surface_data_file  = surface_metrics.csv
z_ref              = 10.0
```

### g. Mapped 3D Wind Field (`windfield`)
Directly maps a pre-interpolated or simulated 3D wind velocity component dataset from an external model onto the solver's computational grid.
```ini
# inputs.i
init_mode      = windfield
windfield_file = external_3d_wind.csv  # Mapping CSV with X, Y, Z, U, V, W components
```

### h. Single Column Model (`scm`)
Initializes the 1D vertical column based on interactive boundary-layer physics (e.g., YSU, MYJ models) and microphysics, which are then extruded horizontally.
```ini
# inputs.i
init_mode               = scm
scm_forcing_type        = geostrophic  # Forcing method
scm_geostrophic_U       = 15.0         # Geostrophic wind speed [m/s]
scm_enable_microphysics = true         # Include moisture and phase changes
```

### i. Ekman Spiral (`ekman_spiral`)
Initializes a profile showing wind direction veer (rotation) with altitude caused by the balance between pressure gradient, Coriolis force, and turbulent friction.
```ini
# inputs.i
init_mode         = ekman_spiral
U_ref             = 10.0   # Reference wind [m/s]
z_ref             = 10.0   # Reference height [m]
latitude          = 45.0   # Coriolis latitude scaling [degrees]
ekman_veer_total  = 30.0   # Total veer angle [degrees]
ekman_veer_height = 600.0  # Height boundary [m]
```

### j. Deaves-Harris (`deaves_harris`)
An advanced boundary-layer profile formulation valid under strong-wind, neutral conditions up to the top of the atmospheric boundary layer.
```ini
# inputs.i
init_mode     = deaves_harris
U_ref         = 15.0       # Reference wind speed [m/s]
z_ref         = 10.0       # Reference height [m]
z0            = 0.05       # Aerodynamic roughness [m]
```

### k. Power-Law Above Boundary Layer (`powerlaw_above_bl`)
A hybrid profile that applies a power-law variation within the boundary layer and maintains a constant velocity above the boundary layer height.
```ini
# inputs.i
init_mode              = powerlaw_above_bl
U_ref                  = 12.0
z_ref                  = 80.0
powerlaw_exponent      = 0.15
transition_layer_height= 120.0  # Boundary layer height [m]
```

---

## 2. Advanced Options and Applicability

These features enhance the solver's capability to simulate real-world physical mechanisms.

1. **Atmospheric Stability Corrections** (`enable_stability_correction`): Modifies the vertical log-law velocity shear based on Monin-Obukhov Similarity Theory.
   - *Applicable to*: `loglaw`, `powerlaw` (when utilizing boundary layer height scaling).

2. **Forest Canopy Models** (`use_canopy`): Parameterizes drag forces and attenuation of wind velocities within vegetated layers using canopy heights and Leaf Area Index (LAI) profiles.
   - *Applicable to*: All profile types.

3. **Wall Functions** (`enable_wall_functions`): Models turbulent boundary layer shear stress on flat surfaces, resolved buildings, and complex terrain meshes rather than requiring highly refined boundary cells.
   - *Applicable to*: All profile types.

4. **Buoyancy & Thermal Stratification** (`enable_buoyancy_stratification`): Couples temperature gradients with the momentum equations using buoyancy forces.
   - *Applicable to*: `loglaw`, `sounding`, `scm` (thermal files are read to define temperature fields).

5. **Kinematic Terrain-Following Boundary Condition** (`enable_terrain_kinematic_bc`): Enforces terrain-following boundary conditions on the resolved grid levels.
   - *Applicable to*: All profile types on non-flat terrain.

6. **SCM Boundary Layer Parameterizations** (`scm_forcing_type`): Uses predictive 1D planetary boundary layer physics (e.g., YSU, MYJ closures) to build steady-state profiles.
   - *Applicable to*: `scm`.

7. **Spatial Lagrange Anisotropy Weighting** (`use_spatial_alpha_coefficients`): Adjusts the horizontal-to-vertical mass adjustment ratio ($\alpha_h/\alpha_v$) locally based on land use or complex topography.
   - *Applicable to*: All profile types.

8. **Coriolis Ekman Veer** (`enable_ekman_veer`): Introduces directional rotation with altitude into the vertical profiles.
   - *Applicable to*: `loglaw`, `powerlaw`, `sounding`.

9. **Subgrid Windbreak Obstacles** (`enable_windbreaks`): Represents subgrid thin barriers (e.g., windbreak fences) using localized drag formulations.
   - *Applicable to*: All profile types.

10. **Building Wakes & Cavity Vortex Models** (`enable_building_wake`): Calculates wind speed deficits and turbulence enhancements in regions behind buildings (e.g., Röckle formulation).
    - *Applicable to*: All profile types.

---

## 3. Compatibility Matrix

The following matrix maps the compatibility of advanced options with each wind initialization mode:

| Advanced Option | `loglaw` | `uniform` | `powerlaw` | `sounding` | `raws` | `surface_data` | `windfield` | `scm` | `ekman_spiral` | `deaves_harris` | `powerlaw_above_bl` |
|---|---|---|---|---|---|---|---|---|---|---|---|
| **Atmospheric Stability** | **Yes** | No | **Yes** | No | No | No | No | No | No | No | No |
| **Forest Canopy Model** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** |
| **Wall Functions** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** |
| **Buoyancy / Thermal** | **Yes** | No | No | **Yes** | No | No | No | **Yes** | No | No | No |
| **Kinematic BC** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** |
| **SCM Physics** | No | No | No | No | No | No | No | **Yes** | No | No | No |
| **Spatial Anisotropy ($\alpha$)** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** |
| **Coriolis Ekman Veer** | **Yes** | No | **Yes** | **Yes** | No | No | No | **Yes** | **Yes** | No | No |
| **Windbreaks** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** |
| **Building Wakes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** | **Yes** |

---

## 4. Dispersion Modeling (`puff_solver`)

The `puff_solver` models pollutant and particulate dispersion over complex domains. It supports two primary execution modes: **Coupled** and **Uncoupled**.

### A. Coupled Mode (`coupled_mode = true`)
In coupled mode, the dispersion solver imports full 3D wind velocity fields directly from the output plotfiles of the mass-consistent wind solver. This is the recommended mode for high-fidelity simulations over complex terrain.

- **Steady Coupled**: A single wind field (representing steady conditions) is used for all dispersion time steps.
- **Unsteady Coupled**: Consecutive, time-varying wind fields (representing transient meteorological states) are loaded dynamically per time step.

#### Sample Input for Coupled Dispersion
```ini
# inputs_dispersion.i
coupled_mode            = true
unsteady_wind           = true                # Enable reading of time-varying winds
wind_plotfile_prefix    = plt_wind_step       # Prefix of plotfiles to load (e.g. plt_wind_step0, plt_wind_step1)
n_steps_puff            = 100                 # Number of dispersion steps
dt_puff                 = 5.0                 # Dispersion step size [s]
dispersion_scheme       = turbulence          # Schemes: "constant", "pasquill_gifford", "turbulence"
```

### B. Uncoupled Mode (`coupled_mode = false`)
In uncoupled mode, the dispersion solver runs independently of the wind solver's output files. It assumes a homogeneous, uniform, and constant analytical wind field across the entire domain using parameters specified within the dispersion input file.

This mode is computationally lightweight and ideal for rapid screening, flat-terrain verification, or idealized analytical tests.

#### Sample Input for Uncoupled Dispersion
```ini
# inputs_dispersion.i
coupled_mode      = false
U_wind            = 8.5      # Homogeneous wind component in X [m/s]
V_wind            = -1.5     # Homogeneous wind component in Y [m/s]
W_wind            = 0.0      # Homogeneous wind component in Z [m/s]
n_steps_puff      = 50
dt_puff           = 10.0
dispersion_scheme = pasquill_gifford
is_urban          = false
```
