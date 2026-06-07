# Mass-Consistent Wind Solver Test Cases

This directory contains four comprehensive test cases for the mass-consistent wind solver with time-varying winds, log-law initialization, and synthetic turbulence fluctuation generation.

## Test Case 1: Gaussian Hill (Synthetic Terrain)

**Location**: `mass_consistent_case1_gaussian_hill/`

**Terrain**: Synthetic Gaussian hill
- Domain: 500m × 500m
- Grid: 21×21 points (25m spacing)
- Peak elevation: 75m at center
- Sigma (width): 100m

**Key Features**:
- ✓ Time-varying wind boundary conditions (10 time steps)
- ✓ Log-law wind profile initialization (z0=0.05m)
- ✓ Reference wind: 12 m/s from west
- ✓ OpenFAST synthetic turbulence fluctuations
- ✓ Von Kármán spectrum model
- ✓ BTS export for OpenFAST compatibility

**Files**:
- `test_case1.py` - Main test script
- `terrain_gen.py` - Terrain generation (21×21 Gaussian hill)
- `terrain.csv` - Pre-generated terrain
- `inputs.i` - Wind solver configuration
- `time_series.csv` - Time-varying wind boundary conditions

**Running the Test**:
```bash
cd mass_consistent_case1_gaussian_hill
python3 test_case1.py
```

**Expected Output**:
- Grid dimensions: 21×21×(variable Z)
- Wind solution convergence with MLMG iterations
- Velocity extraction at 30m AGL
- Plotfile output with and without turbulence fluctuations

---

## Test Case 2: Flatirons NREL Site (Real Terrain)

**Location**: `mass_consistent_case2_flatirons/`

**Terrain**: Flatirons area, Boulder, CO (NREL test site)
- Real SRTM elevation data
- Domain: ~3.5 km × 3.5 km
- Grid: 21×21 points (user-specified spacing)
- Terrain: Rocky foothills with steep slopes

**Key Features**:
- ✓ Real-world SRTM terrain (1-arcsecond resolution)
- ✓ Time-varying wind boundary conditions (20 time steps)
- ✓ Log-law wind profile (z0=0.1m - grassland)
- ✓ Reference wind: 11 m/s from west
- ✓ OpenFAST synthetic turbulence (TI=0.14)
- ✓ Wind turbine hub-height extraction (40m AGL)

**Files**:
- `test_case2.py` - Main test script
- `inputs.i` - Wind solver configuration
- `time_series.csv` - Time-varying wind conditions
- `terrain.csv` - User-generated from SRTM data (see below)

**Generating Terrain**:
1. Download SRTM tile N40W105.hgt from USGS:
   - https://earthexplorer.usgs.gov/
   - Search for: Boulder, CO area
   - Download 1-arcsecond resolution

2. Generate terrain CSV:
```bash
cd mass_consistent_case2_flatirons
python3 ../../tools/terrain_reader_srtm.py N40W105.hgt \
  --output terrain.csv \
  --lat-min 40.010 --lat-max 40.037 \
  --lon-min -105.245 --lon-max -105.218 \
  --nx 21 --ny 21
```

**Running the Test**:
```bash
cd mass_consistent_case2_flatirons
python3 test_case2.py
```

**Expected Output**:
- Real terrain with significant elevation variation
- Wind acceleration over exposed ridges
- Velocity extraction at 40m AGL (typical wind turbine hub height)
- Comparison of wind speeds over complex terrain

---

## Test Case 3: Mt. Hood (Alpine Terrain)

**Location**: `mass_consistent_case3_mt_hood/`

**Terrain**: Mt. Hood area, Oregon
- Real SRTM elevation data
- Domain: Summit area (~4 km × 4 km)
- Grid: 21×21 points (user-specified spacing)
- Terrain: High-altitude alpine with elevation > 3000m

**Key Features**:
- ✓ High-elevation real SRTM terrain
- ✓ Time-varying wind boundary conditions (25 time steps)
- ✓ Log-law wind profile (z0=0.2m - alpine vegetation)
- ✓ Reference wind: 13 m/s from west (strong westerlies)
- ✓ Higher turbulence intensity (TI=0.16)
- ✓ Velocity extraction above alpine terrain (50m AGL)

**Files**:
- `test_case3.py` - Main test script
- `inputs.i` - Wind solver configuration
- `time_series.csv` - Time-varying wind conditions (including gusts)
- `terrain.csv` - User-generated from SRTM data (see below)

**Generating Terrain**:
1. Download SRTM tile N45W121.hgt from USGS:
   - https://earthexplorer.usgs.gov/
   - Search for: Mt. Hood, OR area
   - Download 1-arcsecond resolution

2. Generate terrain CSV:
```bash
cd mass_consistent_case3_mt_hood
python3 ../../tools/terrain_reader_srtm.py N45W121.hgt \
  --output terrain.csv \
  --lat-min 45.366 --lat-max 45.380 \
  --lon-min -121.696 --lon-max -121.680 \
  --nx 21 --ny 21
```

**Running the Test**:
```bash
cd mass_consistent_case3_mt_hood
python3 test_case3.py
```

**Expected Output**:
- High-elevation terrain with significant relief
- Wind shear profile in alpine environment
- Velocity extraction at 50m AGL
- Strong wind speeds at high elevation

---

## Test Case 4: Turbine Wake (Flat Terrain)

**Location**: `mass_consistent_case4_turbine_wake/`

**Terrain**: Flat terrain
- Domain: 100m × 100m
- Grid: 10×10 points (10m spacing)
- Flat surface at elevation 0m

