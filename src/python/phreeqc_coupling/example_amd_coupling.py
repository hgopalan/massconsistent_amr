#!/usr/bin/env python3
"""
example_amd_coupling.py - Demonstration of Wind-PHREEQC Coupling for AMD Analysis

Shows how to use the reactive transport coupling framework to predict acid mine
drainage geochemistry from wind-resolved atmospheric conditions.

This example demonstrates wind-modulated oxygen delivery affecting AMD chemistry
under valley terrain conditions where wind channeling creates chemical hotspots.

References:
    - Nicholson et al. (1990). Pyrite oxidation in carbonate-buffered systems.
"""

from wind_solver import WindSolver
from reactive_transport_coupling import ReactiveTransportCoupling
from pathlib import Path
import sys


def main():
    """Run AMD analysis with wind-PHREEQC coupling."""
    
    # Initialize wind solver
    print("=" * 60)
    print("Wind-PHREEQC Coupling: AMD Analysis Example")
    print("=" * 60)
    
    # Use example input file (adjust path as needed)
    inputs_file = "inputs_single.i"
    
    if not Path(inputs_file).exists():
        print(f"✗ Input file not found: {inputs_file}")
        print("  Please run from repository root with existing inputs file")
        return 1
    
    try:
        # Solve wind field
        print("\n1. Solving mass-consistent wind field...")
        wind = WindSolver(inputs_file)
        wind.solve()
        
        # Initialize coupling interface
        print("\n2. Initializing wind-PHREEQC coupling...")
        coupling = ReactiveTransportCoupling(wind, verbose=True)
        
        # Extract atmospheric fields
        print("\n3. Extracting atmospheric boundary conditions...")
        fields = coupling.extract_fields()
        print(f"   - Domain: {wind.nx} × {wind.ny} × {wind.nz} cells")
        print(f"   - Temperature range: {np.min(fields.T)-273.15:.1f}°C to {np.max(fields.T)-273.15:.1f}°C")
        print(f"   - Wind speed range: 0 to {np.max(np.sqrt(fields.u**2 + fields.v**2)):.2f} m/s")
        
        # Export fields to various formats
        print("\n4. Exporting atmospheric fields...")
        exports = coupling.export_fields("amd_example_output", format="ascii")
        for fmt, filename in exports.items():
            print(f"   ✓ {fmt.upper()}: {filename}")
        
        # Identify AMD hotspots based on oxygen delivery
        print("\n5. Identifying AMD hotspots...")
        hotspot_result = coupling.compute_amd_hotspot_map(
            output_dir="amd_example_output/hotspots"
        )
        print(f"   - Hotspots identified: {hotspot_result['n_hotspots']}")
        print(f"   - O₂ delivery factor (mean): {hotspot_result['O2_factor_mean']:.3f}")
        
        # Generate PHREEQC input for AMD reactive transport
        print("\n6. Setting up PHREEQC reactive transport simulation...")
        amd_result = coupling.run_amd_simulation(
            output_dir="amd_example_output/phreeqc",
            run_phreeqc=False  # Set to True if PHREEQC is installed
        )
        print(f"   ✓ PHREEQC input: {amd_result['input_file']}")
        print(f"   - Temperature BC: {amd_result['boundary_conditions']['temperature']:.1f}°C")
        print(f"   - O₂ concentration BC: {amd_result['boundary_conditions']['O2_concentration']:.1f} µmol/kg")
        
        print("\n" + "=" * 60)
        print("✓ Example completed successfully!")
        print("=" * 60)
        print("\nNext steps:")
        print("  1. Review generated PHREEQC input file")
        print("  2. Run PHREEQC to compute AMD chemistry")
        print("  3. Analyze predicted acid generation rates")
        print("=" * 60)
        
        wind.finalize()
        return 0
        
    except Exception as e:
        print(f"\n✗ Error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    import numpy as np
    sys.exit(main())
