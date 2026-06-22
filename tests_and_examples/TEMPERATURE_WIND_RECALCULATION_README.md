# Temperature-Wind Recalculation Examples

This directory contains example input files demonstrating the temperature-wind recalculation feature.

## Files

### 1. `temperature_wind_recalculation_basic.inp`

**Use Case:** Basic setup demonstrating the simplest use of temperature-wind coupling

**Features:**
- Simple log-law wind initialization
- Constant temperature profile with buoyancy
- Standard 2 iterations with 0.01 m/s convergence tolerance
- 100 time steps for testing

**Typical Runtime:** ~1-2 minutes on modern CPU

**Best For:**
- Learning the feature
- Sensitivity studies
- Regression testing

**Expected Results:**
- Vertical velocity shows response to buoyancy forces
- Convergence achieved typically within 1-2 iterations
- Temperature field remains stable

---

### 2. `temperature_wind_recalculation_datacenter.inp`

**Use Case:** Strong coupling with datacenter heat sources (urban heat island)

**Features:**
- High-resolution urban domain (5m grid spacing)
- Three datacenter heat sources (5-10 MW each)
- Building interactions
- Aggressive recalculation (3 iterations, 0.001 m/s tolerance)
- Multiple extraction heights

**Typical Runtime:** ~5-10 minutes on modern CPU

**Best For:**
- Urban microclimate studies
- Heat island effect modeling
- Thermal plume analysis

**Expected Results:**
- Strong vertical velocities above heat sources
- Well-developed thermal plume
- Wind deflection around heat islands
- Rapid convergence despite intense heating

**Key Settings:**
```
temperature_wind_recalc_iterations = 3        # More iterations for strong effects
temperature_wind_recalc_tolerance = 0.001     # Tighter convergence
max_grid_size = 16                            # Finer grid management
```

---

### 3. `temperature_wind_recalculation_diurnal.inp`

**Use Case:** Diurnal heating cycle with temperature-wind feedback

**Features:**
- Diurnal temperature variation (±12 K)
- 24-hour simulation (1440 timesteps)
- Coupled wind response to time-varying heating
- Stability corrections included
- Boundary layer decay

**Typical Runtime:** ~10-20 minutes on modern CPU

**Best For:**
- Diurnal circulation studies
- Sea-breeze / thermal circulation
- Daily heating/cooling cycles

**Expected Results:**
- Morning: weak upward motion with weak heating
- Midday: strong upward motion, thermal circulation strengthens
- Evening: transition to stable stratification
- Night: weak motion, inversion formation

**Key Settings:**
```
enable_diurnal_temperature = true
diurnal_temperature_amplitude = 12.0
diurnal_phase_hour = 14.0
enable_stability_correction = true
```

---

## How to Run

### Run Example 1 (Basic)
```bash
cd massconsistent_amr
./wind_solver_app temperature_wind_recalculation_basic.inp
```

### Run Example 2 (Datacenter - requires building data)
```bash
# First, prepare building data:
# buildings.csv should contain: x_min, x_max, y_min, y_max, z_min, z_max

# Then run:
./wind_solver_app temperature_wind_recalculation_datacenter.inp
```

### Run Example 3 (Diurnal)
```bash
./wind_solver_app temperature_wind_recalculation_diurnal.inp
```

## Parameter Customization

### Adjust Accuracy vs. Speed

**For faster execution:**
```ini
temperature_wind_recalc_iterations = 1        # Skip 2nd iteration
temperature_wind_recalc_tolerance = 0.02      # Looser convergence
scalar_cfl = 1.0                              # Larger time steps
```

**For better accuracy:**
```ini
temperature_wind_recalc_iterations = 3        # 3 iterations
temperature_wind_recalc_tolerance = 0.001     # Tight convergence
scalar_cfl = 0.5                              # Smaller time steps
```

### Change Grid Resolution

```ini
# Coarser (faster)
wind_solver.dx = 20.0
wind_solver.dy = 20.0
wind_solver.dz = 10.0

# Finer (slower, more accurate)
wind_solver.dx = 5.0
wind_solver.dy = 5.0
wind_solver.dz = 2.0
```

### Adjust Temperature Dynamics

```ini
# Faster diffusion (smoother fields)
temperature_diffusivity = 5.0e-5

# Slower diffusion (preserve gradients)
temperature_diffusivity = 1.0e-5
```

## Output Interpretation

### Diagnostic Messages
```
wind_solver: starting temperature-wind recalculation with up to 2 iterations
wind_solver:   recalculation iteration 1 of 2
wind_solver:     max |Δw| = 0.0245 m/s
wind_solver:   recalculation iteration 2 of 2
wind_solver:     max |Δw| = 0.0032 m/s
wind_solver:   converged at iteration 2 (Δw < 0.01 m/s)
wind_solver: temperature-wind recalculation complete
```

**Interpretation:**
- `max |Δw|` shows change in vertical velocity between iterations
- Should decrease each iteration (convergence)
- When < tolerance, solver stops (saves time)

### Extract File (CSV)
```
z,x,y,u,v,w,temperature
10,50,50,8.23,0.15,0.042,301.2
10,50,100,8.19,-0.08,0.038,300.8
...
```

### Plot Files
- `plt_*/` directory contains HDF5 data for visualization
- Includes u, v, w, temperature, pressure fields
- Can be visualized with VisIt, ParaView, or custom scripts

## Troubleshooting

### Problem: Feature doesn't activate
```
wind_solver: Buoyancy stratification enabled (enable_buoyancy_stratification=true)
```
But you see NO recalculation messages. **Solution:** Check that all three are enabled:
```
enable_temperature_wind_recalculation = true
enable_temperature_transport = true
enable_buoyancy_stratification = true
```

### Problem: Too many iterations (slow)
Example output: `recalculation iteration 8 of 10`

**Causes:**
- Temperature changes too rapidly
- Grid too coarse
- Tolerance too strict

**Solutions:**
```ini
# Reduce transport timestep
scalar_dt = 30.0          # Was 60.0

# Increase tolerance
temperature_wind_recalc_tolerance = 0.02   # Was 0.01

# Increase grid resolution
dx = 10.0                 # Was 20.0
```

### Problem: Solution diverges

**Indicators:** Large |Δw| values, solver doesn't converge, NaNs in output

**Solutions:**
1. Check velocity boundary conditions
2. Reduce temperature diffusivity
3. Reduce scalar CFL number:
   ```ini
   scalar_cfl = 0.5        # Was 0.8
   ```

## Further Documentation

For detailed explanation of parameters and physics, see:
- `docs/TEMPERATURE_WIND_RECALCULATION.md` - Feature documentation
- `docs/GETTING_STARTED_TUTORIAL.md` - General setup
- Source code: `src/wind_solver_app.cpp` - Implementation details

## Performance Benchmarks

On a typical modern CPU (Intel Xeon, 16 cores):

| Example | Grid | Iterations | Time/Step | Total Time |
|---------|------|-----------|-----------|-----------|
| Basic   | 40×40×80 | 2 avg | 0.5 s | 50 s (100 steps) |
| Datacenter | 80×80×150 | 2.5 avg | 2.0 s | 400 s (200 steps) |
| Diurnal | 50×50×100 | 1.8 avg | 0.8 s | 20 min (1440 steps) |

*Times are approximate and machine-dependent*

## Questions or Issues?

See main repository for contact information:
https://github.com/hgopalan/massconsistent_amr
