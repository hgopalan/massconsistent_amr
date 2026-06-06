#!/usr/bin/env python3
import os
import sys
import numpy as np

# Find the repository root dynamically
script_dir = os.path.dirname(os.path.abspath(__file__))
repo_root = os.path.dirname(script_dir)

sys.path.insert(0, os.path.join(repo_root, "build/python"))
sys.path.insert(0, os.path.join(repo_root, "src/python"))

from wind_solver import WindSolver

def main():
    print("====================================================")
    print("Testing Analytical Turbine Wake Model")
    print("====================================================")

    # Change directory to the test case folder dynamically
    flat_terrain_dir = os.path.join(repo_root, "regtest/flat_terrain")
    os.chdir(flat_terrain_dir)

    # 1. Initialize solver from the inputs_turbines.i file
    solver = WindSolver("inputs_turbines.i")

    # 2. Solve the wind field
    print("Solving the wind field...")
    result = solver.solve()

    print("Retrieving computed results...")
    power_outputs = solver.get_turbine_power_outputs()
    inflow_speeds = solver.get_turbine_inflow_speeds()

    print(f"Turbine 0 (Upstream at x=20): Inflow speed = {inflow_speeds[0]:.2f} m/s, Power = {power_outputs[0]:.2f} kW")
    print(f"Turbine 1 (Downstream at x=80): Inflow speed = {inflow_speeds[1]:.2f} m/s, Power = {power_outputs[1]:.2f} kW")

    # Since wind is coming from West (positive X direction), Turbine 1 is directly in the wake of Turbine 0!
    # Therefore, Turbine 1's inflow wind speed should be significantly less than Turbine 0's.
    assert inflow_speeds[0] > 0.1, "Error: Upstream turbine got zero or near-zero wind!"
    assert inflow_speeds[1] < inflow_speeds[0], "Error: Downstream turbine did not experience velocity deficit!"
    print("✓ Success: Downstream turbine experienced wake velocity deficit!")

    # Verify logging CSV has been generated
    log_csv = "turbine_power_output.csv"
    assert os.path.exists(log_csv), f"Error: {log_csv} was not generated!"
    print("✓ Success: Log CSV file generated!")

    # Print log CSV content
    print("\nLog CSV Contents:")
    with open(log_csv, 'r') as f:
        print(f.read())

    # Finalize
    solver.finalize()
    print("====================================================")
    print("All tests passed successfully!")
    print("====================================================")

if __name__ == "__main__":
    main()
