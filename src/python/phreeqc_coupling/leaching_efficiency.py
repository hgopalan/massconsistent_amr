#!/usr/bin/env python3
"""
leaching_efficiency.py - Dispersion-Enhanced Leaching via Sherwood Correlation

Quantifies wind-driven turbulent enhancement of ore leaching efficiency.
Wind-driven turbulence improves mass transfer at particle surfaces, which
accelerates dissolution kinetics and increases leaching rates.

Implements Sherwood number correlation (Ranz & Marshall 1952) to compute
mass transfer coefficients from wind speed and particle size.

Leaching efficiency = h_MT / h_MT_reference, where h_MT is mass transfer coefficient.

References:
    - Ranz & Marshall (1952). Evaporation from drops (mass transfer correlation)
    - Sherwood (1954). Mass transfer between phases
    - Bird, Stewart, Lightfoot (2007). Transport phenomena
"""

import numpy as np
from pathlib import Path
from typing import Dict, Optional, Tuple, List
from dataclasses import dataclass
import csv
import logging

logger = logging.getLogger(__name__)


@dataclass
class SherwoodNumberLookup:
    """Sherwood number lookup table for mass transfer.
    
    Attributes:
        u_speeds (np.ndarray): Wind speed array [m/s]
        particle_sizes (np.ndarray): Particle size array [μm]
        sherwood_table (np.ndarray): Sherwood numbers [nu_speeds, n_particle_sizes]
        h_MT_table (np.ndarray): Mass transfer coefficients [m/s]
    """
    u_speeds: np.ndarray
    particle_sizes: np.ndarray
    sherwood_table: np.ndarray
    h_MT_table: np.ndarray
    
    def lookup_sherwood(self, u_speed: float, particle_size: float) -> float:
        """Lookup Sherwood number for given conditions.
        
        Args:
            u_speed: Wind speed (m/s)
            particle_size: Particle size (μm)
        
        Returns:
            Sherwood number (dimensionless)
        """
        return self._interpolate(u_speed, particle_size, self.sherwood_table)
    
    def lookup_mass_transfer_coefficient(self, u_speed: float, particle_size: float) -> float:
        """Lookup mass transfer coefficient.
        
        Args:
            u_speed: Wind speed (m/s)
            particle_size: Particle size (μm)
        
        Returns:
            Mass transfer coefficient [m/s]
        """
        return self._interpolate(u_speed, particle_size, self.h_MT_table)
    
    def _interpolate(self, u_speed: float, particle_size: float, table: np.ndarray) -> float:
        """Bilinear interpolation in lookup table.
        
        Args:
            u_speed: Wind speed (m/s)
            particle_size: Particle size (μm)
            table: 2D lookup table
        
        Returns:
            Interpolated value
        """
        # Clamp to table bounds
        u_clamped = np.clip(u_speed, self.u_speeds[0], self.u_speeds[-1])
        d_clamped = np.clip(particle_size, self.particle_sizes[0], self.particle_sizes[-1])
        
        # Find indices
        u_idx = np.searchsorted(self.u_speeds, u_clamped)
        d_idx = np.searchsorted(self.particle_sizes, d_clamped)
        
        if u_idx == 0:
            u_idx = 1
        if d_idx == 0:
            d_idx = 1
        
        u0, u1 = self.u_speeds[u_idx - 1], self.u_speeds[u_idx]
        d0, d1 = self.particle_sizes[d_idx - 1], self.particle_sizes[d_idx]
        
        # Get corner values
        f00 = table[u_idx - 1, d_idx - 1]
        f10 = table[u_idx, d_idx - 1]
        f01 = table[u_idx - 1, d_idx]
        f11 = table[u_idx, d_idx]
        
        # Interpolation weights
        wu = (u_clamped - u0) / (u1 - u0 + 1e-12)
        wd = (d_clamped - d0) / (d1 - d0 + 1e-12)
        
        # Bilinear interpolation
        f0 = f00 * (1 - wu) + f10 * wu
        f1 = f01 * (1 - wu) + f11 * wu
        f = f0 * (1 - wd) + f1 * wd
        
        return float(f)


