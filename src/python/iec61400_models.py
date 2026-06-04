#!/usr/bin/env python3
"""
IEC 61400-1 Wind Input Models for Wind Turbine Certification

This module implements the standard wind input models defined in IEC 61400-1:2019
for wind turbine design and certification. Supported models:

- NTM: Normal Turbulence Model - for normal operating wind conditions
- ETM: Extreme Turbulence Model - for extreme wind events
- EOG: Extreme Operating Gust - extreme gust during operation
- EWS: Extreme Wind Shear - extreme vertical wind shear profiles
- ECG: Extreme Coherent Gust - extreme coherent gust with direction change

Reference:
    IEC 61400-1:2019 "Wind turbines - Part 1: Design requirements"
    https://www.iec.ch/
"""

import numpy as np
from typing import Tuple, Dict, List, Optional, Union
from dataclasses import dataclass
from enum import Enum


class WindTurbineClass(Enum):
    """Wind turbine power classes defined in IEC 61400-1."""
    CLASS_I = "I"      # High wind resource sites
    CLASS_II = "II"    # Medium wind resource sites
    CLASS_III = "III"  # Low wind resource sites
    CLASS_IV = "IV"    # Very low wind resource sites


@dataclass
class IECWindParameters:
    """
    IEC 61400-1 parameters for a given wind turbine class and terrain category.
    
    Attributes:
        turbine_class: Wind turbine class (I, II, III, or IV)
        terrain_category: Terrain category (0, 1, 2, 3, or 4)
        vref: Reference wind speed (10-min mean at 10m height) in m/s
        vavg: Average wind speed in m/s
        iref: Reference turbulence intensity at 15 m/s
        a: Turbulence intensity coefficient
        z_hub: Hub height in meters
    """
    turbine_class: str
    terrain_category: int
    vref: float
    vavg: float
    iref: float
    a: float
    z_hub: float = 90.0


# Lookup tables for IEC 61400-1 parameters
IEC_CLASS_PARAMETERS = {
    WindTurbineClass.CLASS_I: {
        "vref": 10.0,   # Reference wind speed (m/s)
        "vavg": 9.0,    # Average wind speed (m/s)
        "iref": 0.18,   # Turbulence intensity reference
        "a": 2,         # Weibull shape parameter
    },
    WindTurbineClass.CLASS_II: {
        "vref": 8.5,
        "vavg": 7.5,
        "iref": 0.18,
        "a": 2,
    },
    WindTurbineClass.CLASS_III: {
        "vref": 7.0,
        "vavg": 6.0,
        "iref": 0.18,
        "a": 2,
    },
    WindTurbineClass.CLASS_IV: {
        "vref": 6.0,
        "vavg": 5.0,
        "iref": 0.18,
        "a": 2,
    },
}

# Terrain category parameters (roughness length z0 in meters)
TERRAIN_ROUGHNESS = {
    0: 0.0002,  # Sea, very smooth
    1: 0.03,    # Smooth terrain (grass fields, etc.)
    2: 0.1,     # Open terrain with obstacles
    3: 0.4,     # Complex terrain (cities, forests)
    4: 1.6,     # Very complex terrain
}

# Shear exponent based on terrain category
TERRAIN_SHEAR_EXPONENT = {
    0: 0.11,
    1: 0.145,
    2: 0.20,
    3: 0.27,
    4: 0.40,
}


