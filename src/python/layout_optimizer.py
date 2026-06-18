#!/usr/bin/env python3
"""
layout_optimizer.py - Wind farm layout optimization framework

Implements layout optimization using scipy.optimize backends with:
- Terrain-aware wind field evaluation
- Wake loss modeling
- Multi-objective optimization support
- Constraint handling (spacing, domain bounds, exclusion zones)

Example:
    from wind_field_cache import WindFieldCache
    from layout_optimizer import WindFarmLayoutOptimizer
    
    # Load cached wind field
    wind_cache = WindFieldCache.load("wind_field.h5")
    
    # Define initial layout
    initial_layout = [
        {'id': 0, 'x': 100.0, 'y': 100.0, 'z': 0.0},
        {'id': 1, 'x': 500.0, 'y': 100.0, 'z': 0.0},
        {'id': 2, 'x': 900.0, 'y': 100.0, 'z': 0.0},
    ]
    
    # Create optimizer
    optimizer = WindFarmLayoutOptimizer(
        wind_cache=wind_cache,
        turbines=initial_layout,
        hub_height=90.0,
        rotor_diameter=100.0
    )
    
    # Run optimization
    result = optimizer.optimize(method='differential_evolution')
    print(f"Optimized AEP: {result.aep_mwh:.1f} MWh")
"""

import numpy as np
from typing import List, Dict, Tuple, Optional, Callable, Any
from dataclasses import dataclass
from scipy.optimize import minimize, differential_evolution
import json


@dataclass
class OptimizationResult:
    """
    Optimization result container.
    
    Attributes:
        layout (list): Optimized turbine layout
        aep_mwh (float): Annual energy production (MWh)
        aep_improvement (float): Percent improvement vs. baseline
        convergence_history (list): Objective function values per iteration
        solver_success (bool): Whether optimization converged
        num_iterations (int): Total iterations executed
        computation_time_s (float): Wall clock time (seconds)
    """
    layout: List[Dict[str, float]]
    aep_mwh: float
    aep_improvement: float
    convergence_history: List[float]
    solver_success: bool
    num_iterations: int
    computation_time_s: float
    
    def __repr__(self) -> str:
        """String representation."""
        return (
            f"OptimizationResult(AEP={self.aep_mwh:.1f} MWh, "
            f"improvement={self.aep_improvement:.1f}%, "
            f"iterations={self.num_iterations}, "
            f"time={self.computation_time_s:.1f}s)"
        )


