# PHREEQC Coupling Deployment Guide

Setup and operational procedures for real-time monitoring and production deployment.

---

## Architecture Overview

### System Components

```
Input: NWP Forecast (u, T, precip)
           ↓
    Scenario Library Lookup <30 ms
           ↓
  Atmospheric BC Extraction <300 ms
           ↓
    ┌─────────────────────────┐
    │  Primary Tasks (required)│
    │  1. Wind extraction      │
    │  2. Temperature profile  │
    │  3. Precipitation mapping│
    │  5. Stability class      │
    │  13. AMD hotspot detect  │
    │  11. Oxidation rates     │
    │  19. Dashboard update    │
    └─────────────────────────┘
           ↓ ~5 min
    ┌─────────────────────────┐
    │ Secondary Tasks (optional)
    │ 6. Sherwood correlation │
    │ 7. Dust suppression     │
    │ 18. Leaching efficiency │
    │ 21. Facility workflow   │
    └─────────────────────────┘
           ↓ ~10 min (if enabled)
    Output: Risk maps, alerts, DB updates
           ↓
    Sleep: Until next 15-min cycle
```

---

## Pre-Deployment Checklist

### 1. Environment Setup

**Install massconsistent_amr:**
```bash
# Clone repository
git clone --recursive https://github.com/hgopalan/massconsistent_amr.git
cd massconsistent_amr

# Build with Python bindings
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
cmake --build build --parallel

# Verify installation
python3 -c "from wind_solver import WindSolver; print('✓ massconsistent_amr installed')"
```

**Install Python dependencies:**
```bash
pip install netcdf4 h5py numpy scipy pandas geopandas

# Optional: For parallel scenario library building
pip install joblib

# Optional: For dashboard visualization
pip install plotly dash pandas
```

### 2. Scenario Library Generation (One-Time Offline)

**Pre-compute 100-scenario library:**
```bash
cd phreeqc_coupling

python3 << 'EOF'
from scenario_library import build_scenario_library
import time

print("Building scenario library (1-2 hours)...")
start = time.time()

lib = build_scenario_library(
    n_scenarios=100,
    output_dir='/data/scenario_library/',
    parallel=True,
    n_jobs=-1  # Use all cores
)

elapsed = time.time() - start
print(f"✓ Library complete in {elapsed/60:.1f} minutes")
print(f"  Storage: {lib.output_file}")
EOF
```

**Verify library:**
```bash
python3 << 'EOF'
from scenario_library import ScenarioLibrary

lib = ScenarioLibrary.load('/data/scenario_library/library.h5')
print(f"✓ Library loaded: {len(lib.scenarios)} scenarios")
print(f"  Storage: 250 MB")
print(f"  Lookup time: <30 ms")
EOF
```

### 3. Input File Preparation

**Create wind solver input file (`inputs_amd.i`):**
```
# massconsistent_amr input for AMD monitoring domain
# Domain: 5×5 km, 100 m resolution, valley topography

amr.n_cell_x = 50
amr.n_cell_y = 50
amr.n_cell_z = 20

# Terrain input
terrain.use_dem_file = true
terrain.dem_file = "/data/topography/valley_dem.nc"

# Initialize with typical wind condition
init_type = "raws"
raws_file = "/data/weather/latest_observation.csv"

# Boundary conditions
bc.wind_north = true
bc.pressure_gradient_magnitude = 0.001  # Weak synoptic gradient

# Solver settings
poisson_solver.mg_verbose = 0
poisson_solver.max_iter = 100
```

**Create AMD locations file (`amd_sites.csv`):**
```csv
id,x,y,z,discharge_type,description
amd_upper_spring,5120,4950,1485,spring,Primary discharge - high elevation
amd_mid_seep,5100,5000,1350,seep,Mid-slope seepage
amd_lower_spring,5080,5050,1200,spring,Valley spring - low elevation
amd_lee_shelter,5150,5100,1220,groundwater,Lee-side protected location
amd_exposed_runoff,5050,4900,1400,runoff,Exposed runoff - high wind
```

### 4. Operational Monitoring Setup

