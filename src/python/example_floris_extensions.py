#!/usr/bin/env python3
"""
example_floris_extensions.py - FLORIS wind farm integration examples

Demonstrates extended FLORIS integration features for wind farm simulation and analysis:
- Native FLORIS configuration file generation
- Wind data export with comprehensive meteorological metadata
- Turbine power and thrust coefficient curve generation
- Wind resource frequency distribution formatting
- Yaw control optimization results formatting

Run from src/python directory:
    python3 example_floris_extensions.py
"""

import sys
import os
import json
import numpy as np

# Ensure imports work
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from floris_extensions import (
    FLORISConfigExporter,
    EnhancedCSVExporter,
    PowerCurveGenerator,
    WindRoseFormatter,
    YawOptimizationFormatter,
    quick_floris_export
)


def export_native_config():
    """Export wind farm configuration to FLORIS native format."""
    print("\n" + "="*70)
    print("FLORIS Native Configuration Export")
    print("="*70)
    
    # Define a wind farm layout
    turbine_locations = [
        (300, 300),
        (800, 300),
        (1300, 300),
        (500, 700),
        (1000, 700)
    ]
    
    turbine_types = ["DTU10MW", "DTU10MW", "NREL15MW", "DTU10MW", "NREL15MW"]
    hub_heights = [90, 90, 120, 90, 120]
    rotor_diameters = [178, 178, 240, 178, 240]
    
    # Export to FLORIS format
    config = FLORISConfigExporter.export_farm_config(
        turbine_locations=turbine_locations,
        turbine_types=turbine_types,
        hub_heights=hub_heights,
        rotor_diameters=rotor_diameters,
        wind_speed=10.0,
        wind_direction=270.0,
        turbulence_intensity=0.05,
        output_file="/tmp/example_floris_config.json"
    )
    
    print("\nGenerated FLORIS configuration structure:")
    print(f"  - Turbines: {len(turbine_locations)}")
    print(f"  - Wind speed: {config['wind_speed']} m/s")
    print(f"  - Wind direction: {config['wind_direction']}°")
    print(f"  - Config saved to /tmp/example_floris_config.json")


def export_enhanced_csv():
    """Export wind data with comprehensive meteorological metadata."""
    print("\n" + "="*70)
    print("Enhanced CSV Export with Meteorological Data")
    print("="*70)
    
    # Define turbine locations and simulated wind data
    turbine_locations = [
        (300, 300),
        (800, 300),
        (1300, 300)
    ]
    
    wind_data = [
        {'u': 8.5, 'v': 1.2, 'speed': 8.58, 'direction': 172.0, 'z': 90.0},
        {'u': 8.2, 'v': 1.1, 'speed': 8.27, 'direction': 171.5, 'z': 90.0},
        {'u': 7.9, 'v': 1.0, 'speed': 7.96, 'direction': 171.0, 'z': 120.0},
    ]
    
    hub_heights = [90, 90, 120]
    turbulence_intensities = [0.05, 0.06, 0.04]
    wind_shear_exponents = [0.2, 0.21, 0.19]
    air_densities = [1.225, 1.226, 1.220]
    power_outputs = [1500000, 1400000, 2100000]
    yaw_angles = [0, 5, -3]
    
    EnhancedCSVExporter.export_with_metadata(
        turbine_locations=turbine_locations,
        wind_data=wind_data,
        hub_heights=hub_heights,
        turbulence_intensities=turbulence_intensities,
        wind_shear_exponents=wind_shear_exponents,
        air_densities=air_densities,
        power_outputs=power_outputs,
        yaw_angles=yaw_angles,
        output_file="/tmp/example_enhanced_wind_data.csv"
    )
    
    print("\nExported CSV includes:")
    print("  ✓ Wind speed and direction")
    print("  ✓ Turbulence intensity")
    print("  ✓ Wind shear exponent")
    print("  ✓ Air density")
    print("  ✓ Power output")
    print("  ✓ Yaw angles")
    
    # Show preview
    with open("/tmp/example_enhanced_wind_data.csv", 'r') as f:
        lines = f.readlines()
        print("\nCSV header:")
        print("  " + lines[0].strip())


def generate_power_curves():
    """Generate power and thrust coefficient curves from simulation data."""
    print("\n" + "="*70)
    print("Power and Thrust Coefficient Curve Generation")
    print("="*70)
    
    # Simulated power curve data from simulations
    wind_speeds = [3.0, 5.0, 7.0, 9.0, 11.0, 13.0, 15.0, 20.0, 25.0]
    power_outputs = [0, 100000, 500000, 1000000, 1500000, 1800000, 2000000, 2000000, 0]
    thrust_coefficients = [0.8, 0.78, 0.75, 0.70, 0.60, 0.40, 0.20, 0.05, 0.0]
    
    # Generate power curve
    curve_data = PowerCurveGenerator.generate_from_turbine_data(
        turbine_id=0,
        wind_speeds=wind_speeds,
        power_outputs=power_outputs,
        thrust_coefficients=thrust_coefficients,
        output_file="/tmp/example_power_curve.json"
    )
    
    print("\nGenerated power curve characteristics:")
    print(f"  - Wind speed points: {len(wind_speeds)}")
    print(f"  - Maximum power: {max(power_outputs):,} W")
    print(f"  - Cut-in speed: ~3.0 m/s")
    print(f"  - Rated speed: ~13.0 m/s")
    print(f"  - Curve saved to /tmp/example_power_curve.json")


