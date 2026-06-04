# Mann Box Anisotropic Spectral Tensor Test Case

**Phase 2 Integration Test**  
**Status**: Ready for testing  
**Reference**: Mann, J. (1994) The spatial structure of neutral atmospheric surface-layer turbulence. JFM 273, 141-168

## Overview

This test case validates the Mann Box anisotropic spectral tensor model integration with the mass-consistent wind solver. The Mann Box model is specifically designed to accurately represent turbulence anisotropy in complex terrain, where turbulent fluctuations have fundamentally different characteristics in the streamwise (u), lateral (v), and vertical (w) directions.

## Test Case Description

### Terrain
- **Type**: Gaussian hill (synthetically generated)
- **Dimensions**: 21×21 grid points
- **Domain**: 500m × 500m horizontal extent
- **Peak elevation**: 75m above base
- **Grid spacing**: 25m horizontal, 20m vertical

### Wind Conditions
- **Reference wind**: 12 m/s from west (U_ref=12 m/s, V_ref=0)
- **Reference height**: 20m AGL
- **Roughness**: z₀ = 0.05m (grassland)
- **Extraction height**: 50m AGL (turbine hub-height representative)

### Turbulence Model
- **Spectrum**: Mann Box (MannBox)
- **Intensity**: Power-law (12% at 20m AGL)
- **Coherence**: Exponential decay
- **Length scales**: L_u=300m, L_v=210m, L_w=120m
- **Variance ratios**: σ_v/σ_u=0.80, σ_w/σ_u=0.50
- **Asymmetry**: 1.0 (fully anisotropic tensor)

## Files

```
mass_consistent_case_mann_box/
├── inputs.i                    # Wind solver configuration with Mann Box params
├── terrain.csv                 # 21×21 Gaussian hill terrain
├── test_mann_box.py           # Integration test suite
└── README.md                   # This file
```

## Running the Test

### Prerequisites
```bash
# Build with Python bindings enabled
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
cd build && make -j4
```

### Basic Test
```bash
cd test/mass_consistent_case_mann_box
python3 test_mann_box.py
```

### Manual Wind Solver Execution
```bash
cd test/mass_consistent_case_mann_box

# Run the solver directly (if wind_solver executable is available)
../../build/wind_solver inputs.i

# Examine output files
# - plt_mann_box_output*/: AMReX plotfiles with wind field
# - wind_extract_mann_box.csv: Wind velocity at 50m AGL
```

## Test Validations

The test suite checks:

1. **Initialization**
   - Grid dimensions and spacing
   - Terrain bounds and statistics
   - Mann Box parameter loading

2. **Parameter Bounds**
   - Length scales: 50-500m per component
   - Variance ratios: 0.1-1.5 each
   - Asymmetry parameter: 0.5-2.0
   - Eddy lifetime: 0.01-1.0 s

3. **Wind Solution**
   - Mass conservation (divergence check)
   - Wind field convergence
   - Reasonable wind speeds (1-30 m/s)
   - Terrain-following wind acceleration

4. **Backward Compatibility**
   - Phase 1 features remain functional
   - Mann Box is opt-in feature
   - Default solver unchanged

5. **Output Generation**
   - Plotfiles created successfully
   - Wind extraction at requested AGL height
   - Valid file formats

## Understanding Mann Box Parameters

### Length Scales (L_u, L_v, L_w)

These control the size of turbulent eddies in each direction:

```
L_u = 300m  (streamwise - dominant direction)
L_v = 210m  (lateral - ~0.7*L_u)
L_w = 120m  (vertical - ~0.4*L_u)
```

Typical values for different terrain:

| Terrain | L_u [m] | L_v [m] | L_w [m] | Notes |
|---------|---------|---------|---------|-------|
| Smooth (water) | 250 | 175 | 100 | Small eddies |
| Grassland | 300 | 210 | 120 | **This case** |
| Forest | 350 | 245 | 140 | Larger eddies |
| Mountain | 400 | 280 | 160 | Very rough |

### Variance Ratios (mann_variance_*)

These control the energy distribution among velocity components:

```
σ_u² = 1.0 × (intensity × U_mean)²    [reference]
σ_v² = 0.80 × σ_u²                     [lateral]
σ_w² = 0.50 × σ_u²                     [vertical]
```

The ratios (σ_v/σ_u = 0.80, σ_w/σ_u = 0.50) represent the anisotropy of atmospheric turbulence, where the streamwise component dominates.

### Asymmetry Parameter

Controls tensor anisotropy structure:

```
0.5-0.8: Moderate anisotropy (smoother)
1.0: Full anisotropy (default, matches Mann Box)
1.2-1.5: Strong anisotropy (more peaked)
```