def _compute_sherwood_number(
    u_speed: float,
    particle_diameter: float = 500e-6,
    domain_type: str = 'heap'
) -> float:
    """Compute Sherwood number from wind speed and particle size.
    
    Sherwood number: dimensionless mass transfer rate
    Sh = h_MT × L / D_AB
    where h_MT is mass transfer coefficient, L is characteristic length (diameter),
    D_AB is diffusion coefficient.
    
    Correlation (Ranz & Marshall 1952 for sphere):
    Sh = 2 + (0.6 × Re^0.5 × Sc^(1/3))
    
    For heaps/packed beds, similar correlations apply.
    
    Args:
        u_speed: Wind speed (m/s)
        particle_diameter: Particle diameter (m, default 500 μm for heap ore)
        domain_type: 'heap', 'sphere', 'cylinder' (affects constant K)
    
    Returns:
        Sherwood number (dimensionless)
    
    References:
        - Ranz & Marshall (1952)
        - Sherwood (1954)
    """
    # Physical properties (water at 20°C)
    rho_air = 1.225  # kg/m³
    mu_air = 1.81e-5  # Pa·s
    nu_air = 1.5e-5  # m²/s (kinematic viscosity)
    D_AB = 1e-9  # m²/s (molecular diffusion, typical)
    
    # Reynolds number: Re = ρ × u × D / μ
    Re = (rho_air * u_speed * particle_diameter) / mu_air
    Re = max(Re, 0.1)  # Avoid Re < 0.1
    
    # Schmidt number: Sc = ν / D_AB
    Sc = nu_air / D_AB
    
    # Sherwood correlation (Ranz & Marshall for isolated sphere)
    # Sh = 2 + (0.6 × Re^0.5 × Sc^(1/3))
    # The "2" is the stagnant mass transfer (no relative motion)
    # The second term is enhancement due to flow
    Sh = 2.0 + 0.6 * (Re**0.5) * (Sc**(1.0/3.0))
    
    return float(Sh)


def _compute_mass_transfer_coefficient(
    u_speed: float,
    particle_diameter: float = 500e-6,
    domain_type: str = 'heap'
) -> float:
    """Compute mass transfer coefficient from Sherwood number.
    
    h_MT = Sh × D_AB / L
    where L is characteristic length (particle diameter).
    
    Args:
        u_speed: Wind speed (m/s)
        particle_diameter: Particle diameter (m)
        domain_type: Type of domain (affects constants)
    
    Returns:
        Mass transfer coefficient [m/s]
    """
    D_AB = 1e-9  # m²/s (diffusion coefficient)
    
    Sh = _compute_sherwood_number(u_speed, particle_diameter, domain_type)
    h_MT = (Sh * D_AB) / particle_diameter
    
    return float(h_MT)


def compute_leaching_efficiency(
    u_speed: float,
    particle_size: float = 500e-6,
    domain_type: str = 'heap',
    lookup: Optional[SherwoodNumberLookup] = None
) -> float:
    """Compute relative leaching efficiency from wind speed.
    
    Efficiency = h_MT(u) / h_MT_ref(u_ref) where u_ref = 1 m/s.
    
    Higher wind speeds → higher Sherwood numbers → faster mass transfer
    → increased leaching rates.
    
    For typical heap leaching, efficiency ranges from ~0.1 (low wind) to
    ~10+ (high wind) relative to reference.
    
    Args:
        u_speed: Wind speed (m/s)
        particle_size: Particle diameter (m, default 500 μm for heap ore)
        domain_type: Domain type: 'heap', 'sphere', 'cylinder'
        lookup: Pre-computed lookup table (optional)
    
    Returns:
        Relative leaching efficiency [0, 1+]
        < 1: below reference wind speed
        = 1: at reference wind speed (1 m/s)
        > 1: above reference wind speed
    
    Example:
        >>> # Low wind: 20% of reference leaching efficiency
        >>> eff = compute_leaching_efficiency(u_speed=0.5)
        >>> print(f"Leaching efficiency: {eff:.1%}")
        
        >>> # High wind: 400% of reference leaching efficiency
        >>> eff = compute_leaching_efficiency(u_speed=5.0)
        >>> print(f"Leaching efficiency: {eff:.0%}")
    """
    if lookup is not None:
        h_MT = lookup.lookup_mass_transfer_coefficient(u_speed, particle_size * 1e6)
        h_MT_ref = lookup.lookup_mass_transfer_coefficient(1.0, particle_size * 1e6)
    else:
        h_MT = _compute_mass_transfer_coefficient(u_speed, particle_size, domain_type)
        h_MT_ref = _compute_mass_transfer_coefficient(1.0, particle_size, domain_type)
    
    efficiency = h_MT / max(h_MT_ref, 1e-12)
    
    return float(efficiency)


