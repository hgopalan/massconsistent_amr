# Phase 2: Multi-Source & Stack Modeling Implementation

This document describes the Phase 2 features implemented in the Gaussian puff dispersion model.

## 2.1 Multiple Simultaneous Sources (Phase 2.1)

### Overview
The puff model now supports multiple concurrent emission sources specified via a CSV file. This allows modeling of complex scenarios with multiple pollution sources operating simultaneously.

### Features
- **Multiple sources**: Emit puffs from multiple locations with independent parameters
- **Backward compatibility**: Single-source mode still works using legacy `source_x`, `source_y`, `source_z` parameters
- **Per-source configuration**: Each source can have its own:
  - Location (x, y, z)
  - Emission rate [units/s]
  - Emission duration [s]
  - Source type (point, line, area, volume)
  - Stack parameters (for Phase 2.2)

### CSV Format: sources.csv

**Columns:**
```
source_id, x, y, z, emission_rate, emission_duration, type, stack_diameter, stack_exit_velocity, stack_exit_temperature
```

**Example:**
```csv
source_id,x,y,z,emission_rate,emission_duration,type,stack_diameter,stack_exit_velocity,stack_exit_temperature
point_source_1,150.0,150.0,10.0,1.0,100.0,point,0.0,0.0,298.15
stack_source_2,250.0,150.0,20.0,2.0,200.0,stack,1.5,8.0,350.0
point_source_3,100.0,250.0,15.0,0.5,150.0,point,0.0,0.0,298.15
```

### Configuration (inputs.i)

```
# Enable multi-source mode
sources_file = "sources.csv"

# Backward compatibility: use legacy parameters if sources_file not specified
# source_x = 150.0
# source_y = 150.0
# source_z = 10.0
```

### Usage Example

```bash
./puff_solver inputs_multisource.i
```

---

## 2.2 Stack Aerodynamic Modeling (Phase 2.2)

### Overview
Implements Briggs stack tip downwash (STD) model to account for reduced plume rise when stacks have high exit velocities relative to ambient wind speed.

### Features
- **Stack downwash model**: Computes velocity deficit at stack exit based on:
  - Stack diameter [m]
  - Stack exit velocity [m/s]
  - Ambient wind speed [m/s]
  - Atmospheric stability class
- **Physical basis**: Briggs formula relating stack Froude number to downwash
- **Optional**: Can be disabled (default)

### Stack Parameters

Add to each source in sources.csv:
- `stack_diameter` [m]: Stack outer diameter (0 = no stack)
- `stack_exit_velocity` [m/s]: Exit velocity from stack
- `stack_exit_temperature` [K]: Exit temperature (optional, for future use)

### Briggs Downwash Function

```cpp
Real compute_briggs_stack_downwash(
    Real stack_diameter,        // Stack diameter [m]
    Real stack_exit_velocity,   // Exit velocity [m/s]
    Real wind_speed,            // Ambient wind speed [m/s]
    int stability_class = 3);   // Stability class (0=A, 1=B, ..., 5=F)
```

**Inputs:**
- Stack Froude number: Fr = Vs² / (g·D)
- Stability-dependent downwash factor

**Output:**
- Downwash velocity deficit [m/s] (positive value)
- Applied as: effective_z = max(z_source, z_with_plume_rise - downwash)

### Configuration (inputs.i)

```
# Stack aerodynamic modeling
stack_tip_downwash_enabled = true
briggs_std_model = true  # Currently only model available
```

### Example: Stack Source

sources.csv:
```csv
source_id,x,y,z,emission_rate,emission_duration,type,stack_diameter,stack_exit_velocity,stack_exit_temperature
coal_plant,200.0,100.0,50.0,100.0,3600.0,stack,2.0,15.0,450.0
```

inputs.i:
```
stack_tip_downwash_enabled = true
```

### Physical Model Details

The Briggs STD model computes:

1. **Stack Froude number**: Fr = Vs² / (g·D)
   - High Fr (>1): Strong buoyancy, minimal downwash
   - Low Fr (<1): Passive plume, significant downwash

2. **Downwash factor**: Depends on stability
   - Stable (E, F): 0.7 (more downwash)
   - Neutral (D): 0.5 (default)
   - Unstable (A, B): 0.3 (less downwash)

3. **Downwash velocity**: w_downwash ≈ factor × U_wind
   - Limited to 95% of exit velocity

---

## 2.3 Multiple Meteorological Profiles (Phase 2.3)

### Overview
Supports spatially-varying meteorological profiles that automatically interpolate based on puff location. Allows different wind speeds and diffusivities in different regions of the domain.

### Features
- **Spatial profiles**: Different wind/diffusivity profiles at different (x,y) locations
- **Height interpolation**: Linear interpolation of wind and K between levels
- **Stability classification**: Each profile can have different stability class
- **Backward compatibility**: Single uniform profile if not specified

### CSV Format: met_profiles.csv

**Columns:**
```
profile_id, x_ref, y_ref, z_agl, u, v, w, K_h, K_v, stability_class
```

**Data Format (long format):**
- One row per height level
- Same `profile_id` appears multiple times with different `z_agl` values
- Heights should be in ascending order

