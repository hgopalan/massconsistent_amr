#!/usr/bin/env python3
"""
test_realistic_farm_case.py - Regression test for a realistic farm case.

Verifies:
1. Setup of a realistic agricultural drone trajectory and nozzle flow rate regulation.
2. Running of the puff and LPD dispersion simulations.
3. Accurate mass conservation within a small numerical tolerance.
4. Physical response: presence of both target crop deposition and off-target drift.
"""

import sys
import os
import numpy as np

# Find repo root and add src/python to path
TEST_DIR = os.path.dirname(os.path.abspath(__file__))
curr = os.path.dirname(TEST_DIR)
repo_root = None
for parent in [curr] + list(os.path.abspath(curr).split(os.sep)):
    # We look for src/python
    if os.path.isdir(os.path.join(repo_root or TEST_DIR, 'src', 'python')):
        break
    repo_root = os.path.dirname(repo_root or TEST_DIR)

if repo_root is None:
    repo_root = os.path.dirname(os.path.dirname(TEST_DIR))

sys.path.insert(0, os.path.join(repo_root, 'src', 'python'))

from agricultural_drone import (
    DroneTrajectory, MassEmissionRegulator, DronePuffDispersion, DroneLpdDispersion
)


def run_realistic_farm_regression():
    print("=" * 70)
    print("RUNNING REALISTIC FARM CASE REGRESSION TEST")
    print("=" * 70)
    
    # 1. Define farm spraying trajectory (Y = 100m swath center, flying at 3m AGL)
    times = [0.0, 5.0, 10.0]
    x_pts = [20.0, 80.0, 140.0]
    y_pts = [100.0, 100.0, 100.0]
    z_pts = [3.0, 3.0, 3.0]
    speeds = [12.0, 12.0, 12.0]
    headings = [0.0, 0.0, 0.0]
    flow_rates = [2.5, 2.5, 2.5]  # L/min
    active_flags = [True, True, True]
    
    trajectory = DroneTrajectory(
        times=times, x_pts=x_pts, y_pts=y_pts, z_pts=z_pts,
        speeds=speeds, headings=headings, flow_rates=flow_rates, active_flags=active_flags
    )
    
    # 2. Nozzle Flow-rate Scaling & Droplet Size Profile
    droplet_bins = {
        'fine': {'diameter': 70e-6, 'fraction': 0.15},
        'medium': {'diameter': 160e-6, 'fraction': 0.55},
        'coarse': {'diameter': 320e-6, 'fraction': 0.30}
    }
    
    regulator = MassEmissionRegulator(
        formulation_density=1000.0,  # g/L
        active_fraction=0.1,         # 10% active chemical
        base_speed=12.0,
        speed_dependent=False,
        droplet_bins=droplet_bins
    )
    
    # 3. Simulate puff dispersion model
    print("\n--- Running DronePuffDispersion ---")
    puff_model = DronePuffDispersion(
        xmin=0.0, xmax=200.0,
        ymin=0.0, ymax=200.0,
        zmin=0.0, zmax=50.0,
        dx=5.0, dy=5.0, dz=2.0
    )
    
    puff_model.simulate(
        trajectory=trajectory,
        regulator=regulator,
        wind_solver=None,
        dt=0.5,
        u_uniform=0.0,
        v_uniform=2.0,  # 2.0 m/s crosswind pushing spray along Y
        w_uniform=0.0,
        K_h=0.5,
        K_v=0.2,
        enable_ground_reflection=True,
        enable_settling=True,
        enable_evaporation=True,
        enable_degradation=False,
        enable_canopy_interception=True,
        canopy_height=1.2,
        leaf_area_index=2.5,
        frontal_area_index=0.6
    )
    
    # Verify mass conservation for Puff Model
    conserved_puff, balance_puff = puff_model.verify_mass_conservation()
    print(f"Puff model mass conservation check: {conserved_puff}")
    print(f"  Total Emitted:  {balance_puff['total_emitted_mass']:.4f} g")
    print(f"  Accounted Mass: {balance_puff['total_accounted']:.4f} g")
    print(f"  Relative Error: {balance_puff['relative_error']:.2e}")
    
    if not conserved_puff:
        print("✗ ERROR: Mass is not conserved in DronePuffDispersion!")
        return False
        
    # 4. Simulate LPD model
    print("\n--- Running DroneLpdDispersion ---")
    lpd_model = DroneLpdDispersion(
        xmin=0.0, xmax=200.0,
        ymin=0.0, ymax=200.0,
        zmin=0.0, zmax=50.0,
        dx=5.0, dy=5.0, dz=2.0
    )
    
    lpd_model.simulate(
        trajectory=trajectory,
        regulator=regulator,
        wind_solver=None,
        dt=0.5,
        u_uniform=0.0,
        v_uniform=2.0,  # 2.0 m/s crosswind pushing spray along Y
        w_uniform=0.0,
        K_h=0.5,
        K_v=0.2,
        particles_per_step=80,
        random_seed=123,
        enable_settling=True,
        enable_evaporation=True,
        enable_degradation=False,
        enable_canopy_interception=True,
        canopy_height=1.2,
        leaf_area_index=2.5,
        frontal_area_index=0.6
    )
    
    # Verify mass conservation for LPD Model
    conserved_lpd, balance_lpd = lpd_model.verify_mass_conservation()
    print(f"LPD model mass conservation check: {conserved_lpd}")
    print(f"  Total Emitted:  {balance_lpd['total_emitted_mass']:.4f} g")
    print(f"  Accounted Mass: {balance_lpd['total_accounted']:.4f} g")
    print(f"  Relative Error: {balance_lpd['relative_error']:.2e}")
    
    if not conserved_lpd:
        print("✗ ERROR: Mass is not conserved in DroneLpdDispersion!")
        return False

    # 5. Physical checks: evaluate deposition vs drift
    # Swath width is 20m around flight path Y = 100m
    swath_min, swath_max = 90.0, 110.0
    
    def evaluate_drift_and_deposition(model):
        on_target = 0.0
        off_target = 0.0
        for j in range(model.ny):
            y_coord = model.y_coords[j]
            dep = (model.ground_deposition[j, :].sum() +
                   model.canopy_top_deposition[j, :].sum() +
                   model.lower_foliage_deposition[j, :].sum())
            if swath_min <= y_coord <= swath_max:
                on_target += dep
            else:
                off_target += dep
                
        total_drift = off_target + model.out_of_bounds_mass
        return on_target, total_drift
        
    on_target_puff, drift_puff = evaluate_drift_and_deposition(puff_model)
    on_target_lpd, drift_lpd = evaluate_drift_and_deposition(lpd_model)
    
    print("\n--- Physical Validation Metrics ---")
    print(f"Puff Model: On-Target = {on_target_puff:.2f} g, Drift/Out-of-Bounds = {drift_puff:.2f} g")
    print(f"LPD Model:  On-Target = {on_target_lpd:.2f} g, Drift/Out-of-Bounds = {drift_lpd:.2f} g")
    
    # Assertions
    # There should be non-zero target deposition and non-zero drift
    if on_target_puff <= 0.0 or drift_puff <= 0.0:
        print("✗ ERROR: Puff model failed physical plausibility checks (no deposition or drift recorded)!")
        return False
        
    if on_target_lpd <= 0.0 or drift_lpd <= 0.0:
        print("✗ ERROR: LPD model failed physical plausibility checks (no deposition or drift recorded)!")
        return False
        
    print("\n✓ REALISTIC FARM CASE REGRESSION TEST PASSED SUCCESSFULLY!")
    print("=" * 70)
    return True


if __name__ == '__main__':
    if len(sys.argv) < 3:
        # Standard fallback for manual execution
        success = run_realistic_farm_regression()
        sys.exit(0 if success else 1)
    
    inputs_file = sys.argv[1]
    work_dir = sys.argv[2]
    
    success = run_realistic_farm_regression()
    sys.exit(0 if success else 1)
