#!/usr/bin/env python3
import os
import sys
import subprocess
import csv
from pathlib import Path

def run_verification():
    test_dir = Path(__file__).resolve().parent
    inputs_file = test_dir / "inputs.i"
    
    print(f"Running verify_wire_loading.py in {test_dir}")
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
    output_csv = test_dir / "wire_output.csv"
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
    
    # We expect 2 wires (id 0 and 1)
    if len(rows) < 2:
        print(f"Error: Expected at least 2 wire spans in CSV, got {len(rows)}")
        return False
        
    w0 = next(r for r in rows if r['wire_id'] == 0)
    w1 = next(r for r in rows if r['wire_id'] == 1)
    
    print("\nSpan 0 (Parallel to wind):")
    print(f"  Avg wind speed: {w0['avg_wind_speed']:.3f} m/s")
    print(f"  Total drag force: {w0['total_drag_force']:.6f} N")
    print(f"  Conductor temp: {w0['conductor_temp_K']:.3f} K")
    print(f"  Sway angle: {w0['sway_angle_deg']:.3f} deg")
    
    print("\nSpan 1 (Perpendicular to wind):")
    print(f"  Avg wind speed: {w1['avg_wind_speed']:.3f} m/s")
    print(f"  Total drag force: {w1['total_drag_force']:.6f} N")
    print(f"  Conductor temp: {w1['conductor_temp_K']:.3f} K")
    print(f"  Sway angle: {w1['sway_angle_deg']:.3f} deg")
    
    # Assertions
    # Span 0 is parallel, so perpendicular wind is ~0, hence drag force should be close to 0
    if w0['total_drag_force'] > 1e-4:
        print("Error: Span 0 is parallel to wind and should have near-zero drag force.")
        return False
        
    if w0['sway_angle_deg'] > 1e-3:
        print("Error: Span 0 is parallel to wind and should have near-zero sway angle.")
        return False
        
    # Span 1 is perpendicular, so it should have significant drag and sway
    if w1['total_drag_force'] < 0.1:
        print("Error: Span 1 is perpendicular and should have positive drag force.")
        return False
        
    if w1['sway_angle_deg'] < 0.1:
        print("Error: Span 1 is perpendicular and should have positive sway angle.")
        return False
        
    # Conductor temperature of Span 0 should be higher than Span 1 due to natural vs forced convection
    if w0['conductor_temp_K'] <= w1['conductor_temp_K']:
        print("Error: Conductor temperature of Span 0 (natural convection) should be higher than Span 1 (forced convection).")
        return False
        
    print("\n✓ SUCCESS: All physical wire loading and rating checks passed perfectly!")
    return True

if __name__ == "__main__":
    success = run_verification()
    sys.exit(0 if success else 1)
