#!/usr/bin/env python3
"""
test_analytical_wake_new_features.py

Regression test for analytical wake enhancements:
1. MAX superposition (maximum deficit superposition)
2. Tilt Angle & Vertical Deflection
3. Standardized Turbine JSON Importer (power/thrust curves from JSON file)
"""

import os
import sys
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

def write_test_inputs(filename, superposition="quadratic", enable_deflection="false"):
    with open(filename, "w") as f:
        f.write(f"""# New wake features test case
terrain_file = terrain.csv
enable_turbine_wake = true
turbine_file = turbines_test.csv
turbine_wake_model_type = bastankhah_gaussian
turbine_wake_superposition = {superposition}
enable_jimenez_deflection = {enable_deflection}
jimenez_kd = 0.05
ambient_ti = 0.075

U_ref = 10.0
V_ref = 0.0
z_ref = 40.0
z0 = 0.1
dx = 5.0
dy = 5.0
dz = 5.0
domain_height = 100.0
alpha_h = 1.0
alpha_v = 1.0
mlmg_verbose = 0
max_grid_size = 32
plot_file = plt_test
""")

def write_turbines(filename, tilt=0.0, use_json=True):
    # x, y, hub_height, rotor_diameter, default_ct, power_curve_file, yaw, orientation, tilt
    curve_file = "test_turbine.json" if use_json else ""
    with open(filename, "w") as f:
        f.write("# x, y, hub_height, rotor_diameter, default_ct, power_curve_file, yaw, orientation, tilt\n")
        f.write(f"20.0, 50.0, 40.0, 30.0, 0.8, {curve_file}, 0.0, 0.0, {tilt}\n")
        f.write(f"50.0, 50.0, 40.0, 30.0, 0.8, {curve_file}, 0.0, 0.0, 0.0\n")
        f.write(f"80.0, 50.0, 40.0, 30.0, 0.8, {curve_file}, 0.0, 0.0, 0.0\n")

def run_solver(inputs_file):
    wind = WindSolver()
    wind.initialize(inputs_file)
    wind.solve()
    inflow_speeds = wind.get_turbine_inflow_speeds()
    power_outputs = wind.get_turbine_power_outputs()
    wind.finalize()
    return inflow_speeds, power_outputs

def test_json_importer():
    print("\n--- Test 1: Standardized Turbine JSON Importer Verification ---")
    write_test_inputs("inputs_test.i")
    write_turbines("turbines_test.csv", tilt=0.0, use_json=True)
    
    inflow_speeds, powers = run_solver("inputs_test.i")
    print(f"Turbine 0 Inflow Speed: {inflow_speeds[0]:.4f} m/s")
    print(f"Turbine 0 Power Output: {powers[0]:.4f} kW")
    
    # Check that Turbine 0 has power close to 1000 kW because wind speed is 10 m/s
    assert abs(powers[0] - 1000.0) < 50.0, f"Error: JSON power curve interpolation failed! Expected ~1000 kW, got {powers[0]} kW"
    print("✓ Standardized Turbine JSON Importer verification passed!")

def test_max_superposition():
    print("\n--- Test 2: MAX Superposition Verification ---")
    
    # 1. Quadratic superposition (standard RSS)
    write_test_inputs("inputs_test.i", superposition="quadratic")
    write_turbines("turbines_test.csv", tilt=0.0, use_json=True)
    inflow_quad, _ = run_solver("inputs_test.i")
    
    # 2. MAX superposition
    write_test_inputs("inputs_test.i", superposition="max")
    write_turbines("turbines_test.csv", tilt=0.0, use_json=True)
    inflow_max, _ = run_solver("inputs_test.i")
    
    print(f"Quadratic - T1 inflow: {inflow_quad[1]:.4f} m/s, T2 inflow: {inflow_quad[2]:.4f} m/s")
    print(f"MAX       - T1 inflow: {inflow_max[1]:.4f} m/s, T2 inflow: {inflow_max[2]:.4f} m/s")
    
    # Since MAX superposition takes the maximum of individual deficits instead of root-sum-squares of deficits,
    # the wind speed under MAX superposition at downstream turbines should be larger (i.e. less combined deficit).
    assert inflow_max[2] > inflow_quad[2], "Error: MAX superposition combined deficit should be less than or equal to QUADRATIC superposition!"
    print("✓ MAX superposition verification passed!")

def test_tilt_vertical_deflection():
    print("\n--- Test 3: Tilt Angle & Vertical Deflection Verification ---")
    
    # 1. Base Case: No Tilt
    write_test_inputs("inputs_test.i", superposition="quadratic")
    write_turbines("turbines_test.csv", tilt=0.0, use_json=True)
    inflow_notilt, _ = run_solver("inputs_test.i")
    
    # 2. Tilted Case: 20 degree tilt on upstream turbine
    write_test_inputs("inputs_test.i", superposition="quadratic")
    write_turbines("turbines_test.csv", tilt=20.0, use_json=True)
    inflow_tilt, _ = run_solver("inputs_test.i")
    
    print(f"No-Tilt - T0 inflow: {inflow_notilt[0]:.4f} m/s, T1 inflow: {inflow_notilt[1]:.4f} m/s")
    print(f"Tilt-20 - T0 inflow: {inflow_tilt[0]:.4f}   m/s, T1 inflow: {inflow_tilt[1]:.4f}   m/s")
    
    # Because of tilt-induced vertical wake deflection, the wake is deflected away from the hub height,
    # so the downstream turbine T1 should experience less deficit (higher wind speed).
    assert inflow_tilt[1] > inflow_notilt[1], "Error: Tilt-induced vertical deflection did not reduce wake deficit at downstream turbine!"
    print("✓ Tilt angle & vertical deflection verification passed!")

if __name__ == "__main__":
    test_json_importer()
    test_max_superposition()
    test_tilt_vertical_deflection()
    print("\nAll new analytical wake features verification tests passed successfully!")
