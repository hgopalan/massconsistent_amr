#!/usr/bin/env python3
"""
Generate Gaussian hill terrain for Case 1 test.

Creates a medium-sized Gaussian hill (21x21 grid over 500x500 m domain)
for testing mass-consistent wind solver with time-varying winds and turbulence.
"""

import sys
import os

# Add parent directory to path to import tools
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../tools'))

from gaussian_hill_generator import GaussianHillGenerator

# Generate medium Gaussian hill (21x21 grid)
# Domain: 500 x 500 m
# Peak: 75 m at center
# Sigma: 100 m (moderate slope)
gen = GaussianHillGenerator(
    nx=21,
    ny=21,
    domain_x=500.0,
    domain_y=500.0,
    peak_height=75.0,
    sigma=100.0
)

gen.generate()
gen.print_stats()

# Write terrain to CSV
if gen.write_terrain_csv("terrain.csv"):
    print("✓ Terrain written to terrain.csv", file=sys.stderr)
    sys.exit(0)
else:
    print("✗ Failed to write terrain", file=sys.stderr)
    sys.exit(1)
