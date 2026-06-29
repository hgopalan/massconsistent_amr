#!/usr/bin/env python3
"""
richards_ros.py - Richards (1990) Rate of Spread calculation implementation

Implements the Richards semi-empirical fire spread model with explicit vector components.

Reference:
  Richards, G. D. (1990). "An elliptical growth model of forest fire fronts and its 
  applications to fire management." International Journal of Wildland Fire, 1(2):91-101.

Author: massconsistent_amr team
Date: 2026-06-28
"""

import numpy as np
from typing import Dict


def compute_richards_ros(fuel_load: np.ndarray, fuel_moisture: np.ndarray,
                        wind_speed: np.ndarray, slope: np.ndarray,
                        ros_0: float = 0.1, wind_factor: float = 2.0,
                        slope_factor: float = 1.5,
                        moisture_response: str = "exponential") -> Dict:
    """
    Compute Rate of Spread using Richards (1990) model.
    
    Richards model provides explicit ROS components useful for:
    - Multi-dimensional fire front propagation
    - Energy-balance coupling with wind solver
    - Sensitivity analysis
    
    Parameters:
        fuel_load (np.ndarray): Fuel load (kg/m²), shape (ny, nx)
        fuel_moisture (np.ndarray): Fuel moisture (%), shape (ny, nx)
        wind_speed (np.ndarray): Wind speed at flame height (m/s), shape (ny, nx)
        slope (np.ndarray): Terrain slope (degrees), shape (ny, nx)
        ros_0 (float): Base ROS coefficient (m/min). Default: 0.1
        wind_factor (float): Wind sensitivity factor. Default: 2.0
        slope_factor (float): Slope sensitivity factor. Default: 1.5
        moisture_response (str): Moisture damping model. Default: "exponential"
                               Options: "linear", "exponential", "rothermel"
    
    Returns:
        dict: Fire spread properties with:
            - 'ros': Total ROS (m/min), shape (ny, nx)
            - 'ros_components': Dict with u, v, slope_factor, wind_factor components
            - 'energy_release': Energy release rate (kJ/m²)
            - 'consumption_rate': Fuel consumption rate (kg/m²/min)
    """
    
    # Moisture damping
    if moisture_response == "linear":
        # Linear damping: ROS decreases linearly with moisture
        # Typically reaches zero at ~30% moisture
        moisture_damp = np.maximum(1.0 - fuel_moisture / 30.0, 0.0)
    elif moisture_response == "exponential":
        # Exponential damping: ROS = ROS_0 * exp(-moisture / 10)
        moisture_damp = np.exp(-fuel_moisture / 10.0)
    elif moisture_response == "rothermel":
        # Rothermel-style: (1 - 2.59*m + 5.11*m^2 - 3.86*m^3)
        m = fuel_moisture / 25.0  # Normalize to moisture extinction ~25%
        moisture_damp = 1.0 - 2.59*m + 5.11*m**2 - 3.86*m**3
        moisture_damp = np.maximum(moisture_damp, 0.0)
    else:
        raise ValueError(f"Unknown moisture_response: {moisture_response}")
    
    # Fuel load dependence (higher load = faster spread initially)
    fuel_factor = np.sqrt(np.maximum(fuel_load, 0.01))
    
    # Slope effects
    slope_rad = np.radians(slope)
    slope_cos = np.cos(slope_rad)
    slope_sin = np.sin(slope_rad)
    
    # Slope enhancement factor
    slope_enh = 1.0 + slope_factor * slope_sin
    slope_enh = np.maximum(slope_enh, 1.0)
    
    # Wind effects (m/s)
    wind_enh = 1.0 + wind_factor * wind_speed
    
    # Base ROS (m/min)
    ros_base = ros_0 * fuel_factor * moisture_damp
    
    # Slope-enhanced ROS
    ros_slope = ros_base * slope_enh
    
    # Wind-enhanced ROS
    ros_wind = ros_slope * wind_enh
    
    # Vector components (x and y directions)
    # Assume fire spreads upslope (max) and with wind
    # Downslope/upwind spread is suppressed
    u_component = ros_wind * slope_cos  # x-component
    v_component = ros_wind * slope_sin  # y-component (upslope)
    
    # Total ROS magnitude
    ros_total = np.sqrt(u_component**2 + v_component**2)
    
    # Energy release (kJ/m²)
    # Typical values: 10-50 kJ/m² depending on fuel
    energy_content = 20000.0  # kJ/kg (typical for wildland fuels)
    energy_release = fuel_load * energy_content
    
    # Consumption rate (kg/m²/min)
    # Based on ROS and flame length
    flame_length = 0.5 + 0.05 * ros_total  # Estimate based on ROS
    consumption_rate = (fuel_load * ros_total) / (5.0 + flame_length)
    consumption_rate = np.maximum(consumption_rate, 0.0)
    
    return {
        'ros': ros_total,
        'ros_components': {
            'u_component': u_component,
            'v_component': v_component,
            'slope_factor': slope_enh,
            'wind_factor': wind_enh,
            'base_ros': ros_base,
        },
        'energy_release': energy_release,
        'consumption_rate': consumption_rate,
    }


