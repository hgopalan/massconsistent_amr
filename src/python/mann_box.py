#!/usr/bin/env python3
"""
Mann Box Model Python Interface

Provides a high-level Python API for the Mann Box anisotropic spectral tensor
model for synthetic turbulence generation in complex terrain.

The Mann Box model is an industry-standard approach for generating realistic
3D turbulent wind fields with proper anisotropy and spatial coherence structure.

Reference:
    Mann, J. (1994). The spatial structure of neutral atmospheric surface-layer
    turbulence. Journal of Fluid Mechanics, 273, 141-168.

Example:
    >>> mann = MannBox(length_scale_u=300.0, length_scale_v=200.0, length_scale_w=120.0)
    >>> spectrum = mann.compute_spectrum(
    ...     frequencies=np.logspace(-2, 1, 100),
    ...     height=90.0,
    ...     mean_wind_speed=12.0
    ... )
    >>> print(f"Spectrum shape: {spectrum['S_uu'].shape}")
"""

import numpy as np
from typing import Dict, Tuple, Optional, List, Union
from dataclasses import dataclass
import warnings


@dataclass
class MannBoxParameters:
    """
    Parameters for Mann Box spectral tensor model.
    
    Attributes:
        length_scale_u: Integral length scale for u-component [m], typical 300-400 m
        length_scale_v: Integral length scale for v-component [m], typical 0.7*L_u
        length_scale_w: Integral length scale for w-component [m], typical 0.4*L_u
        variance_u: Velocity variance for u-component [m²/s²]
        variance_v: Velocity variance for v-component [m²/s²]
        variance_w: Velocity variance for w-component [m²/s²]
        asymmetry: Asymmetry parameter (Mann model parameter α), typical 1.0-2.0
        uv_coherence: u-v cross-spectrum coherence factor, typical 0.75
        uw_coherence: u-w cross-spectrum coherence factor, typical 0.50
        vw_coherence: v-w cross-spectrum coherence factor, typical 0.65
    """
    length_scale_u: float = 300.0
    length_scale_v: float = 210.0
    length_scale_w: float = 120.0
    variance_u: float = 1.0
    variance_v: float = 0.64
    variance_w: float = 0.25
    asymmetry: float = 1.0
    uv_coherence: float = 0.75
    uw_coherence: float = 0.50
    vw_coherence: float = 0.65