def format_wind_rose():
    """Format wind resource as frequency distribution."""
    print("\n" + "="*70)
    print("Wind Resource Frequency Distribution")
    print("="*70)
    
    # Simulate annual wind data
    np.random.seed(42)
    n_samples = 8760  # Annual hourly samples
    
    # Generate realistic wind distribution
    wind_speeds = np.random.weibull(2.0, n_samples) * 9 + 1
    wind_directions = np.random.uniform(0, 360, n_samples)
    
    # Create wind rose
    wind_rose = WindRoseFormatter.create_from_simulation(
        wind_speeds=wind_speeds.tolist(),
        wind_directions=wind_directions.tolist(),
        output_file="/tmp/example_wind_rose.json"
    )
    
    # Export frequency table
    WindRoseFormatter.export_frequency_table(
        wind_rose_data=wind_rose,
        output_file="/tmp/example_wind_rose_frequencies.csv"
    )
    
    print("\nWind rose statistics:")
    print(f"  - Annual samples: {wind_rose['metadata']['num_samples']}")
    print(f"  - Mean wind speed: {wind_rose['metadata']['wind_speed_mean_ms']:.2f} m/s")
    print(f"  - Wind speed std dev: {wind_rose['metadata']['wind_speed_std_ms']:.2f} m/s")
    print(f"  - Mean direction: {wind_rose['metadata']['wind_direction_mean_deg']:.1f}°")
    
    print("\nFrequency matrix dimensions:")
    n_speed_bins = len(wind_rose['wind_speed_bins']) - 1
    n_dir_bins = len(wind_rose['wind_direction_bins']) - 1
    print(f"  - {n_speed_bins} speed bins × {n_dir_bins} direction bins")


def format_yaw_optimization():
    """Format yaw control optimization results for FLORIS."""
    print("\n" + "="*70)
    print("Yaw Control Optimization Results")
    print("="*70)
    
    # Simulated yaw optimization results
    yaw_angles = [0, 5, -3, 8, -2]
    
    # Export yaw results
    results = YawOptimizationFormatter.export_yaw_results(
        yaw_angles=yaw_angles,
        wind_speed=10.0,
        wind_direction=270.0,
        improvement_pct=3.5,
        output_file="/tmp/example_yaw_optimization.json"
    )
    
    print("\nOptimization results:")
    print(f"  - Turbines optimized: {results['metadata']['num_turbines']}")
    print(f"  - Wind speed: {results['wind_speed_ms']} m/s")
    print(f"  - Wind direction: {results['wind_direction_deg']}°")
    print(f"  - Power improvement: {results['power_improvement_pct']}%")
    
    print("\nOptimized yaw angles:")
    for tid, yaw in sorted(results['turbine_yaw_mapping'].items()):
        print(f"  - Turbine {tid}: {yaw:.1f}°")
    
    # Also export as complete FLORIS farm config with yaw control
    turbine_locations = [(300, 300), (800, 300), (1300, 300), (500, 700), (1000, 700)]
    hub_heights = [90, 90, 120, 90, 120]
    rotor_diameters = [178, 178, 240, 178, 240]
    
    YawOptimizationFormatter.export_yaw_control_config(
        yaw_angles=yaw_angles,
        turbine_locations=turbine_locations,
        hub_heights=hub_heights,
        rotor_diameters=rotor_diameters,
        wind_speed=10.0,
        wind_direction=270.0,
        output_file="/tmp/example_yaw_control_config.json"
    )


def quick_export_all_formats():
    """Quick export combining multiple formats."""
    print("\n" + "="*70)
    print("Quick Export - All Formats in Single Call")
    print("="*70)
    
    turbine_locations = [
        (300, 300),
        (800, 300),
        (1300, 300)
    ]
    
    wind_data = [
        {'u': 8.5, 'v': 1.2, 'speed': 8.58, 'direction': 172.0, 'z': 90.0},
        {'u': 8.2, 'v': 1.1, 'speed': 8.27, 'direction': 171.5, 'z': 90.0},
        {'u': 7.9, 'v': 1.0, 'speed': 7.96, 'direction': 171.0, 'z': 120.0},
    ]
    
    hub_heights = [90, 90, 120]
    
    files = quick_floris_export(
        turbine_locations=turbine_locations,
        wind_data=wind_data,
        hub_heights=hub_heights,
        output_prefix="/tmp/quick_export"
    )
    
    print("\nGenerated export files:")
    for export_type, filename in files.items():
        print(f"  - {export_type}: {filename}")


def main():
    """Run all feature demonstrations."""
    print("\n" + "="*70)
    print("FLORIS Wind Farm Integration - Feature Demonstrations")
    print("="*70)
    
    try:
        export_native_config()
        export_enhanced_csv()
        generate_power_curves()
        format_wind_rose()
        format_yaw_optimization()
        quick_export_all_formats()
        
        print("\n" + "="*70)
        print("✓ All feature demonstrations completed successfully!")
        print("="*70)
        print("\nGenerated files in /tmp/:")
        os.system("ls -lh /tmp/example*.* /tmp/quick_export*.* 2>/dev/null || true")
        
        return 0
        
    except Exception as e:
        print(f"\n✗ Error: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
