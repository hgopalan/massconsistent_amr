#!/usr/bin/env python3
"""
weather_window_optimizer.py - Operational Weather Window Optimizer for Agricultural Drone Spraying

Runs batch simulations of agricultural drone spray drift against the pre-computed
meteorological scenario library to identify safe operating weather windows (e.g.,
maximum safe wind speed) and optimal times of day for drone spraying operations.

Safety Criteria:
- Safe Spraying Window: Drift percentage < 15.0% and On-Target Deposition > 45.0%.
- Marginal Spraying Window: Drift percentage < 25.0% and On-Target Deposition > 35.0%.
- Unsafe / No-Fly Window: Drift >= 25.0% or On-Target Deposition <= 35.0%.
"""

import sys
import os
import json
import numpy as np

# Add src/python to path to import agricultural_drone and scenario_library
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
sys.path.insert(0, os.path.join(REPO_ROOT, 'src', 'python'))
sys.path.insert(0, os.path.join(REPO_ROOT, 'src', 'python', 'phreeqc_coupling'))

from agricultural_drone import (
    DroneTrajectory, MassEmissionRegulator, DroneLpdDispersion
)
from scenario_library import ScenarioLibrary


def get_droplet_bins_for_nozzle(nozzle_diameter_um):
    """Maps nozzle diameter (micrometers) to realistic droplet size bins."""
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
    """Maps Pasquill-Gifford stability class to diffusivities (Kh, Kv)."""
    mapping = {
        'A': {'K_h': 5.0, 'K_v': 3.0},   # Very Unstable
        'B': {'K_h': 3.0, 'K_v': 1.5},   # Moderately Unstable
        'C': {'K_h': 1.5, 'K_v': 0.8},   # Slightly Unstable
        'D': {'K_h': 0.8, 'K_v': 0.4},   # Neutral
        'E': {'K_h': 0.4, 'K_v': 0.1},   # Slightly Stable
        'F': {'K_h': 0.1, 'K_v': 0.02}   # Moderately Stable
    }
    return mapping.get(stability_class.upper(), {'K_h': 0.8, 'K_v': 0.4})


def simulate_scenario(scenario, nozzle_diameter=250, flight_altitude=3.0,
                      target_y=100.0, swath_width=30.0):
    """
    Runs LPD dispersion simulation for a given scenario object from the scenario library.
    """
    # Create simple horizontal trajectory
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
    
    droplet_bins = get_droplet_bins_for_nozzle(nozzle_diameter)
    regulator = MassEmissionRegulator(
        formulation_density=1000.0,
        active_fraction=0.1,
        base_speed=8.0,
        speed_dependent=False,
        droplet_bins=droplet_bins
    )
    
    model = DroneLpdDispersion(
        xmin=0.0, xmax=200.0,
        ymin=0.0, ymax=200.0,
        zmin=0.0, zmax=50.0,
        dx=5.0, dy=5.0, dz=2.0
    )
    
    # Get stability diffusivities
    diffs = get_stability_diffusivities(scenario.stability_class)
    
    # Convert scenario temperature from Kelvin to Celsius for the model
    temp_c = scenario.temperature - 273.15
    
    model.simulate(
        trajectory=trajectory,
        regulator=regulator,
        wind_solver=None,
        dt=0.5,
        u_uniform=0.0,
        v_uniform=scenario.u_mag_ref,  # crosswind
        w_uniform=0.0,
        K_h=diffs['K_h'],
        K_v=diffs['K_v'],
        particles_per_step=100,
        random_seed=42,
        temperature=temp_c,
        relative_humidity=scenario.relative_humidity,
        enable_settling=True,
        enable_evaporation=True,
        enable_degradation=False,
        enable_canopy_interception=True,
        canopy_height=1.5,
        leaf_area_index=2.0,
        frontal_area_index=0.5
    )
    
    y_min_target = target_y - swath_width / 2.0
    y_max_target = target_y + swath_width / 2.0
    
    total_emitted = model.total_emitted_mass
    if total_emitted <= 0:
        return {
            'drift_percentage': 0.0, 'efficiency_percentage': 0.0
        }
        
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
            
    total_drift = off_target_dep + model.out_of_bounds_mass + model.degraded_mass
    drift_pct = (total_drift / total_emitted) * 100.0
    efficiency_pct = (on_target_dep / total_emitted) * 100.0
    
    return {
        'drift_percentage': float(drift_pct),
        'efficiency_percentage': float(efficiency_pct)
    }


