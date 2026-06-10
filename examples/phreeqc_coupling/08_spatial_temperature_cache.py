#!/usr/bin/env python3
"""
08_spatial_temperature_cache.py - Scenario Library Caching & Temperature Fields

See the main module: phreeqc_coupling.scenario_library
See documentation: ../../docs/phreeqc_coupling/user_guide.md (Capability #8)

Demonstrates fast runtime lookups (<30 ms) using pre-computed scenario library
instead of full wind solves (10+ minutes).

Quick start:
    from phreeqc_coupling.scenario_library import ScenarioLibrary
    lib = ScenarioLibrary.load('scenario_library/library.h5')
    scenario = lib.nearest_scenario(u_mag=8.5, wind_dir=270, T=288.15)
"""

print(__doc__)
