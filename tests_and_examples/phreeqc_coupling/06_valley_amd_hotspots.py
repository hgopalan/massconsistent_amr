#!/usr/bin/env python3
"""
06_valley_amd_hotspots.py - AMD Hotspot Detection in Mountain Valleys

See the main module: phreeqc_coupling.amd_hotspot_detector
See detailed example: ../src/python/phreeqc_coupling/02_valley_amd_hotspots.py
See documentation: ../../docs/phreeqc_coupling/user_guide.md (Capability #6)

This example identifies and classifies acid mine drainage discharge points
by oxidation risk using Sherwood mass transfer correlations.

Quick start:
    from phreeqc_coupling.amd_hotspot_detector import identify_valley_amd_hotspots
    results = identify_valley_amd_hotspots(wind, 'amd_sites.csv', output_dir='output/')
"""

print(__doc__)
