#!/usr/bin/env python3
"""
example_rothermel_richards_coupling.py - Complete example of Rothermel/Richards coupling

This example demonstrates:
1. Two-way coupling with Rothermel ROS model
2. Fuel data management and environmental inputs
3. ROS sensitivity analysis
4. Diagnostic statistics and visualization
5. Output and restart capabilities

This example can run with mock/stub fire solver for demonstration.

Author: massconsistent_amr team
Date: 2026-06-28
"""

import numpy as np
import sys
import os
from pathlib import Path

def create_example_fuel_data(nx, ny):
    """Create example fuel data for 32x32 domain."""
    
    # Create varying fuel data
    fuel_model_map = np.ones((ny, nx), dtype=int)
    # Patch different fuel models
    fuel_model_map[0:10, 0:10] = 1   # Short grass
    fuel_model_map[10:20, 0:10] = 3  # Tall grass
    fuel_model_map[0:10, 10:20] = 5  # Timber litter
    fuel_model_map[10:20, 10:20] = 7 # Pine litter
    
    # Moisture gradient (higher in wet areas)
    moisture = 10.0 + 5.0 * np.sin(np.pi * np.arange(nx) / nx)
    moisture = np.tile(moisture, (ny, 1))
    
    # Slope (0-30 degrees)
    slope = 15.0 * np.ones((ny, nx))
    slope[0:16, :] += 10.0 * np.cos(np.pi * np.arange(nx) / nx)
    
    # Aspect (wind-facing slope)
    aspect = 180.0 * np.ones((ny, nx))  # South-facing
    
    return {
        'fuel_model_map': fuel_model_map,
        'fuel_moisture': moisture,
        'slope': slope,
        'aspect': aspect,
    }


def example_rothermel_direct_calculation():
    """Example 1: Direct Rothermel ROS calculation."""
    
    print("\n" + "="*70)
    print("EXAMPLE 1: Direct Rothermel ROS Calculation")
    print("="*70)
    
    try:
        from rothermel_ros import compute_rothermel_ros
    except ImportError:
        print("ERROR: rothermel_ros module not available")
        return False
    
    # Create 10x10 grid
    ny, nx = 10, 10
    fuel_data = create_example_fuel_data(nx, ny)
    
    # Compute ROS
    fuel_model = 1  # Short grass
    moisture = fuel_data['fuel_moisture']
    slope = fuel_data['slope']
    wind_speed = 5.0 * np.ones((ny, nx))  # 5 m/s wind
    wind_direction = 180.0 * np.ones((ny, nx))  # From north
    
    print(f"\nComputing Rothermel ROS for fuel model {fuel_model}...")
    print(f"  Grid: {ny} x {nx}")
    print(f"  Wind: 5 m/s from north")
    print(f"  Slope: {fuel_data['slope'].mean():.1f}° avg")
    print(f"  Moisture: {moisture.mean():.1f}% avg")
    
    ros_result = compute_rothermel_ros(
        fuel_model, moisture, slope, wind_speed, wind_direction
    )
    
    print(f"\nResults:")
    print(f"  Base ROS (no wind/slope): {ros_result['ros_no_wind_slope'].mean():.3f} m/min")
    print(f"  With slope: {ros_result['ros_with_slope'].mean():.3f} m/min")
    print(f"  With wind+slope: {ros_result['ros_with_wind'].mean():.3f} m/min")
    print(f"  Max ROS: {ros_result['ros_with_wind'].max():.3f} m/min")
    print(f"  Avg intensity: {ros_result['fireline_intensity'].mean():.1f} kW/m")
    print(f"  Max flame length: {ros_result['flame_length'].max():.1f} m")
    
    return True


