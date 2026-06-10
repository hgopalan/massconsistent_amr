#!/usr/bin/env python3
"""
dust_suppression_lookup.py - Wind-Speed-Dependent Dust Suppression Factors

Computes dust suppression effects on leaching chemistry. High wind zones
suppress dust (keep it in suspension) → different pH in leaching solution.
Low wind zones → dust settles → acidifying effects from dust chemistry.

Provides lookup tables and interpolation functions for dust suppression
factor as a function of wind speed and particle size.

References:
    - Dust transport models: Gillies et al. (2005), Australian dust storms
    - Particle settling: Stokes Law, terminal velocity
    - Atmospheric suspension: Kok et al. (2014), dust emission mechanisms
"""

import numpy as np
from pathlib import Path
from typing import Dict, Tuple, List, Optional
from dataclasses import dataclass
import csv
import logging

logger = logging.getLogger(__name__)


@dataclass
class DustSuppressionLookup:
    """Dust suppression lookup table and parameters.
    
    Attributes:
        u_speeds (np.ndarray): Wind speed array [m/s]
        particle_sizes (np.ndarray): Particle size array [μm]
        suppression_table (np.ndarray): Suppression factors [nu_speeds, n_particle_sizes]
        units: Description of units
    """
    u_speeds: np.ndarray
    particle_sizes: np.ndarray
    suppression_table: np.ndarray
    
    def lookup(self, u_speed: float, particle_size: float) -> float:
        """Interpolate suppression factor for given wind speed and particle size.
        
        Args:
            u_speed: Wind speed (m/s)
            particle_size: Particle size (μm)
        
        Returns:
            Dust suppression factor [0-1]
        """
        # Clamp to table bounds
        u_clamped = np.clip(u_speed, self.u_speeds[0], self.u_speeds[-1])
        d_clamped = np.clip(particle_size, self.particle_sizes[0], self.particle_sizes[-1])
        
        # Find indices for interpolation
        u_idx = np.searchsorted(self.u_speeds, u_clamped)
        d_idx = np.searchsorted(self.particle_sizes, d_clamped)
        
        # Bilinear interpolation
        if u_idx == 0:
            u_idx = 1
        if d_idx == 0:
            d_idx = 1
        
        u0 = self.u_speeds[u_idx - 1]
        u1 = self.u_speeds[u_idx]
        d0 = self.particle_sizes[d_idx - 1]
        d1 = self.particle_sizes[d_idx]
        
        # Get corner values
        f00 = self.suppression_table[u_idx - 1, d_idx - 1]
        f10 = self.suppression_table[u_idx, d_idx - 1]
        f01 = self.suppression_table[u_idx - 1, d_idx]
        f11 = self.suppression_table[u_idx, d_idx]
        
        # Interpolation weights
        wu = (u_clamped - u0) / (u1 - u0 + 1e-12)
        wd = (d_clamped - d0) / (d1 - d0 + 1e-12)
        
        # Bilinear interpolation
        f0 = f00 * (1 - wu) + f10 * wu
        f1 = f01 * (1 - wu) + f11 * wu
        f = f0 * (1 - wd) + f1 * wd
        
        return float(np.clip(f, 0.0, 1.0))


def _dust_suppression_model(
    u_speed: float,
    particle_size: float = 10e-6,
    particle_density: float = 2650
) -> float:
    """Compute dust suppression factor from first principles.
    
    Model: Dust stays in suspension when turbulent mixing exceeds settling velocity.
    
    Suppression factor f(u, d) represents fraction of dust in suspension (not settled).
    - f = 0: all dust settles (low wind)
    - f = 1: all dust remains in suspension (high wind)
    
    Physics:
    1. Terminal settling velocity (Stokes Law): v_s = ρ_p × g × d² / (18 × μ)
    2. Turbulent velocity scale: u_turb ~ u_star ~ 0.05 × u (rough estimate)
    3. Suppression criterion: f = v_turb / (v_turb + v_s)
       or equivalently: f = 1 / (1 + v_s / u_turb)
    
    Simplified model with empirical calibration:
    f(u, d) = 1 - exp(-k(d) × u) where k(d) is particle-size dependent
    
    Args:
        u_speed: Wind speed at reference height (m/s)
        particle_size: Particle diameter (m, default 10 μm)
        particle_density: Particle density (kg/m³, default quartz 2650)
    
    Returns:
        Dust suppression factor [0-1]
    """
    # Physical parameters
    g = 9.81  # gravitational acceleration (m/s²)
    rho_air = 1.225  # air density at sea level (kg/m³)
    mu = 1.81e-5  # dynamic viscosity of air (Pa·s)
    
    # Terminal settling velocity (Stokes law)
    # v_s = ρ_p × g × d² / (18 × μ)
    v_settling = (particle_density * g * particle_size**2) / (18 * mu)
    
    # Friction velocity approximation: u_star ≈ 0.05 × u
    # Turbulent velocity ~ u_star
    u_star = max(0.03 * u_speed, 0.01)  # Minimum u_star for numerical stability
    
    # Suppression based on balance: f = 1 - v_settling / (v_settling + u_turb)
    # Simplified as: f = 1 - exp(-k × u_star) where k depends on particle size
    
    # Empirical constant k (calibrated from dust transport observations)
    # Larger particles settle faster (higher k → lower suppression)
    # k ~ 1 / d for rough scaling
    k_base = 0.2  # empirical constant
    k = k_base / max(particle_size * 1e6, 1.0)  # scale by particle size (μm)
    
    suppression = 1.0 - np.exp(-k * u_star)
    
    return float(np.clip(suppression, 0.0, 1.0))


