#!/usr/bin/env python3
"""
run_colorado_drone_spray.py - Colorado Complex Terrain Drone Spray Workflow

This script sets up and executes a high-fidelity workflow for agricultural drone
spraying operations over a complex terrain in Colorado:
1. Initializes and solves the mass-consistent WindSolver over a 3D terrain grid.
2. Constructs a terrain-following flight pathway (drone trajectory) 3.0m above terrain.
3. Sets up a MassEmissionRegulator with custom droplet size distribution bins.
4. Executes the DronePuffDispersion advection-dispersion simulation coupled to the wind field.
5. Performs rigorous mass conservation checks and analyzes off-target spray drift.
"""

import os
import sys
import numpy as np
from pathlib import Path

# Add src/python to path
TEST_DIR = Path(__file__).resolve().parent
REPO_ROOT = TEST_DIR.parent.parent
SRC_PYTHON_DIR = REPO_ROOT / "src" / "python"
sys.path.insert(0, str(SRC_PYTHON_DIR))

try:
    from wind_solver import WindSolver
except ImportError as e:
    print(f"ERROR: Could not import WindSolver: {e}")
    print("Ensure that pyWindSolver was compiled successfully via CMake.")
    sys.exit(1)

from agricultural_drone import (
    DroneTrajectory, MassEmissionRegulator, DronePuffDispersion
)


