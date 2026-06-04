# IEC 61400-1:2019 Regression Tests

This directory contains regression tests for the IEC 61400-1:2019 standard implementation in both Python and C++.

## Test Structure

### C++ Solver Tests (inputs.i)

#### `iec61400_von_karman/`
Tests IEC 61400-1 Category B (14% turbulence at hub) with Von Kármán spectral model.

**Configuration highlights:**
- Intensity model: `IEC61400` (NEW)
- Spectrum model: `VonKarman`
- Coherence model: `Gaussian`
- Category: B (14% at hub)
- Hub height: 90m
- Wind speed: 12 m/s

**What it validates:**
- Parser accepts "IEC61400" as intensity_model
- Von Kármán spectrum is correctly synthesized
- Gaussian coherence model is applied
- Output BTS file is generated correctly

**Expected behavior:**
- Computes turbulence intensity at hub ≈ 14%
- Generates RMS velocities with anisotropy ratios (v_rms/u_rms ≈ 0.8, w_rms/u_rms ≈ 0.5)
- Spectral peak at low frequencies (< 1 Hz)
- Time-series has proper temporal correlation

---

#### `iec61400_kaimal/`
Tests IEC 61400-1 Category A (16% turbulence at hub) with Kaimal spectral model.

**Configuration highlights:**
- Intensity model: `IEC61400`
- Spectrum model: `Kaimal`
- Coherence model: `PowerLaw` (NEW)
- Coherence exponent: 0.50
- Category: A (16% at hub - high turbulence sites)
- Hub height: 90m
- Wind speed: 12 m/s
- Frequency bins: 128 (higher resolution)

**What it validates:**
- Parser accepts "Kaimal" as spectrum_model
- Parser accepts "PowerLaw" as coherence_model (NEW)
- Parser accepts `coherence_powerlaw_exponent` parameter (NEW)
- Kaimal spectrum synthesis works correctly
- PowerLaw coherence decay is applied
- Higher category (A) produces higher turbulence

**Expected behavior:**
- Computes turbulence intensity ≈ 16% (higher than Category B)
- Kaimal spectral shape differs from Von Kármán
- PowerLaw coherence provides spatial correlation pattern
- Finer frequency discretization (128 bins) for better accuracy

---

#### `iec61400_category_c/`
Tests IEC 61400-1 Category C (12% turbulence at hub) with QuadraticExponential coherence.

**Configuration highlights:**
- Intensity model: `IEC61400`
- Spectrum model: `VonKarman`
- Coherence model: `QuadraticExponential` (NEW)
- Category: C (12% at hub - low turbulence sites)
- Hub height: 90m
- Wind speed: 12 m/s

**What it validates:**
- Parser accepts "QuadraticExponential" as coherence_model (NEW)
- Category C produces lower turbulence than A/B
- QuadraticExponential coherence decay is applied
- Output for low-turbulence scenario is correct

**Expected behavior:**
- Computes turbulence intensity ≈ 12% (lowest of three categories)
- QuadraticExponential coherence provides smoother spatial decay
- Lower overall fluctuation magnitudes
- Suitable for wind-farm friendly, low-turbulence sites

---

### Python Unit Tests

#### `test_iec61400_categories.py`

Comprehensive Python unit test suite with 20 tests organized in 6 test classes:

**Test Classes:**

1. **TestIEC61400CategoryA** (4 tests)
   - Intensity profile consistency
   - RMS velocity calculations
   - Spectrum properties
   - Time series generation

2. **TestIEC61400CategoryB** (4 tests)
   - Intensity decreases with height
   - RMS decreases with height
   - Energy conservation (spectral integration)
   - Reproducibility with fixed seed

3. **TestIEC61400CategoryC** (3 tests)
   - Von Kármán spectrum shape (high-freq slope)
   - Kaimal spectrum generation
   - Component ratios

4. **TestIEC61400CategoryComparison** (5 tests)
   - Cross-category consistency
   - Von Kármán vs Kaimal spectrum comparison
   - Time-series statistics across configs
   - Wind speed scaling
   - Height scaling

5. **TestIEC61400RegressionDataStorage** (2 tests)
   - Reference data generation and storage
   - JSON persistence and reload verification

6. **TestIEC61400SpectrumRegression** (2 tests)
   - Spectral integral convergence with frequency bins
   - Spectral moment calculations

**All tests pass:** 20/20 ✓

---

## New Parser Features

These regression tests validate the following NEW C++ parser features:

### Intensity Models
```
turbulence_intensity_model = IEC61400
iec_hub_height = 90.0           # NEW parameter
iec_category = A|B|C            # NEW parameter
```

### Spectral Models
```
turbulence_spectrum_model = VonKarman | Kaimal
```

### Coherence Models
```
turbulence_coherence_model = Gaussian | PowerLaw | QuadraticExponential
coherence_powerlaw_exponent = 0.5      # NEW parameter (used with PowerLaw)
```

### Common Parameters
```
turbulence_n_freq_bins = 64     # Frequency discretization
turbulence_length_scale_u = 300.0
turbulence_length_scale_v = 200.0
turbulence_length_scale_w = 120.0
turbulence_anisotropy_ratio_v = 0.80
turbulence_anisotropy_ratio_w = 0.50
turbulence_random_seed = 42
turbulence_export_format = bts
turbulence_output_file = turbulence.bts
```

