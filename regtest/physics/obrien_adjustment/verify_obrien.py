#!/usr/bin/env python3
import os
import sys
import subprocess
import csv
from pathlib import Path

def run_verification():
    test_dir = Path(__file__).resolve().parent
    inputs_file = test_dir / "inputs.i"
    
    print(f"Running verify_obrien.py in {test_dir}")
    
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
    output_csv = test_dir / "wind_extract.csv"
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
            
    print(f"Read {len(rows)} rows from extract file.")
    
    # Verify that w is zero at the top boundary (90m AGL cell center)
    tolerance = 1e-10
    all_zero = True
    for r in rows:
        w_val = r['w']
        x_val = r['x']
        y_val = r['y']
        print(f"  At (x={x_val}, y={y_val}), w={w_val:.3e} m/s")
        if abs(w_val) > tolerance:
            print(f"Error: Vertical velocity w is not zero at the top boundary. Got {w_val:.3e}")
            all_zero = False
            
    if not all_zero:
        return False
        
    print("\n✓ SUCCESS: O'Brien Vertical Velocity Adjustment Procedure verified successfully!")
    return True

if __name__ == "__main__":
    success = run_verification()
    sys.exit(0 if success else 1)
