# PHREEQC Coupling Troubleshooting Guide

Common issues, diagnostic procedures, and resolution steps.

---

## Installation and Setup Issues

### Import Errors

**Error:** `ModuleNotFoundError: No module named 'phreeqc_coupling'`

**Diagnosis:**
```bash
python3 -c "import sys; print(sys.path)"
python3 -c "import massconsistent_amr; print(massconsistent_amr.__file__)"
```

**Solutions:**
1. Verify massconsistent_amr installation:
   ```bash
   cd massconsistent_amr
   python3 -c "from wind_solver import WindSolver"
   ```

2. Add to Python path if necessary:
   ```bash
   export PYTHONPATH="/path/to/massconsistent_amr/src/python:$PYTHONPATH"
   ```

3. Reinstall with Python bindings:
   ```bash
   cmake -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
   cmake --build build
   ```

---

**Error:** `ImportError: netcdf4 not installed`

**Solution:**
```bash
pip install netcdf4

# If installation fails, may need system dependencies:
# Ubuntu/Debian:
sudo apt-get install libhdf5-dev libnetcdf-dev

# macOS:
brew install netcdf4 hdf5

# Then retry pip install
```

---

### Dependency Version Conflicts

**Error:** `AttributeError: module 'netCDF4' has no attribute 'Dataset'`

**Likely cause:** Old netCDF4 version

**Solution:**
```bash
pip install --upgrade netcdf4

# Or pin specific version
pip install 'netcdf4>=1.5.9'
```

---

## Wind Solver Issues

### Input File Errors

**Error:** `FileNotFoundError: inputs.i not found`

**Diagnosis:**
```bash
ls -la inputs.i
pwd  # Check current working directory
```

**Solutions:**
1. Verify file exists:
   ```bash
   find . -name "inputs.i" -type f
   ```

2. Use absolute path:
   ```bash
   wind = WindSolver("/full/path/to/inputs.i")
   ```

3. Check file permissions:
   ```bash
   chmod 644 inputs.i
   ```

---

**Error:** `ValueError: Invalid parameter in inputs.i`

**Diagnosis:**
```bash
# Check input file format
head -20 inputs.i

# Verify ParmParse syntax
python3 << 'EOF'
from amrex import ParmParse
pp = ParmParse("inputs")
EOF
```

**Solutions:**
1. Check for typos in parameter names
2. Verify units consistency (m vs. km)
3. Validate grid specifications:
   ```
   amr.n_cell_x = 50   # cells
   amr.n_cell_y = 50
   amr.n_cell_z = 20
   domain.lo = 0 0 0
   domain.hi = 5000 5000 2000  # meters
   ```

---

**Error:** `RuntimeError: Terrain DEM file not found or invalid`

**Diagnosis:**
```bash
ls -la /data/topography/dem.nc
ncdump -h /data/topography/dem.nc  # Check netCDF structure
```

**Solutions:**
1. Verify DEM file path (absolute vs. relative)
2. Check DEM grid matches solver domain:
   ```python
   import netCDF4
   ds = netCDF4.Dataset('/data/topography/dem.nc')
   print(ds.dimensions)  # Should match solver grid
   ```

3. Validate no NaN or invalid values:
   ```python
   import numpy as np
   dem = ds.variables['elevation'][:]
   print(f"DEM range: {np.nanmin(dem):.1f} to {np.nanmax(dem):.1f} m")
   print(f"Invalid values: {np.isnan(dem).sum()}")
   ```

---

### Wind Solver Convergence Issues

**Error:** `Warning: Poisson solver did not converge after 100 iterations`

**Diagnosis:**
```bash
# Check solver settings in inputs.i
grep -i "poisson_solver\|mg_" inputs.i

# Check for discontinuities in terrain or boundary conditions
```

**Solutions:**
1. Increase maximum iterations:
   ```
   poisson_solver.max_iter = 200
   poisson_solver.verbose = 1  # Enable diagnostics
   ```

2. Adjust multigrid settings:
   ```
   poisson_solver.bottom_solver = "bicg"
   poisson_solver.mg_agglomeration = 1
   ```

3. Reduce AMR levels (fewer refinements):
   ```
   amr.max_level = 1  # Instead of 3
   ```

4. Check boundary conditions for conflicts (contradictory pressure gradients)

---

### Memory Issues

**Error:** `MemoryError: Unable to allocate 16.7 GB`

