#!/usr/bin/env python3
"""
valley_amd_hotspots.py - AMD Hotspot Detection Example

Demonstrates wind-driven acid mine drainage hotspot identification in a valley.
Shows how wind steering and channeling create chemically active zones where
oxidation potential is significantly enhanced.

This example:
  1. Solves the mass-consistent wind field
  2. Loads AMD discharge point locations
  3. Identifies and classifies hotspots by oxidation risk
  4. Exports results to GeoJSON for visualization
  5. Reports hotspot statistics and diagnostics

References:
    - Nicholson et al. (1990). Pyrite oxidation in carbonate-buffered systems.
    - Businger et al. (1971). Flux-profile relationships in the atmospheric
      surface layer.
"""

import sys
from pathlib import Path
import numpy as np

# Add src/python directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src" / "python"))

from wind_solver import WindSolver
from phreeqc_coupling.amd_hotspot_detector import (
    identify_valley_amd_hotspots,
    AMDHotspotDetector
)


def create_sample_amd_locations_csv(output_file: str) -> str:
    """Create sample AMD locations CSV for demonstration.
    
    Parameters:
        output_file (str): Output CSV file path
    
    Returns:
        str: File path
    """
    csv_content = """id,x,y,z,discharge_type,description
amd001,5000,5000,150,seep,Valley spring - weak discharge
amd002,5100,5050,140,spring,Primary AMD source - strong flow
amd003,5200,4950,145,groundwater,Diffuse groundwater seepage
amd004,4950,4900,155,runoff,Surface runoff from ore zone
amd005,5150,5150,138,seep,Secondary seep in lower valley
"""
    
    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w') as f:
        f.write(csv_content)
    
    return str(output_path)


def create_sample_inputs(output_file: str, terrain_file: str) -> str:
    """Create sample massconsistent_amr input file."""
    input_content = f"""# Wind Solver Input File
amr.n_cell_x = 30
amr.n_cell_y = 30
amr.n_cell_z = 15

domain.lo = 0 0 0
domain.hi = 10000 10000 1500

init_mode = uniform
U_ref = 8.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1

terrain_file = {terrain_file}

poisson_solver.max_iter = 100
poisson_solver.mg_verbose = 0
"""
    with open(output_file, 'w') as f:
        f.write(input_content)
    return output_file


def create_sample_terrain_csv(output_file: str) -> str:
    """Create a flat terrain CSV file."""
    terrain_content = """# Flat terrain
0.0   0.0   0.0
10000.0  0.0   0.0
10000.0 10000.0 0.0
0.0   10000.0 0.0
"""
    with open(output_file, 'w') as f:
        f.write(terrain_content)
    return output_file


class MockWindSolver:
    """Mock wind solver for fallback execution."""
    def __init__(self):
        self.nx, self.ny, self.nz = 30, 30, 15
        self.dx = self.dy = self.dz = 333.33
        self.xmin = self.ymin = self.zmin = 0.0
        self.xmax = self.ymax = self.zmax = 10000.0
        self.zs_min = 0.0
        self.zs_max = 200.0
        
    def get_velocity(self):
        return {
            'u': np.ones((self.nz, self.ny, self.nx)) * 5.0,
            'v': np.zeros((self.nz, self.ny, self.nx)),
            'w': np.zeros((self.nz, self.ny, self.nx))
        }
        
    def get_terrain(self):
        return np.zeros((self.ny, self.nx))
        
    def solve(self):
        pass
        
    def finalize(self):
        pass