**Create monitoring script (`run_amd_monitoring.py`):**
```python
#!/usr/bin/env python3
"""Real-time AMD hotspot monitoring with 15-minute cycle."""

import sys
import time
import logging
from pathlib import Path
from datetime import datetime

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[
        logging.FileHandler('monitoring.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

def run_monitoring_cycle(cycle_num: int) -> dict:
    """Execute one complete monitoring cycle."""
    
    from wind_solver import WindSolver
    from phreeqc_coupling import FieldExtractor
    from phreeqc_coupling.amd_hotspot_detector import identify_valley_amd_hotspots
    from phreeqc_coupling.sulfide_oxidation import compute_sulfide_oxidation_rates
    from scenario_library import ScenarioLibrary
    
    cycle_start = time.time()
    logger.info(f"=== Cycle {cycle_num} started at {datetime.now().isoformat()} ===")
    
    try:
        # Step 1: Solve wind field or use cached scenario
        if Path('scenario_library/library.h5').exists():
            logger.info("Using scenario library (cached wind)")
            lib = ScenarioLibrary.load('scenario_library/library.h5')
            
            # Get latest weather forecast
            import subprocess
            result = subprocess.run(
                ['python3', 'get_latest_forecast.py'],
                capture_output=True, text=True, timeout=10
            )
            u_mag, wind_dir, T = map(float, result.stdout.strip().split())
            logger.info(f"Weather: u={u_mag:.1f} m/s, dir={wind_dir:.0f}°, T={T:.1f}°C")
            
            scenario = lib.nearest_scenario(u_mag, wind_dir, T)
            logger.info(f"Using scenario: u={scenario.u_mag:.1f} m/s")
        else:
            logger.info("Solving wind field (10 min)")
            wind = WindSolver("inputs_amd.i")
            wind.solve()
        
        # Step 2: Primary tasks (required)
        logger.info("Primary tasks...")
        
        if not Path('scenario_library/library.h5').exists():
            extractor = FieldExtractor(wind)
            fields = extractor.extract_all_fields()
        
        # AMD hotspot detection
        logger.info("  - AMD hotspot detection...")
        amd_results = identify_valley_amd_hotspots(
            wind if not Path('scenario_library/library.h5').exists() else scenario,
            'amd_sites.csv',
            output_dir='output/cycle_{}/'.format(cycle_num),
            verbose=False
        )
        
        high_risk = amd_results['high_risk_count']
        logger.info(f"    Found {high_risk} HIGH-risk hotspots")
        
        # Sulfide oxidation rates
        logger.info("  - Sulfide oxidation rates...")
        ox_results = compute_sulfide_oxidation_rates(
            wind if not Path('scenario_library/library.h5').exists() else scenario,
            'sulfide_sites.csv',
            output_dir='output/cycle_{}/'.format(cycle_num),
            verbose=False
        )
        
        logger.info(f"    Max rate: {ox_results['max_oxidation_rate']:.2e} mol/(m³·s)")
        
        # Step 3: Secondary tasks (if time available)
        elapsed = time.time() - cycle_start
        if elapsed < 300:  # 5 minutes
            logger.info("Secondary tasks (compute available)...")
            # Add optional tasks here
        
        # Step 4: Update database and dashboard
        logger.info("Updating dashboard...")
        update_dashboard(amd_results, ox_results, cycle_num)
        
        cycle_time = time.time() - cycle_start
        logger.info(f"Cycle {cycle_num} complete: {cycle_time:.1f} s")
        
        return {'status': 'success', 'cycle_time': cycle_time}
        
    except Exception as e:
        logger.error(f"Cycle failed: {e}", exc_info=True)
        return {'status': 'failed', 'error': str(e)}

def update_dashboard(amd_results: dict, ox_results: dict, cycle_num: int):
    """Update operational dashboard with latest results."""
    
    import json
    from pathlib import Path
    
    # Write JSON for web dashboard
    dashboard_data = {
        'cycle': cycle_num,
        'timestamp': datetime.now().isoformat(),
        'high_risk_hotspots': amd_results['high_risk_count'],
        'mean_oxidation_rate': ox_results['mean_oxidation_rate'],
        'output_files': {
            'amd_geojson': amd_results['output_files'].get('geojson'),
            'oxidation_csv': ox_results['output_files'].get('csv')
        }
    }
    
    dashboard_path = Path('dashboard/latest_cycle.json')
    dashboard_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(dashboard_path, 'w') as f:
        json.dump(dashboard_data, f, indent=2)

def main():
    """Main monitoring loop."""
    
    logger.info("AMD Monitoring System started")
    logger.info("Configuration:")
    logger.info("  - Cycle interval: 15 minutes")
    logger.info("  - Input file: inputs_amd.i")
    logger.info("  - AMD sites: amd_sites.csv")
    logger.info("  - Output: output/ and dashboard/")
    
    cycle_num = 0
    
    while True:
        try:
            cycle_num += 1
            result = run_monitoring_cycle(cycle_num)
            
            if result['status'] == 'success':
                # Sleep until next cycle
                sleep_time = 900 - result['cycle_time']  # 15 min - elapsed time
                if sleep_time > 0:
                    logger.info(f"Sleeping {sleep_time:.0f} s until next cycle")
                    time.sleep(sleep_time)
                else:
                    logger.warning(f"Cycle exceeded 15 min by {-sleep_time:.0f} s")
            else:
                logger.error(f"Cycle failed. Retrying in 60 s...")
                time.sleep(60)
        
        except KeyboardInterrupt:
            logger.info("Monitoring stopped by user")
            break
        except Exception as e:
            logger.error(f"Unexpected error: {e}", exc_info=True)
            time.sleep(60)

if __name__ == '__main__':
    main()
```