class IEC61400Model:
    """
    Base class for IEC 61400-1 wind input models.
    
    This class provides common functionality for all wind input models defined
    in the IEC 61400-1 standard.
    """
    
    def __init__(
        self,
        turbine_class: Union[str, WindTurbineClass],
        terrain_category: int = 1,
        z_hub: float = 90.0,
    ):
        """
        Initialize the IEC 61400-1 model.
        
        Parameters:
            turbine_class: Wind turbine class (I, II, III, or IV or WindTurbineClass enum)
            terrain_category: Terrain roughness category (0-4)
            z_hub: Hub height in meters (default: 90 m)
        
        Raises:
            ValueError: If terrain_category is not in valid range [0, 4]
        """
        if isinstance(turbine_class, str):
            try:
                turbine_class = WindTurbineClass(turbine_class)
            except ValueError:
                raise ValueError(
                    f"Invalid turbine class: {turbine_class}. "
                    f"Must be one of: {[c.value for c in WindTurbineClass]}"
                )
        
        if not 0 <= terrain_category <= 4:
            raise ValueError("Terrain category must be between 0 and 4")
        
        self.turbine_class = turbine_class
        self.terrain_category = terrain_category
        self.z_hub = z_hub
        
        # Get parameters from lookup table
        class_params = IEC_CLASS_PARAMETERS[turbine_class]
        self.vref = class_params["vref"]
        self.vavg = class_params["vavg"]
        self.iref = class_params["iref"]
        self.a = class_params["a"]
        self.z0 = TERRAIN_ROUGHNESS[terrain_category]
        self.shear_exponent = TERRAIN_SHEAR_EXPONENT[terrain_category]
    
    def log_law_profile(
        self,
        heights: np.ndarray,
        reference_speed: float,
        reference_height: float = 10.0,
    ) -> np.ndarray:
        """
        Calculate logarithmic wind profile.
        
        Parameters:
            heights: Array of heights in meters
            reference_speed: Wind speed at reference height (m/s)
            reference_height: Reference height for speed (default: 10 m)
        
        Returns:
            Wind speed profile at given heights (m/s)
        """
        # Avoid log(0) by ensuring heights > z0
        heights = np.atleast_1d(heights)
        z_safe = np.maximum(heights, self.z0 * 1.001)
        
        von_karman = 0.41  # Von Kármán constant
        u_star = (reference_speed * von_karman) / np.log(reference_height / self.z0)
        
        return u_star * np.log(z_safe / self.z0) / von_karman
    
    def power_law_profile(
        self,
        heights: np.ndarray,
        reference_speed: float,
        reference_height: float = 10.0,
        exponent: Optional[float] = None,
    ) -> np.ndarray:
        """
        Calculate power-law wind profile.
        
        Parameters:
            heights: Array of heights in meters
            reference_speed: Wind speed at reference height (m/s)
            reference_height: Reference height for speed (default: 10 m)
            exponent: Power-law exponent (default: terrain shear exponent)
        
        Returns:
            Wind speed profile at given heights (m/s)
        """
        if exponent is None:
            exponent = self.shear_exponent
        
        heights = np.atleast_1d(heights)
        return reference_speed * (heights / reference_height) ** exponent


class NormalTurbulenceModel(IEC61400Model):
    """
    Normal Turbulence Model (NTM) for IEC 61400-1.
    
    The NTM defines the normal wind climate with continuous turbulence.
    It includes:
    - Mean wind speed profile
    - Turbulence intensity profile
    - Turbulent fluctuations (von Kármán spectrum)
    """
    
    def turbulence_intensity(self, height: float) -> float:
        """
        Calculate turbulence intensity at a given height.
        
        Parameters:
            height: Height above ground in meters
        
        Returns:
            Turbulence intensity (fraction)
        """
        # IEC 61400-1 Eq. (1): I(z) = Iref * (0.2 / (z/zref))^0.2
        z_ref = 15.0  # Reference height (15 m)
        ti_exponent = 0.2  # Turbulence intensity exponent
        ti_coefficient = 0.2  # Coefficient in the fraction
        return self.iref * (ti_coefficient / (height / z_ref)) ** ti_exponent if height > 0 else self.iref
    
    def generate_wind_profile(
        self,
        heights: np.ndarray,
        mean_speed: Optional[float] = None,
    ) -> Dict[str, np.ndarray]:
        """
        Generate NTM wind profile with turbulence intensity.
        
        Parameters:
            heights: Array of heights in meters
            mean_speed: Mean wind speed (default: vref)
        
        Returns:
            Dictionary with wind speed and turbulence intensity profiles
        """
        if mean_speed is None:
            mean_speed = self.vref
        
        heights = np.atleast_1d(heights)
        
        # Wind speed profile using power law
        wind_speeds = self.power_law_profile(
            heights, mean_speed, reference_height=10.0
        )
        
        # Turbulence intensity profile
        turbulence_intensities = np.array(
            [self.turbulence_intensity(h) for h in heights]
        )
        
        return {
            "heights": heights,
            "mean_wind_speed": wind_speeds,
            "turbulence_intensity": turbulence_intensities,
            "model_type": "NTM",
        }