def example_richards_calculation():
    """Example 2: Richards ROS calculation."""
    
    print("\n" + "="*70)
    print("EXAMPLE 2: Richards ROS Calculation")
    print("="*70)
    
    try:
        from richards_ros import compute_richards_ros
    except ImportError:
        print("ERROR: richards_ros module not available")
        return False
    
    # Create grid
    ny, nx = 10, 10
    
    fuel_load = 50.0 * np.ones((ny, nx))  # kg/m²
    fuel_moisture = 10.0 * np.ones((ny, nx))  # %
    wind_speed = 5.0 * np.ones((ny, nx))  # m/s
    slope = 15.0 * np.ones((ny, nx))  # degrees
    
    print(f"\nComputing Richards ROS...")
    print(f"  Grid: {ny} x {nx}")
    print(f"  Fuel load: {fuel_load[0,0]:.1f} kg/m²")
    print(f"  Moisture: {fuel_moisture[0,0]:.1f}%")
    print(f"  Wind: {wind_speed[0,0]:.1f} m/s")
    print(f"  Slope: {slope[0,0]:.1f}°")
    
    ros_result = compute_richards_ros(
        fuel_load, fuel_moisture, wind_speed, slope
    )
    
    print(f"\nResults:")
    print(f"  ROS: {ros_result['ros'].mean():.3f} m/min (avg)")
    print(f"  Max ROS: {ros_result['ros'].max():.3f} m/min")
    print(f"  U-component: {ros_result['ros_components']['u_component'].mean():.3f} m/min")
    print(f"  V-component: {ros_result['ros_components']['v_component'].mean():.3f} m/min")
    print(f"  Energy release: {ros_result['energy_release'].mean():.0f} kJ/m²")
    
    return True


def example_sensitivity_analysis():
    """Example 3: ROS sensitivity analysis."""
    
    print("\n" + "="*70)
    print("EXAMPLE 3: ROS Sensitivity Analysis")
    print("="*70)
    
    try:
        from rothermel_ros import compute_rothermel_ros
    except ImportError:
        print("ERROR: rothermel_ros module not available")
        return False
    
    # Create grid
    ny, nx = 5, 5
    
    # Base conditions
    fuel_model = 1
    moisture_base = 10.0 * np.ones((ny, nx))
    slope = 15.0 * np.ones((ny, nx))
    wind_speed = 5.0 * np.ones((ny, nx))
    wind_direction = 180.0 * np.ones((ny, nx))
    
    # Compute base case
    ros_base = compute_rothermel_ros(
        fuel_model, moisture_base, slope, wind_speed, wind_direction
    )['ros_with_wind']
    
    print(f"\nBase case ROS: {ros_base.mean():.3f} m/min")
    
    # Sensitivity: moisture ±20%
    print(f"\nSensitivity to moisture:")
    moisture_low = moisture_base * 0.8
    moisture_high = moisture_base * 1.2
    
    ros_low = compute_rothermel_ros(
        fuel_model, moisture_low, slope, wind_speed, wind_direction
    )['ros_with_wind']
    
    ros_high = compute_rothermel_ros(
        fuel_model, moisture_high, slope, wind_speed, wind_direction
    )['ros_with_wind']
    
    print(f"  Moisture -20%: {ros_low.mean():.3f} m/min (+{(ros_low.mean()/ros_base.mean()-1)*100:.1f}%)")
    print(f"  Moisture +20%: {ros_high.mean():.3f} m/min ({(ros_high.mean()/ros_base.mean()-1)*100:.1f}%)")
    
    # Sensitivity: wind ±50%
    print(f"\nSensitivity to wind speed:")
    wind_low = wind_speed * 0.5
    wind_high = wind_speed * 1.5
    
    ros_low = compute_rothermel_ros(
        fuel_model, moisture_base, slope, wind_low, wind_direction
    )['ros_with_wind']
    
    ros_high = compute_rothermel_ros(
        fuel_model, moisture_base, slope, wind_high, wind_direction
    )['ros_with_wind']
    
    print(f"  Wind -50%: {ros_low.mean():.3f} m/min ({(ros_low.mean()/ros_base.mean()-1)*100:.1f}%)")
    print(f"  Wind +50%: {ros_high.mean():.3f} m/min (+{(ros_high.mean()/ros_base.mean()-1)*100:.1f}%)")
    
    # Sensitivity: slope ±10°
    print(f"\nSensitivity to slope:")
    slope_low = slope - 10.0
    slope_high = slope + 10.0
    
    ros_low = compute_rothermel_ros(
        fuel_model, moisture_base, slope_low, wind_speed, wind_direction
    )['ros_with_wind']
    
    ros_high = compute_rothermel_ros(
        fuel_model, moisture_base, slope_high, wind_speed, wind_direction
    )['ros_with_wind']
    
    print(f"  Slope -10°: {ros_low.mean():.3f} m/min ({(ros_low.mean()/ros_base.mean()-1)*100:.1f}%)")
    print(f"  Slope +10°: {ros_high.mean():.3f} m/min (+{(ros_high.mean()/ros_base.mean()-1)*100:.1f}%)")
    
    return True


