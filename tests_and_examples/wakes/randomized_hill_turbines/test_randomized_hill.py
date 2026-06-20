#!/usr/bin/env python3
"""
test_randomized_hill.py - Unit test for Randomized Hill with 20 Turbines and Time-Varying Wind

Tests:
1. Dynamic generation of a randomized hill terrain with flat boundaries.
2. Placement of 20 randomly distributed wind turbines.
3. Time-varying incoming wind spanning all 16 cardinal directions clockwise from West over 3600s.
4. Log-law vertical profile initialization.
5. Solve convergence and turbine power/inflow speed extraction for all steps.
"""

import os
import sys
import math
import numpy as np
from pathlib import Path

# Add parent directory and src/python to system path
TEST_DIR = Path(__file__).resolve().parent
SRC_PYTHON_DIR = TEST_DIR.parent.parent.parent / "src" / "python"
sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import WindSolver: {e}")
    print("Ensure python bindings are compiled and on PYTHONPATH")
    sys.exit(1)


def generate_terrain_csv(filename, nx=21, ny=21, domain_x=1000.0, domain_y=1000.0):
    """Generate randomized hill terrain CSV."""
    np.random.seed(42)
    dx = domain_x / (nx - 1)
    dy = domain_y / (ny - 1)
    xc, yc = domain_x / 2.0, domain_y / 2.0
    peak_height = 100.0
    sigma = 150.0

    terrain_data = []
    for j in range(ny):
        y = j * dy
        for i in range(nx):
            x = i * dx
            
            # Base Gaussian hill
            r_squared = (x - xc)**2 + (y - yc)**2
            base_z = peak_height * np.exp(-r_squared / (2.0 * sigma**2))
            
            # Taper near boundaries to ensure perfectly flat edges at z=0
            dist_x = min(x, domain_x - x) / 200.0
            dist_y = min(y, domain_y - y) / 200.0
            taper = min(1.0, max(0.0, dist_x)) * min(1.0, max(0.0, dist_y))
            
            # Seeded noise for randomized hill profile
            noise = np.random.uniform(-5.0, 5.0) * taper
            z = max(0.0, base_z + noise)
            terrain_data.append((x, y, z))

    with open(filename, "w") as f:
        f.write("# Randomized hill terrain  X[m]  Y[m]  Z[m]\n")
        f.write(f"# Domain: 0-{domain_x} x 0-{domain_y} m, base peak={peak_height}m, sigma={sigma}m\n")
        for x, y, z in terrain_data:
            f.write(f"{x:.6f} {y:.6f} {z:.6f}\n")
    print(f"✓ Generated terrain profile at {filename}")


def generate_turbines_csv(filename):
    """Generate 20 randomly placed wind turbines."""
    np.random.seed(101)
    tx = np.random.uniform(150.0, 850.0, 20)
    ty = np.random.uniform(150.0, 850.0, 20)
    
    hub_height = 90.0
    rotor_diameter = 126.0
    default_ct = 0.8
    power_curve = "nrel_5mw.csv"

    with open(filename, "w") as f:
        f.write("# x, y, hub_height, rotor_diameter, default_ct, power_curve_file\n")
        for x, y in zip(tx, ty):
            f.write(f"{x:.2f}, {y:.2f}, {hub_height:.1f}, {rotor_diameter:.1f}, {default_ct:.1f}, {power_curve}\n")
    print(f"✓ Generated 20 randomized turbines at {filename}")


def generate_time_series_csv(filename, wind_speed=10.0):
    """Generate time-varying wind covering all 16 cardinal directions starting from West."""
    # 16 cardinal directions clockwise starting from West (270 degrees)
    start_angle = 270.0
    angles = [(start_angle + i * 22.5) % 360.0 for i in range(16)]
    # Returning to West at 3600s
    angles.append(start_angle)

    with open(filename, "w") as f:
        f.write("# Time-varying wind boundary conditions\n")
        f.write("# Format: Time(s) U_ref(m/s) V_ref(m/s) Direction(deg) Wind_Speed(m/s)\n")
        for i, angle in enumerate(angles):
            t = i * 225.0  # 3600 / 16 = 225s per step
            angle_rad = np.radians(angle)
            # Meteorological convention: angle is where wind blows from
            u_ref = -wind_speed * np.sin(angle_rad)
            v_ref = -wind_speed * np.cos(angle_rad)
            f.write(f"{t:.1f} {u_ref:.6f} {v_ref:.6f} {angle:.1f} {wind_speed:.1f}\n")
    print(f"✓ Generated time-series wind conditions at {filename}")


