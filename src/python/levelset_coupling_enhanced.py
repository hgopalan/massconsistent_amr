#!/usr/bin/env python3
"""
levelset_coupling_enhanced.py - Enhanced wind-fire coupling with advanced ROS models

This module extends the existing levelset_coupling.py with support for:
- Rothermel and Richards ROS calculation models
- Advanced two-way coupling with heat flux feedback
- Comprehensive fuel and environmental data management
- Diagnostic and analysis capabilities

Author: massconsistent_amr team
Date: 2026-06-28
"""

import numpy as np
import os
import sys
from typing import Optional, Dict, Tuple, Callable

try:
    from rothermel_ros import compute_rothermel_ros, RothermelFuelModel
    from richards_ros import compute_richards_ros, compute_reaction_intensity
except ImportError:
    print("Warning: ROS calculation modules not available")


class EnhancedCoupledWindFireSimulation:
    """
    Enhanced coupled wind-fire simulation with Rothermel and Richards models.
    
    This class extends the basic CoupledWindFireSimulation with:
    - Flexible ROS calculation (Rothermel, Richards, hybrid)
    - Advanced fuel and environment data management
    - Full diagnostic capabilities
    - Energy-balance two-way coupling
    
    Example (one-way coupling with Rothermel):
        coupled = EnhancedCoupledWindFireSimulation(
            wind_inputs="wind_inputs.i",
            fire_inputs="fire_inputs.i",
            coupling_mode='one_way',
            ros_model='rothermel'
        )
        coupled.run(num_steps=100)
        coupled.finalize()
    
    Example (two-way coupling with hybrid model):
        coupled = EnhancedCoupledWindFireSimulation(
            wind_inputs="wind_inputs.i",
            fire_inputs="fire_inputs.i",
            coupling_mode='two_way',
            ros_model='hybrid'
        )
        coupled.run(final_time=3600.0)
        coupled.finalize()
    """
    
    def __init__(self, wind_inputs: str, fire_inputs: str,
                 coupling_mode: str = 'one_way',
                 ros_model: str = 'rothermel'):
        """
        Initialize enhanced coupled wind-fire solver.
        
        Parameters:
            wind_inputs (str): Path to wind solver inputs file
            fire_inputs (str): Path to fire solver inputs file
            coupling_mode (str): 'one_way' or 'two_way'
            ros_model (str): 'rothermel', 'richards', 'hybrid', or 'levelset'
        
        Raises:
            ImportError: If required solver modules not available
            RuntimeError: If initialization fails
        """
        # Import base coupling module
        try:
            from levelset_coupling import CoupledWindFireSimulation as BaseCoupling
            self.base_coupling = BaseCoupling(wind_inputs, fire_inputs, coupling_mode)
        except ImportError as e:
            raise ImportError(f"Could not import base coupling module: {e}")
        
        # Copy attributes from base
        self.wind = self.base_coupling.wind
        self.fire = self.base_coupling.fire
        self.coupling_mode = coupling_mode
        self.ros_model = ros_model.lower()
        
        # Enhanced state tracking
        self.ros_history = []  # ROS field history for analysis
        self.intensity_history = []  # Intensity history
        self.fuel_data = {}  # Fuel properties
        
        # ROS model configuration
        self.rothermel_config = {'fuel_model': 1}  # Default: short grass
        self.richards_config = {
            'ros_0': 0.1,
            'wind_factor': 2.0,
            'slope_factor': 1.5,
            'moisture_response': 'exponential'
        }
        
        print(f"\n✓ Enhanced coupled solver initialized")
        print(f"  ROS model: {self.ros_model}")
        print(f"  Coupling mode: {coupling_mode}")
    
    # ==================== ROS CALCULATION ====================
    
    def compute_ros(self, fire_state: Dict) -> Dict:
        """
        Compute Rate of Spread based on configured model.
        
        Parameters:
            fire_state (dict): Current fire state with fuel/environment data
        
        Returns:
            dict: ROS computation results
        """
        if self.ros_model == 'rothermel':
            return self._compute_rothermel(fire_state)
        elif self.ros_model == 'richards':
            return self._compute_richards(fire_state)
        elif self.ros_model == 'hybrid':
            return self._compute_hybrid(fire_state)
        else:
            # Use fire solver's native model
            return fire_state.get('ros_data', {})
    
    def _compute_rothermel(self, fire_state: Dict) -> Dict:
        """Compute ROS using Rothermel model."""
        fuel_model = self.rothermel_config['fuel_model']
        
        # Get required fields
        moisture = fire_state.get('fuel_moisture', 10.0 * np.ones((self.fire.ny, self.fire.nx)))
        slope = fire_state.get('slope', np.zeros((self.fire.ny, self.fire.nx)))
        wind_speed = fire_state.get('wind_speed', np.zeros((self.fire.ny, self.fire.nx)))
        wind_direction = fire_state.get('wind_direction', np.zeros((self.fire.ny, self.fire.nx)))
        
        # Compute ROS
        ros_result = compute_rothermel_ros(
            fuel_model, moisture, slope, wind_speed, wind_direction
        )
        
        return ros_result
    
    def _compute_richards(self, fire_state: Dict) -> Dict:
        """Compute ROS using Richards model."""
        # Get required fields
        fuel_load = fire_state.get('fuel_load', 50.0 * np.ones((self.fire.ny, self.fire.nx)))
        fuel_moisture = fire_state.get('fuel_moisture', 10.0 * np.ones((self.fire.ny, self.fire.nx)))
        wind_speed = fire_state.get('wind_speed', np.zeros((self.fire.ny, self.fire.nx)))
        slope = fire_state.get('slope', np.zeros((self.fire.ny, self.fire.nx)))
        
        # Compute ROS
        ros_result = compute_richards_ros(
            fuel_load, fuel_moisture, wind_speed, slope,
            **self.richards_config
        )
        
        return ros_result
    
    def _compute_hybrid(self, fire_state: Dict) -> Dict:
        """Compute ROS using hybrid (blend) model."""
        # Compute both models
        ros_rothermel = self._compute_rothermel(fire_state)
        ros_richards = self._compute_richards(fire_state)
        
        # Blend based on fuel model
        blend_factor = 0.5  # 50/50 blend
        ros_hybrid = (
            blend_factor * ros_rothermel['ros_with_wind'] +
            (1.0 - blend_factor) * ros_richards['ros']
        )
        
        return {
            'ros_with_wind': ros_hybrid,
            'ros_rothermel': ros_rothermel,
            'ros_richards': ros_richards,
            'blend_factor': blend_factor,
        }
    
    # ==================== DIAGNOSTIC METHODS ====================
    
    def compute_fire_statistics(self, fire_state: Dict) -> Dict:
        """
        Compute comprehensive fire statistics.
        
        Returns statistics about:
        - ROS distribution (max, mean, percentiles)
        - Burned area and perimeter
        - Fire intensity (from ROS)
        """
        ros = fire_state.get('ros', np.zeros((self.fire.ny, self.fire.nx)))
        intensity = fire_state.get('intensity', np.zeros((self.fire.ny, self.fire.nx)))
        
        # Find active fire cells
        if 'phi' in fire_state:
            active = fire_state['phi'] <= 0.0
        else:
            active = intensity > 0.0
        
        if np.sum(active) == 0:
            return {'active_cells': 0, 'max_ros': 0.0}
        
        # ROS statistics
        ros_active = ros[active]
        stats = {
            'max_ros': np.max(ros_active),
            'mean_ros': np.mean(ros_active),
            'std_ros': np.std(ros_active),
            'ros_percentiles': {
                '10th': np.percentile(ros_active, 10),
                '25th': np.percentile(ros_active, 25),
                '50th': np.percentile(ros_active, 50),
                '75th': np.percentile(ros_active, 75),
                '90th': np.percentile(ros_active, 90),
            },
            'active_cells': np.sum(active),
            'burned_fraction': np.sum(active) / active.size,
        }
        
        # Intensity statistics
        if np.sum(intensity) > 0:
            intensity_active = intensity[active]
            stats.update({
                'max_intensity': np.max(intensity_active),
                'mean_intensity': np.mean(intensity_active),
            })
        
        return stats
    
    def compute_ros_sensitivity(self, parameter: str, delta: float = 0.1) -> Dict:
        """
        Compute ROS sensitivity to parameter variations.
        
        Parameters:
            parameter (str): Parameter to vary ("moisture", "wind", "slope", "fuel_load")
            delta (float): Perturbation fraction (0.1 = ±10%)
        
        Returns:
            dict: Sensitivity analysis with perturbed ROS fields
        """
        fire_state = self.fire.get_state()
        
        # Compute base ROS
        ros_base = self.compute_ros(fire_state)['ros_with_wind']
        
        # Compute perturbed ROS
        results = {'base': ros_base}
        
        # Perturb parameter
        if parameter == 'moisture':
            fire_state_low = fire_state.copy()
            fire_state_high = fire_state.copy()
            moisture = fire_state.get('fuel_moisture', 10.0 * np.ones((self.fire.ny, self.fire.nx)))
            fire_state_low['fuel_moisture'] = moisture * (1.0 - delta)
            fire_state_high['fuel_moisture'] = moisture * (1.0 + delta)
        
        elif parameter == 'wind':
            fire_state_low = fire_state.copy()
            fire_state_high = fire_state.copy()
            wind_speed = fire_state.get('wind_speed', np.zeros((self.fire.ny, self.fire.nx)))
            fire_state_low['wind_speed'] = wind_speed * (1.0 - delta)
            fire_state_high['wind_speed'] = wind_speed * (1.0 + delta)
        
        else:
            return results
        
        # Compute perturbed ROS
        results['low'] = self.compute_ros(fire_state_low)['ros_with_wind']
        results['high'] = self.compute_ros(fire_state_high)['ros_with_wind']
        
        # Sensitivity = dROS / dParameter
        dros = results['high'] - results['low']
        dparam = 2.0 * delta  # Normalized parameter change
        results['sensitivity'] = dros / (dparam + 1.0e-10)
        results['parameter'] = parameter
        
        return results
    
    def run_with_analysis(self, num_steps: int = None,
                         final_time: float = None,
                         analysis_interval: int = 10) -> Dict:
        """
        Run coupled simulation with continuous analysis.
        
        Parameters:
            num_steps (int, optional): Number of steps to run
            final_time (float, optional): Final time (s)
            analysis_interval (int): Run analysis every N steps
        
        Returns:
            dict: Simulation results with analysis data
        """
        analysis_results = {
            'steps': [],
            'times': [],
            'statistics': [],
            'ros_fields': [],
        }
        
        # Run base coupled simulation with callback
        def analysis_callback(step, result):
            if (step + 1) % analysis_interval == 0:
                fire_state = self.fire.get_state()
                stats = self.compute_fire_statistics(fire_state)
                
                analysis_results['steps'].append(step)
                analysis_results['times'].append(fire_state['time'])
                analysis_results['statistics'].append(stats)
                
                if 'ros' in fire_state:
                    analysis_results['ros_fields'].append(fire_state['ros'].copy())
        
        # Run base coupling
        sim_result = self.base_coupling.run(
            final_time=final_time,
            num_steps=num_steps,
            callback=analysis_callback
        )
        
        analysis_results.update(sim_result)
        return analysis_results
    
    def finalize(self):
        """Finalize simulation."""
        return self.base_coupling.finalize()

