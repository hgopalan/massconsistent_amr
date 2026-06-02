#!/usr/bin/env python3
"""
example_floris_export.py - Example usage of the standalone FLORIS coupling tool

This example shows how to:
1. Initialize and solve a wind field
2. Define turbine locations
3. Extract wind speeds at turbine hubs
4. Export in FLORIS-compatible format
5. (Optional) Use the data with FLORIS

No FLORIS installation is required for steps 1-4; FLORIS is only needed
if you want to actually run wind farm simulations in step 5.

To run this example:
    cd massconsistent_amr
    cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
    cmake --build build
    
    # Set Python path
    export PYTHONPATH=/path/to/build/python:$PYTHONPATH
    
    # Run the example (adjust paths as needed)
    python3 src/python/example_floris_export.py
"""

import sys
import os
import numpy as np


def example_basic_export():
    """
    Example 1: Basic wind extraction and export
    
    This is the simplest workflow: solve wind, export at turbine locations.
    """
    print("\n" + "="*70)
    print("Example 1: Basic Wind Export")
    print("="*70)
    
    try:
        from wind_solver import WindSolver
        from floris_coupling import FLORISWindMap, quick_export
    except ImportError as e:
        print(f"Error: Could not import required modules")
        print(f"Make sure to set PYTHONPATH: export PYTHONPATH=build/python:$PYTHONPATH")
        return False
    
    # Create simple test inputs
    terrain_content = """0.0 0.0 100.0
1000.0 0.0 100.0
0.0 1000.0 100.0
1000.0 1000.0 120.0
500.0 500.0 110.0
"""
    
    inputs_content = """
# Test wind solver inputs
terrain_file = /tmp/test_floris_terrain.csv
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1
dx = 50.0
dy = 50.0
dz = 50.0
domain_height = 300.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 0
tol_rel = 1.e-8
"""
    
    # Write test files
    with open("/tmp/test_floris_terrain.csv", "w") as f:
        f.write(terrain_content)
    
    with open("/tmp/test_floris_inputs.i", "w") as f:
        f.write(inputs_content)
    
    try:
        # Initialize and solve
        print("\n1. Initializing wind solver...")
        wind = WindSolver("/tmp/test_floris_inputs.i")
        
        print("\n2. Solving for mass-consistent wind field...")
        wind.solve()
        
        # Define some turbine locations
        print("\n3. Defining turbine locations...")
        turbines = [
            (200, 200),
            (400, 200),
            (600, 200),
            (300, 500),
            (500, 500),
        ]
        print(f"   {len(turbines)} turbines at positions: {turbines}")
        
        # Method 1: Using quick_export (simplest)
        print("\n4. Exporting wind data (quick method)...")
        hub_height = 90.0
        output_csv = "/tmp/floris_wind_export.csv"
        reference_speed = 10.0  # For speed-up ratios
        
        wind_data = quick_export(
            wind,
            turbines,
            hub_height=hub_height,
            output_file=output_csv,
            reference_speed=reference_speed
        )
        
        # Display results
        print("\n5. Wind data summary:")
        for i, (turbine, wind_info) in enumerate(wind_data['turbines'].items()):
            print(f"\n   Turbine {i}:")
            print(f"      Location: ({wind_info['x']:.1f}, {wind_info['y']:.1f})")
            print(f"      Wind speed: {wind_info['speed']:.2f} m/s")
            print(f"      Direction: {wind_info['direction']:.1f}°")
            print(f"      Speed-up ratio: {wind_info['speedup_ratio']:.3f}")
        
        print(f"\n   ✓ Full results saved to {output_csv}")
        
        # Method 2: Using FLORISWindMap class (more control)
        print("\n6. Alternative: Using FLORISWindMap class for more control...")
        wind_map = FLORISWindMap(wind)
        
        # Export to different formats
        output_json = "/tmp/floris_wind_export.json"
        wind_map.export_to_json(turbines, hub_height, output_json, reference_speed)
        print(f"   ✓ JSON export: {output_json}")
        
        # Get 2D speed map at hub height
        print("\n7. Getting 2D speed map...")
        speed_map, x_coords, y_coords = wind_map.get_speed_map_2d(hub_height)
        print(f"   Speed map shape: {speed_map.shape}")
        print(f"   Speed range: [{np.nanmin(speed_map):.2f}, {np.nanmax(speed_map):.2f}] m/s")
        
        print("\n" + "="*70)
        print("✓ Example 1 completed successfully!")
        print("="*70)
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"\n✗ Example failed: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        # Cleanup
        for f in ["/tmp/test_floris_terrain.csv", "/tmp/test_floris_inputs.i",
                  "/tmp/floris_wind_export.csv", "/tmp/floris_wind_export.json"]:
            if os.path.exists(f):
                os.remove(f)