def compute_elliptical_ros(ros_head: np.ndarray, ros_flank: np.ndarray,
                          ros_rear: np.ndarray,
                          wind_direction: np.ndarray) -> Dict:
    """
    Compute fire spread on elliptical front geometry (Richards model).
    
    The fire front is typically elliptical with:
    - Head fire (maximum ROS, typically 2-4x base ROS)
    - Flank fire (perpendicular to wind, 0.3-0.5x head)
    - Back fire (against wind, 0.1-0.3x head)
    
    Parameters:
        ros_head (np.ndarray): Head fire ROS (m/min), shape (ny, nx)
        ros_flank (np.ndarray): Flank fire ROS (m/min), shape (ny, nx)
        ros_rear (np.ndarray): Rear fire ROS (m/min), shape (ny, nx)
        wind_direction (np.ndarray): Wind direction (degrees from N), shape (ny, nx)
    
    Returns:
        dict: Elliptical fire front geometry and ROS distribution
    """
    
    # Compute ellipse parameters
    # Head-to-flank ratio
    head_flank_ratio = ros_head / (ros_flank + 0.01)
    
    # Semi-major axis (head direction)
    major_axis = ros_head / 60.0  # Convert m/min to standard units
    
    # Semi-minor axis (perpendicular to wind)
    minor_axis = ros_flank / 60.0
    
    # Length-to-width ratio
    lw_ratio = major_axis / (minor_axis + 0.001)
    
    # Eccentricity
    eccentricity = np.sqrt(1.0 - (minor_axis / major_axis) ** 2)
    eccentricity = np.minimum(eccentricity, 0.99)  # Bound for numerical stability
    
    return {
        'head_ros': ros_head,
        'flank_ros': ros_flank,
        'rear_ros': ros_rear,
        'major_axis': major_axis,
        'minor_axis': minor_axis,
        'lw_ratio': lw_ratio,
        'eccentricity': eccentricity,
        'wind_direction': wind_direction,
    }


def estimate_flame_height(intensity: np.ndarray, fuel_type: str = "conifer") -> np.ndarray:
    """
    Estimate flame height from fireline intensity.
    
    Parameters:
        intensity (np.ndarray): Fireline intensity (kW/m), shape (ny, nx)
        fuel_type (str): Type of fuel ("grass", "shrub", "conifer", "timber")
    
    Returns:
        np.ndarray: Estimated flame height (m), shape (ny, nx)
    """
    
    if fuel_type == "grass":
        # Flame length = 0.6 * I^0.4 (Alexander & Cruz 2012)
        flame_height = 0.6 * np.maximum(intensity, 0.0) ** 0.4
    elif fuel_type == "shrub":
        # Intermediate between grass and conifer
        flame_height = 0.45 * np.maximum(intensity, 0.0) ** 0.45
    elif fuel_type == "timber":
        # Flame length = 0.25 * I^0.46 (Crown fire initiation)
        flame_height = 0.25 * np.maximum(intensity, 0.0) ** 0.46
    else:  # Default conifer
        # Flame length = 0.45 * I^0.46 (Scott & Reinhardt 2001)
        flame_height = 0.45 * np.maximum(intensity, 0.0) ** 0.46
    
    return flame_height


def compute_reaction_intensity(fuel_load: np.ndarray, fuel_moisture: np.ndarray,
                               heat_content: float = 20000.0) -> np.ndarray:
    """
    Compute reaction intensity from fuel properties.
    
    Parameters:
        fuel_load (np.ndarray): Fuel load (kg/m²), shape (ny, nx)
        fuel_moisture (np.ndarray): Fuel moisture (%), shape (ny, nx)
        heat_content (float): Heat content of fuel (kJ/kg). Default: 20000.0
    
    Returns:
        np.ndarray: Reaction intensity (kW/m²), shape (ny, nx)
    """
    
    # Combustion efficiency reduces with moisture
    combustion_efficiency = np.maximum(1.0 - fuel_moisture / 100.0, 0.0)
    
    # Heat released per unit area
    heat_released = fuel_load * heat_content * combustion_efficiency
    
    # Reaction intensity (assuming 60-second burn duration)
    reaction_intensity = heat_released / 60.0
    
    return reaction_intensity


def compute_ros_sensitivity(base_ros: np.ndarray, parameter: str,
                           delta: float = 0.1) -> Dict:
    """
    Compute sensitivity of ROS to parameter variations.
    
    Parameters:
        base_ros (np.ndarray): Base ROS field (m/min), shape (ny, nx)
        parameter (str): Parameter to vary ("moisture", "wind", "fuel_load", "slope")
        delta (float): Perturbation fraction (0.1 = ±10%)
    
    Returns:
        dict: Sensitivity analysis with "low", "base", "high" ROS fields
    """
    
    # Typical sensitivity coefficients (dimensionless)
    sensitivity = {
        'moisture': -1.5,   # ROS decreases with moisture
        'wind': 2.0,        # ROS increases with wind
        'fuel_load': 0.8,   # ROS increases with fuel
        'slope': 1.2,       # ROS increases with slope
    }
    
    if parameter not in sensitivity:
        raise ValueError(f"Unknown parameter: {parameter}")
    
    s = sensitivity[parameter]
    
    # Compute low/high cases
    ros_low = base_ros * (1.0 - s * delta)
    ros_high = base_ros * (1.0 + s * delta)
    
    return {
        'low': ros_low,
        'base': base_ros,
        'high': ros_high,
        'sensitivity': s,
        'parameter': parameter,
    }

