# SCM Enhancements: Neutral, Stable, and Unstable ABL Physics

## Overview

The Single Column Model (SCM) in `src/scm_models.H` has been significantly enhanced to support comprehensive physics for three atmospheric boundary layer stability classes:
- **Neutral (N)**: Weakly stratified ABL with minimal buoyancy effects
- **Stable (S)**: Statically stable ABL with suppressed turbulence
- **Unstable (U)**: Convectively unstable ABL with enhanced turbulence and mixing

## New Physical Constants

```cpp
constexpr amrex::Real RHO_AIR = 1.225;  // kg/m^3 (air density at sea level)
constexpr amrex::Real CP_AIR = 1005.0;  // J/(kg*K) (specific heat of air)
```

## New Stability Enumeration

```cpp
enum class StabilityClass {
    UNSTABLE = -1,  // Free convection dominates (L < 0)
    NEUTRAL = 0,    // Weakly stratified (|L| > 1000 m)
    STABLE = 1      // Static stability dominates (L > 0)
};
```

## Enhanced SCMState1D Structure

Added two new fields to track stability effects:
- `std::vector<amrex::Real> Ri;` - Richardson number for each height level
- `amrex::Real stability_class;` - Current stability classification

## New Helper Functions

### 1. **compute_richardson_number()**
Computes the Richardson number (Ri) for stratification analysis:
```
Ri = N² / (dU/dz)²
```
where N² is the Brunt-Väisälä frequency squared representing static stability.

### 2. **get_stability_class()**
Classifies local atmospheric stability based on z/L (height/Obukhov length):
- Unstable: z/L < -0.01
- Neutral: -0.01 ≤ z/L ≤ 0.01
- Stable: z/L > 0.01

### 3. **compute_prandtl_number()**
Computes stability-dependent turbulent Prandtl number:
- **Stable**: Higher values (Pr_t = 1.0 × (1 + 2.0×z/L)) → Reduced heat transfer
- **Unstable**: Lower values (Pr_t = 1.0 / (1 + 2.0×|z/L|)) → Enhanced heat transfer
- **Neutral**: Baseline value (Pr_t = 1.0)

Based on Högström (1988) and Beljaars & Holtslag (1989).

### 4. **compute_cmu()**
Stability-dependent TKE model coefficient based on Richardson number:
- **Stable**: Dampening factor = 1 / (1 + 10×Ri)
- **Unstable**: Enhancement factor = √(1 - 5×Ri)
- **Neutral**: Baseline cmu = 0.1

### 5. **compute_monin_obukhov_length()**
Computes Monin-Obukhov length dynamically from surface friction and heat flux:
```
L = -ρ·cp·T·u*³ / (κ·g·Qh)
```

### 6. **compute_mixing_length()**
Stability-dependent mixing length calculation:

**Stable conditions** (Holtslag & Boville 1993):
```
l_s = l_m / √(1 + 5×z/L)
```
Shorter mixing lengths suppress turbulence.

**Unstable conditions** (Deardorff 1966):
```
l_u = l_m × (1 - 8×z/L)^(1/3)
```
Longer mixing lengths enhance convective mixing.

**Neutral conditions** (Blackadar):
```
l_m = 1 / √(1/l_shear² + 1/l_max²)
```

## Enhanced Functions

### 1. **compute_similarity_surface()**
Now includes:
- Dynamic computation of heat flux from temperature gradients
- Dynamic Monin-Obukhov length calculation (if not prescribed)
- Stability classification based on computed L
- Stability-corrected surface layer parameterization with proper ψ functions

### 2. **compute_eddy_viscosity()**
Significantly enhanced with:
- Richardson number computation at each level
- Stability classification at each level
- Stability-dependent mixing length via `compute_mixing_length()`
- Stability-dependent cmu coefficient via `compute_cmu()`
- Proper Brunt-Väisälä frequency (N²) computation

The eddy viscosity is now:
```
ν_t = cmu(Ri) × √(tke) × l(z/L, Ri)
```

### 3. **update_temperature()**
Now includes:
- Stability-dependent Prandtl number computation
- Enhanced heat diffusion parameterization
- Properly weighted vertical heat fluxes

### 4. **update_tke()**
Significantly enhanced with:
- **Shear production**: ν_t × [(∂u/∂z)² + (∂v/∂z)²]
- **Buoyancy production**: -g/T × (ν_t/σ_t) × ∂T/∂z
  - Positive in unstable (destabilizing)
  - Negative in stable (stabilizing)
- **Diffusion terms** (unchanged)
- **Stability-dependent dissipation**: Ce × (tke^1.5) / l

The buoyancy term implements the critical mechanism differentiating stable/unstable ABLs.

## Physical Mechanisms Implemented

### Neutral ABL
- Standard log-law wind profile
- Constant-stress turbulent layer
- Minimal vertical temperature gradient
- Mixing length follows Blackadar scale

### Stable ABL
- Reduced mixing length → suppressed turbulence
- Increased Prandtl number → reduced heat transfer
- Reduced cmu → lower turbulence production
- Negative buoyancy production → TKE reduction
- Wind profile deviates from log-law near surface

### Unstable ABL
- Enhanced mixing length → vigorous convection
- Reduced Prandtl number → enhanced heat transfer
- Enhanced cmu → higher turbulence production
- Positive buoyancy production → TKE enhancement
- Strong vertical mixing and thermals

## Scientific References

1. **Monin-Obukhov Similarity Theory**: Businger et al. (1971), Högström (1988)
2. **Stable Stratification**: Holtslag & Boville (1993)
3. **Unstable Stratification**: Deardorff (1966)
4. **Turbulence Modeling**: k-epsilon model modifications
5. **Richardson Number**: Classical fluid dynamics stability criterion

## Backward Compatibility

All changes are backward compatible:
- Existing code using default M-O length still works
- Neutral case remains numerically unchanged
- New features are opt-in when stability classes are used
- Existing API remains unchanged

## Usage Examples

### Computing Stability Class
```cpp
amrex::Real z_over_L = state.z[i] / state.mo_length;
StabilityClass stability = get_stability_class(z_over_L);
```

### Dynamic M-O Length
```cpp
state.mo_length = compute_monin_obukhov_length(state.ustar, state.Qh, state.t_ref);
```

### Stability-Dependent Prandtl Number
```cpp
amrex::Real z_over_L = state.z[i] / state.mo_length;
amrex::Real sigma_t = compute_prandtl_number(z_over_L);
```

## Testing and Validation

The enhanced SCM should be tested against:
1. **Neutral cases**: Expected to match pre-enhancement results
2. **Stable cases**: TKE should decay, mixing lengths shorten
3. **Unstable cases**: TKE should grow, mixing lengths expand
4. **Transitions**: Smooth behavior across z/L = 0

## Future Improvements

Potential enhancements for future work:
- Higher-order closure schemes (1.5-equation, 2-equation)
- Prognostic temperature equation with surface heating
- Cloud/LCL parameterization for cumulus
- Surface layer extension to higher z/L values
- Spectral methods for more accurate energy transfer
