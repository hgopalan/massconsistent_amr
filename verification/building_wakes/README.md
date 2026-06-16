# AIJ Benchmark Test Cases

This directory contains **AIJ (Architectural Institute of Japan) Isolated Building Benchmark Cases** based on the **Yoshie et al. (2007) Cooperative project on CFD prediction of pedestrian wind environment in the built environment**.

## Cases

### **Case A: Isolated Square Building (1:1:1 aspect ratio)**
- **Location:** `case_aij_a_square/`
- **Building:** H=20m, W=20m, L=20m (cubic)
- **Purpose:** Symmetric wake baseline - no aspect-ratio effects
- **Reference:** Yoshie et al. (2007), symmetric geometry validation

**Files:**
- `inputs_baseline.i` - Standard 3H linear wake model
- `inputs_enhanced.i` - Extended 15H far-wake + Gaussian profile
- `terrain.csv`, `buildings.csv` - Geometry definitions
- `README.md` - Detailed case documentation

**Expected Metrics:**
- Upstream stagnation: Minimal (square geometry)
- Cavity length: Lr = 0.9 × H = 18m
- Recovery distance: ~3H = 60m
- Downstream extent: 3H for baseline, 15H for enhanced

---

### **Case B: Isolated Tall Narrow Building (3:1:1 aspect ratio)**
- **Location:** `case_aij_b_tall_narrow/`
- **Building:** H=30m, W=10m, L=10m (tall and narrow)
- **Purpose:** Aspect-ratio dependent cavity and corner effects
- **Reference:** Yoshie et al. (2007), Gowardhan et al. (2011)

**Files:**
- `inputs_baseline.i` - Standard 3H linear wake model
- `inputs_enhanced.i` - Aspect-ratio correction + corner acceleration
- `terrain.csv`, `buildings.csv` - Geometry definitions
- `README.md` - Detailed case documentation

**Expected Metrics:**
- Upstream stagnation: 5m (0.5×min(H,W))
- Cavity length: Lr = 0.9 × H × f(H/W) = 27 × f(3.0)
- Recovery distance: ~3H = 90m
- Corner acceleration: ~20% speedup at sides
- Aspect-ratio scaling: H/W = 3.0 triggers correction

---

## Running the Cases

