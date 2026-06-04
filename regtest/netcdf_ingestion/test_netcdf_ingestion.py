#!/usr/bin/env python3
"""
test_netcdf_ingestion.py

Regression test for NetCDF 3D wind field parser and ingestion.
Generates synthetic single and multi-time NetCDF datasets, parses them
using the terrain-aware parser, and runs the wind_solver.
"""

import os
import sys
import subprocess
import numpy as np
import netCDF4 as nc

def generate_synthetic_datasets():
    """Generate synthetic NetCDF files for testing."""
    print("Generating synthetic NetCDF datasets...")
    
    # 1. Single time-instance file
    with nc.Dataset("generic_single.nc", "w") as ds:
        ds.createDimension("x", 5)
        ds.createDimension("y", 5)
        ds.createDimension("z", 4)
        ds.createDimension("time", 1)
        
        x_v = ds.createVariable("x", "f4", ("x",))
        y_v = ds.createVariable("y", "f4", ("y",))
        z_v = ds.createVariable("z", "f4", ("z",))
        t_v = ds.createVariable("time", "f4", ("time",))
        
        x_v[:] = np.linspace(0.0, 300.0, 5)
        y_v[:] = np.linspace(0.0, 300.0, 5)
        z_v[:] = np.array([10.0, 30.0, 60.0, 100.0])
        t_v[:] = np.array([0.0])
        
        hgt_v = ds.createVariable("HGT", "f4", ("y", "x"))
        hgt_v[:, :] = np.zeros((5, 5))
        
        u_v = ds.createVariable("U", "f4", ("time", "z", "y", "x"))
        v_v = ds.createVariable("V", "f4", ("time", "z", "y", "x"))
        w_v = ds.createVariable("W", "f4", ("time", "z", "y", "x"))
        
        u_v[:] = 12.0
        v_v[:] = 3.0
        w_v[:] = 1.0

    # 2. Two files for time interpolation (t=0.0 and t=100.0)
    with nc.Dataset("generic_t1.nc", "w") as ds:
        ds.createDimension("x", 5)
        ds.createDimension("y", 5)
        ds.createDimension("z", 4)
        ds.createDimension("time", 1)
        
        x_v = ds.createVariable("x", "f4", ("x",))
        y_v = ds.createVariable("y", "f4", ("y",))
        z_v = ds.createVariable("z", "f4", ("z",))
        t_v = ds.createVariable("time", "f4", ("time",))
        
        x_v[:] = np.linspace(0.0, 300.0, 5)
        y_v[:] = np.linspace(0.0, 300.0, 5)
        z_v[:] = np.array([10.0, 30.0, 60.0, 100.0])
        t_v[:] = np.array([0.0])
        
        hgt_v = ds.createVariable("HGT", "f4", ("y", "x"))
        hgt_v[:, :] = np.zeros((5, 5))
        
        u_v = ds.createVariable("U", "f4", ("time", "z", "y", "x"))
        v_v = ds.createVariable("V", "f4", ("time", "z", "y", "x"))
        w_v = ds.createVariable("W", "f4", ("time", "z", "y", "x"))
        
        u_v[:] = 10.0
        v_v[:] = 0.0
        w_v[:] = 0.0

    with nc.Dataset("generic_t2.nc", "w") as ds:
        ds.createDimension("x", 5)
        ds.createDimension("y", 5)
        ds.createDimension("z", 4)
        ds.createDimension("time", 1)
        
        x_v = ds.createVariable("x", "f4", ("x",))
        y_v = ds.createVariable("y", "f4", ("y",))
        z_v = ds.createVariable("z", "f4", ("z",))
        t_v = ds.createVariable("time", "f4", ("time",))
        
        x_v[:] = np.linspace(0.0, 300.0, 5)
        y_v[:] = np.linspace(0.0, 300.0, 5)
        z_v[:] = np.array([10.0, 30.0, 60.0, 100.0])
        t_v[:] = np.array([100.0])
        
        hgt_v = ds.createVariable("HGT", "f4", ("y", "x"))
        hgt_v[:, :] = np.zeros((5, 5))
        
        u_v = ds.createVariable("U", "f4", ("time", "z", "y", "x"))
        v_v = ds.createVariable("V", "f4", ("time", "z", "y", "x"))
        w_v = ds.createVariable("W", "f4", ("time", "z", "y", "x"))
        
        u_v[:] = 20.0
        v_v[:] = 4.0
        w_v[:] = 2.0