class MannBox:
    """
    Mann Box Anisotropic Spectral Tensor Model
    
    Provides methods to compute the Mann Box spectral tensor components
    for synthetic turbulence generation in wind energy applications.
    """
    
    def __init__(self, 
                 length_scale_u: float = 300.0,
                 length_scale_v: Optional[float] = None,
                 length_scale_w: Optional[float] = None,
                 variance_u: float = 1.0,
                 variance_v: Optional[float] = None,
                 variance_w: Optional[float] = None,
                 asymmetry: float = 1.0,
                 uv_coherence: float = 0.75,
                 uw_coherence: float = 0.50,
                 vw_coherence: float = 0.65):
        """
        Initialize Mann Box model with parameters.
        
        Parameters
        ----------
        length_scale_u : float
            Integral length scale for u-component [m], default 300 m
        length_scale_v : float, optional
            Integral length scale for v-component [m], default 0.7 * L_u
        length_scale_w : float, optional
            Integral length scale for w-component [m], default 0.4 * L_u
        variance_u : float
            u-component variance [m²/s²], default 1.0
        variance_v : float, optional
            v-component variance [m²/s²], default 0.8² * variance_u = 0.64
        variance_w : float, optional
            w-component variance [m²/s²], default 0.5² * variance_u = 0.25
        asymmetry : float
            Asymmetry parameter α, typical range 1.0-2.0
        uv_coherence : float
            u-v coherence factor, typical 0.75
        uw_coherence : float
            u-w coherence factor, typical 0.50
        vw_coherence : float
            v-w coherence factor, typical 0.65
        """
        # Set length scales with defaults
        self.length_scale_u = length_scale_u
        self.length_scale_v = length_scale_v if length_scale_v is not None else 0.7 * length_scale_u
        self.length_scale_w = length_scale_w if length_scale_w is not None else 0.4 * length_scale_u
        
        # Set variances with defaults (anisotropy ratios: v/u=0.8, w/u=0.5)
        self.variance_u = variance_u
        self.variance_v = variance_v if variance_v is not None else 0.64 * variance_u
        self.variance_w = variance_w if variance_w is not None else 0.25 * variance_u
        
        # Set coherence and asymmetry
        self.asymmetry = asymmetry
        self.uv_coherence = uv_coherence
        self.uw_coherence = uw_coherence
        self.vw_coherence = vw_coherence
        
        # Store parameters
        self.params = MannBoxParameters(
            length_scale_u=self.length_scale_u,
            length_scale_v=self.length_scale_v,
            length_scale_w=self.length_scale_w,
            variance_u=self.variance_u,
            variance_v=self.variance_v,
            variance_w=self.variance_w,
            asymmetry=asymmetry,
            uv_coherence=uv_coherence,
            uw_coherence=uw_coherence,
            vw_coherence=vw_coherence
        )
    
    def compute_spectrum_diagonal(self, 
                                   wavenumber: Union[float, np.ndarray],
                                   length_scale: float,
                                   variance: float) -> Union[float, np.ndarray]:
        """
        Compute Mann Box diagonal spectral component S_ii(k).
        
        The Mann Box diagonal spectrum is:
        
            S_ii(k) = (8√(3/(11π)) * σ_i² * L_i) / (k * (1 + (k*L_i/α)²)^(5/6))
        
        Parameters
        ----------
        wavenumber : float or array
            Wavenumber k [1/m]
        length_scale : float
            Integral length scale L_i [m]
        variance : float
            Velocity component variance σ_i² [m²/s²]
        
        Returns
        -------
        float or array
            Spectral density S_ii(k) [m³/s²]
        """
        # Normalization factor: 8√(3/(11π))
        norm_factor = 8.0 * np.sqrt(3.0 / (11.0 * np.pi))
        
        # Guard against zero wavenumber
        wavenumber = np.asarray(wavenumber)
        k_nonzero = np.maximum(wavenumber, 1e-10)
        
        # Normalized wavenumber
        k_normalized = k_nonzero * length_scale / self.asymmetry
        
        # Denominator: k * (1 + (k*L/α)²)^(5/6)
        denominator = k_nonzero * np.power(1.0 + k_normalized**2, 5.0/6.0)
        
        # Spectrum
        spectrum = (norm_factor * variance * length_scale) / denominator
        
        return spectrum
    
    def compute_spectrum_offdiagonal(self,
                                     wavenumber: Union[float, np.ndarray],
                                     S_ii: Union[float, np.ndarray],
                                     S_jj: Union[float, np.ndarray],
                                     length_scale_i: float,
                                     length_scale_j: float,
                                     coherence_factor: float = 0.75) -> Union[float, np.ndarray]:
        """
        Compute Mann Box off-diagonal spectral component S_ij(k).
        
        The off-diagonal components represent the cross-correlation between
        different velocity components and must satisfy the Cauchy-Schwarz inequality:
        |S_ij|² ≤ S_ii * S_jj
        
        Parameters
        ----------
        wavenumber : float or array
            Wavenumber k [1/m]
        S_ii : float or array
            Diagonal spectrum S_ii(k) [m³/s²]
        S_jj : float or array
            Diagonal spectrum S_jj(k) [m³/s²]
        length_scale_i : float
            Integral length scale for component i [m]
        length_scale_j : float
            Integral length scale for component j [m]
        coherence_factor : float
            Coherence factor η_ij (default 0.75)
        
        Returns
        -------
        float or array
            Cross-spectral density S_ij(k) [m³/s²]
        """
        # Geometric mean of spectra
        geom_mean = np.sqrt(np.maximum(S_ii * S_jj, 1e-20))
        
        # Harmonic mean of length scales
        L_harmonic = 2.0 * length_scale_i * length_scale_j / (length_scale_i + length_scale_j)
        
        # Normalized wavenumber with harmonic mean scale
        wavenumber = np.asarray(wavenumber)
        k_normalized = wavenumber * L_harmonic / 300.0
        
        # Exponential coherence decay
        coherence_decay = np.exp(-k_normalized**2)
        
        # Cross-spectrum with Cauchy-Schwarz constraint
        S_ij = coherence_factor * geom_mean * coherence_decay
        
        return S_ij
    
    def compute_spectrum(self,
                        frequencies: Union[float, np.ndarray],
                        height: float = 90.0,
                        mean_wind_speed: float = 12.0) -> Dict[str, np.ndarray]:
        """
        Compute the complete Mann Box spectral tensor at given frequencies.
        
        Parameters
        ----------
        frequencies : float or array
            Frequencies [Hz]
        height : float
            Height above ground [m], default 90 m
        mean_wind_speed : float
            Mean wind speed [m/s], default 12 m/s
        
        Returns
        -------
        dict
            Dictionary with keys:
            - 'frequency': Input frequencies [Hz]
            - 'S_uu': u-component spectrum [m³/s²]
            - 'S_vv': v-component spectrum [m³/s²]
            - 'S_ww': w-component spectrum [m³/s²]
            - 'S_uv': u-v cross-spectrum [m³/s²]
            - 'S_uw': u-w cross-spectrum [m³/s²]
            - 'S_vw': v-w cross-spectrum [m³/s²]
            - 'variance_u': u-component variance [m²/s²]
            - 'variance_v': v-component variance [m²/s²]
            - 'variance_w': w-component variance [m²/s²]
        """
        frequencies = np.atleast_1d(frequencies)
        
        # Convert frequency to wavenumber: k = 2πf/U
        wavenumber = 2.0 * np.pi * frequencies / mean_wind_speed
        
        # Compute diagonal spectra
        S_uu = self.compute_spectrum_diagonal(wavenumber, self.length_scale_u, self.variance_u)
        S_vv = self.compute_spectrum_diagonal(wavenumber, self.length_scale_v, self.variance_v)
        S_ww = self.compute_spectrum_diagonal(wavenumber, self.length_scale_w, self.variance_w)
        
        # Compute off-diagonal spectra
        S_uv = self.compute_spectrum_offdiagonal(wavenumber, S_uu, S_vv,
                                                 self.length_scale_u, self.length_scale_v,
                                                 self.uv_coherence)
        S_uw = self.compute_spectrum_offdiagonal(wavenumber, S_uu, S_ww,
                                                 self.length_scale_u, self.length_scale_w,
                                                 self.uw_coherence)
        S_vw = self.compute_spectrum_offdiagonal(wavenumber, S_vv, S_ww,
                                                 self.length_scale_v, self.length_scale_w,
                                                 self.vw_coherence)
        
        return {
            'frequency': frequencies,
            'S_uu': S_uu,
            'S_vv': S_vv,
            'S_ww': S_ww,
            'S_uv': S_uv,
            'S_uw': S_uw,
            'S_vw': S_vw,
            'variance_u': self.variance_u,
            'variance_v': self.variance_v,
            'variance_w': self.variance_w,
            'length_scale_u': self.length_scale_u,
            'length_scale_v': self.length_scale_v,
            'length_scale_w': self.length_scale_w,
            'asymmetry': self.asymmetry,
            'mean_wind_speed': mean_wind_speed,
            'height': height
        }
    
    def validate_realizability(self, spectrum_dict: Dict) -> bool:
        """
        Verify that the spectral tensor satisfies physical constraints.
        
        Parameters
        ----------
        spectrum_dict : dict
            Spectrum dictionary from compute_spectrum()
        
        Returns
        -------
        bool
            True if all realizability constraints are satisfied
        """
        S_uu = spectrum_dict['S_uu']
        S_vv = spectrum_dict['S_vv']
        S_ww = spectrum_dict['S_ww']
        S_uv = spectrum_dict['S_uv']
        S_uw = spectrum_dict['S_uw']
        S_vw = spectrum_dict['S_vw']
        
        tolerance = 1e-12
        
        # Check 1: Diagonal components non-negative (energy)
        if np.any(S_uu < -tolerance) or np.any(S_vv < -tolerance) or np.any(S_ww < -tolerance):
            warnings.warn("Negative diagonal spectrum component detected")
            return False
        
        # Check 2: Cauchy-Schwarz inequality for cross-spectra
        cs_uv = S_uv**2 - S_uu * S_vv
        cs_uw = S_uw**2 - S_uu * S_ww
        cs_vw = S_vw**2 - S_vv * S_ww
        
        if np.any(cs_uv > tolerance) or np.any(cs_uw > tolerance) or np.any(cs_vw > tolerance):
            violations = (np.sum(cs_uv > tolerance) + np.sum(cs_uw > tolerance) + 
                         np.sum(cs_vw > tolerance))
            warnings.warn(f"Cauchy-Schwarz inequality violated at {violations} wavenumbers")
            return False
        
        return True
    
    def get_parameters(self) -> MannBoxParameters:
        """Return the current Mann Box parameters."""
        return self.params
    
    def update_parameters(self, **kwargs):
        """
        Update Mann Box parameters.
        
        Parameters
        ----------
        **kwargs
            Any of: length_scale_u, length_scale_v, length_scale_w,
                   variance_u, variance_v, variance_w, asymmetry,
                   uv_coherence, uw_coherence, vw_coherence
        """
        for key, value in kwargs.items():
            if hasattr(self, key):
                setattr(self, key, value)
            else:
                raise ValueError(f"Unknown parameter: {key}")
        
        # Update params dataclass
        self.params = MannBoxParameters(
            length_scale_u=self.length_scale_u,
            length_scale_v=self.length_scale_v,
            length_scale_w=self.length_scale_w,
            variance_u=self.variance_u,
            variance_v=self.variance_v,
            variance_w=self.variance_w,
            asymmetry=self.asymmetry,
            uv_coherence=self.uv_coherence,
            uw_coherence=self.uw_coherence,
            vw_coherence=self.vw_coherence
        )


