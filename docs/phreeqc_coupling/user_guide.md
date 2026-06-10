# PHREEQC Coupling User Guide

## Overview

This guide provides practical, step-by-step instructions for using each capability of the PHREEQC reactive transport coupling framework. The framework enables wind-driven geochemical simulations for critical mineral studies, acid mine drainage analysis, and contaminant transport prediction.

## Table of Contents

1. [Foundation Capabilities](#foundation-capabilities)
2. [Advanced Geochemical Capabilities](#advanced-geochemical-capabilities)
3. [Optimization and Caching](#optimization-and-caching)
4. [Real-Time Operational Deployment](#real-time-operational-deployment)

---

## Foundation Capabilities

### 1. Wind Velocity as Boundary Condition

Wind velocity drives pore-water advection and oxygen delivery in subsurface geochemical simulations.

**When to use:**
- Leaching simulations requiring groundwater flow rates
- Contaminant transport through soils with wind-driven infiltration
- Dust suspension and settling calculations

**Basic workflow:**

```python
from wind_solver import WindSolver
from phreeqc_coupling import FieldExtractor

# Solve wind field
wind = WindSolver("inputs.i")
wind.solve()

# Extract velocity magnitude at specific height
extractor = FieldExtractor(wind)
u_mag = extractor.export_velocity_magnitude(z_level=10.0)  # 10 m height

# Export to PHREEQC
phreeqc_input = f"PRINT; -reset false\nPHREEQC BOUNDARY CONDITION: v_pore = {u_mag:.3f} m/s"
```

**Output interpretation:**
- Units: m/s
- Typical range: 0.1–20 m/s over varied terrain
- High velocities in exposed ridges; low in sheltered valleys

---

### 2. Temperature Profile Extraction

Temperature controls reaction kinetics via Arrhenius relationship and mineral solubility.

**When to use:**
- AMD chemistry prediction with temperature-dependent rate constants
- Leaching efficiency calculations (temperature affects diffusivity)
- CO₂ fugacity and carbonate equilibrium adjustments

**Basic workflow:**

```python
from wind_solver import WindSolver
from phreeqc_coupling import FieldExtractor

wind = WindSolver("inputs.i")
wind.solve()

extractor = FieldExtractor(wind)
z_agl, T_profile = extractor.export_temperature_profile()

# Use in PHREEQC
for z, T_kelvin in zip(z_agl, T_profile):
    T_celsius = T_kelvin - 273.15
    print(f"Height {z:.1f} m: T = {T_celsius:.2f} °C")
```

**Key physics:**
- Lapse rate: ~6–9.8 K/km (varies with stability)
- Affects oxidation kinetics: ~2× rate per 10°C increase
- Controls mineral solubility and precipitation

---

### 3. Precipitation Rate Mapping

Precipitation is a critical boundary condition for infiltration-driven transport and leaching rates.

**When to use:**
- Infiltration-recharge coupling to groundwater
- Dust suppression effects on pH evolution
- Seasonal variability in leaching efficiency

**Basic workflow:**

```python
from wind_solver import WindSolver
from phreeqc_coupling import FieldExtractor

wind = WindSolver("inputs.i")
wind.solve()

extractor = FieldExtractor(wind)
precip_rate = extractor.export_precipitation_rate()  # mm/hr

# Export spatially-varying infiltration
for x, y, P in zip(x_grid, y_grid, precip_rate):
    infiltration = P * 1e-3 / 3600.0  # Convert mm/hr to m/s
```

**Physical constraints:**
- Field-dependent: 0–50 mm/hr typical
- Suppresses dust suspension (low wind + high precip)
- Reduces oxidation potential (water films block O₂ transfer)

---

### 4. Vertical Diffusivity (K_v) Export

Turbulent diffusivity controls dispersivity and mixing in reactive transport, affecting contaminant spreading and reaction rates.

**When to use:**
- Dispersivity parameterization (α = K_v / |u|)
- Vertical mixing and reaction zone extent
- Plume spread estimation from point sources

**Basic workflow:**

```python
from wind_solver import WindSolver
from phreeqc_coupling import FieldExtractor

wind = WindSolver("inputs.i")
wind.solve()

extractor = FieldExtractor(wind)
z_agl, K_v = extractor.export_vertical_diffusivity()

# Compute dispersivity
alpha = K_v / u_mag  # u_mag from capability #1

# PHREEQC input
print(f"DISPERSIVITY = {alpha:.4f}")
```

**Typical values:**
- Near surface: K_v = 0.01–0.1 m²/s
- Free convection layers: K_v = 0.1–1.0 m²/s
- Stable layers: K_v < 0.01 m²/s

---

### 5. Atmospheric Stability Classification

Stability (Pasquill-Gifford-Turner A–F) modifies reaction rates and mixing depths.

**When to use:**
- Stability-dependent reaction rate modifiers (±50% variation)
- Boundary layer depth estimation for mixing zone
- Nighttime vs. daytime chemistry differences

**Basic workflow:**

```python
from wind_solver import WindSolver
from phreeqc_coupling import FieldExtractor

wind = WindSolver("inputs.i")
wind.solve()

extractor = FieldExtractor(wind)
stability_class = extractor.export_stability_classification()  # 'A', 'B', ..., 'F'

# Apply stability-dependent rate modifier
stability_factor = {
    'A': 0.75,  # Very unstable: reduced contact time
    'B': 0.85,  # Unstable
    'C': 0.95,  # Neutral
    'D': 1.00,  # Stable baseline
    'E': 1.15,  # Very stable: increased contact time
    'F': 1.25   # Extremely stable
}
```

**Physical basis:**
- Unstable (A–C): Enhanced mixing, lower residence time, lower oxidation potential
- Stable (E–F): Reduced mixing, higher residence time, higher oxidation potential
- Neutral (D): Baseline conditions

---

## Advanced Geochemical Capabilities

### 6. Valley AMD Hotspot Detection

Identifies and classifies acid mine drainage discharge points by oxidation risk using terrain-resolved wind diagnostics.

**When to use:**
- Monitoring priority assessment for environmental protection
- Prediction of high-risk discharge locations before mining operations
- Real-time alert system calibration

**Basic workflow:**

```python
from wind_solver import WindSolver
from phreeqc_coupling.amd_hotspot_detector import identify_valley_amd_hotspots

wind = WindSolver("inputs.i")
wind.solve()

# Identify hotspots
results = identify_valley_amd_hotspots(
    wind,
    'amd_locations.csv',
    output_dir='hotspots_output/'
)

# Results include:
#   - Locations and risk classification (HIGH/MEDIUM/LOW)
#   - Oxygen supply rates [µmol/(m²·s)]
#   - Wind diagnostics (u*, wind shear, K_v)
#   - GeoJSON for visualization

print(f"High-risk hotspots: {results['high_risk_count']}")
print(f"Output files: {results['output_files']}")
```

**Risk thresholds:**
- **HIGH**: O₂ supply ≥ 100 µmol/(m²·s) — Rapid oxidation, priority monitoring
- **MEDIUM**: 30–100 µmol/(m²·s) — Moderate oxidation risk
- **LOW**: < 30 µmol/(m²·s) — Slow oxidation, seasonal variation dominates

**Physics basis:**
- Sherwood correlation: Sh = 0.332 × Re^0.5
- Friction velocity from log-law: u* = κ × u / ln(z/z₀)
- O₂ supply rate: r_O₂ = k_c × [O₂]_sat (dimensionally: µmol/(m²·s))

**Reference:** Sherwood (1954); Businger et al. (1971)

---

### 7. Sulfide Oxidation Kinetics

Quantifies wind-dependent oxidation rates for sulfide minerals (pyrite, chalcopyrite, etc.) with acid generation prediction.

**When to use:**
- AMD generation forecasting for sulfide ore piles
- Acid rate estimation for treatment plant design
- pH evolution prediction in discharge zones

**Basic workflow:**

```python
from wind_solver import WindSolver
from phreeqc_coupling.sulfide_oxidation import compute_sulfide_oxidation_rates

wind = WindSolver("inputs.i")
wind.solve()

results = compute_sulfide_oxidation_rates(
    wind,
    'sulfide_locations.csv',
    temperature=288.15,  # 15°C
    output_dir='oxidation_output/'
)

# Results include:
#   - Oxidation rates [mol/(m³·s)] at each location
#   - Acid generation rates [mol H⁺/(m³·s)]
#   - O₂ delivery enhancement factors
#   - pH change rates

print(f"Mean oxidation: {results['mean_oxidation_rate']:.2e} mol/(m³·s)")
print(f"Max pH change rate: {results['max_pH_change_rate']:.3f} pH_units/day")
```

**Kinetics equations:**
- Arrhenius: k(T) = A × exp(-E_a/(R×T)), E_a = 45 kJ/mol
- Wind enhancement: f(u) = (u/u_ref)^0.75
- Stoichiometry: 2FeS₂ + 7O₂ + 2H₂O → 2FeSO₄ + 2H₂SO₄ (produces 2 H⁺/FeS₂)

**Temperature sensitivity:** ~2.5× rate increase per 10°C

**Reference:** Nicholson et al. (1990)

---

## Optimization and Caching

### 8. Scenario Library Caching

Pre-computed library of representative weather scenarios (100+ scenarios) enables <30 s runtime lookups versus 10+ minute wind solves.

**When to use:**
- Real-time operational monitoring (15-min cycle)
- Ensemble forecasting with multiple weather scenarios
- Mobile device deployment with limited compute

**Basic workflow:**

```python
from phreeqc_coupling.scenario_library import build_scenario_library, ScenarioLibrary

# One-time offline computation (1–2 hours, parallelizable)
lib = build_scenario_library(n_scenarios=100, output_dir='scenarios/')

# Then, at runtime: <30 s lookup
loaded_lib = ScenarioLibrary.load('scenarios/library.h5')
scenario = loaded_lib.nearest_scenario(
    u_mag=8.5,      # m/s
    wind_dir=270,   # degrees
    T=288.15        # K
)

# Use scenario fields directly
print(f"Wind at nearest scenario: {scenario.u_mag:.2f} m/s")
print(f"Cached K_v: {scenario.K_v_profile}")
```

**Storage options:**
- HDF5: Efficient binary, ~100–500 MB for 100 scenarios
- JSON: Portable, slightly larger

**Use cases:**
- Operational hotspot monitoring every 15 minutes
- Mobile/field devices with limited storage and compute
- Integration with NWP ensemble forecasts

---

### 9. Spatially-Varying Temperature Fields

Extracts temperature at specific (x, y) locations from scenario library with automatic elevation corrections and lapse-rate adjustments.

**When to use:**
- Column-wise PHREEQC simulations at specific locations
- Temperature-dependent reaction rate adjustments at hotspots
- Elevation-corrected boundary conditions for steep terrain

**Basic workflow:**

```python
from phreeqc_coupling.spatial_temperature_cache import SpatialTemperatureCache

cache = SpatialTemperatureCache(scenario_library)

# Extract 1D T profile at specific (x, y) location
x, y = 5000, 5000  # Coordinates
z_agl, T_profile = cache.export_phreeqc_boundary_conditions(
    x=x, y=y,
    elevation_correction=True
)

# Write to PHREEQC input
with open('phreeqc_bc.txt', 'w') as f:
    for z, T in zip(z_agl, T_profile):
        T_celsius = T - 273.15
        f.write(f"# Height {z:.1f} m: T = {T_celsius:.2f} °C\n")
```

**Features:**
- Automatic elevation corrections (lapse rate adjustment)
- Topology-aware interpolation for complex terrain
- Cache validation and bounds checking

---

### 10. Dust Suppression Lookup Tables

Pre-computed wind-dependent dust settling affects pH evolution (high wind → suspension → less acidification; low wind → settling → acidification).

**When to use:**
- pH evolution modeling in leaching simulations
- Wind-controlled dust transport and reactivity
- Seasonal variability in AMD chemistry

**Basic workflow:**

```python
from phreeqc_coupling.dust_suppression_lookup import (
    compute_dust_suppression_factor,
    compute_dust_suppression_effect_on_ph
)

# Dust suppression factor (0 = full settling, 1 = full suspension)
u_speed = 5.0  # m/s
particle_size = 10.0  # microns
suppression_factor = compute_dust_suppression_factor(u_speed, particle_size)

# pH modification
reference_pH = 3.5
adjusted_pH = compute_dust_suppression_effect_on_ph(u_speed, reference_pH)

print(f"High wind (suppression={suppression_factor:.2f}) → pH = {adjusted_pH:.2f}")
print(f"Low wind (settling) → lower pH (more acidic)")
```

**Particle size range:** 0.1–1000 µm (clay to coarse sand)

**Physical basis:**
- Settling velocity: v_s = ρ_p × d_p² × g / (18 × μ)
- Wind suspension threshold varies with particle size

---

### 11. Leaching Efficiency (Sherwood Correlation)

Wind-driven mass transfer enhancement of ore leaching efficiency via Sherwood number correlation.

**When to use:**
- Critical mineral leaching rate prediction
- Extraction efficiency optimization with wind conditions
- Comparisons of sheltered vs. exposed leaching sites

**Basic workflow:**

```python
from phreeqc_coupling.leaching_efficiency import (
    compute_leaching_efficiency,
    compute_leaching_rate_enhancement
)

# Compute efficiency factor
u_speed = 3.5  # m/s
particle_size = 500.0  # microns (ore particle)
efficiency = compute_leaching_efficiency(u_speed, particle_size)

# Apply to baseline dissolution rate
baseline_rate = 1e-6  # mol/(m²·s)
enhanced_rate = compute_leaching_rate_enhancement(u_speed, baseline_rate)

print(f"Wind speed {u_speed:.1f} m/s → efficiency factor {efficiency:.2f}")
print(f"Dissolution rate enhanced: {baseline_rate:.2e} → {enhanced_rate:.2e} mol/(m²·s)")
```

**Sherwood correlation:**
- Sh = 2 + 0.6 × Re^0.5 × Sc^0.33 (Ranz & Marshall 1952)
- Reynolds: Re = ρ × u × D / μ
- Leaching efficiency: ∝ Sh × diffusivity

**Typical range:** 1–10× enhancement with wind speeds 0–20 m/s

**Reference:** Sherwood (1954); Ranz & Marshall (1952)

---

## Real-Time Operational Deployment

### Continuous Monitoring Loop (15-min Cycle)

**Typical task prioritization:**

```python
from operational_amd_monitoring_system import OperationalMonitor

monitor = OperationalMonitor(
    scenario_library='scenarios/library.h5',
    output_dir='monitoring_output/',
    log_file='monitoring.log'
)

while True:
    # Primary tasks (required every cycle)
    monitor.run_task(1, "Wind field extraction")
    monitor.run_task(2, "Temperature profile extraction")
    monitor.run_task(3, "Precipitation infiltration")
    monitor.run_task(5, "Stability classification")
    monitor.run_task(13, "AMD hotspot detection")
    monitor.run_task(11, "Sulfide oxidation rates")
    monitor.run_task(19, "Real-time dashboard update")
    
    # Secondary tasks (optional if compute-limited)
    if monitor.compute_available():
        monitor.run_task(6, "Sherwood correlation")
        monitor.run_task(7, "Dust suppression effects")
        monitor.run_task(18, "Leaching efficiency")
        monitor.run_task(21, "End-to-end facility workflow")
    
    # Wait for next cycle
    monitor.sleep_until_next_cycle(15 * 60)  # 15 minutes
```

**Performance targets:**
- Primary tasks: <5 min total (using cached scenarios)
- Secondary tasks: <10 min if enabled
- Dashboard update: <30 s
- Cycle completion: <15 min for reliable operation

---

## Quick Reference: Physics Constants and Thresholds

| Constant | Value | Units |
|----------|-------|-------|
| Karman constant (κ) | 0.41 | dimensionless |
| Gas constant (R) | 8.314 | J/(mol·K) |
| Activation energy (E_a, pyrite) | 45,000 | J/mol |
| Sherwood prefactor (K_sh) | 0.332 | dimensionless |
| Wind exponent (oxidation) | 0.75 | dimensionless |
| O₂ saturation at 25°C | ~280 | µmol/L |
| H⁺ per FeS₂ oxidized | 2 | moles |

| Threshold | Low Risk | Medium Risk | High Risk | Units |
|-----------|----------|-------------|-----------|-------|
| O₂ supply rate | <30 | 30–100 | ≥100 | µmol/(m²·s) |
| Dust suppression (20 µm) | <2 | 2–8 | >8 | m/s wind |
| Oxidation rate (pyrite) | <1e-8 | 1e-8–1e-7 | >1e-7 | mol/(m³·s) |

---

## References

1. **Businger, J.A., Wyngaard, J.C., Izumi, Y., & Bradley, E.F.** (1971). Flux-profile relationships in the atmospheric surface layer. *Journal of Atmospheric Sciences*, 28(2), 181–189.

2. **Nicholson, R.V., Gillham, R.W., & Reardon, E.J.** (1990). Pyrite oxidation in carbonate-buffered systems. *Geochimica et Cosmochimica Acta*, 54(2), 395–405.

3. **Sherwood, T.K.** (1954). Mass transfer between phases. *Industrial & Engineering Chemistry*, 46(2), 221–231.

4. **Parkhurst, D.L., & Appelo, C.A.J.** (2013). Description of the PHREEQC (Version 3) computer program for speciation, batch-reaction, one-dimensional transport, and inverse geochemical calculations. *USGS Techniques and Methods*, Book 6, Chapter A43.

5. **Gelhar, L.W., Welty, C., & Rehfeldt, K.R.** (1992). A critical review of data on field-scale dispersion in aquifers. *Water Resources Research*, 28(7), 1955–1974.

6. **Stull, R.B.** (2011). *An Introduction to Boundary Layer Meteorology* (2nd ed.). Kluwer Academic Publishers.

7. **Plummer, L.N., & Busenberg, E.** (1982). The solubility of calcite, aragonite and vaterite in CO₂-H₂O solutions. *Geochimica et Cosmochimica Acta*, 46(6), 1011–1040.

8. **Paulson, C.A., & Simpson, J.E.** (1981). The mathematical representation of wind speed and temperature profiles in the unstable atmospheric surface layer. *Journal of Applied Meteorology*, 20(4), 466–478.

9. **King, D.L., Cooper, W.J., & Furlong, E.T.** (1991). Kinetics of oxidation of Fe(II) and Mn(II) by permanganate. *Environmental Science & Technology*, 25(4), 666–671.

10. **Stumm, W., & Morgan, J.J.** (1996). *Aquatic Chemistry* (3rd ed.). Wiley-Interscience.

11. **Ranz, W.E., & Marshall, W.R.** (1952). Evaporation from drops. *Chemical Engineering Progress*, 48(3), 141–146.

---

**Last Updated:** 2026-06-10  
**massconsistent_amr PHREEQC Coupling v1.0.0**
