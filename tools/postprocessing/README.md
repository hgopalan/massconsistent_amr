# Post-Processing and Visualization Tools

This directory contains the suite of post-processing and visualization Python scripts used to generate the high-resolution, publication-quality scenario gallery images in the project documentation and README.

These scripts run the mass-consistent wind solver or analytical physical models to extract the computed flow fields and display them alongside terrain, structural loading, wakes, and deposition registers.

## Prerequisites

Ensure you have Python 3 and the following scientific plotting libraries installed:
```bash
pip install matplotlib numpy pandas scipy
```

Additionally, make sure the Python bindings (`pyWindSolver`) have been compiled and built via CMake with `-DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON` and are available in your `PYTHONPATH`.

## Post-Processing Scripts

### 1. Agricultural Drone Deposition (`plot_drone_deposition.py`)
Runs the Colorado Complex Terrain Drone Spray workflow and creates a side-by-side two-panel plot.
- **Left panel**: High-resolution 2D terrain elevation contour map with the terrain-following flight pathway.
- **Right panel**: 2D total pesticide deposition density contour map.
- **Output**: Generates `docs/drone_deposition_plot.png`.
- **Run**:
  ```bash
  python3 tools/postprocessing/plot_drone_deposition.py
  ```

### 2. Terrain-Following Wind Flow (`plot_terrain_following.py`)
Generates a vertical slice (X-Z plane) of the mass-consistent wind field over a Gaussian hill.
- **Visualizes**: Shaded velocity magnitude contours, coordinate transformation terrain-following grid boundary, and wind velocity vectors showcasing orographic acceleration/compression over the hill peak.
- **Output**: Generates `docs/terrain_following_complex_flow.png`.
- **Run**:
  ```bash
  python3 tools/postprocessing/plot_terrain_following.py
  ```

### 3. Gorge Bridge Crossing Wind Loading (`plot_gorge_bridge.py`)
Models and plots the extreme wind deflection and acceleration across a cable-stayed suspension bridge crossing a deep canyon.
- **Left panel**: High-resolution 2D canyon topography contour map with the bridge span overlaid.
- **Right panel**: Horizontal wind speed magnitude and vector field showing canyon wind channeling speedup.
- **Output**: Generates `docs/gorge_bridge_crossing.png`.
- **Run**:
  ```bash
  python3 tools/postprocessing/plot_gorge_bridge.py
  ```

### 4. Urban Street Canyon & Building Wakes (`plot_urban_street_canyon.py`)
Visualizes building-induced wind channeling corridors and leeward building wakes.
- **Left panel**: Urban building layout block grid with the central 200m skyscraper.
- **Right panel**: Street-level horizontal velocity showing accelerated street canyon corridors and the wake recirculation region behind the central tower.
- **Output**: Generates `docs/urban_street_canyon.png`.
- **Run**:
  ```bash
  python3 tools/postprocessing/plot_urban_street_canyon.py
  ```

### 5. Transmission Line Wind Loading (`plot_transmission_line.py`)
Models high-voltage transmission lines crossing the Altamont Pass gap-flow acceleration region.
- **Left panel**: Shaded terrain pass constriction and the 500 kV transmission line corridor path.
- **Right panel**: Ground-level and tower-height wind speed contours highlighting gap-flow speedup.
- **Output**: Generates `docs/transmission_line_loading.png`.
- **Run**:
  ```bash
  python3 tools/postprocessing/plot_transmission_line.py
  ```

### 6. Yawed Wind Turbine Wake Deflection (`plot_turbine_wake.py`)
Demonstrates analytical wind turbine wake deflection using the Bastankhah Gaussian wake deflection formulation.
- **Top panel**: Wind speed wake deficit under neutral operation (0° yaw) showing strong deficit at the downstream rotor.
- **Bottom panel**: Wake deflection under yawed operation (25° yaw) demonstrating wake steering away from the downstream turbine.
- **Output**: Generates `docs/turbine_wake_deflection.png`.
- **Run**:
  ```bash
  python3 tools/postprocessing/plot_turbine_wake.py
  ```

### 7. Valley Geochemical Hotspots (`plot_valley_amd_hotspots.py`)
Identifies and classifies acid mine drainage (AMD) risk points along a valley based on local wind speeds and Sherwood mass transfer oxygen supply.
- **Left panel**: 2D valley terrain contour map with AMD discharge points colored by risk level (HIGH, MEDIUM, LOW).
- **Right panel**: Ground-level wind speed contours and vectors showing valley channeling and how wind-driven oxygen delivery defines hotspot risk.
- **Output**: Generates `docs/valley_amd_hotspots.png`.
- **Run**:
  ```bash
  python3 tools/postprocessing/plot_valley_amd_hotspots.py
  ```

### 8. 3D Puff & Particle Dispersion Modeling (`plot_puff_dispersion.py`)
Runs/simulates the 3D Puff & Particle Dispersion Modeling scenario over a Gaussian hill.
- **Left panel**: 2D contour map of terrain elevation with puff emission points and wind vector streamlines.
- **Right panel**: Ground-level concentration and wet/dry deposition footprint of the dispersed pollutant.
- **Output**: Generates `docs/puff_deposition_plot.png`.
- **Run**:
  ```bash
  python3 tools/postprocessing/plot_puff_dispersion.py
  ```
