#!/usr/bin/env python3
"""
temperature_profile_bc.py - Temperature Profile Extraction

Demonstrates extraction of vertical temperature profiles for temperature-dependent
reactive transport simulations. Temperature controls reaction kinetics, mineral
solubility, and Arrhenius rate constants.

This example:
  1. Solves the wind field with thermal effects
  2. Extracts temperature profile (lapse rate adjustments)
  3. Computes Arrhenius rate corrections
  4. Maps temperature to PHREEQC reaction rates

Key Physics:
  - Moist adiabatic lapse rate: ~5-7 K/km
  - Arrhenius: k(T) = A × exp(-E_a/(R×T))
  - Temperature sensitivity: ~2-3× rate change per 10°C

References:
    - Businger et al. (1971). Flux-profile relationships.
    - Nicholson et al. (1990). Pyrite oxidation kinetics (E_a = 45 kJ/mol).
"""

import sys
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).parent.parent))

from phreeqc_coupling import FieldExtractor


def compute_arrhenius_correction(T_kelvin, E_a=45000, R=8.314, T_ref=288.15):
    """Compute temperature-dependent rate correction via Arrhenius equation.
    
    Parameters:
        T_kelvin: Temperature [K]
        E_a: Activation energy [J/mol] (default: 45 kJ/mol for pyrite oxidation)
        R: Gas constant [J/(mol·K)]
        T_ref: Reference temperature [K]
    
    Returns:
        float: Rate correction factor (multiplier relative to reference)
    """
    exponent = -E_a/R * (1/T_kelvin - 1/T_ref)
    return np.exp(exponent)


def extract_and_analyze_temperature(wind_solver=None, verbose=True):
    """Extract temperature profile and compute thermal corrections."""
    
    print("\n" + "="*70)
    print("TEMPERATURE PROFILE EXTRACTION")
    print("="*70)
    
    if wind_solver is not None:
        extractor = FieldExtractor(wind_solver, verbose=verbose)
        z_agl, T_profile = extractor.export_temperature_profile()
    else:
        # Synthetic temperature profile
        z_agl = np.array([1, 5, 10, 25, 50, 100, 200, 500])
        T_surface = 288.15  # 15°C at surface
        lapse_rate = 0.0065  # K/m (moist adiabatic)
        T_profile = T_surface - lapse_rate * z_agl
    
    # Convert to Celsius for display
    T_celsius = T_profile - 273.15
    
    print(f"\nTemperature profile (from surface):")
    print(f"  {'Height (m)':>12} {'T (°C)':>10} {'T (K)':>10} {'Rate Factor':>12}")
    print(f"  {'-'*44}")
    
    # Compute Arrhenius correction for pyrite oxidation
    rate_factors = []
    for z, T_k, T_c in zip(z_agl, T_profile, T_celsius):
        factor = compute_arrhenius_correction(T_k, E_a=45000)
        rate_factors.append(factor)
        
        print(f"  {z:12.1f} {T_c:10.2f} {T_k:10.2f} {factor:12.2f}x")
    
    return z_agl, T_profile, np.array(rate_factors)


def compute_seasonal_variation():
    """Show seasonal temperature effects on reaction rates."""
    
    print("\n" + "-"*70)
    print("SEASONAL VARIATION ANALYSIS")
    print("-"*70)
    
    seasons = {
        'Winter': 273.15 + 0,   # 0°C
        'Spring': 273.15 + 8,   # 8°C
        'Summer': 273.15 + 18,  # 18°C
        'Fall':   273.15 + 12   # 12°C
    }
    
    print(f"\nRate correction factors (relative to 15°C baseline):")
    print(f"  {'Season':>10} {'T (°C)':>8} {'Rate Factor':>15}")
    print(f"  {'-'*33}")
    
    baseline_factor = compute_arrhenius_correction(288.15, E_a=45000)
    
    for season, T_k in seasons.items():
        factor = compute_arrhenius_correction(T_k, E_a=45000)
        relative_factor = factor / baseline_factor
        T_c = T_k - 273.15
        
        print(f"  {season:>10} {T_c:8.1f} {relative_factor:15.2f}x")


def export_temperature_for_phreeqc(z_agl, T_profile, output_file="temperature_bc.txt"):
    """Export temperature profile in PHREEQC-compatible format."""
    
    print("\n" + "-"*70)
    print("EXPORTING TEMPERATURE BOUNDARY CONDITIONS")
    print("-"*70)
    
    with open(output_file, 'w') as f:
        f.write("# Temperature Profile for PHREEQC Reactive Transport\n")
        f.write("# Format: Height (m), Temperature (°C), Temperature (K)\n")
        f.write("# Use this profile for temperature-dependent rate constants\n\n")
        
        for z, T_k in zip(z_agl, T_profile):
            T_c = T_k - 273.15
            # PHREEQC format: TEMPERATURE keyword
            f.write(f"TEMPERATURE {T_c:.2f}  # Height {z:.1f} m: {T_c:.2f} °C ({T_k:.2f} K)\n")
    
    print(f"✓ Exported to: {output_file}")
    return output_file


def main():
    """Main workflow."""
    
    print("\n" + "="*70)
    print("TEMPERATURE PROFILE EXTRACTION - EXAMPLE 02")
    print("="*70)
    print("\nTemperature-dependent reaction rate calculations")
    
    # Extract temperature
    z_agl, T_profile, rate_factors = extract_and_analyze_temperature()
    
    # Seasonal analysis
    compute_seasonal_variation()
    
    # Export
    output_dir = Path("./temperature_output")
    output_dir.mkdir(exist_ok=True)
    export_file = export_temperature_for_phreeqc(
        z_agl, T_profile,
        str(output_dir / "temperature_bc.txt")
    )
    
    # Summary
    print("\n" + "="*70)
    print("SUMMARY")
    print("="*70)
    print(f"\nTemperature-dependent kinetics:")
    print(f"  - Lapse rate: {(T_profile[0] - T_profile[-1]) / (z_agl[-1] - z_agl[0]) * 1000:.1f} K/km")
    print(f"  - Temperature range: {T_profile.min()-273.15:.1f} to {T_profile.max()-273.15:.1f} °C")
    print(f"  - Rate factor range: {rate_factors.min():.2f} to {rate_factors.max():.2f}x")
    print(f"  - Temperature sensitivity: ~{(rate_factors.max()/rate_factors.min())**0.1:.2f}x per °C")
    print(f"\nKey insight:")
    print(f"  Higher temperatures (lower elevations) → Faster oxidation rates")
    print(f"  This enhances AMD generation in valley bottoms")
    print("\n" + "="*70 + "\n")


if __name__ == "__main__":
    main()
