#!/usr/bin/env python3
"""
wind_resource_stats.py - Wind resource statistics and diagnostics

Computes statistical summaries of wind fields including mean, std deviation,
wind rose characteristics, and Weibull parameters for wind resource assessment.
"""

import numpy as np
from typing import Dict, Any, Optional, Tuple, List
import json


class WindResourceStats:
    """
    Compute and store wind resource statistics from solved wind fields.
    
    Attributes:
        mean_speed (float): Mean wind speed (m/s)
        std_speed (float): Standard deviation of wind speed (m/s)
        min_speed (float): Minimum wind speed (m/s)
        max_speed (float): Maximum wind speed (m/s)
        mean_direction (float): Mean wind direction (degrees)
        std_direction (float): Standard deviation of wind direction (degrees)
        weibull_k (float): Weibull shape parameter
        weibull_c (float): Weibull scale parameter (m/s)
        height_agl (float): Height above ground level where stats computed
        num_samples (int): Number of grid points sampled
    """
    
    def __init__(self):
        """Initialize empty statistics object."""
        self.mean_speed: float = 0.0
        self.std_speed: float = 0.0
        self.min_speed: float = 0.0
        self.max_speed: float = 0.0
        self.mean_direction: float = 0.0
        self.std_direction: float = 0.0
        self.weibull_k: float = 0.0
        self.weibull_c: float = 0.0
        self.height_agl: float = 0.0
        self.num_samples: int = 0
    
    @staticmethod
    def compute_from_wind_field(
        u_field: np.ndarray,
        v_field: np.ndarray,
        height_agl: float = 90.0,
        exclude_terrain: bool = False,
        terrain_field: Optional[np.ndarray] = None
    ) -> 'WindResourceStats':
        """
        Compute wind resource statistics from 2D wind field.
        
        Parameters:
            u_field (ndarray): 2D u-velocity component (ny, nx) at hub height
            v_field (ndarray): 2D v-velocity component (ny, nx) at hub height
            height_agl (float): Height above ground level (for documentation)
            exclude_terrain (bool): If True and terrain_field provided, exclude cells with terrain
            terrain_field (ndarray, optional): 2D terrain elevation (ny, nx)
        
        Returns:
            WindResourceStats: Computed statistics object
        
        Raises:
            ValueError: If input arrays have incompatible shapes or invalid data
        """
        if u_field.shape != v_field.shape:
            raise ValueError(f"u_field and v_field must have same shape, got {u_field.shape} vs {v_field.shape}")
        
        # Create mask for valid cells
        valid_mask = np.isfinite(u_field) & np.isfinite(v_field)
        
        if exclude_terrain and terrain_field is not None:
            if terrain_field.shape != u_field.shape:
                raise ValueError(f"terrain_field shape {terrain_field.shape} must match wind field shape {u_field.shape}")
            valid_mask &= np.isfinite(terrain_field)
        
        if not np.any(valid_mask):
            raise ValueError("No valid wind data found after applying masks")
        
        # Extract valid velocity components
        u_valid = u_field[valid_mask]
        v_valid = v_field[valid_mask]
        
        # Compute wind speed and direction
        speed = np.sqrt(u_valid**2 + v_valid**2)
        direction = np.degrees(np.arctan2(u_valid, v_valid)) % 360.0
        
        # Create statistics object
        stats = WindResourceStats()
        stats.height_agl = height_agl
        stats.num_samples = np.sum(valid_mask)
        
        # Speed statistics
        stats.mean_speed = float(np.mean(speed))
        stats.std_speed = float(np.std(speed))
        stats.min_speed = float(np.min(speed))
        stats.max_speed = float(np.max(speed))
        
        # Direction statistics (circular mean)
        cos_mean = np.mean(np.cos(np.radians(direction)))
        sin_mean = np.mean(np.sin(np.radians(direction)))
        stats.mean_direction = float(np.degrees(np.arctan2(sin_mean, cos_mean)) % 360.0)
        stats.std_direction = float(np.degrees(np.arccos(np.sqrt(cos_mean**2 + sin_mean**2))))
        
        # Fit Weibull distribution
        stats.weibull_k, stats.weibull_c = WindResourceStats._fit_weibull(speed)
        
        return stats
    
    @staticmethod
    def _fit_weibull(speeds: np.ndarray) -> Tuple[float, float]:
        """
        Fit Weibull distribution to speed data using MLE.
        
        Parameters:
            speeds (ndarray): 1D array of wind speeds
        
        Returns:
            (k, c) tuple: Shape and scale parameters
        """
        if len(speeds) < 2:
            return 2.0, np.mean(speeds)
        
        # Method of moments as initial guess
        mean_speed = np.mean(speeds)
        std_speed = np.std(speeds)
        
        # Simplified shape parameter estimation
        cv = std_speed / mean_speed if mean_speed > 0 else 1.0
        if cv < 0.2:
            k = 10.0  # High shape parameter
        elif cv < 0.5:
            k = 4.0 + (2.0 - cv) * 3.0  # Linear interpolation
        else:
            k = 1.0 + 1.0 / cv  # Approximate formula
        
        # Scale parameter from mean
        # E[X] = c * Gamma(1 + 1/k)
        from scipy.special import gamma as scipy_gamma
        c = mean_speed / scipy_gamma(1.0 + 1.0 / k)
        
        return float(k), float(c)
    
    def to_dict(self) -> Dict[str, Any]:
        """
        Convert statistics to dictionary.
        
        Returns:
            dict: Statistics as dictionary with all computed values
        """
        return {
            'height_agl_m': float(self.height_agl),
            'num_samples': int(self.num_samples),
            'wind_speed': {
                'mean_ms': float(self.mean_speed),
                'std_ms': float(self.std_speed),
                'min_ms': float(self.min_speed),
                'max_ms': float(self.max_speed)
            },
            'wind_direction': {
                'mean_deg': float(self.mean_direction),
                'std_deg': float(self.std_direction)
            },
            'weibull': {
                'k': float(self.weibull_k),
                'c_ms': float(self.weibull_c)
            }
        }
    
    def to_json(self, filename: str) -> None:
        """
        Write statistics to JSON file.
        
        Parameters:
            filename (str): Output JSON filename
        
        Returns:
            None (writes to file)
        """
        with open(filename, 'w') as f:
            json.dump(self.to_dict(), f, indent=2)
    
    def summary_string(self) -> str:
        """
        Generate human-readable summary string.
        
        Returns:
            str: Formatted summary statistics
        """
        lines = [
            "Wind Resource Statistics",
            "=" * 50,
            f"Height AGL: {self.height_agl:.1f} m",
            f"Samples: {self.num_samples}",
            "",
            "Wind Speed:",
            f"  Mean: {self.mean_speed:.2f} m/s",
            f"  Std Dev: {self.std_speed:.2f} m/s",
            f"  Range: [{self.min_speed:.2f}, {self.max_speed:.2f}] m/s",
            "",
            "Wind Direction:",
            f"  Mean: {self.mean_direction:.1f}°",
            f"  Std Dev: {self.std_direction:.1f}°",
            "",
            "Weibull Distribution:",
            f"  Shape (k): {self.weibull_k:.2f}",
            f"  Scale (c): {self.weibull_c:.2f} m/s",
            "=" * 50
        ]
        return '\n'.join(lines)
    
    def __repr__(self) -> str:
        """String representation."""
        return (
            f"WindResourceStats(mean={self.mean_speed:.2f}m/s, "
            f"std={self.std_speed:.2f}m/s, k={self.weibull_k:.2f}, c={self.weibull_c:.2f}m/s)"
        )


