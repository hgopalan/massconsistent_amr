#!/usr/bin/env python3
"""
04_kv_dispersivity.py - Vertical Diffusivity and Dispersivity Export

Demonstrates extraction of turbulent vertical diffusivity (K_v) for use in
PHREEQC dispersivity parameterization. K_v controls mixing, dispersion, and
reaction rate enhancement in reactive transport simulations.

Key Physics:
  - K_v = u* × z × f(ζ) where ζ = z/Monin-Obukhov length
  - Dispersivity: α = K_v / |velocity|
  - Stability dependence: α ~2-10× variation stable vs. unstable

References:
    - Businger et al. (1971). Monin-Obukhov surface layer theory.
    - Gelhar et al. (1992). Field-scale dispersion in aquifers.
"""

import sys
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).parent.parent))

from phreeqc_coupling import FieldExtractor

def main():
    print("\n" + "="*70)
    print("VERTICAL DIFFUSIVITY & DISPERSIVITY - EXAMPLE 04")
    print("="*70)
    
    # Extract K_v profile
    print("\nExtracting vertical diffusivity profile...")
    z_agl = np.logspace(-1, 3, 15)  # 0.1 to 1000 m
    
    # Synthetic K_v profile (typical atmosphere)
    K_v = 0.01 * z_agl**1.3 * (1 + 0.5 * np.sin(z_agl / 100))
    K_v = np.minimum(K_v, 1.0)  # Cap at 1 m²/s
    
    print(f"\nK_v Profile (Vertical Diffusivity):")
    print(f"  {'Height (m)':>12} {'K_v (m²/s)':>15} {'α (dispersivity)':>20}")
    print(f"  {'-'*47}")
    
    # Typical wind speed for alpha calculation
    u_mag = 5.0  # m/s
    
    for z, kv in zip(z_agl, K_v):
        alpha = kv / u_mag if u_mag > 0 else 0
        print(f"  {z:12.1f} {kv:15.4f} {alpha:20.6f}")
    
    # Export
    output_dir = Path("./04_dispersivity_output")
    output_dir.mkdir(exist_ok=True)
    
    with open(output_dir / "dispersivity.txt", 'w') as f:
        f.write("# Dispersivity for PHREEQC (α = K_v / |u|)\n")
        f.write("# Height (m)  K_v (m²/s)  Dispersivity (m)\n")
        for z, kv in zip(z_agl, K_v):
            alpha = kv / u_mag
            f.write(f"{z:10.2f}  {kv:12.4f}  {alpha:15.6f}\n")
    
    print(f"\n✓ Exported to: dispersivity.txt")
    print(f"\nKey findings:")
    print(f"  - K_v range: {K_v.min():.2e} to {K_v.max():.2e} m²/s")
    print(f"  - Dispersivity range: {K_v.min()/u_mag:.2e} to {K_v.max()/u_mag:.2e} m")
    print("="*70 + "\n")

if __name__ == "__main__":
    main()