**Key Features**:
- ✓ Analytical wind turbine wake modeling (Jensen / Park model)
- ✓ Upstream and downstream turbine placement (wake alignment)
- ✓ Power output calculations using reference power curves (`nrel_5mw.csv`)
- ✓ Inflow wind speed extraction and wake velocity deficit validation
- ✓ Automated logging of turbine power outputs to CSV

**Files**:
- `test_case4.py` - Main test script
- `terrain.csv` - Flat terrain file
- `turbines.csv` - Turbine placement and parameter specification
- `nrel_5mw.csv` - NREL 5MW turbine power curve definition
- `inputs.i` - Wind solver configuration with turbine wake enabled

**Running the Test**:
```bash
cd mass_consistent_case4_turbine_wake
python3 test_case4.py
```

**Expected Output**:
- Grid dimensions: 10×10×10
- Successful wind field solution with wake effects incorporated
- Turbine 0 (Upstream at x=20): Inflow speed = 12.97 m/s, Power = 5000.00 kW
- Turbine 1 (Downstream at x=80): Inflow speed = 11.13 m/s, Power = 5000.00 kW
- Downstream turbine experiences clear wake velocity deficit
- Power output CSV logging generated successfully

---

## Shared Tools

### Gaussian Hill Generator

Generate synthetic Gaussian hills with customizable dimensions:

```bash
python3 ../../tools/gaussian_hill_generator.py \
  --output terrain.csv \
  --nx 21 --ny 21 \
  --domain-x 500.0 --domain-y 500.0 \
  --peak 75.0 --sigma 100.0
```

Parameters:
- `--nx, --ny`: Grid dimensions
- `--domain-x, --domain-y`: Domain extent [m]
- `--peak`: Peak elevation [m]
- `--sigma`: Gaussian width parameter [m]

### SRTM Terrain Reader

Read and process SRTM digital elevation model (DEM) data:

```bash
python3 ../../tools/terrain_reader_srtm.py <input.hgt> \
  --output terrain.csv \
  --lat-min <LAT_MIN> --lat-max <LAT_MAX> \
  --lon-min <LON_MIN> --lon-max <LON_MAX> \
  --nx 21 --ny 21
```

Features:
- Reads SRTM 1-arcsecond resolution (~30m) HGT files
- Bilinear interpolation for sub-grid accuracy
- Converts lat/lon to projected coordinates
- Outputs CSV compatible with wind solver

---

## Wind Solver Configuration

All three cases use the same solver parameters with site-specific variations:

### Common Parameters
- **Grid spacing**: 20-40m horizontal, 20-25m vertical
- **MLMG solver**: Multigrid acceleration for Poisson equation
- **Extract height**: Terrain-following AGL extraction

### OpenFAST Synthetic Turbulence
- **Spectrum model**: Von Kármán (standard for wind energy)
- **Intensity model**: Power-law with height
- **Coherence model**: Gaussian exponential decay
- **BTS export**: Binary format for OpenFAST/TurbSim compatibility

### Time-Varying Winds
- Multiple time steps representing diurnal or transient conditions
- Linear interpolation between boundary conditions
- Consistent direction variation

---

## Output Files

Each test case generates:

1. **Plotfiles** (AMReX format):
   - `plt_case#_winds` - Corrected wind field
   - `plt_case#_winds_with_fluctuations` - Wind + turbulence

2. **Extracts** (CSV format):
   - `wind_extract*.csv` - 2D wind field at specified AGL height

3. **Turbulence** (Binary BTS format):
   - `case#_turbulence.bts` - OpenFAST/TurbSim format
   - `case#_turbulence.meta` - Metadata file

---

## Requirements

### Build Requirements
```bash
cmake -S . -B build \
  -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON \
  -DMASSCONSISTENT_ENABLE_OPENFAST=ON
cd build && make -j4
```

### Python Modules
- numpy (for array operations)
- struct (for BTS binary format)

### External Data (Cases 2 & 3)
- SRTM HGT files from USGS SRTM server

---

## References

- Mass-consistent wind solver: AMReX-based atmospheric solver
- SRTM (Shuttle Radar Topography Mission): https://earthexplorer.usgs.gov/
- OpenFAST: https://openfast.readthedocs.io/
- TurbSim: https://www.nrel.gov/wind/nwtc/turbsim.html
- Von Kármán spectrum: Kármán, T. (1948). Progress in the statistical theory of turbulence

---

## Test Validation Checklist

Each test case validates:

- [ ] Terrain file exists and is properly formatted
- [ ] Solver initialization with correct grid dimensions
- [ ] Wind field solution convergence
- [ ] Velocity field extraction at AGL heights
- [ ] Physical parameter ranges (wind speeds, turbulence intensity)
- [ ] Plotfile generation
- [ ] BTS export for OpenFAST compatibility

---

## Troubleshooting

### SRTM Terrain Reader Issues

**Q: "No Data" values appearing in terrain?**
- A: Ensure SRTM tile covers the requested lat/lon region
- A: Check tile naming convention matches requested coordinates

**Q: Coordinate mismatch error?**
- A: Verify lat/lon bounds are within tile coverage
- A: Use standard decimal format (e.g., 40.010 not 40°00'36")

### Wind Solver Issues

**Q: MLMG solver not converging?**
- A: Increase `max_iter` in inputs.i
- A: Reduce `tol_rel` for lower tolerance requirement

**Q: Memory error with large grids?**
- A: Reduce grid dimensions or domain size
- A: Use coarser grid spacing

---

## Contributing

To add new test cases:
1. Create new directory: `mass_consistent_case#_<description>/`
2. Generate terrain file (synthetic or from SRTM)
3. Create `inputs.i` configuration file
4. Create `time_series.csv` for time-varying winds
5. Create `test_case#.py` following existing pattern
6. Update this README with case description

---

## License

These test cases are part of the massconsistent_amr project and follow the same license terms.
