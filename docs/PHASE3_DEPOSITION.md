# Phase 3.3: Enhanced Deposition Modeling

## Overview

Deposition removes pollutant mass from atmospheric puffs via two primary mechanisms:

1. **Dry Deposition**: Gravitational settling, impaction, diffusion to surface
2. **Wet Deposition**: Precipitation scavenging (removal by rain/snow)

This module implements size-dependent and surface-dependent deposition parameterizations.

## Physical Background

### Dry Deposition Mechanisms

1. **Gravitational Settling** (larger particles)
   - Particles settling at terminal velocity
   - Governed by Stokes law with slip correction
   - Range: 10⁻³ to 10⁻¹ m/s (submicron to 10 μm)

2. **Impaction** (intermediate particles)
   - Particles unable to follow air streamlines around obstacles
   - Efficiency increases with particle size

3. **Diffusion** (very small particles and gases)
   - Brownian motion drives particles to surface
   - Molecular diffusion for gases
   - More efficient at higher temperature

4. **Chemical Reactivity** (reactive gases)
   - HNO₃, SO₂ dissolve on wet surfaces
   - Very high deposition velocities (cm/s range)

### Wet Deposition Mechanisms

1. **In-Cloud Scavenging**
   - Aerosol nucleation (CCN)
   - Droplet capture in cloud

2. **Below-Cloud Scavenging**
   - Raindrops collision with particles
   - Efficiency increases with raindrop size

3. **Gas Dissolution**
   - Highly soluble gases (SO₂, HNO₃) dissolve in raindrops

## Dry Deposition

### Stokes Settling Velocity

For particles in the Stokes regime (Re < 1):

```
v_s = (ρ_p * d_p² * g * C_c) / (18 * η)
```

Where:
- v_s = settling velocity [m/s]
- ρ_p = particle density [kg/m³]
- d_p = particle diameter [m]
- g = gravitational acceleration = 9.81 m/s²
- C_c = Cunningham slip correction factor
- η = air dynamic viscosity = 1.8×10⁻⁵ Pa·s

### Cunningham Slip Correction

For small particles where d_p ≈ mean free path:

```
C_c = 1 + 2K_n * (1.257 + 0.4 * exp(-1.1/K_n))
```

Where:
- K_n = Knudsen number = 2λ_air / d_p
- λ_air = air mean free path = 6.6×10⁻⁸ m

**Effect:**
- d_p > 1 μm: C_c ≈ 1 (Stokes law applies)
- d_p = 0.1 μm: C_c ≈ 2.2 (slip correction significant)
- d_p = 0.01 μm: C_c ≈ 6+ (diffusion-limited regime)

### Deposition Velocity

Total dry deposition velocity combines settling and surface-dependent uptake:

```
v_d = v_settling + v_d_base
```

Where v_d_base depends on surface type:

| Surface Type | v_d_base [m/s] | Description |
|--------------|----------------|-------------|
| Grass (smooth) | 0.001 | Low roughness |
| Urban (rough) | 0.002 | Building roughness |
| Water (smooth) | 0.0005 | Very smooth, low uptake |
| Forest (canopy) | 0.005 | High roughness, leaves |

### Database Format

`dry_deposition_velocity.csv`:
```csv
species_id,surface_type,vd_m_per_s,description,particle_diameter_um
SO4,grass,0.001,Sulfate over grass
NO3,forest,0.006,Nitrate over forest canopy
Dust,urban,0.02,Coarse particles in urban
```

## Wet Deposition (Precipitation Scavenging)

### Scavenging Coefficient

Empirical relationship between scavenging rate and precipitation intensity:

```
Λ(t) = Λ₀ * (P / P_ref)^a
```

Where:
- Λ = scavenging removal rate [s⁻¹]
- Λ₀ = base coefficient at reference precipitation = 1×10⁻⁴ s⁻¹
- P = precipitation rate [mm/hr]
- P_ref = reference precipitation = 0.1 mm/hr
- a = precipitation exponent (typically 0.6-0.8)

### Physical Interpretation

- **No precipitation** (P = 0): Λ = 0 (no wet removal)
- **Light rain** (P = 0.5 mm/hr): Λ = 1×10⁻⁴ × 5^0.8 ≈ 2.4×10⁻⁴ s⁻¹
- **Moderate rain** (P = 5 mm/hr): Λ = 1×10⁻⁴ × 50^0.8 ≈ 7.6×10⁻⁴ s⁻¹
- **Heavy rain** (P = 50 mm/hr): Λ = 1×10⁻⁴ × 500^0.8 ≈ 2.4×10⁻³ s⁻¹

### Removal Timescale

Half-life for mass removal via scavenging:
```
t₁/₂ = ln(2) / Λ
```

Examples:
- No rain: t₁/₂ = ∞ (no removal)
- 0.5 mm/hr: t₁/₂ ≈ 47 minutes
- 5 mm/hr: t₁/₂ ≈ 15 minutes
- 50 mm/hr: t₁/₂ ≈ 5 minutes

### Database Format

`wet_scavenging_coefficients.csv`:
```csv
species_id,lambda0_per_s,exponent_a,description,henry_constant
SO2,1.0e-4,0.8,Highly soluble gas
HNO3,1.5e-4,0.85,Very soluble acid gas
SO4,5.0e-5,0.7,Particle, moderate scavenging
```

## Combined Dry + Wet Removal

Both mechanisms act simultaneously:

