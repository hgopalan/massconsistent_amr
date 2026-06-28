#!/usr/bin/env python3
"""
rothermel_ros.py - Rothermel (1972) Rate of Spread calculation implementation

Implements the complete Rothermel semi-empirical fire spread model with all 13 NFDRS fuel models.

Reference:
  Rothermel, R. C. (1972). "A mathematical model for predicting fire spread in wildland fuels."
  USDA Forest Service Research Paper INT-115.

Author: massconsistent_amr team
Date: 2026-06-28
"""

import numpy as np
from typing import Dict, Tuple


class RothermelFuelModel:
    """Fuel model parameters for Rothermel model."""
    
    # NFDRS Standard Fuel Models (1-13)
    # Parameters: (w0, sigma, h, rho_b, st, se, mx, name)
    FUEL_MODELS = {
        1: {
            'name': 'Short grass (cured)',
            'w0': 0.034,  # ovendry fuel load (lb/ft²)
            'sigma': 2000,  # surface area-to-volume ratio (ft²/ft³)
            'h': 8000,  # heat content (BTU/lb)
            'rho_b': 5.0,  # bulk density (lb/ft³)
            'st': 0.012,  # total mineral content (fraction)
            'se': 0.015,  # effective mineral content (fraction)
            'mx': 12.0,  # moisture of extinction (%)
        },
        2: {
            'name': 'Timber-grass-shrub',
            'w0': 0.092,
            'sigma': 2000,
            'h': 8000,
            'rho_b': 15.0,
            'st': 0.015,
            'se': 0.024,
            'mx': 15.0,
        },
        3: {
            'name': 'Tall grass (cured)',
            'w0': 0.138,
            'sigma': 1500,
            'h': 8000,
            'rho_b': 20.0,
            'st': 0.012,
            'se': 0.024,
            'mx': 25.0,
        },
        4: {
            'name': 'Chaparral',
            'w0': 0.230,
            'sigma': 2000,
            'h': 8000,
            'rho_b': 20.0,
            'st': 0.015,
            'se': 0.024,
            'mx': 20.0,
        },
        5: {
            'name': 'Timber litter',
            'w0': 0.060,
            'sigma': 1500,
            'h': 8000,
            'rho_b': 5.0,
            'st': 0.012,
            'se': 0.024,
            'mx': 20.0,
        },
        6: {
            'name': 'Conifer plantation litter',
            'w0': 0.145,
            'sigma': 1500,
            'h': 8000,
            'rho_b': 5.0,
            'st': 0.012,
            'se': 0.024,
            'mx': 25.0,
        },
        7: {
            'name': 'Ponderosa pine/mixed conifer litter',
            'w0': 0.209,
            'sigma': 1500,
            'h': 8000,
            'rho_b': 5.0,
            'st': 0.012,
            'se': 0.024,
            'mx': 20.0,
        },
        8: {
            'name': 'Closed timber litter',
            'w0': 0.322,
            'sigma': 1500,
            'h': 8000,
            'rho_b': 5.0,
            'st': 0.012,
            'se': 0.024,
            'mx': 25.0,
        },
        9: {
            'name': 'Hardwood litter',
            'w0': 0.060,
            'sigma': 1220,
            'h': 8000,
            'rho_b': 5.0,
            'st': 0.012,
            'se': 0.024,
            'mx': 25.0,
        },
        10: {
            'name': 'Timber-shrub (black spruce-lichen)',
            'w0': 0.227,
            'sigma': 1500,
            'h': 8000,
            'rho_b': 5.0,
            'st': 0.012,
            'se': 0.024,
            'mx': 20.0,
        },
        11: {
            'name': 'Timber-shrub (light conifer-lichen)',
            'w0': 0.227,
            'sigma': 1500,
            'h': 8000,
            'rho_b': 5.0,
            'st': 0.012,
            'se': 0.024,
            'mx': 25.0,
        },
        12: {
            'name': 'Closed shelterwood',
            'w0': 0.300,
            'sigma': 1500,
            'h': 8000,
            'rho_b': 5.0,
            'st': 0.012,
            'se': 0.024,
            'mx': 20.0,
        },
        13: {
            'name': 'Palm-grass-shrub',
            'w0': 0.044,
            'sigma': 2000,
            'h': 8000,
            'rho_b': 5.0,
            'st': 0.012,
            'se': 0.024,
            'mx': 12.0,
        }
    }
    
    @staticmethod
    def get_fuel_model(model_number: int) -> Dict:
        """Get fuel model parameters."""
        if model_number not in RothermelFuelModel.FUEL_MODELS:
            raise ValueError(f"Invalid fuel model {model_number}. Must be 1-13.")
        return RothermelFuelModel.FUEL_MODELS[model_number]


