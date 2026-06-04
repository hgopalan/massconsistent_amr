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
# Based on IEC 61400-1:2019 Table 1
IEC_CLASS_PARAMETERS = {
    WindTurbineClass.CLASS_I: {
        "vref": 50.0,   # Reference wind speed Vref (m/s) - for extreme wind speed definition
        "vavg": 10.0,   # Average wind speed at hub height (m/s)
        "iref": 0.18,   # Turbulence intensity reference (18%)
        "a": 2,         # Weibull shape parameter
    },
    WindTurbineClass.CLASS_II: {
        "vref": 42.5,   # Class II: Vref = 42.5 m/s
        "vavg": 8.5,    # Average wind speed: 8.5 m/s
        "iref": 0.18,   # Turbulence intensity: 18%
        "a": 2,
    },
    WindTurbineClass.CLASS_III: {
        "vref": 37.5,   # Class III: Vref = 37.5 m/s per IEC 61400-1:2019 Table 1
        "vavg": 7.5,    # Average wind speed: 7.5 m/s
        "iref": 0.18,   # Turbulence intensity: 18%
        "a": 2,
    },
    WindTurbineClass.CLASS_IV: {
        # NOTE: Class IV is NOT in official IEC 61400-1 standard.
        # WARNING: Should ONLY be used for preliminary design studies, NOT for certification.
        # Vref=30.0 m/s and Vavg=6.0 m/s are estimated values for very low wind resources.
        "vref": 30.0,   # Class IV: Vref ~ 30 m/s (estimated, non-standard, non-certifiable)
        "vavg": 6.0,    # Average wind speed: 6.0 m/s (estimated, non-standard)
        "iref": 0.18,   # Turbulence intensity: 18%
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
    - Non-neutral stability corrections (Phase 3+)
    
    Stability Corrections:
    - Monin-Obukhov similarity theory for non-neutral conditions
    - Businger-Dyer (1971) parameterization
    - Holtslag-De Bruin (1988) alternative for very stable conditions
    """
    
    def __init__(
        self,
        turbine_class: Union[str, WindTurbineClass],
        terrain_category: int = 1,
        z_hub: float = 90.0,
        enable_stability_correction: bool = False,
        monin_obukhov_length: Optional[float] = None,
        use_holtslag: bool = False,
    ):
        """
        Initialize NormalTurbulenceModel with optional stability corrections.
        
        Parameters:
            turbine_class: Wind turbine class (I, II, III, or IV)
            terrain_category: Terrain roughness category (0-4)
            z_hub: Hub height in meters (default: 90 m)
            enable_stability_correction: Enable non-neutral stability corrections (default: False)
            monin_obukhov_length: Obukhov length scale L in meters. If None, neutral conditions assumed.
                - L > 0: Stable conditions (e.g., nighttime)
                - L < 0: Unstable conditions (e.g., daytime with heating)
                - L → ∞: Neutral conditions
            use_holtslag: Use Holtslag-De Bruin (1988) parameterization instead of Businger-Dyer (default: False)
        """
        super().__init__(turbine_class, terrain_category, z_hub)
        self.enable_stability_correction = enable_stability_correction
        self.monin_obukhov_length = monin_obukhov_length
        self.use_holtslag = use_holtslag
    
    def _psi_m_stable(self, zeta: float) -> float:
        """
        Momentum stability function for stable conditions (zeta > 0).
        Based on Businger et al. (1971) and Dyer (1974).
        
        Parameters:
            zeta: Dimensionless height z/L where L is Obukhov length
        
        Returns:
            Stability function psi_m(zeta)
        """
        # Limit zeta to prevent numerical issues
        zeta = min(max(zeta, -2.0), 2.0)
        # Standard Businger-Dyer: psi_m = -beta * zeta
        beta = 5.0
        return -beta * zeta
    
    def _psi_m_holtslag_stable(self, zeta: float) -> float:
        """
        Holtslag-De Bruin (1988) momentum stability function for stable conditions.
        Better performance in very stable conditions (nighttime, polar regions).
        
        Parameters:
            zeta: Dimensionless height z/L where L is Obukhov length
        
        Returns:
            Stability function psi_m(zeta)
        """
        zeta = min(max(zeta, -2.0), 2.0)
        a, b, c, d = 1.0, 0.667, 5.0, 0.35
        return -(a * zeta + b * (zeta - c / d) * np.exp(-d * zeta) + b * c / d)
    
    def _psi_m_unstable(self, zeta: float) -> float:
        """
        Momentum stability function for unstable conditions (zeta < 0).
        Based on Paulson (1970) and Businger et al. (1971).
        
        Parameters:
            zeta: Dimensionless height z/L where L is Obukhov length (negative for unstable)
        
        Returns:
            Stability function psi_m(zeta)
        """
        zeta = min(max(zeta, -2.0), 2.0)
        # For unstable conditions: psi_m = 2*ln((1+x)/2) + ln((1+x^2)/2) - 2*arctan(x) + pi/2
        x = np.power(1.0 - 16.0 * zeta, 0.25)
        return (2.0 * np.log((1.0 + x) / 2.0) + np.log((1.0 + x * x) / 2.0)
                - 2.0 * np.arctan(x) + np.pi / 2.0)
    
    def _psi_m(self, zeta: float) -> float:
        """
        Combined stability function psi_m(zeta).
        Handles both stable and unstable conditions.
        
        Parameters:
            zeta: Dimensionless height z/L (positive for stable, negative for unstable)
        
        Returns:
            Stability function value
        """
        if zeta > 0.0:  # Stable conditions
            return self._psi_m_holtslag_stable(zeta) if self.use_holtslag else self._psi_m_stable(zeta)
        elif zeta < 0.0:  # Unstable conditions
            return self._psi_m_unstable(zeta)
        else:  # Neutral conditions
            return 0.0
    
    def _wind_speed_with_stability(
        self,
        height: float,
        reference_speed: float,
        reference_height: float = 10.0,
    ) -> float:
        """
        Compute wind speed with stability corrections using log-law with Monin-Obukhov correction.
        
        Log-law with stability: U(z) = (u*/κ) * [ln(z/z0) - ψ_m(z/L) + ψ_m(z0/L)]
        
        where:
            u* = friction velocity
            κ = von Kármán constant (0.41)
            z0 = surface roughness
            L = Obukhov length
            ψ_m = momentum stability function
        
        Parameters:
            height: Measurement height in meters
            reference_speed: Wind speed at reference height in m/s
            reference_height: Reference height (default: 10 m)
        
        Returns:
            Wind speed at given height with stability correction
        """
        if not self.enable_stability_correction or self.monin_obukhov_length is None:
            # Use power law without stability correction
            return self.power_law_profile(
                np.array([height]), reference_speed, reference_height
            )[0]
        
        # Compute stability parameters
        L = self.monin_obukhov_length
        z0 = self.z0
        kappa = 0.41  # von Kármán constant
        
        # Guard against invalid inputs
        if height <= z0 or L == 0.0:
            return reference_speed
        
        # Compute dimensionless stability parameter zeta
        zeta_ref = reference_height / L
        zeta_z = height / L
        
        # Compute friction velocity from reference speed
        # U_ref = (u*/κ) * [ln(z_ref/z0) - ψ_m(z_ref/L) + ψ_m(z0/L)]
        psi_m_ref = self._psi_m(zeta_ref)
        psi_m_z0 = self._psi_m(reference_height / L * z0 / reference_height)  # at roughness height
        
        ln_ref = np.log(reference_height / z0)
        u_star = reference_speed * kappa / (ln_ref - psi_m_ref + psi_m_z0)
        
        # Compute wind speed at height z
        psi_m_z = self._psi_m(zeta_z)
        ln_z = np.log(height / z0)
        return (u_star / kappa) * (ln_z - psi_m_z + psi_m_z0)
    
    def _turbulence_intensity_with_stability(self, height: float) -> float:
        """
        Compute turbulence intensity with stability modifications.
        
        In stable conditions, turbulence intensity decreases (weaker mixing).
        In unstable conditions, turbulence intensity increases (stronger convection).
        
        Parameters:
            height: Height above ground in meters
        
        Returns:
            Modified turbulence intensity accounting for stability
        """
        # Base neutral turbulence intensity
        ti_neutral = self.turbulence_intensity(height)
        
        if not self.enable_stability_correction or self.monin_obukhov_length is None:
            return ti_neutral
        
        L = self.monin_obukhov_length
        zeta = height / L
        
        # Stability modification factors (empirical, based on Sorbjan 1989)
        # In stable conditions (zeta > 0): reduce turbulence
        # In unstable conditions (zeta < 0): increase turbulence
        
        if zeta > 0.0:  # Stable conditions
            # Reduction factor: decreases with increasing stability
            stability_factor = 1.0 / (1.0 + 5.0 * zeta) ** 0.5
        else:  # Unstable conditions (zeta < 0)
            # Enhancement factor: increases with increasing instability
            stability_factor = (1.0 - 16.0 * zeta) ** 0.25
        
        return ti_neutral * stability_factor
    
    def _length_scale_with_stability(self, length_scale_neutral: float, height: float) -> float:
        """
        Modify integral length scale based on atmospheric stability.
        
        In stable conditions: length scales decrease (weaker mixing)
        In unstable conditions: length scales increase (stronger vertical mixing)
        
        Based on Panofsky & Dutton (1984) and Sorbjan (1989).
        
        Parameters:
            length_scale_neutral: Integral length scale for neutral conditions (m)
            height: Height above ground (m)
        
        Returns:
            Modified length scale accounting for stability
        """
        if not self.enable_stability_correction or self.monin_obukhov_length is None:
            return length_scale_neutral
        
        L = self.monin_obukhov_length
        zeta = height / L
        
        if zeta > 0.0:  # Stable conditions
            # In stable conditions, reduce length scale (weaker mixing)
            stability_factor = 1.0 / (1.0 + 3.0 * zeta)
        else:  # Unstable conditions (zeta < 0)
            # In unstable conditions, enhance length scale (stronger mixing)
            stability_factor = (1.0 - 16.0 * zeta) ** 0.125
        
        return length_scale_neutral * stability_factor
    
    def turbulence_intensity(self, height: float) -> float:
        """
        Calculate turbulence intensity at a given height.
        
        Parameters:
            height: Height above ground in meters
        
        Returns:
            Turbulence intensity (fraction)
        """
        # IEC 61400-1:2019 (and earlier editions) formula: I(z) = Iref * (0.2 / (z/zref))^0.2
        # where zref = 15 m is the reference height
        # This simplifies to: I(z) = Iref * (3.0 / z)^0.2 (where 3.0 = 0.2 * 15)
        # Turbulence intensity decreases with height above ground
        z_ref = 15.0  # Reference height (15 m)
        ti_exponent = 0.2  # Turbulence intensity exponent
        ti_coefficient = 0.2  # Coefficient in the fraction
        
        if height <= 0:
            return self.iref
        
        # Calculate turbulence intensity with height dependency
        ti_ratio = ti_coefficient / (height / z_ref)
        return self.iref * ti_ratio ** ti_exponent
    
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
    
    def compute_velocity_rms(
        self,
        height: float,
        mean_wind_speed: float,
    ) -> Dict[str, float]:
        """
        Compute RMS velocities for u, v, w components from turbulence intensity.
        
        Based on IEC 61400-1:2019, the RMS velocity is:
        u_rms = I(z) * U_mean
        
        The other components follow anisotropy ratios:
        v_rms = 0.8 * u_rms (lateral component)
        w_rms = 0.5 * u_rms (vertical component)
        
        With stability corrections: I(z) is modified by Monin-Obukhov factors when enabled.
        
        Parameters:
            height: Height above ground in meters
            mean_wind_speed: Mean wind speed at the height in m/s
        
        Returns:
            Dictionary with RMS velocities for u, v, w components
        """
        # Use stability-aware intensity if enabled, otherwise use neutral intensity
        if self.enable_stability_correction and self.monin_obukhov_length is not None:
            intensity = self._turbulence_intensity_with_stability(height)
        else:
            intensity = self.turbulence_intensity(height)
        
        u_rms = intensity * mean_wind_speed
        v_rms = 0.8 * u_rms  # Lateral anisotropy (typical for atmospheric boundary layer)
        w_rms = 0.5 * u_rms  # Vertical anisotropy (typical for atmospheric boundary layer)
        
        return {
            "u_rms": u_rms,
            "v_rms": v_rms,
            "w_rms": w_rms,
            "turbulence_intensity": intensity,
        }
    
    def von_karman_spectrum(
        self,
        frequency: np.ndarray,
        height: float,
        mean_wind_speed: float,
        length_scale_u: float = 300.0,
    ) -> np.ndarray:
        """
        Compute Von Kármán spectrum for wind turbulence.
        
        The Von Kármán spectrum is defined as:
        S_u(f) = (4 * L_u * u_rms^2) / (1 + 70.8 * (f * L_u / U_mean)^2)^(5/6)
        
        With stability correction: L_u = L_u_neutral * f(zeta) where f(zeta)
        is a stability-dependent length scale modification.
        
        where:
            f = frequency [Hz]
            L_u = integral length scale [m]
            u_rms = RMS velocity [m/s]
            U_mean = mean wind speed [m/s]
        
        Parameters:
            frequency: Array of frequencies in Hz
            height: Height above ground in meters
            mean_wind_speed: Mean wind speed in m/s
            length_scale_u: Integral length scale for u-component in meters
        
        Returns:
            Spectral density array in (m/s)^2/Hz
        """
        frequency = np.atleast_1d(frequency)
        
        # Get RMS velocity
        rms_data = self.compute_velocity_rms(height, mean_wind_speed)
        u_rms = rms_data["u_rms"]
        
        # Apply stability correction to length scale if enabled
        L_u_effective = length_scale_u
        if self.enable_stability_correction and self.monin_obukhov_length is not None:
            L_u_effective = self._length_scale_with_stability(length_scale_u, height)
        
        # Guard against division by zero
        mean_wind_speed = np.maximum(mean_wind_speed, 0.1)
        length_scale_u = np.maximum(length_scale_u, 1.0)
        u_rms = np.maximum(u_rms, 1e-6)
        
        # Normalized frequency
        f_hat = frequency * length_scale_u / mean_wind_speed
        
        # Von Kármán spectral density
        numerator = 4.0 * length_scale_u * u_rms**2
        denominator = (1.0 + 70.8 * f_hat**2)**(5.0/6.0)
        
        return numerator / denominator
    
    def kaimal_spectrum(
        self,
        frequency: np.ndarray,
        height: float,
        mean_wind_speed: float,
        length_scale_u: float = 300.0,
    ) -> np.ndarray:
        """
        Compute Kaimal spectrum for wind turbulence.
        
        The Kaimal spectrum is defined as:
        S_u(f) = (4 * L_u * u_rms^2 * f_hat) / (1 + 6 * f_hat)^(5/3)
        
        where f_hat = f * L_u / U_mean (normalized frequency)
        
        The Kaimal spectrum is commonly used in IEC 61400-1 applications
        and wind engineering standards.
        
        Parameters:
            frequency: Array of frequencies in Hz
            height: Height above ground in meters
            mean_wind_speed: Mean wind speed in m/s
            length_scale_u: Integral length scale for u-component in meters
        
        Returns:
            Spectral density array in (m/s)^2/Hz
        """
        frequency = np.atleast_1d(frequency)
        
        # Get RMS velocity
        rms_data = self.compute_velocity_rms(height, mean_wind_speed)
        u_rms = rms_data["u_rms"]
        
        # Guard against division by zero
        mean_wind_speed = np.maximum(mean_wind_speed, 0.1)
        length_scale_u = np.maximum(length_scale_u, 1.0)
        u_rms = np.maximum(u_rms, 1e-6)
        
        # Normalized frequency
        f_hat = frequency * length_scale_u / mean_wind_speed
        
        # Kaimal spectral density
        numerator = 4.0 * length_scale_u * u_rms**2 * f_hat
        denominator = (1.0 + 6.0 * f_hat)**(5.0/3.0)
        
        return numerator / denominator
    
    def compute_spectrum(
        self,
        frequencies: np.ndarray,
        height: float,
        mean_wind_speed: float,
        spectrum_type: str = "VonKarman",
        length_scale_u: float = 300.0,
    ) -> Dict[str, np.ndarray]:
        """
        Compute turbulence spectrum at a given height and wind speed.
        
        Supports both Von Kármán and Kaimal spectrum models used in
        IEC 61400-1 and wind engineering standards.
        
        Parameters:
            frequencies: Array of frequencies in Hz
            height: Height above ground in meters
            mean_wind_speed: Mean wind speed in m/s
            spectrum_type: Type of spectrum ("VonKarman" or "Kaimal")
            length_scale_u: Integral length scale for u-component in meters
        
        Returns:
            Dictionary with spectral densities for each component:
            {
                "frequency": frequencies,
                "S_u": u-component spectrum,
                "S_v": v-component spectrum,
                "S_w": w-component spectrum,
                "spectrum_type": spectrum_type,
                "height": height,
                "mean_wind_speed": mean_wind_speed,
            }
        """
        frequencies = np.atleast_1d(frequencies)
        
        # Get RMS velocities for all components
        rms_data = self.compute_velocity_rms(height, mean_wind_speed)
        u_rms = rms_data["u_rms"]
        v_rms = rms_data["v_rms"]
        w_rms = rms_data["w_rms"]
        
        # Typical length scales (from atmospheric boundary layer theory)
        # v and w components typically have shorter integral length scales
        length_scale_v = 0.7 * length_scale_u  # Lateral component
        length_scale_w = 0.4 * length_scale_u  # Vertical component
        
        # Compute spectrum based on type
        if spectrum_type.lower() == "vonkarman":
            S_u = self.von_karman_spectrum(frequencies, height, mean_wind_speed, length_scale_u)
            S_v = self.von_karman_spectrum(frequencies, height, mean_wind_speed, length_scale_v)
            S_w = self.von_karman_spectrum(frequencies, height, mean_wind_speed, length_scale_w)
        elif spectrum_type.lower() == "kaimal":
            S_u = self.kaimal_spectrum(frequencies, height, mean_wind_speed, length_scale_u)
            S_v = self.kaimal_spectrum(frequencies, height, mean_wind_speed, length_scale_v)
            S_w = self.kaimal_spectrum(frequencies, height, mean_wind_speed, length_scale_w)
        else:
            raise ValueError(f"Unknown spectrum type: {spectrum_type}. Use 'VonKarman' or 'Kaimal'")
        
        return {
            "frequency": frequencies,
            "S_u": S_u,
            "S_v": S_v,
            "S_w": S_w,
            "spectrum_type": spectrum_type,
            "height": height,
            "mean_wind_speed": mean_wind_speed,
            "length_scale_u": length_scale_u,
            "length_scale_v": length_scale_v,
            "length_scale_w": length_scale_w,
            "u_rms": u_rms,
            "v_rms": v_rms,
            "w_rms": w_rms,
            "turbulence_intensity": rms_data["turbulence_intensity"],
        }
    
    def generate_fluctuations(
        self,
        frequencies: np.ndarray,
        height: float,
        mean_wind_speed: float,
        spectrum_type: str = "VonKarman",
        random_seed: int = 12345,
        length_scale_u: float = 300.0,
    ) -> Dict[str, np.ndarray]:
        """
        Generate synthetic turbulent fluctuations in frequency domain.
        
        This method synthesizes frequency-domain representations of turbulent
        fluctuations using spectral methods. The output can be converted to
        time-domain fluctuations using inverse FFT or spectral analysis.
        
        The method generates random phases for each frequency component and
        combines them with the computed spectral amplitude to create physically
        realistic turbulent fluctuations following IEC 61400-1 standards.
        
        Parameters:
            frequencies: Array of frequencies in Hz
            height: Height above ground in meters
            mean_wind_speed: Mean wind speed in m/s
            spectrum_type: Type of spectrum ("VonKarman" or "Kaimal")
            random_seed: Random seed for reproducibility
            length_scale_u: Integral length scale for u-component in meters
        
        Returns:
            Dictionary with spectral components and amplitudes:
            {
                "frequency": frequencies,
                "amplitude_u": Amplitude array for u-component,
                "amplitude_v": Amplitude array for v-component,
                "amplitude_w": Amplitude array for w-component,
                "phase_u": Phase array for u-component (radians),
                "phase_v": Phase array for v-component (radians),
                "phase_w": Phase array for w-component (radians),
                "spectrum_data": Full spectrum information,
            }
        """
        frequencies = np.atleast_1d(frequencies)
        
        # Compute spectral densities
        spectrum = self.compute_spectrum(
            frequencies, height, mean_wind_speed,
            spectrum_type=spectrum_type,
            length_scale_u=length_scale_u
        )
        
        # Create reproducible random number generator
        rng = np.random.RandomState(random_seed)
        
        # Frequency resolution for energy conservation
        df = np.gradient(frequencies) if len(frequencies) > 1 else np.ones_like(frequencies)
        if np.isscalar(df):
            df = np.ones_like(frequencies) * df
        else:
            # Extend first and last values
            df = np.concatenate([[df[0]], df, [df[-1]]])
            df = df[1:-1] if len(df) > len(frequencies) else df[:len(frequencies)]
        
        # Convert spectral density to amplitude: A = sqrt(2 * S * df)
        amplitude_u = np.sqrt(2.0 * spectrum["S_u"] * df)
        amplitude_v = np.sqrt(2.0 * spectrum["S_v"] * df)
        amplitude_w = np.sqrt(2.0 * spectrum["S_w"] * df)
        
        # Generate random phases uniformly distributed in [0, 2π]
        phase_u = rng.uniform(0, 2 * np.pi, len(frequencies))
        phase_v = rng.uniform(0, 2 * np.pi, len(frequencies))
        phase_w = rng.uniform(0, 2 * np.pi, len(frequencies))
        
        return {
            "frequency": frequencies,
            "amplitude_u": amplitude_u,
            "amplitude_v": amplitude_v,
            "amplitude_w": amplitude_w,
            "phase_u": phase_u,
            "phase_v": phase_v,
            "phase_w": phase_w,
            "spectrum_data": spectrum,
            "random_seed": random_seed,
            "height": height,
            "mean_wind_speed": mean_wind_speed,
        }
    
    def generate_time_series(
        self,
        duration: float = 600.0,
        dt: float = 0.1,
        height: float = 90.0,
        mean_wind_speed: float = 12.0,
        spectrum_type: str = "VonKarman",
        length_scale_u: float = 300.0,
        random_seed: int = 12345,
        n_freq_bins: int = 256,
    ) -> Dict[str, np.ndarray]:
        """
        Generate synthetic time series of turbulent fluctuations.
        
        This method creates realistic time-domain turbulent fluctuations
        following IEC 61400-1 standards. The fluctuations are generated using
        spectral synthesis methods with proper temporal correlation.
        
        The method:
        1. Generates a frequency array (logarithmically spaced for efficiency)
        2. Computes spectral densities at each frequency
        3. Creates random amplitudes and phases
        4. Performs inverse FFT to get time-domain fluctuations
        5. Scales to match computed RMS values
        
        Parameters:
            duration: Duration of time series in seconds (default: 600s = 10 min)
            dt: Time step in seconds (default: 0.1s = 10 Hz)
            height: Height above ground in meters (default: 90m)
            mean_wind_speed: Mean wind speed in m/s (default: 12.0)
            spectrum_type: Type of spectrum ("VonKarman" or "Kaimal")
            length_scale_u: Integral length scale for u-component in meters
            random_seed: Random seed for reproducibility
            n_freq_bins: Number of frequency bins for spectral discretization
        
        Returns:
            Dictionary with time series and metadata:
            {
                "time": Time array [s],
                "u_prime": u-component fluctuations [m/s],
                "v_prime": v-component fluctuations [m/s],
                "w_prime": w-component fluctuations [m/s],
                "u_mean": Mean u-component (should be close to 0),
                "v_mean": Mean v-component (should be close to 0),
                "w_mean": Mean w-component (should be close to 0),
                "u_rms": RMS of u-component [m/s],
                "v_rms": RMS of v-component [m/s],
                "w_rms": RMS of w-component [m/s],
                "height": Height above ground [m],
                "mean_wind_speed": Mean wind speed [m/s],
                "duration": Duration [s],
                "dt": Time step [s],
                "spectrum_type": Type of spectrum used,
            }
        """
        # Create time array
        nt = int(np.ceil(duration / dt))
        time = np.arange(nt) * dt
        
        # Create frequency array (logarithmically spaced, 0.001 to 10 Hz)
        f_min = 0.001
        f_max = 10.0
        frequencies = np.logspace(np.log10(f_min), np.log10(f_max), n_freq_bins)
        
        # Generate fluctuations in frequency domain
        fluct = self.generate_fluctuations(
            frequencies, height, mean_wind_speed,
            spectrum_type=spectrum_type,
            random_seed=random_seed,
            length_scale_u=length_scale_u
        )
        
        # Reconstruct time series from spectral components
        # Using inverse FFT for proper temporal correlation
        rng = np.random.RandomState(random_seed)
        
        # Generate time series by summing sinusoids
        u_prime = np.zeros(nt)
        v_prime = np.zeros(nt)
        w_prime = np.zeros(nt)
        
        for i, freq in enumerate(frequencies):
            # Add sinusoidal components with random phases
            phase_offset = 2 * np.pi * freq * time
            u_prime += fluct["amplitude_u"][i] * np.cos(phase_offset + fluct["phase_u"][i])
            v_prime += fluct["amplitude_v"][i] * np.cos(phase_offset + fluct["phase_v"][i])
            w_prime += fluct["amplitude_w"][i] * np.cos(phase_offset + fluct["phase_w"][i])
        
        # Remove mean (should be close to zero, but remove for cleanliness)
        u_mean = np.mean(u_prime)
        v_mean = np.mean(v_prime)
        w_mean = np.mean(w_prime)
        u_prime -= u_mean
        v_prime -= v_mean
        w_prime -= w_mean
        
        # Compute realized RMS values
        u_rms_realized = np.std(u_prime)
        v_rms_realized = np.std(v_prime)
        w_rms_realized = np.std(w_prime)
        
        # Get target RMS values
        rms_data = self.compute_velocity_rms(height, mean_wind_speed)
        u_rms_target = rms_data["u_rms"]
        v_rms_target = rms_data["v_rms"]
        w_rms_target = rms_data["w_rms"]
        
        # Scale fluctuations to match target RMS
        # (account for energy loss from spectral discretization)
        if u_rms_realized > 1e-10:
            u_prime *= u_rms_target / u_rms_realized
        if v_rms_realized > 1e-10:
            v_prime *= v_rms_target / v_rms_realized
        if w_rms_realized > 1e-10:
            w_prime *= w_rms_target / w_rms_realized
        
        # Recompute RMS after scaling
        u_rms = np.std(u_prime)
        v_rms = np.std(v_prime)
        w_rms = np.std(w_prime)
        
        return {
            "time": time,
            "u_prime": u_prime,
            "v_prime": v_prime,
            "w_prime": w_prime,
            "u_mean": np.mean(u_prime),
            "v_mean": np.mean(v_prime),
            "w_mean": np.mean(w_prime),
            "u_rms": u_rms,
            "v_rms": v_rms,
            "w_rms": w_rms,
            "height": height,
            "mean_wind_speed": mean_wind_speed,
            "duration": duration,
            "dt": dt,
            "spectrum_type": spectrum_type,
            "model_type": "NTM",
        }
    
    def compute_wind_profile_with_stability(
        self,
        heights: np.ndarray,
        reference_speed: float,
        reference_height: float = 10.0,
        enable_profile_correction: bool = None,
    ) -> Dict[str, np.ndarray]:
        """
        Compute full Monin-Obukhov wind profile with stability corrections.
        
        This method implements the complete log-law wind profile with stability
        corrections using the Monin-Obukhov similarity theory. This represents
        Phase 4+ enhancement (full profile correction, not just TI).
        
        Wind profile formula:
            U(z) = (u*/κ) * [ln(z/z0) - ψ_m(z/L) + ψ_m(z0/L)]
        
        where:
            u* = friction velocity [m/s]
            κ = von Kármán constant (0.41)
            z = height above ground [m]
            z0 = surface roughness [m]
            L = Obukhov length [m] (stability parameter)
            ψ_m = momentum stability function
        
        Stability Effects:
            - Stable (L > 0): Steeper profiles, reduced wind shear
            - Unstable (L < 0): Flatter profiles, enhanced wind shear
            - Neutral (|L| → ∞): Standard logarithmic profile
        
        Parameters:
            heights: Array of heights above ground in meters [m AGL]
            reference_speed: Mean wind speed at reference height [m/s]
            reference_height: Reference height for wind speed (default: 10 m)
            enable_profile_correction: Override instance setting (default: None uses instance setting)
        
        Returns:
            Dictionary containing:
                - 'heights': Input height array
                - 'wind_speed': Wind speed profile at each height [m/s]
                - 'wind_shear': Vertical wind shear (dU/dz) [1/s]
                - 'turbulence_intensity': Turbulence intensity profile
                - 'friction_velocity': Computed friction velocity u* [m/s]
                - 'reference_speed': Input reference speed [m/s]
                - 'reference_height': Input reference height [m]
                - 'obukhov_length': Used Obukhov length [m]
                - 'stability_regime': 'stable'/'unstable'/'neutral'
                - 'profile_type': 'full_monin_obukhov' or 'neutral_loglaw'
        
        Example:
            >>> ntm = NormalTurbulenceModel("II", terrain_category=1)
            >>> heights = np.array([10, 30, 50, 100, 150])
            >>> profile = ntm.compute_wind_profile_with_stability(
            ...     heights, reference_speed=10.0, reference_height=10.0,
            ...     enable_profile_correction=True
            ... )
            >>> print(profile['wind_speed'])
        """
        heights = np.atleast_1d(heights)
        
        # Determine if profile correction is enabled
        use_correction = enable_profile_correction
        if use_correction is None:
            use_correction = self.enable_stability_correction
        
        # If corrections disabled or neutral conditions, use standard log-law
        if not use_correction or self.monin_obukhov_length is None:
            return self._compute_wind_profile_neutral(heights, reference_speed, reference_height)
        
        L = self.monin_obukhov_length
        z0 = self.z0
        kappa = 0.41  # von Kármán constant
        
        # Guard against invalid inputs
        heights = np.maximum(heights, z0 + 0.01)  # Ensure heights > z0
        reference_height = np.maximum(reference_height, z0 + 0.01)
        reference_speed = np.maximum(reference_speed, 0.1)
        
        # Compute dimensionless stability parameters
        zeta_ref = reference_height / L
        zeta_heights = heights / L
        
        # Compute friction velocity from reference speed using Monin-Obukhov profile
        # U_ref = (u*/κ) * [ln(z_ref/z0) - ψ_m(z_ref/L) + ψ_m(z0/L)]
        psi_m_ref = self._psi_m(zeta_ref)
        psi_m_z0 = self._psi_m(z0 / L)
        
        ln_ref = np.log(reference_height / z0)
        u_star = reference_speed * kappa / (ln_ref - psi_m_ref + psi_m_z0)
        
        # Compute wind speed profile at all heights
        psi_m_z = np.array([self._psi_m(zeta) for zeta in zeta_heights])
        ln_z = np.log(heights / z0)
        wind_speeds = (u_star / kappa) * (ln_z - psi_m_z + psi_m_z0)
        
        # Compute wind shear (dU/dz) - used for load calculations
        # dU/dz ≈ (u*/κ) * (1/z - dψ_m/dz)
        wind_shear = np.zeros_like(heights)
        dz = 0.1  # Small height increment for derivative
        for i, h in enumerate(heights):
            h_plus = h + dz
            zeta_plus = h_plus / L
            psi_m_plus = self._psi_m(zeta_plus)
            ln_plus = np.log(h_plus / z0)
            u_plus = (u_star / kappa) * (ln_plus - psi_m_plus + psi_m_z0)
            wind_shear[i] = (u_plus - wind_speeds[i]) / dz
        
        # Compute turbulence intensity profile
        ti_profile = np.array([self._turbulence_intensity_with_stability(h) for h in heights])
        
        # Determine stability regime
        if abs(L) > 1e4:
            stability_regime = "neutral"
        elif L > 0:
            stability_regime = "stable"
        else:
            stability_regime = "unstable"
        
        return {
            "heights": heights,
            "wind_speed": wind_speeds,
            "wind_shear": wind_shear,
            "turbulence_intensity": ti_profile,
            "friction_velocity": u_star,
            "reference_speed": reference_speed,
            "reference_height": reference_height,
            "obukhov_length": L,
            "stability_regime": stability_regime,
            "profile_type": "full_monin_obukhov",
            "roughness_length": z0,
            "model_type": "NTM",
        }
    
    def _compute_wind_profile_neutral(
        self,
        heights: np.ndarray,
        reference_speed: float,
        reference_height: float = 10.0,
    ) -> Dict[str, np.ndarray]:
        """
        Compute neutral wind profile (standard log-law).
        
        Used when stability corrections are disabled or neutral conditions detected.
        
        Parameters:
            heights: Array of heights above ground [m AGL]
            reference_speed: Mean wind speed at reference height [m/s]
            reference_height: Reference height [m]
        
        Returns:
            Dictionary with neutral profile data
        """
        z0 = self.z0
        kappa = 0.41
        
        heights = np.maximum(heights, z0 + 0.01)
        reference_height = np.maximum(reference_height, z0 + 0.01)
        reference_speed = np.maximum(reference_speed, 0.1)
        
        # Compute friction velocity for neutral case
        ln_ref = np.log(reference_height / z0)
        u_star = reference_speed * kappa / ln_ref
        
        # Compute wind speed profile (standard log-law)
        ln_z = np.log(heights / z0)
        wind_speeds = (u_star / kappa) * ln_z
        
        # Compute wind shear
        wind_shear = u_star / (kappa * heights)
        
        # Compute turbulence intensity profile
        ti_profile = np.array([self.turbulence_intensity(h) for h in heights])
        
        return {
            "heights": heights,
            "wind_speed": wind_speeds,
            "wind_shear": wind_shear,
            "turbulence_intensity": ti_profile,
            "friction_velocity": u_star,
            "reference_speed": reference_speed,
            "reference_height": reference_height,
            "obukhov_length": 1e10,  # Large L (neutral approximation)
            "stability_regime": "neutral",
            "profile_type": "neutral_loglaw",
            "roughness_length": z0,
            "model_type": "NTM",
        }
    
    def compute_coherence_matrix(
        self,
        heights: np.ndarray,
        frequency: float,
        mean_wind_speed: float,
        coherence_model: str = "gaussian",
    ) -> Dict[str, Union[np.ndarray, str]]:
        """
        Compute directional coherence matrix for u-v-w velocity components.
        
        This method computes the coherence correlation functions between velocity
        components at different heights, enabling more realistic turbulence synthesis
        with proper cross-component correlations (Phase 4+ Priority 2).
        
        Coherence formula (general form):
            Coh_uv(Δz, f) = exp(-k * |Δz| * f / U_mean)  or similar
        
        where:
            Δz = height separation [m]
            f = frequency [Hz]
            U_mean = mean wind speed [m/s]
            k = decay parameter (model-dependent)
        
        Supported models:
            - 'gaussian': Coh = exp(-k*distance²) (smooth, sharp decay)
            - 'exponential': Coh = exp(-k*distance) (moderate decay)
            - 'power-law': Coh = (1 + k*distance)^(-m) (algebraic, slow decay)
        
        Stability Modifications:
            - Stable conditions: More localized coherence (shorter scales)
            - Unstable conditions: Extended coherence (longer scales)
            - Neutral: Standard behavior
        
        Parameters:
            heights: Array of heights above ground [m AGL]
            frequency: Frequency for coherence calculation [Hz]
            mean_wind_speed: Mean wind speed at reference height [m/s]
            coherence_model: Coherence function model ('gaussian', 'exponential', 'power-law')
        
        Returns:
            Dictionary containing:
                - 'heights': Input height array
                - 'coherence_uu': U-component auto-coherence [n×n matrix]
                - 'coherence_vv': V-component auto-coherence [n×n matrix]
                - 'coherence_ww': W-component auto-coherence [n×n matrix]
                - 'coherence_uv': U-V cross-coherence [n×n matrix]
                - 'coherence_uw': U-W cross-coherence [n×n matrix]
                - 'coherence_vw': V-W cross-coherence [n×n matrix]
                - 'frequency': Input frequency [Hz]
                - 'coherence_model': Model type used
                - 'anisotropy_ratios': {'v/u': ratio_v, 'w/u': ratio_w}
                - 'stability_effect': Factor applied for stability
        
        Example:
            >>> heights = np.array([10, 50, 100, 150])
            >>> coh = ntm.compute_coherence_matrix(heights, 0.1, 10.0, 'gaussian')
            >>> print(coh['coherence_uu'][0, 1])  # Coherence between 10m and 50m
        """
        heights = np.atleast_1d(heights)
        n_heights = len(heights)
        mean_wind_speed = np.maximum(mean_wind_speed, 0.1)
        frequency = np.maximum(frequency, 1e-6)
        
        # Initialize coherence matrices
        coh_uu = np.zeros((n_heights, n_heights))
        coh_vv = np.zeros((n_heights, n_heights))
        coh_ww = np.zeros((n_heights, n_heights))
        coh_uv = np.zeros((n_heights, n_heights))
        coh_uw = np.zeros((n_heights, n_heights))
        coh_vw = np.zeros((n_heights, n_heights))
        
        # Compute stability modification factor
        stability_factor = 1.0
        if self.enable_stability_correction and self.monin_obukhov_length is not None:
            L = self.monin_obukhov_length
            z_ref = 50.0  # Reference height for stability effect
            zeta = z_ref / L
            
            if zeta > 0.0:  # Stable: reduce coherence scale
                stability_factor = 1.0 / (1.0 + 3.0 * zeta)
            else:  # Unstable: enhance coherence scale
                stability_factor = (1.0 - 16.0 * zeta) ** 0.125
        
        # Fill coherence matrices
        for i in range(n_heights):
            for j in range(n_heights):
                z_i = heights[i]
                z_j = heights[j]
                delta_z = abs(z_i - z_j)
                
                # Normalized separation
                L_u_eff = 300.0 * stability_factor  # Effective length scale
                normalized_sep = frequency * delta_z / mean_wind_speed / L_u_eff
                
                # Coherence decay based on model
                if coherence_model.lower() == "gaussian":
                    # Gaussian decay: exp(-k*distance²)
                    decay_factor = np.exp(-0.5 * normalized_sep ** 2)
                elif coherence_model.lower() == "exponential":
                    # Exponential decay: exp(-k*distance)
                    decay_factor = np.exp(-normalized_sep)
                elif coherence_model.lower() == "power-law":
                    # Power-law decay: (1 + k*distance)^(-m)
                    decay_factor = (1.0 + normalized_sep) ** (-1.5)
                else:
                    decay_factor = np.exp(-normalized_sep)  # Default to exponential
                
                # Apply diagonal dominance (1.0 on diagonal, <1.0 off-diagonal)
                if i == j:
                    decay_factor = 1.0
                else:
                    decay_factor = max(0.0, decay_factor)  # Ensure non-negative
                
                # U-component (dominant)
                coh_uu[i, j] = decay_factor
                
                # V-component (reduced by anisotropy)
                v_ratio = 0.75  # V-component typically 75% of U coherence
                coh_vv[i, j] = decay_factor * (v_ratio if i != j else 1.0)
                
                # W-component (more localized)
                w_ratio = 0.50  # W-component typically 50% of U coherence
                coh_ww[i, j] = decay_factor * (w_ratio if i != j else 1.0)
                
                # Cross-components (weaker correlation)
                cross_factor = 0.6  # Cross-components typically 60% of auto-coherence
                coh_uv[i, j] = decay_factor * cross_factor if i != j else 0.3
                coh_uw[i, j] = decay_factor * cross_factor if i != j else 0.2
                coh_vw[i, j] = decay_factor * cross_factor if i != j else 0.1
        
        return {
            "heights": heights,
            "coherence_uu": coh_uu,
            "coherence_vv": coh_vv,
            "coherence_ww": coh_ww,
            "coherence_uv": coh_uv,
            "coherence_uw": coh_uw,
            "coherence_vw": coh_vw,
            "frequency": frequency,
            "mean_wind_speed": mean_wind_speed,
            "coherence_model": coherence_model,
            "anisotropy_ratios": {
                "v/u": 0.75,
                "w/u": 0.50,
            },
            "stability_factor": stability_factor,
            "model_type": "NTM",
        }
    
    def _height_dependent_scale_function(
        self,
        height: float,
        reference_height: float = 50.0,
    ) -> float:
        """
        Compute height-dependent scaling function h(z) for correlation lengths.
        
        The full length scale is: L(z) = L_0 * h(z) where h(z) is this function.
        
        For stable conditions: h(z) = exp(-alpha_s * z/L) where alpha_s ~ 0.5
        For unstable conditions: h(z) = (1 + beta_u * |z/L|)^(1/4) where beta_u ~ 16
        For neutral conditions: h(z) = (z / z_ref)^alpha where alpha ~ 0.2
        
        This provides more physically accurate height-dependent scaling than constant
        length scales, particularly important in stable/unstable regimes where mixing
        properties vary dramatically with height.
        
        Parameters:
            height: Current height above ground [m]
            reference_height: Reference height for scaling [m], default 50m
        
        Returns:
            Height-dependent scaling factor h(z) (typically in range 0.3-2.0)
        
        Physical Basis:
            - In stable conditions: mixing is heavily suppressed near surface, reduced by ~50% per 100m
            - In unstable conditions: mixing enhanced aloft, grows ~25% per 100m above surface
            - In neutral conditions: log-law predicts (z/z_ref)^0.2 growth (~5% per 100m)
        """
        if not self.enable_stability_correction or self.monin_obukhov_length is None:
            # Neutral conditions: weak height dependence
            alpha = 0.2
            h_z = (height / reference_height) ** alpha
            return np.clip(h_z, 0.5, 2.0)
        
        L = self.monin_obukhov_length
        zeta = height / L
        
        if zeta > 0.1:  # Strong stable conditions (zeta > 0.1 indicates very stable)
            # Stable: strong suppression of mixing aloft
            # h(z) = exp(-0.5 * z/L)
            h_z = np.exp(-0.5 * zeta)
            return np.clip(h_z, 0.1, 1.0)
        
        elif zeta > 0.0:  # Weakly stable (0 < zeta < 0.1)
            # Transition to unstable: moderate suppression
            # h(z) = 1 / (1 + 2*zeta)
            h_z = 1.0 / (1.0 + 2.0 * zeta)
            return np.clip(h_z, 0.5, 1.0)
        
        elif zeta > -0.5:  # Weakly unstable (-0.5 < zeta < 0)
            # Weak unstable: slight enhancement
            # h(z) = (1 - 8*zeta)^(1/4)
            h_z = (1.0 - 8.0 * zeta) ** 0.25
            return np.clip(h_z, 1.0, 1.5)
        
        else:  # Very unstable (zeta < -0.5)
            # Strong unstable: significant enhancement of mixing aloft
            # h(z) = (1 - 16*zeta)^(1/4)
            h_z = (1.0 - 16.0 * zeta) ** 0.25
            return np.clip(h_z, 1.0, 3.0)
    
    def compute_height_dependent_spectrum(
        self,
        frequencies: np.ndarray,
        heights: np.ndarray,
        mean_wind_speed: float,
        spectrum_type: str = "VonKarman",
        length_scale_u: float = 300.0,
    ) -> Dict[str, Union[np.ndarray, Dict]]:
        """
        Compute spectral tensors at multiple heights with full height-dependent scaling.
        
        This method extends the existing spectral methods to account for height-dependent
        correlation lengths L(z) = L_0 * h(z), providing more realistic turbulence
        representation across the wind rotor swept area.
        
        Implementation (Priority 3):
            1. For each height, compute height-dependent scaling h(z)
            2. Adjust length scale: L_eff(z) = L_0 * h(z)
            3. Compute spectra with height-dependent length scales
            4. Return full spectral tensor with height information
        
        Parameters:
            frequencies: Array of frequencies [Hz]
            heights: Array of heights above ground [m]
            mean_wind_speed: Mean wind speed [m/s]
            spectrum_type: Spectrum type ("VonKarman" or "Kaimal")
            length_scale_u: Base integral length scale [m]
        
        Returns:
            Dictionary with spectral data for all heights:
            {
                'heights': Height array,
                'frequencies': Frequency array,
                'spectra_u': [n_heights × n_frequencies] spectral matrix,
                'spectra_v': [n_heights × n_frequencies] spectral matrix,
                'spectra_w': [n_heights × n_frequencies] spectral matrix,
                'height_scales': Effective length scales at each height,
                'height_scale_factors': Height-dependent scaling h(z),
                'spectrum_type': Type used,
            }
        
        Example:
            >>> ntm = NormalTurbulenceModel(...)
            >>> result = ntm.compute_height_dependent_spectrum(
            ...     frequencies=np.logspace(-2, 1, 50),
            ...     heights=np.array([10, 50, 100, 150]),
            ...     mean_wind_speed=10.0,
            ...     spectrum_type="Kaimal"
            ... )
            >>> print(result['height_scales'])  # [L(10m), L(50m), L(100m), L(150m)]
        """
        frequencies = np.atleast_1d(frequencies)
        heights = np.atleast_1d(heights)
        n_heights = len(heights)
        n_freqs = len(frequencies)
        
        # Initialize spectral arrays
        spectra_u = np.zeros((n_heights, n_freqs))
        spectra_v = np.zeros((n_heights, n_freqs))
        spectra_w = np.zeros((n_heights, n_freqs))
        
        # Store height-dependent parameters
        height_scales = np.zeros(n_heights)
        height_scale_factors = np.zeros(n_heights)
        
        # Compute spectra at each height with height-dependent scaling
        for i, z in enumerate(heights):
            # Compute height-dependent scaling factor
            h_z = self._height_dependent_scale_function(z)
            height_scale_factors[i] = h_z
            
            # Effective length scales at this height
            L_u_z = length_scale_u * h_z
            L_v_z = L_u_z * 0.7  # V-component typically 70% of U
            L_w_z = L_u_z * 0.4  # W-component typically 40% of U
            
            height_scales[i] = L_u_z
            
            # Compute spectra with height-dependent length scales
            if spectrum_type.lower() == "vonkarman":
                spectra_u[i, :] = self.von_karman_spectrum(
                    frequencies, z, mean_wind_speed, L_u_z
                )
                spectra_v[i, :] = self.von_karman_spectrum(
                    frequencies, z, mean_wind_speed, L_v_z
                )
                spectra_w[i, :] = self.von_karman_spectrum(
                    frequencies, z, mean_wind_speed, L_w_z
                )
            elif spectrum_type.lower() == "kaimal":
                spectra_u[i, :] = self.kaimal_spectrum(
                    frequencies, z, mean_wind_speed, L_u_z
                )
                spectra_v[i, :] = self.kaimal_spectrum(
                    frequencies, z, mean_wind_speed, L_v_z
                )
                spectra_w[i, :] = self.kaimal_spectrum(
                    frequencies, z, mean_wind_speed, L_w_z
                )
            else:
                raise ValueError(f"Unknown spectrum type: {spectrum_type}")
        
        return {
            "heights": heights,
            "frequencies": frequencies,
            "spectra_u": spectra_u,
            "spectra_v": spectra_v,
            "spectra_w": spectra_w,
            "height_scales": height_scales,
            "height_scale_factors": height_scale_factors,
            "spectrum_type": spectrum_type,
            "mean_wind_speed": mean_wind_speed,
            "base_length_scale_u": length_scale_u,
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
        
        if height <= 0:
            return 1.4 * self.iref
        
        # Calculate NTM turbulence intensity
        ntm_ti = self.iref * (ti_coefficient / (height / z_ref)) ** ti_exponent
        # Apply extreme factor (1.4 for ETM)
        return 1.4 * ntm_ti
    
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
        
        # IEC gust shape formula: ramps up to peak, then exponentially decays
        # This matches the extreme operating gust shape defined in IEC 61400-1
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