**Diagnosis:**
```bash
python3 << 'EOF'
import tracemalloc
tracemalloc.start()

# Run wind solver
from wind_solver import WindSolver
wind = WindSolver("inputs.i")
wind.solve()

current, peak = tracemalloc.get_traced_memory()
print(f"Current: {current/1e9:.1f} GB")
print(f"Peak: {peak/1e9:.1f} GB")
EOF
```

**Solutions:**
1. Reduce grid resolution:
   ```
   amr.n_cell_x = 30  # Instead of 50
   amr.n_cell_y = 30
   ```

2. Reduce AMR levels:
   ```
   amr.max_level = 1
   ```

3. Enable GPU if available:
   ```bash
   cmake -B build -DMASSCONSISTENT_GPU_BACKEND=CUDA
   ```

4. Increase system swap (temporary measure):
   ```bash
   # Linux: Create swap file
   sudo fallocate -l 32G /swapfile
   sudo chmod 600 /swapfile
   sudo mkswap /swapfile
   sudo swapon /swapfile
   ```

---

## AMD Hotspot Detection Issues

### CSV Format Errors

**Error:** `ValueError: CSV must contain columns: id, x, y, z, discharge_type, description`

**Diagnosis:**
```bash
head -3 amd_sites.csv
```

**Solutions:**
1. Check column names (case-sensitive):
   ```
   id,x,y,z,discharge_type,description  ← Correct
   ID,X,Y,Z,DISCHARGE_TYPE,DESCRIPTION ← Wrong
   ```

2. Verify no extra spaces:
   ```bash
   cat amd_sites.csv | sed 's/, /,/g' > amd_sites_fixed.csv
   ```

3. Ensure valid encoding (UTF-8):
   ```bash
   file amd_sites.csv
   iconv -f ISO-8859-1 -t UTF-8 amd_sites.csv > amd_sites_utf8.csv
   ```

---

**Error:** `ValueError: Coordinates out of domain bounds`

**Diagnosis:**
```python
import pandas as pd
df = pd.read_csv('amd_sites.csv')
print(f"X range: {df['x'].min():.1f} to {df['x'].max():.1f}")
print(f"Y range: {df['y'].min():.1f} to {df['y'].max():.1f}")

# Compare with wind solver domain
from wind_solver import WindSolver
wind = WindSolver("inputs.i")
print(f"Domain: 0 to 5000 (assumed)")
```

**Solutions:**
1. Verify coordinate system matches (UTM vs. local)
2. Check units (meters vs. km)
3. Ensure coordinates are within domain:
   ```python
   df_valid = df[(df['x'] >= 0) & (df['x'] <= 5000) &
                 (df['y'] >= 0) & (df['y'] <= 5000)]
   df_valid.to_csv('amd_sites_valid.csv', index=False)
   ```

---

### Zero or Unrealistic O₂ Supply Rates

**Error:** `All hotspots classified as LOW risk (O₂ supply ≈ 0)`

**Possible causes:**
1. Wind solver produced zero velocities
2. Roughness length z₀ too large
3. Friction velocity calculation error

**Diagnosis:**
```python
from wind_solver import WindSolver
from phreeqc_coupling import FieldExtractor

wind = WindSolver("inputs.i")
wind.solve()

extractor = FieldExtractor(wind)
u_star = extractor.export_friction_velocity()
print(f"Friction velocity: {u_star:.3f} m/s")

if u_star < 0.01:
    print("WARNING: Very low friction velocity")
    print("Check: wind speed, roughness length, solver convergence")
```

**Solutions:**
1. Verify wind solver convergence:
   ```bash
   grep -i "converged\|iteration" wind_solver.log
   ```

2. Check roughness length:
   ```python
   # Should be 0.01-0.1 m for typical terrain
   assert 0.01 <= z0 <= 0.1, f"Unreasonable z0 = {z0}"
   ```

3. Verify boundary conditions (pressure gradient, inflow wind)

---

### GeoJSON Output Issues

**Error:** `ValueError: Invalid GeoJSON feature`

**Diagnosis:**
```bash
python3 << 'EOF'
import json
with open('amd_hotspots.geojson', 'r') as f:
    geojson = json.load(f)
    
for feature in geojson['features']:
    coords = feature['geometry']['coordinates']
    if not (-180 <= coords[0] <= 180 and -90 <= coords[1] <= 90):
        print(f"Invalid coords: {coords}")
EOF
```

**Solutions:**
1. Ensure coordinates are in geographic projection (lat/lon) for GeoJSON
2. Verify coordinate order (lon, lat, not lat, lon)
3. Check for NaN or infinity values

---

