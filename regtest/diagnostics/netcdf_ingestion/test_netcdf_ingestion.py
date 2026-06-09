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
from pathlib import Path

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

    # 3. ERA5 format file
    with nc.Dataset("era5_synthetic.nc", "w") as ds:
        ds.createDimension("longitude", 5)
        ds.createDimension("latitude", 5)
        ds.createDimension("level", 4)
        ds.createDimension("time", 1)
        
        lon_v = ds.createVariable("longitude", "f4", ("longitude",))
        lat_v = ds.createVariable("latitude", "f4", ("latitude",))
        lev_v = ds.createVariable("level", "f4", ("level",))
        t_v = ds.createVariable("time", "f4", ("time",))
        
        lon_v[:] = np.array([-105.02, -105.01, -105.00, -104.99, -104.98], dtype=np.float32)
        lat_v[:] = np.array([39.98, 39.99, 40.00, 40.01, 40.02], dtype=np.float32)
        lev_v[:] = np.array([1000.0, 850.0, 700.0, 500.0], dtype=np.float32)
        t_v[:] = np.array([0.0], dtype=np.float32)
        
        u_v = ds.createVariable("u", "f4", ("time", "level", "latitude", "longitude"))
        v_v = ds.createVariable("v", "f4", ("time", "level", "latitude", "longitude"))
        z_v = ds.createVariable("z", "f4", ("time", "level", "latitude", "longitude"))
        temp_v = ds.createVariable("t", "f4", ("time", "level", "latitude", "longitude"))
        q_v = ds.createVariable("q", "f4", ("time", "level", "latitude", "longitude"))
        
        # 2D surface variables
        sr_v = ds.createVariable("sr", "f4", ("latitude", "longitude"))
        ustar_v = ds.createVariable("ustar", "f4", ("latitude", "longitude"))
        blh_v = ds.createVariable("blh", "f4", ("latitude", "longitude"))
        oro_v = ds.createVariable("orography", "f4", ("latitude", "longitude"))
        
        u_v[:] = 15.0
        v_v[:] = 5.0
        
        # Geopotential height values to simulate: [10.0, 30.0, 60.0, 100.0] meters above sea level
        # To get geopotential z, we multiply by standard gravity 9.80665
        z_heights = np.array([10.0, 30.0, 60.0, 100.0], dtype=np.float32) * 9.80665
        for t in range(1):
            for k in range(4):
                z_v[t, k, :, :] = z_heights[k]
                
        # Simulate temperature profile [288.15, 283.15, 278.15, 268.15] Kelvin
        t_levels = np.array([288.15, 283.15, 278.15, 268.15], dtype=np.float32)
        for t in range(1):
            for k in range(4):
                temp_v[t, k, :, :] = t_levels[k]
                
        # Simulate humidity
        q_v[:] = 0.005
        
        # Simulate surface variables
        sr_v[:] = 0.1
        ustar_v[:] = 0.35
        blh_v[:] = 800.0
        oro_v[:] = 150.0 * 9.80665  # surface geopotential in m^2/s^2, which corresponds to 150m terrain elevation when divided by standard gravity

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
    curr = Path(test_dir)
    repo_dir = None
    for parent in [curr] + list(curr.parents):
        if (parent / "tools").is_dir():
            repo_dir = parent
            break
    if repo_dir is None:
        repo_dir = os.path.dirname(os.path.dirname(os.path.dirname(test_dir)))
    else:
        repo_dir = str(repo_dir)
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
    
    # =========================================================================
    # Test 3: ERA5 Format Ingestion, Profile Printing, and Solver Run
    # =========================================================================
    print("\n--- TEST 3: ERA5 Format Ingestion, Profile Printing, and Solver Run ---")
    era5_script = os.path.join(repo_dir, "tools", "era5_to_windfield.py")
    
    # Run the ERA5 converter
    run_cmd([
        sys.executable, era5_script,
        "--input", "era5_synthetic.nc",
        "--output", "formatted_era5.nc"
    ])
    
    # Verify formatted_era5.nc was created
    assert os.path.exists("formatted_era5.nc"), "formatted_era5.nc was not created!"
    
    # Run the NetCDF converter on the formatted ERA5 data
    run_cmd([
        sys.executable, parser_script,
        "--nc-files", "formatted_era5.nc",
        "--inputs", "inputs_single.i",
        "--output", "windfield_era5.csv"
    ])
    
    # Verify windfield_era5.csv was created
    assert os.path.exists("windfield_era5.csv"), "windfield_era5.csv was not created!"
    
    # Verify values inside windfield_era5.csv (should be U=15.0, V=5.0, W=0.0)
    with open("windfield_era5.csv", "r") as f:
        lines = f.readlines()
    data_lines = [l for l in lines if not l.startswith('#') and l.strip()]
    
    sample_pt = [float(p) for p in data_lines[50].split()]
    print(f"Sample ERA5 point values (X, Y, Z, U, V, W): {sample_pt}")
    assert abs(sample_pt[3] - 15.0) < 1e-3, f"Expected U near 15.0, got {sample_pt[3]}"
    assert abs(sample_pt[4] - 5.0) < 1e-3, f"Expected V near 5.0, got {sample_pt[4]}"
    assert abs(sample_pt[5] - 0.0) < 1e-3, f"Expected W near 0.0, got {sample_pt[5]}"
    
    # Generate inputs_era5.i
    with open("inputs_single.i", "r") as f:
        lines = f.readlines()
    with open("inputs_era5.i", "w") as f:
        for l in lines:
            if "windfield_file" in l:
                f.write("windfield_file = windfield_era5.csv\n")
            elif "plot_file" in l:
                f.write("plot_file = plt_netcdf_era5\n")
            elif "extract_file" in l:
                f.write("extract_file = wind_extract_era5.csv\n")
            else:
                f.write(l)
                
    # Run solver on inputs_era5.i
    run_cmd([solver_exe, "inputs_era5.i"])
    assert os.path.exists("wind_extract_era5.csv"), "extract_file for ERA5 not created!"
    
    print("\n✓ ALL NETCDF INGESTION TESTS PASSED SUCCESSFULLY!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