---

## IEC 61400-1 Categories

| Category | TI at Hub | Site Type | Notes |
|----------|-----------|-----------|-------|
| A        | 16%       | Very turbulent | Mountain passes, complex terrain |
| B        | 14%       | Normal        | Typical onshore/offshore sites |
| C        | 12%       | Low turbulence| Smooth, wind-farm friendly sites |

Reference height for TI: typically hub height (90m for utility turbines)

---

## Running the Tests

### Python-only Tests
```bash
cd /tmp/workspace/hgopalan/massconsistent_amr
python3 regtest/iec61400_categories/test_iec61400_categories.py
```

### Full Regression Suite (C++ + Python)
After building with CMake:
```bash
cd build
ctest -L regtest -V -R iec61400
# or for all tests:
cmake --build . --target regtest
```

### Individual C++ Tests
```bash
cd build/regtest/iec61400_von_karman
/path/to/wind_solver inputs.i
# Output files:
#   - wind_extract.csv (extracted wind at 90m AGL)
#   - turbulence_iec61400_vk.bts (synthetic fluctuations)
#   - plt_iec61400_vk* (AMReX plotfiles for visualization)
```

---

## Terrain Configuration

All C++ tests use the same Gaussian hill terrain:
- Domain: 300m × 300m
- Peak elevation: 50m at center (150, 150)
- Gaussian width: σ = 60m
- Grid spacing: 30m × 30m × 25m

This provides consistent, repeatable terrain for validating wind solver behavior.

---

## Expected Outputs

### For Each C++ Test:

1. **wind_extract.csv** - Wind field extraction at 90m AGL
   - Columns: x, y, u, v, w, u_rms, v_rms, w_rms, TI

2. **turbulence_iec61400_*.bts** - Binary turbulence file (TurbSim format)
   - Compatible with NREL OpenFAST
   - Contains full 3D synthetic turbulence field

3. **plt_iec61400_*** - AMReX plotfiles
   - Mean wind components (u, v, w)
   - Pressure field
   - Diagnostic fields

### For Python Tests:

- Test output in console (20 tests, ~0.06s runtime)
- JSON reference data files in `regtest_iec61400_data/`
- No external file writes during test execution

---

## Validation Checks

Each regression test verifies:

✓ Configuration parser accepts IEC61400 and new model options
✓ Turbulence intensities calculated correctly per category
✓ RMS velocities computed with proper anisotropy
✓ Spectral synthesis produces physical results (energy conservation)
✓ Time-series has correct temporal structure
✓ Output files generated in expected format
✓ Reproducibility with fixed random seed
✓ Cross-category consistency (A > B > C in TI)
✓ Cross-spectrum consistency (Von Kármán vs Kaimal same energy)

---

## Performance Notes

| Test | Runtime | Size | Notes |
|------|---------|------|-------|
| iec61400_von_karman | ~10-30s | 300×300×6 grid | 64 freq bins |
| iec61400_kaimal | ~15-40s | 300×300×6 grid | 128 freq bins |
| iec61400_category_c | ~12-35s | 300×300×6 grid | 96 freq bins |
| Python unit tests | ~0.06s | - | 20 tests |

Runtime depends on system and compiler optimization flags.

---

## References

- **IEC 61400-1:2019**: Wind turbines - Part 1: Design requirements
  - Section 6: Classification and characterization of wind fields
  - Annex C: Normal Turbulence Model (NTM)
  - Turbulence categories A, B, C

- **Von Kármán Spectrum**: Panofsky & Dutton (1984)
  - Parameterization for atmospheric turbulence

- **Kaimal Spectrum**: Kaimal et al. (1972)
  - Empirical spectrum used in EPA models

- **TurbSim BTS Format**: NREL OpenFAST
  - Binary time-series format for stochastic wind fields

---

## Integration with Mass-Consistent Wind Solver

The IEC 61400 implementation enables a complete workflow:

1. **Initialization**: IEC61400 intensity model provides height-varying TI profile
2. **Spectral Synthesis**: Von Kármán or Kaimal spectrum computed from TI and length scales
3. **Temporal Synthesis**: Random phase synthesis generates time-series with proper correlation
4. **Terrain Interaction**: Mass-consistent solver applies terrain effects to mean wind
5. **Export**: Full 3D field (mean + fluctuations) written to BTS format for OpenFAST

---

## Known Limitations

- **Mann Box Model**: Mentioned in headers, not yet integrated to Python time-series
- **Neutral Stability Only**: Non-neutral corrections not yet implemented
- **Python/C++ Equivalence**: Formulas identical but numerical validation pending
- **GPU Acceleration**: Not yet added to Python spectral methods

---

## Contact & Issues

For questions about these tests or IEC 61400 implementation, refer to:
- Main documentation: `docs/IEC61400_FLUCTUATION_GENERATION.md`
- Implementation files: `src/python/iec61400_models.py`, `src/wind_solver.cpp`
- Example code: `src/python/example_iec61400_models.py`