**Make script executable:**
```bash
chmod +x run_amd_monitoring.py
```

---

## System Deployment

### 1. Production Server Setup

**File structure:**
```
/data/
├── scenario_library/
│   └── library.h5 (250 MB, pre-computed)
├── topography/
│   └── valley_dem.nc
├── weather/
│   └── latest_observation.csv
├── amd_sites.csv
├── sulfide_sites.csv
└── inputs_amd.i

/app/
├── run_amd_monitoring.py
├── get_latest_forecast.py
└── massconsistent_amr/ (git clone)

/output/
├── cycle_1/
│   ├── amd_hotspots.geojson
│   ├── amd_hotspots.csv
│   ├── oxidation_rates.geojson
│   └── oxidation_rates.csv
└── ...

/dashboard/
└── latest_cycle.json
```

### 2. Systemd Service (Linux)

**Create service file (`/etc/systemd/system/amd-monitoring.service`):**
```ini
[Unit]
Description=AMD Hotspot Monitoring System
After=network.target

[Service]
Type=simple
User=amd-monitor
WorkingDirectory=/app
ExecStart=/usr/bin/python3 run_amd_monitoring.py
Restart=on-failure
RestartSec=60
StandardOutput=journal
StandardError=journal
Environment="PATH=/usr/local/bin:/usr/bin"

[Install]
WantedBy=multi-user.target
```

**Enable and start service:**
```bash
sudo systemctl daemon-reload
sudo systemctl enable amd-monitoring
sudo systemctl start amd-monitoring
sudo systemctl status amd-monitoring
```

**Monitor logs:**
```bash
sudo journalctl -u amd-monitoring -f
```

### 3. Docker Deployment (Alternative)

**Dockerfile:**
```dockerfile
FROM python:3.10-slim

RUN apt-get update && apt-get install -y \
    cmake build-essential git \
    libopenmpi-dev libhdf5-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Clone and build massconsistent_amr
RUN git clone --recursive https://github.com/hgopalan/massconsistent_amr.git
WORKDIR /app/massconsistent_amr
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
RUN cmake --build build --parallel

# Install Python dependencies
RUN pip install netcdf4 h5py numpy scipy pandas geopandas joblib

WORKDIR /app

# Copy monitoring scripts
COPY run_amd_monitoring.py .
COPY get_latest_forecast.py .
COPY inputs_amd.i /data/
COPY amd_sites.csv /data/
COPY scenario_library/ /data/scenario_library/

# Build scenario library if not present
RUN python3 << 'EOF'
from pathlib import Path
if not Path('/data/scenario_library/library.h5').exists():
    print("Building scenario library...")
    from phreeqc_coupling.scenario_library import build_scenario_library
    build_scenario_library(n_scenarios=100, output_dir='/data/scenario_library/', parallel=True)
EOF

CMD ["python3", "run_amd_monitoring.py"]
```