def build_dust_suppression_lookup(
    u_speeds: Optional[np.ndarray] = None,
    particle_sizes: Optional[np.ndarray] = None
) -> DustSuppressionLookup:
    """Build dust suppression lookup table.
    
    Args:
        u_speeds: Wind speed array [m/s] (default: 0.5-25 m/s, 50 points)
        particle_sizes: Particle size array [μm] (default: 0.1-1000 μm, 20 points)
    
    Returns:
        DustSuppressionLookup table
    """
    if u_speeds is None:
        u_speeds = np.logspace(-0.3, 1.4, 50)  # 0.5 - 25 m/s
    
    if particle_sizes is None:
        # Common dust sizes: clay (0.1-2 μm), silt (2-63 μm), sand (63-2000 μm)
        particle_sizes = np.logspace(-1, 3, 20)  # 0.1 - 1000 μm
    
    # Compute lookup table
    nu = len(u_speeds)
    nd = len(particle_sizes)
    suppression_table = np.zeros((nu, nd))
    
    for i, u in enumerate(u_speeds):
        for j, d_um in enumerate(particle_sizes):
            d_m = d_um * 1e-6  # Convert μm to m
            suppression_table[i, j] = _dust_suppression_model(u, d_m)
    
    logger.info(f"Built dust suppression lookup: {nu} wind speeds × {nd} particle sizes")
    
    return DustSuppressionLookup(
        u_speeds=u_speeds,
        particle_sizes=particle_sizes,
        suppression_table=suppression_table
    )


def compute_dust_suppression_factor(
    u_speed: float,
    particle_size: float = 10e-6,
    lookup: Optional[DustSuppressionLookup] = None,
    use_model: bool = False
) -> float:
    """Compute dust suppression factor for given conditions.
    
    High wind speeds → more dust in suspension → less pH acidification from dust
    Low wind speeds → dust settles → more pH acidification
    
    Args:
        u_speed: Wind speed (m/s)
        particle_size: Particle size (m, default 10 μm)
        lookup: Pre-computed lookup table (optional, use model if None)
        use_model: Force use of model instead of lookup (default False)
    
    Returns:
        Dust suppression factor ∈ [0, 1]
        - 0: all dust settles (low wind)
        - 1: all dust in suspension (high wind)
    
    Example:
        >>> # Low wind: dust settles, pH effects
        >>> f_dust = compute_dust_suppression_factor(u_speed=2.0)
        >>> print(f"Dust suppression: {f_dust:.2%}")  # Low value
        
        >>> # High wind: dust suppressed, minimal pH effects
        >>> f_dust = compute_dust_suppression_factor(u_speed=15.0)
        >>> print(f"Dust suppression: {f_dust:.2%}")  # High value
    """
    if lookup is not None and not use_model:
        # Convert particle_size from m to μm if needed
        if particle_size < 1e-3:
            particle_size_um = particle_size * 1e6
        else:
            particle_size_um = particle_size
        
        return lookup.lookup(u_speed, particle_size_um)
    else:
        return _dust_suppression_model(u_speed, particle_size)


def save_dust_suppression_lookup_to_csv(
    lookup: DustSuppressionLookup,
    output_file: str
) -> None:
    """Save dust suppression lookup table to CSV file.
    
    Args:
        lookup: DustSuppressionLookup object
        output_file: Output CSV file path
    """
    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        
        # Header
        header = ['wind_speed_m/s'] + [f'particle_{d:.1f}um' for d in lookup.particle_sizes]
        writer.writerow(header)
        
        # Data rows
        for i, u in enumerate(lookup.u_speeds):
            row = [f'{u:.2f}'] + [f'{lookup.suppression_table[i, j]:.4f}' 
                                   for j in range(len(lookup.particle_sizes))]
            writer.writerow(row)
    
    logger.info(f"Saved dust suppression lookup to {output_file}")