class ExtremeTurbulenceModel(IEC61400Model):
    """
    Extreme Turbulence Model (ETM) for IEC 61400-1.
    
    The ETM defines extreme turbulence conditions with 1-year recurrence period.
    It specifies higher turbulence intensity than NTM.
    """
    
    def turbulence_intensity(self, height: float) -> float:
        """
        Calculate extreme turbulence intensity at a given height.
        
        Parameters:
            height: Height above ground in meters
        
        Returns:
            Turbulence intensity (fraction) - 1-year extreme
        """
        # ETM uses higher turbulence (factor 1.4 relative to NTM at hub height)
        z_ref = 15.0
        ti_exponent = 0.2
        ti_coefficient = 0.2
        ntm_ti = self.iref * (ti_coefficient / (height / z_ref)) ** ti_exponent
        return 1.4 * ntm_ti if height > 0 else 1.4 * self.iref
    
    def generate_wind_profile(
        self,
        heights: np.ndarray,
        mean_speed: Optional[float] = None,
    ) -> Dict[str, np.ndarray]:
        """
        Generate ETM wind profile with extreme turbulence.
        
        Parameters:
            heights: Array of heights in meters
            mean_speed: Mean wind speed (default: vref)
        
        Returns:
            Dictionary with wind speed and turbulence intensity profiles
        """
        if mean_speed is None:
            mean_speed = self.vref
        
        heights = np.atleast_1d(heights)
        
        # Wind speed profile
        wind_speeds = self.power_law_profile(
            heights, mean_speed, reference_height=10.0
        )
        
        # Extreme turbulence intensity profile
        turbulence_intensities = np.array(
            [self.turbulence_intensity(h) for h in heights]
        )
        
        return {
            "heights": heights,
            "mean_wind_speed": wind_speeds,
            "turbulence_intensity": turbulence_intensities,
            "model_type": "ETM",
        }


class ExtremeOperatingGust(IEC61400Model):
    """
    Extreme Operating Gust (EOG) for IEC 61400-1.
    
    The EOG defines a single extreme gust that may occur during turbine operation.
    It specifies gust duration, magnitude, and shape.
    """
    
    def gust_speed(self, wind_speed: float) -> float:
        """
        Calculate extreme gust speed.
        
        Parameters:
            wind_speed: Operating wind speed in m/s
        
        Returns:
            Gust speed in m/s
        """
        # IEC 61400-1: Gust amplitude based on wind speed
        # EOG gust speed = 2.5 * sigma
        sigma = wind_speed * 0.11  # Standard deviation (assuming Iref = 0.18)
        return 2.5 * sigma
    
    def generate_gust_profile(
        self,
        duration: float = 10.0,
        time_to_peak: float = 5.0,
        mean_speed: float = 10.0,
        sampling_rate: float = 10.0,
    ) -> Dict[str, Union[np.ndarray, float]]:
        """
        Generate extreme operating gust profile.
        
        Parameters:
            duration: Gust duration in seconds
            time_to_peak: Time to reach peak gust in seconds
            mean_speed: Operating wind speed in m/s
            sampling_rate: Sampling rate in Hz
        
        Returns:
            Dictionary with time array and gust amplitude
        """
        time = np.arange(0, duration, 1.0 / sampling_rate)
        gust_amplitude = self.gust_speed(mean_speed)
        
        # Halos-Karman formula for gust shape
        # Ramps up to peak, then decays
        ramp_indices = time <= time_to_peak
        decay_indices = time > time_to_peak
        
        gust = np.zeros_like(time)
        if np.any(ramp_indices):
            gust[ramp_indices] = gust_amplitude * (
                0.5 * (1 - np.cos(np.pi * time[ramp_indices] / time_to_peak))
            )
        if np.any(decay_indices):
            decay_time = time[decay_indices] - time_to_peak
            gust[decay_indices] = gust_amplitude * np.exp(
                -3.0 * decay_time / (duration - time_to_peak)
            )
        
        return {
            "time": time,
            "gust_profile": gust,
            "peak_gust": np.max(gust),
            "mean_speed": mean_speed,
            "model_type": "EOG",
        }


class ExtremeWindShear(IEC61400Model):
    """
    Extreme Wind Shear (EWS) for IEC 61400-1.
    
    The EWS defines extreme vertical wind shear profiles.
    It specifies the maximum shear that may occur during turbine operation.
    """
    
    def shear_profile(
        self,
        heights: np.ndarray,
        reference_speed: float = 10.0,
        shear_type: str = "vertical",
    ) -> np.ndarray:
        """
        Generate extreme wind shear profile.
        
        Parameters:
            heights: Array of heights in meters
            reference_speed: Reference wind speed at hub height (m/s)
            shear_type: Type of shear - "vertical" or "horizontal"
        
        Returns:
            Wind speed profile with extreme shear
        """
        heights = np.atleast_1d(heights)
        
        if shear_type == "vertical":
            # Extreme positive shear: enhanced with height
            alpha = self.shear_exponent + 0.2  # Enhanced shear
            return self.power_law_profile(
                heights, reference_speed, reference_height=10.0, exponent=alpha
            )
        elif shear_type == "horizontal":
            # Horizontal shear creates lateral wind variation
            # Simplified: sinusoidal variation across domain
            return reference_speed * (1 + 0.2 * np.sin(2 * np.pi * heights / 100))
        else:
            raise ValueError(f"Unknown shear type: {shear_type}")
    
    def generate_shear_profile(
        self,
        heights: np.ndarray,
        reference_speed: float = 10.0,
    ) -> Dict[str, np.ndarray]:
        """
        Generate extreme wind shear profile.
        
        Parameters:
            heights: Array of heights in meters
            reference_speed: Reference wind speed in m/s
        
        Returns:
            Dictionary with wind speeds showing extreme shear
        """
        heights = np.atleast_1d(heights)
        
        # Generate shear profile
        wind_speeds = self.shear_profile(
            heights, reference_speed, shear_type="vertical"
        )
        
        # Calculate shear exponent
        if len(heights) > 1:
            h1, h2 = heights[0], heights[-1]
            v1, v2 = wind_speeds[0], wind_speeds[-1]
            if h1 > 0 and v1 > 0:
                shear_exp = np.log(v2 / v1) / np.log(h2 / h1)
            else:
                shear_exp = self.shear_exponent
        else:
            shear_exp = self.shear_exponent
        
        return {
            "heights": heights,
            "wind_speed": wind_speeds,
            "shear_exponent": shear_exp,
            "model_type": "EWS",
        }


