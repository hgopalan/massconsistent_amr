#!/usr/bin/env python3
"""
dust_suppression.py - Wind-Dependent Dust Suppression

See the main module: phreeqc_coupling.dust_suppression_lookup
See documentation: ../../docs/phreeqc_coupling/user_guide.md (Capability #10)

Demonstrates wind-dependent dust settling effects on pH evolution in leaching
solutions. High wind → dust suspension → less acidification.

Quick start:
    from phreeqc_coupling.dust_suppression_lookup import compute_dust_suppression_factor
    factor = compute_dust_suppression_factor(u_speed=5.0, particle_size=10.0)
"""

print(__doc__)
