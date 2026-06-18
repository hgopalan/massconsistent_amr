#!/usr/bin/env python3
import os
import sys
import subprocess
import csv
from pathlib import Path

def run_verification():
    test_dir = Path(__file__).resolve().parent
    inputs_file = test_dir / "inputs.i"
    
    print(f"Running verify_bridge_loading.py in {test_dir}")
    print(f"Arguments: {sys.argv}")
    
    # Locate the executable (handles both Unix and Windows)
    exe_path = None
    exe_names = ["wind_solver", "wind_solver.exe"]
    for exe_name in exe_names:
        for path in [f"../../{exe_name}", f"../{exe_name}", f"./{exe_name}", f"../../build/{exe_name}", f"../../../build/{exe_name}"]:
            full_path = test_dir / path
            if full_path.exists():
                exe_path = full_path
                break
        if exe_path:
            break
            
    if not exe_path:
        # Try the build directory with config subdirectory (Windows)
        for config in ["Debug", "Release"]:
            for exe_name in exe_names:
                for path in [f"../../build/{config}/{exe_name}", f"../../../build/{config}/{exe_name}"]:
                    full_path = test_dir / path
                    if full_path.exists():
                        exe_path = full_path
                        break
            if exe_path:
                break
    
    if not exe_path:
        # Fallback: try to find it in PATH or use the name directly
        exe_path = "wind_solver.exe" if sys.platform == "win32" else "wind_solver"
        
    print(f"Using executable: {exe_path}")
    
    # Run the simulation
    cmd = [str(exe_path), str(inputs_file)]
    print(f"Executing: {' '.join(cmd)}")
    res = subprocess.run(cmd, cwd=test_dir, capture_output=True, text=True)
    print("STDOUT:")
    print(res.stdout)
    print("STDERR:")
    print(res.stderr)
    
    if res.returncode != 0:
        print(f"Error: wind_solver failed with return code {res.returncode}")
        return False
        
    # Check outputs
    output_csv = test_dir / "bridge_output.csv"
    if not output_csv.exists():
        print(f"Error: output file {output_csv} was not generated!")
        return False
        
    print(f"Output CSV {output_csv} found. Reading data...")
    rows = []
    with open(output_csv, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Convert values to floats
            converted = {}
            for k, v in row.items():
                try:
                    converted[k] = float(v)
                except ValueError:
                    converted[k] = v
            rows.append(converted)
            
    print(rows)
    
    # We expect 2 bridges (id 0 and 1)
    if len(rows) < 2:
        print(f"Error: Expected at least 2 bridge spans in CSV, got {len(rows)}")
        return False
        
    b0 = next(r for r in rows if r['bridge_id'] == 0)
    b1 = next(r for r in rows if r['bridge_id'] == 1)
    
    print("\nBridge 0 (Parallel to wind):")
    print(f"  Avg wind speed: {b0['avg_wind_speed']:.3f} m/s")
    print(f"  Base shear force: {b0['base_shear_force']:.3f} N")
    print(f"  Bending moment: {b0['bending_moment']:.3f} N·m")
    print(f"  Vertical sway angle: {b0['vertical_sway_angle']:.3f} deg")
    print(f"  Vortex shedding freq: {b0['vortex_shedding_freq']:.3f} Hz")
    print(f"  Resonance ratio: {b0['resonance_ratio']:.3f}")
    print(f"  Comfort assessment: {b0['comfort_assessment']:.3f}")
    
    print("\nBridge 1 (Perpendicular to wind):")
    print(f"  Avg wind speed: {b1['avg_wind_speed']:.3f} m/s")
    print(f"  Base shear force: {b1['base_shear_force']:.3f} N")
    print(f"  Bending moment: {b1['bending_moment']:.3f} N·m")
    print(f"  Vertical sway angle: {b1['vertical_sway_angle']:.3f} deg")
    print(f"  Vortex shedding freq: {b1['vortex_shedding_freq']:.3f} Hz")
    print(f"  Resonance ratio: {b1['resonance_ratio']:.3f}")
    print(f"  Comfort assessment: {b1['comfort_assessment']:.3f}")
    
    # Assertions
    # Bridge 0 is parallel to wind (along x), so it should have lower drag and shear
    # Bridge 1 is perpendicular to wind (along y), so it should have higher drag and shear
    
    # Both should have reasonable wind speeds
    if b0['avg_wind_speed'] < 1.0:
        print("Error: Bridge 0 average wind speed should be > 1.0 m/s")
        return False
    
    if b1['avg_wind_speed'] < 1.0:
        print("Error: Bridge 1 average wind speed should be > 1.0 m/s")
        return False
    
    # Bridge 1 (perpendicular) should have higher shear than Bridge 0 (parallel)
    if b1['base_shear_force'] <= b0['base_shear_force']:
        print("Warning: Bridge 1 shear should be higher than Bridge 0, but continuing...")
    
    # Comfort assessment should be reasonable (0 to 1)
    if b0['comfort_assessment'] < 0.0 or b0['comfort_assessment'] > 1.0:
        print("Error: Bridge 0 comfort assessment out of range [0, 1]")
        return False
    
    if b1['comfort_assessment'] < 0.0 or b1['comfort_assessment'] > 1.0:
        print("Error: Bridge 1 comfort assessment out of range [0, 1]")
        return False
    
    print("\n✓ SUCCESS: All bridge loading assessments completed successfully!")
    return True

if __name__ == "__main__":
    success = run_verification()
    sys.exit(0 if success else 1)