def create_mann_box_preset(preset_name: str) -> MannBox:
    """
    Create a Mann Box model with preset parameters for common scenarios.
    
    Parameters
    ----------
    preset_name : str
        One of: 'neutral', 'stable', 'unstable', 'wind_farm', 'complex_terrain'
    
    Returns
    -------
    MannBox
        Configured Mann Box instance
    
    Examples
    --------
    >>> mann = create_mann_box_preset('wind_farm')
    >>> spectrum = mann.compute_spectrum(frequencies)
    """
    presets = {
        'neutral': {
            'length_scale_u': 300.0,
            'length_scale_v': 210.0,
            'length_scale_w': 120.0,
            'variance_u': 1.0,
            'variance_v': 0.64,
            'variance_w': 0.25,
            'asymmetry': 1.0,
        },
        'stable': {
            'length_scale_u': 200.0,  # Reduced scales in stable conditions
            'length_scale_v': 140.0,
            'length_scale_w': 80.0,
            'variance_u': 0.8,        # Reduced turbulence intensity
            'variance_v': 0.51,
            'variance_w': 0.20,
            'asymmetry': 1.2,         # Increased anisotropy
        },
        'unstable': {
            'length_scale_u': 400.0,  # Increased scales in unstable conditions
            'length_scale_v': 280.0,
            'length_scale_w': 160.0,
            'variance_u': 1.3,        # Increased turbulence intensity
            'variance_v': 0.83,
            'variance_w': 0.33,
            'asymmetry': 0.8,         # Decreased anisotropy
        },
        'wind_farm': {
            'length_scale_u': 250.0,  # Shorter scales for wind farm wakes
            'length_scale_v': 175.0,
            'length_scale_w': 100.0,
            'variance_u': 0.9,
            'variance_v': 0.58,
            'variance_w': 0.225,
            'asymmetry': 1.1,
        },
        'complex_terrain': {
            'length_scale_u': 350.0,  # Larger scales for complex terrain
            'length_scale_v': 245.0,
            'length_scale_w': 140.0,
            'variance_u': 1.2,        # Higher turbulence
            'variance_v': 0.77,
            'variance_w': 0.30,
            'asymmetry': 1.3,         # More anisotropic
        },
    }
    
    if preset_name not in presets:
        raise ValueError(f"Unknown preset: {preset_name}. "
                        f"Available presets: {list(presets.keys())}")
    
    params = presets[preset_name]
    return MannBox(**params)