def load_dust_suppression_lookup_from_csv(
    input_file: str
) -> DustSuppressionLookup:
    """Load dust suppression lookup table from CSV file.
    
    Args:
        input_file: Input CSV file path
    
    Returns:
        DustSuppressionLookup object
    """
    with open(input_file, 'r') as f:
        reader = csv.reader(f)
        header = next(reader)
        
        # Parse header to extract particle sizes
        particle_sizes = []
        for col in header[1:]:
            if 'particle' in col:
                # Extract size from "particle_10.0um" format
                size_str = col.split('_')[1].replace('um', '')
                particle_sizes.append(float(size_str))
        
        # Parse data
        u_speeds = []
        suppression_data = []
        
        for row in reader:
            u_speeds.append(float(row[0]))
            suppression_data.append([float(x) for x in row[1:]])
    
    return DustSuppressionLookup(
        u_speeds=np.array(u_speeds),
        particle_sizes=np.array(particle_sizes),
        suppression_table=np.array(suppression_data)
    )


def compute_dust_suppression_effect_on_ph(
    u_speed: float,
    particle_size: float = 10e-6,
    reference_pH: float = 8.0,
    pH_acidification_per_dust: float = 0.5
) -> Tuple[float, float]:
    """Compute effect of dust suppression on leaching solution pH.
    
    High wind (suppression=1) → minimal dust settling → pH near reference
    Low wind (suppression=0) → dust settles → acidification from dust chemistry
    
    Dust composition typically includes Fe-bearing minerals that release H⁺:
    - Fe³⁺ hydrolysis: Fe³⁺ + 3H₂O ⇌ Fe(OH)₃ + 3H⁺
    - Sulfide oxidation: 2FeS₂ + 7O₂ + 2H₂O → 2Fe³⁺ + 4SO₄²⁻ + 4H⁺
    
    Model: pH change = -pH_acidification_per_dust × (1 - suppression_factor)
    i.e., at suppression=1 (all suspended): pH stays at reference
         at suppression=0 (all settled): pH drops by acidification amount
    
    Args:
        u_speed: Wind speed (m/s)
        particle_size: Particle size (m)
        reference_pH: pH in absence of settled dust (default 8.0)
        pH_acidification_per_dust: pH drop per unit dust settling (default 0.5 pH units)
    
    Returns:
        Tuple of:
        - suppression_factor: Dust suppression [0-1]
        - pH_adjusted: pH after dust suppression effect
    
    Example:
        >>> # Low wind: dust settles, pH drops
        >>> f_supp, pH = compute_dust_suppression_effect_on_ph(u_speed=2.0, reference_pH=8.0)
        >>> print(f"Suppression: {f_supp:.1%}, pH: {pH:.1f}")
        
        >>> # High wind: dust in suspension, pH unchanged
        >>> f_supp, pH = compute_dust_suppression_effect_on_ph(u_speed=15.0, reference_pH=8.0)
        >>> print(f"Suppression: {f_supp:.1%}, pH: {pH:.1f}")
    """
    suppression = compute_dust_suppression_factor(u_speed, particle_size)
    
    # pH change proportional to settled dust fraction: (1 - suppression)
    pH_change = -pH_acidification_per_dust * (1.0 - suppression)
    pH_adjusted = reference_pH + pH_change
    
    return suppression, pH_adjusted


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    
    # Build and save lookup table
    logger.info("Building dust suppression lookup table...")
    lookup = build_dust_suppression_lookup()
    
    output_file = Path(__file__).parent.parent.parent / 'lookup_tables' / 'dust_suppression.csv'
    output_file.parent.mkdir(parents=True, exist_ok=True)
    save_dust_suppression_lookup_to_csv(lookup, str(output_file))
    
    # Test examples
    print("\nDust suppression examples:")
    print("-" * 50)
    
    test_speeds = [1.0, 5.0, 10.0, 15.0, 20.0]
    test_size = 10.0  # μm
    
    for u in test_speeds:
        f_dust = compute_dust_suppression_factor(u, particle_size=test_size*1e-6, lookup=lookup)
        f_supp_ph, pH = compute_dust_suppression_effect_on_ph(u)
        print(f"u={u:5.1f} m/s: suppression={f_dust:5.1%}, "
              f"pH effect: {f_supp_ph:5.1%} suppression → pH={pH:.1f}")
    
    print(f"\nLookup table saved to: {output_file}")
