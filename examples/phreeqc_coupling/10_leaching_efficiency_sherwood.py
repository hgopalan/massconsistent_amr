#!/usr/bin/env python3
"""
10_leaching_efficiency_sherwood.py - Leaching Efficiency via Sherwood Correlation

See the main module: phreeqc_coupling.leaching_efficiency
See documentation: ../../docs/phreeqc_coupling/user_guide.md (Capability #11)

Demonstrates wind-driven mass transfer enhancement of ore leaching efficiency
via Sherwood number correlation: Sh = 2 + 0.6 × Re^0.5 × Sc^0.33

Quick start:
    from phreeqc_coupling.leaching_efficiency import compute_leaching_efficiency
    efficiency = compute_leaching_efficiency(u_speed=6.0, particle_size=500.0)
"""

print(__doc__)
