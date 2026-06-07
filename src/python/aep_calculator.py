#!/usr/bin/env python3
"""
aep_calculator.py - Python-based Annual Energy Production (AEP) Calculator

This module automates batch execution of the massconsistent_amr C++ solver
across a joint wind speed and direction distribution (wind rose) to compute AEP,
perform layout analysis, and profile performance.
"""

import os
import sys
import time
import numpy as np
from typing import Dict, List, Tuple, Union, Any

# Ensure we can import wind_solver from the same folder
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from wind_solver import WindSolver


class AEPCalculator:
    """
    Annual Energy Production (AEP) Calculator for wind farms.
    Automates batch runs across wind directions and wind speeds.
    """
    
    def __init__(self, inputs_file: str):
        """
        Initialize the AEP Calculator.
        
        Parameters:
            inputs_file (str): Path to wind solver inputs file (e.g., "inputs.i")
        """
        self.inputs_file = inputs_file
        self.solver = None
        self.results = {}
        self.profile_data = {}
        
    def run_wind_rose(
        self,
        wind_speeds: Union[List[float], np.ndarray],
        wind_directions: Union[List[float], np.ndarray],
        probabilities: Union[List[List[float]], np.ndarray],
        turbines: List[Dict[str, Any]] = None,
        yaw_offsets: List[float] = None,
        stability_scenarios: List[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """
        Execute batch simulations across the joint wind rose distribution.
        
        Parameters:
            wind_speeds: List/array of bin-center wind speeds (m/s)
            wind_directions: List/array of wind directions (degrees, 0-360)
            probabilities: 2D array of joint probabilities [num_directions, num_speeds]
                           summing to 1.0 or representing hours/frequencies.
            turbines: Optional list of turbine dictionaries. Each dict should have:
                      {'x': x, 'y': y, 'hub_height': h, 'rotor_diameter': d,
                       'default_ct': ct, 'power_curve_file': pc, 'yaw': y, 'orientation': o}
                      If provided, these overwrite turbines in the inputs file.
            yaw_offsets: Optional list of yaw offsets (degrees) per turbine for layout optimization.
            stability_scenarios: Optional list of scenarios with different stability factors (e.g. L_obukhov, alpha_h)
            
        Returns:
            dict: Detailed AEP calculation results, turbine-specific outputs, and profiles.
        """
        start_total_time = time.time()
        
        # Verify probabilities
        probabilities = np.array(probabilities)
        wind_speeds = np.array(wind_speeds)
        wind_directions = np.array(wind_directions)
        
        assert probabilities.shape == (len(wind_directions), len(wind_speeds)), \
            f"Probabilities shape {probabilities.shape} must match (num_directions, num_speeds) which is {(len(wind_directions), len(wind_speeds))}"
            
        # Standardize probabilities to annual fractional hours (8760 hours/year total)
        prob_sum = np.sum(probabilities)
        if prob_sum > 0:
            normalized_probs = probabilities / prob_sum
        else:
            normalized_probs = probabilities
            
        annual_hours = normalized_probs * 8760.0
        
        print("\n" + "=" * 80)
        print(f"AEP Calculation: Starting batch run on wind rose ({len(wind_directions)} dirs x {len(wind_speeds)} speeds)")
        print("=" * 80)
        
        # Track energy productions
        total_aep_kwh = 0.0
        sector_aep_kwh = np.zeros_like(wind_directions, dtype=float)
        speed_aep_kwh = np.zeros_like(wind_speeds, dtype=float)
        turbine_aep_kwh = None
        
        run_times = []
        memory_usage_mb = []
        
        # Initialize solver
        self.solver = WindSolver(self.inputs_file)
        
        # Overwrite turbines if custom turbines list provided
        if turbines is not None:
            self.solver.clear_turbines()
            for t_idx, t in enumerate(turbines):
                yaw = t.get('yaw', 0.0)
                if yaw_offsets is not None and t_idx < len(yaw_offsets):
                    yaw += yaw_offsets[t_idx]
                self.solver.add_turbine(
                    x=t['x'],
                    y=t['y'],
                    hub_height=t['hub_height'],
                    rotor_diameter=t['rotor_diameter'],
                    default_ct=t.get('default_ct', 0.8),
                    power_curve_file=t.get('power_curve_file', ""),
                    yaw=yaw,
                    orientation=t.get('orientation', 0.0)
                )
                
        # Resolve initial geometries to verify and profile size
        grid_points = self.solver.nx * self.solver.ny * self.solver.nz
        # Estimate solver memory based on AMReX MultiFab allocations
        # (approx 10 double-precision 3D/2D arrays of grid size)
        estimated_mem_mb = (grid_points * 10 * 8) / (1024 * 1024)
        
        iteration_count = 0
        
        for d_idx, wd in enumerate(wind_directions):
            for s_idx, ws in enumerate(wind_speeds):
                freq_hours = annual_hours[d_idx, s_idx]
                if freq_hours <= 1e-6:
                    continue  # Skip zero probability conditions to speed up calculations
                
                iteration_count += 1
                iter_start_time = time.time()
                
                # Convert wind speed and direction to U_ref, V_ref
                # wd is meteorological direction: 0 is North (blowing to South: V = -ws), 90 is East (blowing to West: U = -ws)
                # In mathematical coordinates where 0 deg is North, clockwise:
                # U = ws * sin(wd * pi/180)
                # V = ws * cos(wd * pi/180)
                U_ref = ws * np.sin(np.radians(wd))
                V_ref = ws * np.cos(np.radians(wd))
                
                # Update reference wind and solve
                self.solver.update_reference_wind(U_ref, V_ref)
                
                # Handle optional stability scenarios
                if stability_scenarios is not None and d_idx < len(stability_scenarios):
                    scen = stability_scenarios[d_idx]
                    self.solver.update_parameters(
                        alpha_h=scen.get('alpha_h', 1.0),
                        alpha_v=scen.get('alpha_v', 1.0),
                        tol_rel=scen.get('tol_rel', 1.e-8),
                        max_iter=scen.get('max_iter', 200)
                    )
                
                # Solve mass-consistent field
                self.solver.solve()
                
                # Extract power outputs of turbines
                power_outputs = np.array(self.solver.get_turbine_power_outputs()) # kW
                
                # Initialize turbine AEP tracker if first valid run
                if turbine_aep_kwh is None:
                    turbine_aep_kwh = np.zeros(len(power_outputs), dtype=float)
                    
                # Calculate energy (kWh) = power (kW) * duration (hours)
                energy_step = power_outputs * freq_hours
                step_total_energy = np.sum(energy_step)
                
                # Update trackers
                total_aep_kwh += step_total_energy
                sector_aep_kwh[d_idx] += step_total_energy
                speed_aep_kwh[s_idx] += step_total_energy
                turbine_aep_kwh += energy_step
                
                iter_duration = time.time() - iter_start_time
                run_times.append(iter_duration)
                
                # Memory profiling (attempt psutil if available, otherwise use estimation)
                try:
                    import psutil
                    process = psutil.Process(os.getpid())
                    mem_mb = process.memory_info().rss / (1024 * 1024)
                except ImportError:
                    mem_mb = estimated_mem_mb
                memory_usage_mb.append(mem_mb)
                
                print(f"  Run {iteration_count:03d} | WD={wd:05.1f}° WS={ws:04.1f} m/s | Freq={freq_hours:05.1f}h | "
                      f"Power={np.sum(power_outputs):.1f} kW | Energy={step_total_energy:.1f} kWh | Time={iter_duration:.3f}s")
                      
        # Clean up solver
        self.solver.finalize()
        
        total_time = time.time() - start_total_time
        avg_run_time = np.mean(run_times) if run_times else 0.0
        max_run_time = np.max(run_times) if run_times else 0.0
        peak_mem_mb = np.max(memory_usage_mb) if memory_usage_mb else 0.0
        
        print("\n" + "=" * 80)
        print("AEP Calculation Completed Successfully")
        print(f"  Total Simulated Scenarios: {iteration_count}")
        print(f"  Total AEP Computed:        {total_aep_kwh / 1e6:.4f} GWh")
        print(f"  Total Elapsed Time:        {total_time:.2f} seconds")
        print(f"  Average Run Time:          {avg_run_time:.3f} seconds/scenario")
        print(f"  Peak Memory Usage:         {peak_mem_mb:.2f} MB")
        print("=" * 80 + "\n")
        
        self.results = {
            "total_aep_kwh": total_aep_kwh,
            "sector_aep_kwh": sector_aep_kwh.tolist(),
            "speed_aep_kwh": speed_aep_kwh.tolist(),
            "turbine_aep_kwh": turbine_aep_kwh.tolist() if turbine_aep_kwh is not None else [],
            "wind_directions": wind_directions.tolist(),
            "wind_speeds": wind_speeds.tolist(),
        }
        
        self.profile_data = {
            "grid_points": grid_points,
            "total_time_s": total_time,
            "avg_run_time_s": avg_run_time,
            "max_run_time_s": max_run_time,
            "peak_memory_mb": peak_mem_mb,
            "gpu_accelerated": "AMREX_USE_GPU" in os.environ or False,
        }
        
        return {
            "results": self.results,
            "profile": self.profile_data
        }
        
    def optimize_yaw_angles(
        self,
        wind_speeds: List[float],
        wind_directions: List[float],
        probabilities: List[List[float]],
        turbines: List[Dict[str, Any]],
        angle_range: List[float] = [-15, -10, -5, 0, 5, 10, 15]
    ) -> Tuple[List[float], float]:
        """
        Perform a simple sweep-based yaw optimization for the layout to maximize AEP.
        
        Returns:
            Tuple[List[float], float]: Optimized yaw offsets per turbine, and the resulting maximum AEP.
        """
        print("\n" + "~" * 80)
        print("Starting Yaw Optimization Sweep for AEP Maximization")
        print("~" * 80)
        
        num_turbines = len(turbines)
        best_yaw_offsets = [0.0] * num_turbines
        
        # Baseline AEP
        baseline_res = self.run_wind_rose(wind_speeds, wind_directions, probabilities, turbines, yaw_offsets=best_yaw_offsets)
        baseline_aep = baseline_res["results"]["total_aep_kwh"]
        best_aep = baseline_aep
        print(f"Baseline AEP: {best_aep / 1e3:.2f} MWh")
        
        # Sequential optimization sweep (for simplicity and speed in multi-turbine layout)
        for t_idx in range(num_turbines):
            best_t_offset = 0.0
            for offset in angle_range:
                if offset == 0.0:
                    continue
                test_offsets = list(best_yaw_offsets)
                test_offsets[t_idx] = offset
                
                test_res = self.run_wind_rose(wind_speeds, wind_directions, probabilities, turbines, yaw_offsets=test_offsets)
                test_aep = test_res["results"]["total_aep_kwh"]
                
                if test_aep > best_aep:
                    best_aep = test_aep
                    best_t_offset = offset
                    
            best_yaw_offsets[t_idx] = best_t_offset
            print(f"Turbine {t_idx} optimized yaw offset: {best_t_offset:+.1f}° (New AEP: {best_aep / 1e3:.2f} MWh)")
            
        print(f"\n✓ Optimization Complete!")
        print(f"  Optimized Yaw Offsets: {best_yaw_offsets}")
        print(f"  Optimized AEP:         {best_aep / 1e3:.2f} MWh (Gain: {((best_aep - baseline_aep)/baseline_aep)*100:.2f}%)")
        
        return best_yaw_offsets, best_aep
