#!/usr/bin/env python3
"""
test_phase1_wind_farm.py - Wind farm tools demonstration

This example demonstrates three wind farm utilities:
1. CSV Turbine Definition Format (read/write layouts)
2. Wind Resource Summary Statistics (compute wind statistics)
3. PyOptimization Output Formatting (export results)

Run from tests_and_examples directory:
    python3 phase1_features/test_phase1_wind_farm.py
"""

import sys
import os
import json
import numpy as np

# Add src/python to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'src', 'python'))

from turbine_io import TurbineLayout
from wind_resource_stats import WindResourceStats
from pyoptimization_export import PyOptimizationExporter


def main():
    """Run the wind farm tools demonstration."""
    print("\n" + "=" * 80)
    print("Wind Farm Tools Demonstration")
    print("=" * 80)
    
    # =========================================================================
    # CSV Turbine Definition Format
    # =========================================================================
    print("\nCSV Turbine Definition Format")
    print("-" * 80)
    
    # Create a turbine layout programmatically
    layout = TurbineLayout()
    layout.domain_bounds = {
        'xmin': 0.0, 'xmax': 2000.0,
        'ymin': 0.0, 'ymax': 1000.0
    }
    
    # Add turbines
    layout.add_turbine(
        turbine_id=0,
        x=300.0,
        y=300.0,
        z_agl=0.0,
        turbine_type="DTU10MW",
        hub_height=90.0,
        rotor_diameter=178.0,
        power_curve_file="power_curves/dtu10mw.json"
    )
    layout.add_turbine(
        turbine_id=1,
        x=800.0,
        y=300.0,
        z_agl=0.0,
        turbine_type="DTU10MW",
        hub_height=90.0,
        rotor_diameter=178.0,
        power_curve_file="power_curves/dtu10mw.json"
    )
    layout.add_turbine(
        turbine_id=2,
        x=1300.0,
        y=300.0,
        z_agl=0.0,
        turbine_type="NREL15MW",
        hub_height=120.0,
        rotor_diameter=240.0,
        power_curve_file="power_curves/nrel15mw.json"
    )
    layout.add_turbine(
        turbine_id=3,
        x=500.0,
        y=700.0,
        z_agl=50.0,
        turbine_type="DTU10MW",
        hub_height=90.0,
        rotor_diameter=178.0
    )
    
    print(f"✓ Created layout with {len(layout)} turbines")
    
    # Write to CSV
    csv_file = "phase1_turbines.csv"
    TurbineLayout.write_csv(layout, csv_file)
    print(f"✓ Wrote turbine layout to {csv_file}")
    
    # Read back from CSV
    layout_read = TurbineLayout.read_csv(csv_file)
    print(f"✓ Read turbine layout from {csv_file} ({len(layout_read)} turbines)")
    
    # Validate spacing
    is_valid, errors = layout_read.validate_spacing(min_spacing=400.0)
    if is_valid:
        print("✓ Turbine spacing validation passed (min spacing 400m)")
    else:
        print("⚠ Spacing validation warnings:")
        for error in errors:
            print(f"  - {error}")
    
    # =========================================================================
    # Wind Resource Summary Statistics
    # =========================================================================
    print("\nWind Resource Summary Statistics")
    print("-" * 80)
    
    # Create synthetic wind field at hub height (90m AGL)
    # Simulate varying wind speeds with realistic spatial patterns
    ny, nx = 30, 40
    rng = np.random.RandomState(42)
    
    # Base wind with spatial variation
    u_base = 10.0 + rng.normal(0, 1.0, (ny, nx))  # Mean 10 m/s with variation
    v_base = 2.0 + rng.normal(0, 0.5, (ny, nx))   # Mean 2 m/s northerly component
    
    # Ensure no negative values and reasonable bounds
    u_field = np.clip(u_base, 3.0, 20.0)
    v_field = np.clip(v_base, -5.0, 5.0)
    
    print(f"✓ Created synthetic wind field: {ny}×{nx} grid")
    print(f"  u-component range: [{u_field.min():.2f}, {u_field.max():.2f}] m/s")
    print(f"  v-component range: [{v_field.min():.2f}, {v_field.max():.2f}] m/s")
    
    # Compute wind resource statistics
    stats = WindResourceStats.compute_from_wind_field(
        u_field, v_field, height_agl=90.0
    )
    
    # Print summary
    print(stats.summary_string())
    
    # Export statistics to JSON
    stats_json = "phase1_wind_stats.json"
    stats.to_json(stats_json)
    print(f"✓ Exported wind statistics to {stats_json}")
    
    # =========================================================================
    # PyOptimization Output Formatting
    # =========================================================================
    print("\nPyOptimization Output Formatting")
    print("-" * 80)
    
    # Create exporter
    exporter = PyOptimizationExporter("Example_Wind_Farm")
    
    # Add turbine results with realistic power outputs
    turbine_powers = [3500.0, 3200.0, 6500.0, 3000.0]  # kW per turbine
    
    for i, (turbine, power) in enumerate(zip(layout_read.turbines, turbine_powers)):
        # Interpolate wind to turbine location (simplified)
        ix = int((turbine['x'] / 2000.0) * (nx - 1))
        iy = int((turbine['y'] / 1000.0) * (ny - 1))
        ix = np.clip(ix, 0, nx - 1)
        iy = np.clip(iy, 0, ny - 1)
        
        u_at_turbine = u_field[iy, ix]
        v_at_turbine = v_field[iy, ix]
        
        speed = np.sqrt(u_at_turbine**2 + v_at_turbine**2)
        direction = np.degrees(np.arctan2(u_at_turbine, v_at_turbine)) % 360.0
        
        exporter.add_turbine_result(
            turbine_id=turbine['id'],
            x=turbine['x'],
            y=turbine['y'],
            power_kw=power,
            wind_speed_ms=float(speed),
            wind_direction_deg=float(direction),
            thrust_coefficient=0.82,
            hub_height=turbine['hub_height'],
            rotor_diameter=turbine['rotor_diameter'],
            turbine_type=turbine['turbine_type']
        )
    
    print(f"✓ Added {len(exporter.turbines)} turbine results to exporter")
    
    # Set farm-level aggregates
    total_power = sum(turbine_powers)
    aep = total_power * 24.0 * 365.0 / 1_000_000.0  # Simplified AEP in GWh
    
    exporter.set_farm_power(
        total_power_kw=total_power,
        annual_energy_gwh=aep
    )
    print(f"✓ Set farm power: {total_power:.1f} kW, AEP: {aep:.2f} GWh/year")
    
    # Set wind resource statistics
    exporter.set_wind_resource(
        mean_speed_ms=stats.mean_speed,
        mean_direction_deg=stats.mean_direction,
        std_speed_ms=stats.std_speed,
        std_direction_deg=stats.std_direction,
        height_agl=90.0,
        turbulence_intensity=0.08
    )
    
    # Export results
    json_file = "phase1_results.json"
    exporter.export_json(json_file, pretty=True)
    print(f"✓ Exported results to {json_file}")
    
    csv_turbine_file = "phase1_turbine_results.csv"
    exporter.export_turbine_csv(csv_turbine_file)
    print(f"✓ Exported turbine-level results to {csv_turbine_file}")
    
    csv_summary_file = "phase1_farm_summary.csv"
    exporter.export_summary_csv(csv_summary_file)
    print(f"✓ Exported farm summary to {csv_summary_file}")
    
    # =========================================================================
    # Display sample outputs
    # =========================================================================
    print("\n" + "=" * 80)
    print("Sample Output: PyOptimization JSON Export")
    print("=" * 80)
    
    with open(json_file, 'r') as f:
        data = json.load(f)
    
    print(json.dumps(data, indent=2)[:800] + "\n... (truncated)")
    
    # =========================================================================
    # Cleanup
    # =========================================================================
    print("\n" + "=" * 80)
    print("Demonstration Complete")
    print("=" * 80)
    print("\nGenerated files:")
    print(f"  - {csv_file}")
    print(f"  - {stats_json}")
    print(f"  - {json_file}")
    print(f"  - {csv_turbine_file}")
    print(f"  - {csv_summary_file}")
    print("\n✓ All wind farm utilities demonstrated successfully!")


if __name__ == '__main__':
    main()
