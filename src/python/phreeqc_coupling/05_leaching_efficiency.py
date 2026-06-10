#!/usr/bin/env python3
"""
05_leaching_efficiency.py - Wind-Dependent Leaching Efficiency Example

Demonstrates dust suppression and Sherwood number correlations for wind-enhanced
ore leaching. Combines dust settling (low pH effects) with mass transfer
(dissolution kinetics) to predict total leaching efficiency.

Workflow:
1. Build dust suppression lookup table (wind → settling fraction)
2. Build Sherwood number lookup table (wind × particle size → Sh)
3. Combine both effects to compute net leaching efficiency
4. Analyze wind-dependent chemistry effects (pH, dissolution rates)
5. Generate output for PHREEQC integration

References:
    - Dust suppression: Gillies et al. (2005), particle settling
    - Sherwood correlation: Ranz & Marshall (1952), mass transfer
    - Leaching kinetics: Nicholson et al. (1990), oxidation rates
"""

import numpy as np
from pathlib import Path
import logging
import sys
from typing import List, Tuple

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from dust_suppression_lookup import (
    build_dust_suppression_lookup,
    compute_dust_suppression_factor,
    compute_dust_suppression_effect_on_ph,
    save_dust_suppression_lookup_to_csv
)
from leaching_efficiency import (
    build_sherwood_lookup,
    compute_leaching_efficiency,
    compute_leaching_rate_enhancement,
    save_sherwood_lookup_to_csv
)

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def main():
    """Run leaching efficiency analysis example."""
    
    print("=" * 70)
    print("Wind-Dependent Leaching Efficiency Analysis")
    print("=" * 70)
    
    # Step 1: Build dust suppression lookup
    print("\n[Step 1] Building dust suppression lookup table...")
    
    dust_lookup = build_dust_suppression_lookup(
        u_speeds=np.linspace(0.5, 25, 30),
        particle_sizes=np.array([1, 5, 10, 50, 100, 500, 1000])  # μm
    )
    
    print(f"  ✓ Built lookup: {len(dust_lookup.u_speeds)} wind speeds × "
          f"{len(dust_lookup.particle_sizes)} particle sizes")
    
    # Step 2: Build Sherwood number lookup
    print("\n[Step 2] Building Sherwood number lookup table...")
    
    sherwood_lookup = build_sherwood_lookup(
        u_speeds=np.linspace(0.5, 25, 30),
        particle_sizes=np.logspace(2, 3, 15)  # 100-1000 μm
    )
    
    print(f"  ✓ Built lookup: {len(sherwood_lookup.u_speeds)} wind speeds × "
          f"{len(sherwood_lookup.particle_sizes)} particle sizes")
    
    # Step 3: Save lookups to CSV
    print("\n[Step 3] Saving lookup tables to CSV files...")
    
    output_dir = Path('./lookup_tables')
    output_dir.mkdir(exist_ok=True)
    
    dust_file = output_dir / 'dust_suppression.csv'
    save_dust_suppression_lookup_to_csv(dust_lookup, str(dust_file))
    
    sherwood_file = output_dir / 'sherwood_number.csv'
    save_sherwood_lookup_to_csv(sherwood_lookup, str(sherwood_file), output_type='sherwood')
    
    print(f"  ✓ Dust suppression lookup: {dust_file}")
    print(f"  ✓ Sherwood number lookup: {sherwood_file}")
    
    # Step 4: Dust suppression analysis
    print("\n[Step 4] Dust suppression wind sensitivity analysis...")
    
    test_speeds = np.array([0.5, 1, 2, 5, 10, 15, 20, 25])
    test_particle_size_um = 10  # μm
    
    print(f"\n  Particle size: {test_particle_size_um} μm")
    print(f"  {'Wind Speed (m/s)':<20} {'Dust Suppression':<20} {'Settled Fraction':<20}")
    print(f"  {'-'*60}")
    
    for u in test_speeds:
        f_dust = compute_dust_suppression_factor(u, particle_size=test_particle_size_um*1e-6, lookup=dust_lookup)
        f_settled = 1.0 - f_dust
        print(f"  {u:<20.1f} {f_dust:<20.1%} {f_settled:<20.1%}")
    
    # Step 5: Sherwood number and leaching efficiency analysis
    print("\n[Step 5] Leaching efficiency (Sherwood correlation) analysis...")
    
    print(f"\n  Particle size: 500 μm (typical heap ore)")
    print(f"  {'Wind Speed (m/s)':<20} {'Sherwood Number':<20} {'Efficiency Factor':<20}")
    print(f"  {'-'*60}")
    
    for u in test_speeds:
        Sh = sherwood_lookup.lookup_sherwood(u, 500)
        eff = compute_leaching_efficiency(u, particle_size=500e-6, lookup=sherwood_lookup)
        print(f"  {u:<20.1f} {Sh:<20.2f} {eff:<20.2f}×")
    
    # Step 6: pH effect analysis (dust suppression impact)
    print("\n[Step 6] pH effect from dust suppression...")
    
    reference_pH = 8.0
    print(f"\n  Reference pH (no dust settling): {reference_pH:.1f}")
    print(f"  {'Wind Speed (m/s)':<20} {'Dust Supp.':<15} {'pH (dust effect)':<20}")
    print(f"  {'-'*55}")
    
    for u in test_speeds:
        f_supp, pH_adjusted = compute_dust_suppression_effect_on_ph(
            u, reference_pH=reference_pH
        )
        pH_change = pH_adjusted - reference_pH
        print(f"  {u:<20.1f} {f_supp:<15.1%} {pH_adjusted:<20.2f} (Δ{pH_change:+.2f})")
    
    # Step 7: Combined leaching rate analysis
    print("\n[Step 7] Combined leaching rate enhancement...")
    
    reference_dissolution_rate = 1e-6  # mol/(m²·s) at 1 m/s wind
    
    print(f"\n  Reference dissolution rate (at 1 m/s): {reference_dissolution_rate:.2e} mol/(m²·s)")
    print(f"  {'Wind Speed':<15} {'Mass Transfer':<20} {'Leaching Rate':<25}")
    print(f"  {'(m/s)':<15} {'Enhancement':<20} {'[mol/(m²·s)]':<25}")
    print(f"  {'-'*60}")
    
    for u in test_speeds:
        eff, rate = compute_leaching_rate_enhancement(
            u, reference_dissolution_rate, particle_size=500e-6
        )
        print(f"  {u:<15.1f} {eff:<20.2f}× {rate:<25.2e}")
    
    # Step 8: Multi-particle-size analysis
    print("\n[Step 8] Leaching efficiency varies with particle size...")
    
    u_test = 10.0  # m/s
    particle_sizes_um = np.array([100, 250, 500, 750, 1000])  # μm
    
    print(f"\n  Wind speed: {u_test} m/s")
    print(f"  {'Particle Size (μm)':<25} {'Sherwood Number':<20} {'Efficiency':<20}")
    print(f"  {'-'*65}")
    
    for d in particle_sizes_um:
        Sh = sherwood_lookup.lookup_sherwood(u_test, d)
        eff = compute_leaching_efficiency(u_test, particle_size=d*1e-6, lookup=sherwood_lookup)
        print(f"  {d:<25.0f} {Sh:<20.2f} {eff:<20.2f}×")
    
    # Step 9: Sensitivity analysis
    print("\n[Step 9] Sensitivity analysis: wind speed doubling...")
    
    print(f"\n  Reference wind speed: 5 m/s")
    print(f"  Doubled wind speed: 10 m/s")
    print(f"\n  {'Effect':<30} {'Change':<20}")
    print(f"  {'-'*50}")
    
    # Dust suppression sensitivity
    f_dust_ref = compute_dust_suppression_factor(5.0, lookup=dust_lookup)
    f_dust_double = compute_dust_suppression_factor(10.0, lookup=dust_lookup)
    
    eff_ref = compute_leaching_efficiency(5.0, particle_size=500e-6, lookup=sherwood_lookup)
    eff_double = compute_leaching_efficiency(10.0, particle_size=500e-6, lookup=sherwood_lookup)
    
    print(f"  {'Dust suppression':<30} {f_dust_double/f_dust_ref:<20.2f}×")
    print(f"  {'Leaching efficiency':<30} {eff_double/eff_ref:<20.2f}×")
    print(f"  {'Combined effect':<30} {(f_dust_double * eff_double)/(f_dust_ref * eff_ref):<20.2f}×")
    
    # Step 10: Output summary for PHREEQC
    print("\n[Step 10] Summary for PHREEQC integration...")
    
    phreeqc_inputs = {
        'wind_speeds': test_speeds,
        'dust_suppression': [compute_dust_suppression_factor(u, lookup=dust_lookup) for u in test_speeds],
        'leaching_efficiency': [compute_leaching_efficiency(u, particle_size=500e-6, lookup=sherwood_lookup) for u in test_speeds],
        'ph_adjustments': [compute_dust_suppression_effect_on_ph(u)[1] - reference_pH for u in test_speeds]
    }
    
    print(f"\n  Generated lookup tables for PHREEQC:")
    print(f"    - {len(phreeqc_inputs['wind_speeds'])} wind speed scenarios")
    print(f"    - Dust suppression factors [0-1]")
    print(f"    - Leaching efficiency factors (relative to reference)")
    print(f"    - pH adjustment (relative to {reference_pH:.1f})")
    
    print("\n" + "=" * 70)
    print("✓ Leaching efficiency analysis complete")
    print(f"  Lookup tables: {output_dir}/")
    print("=" * 70)
    
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as e:
        logger.error(f"Fatal error: {e}", exc_info=True)
        sys.exit(1)