def example_programmatic_usage():
    """
    Example 2: Programmatic usage without files
    
    Useful when running within another application.
    """
    print("\n" + "="*70)
    print("Example 2: Programmatic Usage")
    print("="*70)
    
    try:
        from wind_solver import WindSolver
        from floris_coupling import FLORISWindMap
    except ImportError as e:
        print(f"Error: Could not import required modules")
        return False
    
    # Create inline test data
    terrain_content = """0.0 0.0 50.0
100.0 0.0 50.0
0.0 100.0 50.0
100.0 100.0 60.0
50.0 50.0 55.0
"""
    
    inputs_content = """
terrain_file = /tmp/prog_terrain.csv
init_mode = uniform
uniform_U = 8.0
uniform_V = 2.0
dx = 25.0
dy = 25.0
dz = 25.0
domain_height = 150.0
mlmg_verbose = 0
"""
    
    # Write files
    with open("/tmp/prog_terrain.csv", "w") as f:
        f.write(terrain_content)
    
    with open("/tmp/prog_inputs.i", "w") as f:
        f.write(inputs_content)
    
    try:
        # Solve
        wind = WindSolver("/tmp/prog_inputs.i")
        wind.solve()
        
        # Create wind map
        wind_map = FLORISWindMap(wind)
        
        # Get wind at arbitrary points (not necessarily grid-aligned)
        print("\nQuerying wind at arbitrary locations:")
        test_points = [
            (25.0, 25.0, 80.0),   # (x, y, z in meters)
            (50.0, 50.0, 90.0),
            (75.0, 75.0, 85.0),
        ]
        
        for x, y, z in test_points:
            wind_pt = wind_map.get_wind_at_point(x, y, z)
            print(f"\n  Point ({x}, {y}, {z}):")
            print(f"    Speed: {wind_pt['speed']:.2f} m/s")
            print(f"    Direction: {wind_pt['direction']:.1f}°")
        
        # Get wind at turbine locations (automatic terrain handling)
        print("\nQuerying wind at turbine locations (terrain-aware):")
        turbine_locs = [(30, 30), (70, 70)]
        hub_height = 80.0
        
        turbine_winds = wind_map.get_wind_at_turbines(turbine_locs, hub_height)
        for i, wind_pt in enumerate(turbine_winds):
            print(f"\n  Turbine {i}:")
            print(f"    Location: ({wind_pt['x']:.1f}, {wind_pt['y']:.1f})")
            print(f"    Hub height (absolute): {wind_pt['z']:.1f} m")
            print(f"    Wind speed: {wind_pt['speed']:.2f} m/s")
        
        print("\n" + "="*70)
        print("✓ Example 2 completed successfully!")
        print("="*70)
        
        wind.finalize()
        return True
        
    except Exception as e:
        print(f"\n✗ Example failed: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        for f in ["/tmp/prog_terrain.csv", "/tmp/prog_inputs.i"]:
            if os.path.exists(f):
                os.remove(f)


def example_with_floris():
    """
    Example 3: Integration with FLORIS (optional)
    
    This example shows how to use the exported wind data with FLORIS.
    FLORIS must be installed separately.
    
    Note: This is a template - FLORIS API may vary by version.
    """
    print("\n" + "="*70)
    print("Example 3: Integration with FLORIS (optional)")
    print("="*70)
    
    try:
        import floris
    except ImportError:
        print("\nFLORIS not installed. To use this example:")
        print("  pip install floris")
        print("\nSkipping FLORIS integration example.")
        return True
    
    try:
        from wind_solver import WindSolver
        from floris_coupling import FLORISWindMap
    except ImportError:
        print("Could not import wind solver modules")
        return False
    
    print("\n(This is a template - actual FLORIS usage depends on your version)")
    print("\nTypical workflow:")
    print("  1. Export wind with massconsistent_amr (this tool)")
    print("  2. Load wind data in FLORIS")
    print("  3. Run wind farm simulation")
    print("  4. Analyze power production with speed-up effects")
    
    return True


def main():
    """Run all examples"""
    print("\n" + "="*70)
    print("FLORIS Coupling Examples")
    print("="*70)
    
    examples = [
        ("Basic Export", example_basic_export),
        ("Programmatic Usage", example_programmatic_usage),
        ("FLORIS Integration", example_with_floris),
    ]
    
    results = []
    for name, example_func in examples:
        try:
            result = example_func()
            results.append((name, result))
        except Exception as e:
            print(f"\n✗ Example crashed: {e}")
            import traceback
            traceback.print_exc()
            results.append((name, False))
    
    # Summary
    print("\n" + "="*70)
    print("Summary")
    print("="*70)
    
    for name, result in results:
        status = "✓ PASSED" if result else "✗ FAILED"
        print(f"{name:.<50} {status}")
    
    passed = sum(1 for _, r in results if r)
    total = len(results)
    print(f"\nPassed: {passed}/{total}")
    
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