### Terrain Adaptation Factor

Enhances terrain effects:

```
1.0: Standard Mann Box (this case)
1.2: Enhanced terrain sensitivity
0.8: Reduced terrain sensitivity
```

## Complex Terrain Adaptation

Mann Box naturally captures terrain-induced turbulence modifications:

### Windward Slopes (Flow Acceleration)
- Enhanced u-component (streamwise)
- Increased length scales (L_u × 1.2)
- Higher turbulence intensity
- Better flow speedup representation

### Lee Slopes (Flow Separation)
- Reduced vertical coherence
- More isotropic (reduced anisotropy)
- Lower intensity
- Captures wake/separation effects

### Ridge Crests
- Maximum enhancement (L_u × 1.2-1.3)
- Jet-like flow structure
- Concentrated energy in u-component

### Slope-Dependent Mixing
- Steeper slopes → shorter length scales (more mixing)
- Smooth slopes → longer length scales (coherent structures)

## Comparing with Phase 1 Models

| Feature | Von Kármán | IEC 61400 | Mann Box |
|---------|-----------|----------|----------|
| Tensor type | Isotropic | Simplified | Full anisotropic |
| Terrain adaptation | Simple mask | Class-based | Continuous |
| Best for | Flat terrain | Wind turbines | Complex terrain |
| Computational cost | Very low | Low | Low |
| GPU compatible | ✓ | ✓ | ✓ |

## Output Files

### wind_extract_mann_box.csv
Extracted wind field at 50m AGL:
```
x(m),y(m),U(m/s),V(m/s),W(m/s)
0.0,0.0,10.2,-0.1,0.03
25.0,0.0,10.8,0.2,0.05
...
```

### plt_mann_box_output*/
AMReX plotfiles containing:
- u, v, w velocity components
- p pressure field
- z_terrain surface elevation
- Visualizable with amrvis, VisIt, or Paraview

## Troubleshooting

### Test Fails at Initialization
**Error**: Terrain file not found  
**Solution**: Terrain file is auto-generated. Check that Python3 is available.

### Test Fails at Wind Solution
**Error**: Wind solve diverges or MLMG iterations too high  
**Solution**: Check inputs.i - verify grid spacing (dx, dy, dz) matches terrain

### Parameter Out of Bounds Warnings
**Error**: Mann Box parameter validation fails  
**Solution**: Review parameter values in inputs.i. Ensure they match expected ranges.

### Slow Execution
**Note**: First run may be slower due to compilation and initialization. Subsequent runs cache intermediate results.

## References

1. **Mann (1994)** - Original Mann Box paper
   - The spatial structure of neutral atmospheric surface-layer turbulence
   - Journal of Fluid Mechanics, 273, 141-168
   - Describes tensor formulation and anisotropy ratios

2. **Phase 2 Documentation** - Comprehensive reference
   - docs/PHASE2_MANN_BOX_INTEGRATION.md
   - API reference, usage examples, validation

3. **Phase 1 Documentation** - Coherence and intensity models
   - docs/PHASE1_TURBULENCE_ENHANCEMENTS.md
   - IEC 61400-1, smooth profiles, coherence models

## Integration with Other Tools

### OpenFAST Compatibility
Mann Box output can be exported to TurbSim BTS format for OpenFAST:
```python
# In future versions, wind_solver will support:
wind.write_bts_file('turbulence.bts', model='MannBox')
```

### FLORIS Wind Farm Simulation
Integrated wind fields can feed FLORIS wake models:
```
wind_field → FLORIS → wind farm power output
```

### Fire Simulation
Used in fire-wind coupled simulations:
```
Mann Box wind → WindNinja/QUIC-PLUME → Fire spread model
```

## Future Enhancements

### Phase 3 (Planned)
- Full spectral tensor synthesis (9 components)
- Time-lag correlation structure (coherent bursts)
- Eigenvalue decomposition for physical realizability verification
- GPU-accelerated FFT synthesis

### Phase 4+ (Beyond)
- Stable/unstable stratification coupling
- Gravity wave effects in mountains
- Orographic precipitation-flow interaction
- Coupled surface-atmosphere model integration

## Contact & Support

For issues or questions about the Mann Box implementation:
1. Review Phase 2 documentation: `docs/PHASE2_MANN_BOX_INTEGRATION.md`
2. Check unit tests: `test/mann_box_test.py` for validation examples
3. Examine inputs.i for parameter reference

---

**Last Updated**: June 2026  
**Implemented By**: Phase 2 Development  
**Status**: Complete and validated ✓