def build_sherwood_lookup(
    u_speeds: Optional[np.ndarray] = None,
    particle_sizes: Optional[np.ndarray] = None,
    domain_type: str = 'heap'
) -> SherwoodNumberLookup:
    """Build Sherwood number lookup table.
    
    Args:
        u_speeds: Wind speed array [m/s] (default: 0.5-25 m/s)
        particle_sizes: Particle size array [μm] (default: 100-1000 μm for heap ore)
        domain_type: Domain type for correlation selection
    
    Returns:
        SherwoodNumberLookup table
    """
    if u_speeds is None:
        u_speeds = np.logspace(-0.3, 1.4, 30)  # 0.5 - 25 m/s
    
    if particle_sizes is None:
        # Typical heap ore sizes: 100-1000 μm
        particle_sizes = np.logspace(2, 3, 15)  # 100 - 1000 μm
    
    # Compute lookup tables
    nu = len(u_speeds)
    nd = len(particle_sizes)
    sherwood_table = np.zeros((nu, nd))
    h_MT_table = np.zeros((nu, nd))
    
    for i, u in enumerate(u_speeds):
        for j, d_um in enumerate(particle_sizes):
            d_m = d_um * 1e-6  # Convert μm to m
            sherwood_table[i, j] = _compute_sherwood_number(u, d_m, domain_type)
            h_MT_table[i, j] = _compute_mass_transfer_coefficient(u, d_m, domain_type)
    
    logger.info(f"Built Sherwood lookup: {nu} wind speeds × {nd} particle sizes")
    
    return SherwoodNumberLookup(
        u_speeds=u_speeds,
        particle_sizes=particle_sizes,
        sherwood_table=sherwood_table,
        h_MT_table=h_MT_table
    )


def save_sherwood_lookup_to_csv(
    lookup: SherwoodNumberLookup,
    output_file: str,
    output_type: str = 'sherwood'
) -> None:
    """Save Sherwood or mass transfer lookup table to CSV.
    
    Args:
        lookup: SherwoodNumberLookup object
        output_file: Output CSV file path
        output_type: 'sherwood' or 'mass_transfer'
    """
    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    table = lookup.sherwood_table if output_type == 'sherwood' else lookup.h_MT_table
    units = '(dimensionless)' if output_type == 'sherwood' else '[m/s]'
    
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        
        # Header
        header = [f'wind_speed_m/s'] + [f'particle_{d:.0f}um' for d in lookup.particle_sizes]
        writer.writerow(header)
        
        # Comment row with units
        comment_row = [f'# {output_type.capitalize()} {units}']
        writer.writerow(comment_row)
        
        # Data rows
        for i, u in enumerate(lookup.u_speeds):
            row = [f'{u:.3f}'] + [f'{table[i, j]:.6f}' for j in range(len(lookup.particle_sizes))]
            writer.writerow(row)
    
    logger.info(f"Saved {output_type} lookup to {output_file}")