### Prerequisites
```bash
# Build the wind_solver
cd /path/to/massconsistent_amr
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Run Individual Case
```bash
cd verification/building_wakes/case_aij_a_square
/path/to/wind_solver inputs_baseline.i
/path/to/wind_solver inputs_enhanced.i
```

### Run All Cases with Verification
```bash
cd verification/building_wakes
python3 run_aij_verification.py
```

This script will:
1. Build wind_solver (if needed)
2. Run baseline and enhanced simulations for both cases
3. Extract velocity field data to CSV
4. Verify metrics against literature reference values
5. Generate comprehensive verification report

---

## Verification Metrics

Each case verifies three key wake dimensions:

### 1. **Upstream Stagnation Zone Length**
- **Measure:** Distance upstream where flow is reduced
- **Case A:** Minimal (square building)
- **Case B:** ~5m (0.5×min(H,W))

### 2. **Cavity/Top Wake Length**
- **Measure:** From building trailing edge to cavity exit
- **Case A:** Lr = 18m (0.9H)
- **Case B:** Lr = 27m (0.9H) scaled by aspect ratio

### 3. **Downstream Wake Recovery Length**
- **Measure:** Distance to full recovery
- **Case A:** 60m (3H baseline), 300m (15H enhanced)
- **Case B:** 90m (3H baseline)

---

## Literature References

### Primary Sources
- **Yoshie, R., Mochida, A., Tominaga, Y., Kataoka, H., Harimoto, K., Nozu, T., & Shirasawa, T. (2007)**
  - *Cooperative project on CFD prediction of pedestrian wind environment in the built environment*
  - Journal of Wind Engineering and Industrial Aerodynamics, 95(12), 1551–1578
  - Multi-institution CFD benchmark comparing different codes against wind tunnel data

- **Gowardhan, A., Brown, M. J., Pardyjak, E. R., & Norgren, H. (2011)**
  - *Evaluation of a fast and simple obstruction modeling approach for use in urban wind resource estimation*
  - Wind Engineering, 35(6), 697–713
  - Aspect-ratio dependent cavity length and corner acceleration effects

- **Pardyjak, E. R., & Brown, M. J. (2001)**
  - *QUIC-URB v. 1.1: Theory and User's Guide*
  - Los Alamos National Laboratory, LA-UR-01-4228
  - Foundational work on mass-consistent urban wind solvers

### Related References
- Brown et al. (2000): Wind tunnel 2D building arrays (MUST experiment basis)
- Stathopoulos (1988): Urban canyon vortex structures
- Britter & Hanna (2003): Urban flow and dispersion
- Blocken & Carmeliet (2004): Pedestrian wind environment

---

## Output Files

### Simulations Generate:
- `plt_aij_case_*_baseline/` - AMReX plot files (full 3D solution)
- `plt_aij_case_*_enhanced/` - AMReX plot files (full 3D solution)
- `case_aij_*_extract_baseline.csv` - Extracted wind field (baseline)
- `case_aij_*_extract_enhanced.csv` - Extracted wind field (enhanced)

### CSV Format:
Each extraction file contains:
```
x,y,z,u,v,w
[downwind dist] [lateral] [height] [u-velocity] [v-velocity] [w-velocity]
```

---

## Physical Interpretation

### Case A: Square Building
- **Symmetry:** Perfect lateral symmetry (y-direction)
- **Isotropy:** No preferred flow direction horizontally
- **Aspect ratio:** 1:1:1 → no correction needed
- **Use:** Baseline symmetric wake validation
- **Validation focus:** Cavity symmetry, linear recovery, Gaussian spreading

### Case B: Tall Narrow Building
- **Asymmetry:** High aspect ratio (3:1) induces corrections
- **Anisotropy:** Different lateral vs. streamwise scales
- **Corner effects:** Sideward acceleration at building edges
- **Tall building physics:** Reduced cavity zone due to aspect ratio
- **Use:** Advanced wake physics validation
- **Validation focus:** Aspect-ratio correction, corner speedup, anisotropic deficit

---

## Expected Results Summary

| Metric | Case A Baseline | Case A Enhanced | Case B Baseline | Case B Enhanced |
|--------|-----------------|-----------------|-----------------|-----------------|
| Upstream stagnation (m) | 0 | 0 | ~5 | ~5 |
| Cavity length (m) | 18 | 18 | 27 | 27×f(3.0) |
| Recovery distance (m) | 60 | 300 | 90 | 90 |
| Cavity deficit (%) | ~5 | ~8 | ~10 | ~13 |
| Corner acceleration (%) | 0 | 0 | 0 | ~20 |
| Farwake persistence | 3H | 15H | 3H | Modified |

---

## Troubleshooting

### Case fails to run
1. Check `wind_solver` exists: `which wind_solver` or `./build/wind_solver --version`
2. Verify input files: `inputs_baseline.i`, `inputs_enhanced.i`
3. Check terrain/building CSV files are readable
4. Check current directory matches case directory

### Unexpected velocity values
1. Verify reference wind U_ref = 10.0 m/s
2. Check extraction height (AGL) matches case documentation
3. Confirm building geometry in buildings.csv matches README
4. Check log-law profile is enabled (z_ref = 10m, z0 = 0.1m)

### Missing output files
1. Verify output file paths in inputs file
2. Check write permissions in case directory
3. Ensure plot_file and extract_file names are unique per simulation

---

## Future Extensions

These cases can be extended to study:
- Grid resolution sensitivity (coarsen/refine dx, dy, dz)
- Wind direction effects (vary wind angle, add V_ref)
- Canopy effects (add vegetation roughness)
- Multiple building interactions (array configurations)
- Atmospheric stability effects (use Monin-Obukhov profiles)
- Integration with turbulence models

---

**Last Updated:** 2026-06-16
**Status:** Ready for verification testing
**Expected Runtime:** ~30 minutes total (both cases, baseline + enhanced)
