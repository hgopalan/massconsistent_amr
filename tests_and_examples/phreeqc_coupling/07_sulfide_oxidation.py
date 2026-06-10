#!/usr/bin/env python3
"""
07_sulfide_oxidation.py - Sulfide Oxidation Kinetics

See the main module: phreeqc_coupling.sulfide_oxidation
See detailed example: ../src/python/phreeqc_coupling/03_sulfide_oxidation.py
See documentation: ../../docs/phreeqc_coupling/user_guide.md (Capability #7)

This example computes wind-dependent sulfide oxidation rates with temperature
corrections and acid generation prediction.

Quick start:
    from phreeqc_coupling.sulfide_oxidation import compute_sulfide_oxidation_rates
    results = compute_sulfide_oxidation_rates(wind, 'sulfide_sites.csv', temperature=288.15)
"""

print(__doc__)
