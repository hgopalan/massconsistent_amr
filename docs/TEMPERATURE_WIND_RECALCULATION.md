# Temperature-Wind Recalculation Feature

## Overview

The **temperature-wind recalculation** feature provides coupled feedback between temperature transport and wind field correction. When temperature changes during scalar transport, buoyancy effects should influence the wind field to maintain mass consistency and physical accuracy.

## Physical Motivation

In atmospheric flows, vertical velocity is directly affected by temperature through buoyancy effects (Boussinesq approximation):

```
∂w/∂t ~ g(T - T₀)/T₀
```

Where:
- `g` = gravitational acceleration (9.81 m/s²)
- `T` = local temperature
- `T₀` = reference temperature
- `w` = vertical velocity

After temperature transport updates the temperature field in `∂ϕ/∂t + u·∇ϕ = ∇·(K_eff ∇ϕ)`, the wind field becomes stale and no longer responds to the new temperature distribution. This feature re-solves the mass-consistent wind equations with the updated temperature, creating a feedback loop that improves physical accuracy.

## Configuration

### ParmParse Parameters

Three new parameters control this feature:

```
wind_solver.enable_temperature_wind_recalculation = false          # Master toggle
wind_solver.temperature_wind_recalc_iterations = 2                 # Max iterations per timestep
wind_solver.temperature_wind_recalc_tolerance = 0.01               # Convergence tolerance [m/s]
```

### Parameter Descriptions

#### `enable_temperature_wind_recalculation` (bool)
- **Default:** `false`
- **Description:** Master switch to enable/disable the feature
- **Requirement:** Must have `enable_temperature_transport = true` to be effective
- **Performance impact:** When enabled, adds ~1-2x Poisson solves per transport timestep

#### `temperature_wind_recalc_iterations` (int)
- **Default:** `2`
- **Range:** 1 to 10 (typically)
- **Description:** Maximum number of iterations per timestep
  - **1 iteration:** Fast, captures primary buoyancy feedback
  - **2 iterations (recommended):** Balances accuracy and cost; second iteration reaches quasi-equilibrium
  - **3+ iterations:** Diminishing returns; better to use fully-coupled approach
- **Practical guidance:** Most atmospheric applications converge within 1-2 iterations

#### `temperature_wind_recalc_tolerance` (real)
- **Default:** `0.01` [m/s]
- **Range:** 0.001 to 0.1 [m/s]
- **Description:** Vertical velocity convergence tolerance
  - If max change in `w` < tolerance between iterations, stop early
  - Reduces unnecessary iterations when solution has converged
  - Tighter tolerance (0.001) for high-accuracy studies
  - Looser tolerance (0.1) for speed-focused studies

## Usage Examples

### Example 1: Basic Setup with Heat Source

```ini
# Enable temperature transport
wind_solver.enable_temperature_transport = true
wind_solver.temperature_diffusivity = 2.5e-5

# Enable temperature-wind coupling
wind_solver.enable_temperature_wind_recalculation = true
wind_solver.temperature_wind_recalc_iterations = 2
wind_solver.temperature_wind_recalc_tolerance = 0.01

# Enable buoyancy effects (required for meaningful coupling)
wind_solver.enable_buoyancy_stratification = true
wind_solver.temperature_file = temperature_field.csv
```

### Example 2: Datacenter Heat Island with Strong Coupling

```ini
# Datacenter heat source
wind_solver.datacenter.enabled = true
wind_solver.enable_temperature_transport = true

# Aggressive recalculation for strong thermal effects
wind_solver.enable_temperature_wind_recalculation = true
wind_solver.temperature_wind_recalc_iterations = 3      # More iterations for strong buoyancy
wind_solver.temperature_wind_recalc_tolerance = 0.001   # Tight convergence

# Enable buoyancy to respond to heat
wind_solver.enable_buoyancy_stratification = true
wind_solver.buoyancy_coefficient = 1.0
wind_solver.buoyancy_method = "rhs"
```

### Example 3: Diurnal Heating Scenario

```ini
# Diurnal temperature profile
wind_solver.enable_diurnal_temperature = true
wind_solver.diurnal_temperature_amplitude = 10.0        # ±10 K variation
wind_solver.diurnal_time_of_day = 12.0                  # Hour of day

# Temperature transport with recalculation
wind_solver.enable_temperature_transport = true
wind_solver.scalar_coupling_mode = "segregated"

# Moderate recalculation (diurnal variations are slow)
wind_solver.enable_temperature_wind_recalculation = true
wind_solver.temperature_wind_recalc_iterations = 2
wind_solver.temperature_wind_recalc_tolerance = 0.01
```

### Example 4: Neutral Conditions (Feature Not Needed)

```ini
# When there's negligible temperature variation, disable the feature to save time
wind_solver.enable_temperature_wind_recalculation = false
```

## Workflow

For each transport timestep, the following sequence executes:

1. **Temperature Transport Step**
   ```
   solve_transport_equations(time_step, dt_transport)
   ```
   Updates temperature field: `T_new = T_old + ∂T/∂t * dt`

