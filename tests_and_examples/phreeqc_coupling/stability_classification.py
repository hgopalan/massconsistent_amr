#!/usr/bin/env python3
"""
stability_classification.py - Pasquill-Gifford-Turner Stability Classification

Demonstrates extraction and application of atmospheric stability (PGT A-F)
for reaction rate modifiers and boundary layer depth estimation.

Stability controls:
  - A (Very unstable): Enhanced mixing, lower residence time → lower oxidation
  - D (Neutral): Baseline conditions
  - F (Very stable): Reduced mixing, higher residence time → higher oxidation

References:
    - Turner (1994). Workbook of atmospheric dispersion estimates.
    - Paulson & Simpson (1981). Monin-Obukhov stability parameter.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

def main():
    print("\n" + "="*70)
    print("ATMOSPHERIC STABILITY CLASSIFICATION - EXAMPLE 05")
    print("="*70)
    
    # Stability classes and effects
    stability_classes = {
        'A': {'name': 'Very Unstable', 'rate_factor': 0.75, 'mixing_height': 800},
        'B': {'name': 'Unstable', 'rate_factor': 0.85, 'mixing_height': 600},
        'C': {'name': 'Slightly Unstable', 'rate_factor': 0.95, 'mixing_height': 500},
        'D': {'name': 'Neutral', 'rate_factor': 1.00, 'mixing_height': 400},
        'E': {'name': 'Slightly Stable', 'rate_factor': 1.15, 'mixing_height': 300},
        'F': {'name': 'Stable', 'rate_factor': 1.25, 'mixing_height': 200}
    }
    
    print(f"\nStability Classification and Effects:")
    print(f"  {'Class':>6} {'Description':>18} {'Rate Factor':>15} {'Mix Height (m)':>16}")
    print(f"  {'-'*55}")
    
    for class_id, info in stability_classes.items():
        print(f"  {class_id:>6} {info['name']:>18} {info['rate_factor']:>15.2f}x {info['mixing_height']:>16.0f}")
    
    print(f"\nInterpretation:")
    print(f"  - Unstable (A-C): Enhanced vertical mixing → shorter residence time")
    print(f"  - Stable (E-F): Suppressed mixing → longer residence time")
    print(f"  - Neutral (D): Baseline conditions")
    
    print("="*70 + "\n")

if __name__ == "__main__":
    main()