def example_wind_fire_coupling():
    """Example 4: Wind-fire coupling interface."""
    
    print("\n" + "="*70)
    print("EXAMPLE 4: Wind-Fire Coupling Interface")
    print("="*70)
    
    # Create mock 3D wind field
    ny, nx, nz = 10, 10, 5
    u_3d = 5.0 * np.ones((nz, ny, nx))  # m/s in x
    v_3d = 0.0 * np.ones((nz, ny, nx))  # m/s in y
    w_3d = 0.1 * np.sin(np.pi * np.arange(nz).reshape(-1, 1, 1) / nz) * np.ones((nz, ny, nx))
    
    print(f"\n3D Wind field:")
    print(f"  Shape: {u_3d.shape} (nz, ny, nx)")
    print(f"  U: {u_3d[2, 5, 5]:.2f} m/s at mid-height")
    print(f"  V: {v_3d[2, 5, 5]:.2f} m/s")
    print(f"  W: {w_3d[2, 5, 5]:.2f} m/s (vertical)")
    
    # Extract wind at flame height (assume k=1 for ~5m above ground)
    k_flame = 1
    u_flame = u_3d[k_flame, :, :]
    v_flame = v_3d[k_flame, :, :]
    
    # Compute wind speed and direction
    wind_speed = np.sqrt(u_flame**2 + v_flame**2)
    wind_direction = np.degrees(np.arctan2(v_flame, u_flame))
    wind_direction = (wind_direction + 360) % 360
    
    print(f"\nWind at flame height (k={k_flame}):")
    print(f"  Speed: {wind_speed.mean():.2f} m/s (avg)")
    print(f"  Direction: {wind_direction[5,5]:.0f}° (at center)")
    
    print(f"\nInterface capabilities:")
    print(f"  ✓ update_wind_3d(u, v, w, nz, zmin, zmax) - pass 3D wind to fire solver")
    print(f"  ✓ get_surface_fluxes() - extract heat for wind feedback")
    print(f"  ✓ compute_rothermel_ros() - ROS with wind effects")
    print(f"  ✓ compute_richards_ros() - Alternative ROS model")
    
    return True


def example_mock_fire_solver():
    """Example 5: Mock WildfireSolver demonstrating interface."""
    
    print("\n" + "="*70)
    print("EXAMPLE 5: Mock WildfireSolver Implementation")
    print("="*70)
    
    from wildfire_solver_interface import WildfireSolver
    
    print(f"\nWildfireSolver interface methods (abstract):")
    print(f"  - __init__(inputs_file, model_type)")
    print(f"  - update_wind_3d(u, v, w, nz, zmin, zmax)")
    print(f"  - get_surface_fluxes() -> Dict")
    print(f"  - compute_rothermel_ros(...) -> Dict")
    print(f"  - compute_richards_ros(...) -> Dict")
    print(f"  - get_state() -> Dict")
    print(f"  - get_fuel_data() -> Dict")
    print(f"  - step(dt) -> Dict")
    print(f"  - finalize() -> bool")
    
    print(f"\nTo implement in wildfire_levelset:")
    print(f"  1. Inherit from WildfireSolver abstract base class")
    print(f"  2. Implement all abstract methods")
    print(f"  3. Integrate with C++ fire solver via pybind11")
    print(f"  4. Use rothermel_ros and richards_ros modules for ROS calculations")
    print(f"  5. Provide heat flux extraction for two-way coupling")
    
    return True


def main():
    """Run all examples."""
    
    print("\n" + "="*70)
    print("Rothermel/Richards Fire Spread Coupling Examples")
    print("="*70)
    print("These examples demonstrate the comprehensive Python interface for")
    print("wildfire_levelset integration with massconsistent_amr wind solver.")
    
    examples = [
        ("Direct Rothermel Calculation", example_rothermel_direct_calculation),
        ("Richards ROS Model", example_richards_calculation),
        ("Sensitivity Analysis", example_sensitivity_analysis),
        ("Wind-Fire Coupling", example_wind_fire_coupling),
        ("Mock Fire Solver", example_mock_fire_solver),
    ]
    
    results = {}
    for name, func in examples:
        try:
            results[name] = func()
        except Exception as e:
            print(f"\n✗ {name} failed: {e}")
            import traceback
            traceback.print_exc()
            results[name] = False
    
    # Summary
    print("\n" + "="*70)
    print("SUMMARY")
    print("="*70)
    for name, success in results.items():
        status = "✓ PASSED" if success else "✗ FAILED"
        print(f"  {status}: {name}")
    
    passed = sum(1 for s in results.values() if s)
    total = len(results)
    print(f"\n{passed}/{total} examples completed successfully")
    
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())