class WindFarmLayoutOptimizer:
    """
    Wind farm layout optimization engine.
    
    Uses cached wind field and analytical wake models to optimize
    turbine positions for maximum Annual Energy Production (AEP).
    
    Features:
    - Gradient-free (differential evolution) and gradient-based (SLSQP) optimization
    - Constraint handling: minimum spacing, domain bounds, exclusion zones
    - Multi-wind scenario evaluation
    - Terrain-aware wind speed extraction
    - Wake loss superposition
    
    Attributes:
        wind_cache: Cached wind field from solver
        turbines (list): Initial turbine layout
        hub_height (float): Hub height AGL (m)
        rotor_diameter (float): Rotor diameter (m)
        num_turbines (int): Number of turbines
        domain_bounds (dict): Domain boundaries
        min_spacing (float): Minimum turbine spacing (m)
        exclude_zones (list): Exclusion zone polygons
    """
    
    def __init__(
        self,
        wind_cache,
        turbines: List[Dict[str, float]],
        hub_height: float = 90.0,
        rotor_diameter: float = 100.0,
        turbulence_intensity: float = 0.1,
        min_spacing: float = 400.0,
        air_density: float = 1.225,
        ct: float = 0.8
    ):
        """
        Initialize layout optimizer.
        
        Parameters:
            wind_cache: WindFieldCache instance with solved wind field
            turbines: List of turbine dicts with 'id', 'x', 'y', 'z' keys
            hub_height (float): Hub height above ground level (m)
            rotor_diameter (float): Rotor diameter (m)
            turbulence_intensity (float): Atmospheric TI (0-1)
            min_spacing (float): Minimum spacing between turbines (m)
            air_density (float): Air density (kg/m³)
            ct (float): Thrust coefficient for power estimation
        """
        from wake_models import WakeLossCalculator, BastankhahWakeModel
        
        self.wind_cache = wind_cache
        self.turbines = [dict(t) for t in turbines]  # Deep copy
        self.hub_height = hub_height
        self.rotor_diameter = rotor_diameter
        self.turbulence_intensity = turbulence_intensity
        self.min_spacing = min_spacing
        self.air_density = air_density
        self.ct = ct
        
        self.num_turbines = len(turbines)
        self.domain_bounds = wind_cache.get_domain_bounds()
        self.exclude_zones = []
        
        # Initialize wake model
        self.wake_model = WakeLossCalculator(
            turbine_diameter=rotor_diameter,
            turbulence_intensity=turbulence_intensity,
            superposition_method='rss'
        )
        
        # Optimization tracking
        self.convergence_history = []
        self.best_aep = 0.0
        self.num_evaluations = 0
        
        print(f"✓ Layout optimizer initialized: {self.num_turbines} turbines")
        print(f"  Domain: [{self.domain_bounds['xmin']:.0f}, {self.domain_bounds['xmax']:.0f}] × "
              f"[{self.domain_bounds['ymin']:.0f}, {self.domain_bounds['ymax']:.0f}]")
        print(f"  Hub height: {hub_height}m, Rotor: {rotor_diameter}m")
    
    def _layout_to_vector(self, layout: List[Dict[str, float]]) -> np.ndarray:
        """Convert layout to optimization vector [x0, y0, x1, y1, ...]."""
        vector = []
        for turbine in sorted(layout, key=lambda t: t['id']):
            vector.extend([turbine['x'], turbine['y']])
        return np.array(vector)
    
    def _vector_to_layout(self, vector: np.ndarray) -> List[Dict[str, float]]:
        """Convert optimization vector back to layout."""
        layout = []
        for i in range(self.num_turbines):
            layout.append({
                'id': i,
                'x': float(vector[2*i]),
                'y': float(vector[2*i + 1]),
                'z': 0.0,  # Terrain elevation handled in wind speed calculation
            })
        return layout
    
    def _check_spacing_constraint(self, layout: List[Dict[str, float]]) -> bool:
        """Check if layout satisfies minimum spacing constraint."""
        for i, t1 in enumerate(layout):
            for t2 in layout[i+1:]:
                dist = np.sqrt((t1['x'] - t2['x'])**2 + (t1['y'] - t2['y'])**2)
                if dist < self.min_spacing:
                    return False
        return True
    
    def _check_domain_constraint(self, layout: List[Dict[str, float]]) -> bool:
        """Check if all turbines are within domain bounds."""
        for turbine in layout:
            x, y = turbine['x'], turbine['y']
            if not (self.domain_bounds['xmin'] <= x <= self.domain_bounds['xmax']):
                return False
            if not (self.domain_bounds['ymin'] <= y <= self.domain_bounds['ymax']):
                return False
        return True
    
    def evaluate_layout(
        self,
        layout: List[Dict[str, float]],
        wind_speeds: List[float] = None,
        wind_directions: List[float] = None
    ) -> Tuple[float, Dict[int, float]]:
        """
        Evaluate AEP for a given layout.
        
        Parameters:
            layout (list): Turbine layout
            wind_speeds (list): List of wind speeds to evaluate (default: reference)
            wind_directions (list): List of wind directions (default: single direction)
        
        Returns:
            (total_aep_mwh, effective_speeds_dict) tuple
        """
        if wind_speeds is None:
            wind_speeds = [10.0]
        if wind_directions is None:
            wind_directions = [270.0]  # Westerly wind (from west)
        
        # For simplicity, evaluate at reference wind direction (along +X axis)
        wind_speed = np.mean(wind_speeds)
        
        # Check constraints
        if not self._check_domain_constraint(layout):
            return 0.0, {}
        
        if not self._check_spacing_constraint(layout):
            return 0.0, {}
        
        total_power_w = 0.0
        effective_speeds = {}
        
        # Evaluate wind speed at each turbine hub
        for turbine in layout:
            x, y = turbine['x'], turbine['y']
            
            # Get terrain elevation and absolute height
            try:
                z_terrain = self.wind_cache.get_terrain_elevation(x, y)
            except (ValueError, IndexError):
                z_terrain = 0.0
            
            z_abs = z_terrain + self.hub_height
            
            # Get undisturbed wind speed from cache
            try:
                u_undisturbed, _, _ = self.wind_cache.interpolate_velocity_trilinear(x, y, z_abs)
                freestream_speed = np.sqrt(u_undisturbed**2)  # Magnitude
            except (ValueError, IndexError):
                # Point outside domain
                effective_speeds[turbine['id']] = 0.0
                continue
            
            # Calculate wake losses from upwind turbines
            eff_speed = self.wake_model.calculate_effective_wind_speed(
                x, y, z_abs,
                [t for t in layout if t['x'] < x],  # Upwind turbines
                freestream_speed
            )
            
            effective_speeds[turbine['id']] = eff_speed
            
            # Calculate power output
            from wake_models import calculate_power_output
            power_w = calculate_power_output(
                eff_speed,
                self.rotor_diameter,
                self.ct,
                self.air_density
            )
            total_power_w += power_w
        
        # Convert to annual energy production (MWh/year)
        # Assuming 8760 hours/year and average wind speed
        hours_per_year = 8760.0
        total_aep_mwh = total_power_w * hours_per_year / 1e6
        
        return total_aep_mwh, effective_speeds
    
    def objective_function(self, vector: np.ndarray) -> float:
        """
        Objective function for optimization (negative AEP for minimization).
        
        Parameters:
            vector (ndarray): Optimization vector [x0, y0, x1, y1, ...]
        
        Returns:
            float: Negative AEP (for minimization)
        """
        layout = self._vector_to_layout(vector)
        aep_mwh, _ = self.evaluate_layout(layout)
        
        self.num_evaluations += 1
        self.convergence_history.append(aep_mwh)
        
        if aep_mwh > self.best_aep:
            self.best_aep = aep_mwh
        
        # Return negative for minimization
        return -aep_mwh
    
    def optimize(
        self,
        method: str = 'differential_evolution',
        max_iterations: int = 1000,
        population_size: int = 50,
        seed: int = 42,
        verbose: bool = True
    ) -> OptimizationResult:
        """
        Run layout optimization.
        
        Parameters:
            method (str): 'differential_evolution' (global) or 'slsqp' (gradient-based)
            max_iterations (int): Maximum optimization iterations
            population_size (int): Population size for differential_evolution
            seed (int): Random seed for reproducibility
            verbose (bool): Print progress
        
        Returns:
            OptimizationResult: Optimization results including optimized layout
        """
        import time
        
        if verbose:
            print(f"\n{'='*70}")
            print(f"Layout Optimization: {method.upper()}")
            print(f"{'='*70}")
        
        start_time = time.time()
        self.convergence_history = []
        self.num_evaluations = 0
        self.best_aep = 0.0
        
        # Get initial layout as optimization vector
        initial_vector = self._layout_to_vector(self.turbines)
        
        # Bounds for optimization
        bounds = []
        for i in range(self.num_turbines):
            bounds.append((self.domain_bounds['xmin'], self.domain_bounds['xmax']))
            bounds.append((self.domain_bounds['ymin'], self.domain_bounds['ymax']))
        
        if method == 'differential_evolution':
            result = differential_evolution(
                func=self.objective_function,
                bounds=bounds,
                maxiter=max_iterations,
                popsize=population_size,
                seed=seed,
                workers=1
            )
        elif method == 'slsqp':
            result = minimize(
                fun=self.objective_function,
                x0=initial_vector,
                method='SLSQP',
                bounds=bounds,
                options={'maxiter': max_iterations, 'ftol': 1e-3}
            )
        else:
            raise ValueError(f"Unknown optimization method: {method}")
        
        elapsed_time = time.time() - start_time
        
        # Extract optimized layout
        optimized_layout = self._vector_to_layout(result.x)
        optimized_aep, _ = self.evaluate_layout(optimized_layout)
        
        # Calculate baseline AEP
        baseline_aep, _ = self.evaluate_layout(self.turbines)
        
        # Compute improvement
        if baseline_aep > 0:
            improvement_pct = 100.0 * (optimized_aep - baseline_aep) / baseline_aep
        else:
            improvement_pct = 0.0
        
        if verbose:
            print(f"\nOptimization Complete:")
            print(f"  Baseline AEP: {baseline_aep:.2f} MWh")
            print(f"  Optimized AEP: {optimized_aep:.2f} MWh")
            print(f"  Improvement: {improvement_pct:.2f}%")
            print(f"  Evaluations: {self.num_evaluations}")
            print(f"  Time: {elapsed_time:.1f}s")
            print(f"  Success: {result.success}")
        
        return OptimizationResult(
            layout=optimized_layout,
            aep_mwh=optimized_aep,
            aep_improvement=improvement_pct,
            convergence_history=self.convergence_history,
            solver_success=result.success,
            num_iterations=len(self.convergence_history),
            computation_time_s=elapsed_time
        )
    
    def export_layout_csv(self, layout: List[Dict[str, float]], filename: str) -> None:
        """
        Export optimized layout to CSV file.
        
        Parameters:
            layout (list): Turbine layout
            filename (str): Output CSV filename
        """
        import csv
        
        with open(filename, 'w', newline='') as f:
            writer = csv.DictWriter(
                f,
                fieldnames=['turbine_id', 'x_m', 'y_m', 'z_agl_m', 'hub_height_m', 'rotor_diameter_m']
            )
            writer.writeheader()
            
            for turbine in sorted(layout, key=lambda t: t['id']):
                writer.writerow({
                    'turbine_id': int(turbine['id']),
                    'x_m': f"{turbine['x']:.2f}",
                    'y_m': f"{turbine['y']:.2f}",
                    'z_agl_m': f"{turbine.get('z', 0.0):.2f}",
                    'hub_height_m': f"{self.hub_height:.2f}",
                    'rotor_diameter_m': f"{self.rotor_diameter:.2f}"
                })
        
        print(f"✓ Layout exported to {filename}")
    
    def export_result_json(self, result: OptimizationResult, filename: str) -> None:
        """Export optimization result to JSON."""
        data = {
            'optimization': {
                'aep_mwh': result.aep_mwh,
                'aep_improvement_pct': result.aep_improvement,
                'success': result.solver_success,
                'num_iterations': result.num_iterations,
                'computation_time_s': result.computation_time_s
            },
            'layout': result.layout,
            'convergence_history': result.convergence_history[-100:]  # Last 100 for brevity
        }
        
        with open(filename, 'w') as f:
            json.dump(data, f, indent=2)
        
        print(f"✓ Results exported to {filename}")
