#!/usr/bin/env python3
"""
drone_sensitivity_analysis.py - Validation & Sensitivity Analysis Suite for Agricultural Drone Spray Drift

Tests the sensitivity of spray drift and crop deposition to:
1. Nozzle diameter (varying droplet size / bin fractions)
2. Flight altitude (above ground/canopy level)
3. Wind speed (advection velocity)
4. Atmospheric stability (affecting horizontal/vertical diffusivities Kh/Kv)

Saves results to drone_sensitivity_results.json and outputs clean summary tables.
"""

import sys
import os
import json
import numpy as np

# Add src/python to path to import agricultural_drone
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
sys.path.insert(0, os.path.join(REPO_ROOT, 'src', 'python'))

from agricultural_drone import (
    DroneTrajectory, MassEmissionRegulator, DroneLpdDispersion,
    compute_settling_velocity
)


def get_droplet_bins_for_nozzle(nozzle_diameter_um):
    """
    Maps nozzle diameter (in micrometers) to realistic droplet size bins.
    Finer nozzles produce more fine droplets (which drift more),
    while larger nozzles produce coarser droplets.
    """
    if nozzle_diameter_um < 150:
        return {
            'fine': {'diameter': 50e-6, 'fraction': 0.70},
            'medium': {'diameter': 150e-6, 'fraction': 0.25},
            'coarse': {'diameter': 350e-6, 'fraction': 0.05}
        }
    elif nozzle_diameter_um <= 250:
        return {
            'fine': {'diameter': 50e-6, 'fraction': 0.20},
            'medium': {'diameter': 150e-6, 'fraction': 0.50},
            'coarse': {'diameter': 350e-6, 'fraction': 0.30}
        }
    else:
        return {
            'fine': {'diameter': 50e-6, 'fraction': 0.02},
            'medium': {'diameter': 150e-6, 'fraction': 0.28},
            'coarse': {'diameter': 350e-6, 'fraction': 0.70}
        }


def get_stability_diffusivities(stability_class):
    """
    Maps Pasquill-Gifford atmospheric stability classes to horizontal
    and vertical eddy diffusivities (K_h, K_v) [m^2/s].
    """
    mapping = {
        'A': {'K_h': 5.0, 'K_v': 3.0},   # Very Unstable
        'B': {'K_h': 3.0, 'K_v': 1.5},   # Moderately Unstable
        'C': {'K_h': 1.5, 'K_v': 0.8},   # Slightly Unstable
        'D': {'K_h': 0.8, 'K_v': 0.4},   # Neutral
        'E': {'K_h': 0.4, 'K_v': 0.1},   # Slightly Stable
        'F': {'K_h': 0.1, 'K_v': 0.02}   # Moderately Stable
    }
    return mapping.get(stability_class.upper(), {'K_h': 0.8, 'K_v': 0.4})


