#!/usr/bin/env python3
import os
import sys
import subprocess
import csv
from pathlib import Path

def run_verification():
    test_dir = Path(__file__).resolve().parent
    inputs_file = test_dir / "inputs.i"
    
    print(f"Running verify_eb.py in {test_dir}")
    
    # Locate the executable
    exe_path = None
    for path in ["../../wind_solver", "../wind_solver", "./wind_solver", "../../build/wind_solver", "../../../build/wind_solver"]:
        full_path = test_dir / path
        if full_path.exists():
            exe_path = full_path
            break
            
    if not exe_path:
        exe_path = "wind_solver"
        
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
    output_csv = test_dir / "eb_extract.csv"
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
    
    # Verify that u, v, w are zero inside the EB box (x in [60, 140], y in [60, 140])
    tolerance = 1e-10
    all_zero_inside_box = True
    cells_checked_inside_box = 0
    
    for r in rows:
        x_val = r['x']
        y_val = r['y']
        u_val = r['u']
        v_val = r['v']
        w_val = r['w']
        
        # Check if coordinates are inside the box [60, 140] (strictly inside cell centers)
        if 60.0 <= x_val <= 140.0 and 60.0 <= y_val <= 140.0:
            cells_checked_inside_box += 1
            if abs(u_val) > tolerance or abs(v_val) > tolerance or abs(w_val) > tolerance:
                print(f"Error: Velocity is not zero inside the EB solid box at (x={x_val}, y={y_val}). Got u={u_val:.3e}, v={v_val:.3e}, w={w_val:.3e}")
                all_zero_inside_box = False
                
    if cells_checked_inside_box == 0:
        print("Error: No cell centers were found inside the specified EB box coordinate range!")
        return False
        
    if not all_zero_inside_box:
        return False
        
    print(f"\n✓ SUCCESS: Verified {cells_checked_inside_box} cells inside the EB solid box. All velocity components are zero!")
    return True

if __name__ == "__main__":
    success = run_verification()
    sys.exit(0 if success else 1)