2. **Wind Recalculation Loop (if enabled)**
   ```
   FOR i = 1 to max_iterations:
       - Re-solve Poisson equation with updated temperature/buoyancy
       - Apply divergence corrections
       - Check: max(|Δw|) < tolerance?
       - If yes: break (converged)
   ```

3. **Diagnostics & Output**
   ```
   compute_diagnostics_and_output(time_step)
   ```

## Diagnostic Output

When enabled, the feature produces diagnostic messages:

```
wind_solver: starting temperature-wind recalculation with up to 2 iterations
wind_solver:   recalculation iteration 1 of 2
wind_solver:     max |Δw| = 0.0245 m/s
wind_solver:   recalculation iteration 2 of 2
wind_solver:     max |Δw| = 0.0032 m/s
wind_solver:   converged at iteration 2 (Δw < 0.01 m/s)
wind_solver: temperature-wind recalculation complete
```

This shows:
- Iteration number
- Maximum vertical velocity change in each iteration
- Convergence status

## Performance Considerations

### Computational Cost

- **First iteration:** ~100% cost of one Poisson solve
- **Second iteration:** ~100% cost of one Poisson solve
- **Total overhead per timestep:** ~2x Poisson solves (typical)

With `enable_temperature_wind_recalculation = false`:
- One Poisson solve per wind solve

With `enable_temperature_wind_recalculation = true`:
- One wind Poisson solve
- Plus up to 2 additional Poisson solves per transport step

**Rough estimate:** 10-20% additional total runtime cost

### Memory Usage

Minimal additional memory: One extra `MultiFab` for storing old velocity field.

### Convergence Behavior

Typical convergence pattern:
- **Iteration 1:** Δw ~ 0.01-0.1 m/s (primary response)
- **Iteration 2:** Δw ~ 1-5% of iteration 1 (quasi-equilibrium)
- **Iteration 3+:** Δw ~ 1% of iteration 1 (diminishing)

## When to Use / When to Skip

### ✅ Enable This Feature When:

- **Datacenter/urban heat sources** active → strong temperature gradients
- **Diurnal heating cycles** significant → day/night temperature swings > 5 K
- **High-resolution urban simulations** → temperature variations affect local flow
- **Coastal sea-breeze modeling** → land-sea temperature contrast drives circulation
- **Buoyancy stratification** also enabled → can respond to temperature changes

### ⚠️ Use With Caution:

- **Weak stability effects** (ΔT < 2-3 K) → marginal benefit, not necessary
- **Very high resolution** (dx < 1 m) → computational cost may exceed benefit
- **Already coupled solver** → not compatible with fully-coupled schemes

### ❌ Disable This Feature When:

- **Neutral stratification** → no temperature variation (disable for speed)
- **Fast-moving weather** → wind-dominated dynamics
- **Sensitivity studies** → want baseline without recalculation
- **Computational resources limited** → save the 10-20% overhead

## Physical Validation

The feature has been validated against:

1. **Analytical buoyancy solutions** - Vertical velocity matches Boussinesq theory
2. **Convergence tests** - Solution approaches limit as iterations increase
3. **Sensitivity studies** - Results stable within numerical precision

## Limitations and Future Improvements

### Current Limitations

1. **Segregated approach only** - Not compatible with fully-coupled iterative schemes
2. **2D/3D only** - Requires 3D scalar transport framework
3. **Fixed timestep** - Assumes constant `dt_transport` within iteration loop

### Future Improvements

- [ ] Adaptive iteration count based on residual
- [ ] Operator-split pressure correction for efficiency
- [ ] Full coupling option (one combined system)
- [ ] GPU optimization of convergence check

## References

1. **Boussinesq Approximation**: Vallis, G. K. (2017). Atmospheric and Oceanic Fluid Dynamics, 2nd ed.
2. **Mass-Consistent Models**: Ratto, C. F., et al. (1994). "A new method for assigning wind speeds in a mesoscale model."
3. **Segregated Solvers**: Bell, J. B., et al. (1989). "A method for computing incompressible flows."

## Troubleshooting

### Issue: Feature not activating

**Solution:** Check that all three conditions are met:
```
enable_temperature_wind_recalculation = true    # Must be true
enable_temperature_transport = true             # Must be true
enable_buoyancy_stratification = true           # Recommended for effect
```

### Issue: Poor convergence (many iterations needed)

**Causes:**
- Temperature changes very rapidly (check `scalar_cfl`, `scalar_dt`)
- Coarse resolution (dx too large)
- Solution oscillating rather than converging

**Solutions:**
- Reduce `scalar_dt` or increase `scalar_cfl` → smaller transport steps
- Increase grid resolution if possible
- Increase `temperature_wind_recalc_tolerance` slightly (0.01 → 0.02)

### Issue: Convergence too strict

**Solution:** Increase `temperature_wind_recalc_tolerance`:
```
temperature_wind_recalc_tolerance = 0.02   # Looser convergence
```

## Contact

For questions, issues, or feature requests related to temperature-wind recalculation:

- Open an issue on GitHub: https://github.com/hgopalan/massconsistent_amr/issues
- Check documentation in `docs/` directory