def analyze_diurnal_weather_windows():
    """
    Evaluates typical diurnal weather conditions to find optimal spraying hours.
    """
    print("\n======================================================================")
    # Diurnal conditions (hour of day: (Temp_C, RH, Stability, Wind_m_s, Name))
    diurnal_profile = {
        6:  (15.0, 0.80, 'E', 1.5, '06:00 - Early Morning'),
        9:  (20.0, 0.65, 'C', 2.5, '09:00 - Mid Morning'),
        12: (28.0, 0.40, 'A', 4.5, '12:00 - Mid Day'),
        15: (30.0, 0.35, 'B', 5.0, '15:00 - Afternoon'),
        18: (22.0, 0.55, 'D', 3.0, '18:00 - Late Afternoon'),
        21: (16.0, 0.75, 'F', 1.2, '21:00 - Night')
    }
    
    # Create a mock WeatherScenario container class for evaluating diurnal conditions
    class MockScenario:
        def __init__(self, temp_k, rh, stability, wind):
            self.temperature = temp_k
            self.relative_humidity = rh
            self.stability_class = stability
            self.u_mag_ref = wind
            
    print("Evaluating Diurnal Spray Safety Windows (Nozzle: 250 um, Altitude: 3.0 m)")
    print(f"{'Time of Day':<25} {'Wind (m/s)':<12} {'Drift %':<10} {'Efficiency %':<15} {'Safety Status'}")
    print("-" * 75)
    
    diurnal_results = []
    for hour, (temp_c, rh, stability, wind, name) in diurnal_profile.items():
        scenario = MockScenario(temp_c + 273.15, rh, stability, wind)
        metrics = simulate_scenario(scenario, nozzle_diameter=250, flight_altitude=3.0)
        
        drift = metrics['drift_percentage']
        efficiency = metrics['efficiency_percentage']
        
        # Classify safety status
        if drift < 15.0 and efficiency > 45.0:
            status = "SAFE (Optimal)"
        elif drift < 25.0 and efficiency > 35.0:
            status = "MARGINAL"
        else:
            status = "UNSAFE (No-Fly)"
            
        diurnal_results.append({
            'hour': hour,
            'name': name,
            'wind_speed_m_s': wind,
            'temperature_c': temp_c,
            'relative_humidity': rh,
            'stability_class': stability,
            'drift_percentage': drift,
            'efficiency_percentage': efficiency,
            'status': status
        })
        
        print(f"{name:<25} {wind:<12.1f} {drift:<10.1f} {efficiency:<15.1f} {status}")
        
    return diurnal_results


def run_batch_scenario_optimization():
    """
    Loads pre-computed scenario library and performs systematic batch simulation.
    Finds the maximum safe wind speed and optimal operating parameters.
    """
    print("\n======================================================================")
    print("        OPERATIONAL WEATHER WINDOW OPTIMIZER (BATCH RUNS)")
    print("======================================================================")
    
    # 1. Instantiate and build ScenarioLibrary
    print("\nGenerating Pre-Computed Weather Scenario Library...")
    lib = ScenarioLibrary(n_scenarios=40)
    lib.generate_scenarios(random_seed=42)
    print(f"Loaded {len(lib.scenarios)} representative weather scenarios.")
    
    # 2. Evaluate each scenario for fine vs. coarse nozzles
    nozzles = [120, 250, 450]  # Fine, Medium, Coarse
    nozzle_names = {120: 'Fine Nozzle (120 um)', 250: 'Medium Nozzle (250 um)', 450: 'Coarse Nozzle (450 um)'}
    
    optimization_results = {}
    
    for nozzle in nozzles:
        print(f"\nEvaluating weather envelopes for: {nozzle_names[nozzle]}...")
        safe_runs = []
        unsafe_runs = []
        
        for scenario in lib.scenarios:
            metrics = simulate_scenario(scenario, nozzle_diameter=nozzle, flight_altitude=3.0)
            drift = metrics['drift_percentage']
            efficiency = metrics['efficiency_percentage']
            
            # Record run
            run_data = {
                'weather_id': scenario.weather_id,
                'wind_speed': float(scenario.u_mag_ref),
                'stability': scenario.stability_class,
                'temperature_k': float(scenario.temperature),
                'drift_percentage': drift,
                'efficiency_percentage': efficiency
            }
            
            if drift < 15.0 and efficiency > 45.0:
                safe_runs.append(run_data)
            else:
                unsafe_runs.append(run_data)
                
        # Find maximum safe wind speed for this nozzle type
        if safe_runs:
            max_safe_wind = max(r['wind_speed'] for r in safe_runs)
            avg_safe_drift = np.mean([r['drift_percentage'] for r in safe_runs])
            avg_safe_efficiency = np.mean([r['efficiency_percentage'] for r in safe_runs])
        else:
            max_safe_wind = 0.0
            avg_safe_drift = 0.0
            avg_safe_efficiency = 0.0
            
        optimization_results[str(nozzle)] = {
            'nozzle_name': nozzle_names[nozzle],
            'max_safe_wind_speed_m_s': float(max_safe_wind),
            'avg_safe_drift_percentage': float(avg_safe_drift),
            'avg_safe_efficiency_percentage': float(avg_safe_efficiency),
            'total_safe_scenarios': len(safe_runs),
            'total_unsafe_scenarios': len(unsafe_runs)
        }
        
    print("\n" + "=" * 70)
    print("OPERATIONAL WEATHER ENVELOPE SUMMARY")
    print("=" * 70)
    print(f"{'Nozzle Type':<25} {'Max Safe Wind (m/s)':<22} {'Avg Safe Drift %':<18} {'Safe/Total Scenarios'}")
    print("-" * 75)
    for nozzle in nozzles:
        res = optimization_results[str(nozzle)]
        total = res['total_safe_scenarios'] + res['total_unsafe_scenarios']
        print(f"{res['nozzle_name']:<25} {res['max_safe_wind_speed_m_s']:<22.2f} {res['avg_safe_drift_percentage']:<18.1f} {res['total_safe_scenarios']}/{total}")
        
    return optimization_results


def main():
    # Run both Diurnal Optimization and Batch Scenario Envelope Analysis
    diurnal_data = analyze_diurnal_weather_windows()
    batch_data = run_batch_scenario_optimization()
    
    # Save combined results
    combined_results = {
        'diurnal_optimization': diurnal_data,
        'weather_envelope_optimization': batch_data
    }
    
    out_file = os.path.join(REPO_ROOT, 'tools', 'weather_window_optimization_report.json')
    with open(out_file, 'w') as f:
        json.dump(combined_results, f, indent=4)
        
    print(f"\n✓ Successfully exported optimization report to {out_file}\n")


if __name__ == '__main__':
    main()
