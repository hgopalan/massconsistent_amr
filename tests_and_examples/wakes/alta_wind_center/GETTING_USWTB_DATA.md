# Getting USWTB Data for Alta Wind Energy Center

This guide explains how to obtain real turbine coordinates from the USGS Wind Turbine Database (USWTB) for the Alta Wind Energy Center simulation.

## Quick Start

### Option 1: Automatic Download (Recommended)

If you have internet access, run the fetch tool:

```bash
python3 tools/data_ingestion/fetch_uswtb_turbines.py \
  --output tests_and_examples/wakes/alta_wind_center/turbines_uswtb.csv \
  --project "Alta Wind"
```

This will:
1. Download the full USWTB database from USGS
2. Filter for all "Alta Wind" projects
3. Extract ~600 turbines with exact WGS84 coordinates
4. Write to `turbines_uswtb.csv` in the test directory

### Option 2: Manual Download

1. **Visit USGS USWTB**: https://energy.usgs.gov/uswtdb/

2. **Apply Filters**:
   - State: California
   - Project: Alta Wind (or search for specific projects)
   - Select all turbines

3. **Export to CSV**:
   - Click "Export Data"
   - Choose CSV format
   - Save as `turbines_uswtb.csv` in this directory

4. **Format Conversion** (optional):
   If the downloaded CSV has different column names, run:
   ```bash
   python3 tools/data_ingestion/fetch_uswtb_turbines.py \
     --input /path/to/downloaded.csv \
     --output turbines_uswtb.csv
   ```

### Option 3: Direct CSV Download

```bash
# Download full USWTB database ZIP
wget https://energy.usgs.gov/uswtdb/assets/data/uswtdbCSV.zip

# Extract
unzip uswtdbCSV.zip

# Use fetch tool to extract Alta Wind turbines
python3 tools/data_ingestion/fetch_uswtb_turbines.py \
  --input path/to/uswtdbCSV_File.csv \
  --output turbines_uswtb.csv \
  --project "Alta Wind"
```

## CSV Format Requirements

The test expects `turbines_uswtb.csv` with at minimum these columns:

```csv
turbine,project,latitude,longitude,hub_height,rotor_diameter,manufacturer,model
1,Alta Wind Energy Center I,35.040,-118.340,80.0,100.0,GE,GE 2.5-100
2,Alta Wind Energy Center I,35.0405,-118.340,80.0,100.0,GE,GE 2.5-100
...
```

### Required Columns
- `latitude`: WGS84 latitude (decimal degrees, ±90)
- `longitude`: WGS84 longitude (decimal degrees, ±180)

### Optional Columns (used if available)
- `turbine`: Turbine ID
- `project`: Project name
- `hub_height`: Hub height in meters (default: 80.0)
- `rotor_diameter`: Rotor diameter in meters (default: 100.0)
- `manufacturer`: Turbine manufacturer
- `model`: Turbine model

## Verifying Your Data

After obtaining the CSV:

1. **Check file location**:
   ```bash
   ls -l tests_and_examples/wakes/alta_wind_center/turbines_uswtb.csv
   ```

2. **Verify content**:
   ```bash
   head -5 tests_and_examples/wakes/alta_wind_center/turbines_uswtb.csv
   wc -l tests_and_examples/wakes/alta_wind_center/turbines_uswtb.csv
   ```

3. **Check coordinate bounds** (should be in Tehachapi Pass, CA):
   ```bash
   awk -F, 'NR>1 {print $3}' turbines_uswtb.csv | sort -n | head -1
   awk -F, 'NR>1 {print $3}' turbines_uswtb.csv | sort -n | tail -1
   ```
   Should show latitudes around 35.0-35.05°N

## Running the Test

Once you have `turbines_uswtb.csv` in place:

```bash
export PYTHONPATH=/path/to/massconsistent_amr/build/python:$PYTHONPATH
cd tests_and_examples/wakes/alta_wind_center
python3 test_alta_wind_center.py
```

The test will:
1. Load exact turbine coordinates from your CSV
2. Convert from WGS84 to UTM Zone 11N projection
3. Automatically size the domain based on turbine bounds
4. Generate realistic terrain
5. Run the wind field solver
6. Verify wind field characteristics

## Troubleshooting

### "USWTB turbine data file not found"

**Solution**: Place `turbines_uswtb.csv` in the test directory:
```bash
tests_and_examples/wakes/alta_wind_center/turbines_uswtb.csv
```

### "No valid turbine coordinates found"

**Check**:
- CSV has `latitude` and `longitude` columns
- Values are decimal numbers (not text)
- Latitude in range [-90, 90]
- Longitude in range [-180, 180]

### Fetch tool fails to download

**Cause**: Network access blocked or USGS server unreachable

**Solution**: 
- Use manual download option above
- Check internet connection
- Try again later
- Manually download from https://energy.usgs.gov/uswtdb/

## Sample Data

A sample file with 20 turbines is included for testing:

```bash
cp turbines_uswtb_sample.csv turbines_uswtb.csv
python3 test_alta_wind_center.py  # Quick test with smaller dataset
```

## Data Source References

1. **USGS Wind Turbine Database**:
   - Website: https://energy.usgs.gov/uswtdb/
   - Download: https://energy.usgs.gov/uswtdb/assets/data/uswtdbCSV.zip

2. **Hoen et al. (2024)**:
   - "United States Wind Turbine Database"
   - Version 7.1, May 2024
   - Berkeley Lab, LBNL

3. **Coordinate System**:
   - Input: WGS84 (EPSG:4326)
   - Output: UTM Zone 11N (EPSG:32611)
   - Conversion: pyproj library

## For Developers

To modify how USWTB data is processed, see:
- `tools/data_ingestion/fetch_uswtb_turbines.py` - Data extraction
- `tests_and_examples/wakes/alta_wind_center/test_alta_wind_center.py` (setUp method) - Coordinate loading
- `TURBINE_LOCATIONS.md` - Technical coordinate system documentation
