#!/usr/bin/env python3
"""
wake_models.py - Wake loss calculation using analytical wake models

Implements Bastankhah Gaussian wake model with linear superposition for
calculating wake losses in wind farms.

Features:
- Bastankhah wake deficit model
- Wake superposition (root-sum-square and linear)
- Multi-turbine wake interaction
- Efficient NumPy implementations

References:
- Bastankhah, M., & Porté-Agel, F. (2016). "A new analytical model for wind farm power prediction"
- Dilip, D., et al. (2020). "Analytical solutions for the cumulative wake of wind farms"

Example:
    from wake_models import BastankhahWakeModel
    
    # Initialize wake model
    wake_model = BastankhahWakeModel(
        turbine_diameter=100.0,
        turbulence_intensity=0.10
    )
    
    # Calculate wake deficit at downwind turbine
    deficit = wake_model.calculate_wake_deficit(
        x_distance=500.0,
        y_distance=50.0,
        freestream_speed=10.0
    )
"""

import numpy as np
from typing import List, Tuple, Dict, Any
import math


class BastankhahWakeModel:
    """
    Bastankhah Gaussian wake deficit model.
    
    Calculates wind speed deficit in the wake of a wind turbine using
    the analytical Bastankhah (2016) model.
    
    Attributes:
        turbine_diameter (float): Wind turbine rotor diameter (m)
        turbulence_intensity (float): Atmospheric turbulence intensity (0-1)
        ct (float): Thrust coefficient (default: 0.8)
    """
    
    def __init__(
        self,
        turbine_diameter: float = 100.0,
        turbulence_intensity: float = 0.1,
        ct: float = 0.8
    ):
        """
        Initialize Bastankhah wake model.
        
        Parameters:
            turbine_diameter (float): Rotor diameter in meters
            turbulence_intensity (float): Atmospheric TI (0-1)
            ct (float): Thrust coefficient (default: 0.8)
        """
        self.turbine_diameter = turbine_diameter
        self.turbulence_intensity = turbulence_intensity
        self.ct = ct
        
        # Wake model parameters (from Bastankhah & Porté-Agel 2016)
        self.k_e = 0.05  # Wake expansion coefficient (atmospheric stability dependent)
        self._update_wake_params()
    
    def _update_wake_params(self) -> None:
        """Update wake parameters based on turbulence intensity."""
        # Wake expansion rate depends on TI
        # k = 0.03865 * TI + 0.00386 (more accurate fit)
        self.k_e = 0.03865 * self.turbulence_intensity + 0.00386
        
        # Ensure k_e is reasonable
        self.k_e = max(0.02, min(0.15, self.k_e))
    
    def calculate_wake_deficit(
        self,
        x_distance: float,
        y_distance: float,
        z_distance: float = 0.0,
        freestream_speed: float = 10.0
    ) -> float:
        """
        Calculate normalized wake velocity deficit at a point.
        
        Parameters:
            x_distance (float): Downwind distance from turbine (m)
            y_distance (float): Lateral distance from turbine centerline (m)
            z_distance (float): Vertical distance from turbine hub (m, optional)
            freestream_speed (float): Freestream wind speed (m/s)
        
        Returns:
            float: Normalized velocity deficit (0-1), where 0 = no deficit, 1 = zero velocity
        
        Notes:
            - Only considers x,y distances (2D model)
            - z_distance is tracked but not used in deficit calculation
            - Returns 0 if upwind (x_distance < 0)
        """
        if x_distance <= 0:
            return 0.0
        
        # Wake diameter expansion with downwind distance
        # D = D0 * (1 + 2*k_e*x/D0)
        wake_diameter = self.turbine_diameter * (1.0 + 2.0 * self.k_e * x_distance / self.turbine_diameter)
        
        # Lateral distance normalized by wake radius
        wake_radius = wake_diameter / 2.0
        
        if wake_radius <= 0:
            return 0.0
        
        # Lateral normalized distance (sigma in Gaussian profile)
        sigma = wake_radius / (2.0 * np.sqrt(2.0))  # 2D Gaussian width
        r_normalized = y_distance / sigma
        
        # Gaussian profile: Δu/u_ref = C * exp(-r^2 / (2*sigma^2))
        # Coefficient C = sqrt(1 - Ct) * D0 / (8 * k_e * x)
        if x_distance > 0:
            c_deficit = np.sqrt(1.0 - self.ct) * self.turbine_diameter / (8.0 * self.k_e * x_distance)
        else:
            return 0.0
        
        # Gaussian deficit profile
        deficit = c_deficit * np.exp(-(r_normalized**2) / 2.0)
        
        # Clamp to [0, 1]
        deficit = max(0.0, min(1.0, deficit))
        
        return float(deficit)
    
    def get_affected_speed(
        self,
        x_distance: float,
        y_distance: float,
        z_distance: float = 0.0,
        freestream_speed: float = 10.0
    ) -> float:
        """
        Calculate wind speed at a point in the wake.
        
        Parameters:
            x_distance (float): Downwind distance (m)
            y_distance (float): Lateral distance (m)
            z_distance (float): Vertical distance (m)
            freestream_speed (float): Undisturbed wind speed (m/s)
        
        Returns:
            float: Wind speed at the point (m/s)
        """
        deficit = self.calculate_wake_deficit(
            x_distance, y_distance, z_distance, freestream_speed
        )
        return freestream_speed * (1.0 - deficit)