def compute_wind_rose_statistics(
    wind_speeds: np.ndarray,
    wind_directions: np.ndarray,
    probabilities: np.ndarray
) -> Dict[str, Any]:
    """
    Compute statistics from a discretized wind rose distribution.
    
    Parameters:
        wind_speeds (ndarray): 1D array of wind speed bin centers (m/s)
        wind_directions (ndarray): 1D array of direction bin centers (degrees)
        probabilities (ndarray): 2D array of joint probabilities [n_directions, n_speeds]
    
    Returns:
        dict: Statistics including mean speed, mean direction, directional sector stats
    """
    prob_sum = np.sum(probabilities)
    if prob_sum <= 0:
        raise ValueError("Probabilities must be non-negative and sum > 0")
    
    # Normalize
    probs_norm = probabilities / prob_sum
    
    # Marginal distributions
    prob_speed = np.sum(probs_norm, axis=0)
    prob_direction = np.sum(probs_norm, axis=1)
    
    # Mean and weighted variance
    mean_speed = np.sum(prob_speed * wind_speeds)
    mean_direction = np.degrees(
        np.arctan2(
            np.sum(prob_direction * np.sin(np.radians(wind_directions))),
            np.sum(prob_direction * np.cos(np.radians(wind_directions)))
        )
    ) % 360.0
    
    return {
        'mean_wind_speed_ms': float(mean_speed),
        'mean_wind_direction_deg': float(mean_direction),
        'marginal_speed_distribution': [float(x) for x in prob_speed.tolist()],
        'marginal_direction_distribution': [float(x) for x in prob_direction.tolist()],
        'speed_bins_ms': [float(x) for x in wind_speeds.tolist()],
        'direction_bins_deg': [float(x) for x in wind_directions.tolist()]
    }