def run_workflow():
    print("=" * 80)
    print("      COLORADO COMPLEX TERRAIN DRONE SPRAY OPERATIONAL WORKFLOW")
    print("=" * 80)

    # 1. Change working directory to test case folder for loading relative paths
    os.chdir(TEST_DIR)
    print(f"Working directory: {TEST_DIR}\n")

    # 2. Initialize and solve Mass-Consistent Wind Solver
    inputs_file = "inputs.i"
    print(f"--- 1. Wind Solver Initialization & Solution ---")
    print(f"Loading inputs from {inputs_file}...")
    
    wind = WindSolver(inputs_file)
    print("Solving for 3D mass-consistent wind field...")
    solve_result = wind.solve()
    
    if not solve_result['success']:
        print("✗ ERROR: Wind solver failed to solve the wind field.")
        return False
        
    print("✓ Wind field solved successfully!")
    print(f"  MLMG iterations: {wind.iters}")
    print(f"  Final residual: {wind.residual:.2e}")
    
    # Analyze wind field
    vel = wind.get_velocity()
    u_mean = vel['u'].mean()
    v_mean = vel['v'].mean()
    w_mean = vel['w'].mean()
    print(f"  Wind field statistics:")
    print(f"    U-velocity mean: {u_mean:.3f} m/s (range: [{vel['u'].min():.2f}, {vel['u'].max():.2f}])")
    print(f"    V-velocity mean: {v_mean:.3f} m/s (range: [{vel['v'].min():.2f}, {vel['v'].max():.2f}])")
    print(f"    W-velocity mean: {w_mean:.3f} m/s (range: [{vel['w'].min():.2f}, {vel['w'].max():.2f}])")

    # Get terrain field
    terrain = wind.get_terrain()
    ny, nx = terrain.shape
    print(f"  Terrain resolution: {nx} x {ny} points")
    print(f"  Elevation bounds: {wind.zs_min:.1f} m to {wind.zs_max:.1f} m AGL/sea-level")

    # Helper function to get terrain elevation at any (x, y) coordinate
    def get_terrain_elevation(x, y):
        i_idx = int((x - wind.xmin) / wind.dx)
        j_idx = int((y - wind.ymin) / wind.dy)
        i_idx = max(0, min(nx - 1, i_idx))
        j_idx = max(0, min(ny - 1, j_idx))
        return float(terrain[j_idx, i_idx])

    # 3. Define terrain-following flight trajectory (drone pathway)
    # Drone flies from West (X = -150.0m) to East (X = 150.0m) along Y = 0.0m
    # Flight height is exactly 3.0 m AGL
    print(f"\n--- 2. Drone Pathway & Trajectory Construction ---")
    
    x_pts = np.array([-150.0, -75.0, 0.0, 75.0, 150.0])
    y_pts = np.array([0.0, 0.0, 0.0, 0.0, 0.0])
    
    # Calculate absolute Z coordinates based on terrain elevation
    z_pts = np.array([get_terrain_elevation(x, y) + 3.0 for x, y in zip(x_pts, y_pts)])
    
    # Total flight duration of 40 seconds
    times = np.array([0.0, 10.0, 20.0, 30.0, 40.0])
    speeds = np.full_like(times, 7.5)      # 7.5 m/s constant speed
    headings = np.full_like(times, 0.0)     # Flying along +X direction (heading 0 degrees)
    flow_rates = np.full_like(times, 2.0)   # Nozzle flow rate of 2.0 L/min
    active_flags = np.full_like(times, True, dtype=bool)

    print("Constructing DroneTrajectory object...")
    trajectory = DroneTrajectory(
        times=times, x_pts=x_pts, y_pts=y_pts, z_pts=z_pts,
        speeds=speeds, headings=headings, flow_rates=flow_rates, active_flags=active_flags
    )
    
    print(f"Flight Pathway Waypoints:")
    for t, x, y, z in zip(times, x_pts, y_pts, z_pts):
        g_elev = get_terrain_elevation(x, y)
        print(f"  t = {t:4.1f} s | Position: ({x:6.1f}, {y:6.1f}) | Terrain Elev: {g_elev:7.1f} m | Drone Altitude: {z:7.1f} m (AGL: {z - g_elev:.1f} m)")

    # 4. Configure MassEmissionRegulator (Nozzle scaling and Droplet bins)
    print(f"\n--- 3. Nozzle Emission & Droplet Size Profile ---")
    droplet_bins = {
        'fine': {'diameter': 60e-6, 'fraction': 0.15},
        'medium': {'diameter': 150e-6, 'fraction': 0.50},
        'coarse': {'diameter': 300e-6, 'fraction': 0.35}
    }
    
    regulator = MassEmissionRegulator(
        formulation_density=1000.0,  # 1000 g/L (water-like density)
        active_fraction=0.1,         # 10% active chemical ingredient
        base_speed=7.5,
        speed_dependent=False,
        droplet_bins=droplet_bins
    )
    print("Mass emission regulator successfully configured.")
    print("Droplet classification bins:")
    for name, info in droplet_bins.items():
        print(f"  - {name:8s}: Diameter = {info['diameter']*1e6:5.1f} µm, Mass Fraction = {info['fraction']*100.0:5.1f}%")

    # 5. Initialize and Run Puff Dispersion Solver coupled with WindSolver
    print(f"\n--- 4. Running Puff Dispersion Simulation ---")
    puff_model = DronePuffDispersion(
        xmin=wind.xmin, xmax=wind.xmax,
        ymin=wind.ymin, ymax=wind.ymax,
        zmin=wind.zmin, zmax=wind.zmax,
        dx=wind.dx, dy=wind.dy, dz=wind.dz
    )
    
    # Run simulation
    print("Executing dynamic advection-dispersion simulation loop...")
    puff_model.simulate(
        trajectory=trajectory,
        regulator=regulator,
        wind_solver=wind,
        dt=0.5,
        K_h=1.0,                     # Horizontal diffusivity [m^2/s]
        K_v=0.5,                     # Vertical diffusivity [m^2/s]
        sigma_y0=0.5,                # Initial puff width [m]
        sigma_z0=0.5,                # Initial puff height [m]
        enable_ground_reflection=True,
        enable_settling=True,
        enable_evaporation=True,
        enable_degradation=False,
        enable_canopy_interception=True,
        canopy_height=1.2,           # Canopy height of 1.2 m
        leaf_area_index=2.5,         # Foliage LAI
        frontal_area_index=0.6       # Frontal area index
    )
    print("✓ Dispersion simulation complete.")

    # 6. Verify Mass Conservation
    print(f"\n--- 5. Mass Conservation Verification ---")
    conserved, balance = puff_model.verify_mass_conservation(tolerance=1e-4)
    
    print(f"Mass Conservation Conserved? : {conserved}")
    print(f"  Total Pesticide Emitted    : {balance['total_emitted_mass']:.4f} g")
    print(f"  Active Airborne Mass       : {balance['airborne_mass']:.4f} g")
    print(f"  Canopy-Deposited Mass      : {balance['canopy_deposited_mass']:.4f} g")
    print(f"    - Canopy Top Deposition  : {balance['canopy_top_deposited']:.4f} g")
    print(f"    - Lower Foliage Dep      : {balance['lower_foliage_deposited']:.4f} g")
    print(f"  Ground-Deposited Mass      : {balance['ground_deposited_mass']:.4f} g")
    print(f"  Out of Bounds Mass         : {balance['out_of_bounds_mass']:.4f} g")
    print(f"  Degraded/Decayed Mass      : {balance['degraded_mass']:.4f} g")
    print(f"  Total Accounted Mass       : {balance['total_accounted']:.4f} g")
    print(f"  Relative Mass Balance Error: {balance['relative_error']:.2e}")

    # 7. Evaluate On-Target Deposition and Off-Target Drift
    print(f"\n--- 6. Deposition & Spray Drift Analysis ---")
    # Swath width is 40m centered around the flight path Y = 0.0m
    swath_min, swath_max = -20.0, 20.0
    
    on_target = 0.0
    off_target = 0.0
    for j in range(puff_model.ny):
        y_coord = puff_model.y_coords[j]
        dep = (puff_model.ground_deposition[j, :].sum() +
               puff_model.canopy_top_deposition[j, :].sum() +
               puff_model.lower_foliage_deposition[j, :].sum())
        if swath_min <= y_coord <= swath_max:
            on_target += dep
        else:
            off_target += dep
            
    total_drift = off_target + balance['out_of_bounds_mass'] + balance['degraded_mass']
    total_emitted = balance['total_emitted_mass']
    
    on_target_pct = (on_target / total_emitted) * 100.0 if total_emitted > 0 else 0.0
    drift_pct = (total_drift / total_emitted) * 100.0 if total_emitted > 0 else 0.0
    
    print(f"  On-Target Swath Range      : [{swath_min:.1f} m, {swath_max:.1f} m]")
    print(f"  On-Target Canopy & Ground  : {on_target:.3f} g ({on_target_pct:.2f}%)")
    print(f"  Off-Target Drift / Out-of-B: {total_drift:.3f} g ({drift_pct:.2f}%)")

    # 8. Sanity Check / Validation Assertions
    print(f"\n--- 7. Workflow Validation & Plausibility ---")
    
    if not conserved:
        print("✗ FAIL: Mass is not conserved within specified tolerance.")
        return False
        
    if on_target <= 0.0:
        print("✗ FAIL: No on-target pesticide deposition recorded.")
        return False
        
    if total_drift <= 0.0:
        print("✗ FAIL: No off-target spray drift recorded (unrealistic physical scenario).")
        return False
        
    print("✓ PASS: Mass conservation and physical plausibility checks passed successfully!")
    print("=" * 80)
    print("🎉 COLORADO DRONE SPRAY OPERATIONAL WORKFLOW COMPLETED SUCCESSFULLY!")
    print("=" * 80)
    
    # Finalize the wind solver to clean up memory
    wind.finalize()
    return True


if __name__ == '__main__':
    success = run_workflow()
    sys.exit(0 if success else 1)
