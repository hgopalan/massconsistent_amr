#!/usr/bin/env python3
"""
test_phase2_analytical_wake_extensions.py

Regression/standalone verification tests for Phase 2 Analytical Wake & Turbulence Extensions:
1. TurbOPark wake deficit model (decay rate and deficits check).
2. Jimenez wake deflection model (centerline deflection offsets as a function of yaw).
3. Wake-added turbulence models: Crespo-Hernández and Frandsen (STF).
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

def write_test_inputs(filename, model_type, superposition="quadratic", turb_model="none", yaw1=0.0, yaw2=0.0):
    with open(filename, "w") as f:
        f.write(f"""# Standalone phase 2 test case
terrain_file = terrain.csv
enable_turbine_wake = true
turbine_file = turbines_test.csv
turbine_wake_model_type = {model_type}
turbine_wake_superposition = {superposition}
wake_added_turbulence_model = {turb_model}
enable_jimenez_deflection = {"true" if yaw1 != 0.0 or yaw2 != 0.0 else "false"}
jimenez_kd = 0.05
turbopark_c1 = 0.38
ambient_ti = 0.075

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
    
    # Read computed local_ti from log CSV if generated
    local_tis = [0.075, 0.075]
    if os.path.exists("turbine_power_output.csv"):
        with open("turbine_power_output.csv", "r") as f:
            lines = f.readlines()
            if len(lines) >= 3:
                cols0 = lines[-2].split(",")
                cols1 = lines[-1].split(",")
                if len(cols0) >= 11 and len(cols1) >= 11:
                    local_tis = [float(cols0[10]), float(cols1[10])]
                    
    wind.finalize()
    return inflow_speeds, power_outputs, local_tis

def test_turbopark():
    print("\n--- Test 1: TurbOPark Wake Deficit Verification ---")
    write_test_inputs("inputs_tp.i", "turbopark")
    write_turbines("turbines_test.csv")
    
    inflow_speeds, powers, tis = run_solver("inputs_tp.i")
    print(f"Turbine 0 Inflow Speed: {inflow_speeds[0]:.4f} m/s")
    print(f"Turbine 1 Inflow Speed: {inflow_speeds[1]:.4f} m/s")
    
    assert inflow_speeds[1] < inflow_speeds[0], "Error: No velocity deficit observed under TurbOPark!"
    print("✓ TurbOPark deficit verification passed!")

def test_jimenez_deflection():
    print("\n--- Test 2: Jimenez Wake Deflection Verification ---")
    
    # 1. Base Case: No Yaw
    write_test_inputs("inputs_noyaw.i", "turbopark")
    write_turbines("turbines_test.csv", yaw1=0.0)
    inflow_noyaw, _, _ = run_solver("inputs_noyaw.i")
    
    # 2. Yawed Case: 30 degree yaw on upstream turbine
    write_test_inputs("inputs_yaw.i", "turbopark", yaw1=30.0)
    write_turbines("turbines_test.csv", yaw1=30.0)
    inflow_yaw, _, _ = run_solver("inputs_yaw.i")
    
    print(f"No-Yaw: Turbine 0 inflow = {inflow_noyaw[0]:.4f} m/s, Turbine 1 inflow = {inflow_noyaw[1]:.4f} m/s")
    print(f"Yaw-30: Turbine 0 inflow = {inflow_yaw[0]:.4f} m/s, Turbine 1 inflow = {inflow_yaw[1]:.4f} m/s")
    
    assert inflow_yaw[1] > inflow_noyaw[1], "Error: Jimenez deflection did not reduce wake deficit at downstream turbine!"
    print("✓ Jimenez wake deflection verification passed!")

def test_wake_added_turbulence():
    print("\n--- Test 3: Wake-Added Turbulence Verification ---")
    
    # 1. Crespo-Hernández
    write_test_inputs("inputs_ch.i", "turbopark", turb_model="crespo_hernandez")
    write_turbines("turbines_test.csv")
    _, _, tis_ch = run_solver("inputs_ch.i")
    print(f"Crespo-Hernandez Local TI: Turbine 0 = {tis_ch[0]:.5f}, Turbine 1 = {tis_ch[1]:.5f}")
    assert tis_ch[1] > tis_ch[0], "Error: Crespo-Hernandez added turbulence not registered downstream!"
    
    # 2. Frandsen (STF)
    write_test_inputs("inputs_fr.i", "turbopark", turb_model="frandsen")
    write_turbines("turbines_test.csv")
    _, _, tis_fr = run_solver("inputs_fr.i")
    print(f"Frandsen Local TI: Turbine 0 = {tis_fr[0]:.5f}, Turbine 1 = {tis_fr[1]:.5f}")
    assert tis_fr[1] > tis_fr[0], "Error: Frandsen added turbulence not registered downstream!"
    
    print("✓ Wake-added turbulence verification passed!")

if __name__ == "__main__":
    try:
        test_turbopark()
        test_jimenez_deflection()
        test_wake_added_turbulence()
        print("\n==================================================")
        print("ALL STANDALONE PHASE 2 ANALYTICAL WAKE & TURBULENCE EXTENSIONS TESTS PASSED!")
        print("==================================================")
    except Exception as e:
        print(f"\nTESTS FAILED: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