## Sulfide Oxidation Issues

### Temperature-Dependent Rate Anomalies

**Error:** `Warning: Oxidation rate very low despite high wind (T = 10°C)`

**Explanation:**
This is physically correct. Oxidation kinetics are strongly temperature-dependent:
- k(T) = A × exp(-E_a/(R×T))
- At 10°C: k ≈ 0.5 × k(25°C)
- At 0°C: k ≈ 0.2 × k(25°C)

**Check assumptions:**
```python
# Verify E_a is correct
E_a = 45000  # J/mol (Nicholson et al. 1990)
R = 8.314

# Manual check
T_winter = 273.15 + 0  # 0°C
T_summer = 273.15 + 20  # 20°C

ratio = np.exp(-E_a/R * (1/T_summer - 1/T_winter))
print(f"Rate ratio (summer/winter): {ratio:.2f}")
# Expected: ~2-3× increase
```

---

**Error:** `ValueError: Negative oxidation rate`

**Cause:** Numerical error at low temperatures or high pH

**Solution:**
```python
# Add bounds checking
rate = max(0, oxidation_rate)  # Clamp to zero if negative
```

---

### Acid Generation Mismatch

**Error:** `Oxidation rates don't match observed AMD flow rates`

**Possible causes:**
1. Site-specific mineral composition unknown
2. Specific surface area incorrect
3. Oxygen availability limited (not wind-limited)

**Calibration procedure:**
```python
# Estimate prefactor A from field observations
# Observed rate: r_obs [mol/(m³·s)]
# Calculate: A_field = r_obs / (exp(-E_a/(R*T)) * [FeS2] * [O2])

# Compare with literature A ≈ 1.0e-8
if A_field > 1e-6:
    print(f"WARNING: Prefactor {A_field:.2e} > 1e-6 (unusual)")
```

---

## Scenario Library Issues

### Library Build Timeout

**Error:** `Timeout: Scenario library build exceeded 3 hours`

**Solutions:**
1. Use parallel processing:
   ```python
   build_scenario_library(n_scenarios=100, parallel=True, n_jobs=8)
   ```

2. Reduce scenario count:
   ```python
   build_scenario_library(n_scenarios=50)  # Faster, less accurate
   ```

3. Pre-compute on high-performance system and transfer file

---

### Scenario Lookup Error

**Error:** `ValueError: No scenarios found within tolerance`

**Diagnosis:**
```python
lib = ScenarioLibrary.load('library.h5')
print(f"Scenarios: {len(lib.scenarios)}")
print(f"Wind speed range: {min(s.u_mag for s in lib.scenarios):.1f} to {max(s.u_mag for s in lib.scenarios):.1f}")

# Try lookup with wider tolerance
try:
    scenario = lib.nearest_scenario(15.0, 270, 280, n_neighbors=5)
    print(f"Found {len(scenario)} neighbors")
except ValueError:
    print("No scenarios match parameters")
```

**Solutions:**
1. Use `n_neighbors > 1` to get multiple candidates
2. Rebuild library with expanded parameter ranges
3. Fall back to full wind solve

---

## Performance and Timing Issues

### Excessive Cycle Time

**Problem:** `Cycle time 1200+ s (>15 min), missing deadlines`

**Diagnosis:**
```bash
# Check bottleneck
grep "Step.*seconds" monitoring.log

# Typical breakdown:
# Wind solve: 600 s
# AMD detection: 0.2 s
# Output: 0.05 s
# Total: ~600 s
```

**Solutions:**
1. **Use scenario library (60× speedup):**
   ```python
   lib = ScenarioLibrary.load('scenario_library/library.h5')
   scenario = lib.nearest_scenario(u_mag, wind_dir, T)
   # Replaces 600 s wind solve with <30 ms lookup
   ```

2. **Reduce wind solver resolution:**
   ```
   amr.n_cell_x = 30  # Instead of 50
   amr.max_level = 1  # Instead of 2
   ```

3. **Disable secondary tasks:**
   ```python
   if compute_available():
       # Skip secondary tasks if compute limited
   ```

---

### Slow Hotspot Detection

**Problem:** `AMD detection takes 5+ seconds for 10 sites`

**Diagnosis:**
```python
import timeit

# Measure Sherwood correlation
time_per_site = timeit.timeit(
    'compute_oxygen_supply_rate(amd_site)',
    number=100
) / 100

print(f"Time per site: {time_per_site*1000:.2f} ms")
print(f"Expected for 10 sites: {time_per_site*10*1000:.0f} ms")
```