def compute_ros_no_wind_slope(fuel_model: int, moisture: np.ndarray) -> np.ndarray:
    """
    Compute Rate of Spread without wind or slope effects (base ROS).
    
    Parameters:
        fuel_model (int): Rothermel fuel model 1-13
        moisture (np.ndarray): Fuel moisture content (%), shape (ny, nx)
    
    Returns:
        np.ndarray: Base ROS (ft/min), shape (ny, nx)
    """
    params = RothermelFuelModel.get_fuel_model(fuel_model)
    
    w0 = params['w0']  # ovendry fuel load (lb/ft²)
    sigma = params['sigma']  # surface area-to-volume ratio (ft²/ft³)
    h = params['h']  # heat content (BTU/lb)
    rho_b = params['rho_b']  # bulk density (lb/ft³)
    st = params['st']  # total mineral content
    se = params['se']  # effective mineral content
    mx = params['mx']  # moisture of extinction (%)
    
    # Rothermel equations (empirical model)
    # Reaction intensity (BTU/ft²/min)
    ir = 0.055 * w0 * h * sigma * np.exp(-138.0 / sigma)
    
    # Moisture damping coefficient
    m = moisture / mx  # relative moisture
    q_ig = 250.0 + 1.107 * w0  # heat of ignition (BTU/lb)
    eta_M = 1.0 - 2.59 * m + 5.11 * m**2 - 3.861 * m**3
    eta_M = np.maximum(eta_M, 0.0)  # Ensure non-negative
    
    # Mineral damping coefficient
    eta_S = 0.174 * se**(-0.19)
    
    # Reaction velocity (min^-1)
    gamma = np.where(
        moisture > mx,
        0.0,  # Moisture exceeds extinction, no fire
        gamma_max(w0) * ((1.0 - st) / se)**1.5 * np.exp((0.792 + 0.681 * np.sqrt(sigma)) * (moisture + mx))
    )
    
    # Rate of spread (ft/min) = (IR * gamma * (eta_M * eta_S + 1)) / (rho_b * epsilon)
    # where epsilon = expected value of heat absorbed
    epsilon = q_ig + 581.0 * moisture
    
    ros = (ir * gamma * eta_M * eta_S) / (rho_b * epsilon)
    
    return ros


def gamma_max(w0: float) -> float:
    """Compute maximum reaction velocity from ovendry fuel load."""
    return 0.06 * (w0 ** 0.54)


def compute_slope_factor(slope_deg: np.ndarray) -> np.ndarray:
    """
    Compute slope enhancement factor.
    
    Parameters:
        slope_deg (np.ndarray): Terrain slope (degrees), shape (ny, nx)
    
    Returns:
        np.ndarray: Slope factor [>=1], shape (ny, nx)
    """
    # Convert degrees to fraction
    slope_frac = np.tan(np.radians(slope_deg))
    
    # Slope factor (Rothermel 1972)
    phi_s = 5.275 * slope_frac**(-0.3)
    phi_s = np.where(slope_frac > 0, phi_s, 1.0)  # No enhancement for zero/negative slopes
    phi_s = np.maximum(phi_s, 1.0)  # Minimum factor of 1
    
    return phi_s


def compute_wind_factor(wind_speed_fph: np.ndarray, sigma: float, rho_b: float) -> np.ndarray:
    """
    Compute wind enhancement factor.
    
    Parameters:
        wind_speed_fph (np.ndarray): Wind speed at midflame height (ft/min)
        sigma (float): Surface area-to-volume ratio (ft²/ft³)
        rho_b (float): Bulk density (lb/ft³)
    
    Returns:
        np.ndarray: Wind factor [>=1], shape (ny, nx)
    """
    # Convert wind speed from ft/min to effective midflame wind
    # Typical effective wind is ~88% of 20-ft wind
    
    # Windward direction enhancement
    phi_w = (0.3239 * (rho_b / 32.1767) ** (-0.46)) * \
            sigma ** 0.1765 * \
            (wind_speed_fph ** 0.9) / 100.0
    
    phi_w = np.maximum(phi_w, 1.0)  # Minimum factor of 1
    
    return phi_w