```
m(t) = m₀ * exp[-(v_d/h + Λ) * t]
```

Where:
- v_d = dry deposition velocity [m/s]
- h = puff depth scale [m]
- Λ = wet scavenging coefficient [s⁻¹]

**Example**: SO₄ with v_d = 0.001 m/s, h = 50 m, Λ = 2×10⁻⁴ s⁻¹ (light rain)
```
Total removal rate = 0.001/50 + 2×10⁻⁴ = 2×10⁻⁵ + 2×10⁻⁴ = 2.2×10⁻⁴ s⁻¹
Half-life = ln(2) / 2.2×10⁻⁴ ≈ 52 minutes
```

## Particle Size Distribution

Real aerosols have a distribution of sizes. The model assumes lognormal distribution:

```
n(d) ∝ (1/d) * exp[-(ln(d) - ln(d_g))² / (2 * (ln(σ_g))²)]
```

Where:
- d_g = geometric mean diameter
- σ_g = geometric standard deviation (typically 1.2-2.5)

## Usage in Input Files

### Basic Configuration

```
# Enable size-dependent settling
puff_model.enable_size_dependent_settling = true
puff_model.particle_diameter_mean = 1.0e-6     # 1 μm
puff_model.particle_diameter_std = 0.3         # log-std

# Enable precipitation scavenging
puff_model.enable_rain_dependent_scavenging = true
puff_model.wet_scavenging_file = "path/to/wet_scavenging_coefficients.csv"
puff_model.dry_deposition_file = "path/to/dry_deposition_velocity.csv"
```

### Typical Particle Sizes

| Species | Typical d_g [μm] | Type | Comment |
|---------|------------------|------|---------|
| SO₄ | 0.3-1.0 | Accumulation | Nucleation mode growth |
| NO₃ | 0.5-2.0 | Accumulation | Larger than SO₄ |
| OC | 0.2-1.0 | Mixed | Both nucleation & accumulation |
| BC | 0.05-0.3 | Nucleation | Primary emissions only |
| Dust | 2-20 | Coarse | Large settling velocity |

## Computational Implementation

### Dry Deposition Time Integration

```cpp
// Remove mass due to settling
Real h_eff = puff_height + sigma_z;  // effective depth
Real dry_removal_rate = vd / h_eff;
mass *= exp(-dry_removal_rate * dt);
```

### Wet Deposition Time Integration

```cpp
// Compute scavenging rate from precipitation
Real lambda = lambda0 * pow(precip_rate / precip_ref, exponent_a);

// Remove mass due to scavenging
mass *= exp(-lambda * dt);
```

### Combined Application

```cpp
void apply_combined_deposition(
    Real& mass,
    Real dt,
    Real vd,           // dry deposition velocity
    Real puff_height,  // puff center height
    Real lambda,       // wet scavenging coefficient
    Real sigma_z)      // puff vertical spread
{
    Real h_eff = max(puff_height + sigma_z, 1.0);
    Real dry_rate = vd / h_eff;
    Real total_rate = dry_rate + lambda;
    mass *= exp(-total_rate * dt);
}
```

## Validation & Observational Data

### EPA Studies

- Median SO₄ deposition velocity: 0.1-0.3 cm/s over grass
- NO₃ deposition velocity: Similar to SO₄, but higher over forests
- Dust: 0.5-3 cm/s (very size-dependent)

### Rain Scavenging Coefficients

From EPA/NOAA monitoring:
- SO₂ (very soluble): Λ ≈ 1-2 × 10⁻⁴ s⁻¹ (light rain)
- HNO₃ (ultra-soluble): Λ ≈ 1.5-3 × 10⁻⁴ s⁻¹
- SO₄ (particle): Λ ≈ 0.5-1 × 10⁻⁴ s⁻¹ (weaker)

## Computational Cost

- **Dry deposition**: ~1 μs per puff
- **Wet deposition**: ~2 μs per puff per timestep
- **Combined**: ~3 μs per puff per timestep
- **Overall overhead**: ~1-2% for typical simulations

## Known Limitations

1. **Uniform Puff Assumption**: Deposition applied to entire puff uniformly
   - Reality: Surface-near puff region deposits faster

2. **No Size-Resolved Tracking**: All mass within species treated equally
   - Future: Could track multiple size bins per species

3. **Surface Type Assumed Constant**: No variation within domain
   - Current: Can specify one surface type per deposition database row
   - Future: Could use landuse grid already available

4. **No Gravitational Settling for Gases**: Only for particles
   - SO₂, HNO₃ gases have zero settling (correct)

## Future Enhancements

1. **Multi-Bin Size Distribution**: Track 5-10 size modes
2. **Landuse Integration**: Vary v_d based on local surface type
3. **Seasonal Variation**: Different v_d for summer/winter
4. **In-Cloud vs. Below-Cloud Scavenging**: Separate algorithms

## References

1. **Zhang, L., et al. (2001)**. A size-segregated, size-dependent particle dry deposition scheme. Atmospheric Environment, 35(3), 549-560.

2. **Seinfeld, J.H., & Pandis, S.N. (2016)**. Atmospheric Chemistry and Physics (3rd ed.). Wiley-Interscience.

3. **EPA (2021)**. AERMOD User's Guide. EPA-454/R-03-004.

4. **Hegg, D.A., et al. (2000)**. Measurements of aerosol optical properties in the Sagano forest, Japan. Journal of Geophysical Research, 105(D2), 2383-2396.