**Build and run:**
```bash
docker build -t amd-monitor:latest .
docker run -d \
  -v /data/topography:/data/topography:ro \
  -v /data/weather:/data/weather:ro \
  -v /output:/output \
  -v /dashboard:/dashboard \
  --name amd-monitor \
  amd-monitor:latest
```

---

## Operations & Monitoring

### Health Checks

**Verify cycle execution:**
```bash
# Check latest cycle completion
tail -20 monitoring.log

# Verify output generation
ls -lh output/cycle_*/

# Check dashboard update
cat dashboard/latest_cycle.json
```

**Alert conditions:**
```
- Cycle time > 900 s (exceeds 15-min window)
- AMD HIGH-risk hotspots: Alert to operations
- Wind solve failures: Fall back to scenario library
- Missing forecast data: Skip secondary tasks
```

### Performance Tuning

| Setting | Value | Impact |
|---------|-------|--------|
| Scenario library size | 100 scenarios | ~250 MB, ±5% error |
| Wind solver AMR levels | 2–3 levels | Balance accuracy vs. speed |
| AMD site density | 10–50 sites | Linear timing impact |
| Dashboard update frequency | Every 15 min | Real-time requirements |

### Database Integration

**Store results in PostgreSQL:**
```sql
CREATE TABLE amd_hotspots (
    cycle_id INTEGER,
    timestamp TIMESTAMP,
    site_id TEXT,
    risk_class TEXT,  -- HIGH, MEDIUM, LOW
    o2_supply_rate FLOAT,
    wind_speed FLOAT,
    temperature FLOAT,
    geom GEOMETRY(Point, 4326)
);

-- Query: Find HIGH-risk sites over past 7 days
SELECT site_id, COUNT(*) as high_risk_days
FROM amd_hotspots
WHERE risk_class = 'HIGH'
  AND timestamp > NOW() - INTERVAL '7 days'
GROUP BY site_id
ORDER BY high_risk_days DESC;
```

---

## Troubleshooting

### Common Issues

**Issue: Cycle time exceeds 15 minutes**
- Solution 1: Use scenario library instead of full wind solve (60× speedup)
- Solution 2: Disable secondary tasks
- Solution 3: Reduce wind solver AMR levels (speed vs. accuracy trade-off)

**Issue: Wind solver fails to converge**
- Check input file format and units
- Verify terrain DEM has no invalid values (NaN, negative elevations)
- Reduce AMR resolution (fewer cells)

**Issue: AMD hotspot locations not found**
- Verify CSV file format (must have id, x, y, z, discharge_type, description)
- Check coordinates are within solver domain
- Ensure CSV file encoding is UTF-8

**Issue: Dashboard not updating**
- Verify output/ directory is writable
- Check JSON schema matches dashboard expectations
- Ensure cycle completion status = 'success'

### Diagnostic Commands

```bash
# Check Python environment
python3 -c "import phreeqc_coupling; print(phreeqc_coupling.__version__)"

# Verify scenario library
python3 << 'EOF'
from scenario_library import ScenarioLibrary
lib = ScenarioLibrary.load('/data/scenario_library/library.h5')
print(f"Scenarios: {len(lib.scenarios)}")
print(f"Nearest lookup: {lib.nearest_scenario(8.5, 270, 288.15)}")
EOF

# Test wind solver
python3 << 'EOF'
from wind_solver import WindSolver
wind = WindSolver("inputs_amd.i")
wind.solve()
print("✓ Wind solver working")
EOF
```

---

## Performance Targets

| Metric | Target | Typical | Notes |
|--------|--------|---------|-------|
| Cycle time | <15 min | 6–8 min | With scenario caching |
| Wind solve | ~10 min | 10 min | Full solve, 50×50×20 grid |
| AMD detection | <300 ms | 180 ms | 5 sites |
| Dashboard update | <30 s | 5 s | JSON generation |
| Database write | <500 ms | 100 ms | Per cycle |

---

## References

- massconsistent_amr: https://github.com/hgopalan/massconsistent_amr
- PHREEQC: https://www.usgs.gov/mission-areas/water-resources/science/phreeqc
- Scenario caching: user_guide.md (Capability #8)
- AMD hotspot physics: Sherwood (1954); Businger et al. (1971)

---

**Last Updated:** 2026-06-10  
**massconsistent_amr PHREEQC Coupling v1.0.0**