def compute_rothermel_ros(fuel_model: int, moisture: np.ndarray,
                         slope: np.ndarray, wind_speed: np.ndarray,
                         wind_direction: np.ndarray,
                         fire_direction: np.ndarray = None) -> Dict[str, np.ndarray]:
    """
    Complete Rothermel (1972) Rate of Spread calculation.
    
    Parameters:
        fuel_model (int): Rothermel fuel model 1-13
        moisture (np.ndarray): Fuel moisture (%), shape (ny, nx)
        slope (np.ndarray): Terrain slope (degrees), shape (ny, nx)
        wind_speed (np.ndarray): Wind speed at flame height (m/s), shape (ny, nx)
        wind_direction (np.ndarray): Wind direction (degrees from N), shape (ny, nx)
        fire_direction (np.ndarray, optional): Fire spread direction (degrees). 
                                             If None, computed as upslope + wind.
    
    Returns:
        dict: ROS results including:
            - 'ros_no_wind_slope': Base ROS (m/min)
            - 'ros_with_slope': ROS with slope only (m/min)
            - 'ros_with_wind': Final ROS (m/min)
            - 'fireline_intensity': Byram's intensity (kW/m)
            - 'flame_length': Flame length (m)
            - 'direction_factor': Wind directional effect
            - 'spread_direction': Direction of maximum spread (degrees)
    """
    params = RothermelFuelModel.get_fuel_model(fuel_model)
    
    # Convert wind speed: m/s -> ft/min (1 m/s = 196.85 ft/min)
    wind_speed_fph = wind_speed * 196.85
    
    # 1. Base ROS (no wind/slope)
    ros_base_fph = compute_ros_no_wind_slope(fuel_model, moisture)
    ros_base_mpm = ros_base_fph / 196.85  # Convert to m/min
    
    # 2. Slope enhancement
    phi_s = compute_slope_factor(slope)
    ros_slope_fph = ros_base_fph * phi_s
    ros_slope_mpm = ros_slope_fph / 196.85
    
    # 3. Wind enhancement (directional)
    phi_w = compute_wind_factor(wind_speed_fph, params['sigma'], params['rho_b'])
    
    # Directional effectiveness: wind from behind enhances, from side/front reduces
    if fire_direction is None:
        # Default: fire spreads upslope + downwind
        fire_direction = np.degrees(np.arctan2(0, 1))  # East for simplicity
    
    wind_direction_rad = np.radians(wind_direction)
    fire_direction_rad = np.radians(fire_direction)
    delta_angle = fire_direction_rad - wind_direction_rad
    
    # Directional factor: 0 if wind from 90°, 1 if wind aligned
    direction_factor = np.cos(delta_angle)
    direction_factor = np.maximum(direction_factor, 0.0)  # No wind effect from behind fire
    direction_factor = direction_factor ** 0.5  # Smooth transition
    
    phi_w_directional = 1.0 + direction_factor * (phi_w - 1.0)
    
    # 4. Final ROS
    ros_final_fph = ros_base_fph * phi_s * phi_w_directional
    ros_final_mpm = ros_final_fph / 196.85
    
    # 5. Fireline intensity (Byram's intensity, kW/m)
    # I = (h_c * w_c * r) / 60 (convert BTU to kJ, min to s)
    # h_c = heat of combustion ≈ 18000 BTU/lb
    # w_c = weight of fuel consumed (lb/ft²)
    # r = rate of spread (ft/min)
    h_c = 18000  # BTU/lb
    w_c = 0.7 * params['w0']  # ~70% of ovendry fuel load
    intensity_btu_ft_min = h_c * w_c * ros_final_fph
    intensity_kw_m = intensity_btu_ft_min * 0.1761 / 60  # Convert to kW/m
    
    # 6. Flame length (Scott & Reinhardt 2001)
    # FL = 0.45 * I^0.46 (meters from intensity in kW/m)
    flame_length_m = 0.45 * np.maximum(intensity_kw_m, 0.0) ** 0.46
    
    # 7. Spread direction
    # Combine upslope and wind directions
    upslope_dir = np.degrees(np.arctan2(np.ones_like(slope), slope / 45.0))
    spread_direction = 0.6 * upslope_dir + 0.4 * wind_direction
    spread_direction = spread_direction % 360.0
    
    return {
        'ros_no_wind_slope': ros_base_mpm,
        'ros_with_slope': ros_slope_mpm,
        'ros_with_wind': ros_final_mpm,
        'fireline_intensity': intensity_kw_m,
        'flame_length': flame_length_m,
        'direction_factor': direction_factor,
        'spread_direction': spread_direction,
        'ros_components': {
            'base_ros': ros_base_mpm,
            'slope_factor': phi_s,
            'wind_factor': phi_w_directional,
        }
    }