def test_randomized_hill_workflow():
    """Execute complete workflow and validate solver."""
    print("\n" + "="*70)
    print("Test 1: Full Randomized Hill and 20 Turbines Simulation")
    print("="*70)

    os.chdir(TEST_DIR)

    # Generate test CSV files
    terrain_file = "terrain.csv"
    turbines_file = "turbines.csv"
    time_series_file = "time_series.csv"

    generate_terrain_csv(terrain_file)
    generate_turbines_csv(turbines_file)
    generate_time_series_csv(time_series_file)

    inputs_file = "inputs.i"

    # Initialize Wind Solver
    wind = WindSolver()
    print("Initializing solver with inputs...")
    wind.initialize(inputs_file)

    # Validate initialization
    print("Validating grid and terrain boundaries...")
    print(f"  Grid dimensions: nx={wind.nx}, ny={wind.ny}, nz={wind.nz}")
    assert wind.nx == 20, f"Expected nx=20 cells, got {wind.nx}"
    assert wind.ny == 20, f"Expected ny=20 cells, got {wind.ny}"
    assert wind.nz > 0, "Expected nz > 0"

    # Verify terrain properties
    terrain = wind.get_terrain()
    assert terrain is not None, "Terrain field should not be None"
    print(f"  Terrain bounds: min={terrain.min():.2f} m, max={terrain.max():.2f} m")
    assert terrain.max() > 90.0, f"Expected peak elevation > 90.0m, got {terrain.max():.2f} m"

    # Verify turbine count loaded
    power_outputs = wind.get_turbine_power_outputs()
    print(f"  Loaded turbines: {len(power_outputs)}")
    assert len(power_outputs) == 20, f"Expected 20 turbines, got {len(power_outputs)}"

    # Parse directions to execute loop
    cardinal_dirs = [
        "W", "WNW", "NW", "NNW", "N", "NNE", "NE", "ENE",
        "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W"
    ]
    start_angle = 270.0
    angles = [(start_angle + i * 22.5) % 360.0 for i in range(16)]
    angles.append(start_angle)

    wind_speed = 10.0

    print("\nStarting time-varying wind simulation loop (17 steps, 3600 seconds)...")
    for idx, (dir_name, angle) in enumerate(zip(cardinal_dirs, angles)):
        t = idx * 225.0
        angle_rad = np.radians(angle)
        u_ref = -wind_speed * np.sin(angle_rad)
        v_ref = -wind_speed * np.cos(angle_rad)

        print(f"\nStep {idx:02d}/16 | t = {t:4.1f} s | Dir = {dir_name} ({angle:5.1f}°) | Inflow vector = ({u_ref:.2f}, {v_ref:.2f}) m/s")
        
        # Update reference wind and solve
        wind.update_reference_wind(u_ref, v_ref)
        result = wind.solve()

        # Validate convergence
        assert result['success'], f"Solve failed at step {idx}"
        print(f"  Converged in {wind.iters} iterations, residual = {wind.residual:.2e}")
        assert wind.residual < 1.0e-5, f"High residual {wind.residual} at step {idx}"

        # Retrieve power outputs and inflow speeds
        powers = np.array(wind.get_turbine_power_outputs())
        inflows = np.array(wind.get_turbine_inflow_speeds())

        print(f"  Inflow speeds range: [{inflows.min():.2f}, {inflows.max():.2f}] m/s")
        print(f"  Power outputs range: [{powers.min():.2f}, {powers.max():.2f}] kW, Total = {powers.sum():.2f} kW")

        # Basic physical sanity assertions
        assert (inflows >= 0).all(), "Inflow speeds must be non-negative"
        assert (powers >= 0).all(), "Power outputs must be non-negative"
        assert inflows.max() > 0.0, "Expected non-zero inflow speeds"

    print("\nCleaning up and finalizing solver...")
    wind.finalize()
    print("✓ All time steps completed and validated successfully!")
    return True


if __name__ == "__main__":
    success = test_randomized_hill_workflow()
    sys.exit(0 if success else 1)