class WakeLossCalculator:
    """
    Multi-turbine wake loss calculator using superposition.
    
    Combines wakes from multiple upwind turbines using root-sum-square
    (RSS) superposition to calculate effective wind speed at downwind turbine.
    
    Attributes:
        wake_model (BastankhahWakeModel): Wake model instance
        superposition_method (str): 'rss' or 'linear' superposition
    """
    
    def __init__(
        self,
        turbine_diameter: float = 100.0,
        turbulence_intensity: float = 0.1,
        superposition_method: str = 'rss'
    ):
        """
        Initialize wake loss calculator.
        
        Parameters:
            turbine_diameter (float): Rotor diameter (m)
            turbulence_intensity (float): Atmospheric TI (0-1)
            superposition_method (str): 'rss' (default) or 'linear'
        """
        self.wake_model = BastankhahWakeModel(turbine_diameter, turbulence_intensity)
        self.superposition_method = superposition_method
        self.turbine_diameter = turbine_diameter
    
    def calculate_effective_wind_speed(
        self,
        target_x: float,
        target_y: float,
        target_z: float,
        upwind_turbines: List[Dict[str, float]],
        freestream_speed: float = 10.0
    ) -> float:
        """
        Calculate effective wind speed at target turbine considering all upwind wakes.
        
        Parameters:
            target_x, target_y, target_z (float): Target turbine location (m)
            upwind_turbines (list): List of upwind turbine dicts with keys:
                                  {'x', 'y', 'z', 'speed'} where 'speed' is freestream speed
            freestream_speed (float): Reference freestream speed (m/s)
        
        Returns:
            float: Effective wind speed at target location (m/s)
        
        Notes:
            - Uses root-sum-square (RSS) superposition by default
            - Upwind turbines are those with x < target_x (wind direction assumed +x)
        """
        if not upwind_turbines:
            return freestream_speed
        
        # Calculate deficit from each upwind turbine
        deficits = []
        
        for upwind in upwind_turbines:
            x_dist = target_x - upwind['x']
            y_dist = target_y - upwind['y']
            z_dist = target_z - upwind.get('z', 0.0)
            
            # Only consider actual upwind turbines
            if x_dist > 0:
                deficit = self.wake_model.calculate_wake_deficit(
                    x_dist, y_dist, z_dist, freestream_speed
                )
                if deficit > 0:
                    deficits.append(deficit)
        
        if not deficits:
            return freestream_speed
        
        # Superposition of deficits
        if self.superposition_method == 'rss':
            # Root-sum-square superposition
            combined_deficit = np.sqrt(np.sum(np.array(deficits)**2))
        else:  # linear
            # Simple linear superposition
            combined_deficit = np.sum(deficits)
        
        # Clamp to valid range
        combined_deficit = max(0.0, min(1.0, combined_deficit))
        
        effective_speed = freestream_speed * (1.0 - combined_deficit)
        
        return float(effective_speed)
    
    def calculate_farm_wake_losses(
        self,
        layout: List[Dict[str, float]],
        wind_speed: float = 10.0,
        wind_direction: float = 270.0
    ) -> Dict[int, float]:
        """
        Calculate wake losses for all turbines in a layout.
        
        Parameters:
            layout (list): List of turbine dicts with keys: {'id', 'x', 'y', 'z'}
            wind_speed (float): Freestream wind speed (m/s)
            wind_direction (float): Wind direction (degrees, 0=N, 90=E, 180=S, 270=W)
        
        Returns:
            dict: {turbine_id: effective_wind_speed} mapping
        
        Notes:
            - Assumes wind aligned with domain axes (need rotation for arbitrary directions)
            - For simplicity, uses +X as wind direction
        """
        # Rotate layout to wind-aligned coordinates if needed
        # For now, assume wind is in +X direction
        
        effective_speeds = {}
        
        for target_turbine in layout:
            target_id = target_turbine['id']
            target_x = target_turbine['x']
            target_y = target_turbine['y']
            target_z = target_turbine.get('z', 0.0)
            
            # Find all upwind turbines
            upwind = []
            for other in layout:
                if other['id'] != target_id and other['x'] < target_x:
                    upwind.append(other)
            
            # Calculate effective speed
            eff_speed = self.calculate_effective_wind_speed(
                target_x, target_y, target_z,
                upwind,
                wind_speed
            )
            
            effective_speeds[target_id] = eff_speed
        
        return effective_speeds


class WakeDeflectionModel:
    """
    Placeholder for wake deflection model (for future yaw control).
    
    When implemented, will calculate wake centerline deflection under yawed operation.
    """
    
    def __init__(self, turbine_diameter: float = 100.0):
        """Initialize wake deflection model."""
        self.turbine_diameter = turbine_diameter
    
    def calculate_deflection(
        self,
        yaw_angle: float,
        x_distance: float
    ) -> float:
        """
        Calculate lateral wake deflection due to yaw angle.
        
        Parameters:
            yaw_angle (float): Yaw angle (degrees)
            x_distance (float): Downwind distance (m)
        
        Returns:
            float: Lateral deflection distance (m)
        
        Notes:
            - Not yet implemented
            - Placeholder for future enhancement
        """
        # TODO: Implement Bastankhah wake deflection model
        return 0.0


def calculate_power_output(
    wind_speed: float,
    rotor_diameter: float = 100.0,
    ct: float = 0.8,
    air_density: float = 1.225
) -> float:
    """
    Simple power calculation from wind speed.
    
    Parameters:
        wind_speed (float): Wind speed (m/s)
        rotor_diameter (float): Rotor diameter (m)
        ct (float): Thrust coefficient
        air_density (float): Air density (kg/m³)
    
    Returns:
        float: Power output (W)
    
    Notes:
        - This is a simplified calculation using thrust coefficient
        - Real power curves from turbine manufacturers should be used
        - Power = 0.5 * ρ * A * Ct * V^3
    """
    if wind_speed < 0:
        return 0.0
    
    rotor_area = np.pi * (rotor_diameter / 2.0)**2
    power_w = 0.5 * air_density * rotor_area * ct * wind_speed**3
    
    return float(power_w)