def run_cmd(cmd):
    """Execute shell command and print output."""
    print(f"Executing: {' '.join(cmd)}")
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"ERROR executing command: {' '.join(cmd)}")
        print("STDOUT:")
        print(res.stdout)
        print("STDERR:")
        print(res.stderr)
        sys.exit(res.returncode)
    return res.stdout

def main():
    # Set paths
    test_dir = os.path.dirname(os.path.abspath(__file__))
    repo_dir = os.path.dirname(os.path.dirname(test_dir))
    parser_script = os.path.join(repo_dir, "tools", "netcdf_to_windfield.py")
    
    # Copy inputs and terrain files to current working directory
    import shutil
    for f in ["inputs_single.i", "inputs_multi.i", "terrain.csv"]:
        src_f = os.path.join(test_dir, f)
        if os.path.exists(src_f):
            shutil.copy(src_f, ".")
            print(f"Copied {f} to current working directory")
            
    # Run from the test working directory (current dir)
    generate_synthetic_datasets()
    
    # Find solver executable
    solver_exe = os.environ.get("MASSCONSISTENT_EXE", os.path.join(repo_dir, "build", "wind_solver"))
    if not os.path.exists(solver_exe):
        # check parent/build
        solver_exe = os.path.join(repo_dir, "build", "wind_solver")
        
    print(f"Solver executable: {solver_exe}")
    
    # =========================================================================
    # Test 1: Single time instance parser & solver run
    # =========================================================================
    print("\n--- TEST 1: Single Time Instance ---")
    run_cmd([
        sys.executable, parser_script,
        "--nc-files", "generic_single.nc",
        "--inputs", "inputs_single.i",
        "--output", "windfield_single.csv"
    ])
    
    # Verify windfield_single.csv was created
    assert os.path.exists("windfield_single.csv"), "windfield_single.csv was not created!"
    
    # Read and verify content
    with open("windfield_single.csv", "r") as f:
        lines = f.readlines()
    data_lines = [l for l in lines if not l.startswith('#') and l.strip()]
    
    # Check values on a few points
    sample_pt = [float(p) for p in data_lines[50].split()]
    print(f"Sample parsed point values (X, Y, Z, U, V, W): {sample_pt}")
    assert len(sample_pt) == 6, "Expected 6 columns in windfield.csv"
    assert abs(sample_pt[3] - 12.0) < 1e-3, f"Expected U near 12.0, got {sample_pt[3]}"
    assert abs(sample_pt[4] - 3.0) < 1e-3, f"Expected V near 3.0, got {sample_pt[4]}"
    assert abs(sample_pt[5] - 1.0) < 1e-3, f"Expected W near 1.0, got {sample_pt[5]}"
    
    # Run solver
    run_cmd([solver_exe, "inputs_single.i"])
    assert os.path.exists("wind_extract_single.csv"), "extract_file for single step not created!"
    
    # =========================================================================
    # Test 2: Multiple time instance time interpolation & solver run
    # =========================================================================
    print("\n--- TEST 2: Multi-Time Interpolation (t=50.0) ---")
    run_cmd([
        sys.executable, parser_script,
        "--nc-files", "generic_t1.nc", "generic_t2.nc",
        "--inputs", "inputs_multi.i",
        "--output", "windfield_multi.csv",
        "--time", "50.0"
    ])
    
    # Verify windfield_multi.csv was created
    assert os.path.exists("windfield_multi.csv"), "windfield_multi.csv was not created!"
    
    # Read and verify time-interpolated values
    # t1: U=10.0, V=0.0, W=0.0; t2: U=20.0, V=4.0, W=2.0. At t=50.0, we expect exactly midpoint: U=15.0, V=2.0, W=1.0.
    with open("windfield_multi.csv", "r") as f:
        lines = f.readlines()
    data_lines = [l for l in lines if not l.startswith('#') and l.strip()]
    
    sample_pt = [float(p) for p in data_lines[50].split()]
    print(f"Sample parsed time-interpolated values (X, Y, Z, U, V, W): {sample_pt}")
    assert abs(sample_pt[3] - 15.0) < 1e-3, f"Expected U near 15.0, got {sample_pt[3]}"
    assert abs(sample_pt[4] - 2.0) < 1e-3, f"Expected V near 2.0, got {sample_pt[4]}"
    assert abs(sample_pt[5] - 1.0) < 1e-3, f"Expected W near 1.0, got {sample_pt[5]}"
    
    # Run solver
    run_cmd([solver_exe, "inputs_multi.i"])
    assert os.path.exists("wind_extract_multi.csv"), "extract_file for multi step not created!"
    
    print("\n✓ ALL NETCDF INGESTION TESTS PASSED SUCCESSFULLY!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