def run_simulation(nozzle_diameter=250, flight_altitude=4.0, wind_speed=3.0,
                   stability_class='D', target_y=100.0, swath_width=30.0):
    """
    Runs a single puff dispersion simulation with specified parameters and
    returns spray drift and deposition metrics.
    """
    # 1. Create a simple linear drone trajectory
    # Flight from x = 20 to x = 180 along y = target_y at the specified altitude
    times = np.array([0.0, 10.0, 20.0])
    x_pts = np.array([20.0, 100.0, 180.0])
    y_pts = np.array([target_y, target_y, target_y])
    z_pts = np.array([flight_altitude, flight_altitude, flight_altitude])
    speeds = np.array([8.0, 8.0, 8.0])
    headings = np.array([0.0, 0.0, 0.0])
    flow_rates = np.array([2.0, 2.0, 2.0])  # L/min
    active_flags = np.array([True, True, True])
    
    trajectory = DroneTrajectory(
        times=times, x_pts=x_pts, y_pts=y_pts, z_pts=z_pts,
        speeds=speeds, headings=headings, flow_rates=flow_rates, active_flags=active_flags
    )
    
    # 2. Configure MassEmissionRegulator with nozzle diameter droplet bins
    droplet_bins = get_droplet_bins_for_nozzle(nozzle_diameter)
    regulator = MassEmissionRegulator(
        formulation_density=1000.0,  # g/L
        active_fraction=0.1,         # 10% active chemical
        base_speed=8.0,
        speed_dependent=False,
        droplet_bins=droplet_bins
    )
    
    # 3. Create and configure Dispersion Model
    # Domain: 0 to 200m in X, 0 to 200m in Y, 0 to 50m in Z
    model = DroneLpdDispersion(
        xmin=0.0, xmax=200.0,
        ymin=0.0, ymax=200.0,
        zmin=0.0, zmax=50.0,
        dx=5.0, dy=5.0, dz=2.0
    )
    
    # Get atmospheric diffusivities
    diffs = get_stability_diffusivities(stability_class)
    
    # 4. Simulate puff advection
    model.simulate(
        trajectory=trajectory,
        regulator=regulator,
        wind_solver=None,
        dt=0.5,
        u_uniform=0.0,              # No wind along flight path X
        v_uniform=wind_speed,       # Crosswind along Y (pushes spray sideways)
        w_uniform=0.0,
        K_h=diffs['K_h'],
        K_v=diffs['K_v'],
        particles_per_step=100,
        random_seed=42,
        enable_settling=True,
        enable_evaporation=True,
        enable_degradation=False,
        enable_canopy_interception=True,
        canopy_height=1.5,
        leaf_area_index=2.0,
        frontal_area_index=0.5
    )
    
    # 5. Evaluate on-target deposition vs. off-target drift
    # Swath width determines target boundaries along Y direction
    y_min_target = target_y - swath_width / 2.0
    y_max_target = target_y + swath_width / 2.0
    
    total_emitted = model.total_emitted_mass
    if total_emitted <= 0:
        return {
            'total_emitted': 0.0, 'on_target_deposition': 0.0,
            'off_target_drift': 0.0, 'drift_percentage': 0.0, 'efficiency': 0.0
        }
    
    # Calculate deposition across target grid cells
    on_target_dep = 0.0
    off_target_dep = 0.0
    
    for j in range(model.ny):
        y_coord = model.y_coords[j]
        dep_val = (model.ground_deposition[j, :].sum() +
                   model.canopy_top_deposition[j, :].sum() +
                   model.lower_foliage_deposition[j, :].sum())
        
        if y_min_target <= y_coord <= y_max_target:
            on_target_dep += dep_val
        else:
            off_target_dep += dep_val
            
    # Include out of bounds mass as off-target drift
    total_drift = off_target_dep + model.out_of_bounds_mass + model.degraded_mass
    
    # Normalize by total emitted mass
    drift_pct = (total_drift / total_emitted) * 100.0
    efficiency_pct = (on_target_dep / total_emitted) * 100.0
    
    return {
        'total_emitted_g': float(total_emitted),
        'on_target_deposition_g': float(on_target_dep),
        'off_target_drift_g': float(total_drift),
        'drift_percentage': float(drift_pct),
        'efficiency_percentage': float(efficiency_pct)
    }


def format_bar(pct, length=20):
    """Generates a text bar chart representation of a percentage."""
    filled = int(round(pct / 100 * length))
    return '[' + '#' * filled + '-' * (length - filled) + ']'


