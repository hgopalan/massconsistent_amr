#!/usr/bin/env python3
"""
test_bastankhah_deflection.py

Regression/standalone verification tests for:
1. Bastankhah & Porté-Agel Wake Deflection Model (2016)
2. Height-Varying (Veered) Wake Orientation
"""

import os
import sys
import shutil
import math
from pathlib import Path

# Add python path for bindings
SCRIPT_DIR = Path(__file__).resolve().parent
ROOT_DIR = SCRIPT_DIR.parent.parent
sys.path.insert(0, str(ROOT_DIR / "src" / "python"))
sys.path.insert(0, str(ROOT_DIR / "build" / "python"))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import WindSolver: {e}")
    sys.exit(1)

def write_test_inputs(filename, model_type, superposition="quadratic", enable_bastankhah=True, enable_jimenez=False, enable_veer=False):
    with open(filename, "w") as f:
        f.write(f"""# Standalone Bastankhah deflection / veer test case
terrain_file = terrain.csv
enable_turbine_wake = true
turbine_file = turbines_test.csv
turbine_wake_model_type = {model_type}
turbine_wake_superposition = {superposition}
wake_added_turbulence_model = none
enable_jimenez_deflection = {"true" if enable_jimenez else "false"}
enable_bastankhah_deflection = {"true" if enable_bastankhah else "false"}
turbopark_c1 = 0.38
ambient_ti = 0.075

# Veer settings if enabled
enable_ekman_veer = {"true" if enable_veer else "false"}
ekman_veer_total = 30.0
ekman_veer_height = 50.0

U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.1
dx = 10.0
dy = 10.0
dz = 10.0
domain_height = 100.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 0
max_grid_size = 32
plot_file = plt_test
""")

def write_turbines(filename, yaw1=0.0, yaw2=0.0):
    with open(filename, "w") as f:
        f.write("# x, y, hub_height, rotor_diameter, default_ct, power_curve_file, yaw, orientation\n")
        f.write(f"20.0, 50.0, 40.0, 30.0, 0.8, nrel_5mw.csv, {yaw1}, 0.0\n")
        f.write(f"80.0, 50.0, 40.0, 30.0, 0.8, nrel_5mw.csv, {yaw2}, 0.0\n")

def run_solver(inputs_file):
    wind = WindSolver()
    wind.initialize(inputs_file)
    wind.solve()
    inflow_speeds = wind.get_turbine_inflow_speeds()
    power_outputs = wind.get_turbine_power_outputs()
    wind.finalize()
    return inflow_speeds, power_outputs

def test_bastankhah_deflection():
    print("\n--- Test 1: Bastankhah Wake Deflection Verification ---")
    
    # 1. Base Case: No Yaw
    write_test_inputs("inputs_noyaw.i", "bastankhah_gaussian", enable_bastankhah=True)
    write_turbines("turbines_test.csv", yaw1=0.0)
    inflow_noyaw, _ = run_solver("inputs_noyaw.i")
    
    # 2. Yawed Case: 30 degree yaw on upstream turbine
    write_test_inputs("inputs_yaw.i", "bastankhah_gaussian", enable_bastankhah=True)
    write_turbines("turbines_test.csv", yaw1=30.0)
    inflow_yaw, _ = run_solver("inputs_yaw.i")
    
    print(f"No-Yaw: Turbine 0 inflow = {inflow_noyaw[0]:.4f} m/s, Turbine 1 inflow = {inflow_noyaw[1]:.4f} m/s")
    print(f"Yaw-30: Turbine 0 inflow = {inflow_yaw[0]:.4f} m/s, Turbine 1 inflow = {inflow_yaw[1]:.4f} m/s")
    
    # Deflection steers the wake away, reducing the wake deficit at Turbine 1
    assert inflow_yaw[1] > inflow_noyaw[1], "Error: Bastankhah deflection did not reduce wake deficit at downstream turbine!"
    print("✓ Bastankhah wake deflection verification passed!")

def test_height_varying_wake_orientation():
    print("\n--- Test 2: Height-Varying (Veered) Wake Orientation Verification ---")
    
    # Run with Ekman wind veer enabled to verify that coordinate projection succeeds and computes stable fields
    write_test_inputs("inputs_veer.i", "bastankhah_gaussian", enable_bastankhah=True, enable_veer=True)
    write_turbines("turbines_test.csv", yaw1=30.0)
    inflow_speeds, _ = run_solver("inputs_veer.i")
    
    print(f"Veered Wind: Turbine 0 inflow = {inflow_speeds[0]:.4f} m/s, Turbine 1 inflow = {inflow_speeds[1]:.4f} m/s")
    assert inflow_speeds[0] > 1.0, "Error: Inflow wind speed is too low!"
    print("✓ Height-Varying (Veered) Wake Orientation verification passed!")

if __name__ == "__main__":
    try:
        # Copy nrel_5mw.csv to current directory if not present
        if not os.path.exists("nrel_5mw.csv"):
            shutil.copy(str(SCRIPT_DIR / "nrel_5mw.csv"), "nrel_5mw.csv")
        # Copy terrain.csv to current directory if not present
        if not os.path.exists("terrain.csv"):
            shutil.copy(str(SCRIPT_DIR / "terrain.csv"), "terrain.csv")
            
        test_bastankhah_deflection()
        test_height_varying_wake_orientation()
        print("\n==================================================")
        print("ALL BASTANKHAH DEFLECTION & VEER TESTS PASSED!")
        print("==================================================")
    except Exception as e:
        print(f"\nTESTS FAILED: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