**Example:**
```csv
profile_id,x_ref,y_ref,z_agl,u,v,w,K_h,K_v,stability_class
profile_west,50.0,150.0,0.0,0.0,0.0,0.0,0.5,0.1,D
profile_west,50.0,150.0,10.0,8.0,0.5,0.0,1.0,0.3,D
profile_west,50.0,150.0,50.0,10.5,1.0,0.0,2.0,0.8,D
profile_east,250.0,150.0,0.0,0.0,0.0,0.0,0.3,0.05,F
profile_east,250.0,150.0,10.0,7.5,0.2,0.0,0.8,0.2,F
profile_east,250.0,150.0,50.0,9.0,0.5,0.0,1.5,0.5,F
```

### Configuration (inputs.i)

```
# Enable spatial meteorological profiles
met_profile_file = "met_profiles.csv"
enable_spatial_met = true

# If enable_spatial_met = false, uses uniform K_h and K_v
```

### Spatial Interpolation

**Profile Selection:**
- Puff location is compared to each profile's (x_ref, y_ref)
- Nearest profile is selected (nearest-neighbor in horizontal)

**Height Interpolation:**
- Linearly interpolates between adjacent height levels in selected profile
- Extrapolates constant values above/below profile levels

### Functions

```cpp
// Select profile by horizontal position
bool select_met_profile_by_position(
    Real x, Real y,                          // Puff position
    const std::vector<MetProfile>& profiles,
    const MetProfile*& prof_out);

// Interpolate wind and K at height z
bool interpolate_met_profile(
    Real z_agl,                              // Height above ground level
    const MetProfile& profile,
    Real& u_out, Real& v_out, Real& w_out,
    Real& K_h_out, Real& K_v_out);
```

### Example: Two-Region Domain

**Scenario:**
- Western region (x<150): Stable conditions (class F) with lower winds
- Eastern region (x>150): Neutral conditions (class D) with higher winds

**files:**
- `met_profiles.csv`: Two profiles with opposite stability classes
- `sources.csv`: Sources placed in both regions

**Result:**
- Western sources experience lower diffusivity (stable)
- Eastern sources experience higher diffusivity (neutral)

---

## Implementation Details

### Data Structures

#### Source (Phase 2.1)
```cpp
struct Source {
    std::string source_id;           // Identifier
    Real x, y, z;                    // Position [m]
    Real emission_rate;              // Rate [units/s]
    Real emission_duration;          // Duration [s]
    std::string type;                // Type: "point", "line", "area", "volume"
    
    // Stack parameters (Phase 2.2)
    Real stack_diameter;             // Stack diameter [m]
    Real stack_exit_velocity;        // Exit velocity [m/s]
    Real stack_exit_temperature;     // Exit temperature [K]
};
```

#### MetProfile (Phase 2.3)
```cpp
struct MetProfile {
    std::string profile_id;          // Identifier
    Real x_ref, y_ref;               // Reference location [m]
    std::vector<Real> z_agl;         // Heights [m]
    std::vector<Real> u, v, w;       // Wind components [m/s]
    std::vector<Real> K_h, K_v;      // Diffusivities [m²/s]
    std::string stability_class;     // Stability (A-F)
};
```

### CSV Readers

#### read_sources_csv()
- Reads multiple sources from CSV file
- Validates required columns
- Trims whitespace in field values
- Skips comment lines starting with #
- Maintains backward compatibility

#### read_met_profiles_csv()
- Reads profile records and aggregates by profile_id
- Supports long format with multiple rows per profile
- Sorts heights in ascending order
- Validates format and number of columns

### Backward Compatibility

**Single-source mode (default):**
```
# If sources_file not specified, uses legacy parameters:
source_x = 150.0
source_y = 150.0
source_z = 10.0
emission_rate = 1.0
emission_duration = 100.0
```

**Uniform meteorology (default):**
```
# If met_profile_file not specified, uses constant K_h and K_v:
K_h = 1.0
K_v = 0.5
```

---

## Testing & Validation

### Test Case 1: Single-source backward compatibility
- Use legacy parameters without sources.csv
- Verify results match Phase 1 output

### Test Case 2: Multi-source emission
- Use sources.csv with 3+ sources
- Verify puffs emitted from all locations
- Check separate masses for each source

### Test Case 3: Stack downwash
- Source with large stack diameter and high exit velocity
- Enable stack_tip_downwash_enabled = true
- Verify effective source height reduced by downwash

### Test Case 4: Spatial meteorological profiles
- Two profiles with opposite stability classes
- Measure diffusivity changes based on puff location
- Verify wind interpolation at different heights

### Test Case 5: Combined (all features)
- Multiple sources with some having stacks
- Spatial met profiles
- Verify concentration field shows correct effects

---

## References

1. Briggs, G.A. (1975). Plume Rise Predictions. In Lectures on Air Pollution and Its Effects, ed. D.A. Haugen, U.S. Dept. of Commerce.

2. Hanna, S.R., et al. (1982). Handbook on Atmospheric Diffusion. DOE/TIC-11223, U.S. Department of Energy.

3. Pasquill, F., & Gifford, F.A. (1961). The estimation of the dispersion of wind-borne material from industrial sources. Meteorological Magazine.

---

## Future Enhancements

1. **Stack exit effects**: Include initial plume rise from buoyancy and momentum
2. **Receptor-specific profiles**: Different meteorology for different receptor zones
3. **Time-varying profiles**: Profiles that change with time (hourly, diurnal cycles)
4. **Complex source types**: Better support for area and volume sources
5. **Downwash model selection**: Additional downwash models (METEROA, etc.)