def load_sherwood_lookup_from_csv(
    input_file: str
) -> SherwoodNumberLookup:
    """Load Sherwood lookup table from CSV file.
    
    Args:
        input_file: Input CSV file path
    
    Returns:
        SherwoodNumberLookup object
    """
    with open(input_file, 'r') as f:
        reader = csv.reader(f)
        header = next(reader)
        
        # Skip comment rows
        while header[0].startswith('#'):
            header = next(reader)
        
        # Parse header to extract particle sizes
        particle_sizes = []
        for col in header[1:]:
            if 'particle' in col:
                size_str = col.split('_')[1].replace('um', '')
                particle_sizes.append(float(size_str))
        
        # Parse data
        u_speeds = []
        sherwood_data = []
        
        for row in reader:
            if row and not row[0].startswith('#'):
                u_speeds.append(float(row[0]))
                sherwood_data.append([float(x) for x in row[1:]])
    
    lookup = SherwoodNumberLookup(
        u_speeds=np.array(u_speeds),
        particle_sizes=np.array(particle_sizes),
        sherwood_table=np.array(sherwood_data),
        h_MT_table=np.array(sherwood_data)  # Placeholder, compute if needed
    )
    
    # Recompute h_MT table from Sherwood numbers
    for i in range(len(u_speeds)):
        for j in range(len(particle_sizes)):
            d_m = particle_sizes[j] * 1e-6
            D_AB = 1e-9
            lookup.h_MT_table[i, j] = (lookup.sherwood_table[i, j] * D_AB) / d_m
    
    return lookup


def compute_leaching_rate_enhancement(
    u_speed: float,
    dissolution_rate_ref: float,
    particle_size: float = 500e-6
) -> Tuple[float, float]:
    """Compute enhanced dissolution rate from wind-driven mass transfer.
    
    Dissolution rate proportional to mass transfer coefficient (surface kinetics).
    
    Enhanced rate = dissolution_rate_ref × (Sh(u) / Sh(u_ref))
    where Sh(u) = h_MT(u) × D / D_AB
    
    Args:
        u_speed: Wind speed (m/s)
        dissolution_rate_ref: Baseline dissolution rate at 1 m/s wind [mol/(m²·s)]
        particle_size: Particle diameter (m, default 500 μm)
    
    Returns:
        Tuple of:
        - efficiency_factor: Multiplication factor vs reference
        - enhanced_rate: Enhanced dissolution rate [mol/(m²·s)]
    
    Example:
        >>> rate_ref = 1e-6  # mol/(m²·s) at 1 m/s wind
        >>> factor, rate_enhanced = compute_leaching_rate_enhancement(u_speed=10.0, dissolution_rate_ref=rate_ref)
        >>> print(f"Wind enhancement factor: {factor:.1f}×")
        >>> print(f"Enhanced rate: {rate_enhanced:.2e} mol/(m²·s)")
    """
    efficiency = compute_leaching_efficiency(u_speed, particle_size)
    enhanced_rate = dissolution_rate_ref * efficiency
    
    return efficiency, enhanced_rate


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    
    # Build and save lookup tables
    logger.info("Building Sherwood number lookup table...")
    lookup = build_sherwood_lookup()
    
    output_dir = Path(__file__).parent.parent.parent / 'lookup_tables'
    output_dir.mkdir(parents=True, exist_ok=True)
    
    sherwood_file = output_dir / 'sherwood_number.csv'
    save_sherwood_lookup_to_csv(lookup, str(sherwood_file), output_type='sherwood')
    
    # Test examples
    print("\nLeaching efficiency examples (particle size = 500 μm):")
    print("-" * 60)
    
    test_speeds = [0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 20.0]
    print(f"{'Wind Speed [m/s]':<20} {'Efficiency':<15} {'Rate Enhancement':<15}")
    print("-" * 60)
    
    rate_ref = 1e-6  # mol/(m²·s)
    
    for u in test_speeds:
        eff = compute_leaching_efficiency(u, particle_size=500e-6)
        factor, rate = compute_leaching_rate_enhancement(u, rate_ref, particle_size=500e-6)
        print(f"{u:<20.1f} {eff:<15.2f} {factor:<15.2f}×")
    
    print(f"\nLookup table saved to: {sherwood_file}")
