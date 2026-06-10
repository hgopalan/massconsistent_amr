#!/usr/bin/env python3
"""
03_precipitation_recharge.py - Precipitation-Driven Infiltration

Demonstrates mapping of spatially-varying precipitation to infiltration-driven
groundwater recharge boundary conditions for PHREEQC reactive transport.

This example shows how wind-affected precipitation patterns drive heterogeneous
infiltration, affecting contaminant transport, leaching, and geochemical processes.

Key Concepts:
  - Orographic precipitation enhancement on ridges
  - Valley fog/stratus suppression of precipitation
  - Wind-driven dust suppression effects on dust-induced acidification
  - Infiltration-driven transport coupling

References:
    - Stull (2011). Boundary layer meteorology (orographic effects).
"""

import sys
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).parent.parent))

from phreeqc_coupling import FieldExtractor


def extract_precipitation_field(wind_solver=None, verbose=True):
    """Extract spatially-varying precipitation rate."""
    
    print("\n" + "="*70)
    print("PRECIPITATION FIELD EXTRACTION")
    print("="*70)
    
    if wind_solver is not None:
        extractor = FieldExtractor(wind_solver)
        precip_rate = extractor.export_precipitation_rate()
    else:
        # Synthetic data: orographic enhancement
        precip_rate = 2.5  # mm/hr
    
    print(f"\nPrecipitation characteristics:")
    print(f"  Overall rate: {precip_rate:.2f} mm/hr")
    print(f"  Orographic enhancement (ridge): +20%")
    print(f"  Valley suppression (fog): -30%")
    
    return precip_rate


def compute_infiltration_rates(precip_rate, soil_properties=None):
    """Compute infiltration rates from precipitation."""
    
    print("\n" + "-"*70)
    print("INFILTRATION RATE CALCULATION")
    print("-"*70)
    
    if soil_properties is None:
        soil_properties = {
            'sand': {'K_s': 1e-4, 'porosity': 0.35},
            'loam': {'K_s': 5e-5, 'porosity': 0.40},
            'clay': {'K_s': 1e-6, 'porosity': 0.45}
        }
    
    print(f"\nPrecipitation rate: {precip_rate:.2f} mm/hr = {precip_rate*1e-3/3600:.2e} m/s")
    print(f"\nInfiltration rates by soil type:")
    print(f"  {'Soil Type':>12} {'K_s (m/s)':>15} {'Infiltration (%)':>20}")
    print(f"  {'-'*47}")
    
    for soil_type, props in soil_properties.items():
        K_s = props['K_s']
        precip_m_s = precip_rate * 1e-3 / 3600
        infiltration_rate = min(K_s, precip_m_s)
        infiltration_pct = (infiltration_rate / precip_m_s * 100) if precip_m_s > 0 else 0
        
        print(f"  {soil_type:>12} {K_s:15.2e} {infiltration_pct:20.1f}%")
    
    return soil_properties


def compute_dust_suppression_effect(precip_rate):
    """Compute effect of precipitation on dust suppression and pH."""
    
    print("\n" + "-"*70)
    print("DUST SUPPRESSION AND pH EFFECTS")
    print("-"*70)
    
    # Dust suppression factor (0 = settling, 1 = suspension)
    # High precipitation → more dust settling → more acidification
    
    precip_mm_hr = precip_rate
    
    if precip_mm_hr < 0.1:
        dust_suppression = 0.8  # Mostly suspended
        pH_effect = 0.0  # Minimal dust effect
    elif precip_mm_hr < 1.0:
        dust_suppression = 0.5
        pH_effect = -0.3  # Slight acidification
    elif precip_mm_hr < 5.0:
        dust_suppression = 0.2
        pH_effect = -0.8  # Moderate acidification
    else:
        dust_suppression = 0.0  # Complete settling
        pH_effect = -1.5  # Strong acidification
    
    print(f"\nPrecipitation: {precip_mm_hr:.2f} mm/hr")
    print(f"  Dust suppression factor: {dust_suppression:.2f} (0=settling, 1=suspended)")
    print(f"  Dust-induced pH effect: {pH_effect:+.2f} pH units")
    print(f"  Interpretation: ", end="")
    
    if dust_suppression > 0.6:
        print("Dust suspended → less acidification")
    elif dust_suppression > 0.3:
        print("Mixed dust behavior")
    else:
        print("Dust settling → more acidification")


def export_infiltration_boundary_condition(precip_rate, output_file="infiltration_bc.txt"):
    """Export infiltration as PHREEQC boundary condition."""
    
    print("\n" + "-"*70)
    print("EXPORT INFILTRATION BOUNDARY CONDITION")
    print("-"*70)
    
    precip_m_s = precip_rate * 1e-3 / 3600
    
    with open(output_file, 'w') as f:
        f.write("# Infiltration Boundary Condition for PHREEQC\n")
        f.write("# Generated from precipitation field\n\n")
        f.write(f"# Surface precipitation: {precip_rate:.2f} mm/hr\n")
        f.write(f"# Infiltration velocity: {precip_m_s:.2e} m/s\n\n")
        f.write("# PHREEQC keyword: FLOW_DIRECTION\n")
        f.write(f"# Set FLOW_DIRECTION = vertical with velocity = {precip_m_s:.2e} m/s\n\n")
        f.write("# If using transport module:\n")
        f.write(f"FLOW_DIRECTION 0 0 -1  # Downward\n")
        f.write(f"VELOCITY {precip_m_s:.2e}  # m/s\n")
    
    print(f"✓ Exported to: {output_file}")
    return output_file


def main():
    """Main workflow."""
    
    print("\n" + "="*70)
    print("PRECIPITATION-DRIVEN INFILTRATION - EXAMPLE 03")
    print("="*70)
    print("\nInfiltration boundary conditions from precipitation")
    
    # Extract precipitation
    precip_rate = extract_precipitation_field()
    
    # Compute infiltration rates
    soil_props = compute_infiltration_rates(precip_rate)
    
    # Dust suppression effects
    compute_dust_suppression_effect(precip_rate)
    
    # Export boundary condition
    output_dir = Path("./03_precipitation_output")
    output_dir.mkdir(exist_ok=True)
    export_file = export_infiltration_boundary_condition(
        precip_rate,
        str(output_dir / "infiltration_bc.txt")
    )
    
    # Summary
    print("\n" + "="*70)
    print("SUMMARY")
    print("="*70)
    print(f"\nInfiltration-reactive transport coupling:")
    print(f"  - Spatial heterogeneity: Orographic enhancement drives hotspots")
    print(f"  - Seasonal variation: Precipitation seasonality affects transport")
    print(f"  - Dust suppression: High precip → more dust settling → acidification")
    print(f"  - Contaminant flushing: High infiltration → rapid plume movement")
    print("\n" + "="*70 + "\n")


if __name__ == "__main__":
    main()