def run_all_sensitivity_sweeps():
    """Performs systematic sensitivity analysis sweeps and saves results."""
    print("======================================================================")
    print("        AGRICULTURAL DRONE SPRAY DRIFT SENSITIVITY SUITE")
    print("======================================================================")
    
    results = {}
    
    # Baseline Scenario Parameters
    base_nozzle = 250         # microns
    base_altitude = 4.0       # m
    base_wind = 3.0           # m/s
    base_stability = 'D'      # Neutral
    
    # ------------------------------------------------------------------
    # Sweep 1: Nozzle Diameter
    # ------------------------------------------------------------------
    print("\nRunning Sweep 1: Nozzle Diameter (Droplet Size Variation)")
    nozzle_values = [100, 150, 200, 250, 300, 400, 500]
    sweep_results = []
    for val in nozzle_values:
        metrics = run_simulation(
            nozzle_diameter=val, flight_altitude=base_altitude,
            wind_speed=base_wind, stability_class=base_stability
        )
        sweep_results.append({
            'parameter_value': val,
            **metrics
        })
    results['nozzle_diameter'] = sweep_results
    
    print(f"{'Nozzle (um)':<12} {'Drift %':<10} {'Efficiency %':<15} {'Drift Visual'}")
    print("-" * 65)
    for r in sweep_results:
        visual = format_bar(r['drift_percentage'])
        print(f"{r['parameter_value']:<12d} {r['drift_percentage']:<10.1f} {r['efficiency_percentage']:<15.1f} {visual}")
        
    # ------------------------------------------------------------------
    # Sweep 2: Flight Altitude
    # ------------------------------------------------------------------
    print("\nRunning Sweep 2: Flight Altitude")
    altitude_values = [2.0, 3.0, 4.0, 6.0, 8.0, 10.0]
    sweep_results = []
    for val in altitude_values:
        metrics = run_simulation(
            nozzle_diameter=base_nozzle, flight_altitude=val,
            wind_speed=base_wind, stability_class=base_stability
        )
        sweep_results.append({
            'parameter_value': val,
            **metrics
        })
    results['flight_altitude'] = sweep_results
    
    print(f"{'Altitude (m)':<12} {'Drift %':<10} {'Efficiency %':<15} {'Drift Visual'}")
    print("-" * 65)
    for r in sweep_results:
        visual = format_bar(r['drift_percentage'])
        print(f"{r['parameter_value']:<12.1f} {r['drift_percentage']:<10.1f} {r['efficiency_percentage']:<15.1f} {visual}")

    # ------------------------------------------------------------------
    # Sweep 3: Wind Speed
    # ------------------------------------------------------------------
    print("\nRunning Sweep 3: Wind Speed")
    wind_values = [0.5, 1.5, 3.0, 5.0, 7.5, 10.0]
    sweep_results = []
    for val in wind_values:
        metrics = run_simulation(
            nozzle_diameter=base_nozzle, flight_altitude=base_altitude,
            wind_speed=val, stability_class=base_stability
        )
        sweep_results.append({
            'parameter_value': val,
            **metrics
        })
    results['wind_speed'] = sweep_results
    
    print(f"{'Wind (m/s)':<12} {'Drift %':<10} {'Efficiency %':<15} {'Drift Visual'}")
    print("-" * 65)
    for r in sweep_results:
        visual = format_bar(r['drift_percentage'])
        print(f"{r['parameter_value']:<12.1f} {r['drift_percentage']:<10.1f} {r['efficiency_percentage']:<15.1f} {visual}")

    # ------------------------------------------------------------------
    # Sweep 4: Atmospheric Stability
    # ------------------------------------------------------------------
    print("\nRunning Sweep 4: Atmospheric Stability")
    stability_values = ['A', 'B', 'C', 'D', 'E', 'F']
    sweep_results = []
    for val in stability_values:
        metrics = run_simulation(
            nozzle_diameter=base_nozzle, flight_altitude=base_altitude,
            wind_speed=base_wind, stability_class=val
        )
        sweep_results.append({
            'parameter_value': val,
            **metrics
        })
    results['atmospheric_stability'] = sweep_results
    
    print(f"{'Stability':<12} {'Drift %':<10} {'Efficiency %':<15} {'Drift Visual'}")
    print("-" * 65)
    for r in sweep_results:
        visual = format_bar(r['drift_percentage'])
        print(f"{r['parameter_value']:<12} {r['drift_percentage']:<10.1f} {r['efficiency_percentage']:<15.1f} {visual}")

    # Save to JSON file
    out_file = os.path.join(REPO_ROOT, 'tools', 'drone_sensitivity_results.json')
    with open(out_file, 'w') as f:
        json.dump(results, f, indent=4)
        
    print(f"\n✓ Saved sensitivity analysis results to {out_file}\n")
    return results


if __name__ == '__main__':
    run_all_sensitivity_sweeps()