**Solutions:**
1. Reduce precision (faster floating-point):
   ```python
   u_star = np.float32(u_star)  # 32-bit instead of 64-bit
   ```

2. Vectorize operations:
   ```python
   # Instead of loop:
   for site in sites:
       rates.append(compute_oxygen_supply_rate(site))
   
   # Use NumPy:
   rates = compute_oxygen_supply_rate_vectorized(sites)  # Faster
   ```

---

## Numerical Stability Issues

### NaN or Infinity in Results

**Error:** `Warning: NaN in oxidation rates. Check O₂ concentration, temperature`

**Diagnosis:**
```python
print(f"O₂ concentration: {o2_conc}")
print(f"Temperature: {T_kelvin} K")
print(f"Expected: 250 K < T < 330 K, 0 < O₂ < 500 µmol/L")

if not (250 < T_kelvin < 330):
    print("ERROR: Temperature out of valid range")

if not (0 < o2_conc < 500):
    print("ERROR: O₂ concentration out of valid range")
```

**Solutions:**
1. Add input validation:
   ```python
   assert 250 <= T <= 330, f"Temperature {T} K out of range"
   assert 0 < o2_conc < 500, f"O₂ {o2_conc} µmol/L invalid"
   ```

2. Add bounds to calculations:
   ```python
   rate = np.clip(rate, 0, 1e-6)  # Clamp to reasonable range
   ```

---

### Negative Diffusivity or Diffusivity Spikes

**Error:** `K_v negative or extremely large (>100 m²/s)`

**Cause:** Instability in Monin-Obukhov calculations at very stable or very unstable conditions

**Solution:**
```python
K_v = np.clip(K_v, 1e-4, 1.0)  # Physically reasonable range
```

---

## Database and Output Issues

### CSV Export Encoding Problems

**Error:** `UnicodeEncodeError: 'ascii' codec can't encode character`

**Solution:**
```python
# Ensure UTF-8 encoding
df.to_csv('output.csv', index=False, encoding='utf-8')
```

---

### GeoJSON with Special Characters

**Error:** `JSONDecodeError: Invalid \escape`

**Solution:**
```python
import json

# Ensure proper JSON escaping
geojson_str = json.dumps(geojson, ensure_ascii=True)

# Or escape manually
description = description.replace('\\', '\\\\').replace('"', '\\"')
```

---

### Dashboard Update Fails

**Error:** `FileNotFoundError: dashboard/latest_cycle.json not found`

**Solution:**
```python
# Ensure directory exists
Path('dashboard').mkdir(parents=True, exist_ok=True)

# Write JSON
with open('dashboard/latest_cycle.json', 'w') as f:
    json.dump(data, f)
```

---

## Log Messages and Interpretation

### Common Log Entries

```
[INFO] Cycle 1 started at 2026-06-10T12:00:00
→ Normal: cycle beginning

[WARNING] Cycle 1 complete: 650.2 s
→ Cycle exceeded 15-min deadline (900 s limit)
→ Action: Use scenario library or reduce AMR levels

[ERROR] Cycle 1 failed: Wind solver convergence failure
→ Action: Check input file, reduce grid resolution, increase iterations

[WARNING] HIGH-risk hotspots: 3 detected
→ Normal: Alerts operations team for action
→ Check oxygen supply rates in output CSV

[INFO] Using scenario library (cached wind)
→ Normal: 60× speedup vs. full wind solve

[ERROR] CSV format invalid: missing 'z' column
→ Check amd_sites.csv column headers
```

---

## Support Resources

### Debug Output

Enable verbose logging:
```python
from phreeqc_coupling.amd_hotspot_detector import identify_valley_amd_hotspots

results = identify_valley_amd_hotspots(
    wind, 'amd_sites.csv',
    verbose=True  # Enable diagnostics
)

# Check log file
cat monitoring.log | grep ERROR
```

### References

- **Physics validation:** VALIDATION_AMD_HOTSPOTS.md, VALIDATION_SULFIDE_OXIDATION.md
- **API documentation:** api_reference.md
- **Case studies:** case_studies.md
- **Deployment:** deployment_guide.md

### Reporting Issues

Include in bug report:
1. Error message (full traceback)
2. Relevant input files (amd_sites.csv, inputs.i)
3. Log file contents (monitoring.log)
4. System info: `python --version`, `cmake --version`
5. Steps to reproduce

---

**Last Updated:** 2026-06-10  
**massconsistent_amr PHREEQC Coupling v1.0.0**