class ExtremeCoherentGust(IEC61400Model):
    """
    Extreme Coherent Gust (ECG) for IEC 61400-1.
    
    The ECG defines an extreme coherent gust with a change in wind direction.
    It combines gust and directional shear.
    """
    
    def generate_gust_with_direction_change(
        self,
        duration: float = 10.0,
        time_to_peak: float = 5.0,
        mean_speed: float = 10.0,
        direction_change: float = 180.0,
        sampling_rate: float = 10.0,
    ) -> Dict[str, Union[np.ndarray, float]]:
        """
        Generate extreme coherent gust with direction change.
        
        Parameters:
            duration: Gust duration in seconds
            time_to_peak: Time to reach peak gust in seconds
            mean_speed: Operating wind speed in m/s
            direction_change: Total direction change in degrees
            sampling_rate: Sampling rate in Hz
        
        Returns:
            Dictionary with time array, gust amplitude, and direction change
        """
        time = np.arange(0, duration, 1.0 / sampling_rate)
        
        # Gust speed component
        gust_amplitude = 3.3 * mean_speed * 0.11  # IEC ECG gust magnitude
        
        # Gust profile (similar to EOG)
        ramp_indices = time <= time_to_peak
        decay_indices = time > time_to_peak
        
        gust = np.zeros_like(time)
        if np.any(ramp_indices):
            gust[ramp_indices] = gust_amplitude * (
                0.5 * (1 - np.cos(np.pi * time[ramp_indices] / time_to_peak))
            )
        if np.any(decay_indices):
            decay_time = time[decay_indices] - time_to_peak
            gust[decay_indices] = gust_amplitude * np.exp(
                -3.0 * decay_time / (duration - time_to_peak)
            )
        
        # Direction change profile
        direction_profile = np.zeros_like(time)
        if np.any(ramp_indices):
            direction_profile[ramp_indices] = (
                direction_change * 0.5 *
                (1 - np.cos(np.pi * time[ramp_indices] / time_to_peak))
            )
        if np.any(decay_indices):
            direction_profile[decay_indices] = direction_change
        
        return {
            "time": time,
            "gust_speed": gust,
            "direction_change": direction_profile,
            "peak_gust": np.max(gust),
            "total_direction_change": direction_change,
            "mean_speed": mean_speed,
            "model_type": "ECG",
        }


def create_iec_model(
    model_type: str,
    turbine_class: Union[str, WindTurbineClass],
    terrain_category: int = 1,
    z_hub: float = 90.0,
) -> IEC61400Model:
    """
    Factory function to create IEC 61400-1 model instances.
    
    Parameters:
        model_type: Type of model - "NTM", "ETM", "EOG", "EWS", or "ECG"
        turbine_class: Wind turbine class (I, II, III, or IV)
        terrain_category: Terrain roughness category (0-4)
        z_hub: Hub height in meters
    
    Returns:
        Instance of appropriate IEC model class
    
    Raises:
        ValueError: If model_type is not recognized
    """
    model_classes = {
        "NTM": NormalTurbulenceModel,
        "ETM": ExtremeTurbulenceModel,
        "EOG": ExtremeOperatingGust,
        "EWS": ExtremeWindShear,
        "ECG": ExtremeCoherentGust,
    }
    
    if model_type not in model_classes:
        raise ValueError(
            f"Unknown model type: {model_type}. "
            f"Must be one of: {list(model_classes.keys())}"
        )
    
    return model_classes[model_type](turbine_class, terrain_category, z_hub)