def main():
    """Run AMD hotspot detection example."""
    
    print("=" * 70)
    print("AMD Hotspot Detection Example")
    print("=" * 70)
    print("\nDemonstrates identification and risk classification of AMD discharge")
    print("points using terrain-resolved wind fields.")
    
    # Setup
    inputs_file = "inputs_amd.i"
    terrain_file = "terrain_amd.csv"
    amd_csv = "amd_locations_example.csv"
    output_dir = "amd_hotspots_output"
    
    create_sample_terrain_csv(terrain_file)
    create_sample_inputs(inputs_file, terrain_file)
    
    try:
        # Step 1: Solve wind field
        print("\n" + "-" * 70)
        print("STEP 1: Solving Mass-Consistent Wind Field")
        print("-" * 70)
        
        try:
            wind = WindSolver(inputs_file)
            wind.solve()
            print(f"✓ Wind field solved successfully")
            print(f"  Domain: {wind.nx} × {wind.ny} × {wind.nz} cells")
        except Exception as e:
            print(f"✗ Wind solver initialization/execution failed: {e}")
            print("  Falling back to MockWindSolver for demonstration...")
            wind = MockWindSolver()
        
        # Step 2: Create sample AMD locations
        print("\n" + "-" * 70)
        print("STEP 2: Loading AMD Discharge Point Locations")
        print("-" * 70)
        
        amd_csv_path = create_sample_amd_locations_csv(amd_csv)
        print(f"✓ Created sample AMD locations file: {amd_csv}")
        
        # Step 3: Initialize hotspot detector
        print("\n" + "-" * 70)
        print("STEP 3: Initializing AMD Hotspot Detector")
        print("-" * 70)
        
        detector = AMDHotspotDetector(wind, verbose=True)
        detector.load_amd_locations(amd_csv)
        
        # Step 4: Identify hotspots
        print("\n" + "-" * 70)
        print("STEP 4: Identifying and Classifying Hotspots")
        print("-" * 70)
        
        hotspots = detector.identify_valley_amd_hotspots(
            output_geojson=f"{output_dir}/amd_hotspots.geojson"
        )
        
        # Step 5: Display results
        print("\n" + "-" * 70)
        print("STEP 5: Hotspot Classification Results")
        print("-" * 70)
        
        print(f"\nTotal hotspots identified: {len(hotspots)}")
        
        for risk_class in ['HIGH', 'MEDIUM', 'LOW']:
            count = sum(1 for h in hotspots if h.risk_class == risk_class)
            print(f"  {risk_class:6s}: {count} locations")
        
        print("\nDetailed hotspot information:")
        print("-" * 70)
        print(f"{'ID':<10} {'Risk':<8} {'O₂ Rate':<15} {'u*':<8} {'Wind':<8}")
        print(f"{'':10} {'Class':<8} {'[µmol/(m²·s)]':<15} {'[m/s]':<8} {'[m/s]':<8}")
        print("-" * 70)
        
        for hotspot in hotspots:
            print(f"{hotspot.amd_id:<10} {hotspot.risk_class:<8} "
                  f"{hotspot.O2_supply_rate:>13.2f}   "
                  f"{hotspot.friction_velocity:>6.3f}  "
                  f"{hotspot.wind_speed:>6.2f}")
        
        # Step 6: Summary statistics
        print("\n" + "-" * 70)
        print("STEP 6: Hotspot Statistics")
        print("-" * 70)
        
        O2_rates = [h.O2_supply_rate for h in hotspots]
        wind_speeds = [h.wind_speed for h in hotspots]
        u_stars = [h.friction_velocity for h in hotspots]
        
        print(f"\nOxygen Supply Rate Statistics:")
        print(f"  Mean:   {np.mean(O2_rates):8.2f} µmol/(m²·s)")
        print(f"  Median: {np.median(O2_rates):8.2f} µmol/(m²·s)")
        print(f"  Min:    {np.min(O2_rates):8.2f} µmol/(m²·s)")
        print(f"  Max:    {np.max(O2_rates):8.2f} µmol/(m²·s)")
        
        print(f"\nWind Speed Statistics:")
        print(f"  Mean:   {np.mean(wind_speeds):8.2f} m/s")
        print(f"  Min:    {np.min(wind_speeds):8.2f} m/s")
        print(f"  Max:    {np.max(wind_speeds):8.2f} m/s")
        
        print(f"\nFriction Velocity Statistics:")
        print(f"  Mean:   {np.mean(u_stars):8.3f} m/s")
        print(f"  Min:    {np.min(u_stars):8.3f} m/s")
        print(f"  Max:    {np.max(u_stars):8.3f} m/s")
        
        # Step 7: High-risk locations for monitoring
        print("\n" + "-" * 70)
        print("STEP 7: High-Risk Locations Requiring Monitoring")
        print("-" * 70)
        
        high_risk = [h for h in hotspots if h.risk_class == 'HIGH']
        if high_risk:
            print(f"\n⚠️  {len(high_risk)} high-risk location(s) identified:\n")
            for h in high_risk:
                amd_loc = detector.amd_locations[
                    next(i for i, loc in enumerate(detector.amd_locations)
                         if loc.point_id == h.amd_id)
                ]
                print(f"  • {h.amd_id} ({amd_loc.discharge_type})")
                print(f"    Location: ({amd_loc.x:.0f}, {amd_loc.y:.0f}, {amd_loc.z:.0f}) m")
                print(f"    O₂ supply: {h.O2_supply_rate:.2f} µmol/(m²·s)")
                print(f"    Wind speed: {h.wind_speed:.2f} m/s")
                if amd_loc.description:
                    print(f"    Notes: {amd_loc.description}")
                print()
        else:
            print("\n✓ No high-risk locations detected at current wind conditions")
        
        # Step 8: Output summary
        print("\n" + "-" * 70)
        print("STEP 8: Output Files")
        print("-" * 70)
        
        print(f"\n✓ Results saved to: {output_dir}/")
        print(f"  - GeoJSON: amd_hotspots.geojson (for visualization)")
        print(f"  - CSV: amd_hotspots.csv (for analysis)")
        
        print("\n" + "=" * 70)
        print("✓ Example completed successfully!")
        print("=" * 70)
        
        print("\nNext steps:")
        print("  1. Open amd_hotspots.geojson in QGIS or other GIS viewer")
        print("  2. Review high-risk locations for AMD monitoring priority")
        print("  3. Compare with field observations and actual AMD chemistry")
        print("  4. Couple with PHREEQC for reactive transport predictions")
        
        wind.finalize()
        return 0
        
    except Exception as e:
        print(f"\n✗ Error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
