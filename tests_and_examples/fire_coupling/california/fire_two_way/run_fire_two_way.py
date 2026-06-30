#!/usr/bin/env python3
"""
run_fire_two_way.py - Run two-way fire coupling scenario (California)

Two-way coupling: Wind ↔ Fire
Wind field is computed with fire heating effects. Fire heating is extracted
and fed back to the wind solver for fire-induced wind changes.

California Scenario:
- Coastal terrain (400-700m)
- 5 m/s northwesterly wind
- Circular fire ignition at domain center, 256m radius
- Fire heating affects wind field through buoyancy

Date: June 2026
"""

import os
import sys
from pathlib import Path

def main():
    """Run two-way coupled wind-fire simulation"""
    
    print("\n" + "="*70)
    print("TWO-WAY FIRE COUPLING: CALIFORNIA")
    print("Wind ↔ Fire (Fire heating affects wind)")
    print("="*70 + "\n")
    
    script_dir = Path(__file__).resolve().parent
    
    print("Configuration Summary:")
    print("-" * 70)
    print("Location:        Northern California (coastal)")
    print("Coupling Mode:   TWO-WAY (Wind ↔ Fire)")
    print("Domain Size:     10 km × 10 km")
    print("Grid Resolution: 156 × 156 cells (64m horizontal)")
    print()
    print("Wind Configuration:")
    print("  Speed:    5.0 m/s northwesterly at 10m height")
    print("  Profile:  Powerlaw (α=0.2)")
    print("  z0:       0.08 m (coastal)")
    print("  Heating:  Accepts fire heat source for two-way coupling")
    print()
    print("Fire Configuration:")
    print("  Fuel Model:      Rothermel model #1 (short grass)")
    print("  Fuel Moisture:   15% (0.15)")
    print("  Propagation:     FARSITE")
    print("  Ignition Type:   Circular sphere")
    print("  Ignition Center: (5000, 5000) m (domain center)")
    print("  Ignition Radius: 256 m")
    print("  Heat Source Extraction: Enabled at 5m height")
    print("  Simulation Time: 1200 s (20 minutes)")
    print()
    print("Coupling Strategy:")
    print("  1. Solve wind field with current heat source")
    print("  2. Pass wind field to fire solver")
    print("  3. Advance fire simulation")
    print("  4. Extract heat sources from fire")
    print("  5. Repeat with updated wind field")
    print()
    print("Input Files:")
    print(f"  Wind config:    {script_dir}/wind_inputs.i")
    print(f"  Fire config:    {script_dir}/fire_inputs.i")
    print(f"  Terrain data:   {script_dir}/terrain.csv")
    print()
    
    # Check input files
    wind_inputs = script_dir / "wind_inputs.i"
    fire_inputs = script_dir / "fire_inputs.i"
    terrain_file = script_dir / "terrain.csv"
    
    for f in [wind_inputs, fire_inputs, terrain_file]:
        if not f.exists():
            print(f"ERROR: Input file not found: {f}")
            return 1
    
    print("✓ All input files present\n")
    
    print("Running two-way coupled simulation...")
    print("-" * 70)
    
    try:
        # Import solvers
        try:
            from wind_solver import WindSolver
        except ImportError:
            print("ERROR: Could not import WindSolver")
            return 1
        
        try:
            from wildfire_solver import WildfireSolver
        except ImportError:
            print("ERROR: Could not import WildfireSolver")
            print("Make sure wildfire_levelset is built with Python bindings")
            return 1
        
        try:
            from levelset_coupling import CoupledWindFireSimulation
        except ImportError:
            print("ERROR: Could not import CoupledWindFireSimulation")
            return 1
        
        # Initialize coupled solver
        print("\nInitializing two-way coupled wind-fire solver...")
        coupled = CoupledWindFireSimulation(
            wind_inputs=str(wind_inputs),
            fire_inputs=str(fire_inputs),
            coupling_mode='two_way'
        )
        
        print("✓ Coupled solver initialized")
        print(f"  Wind domain: [{coupled.wind.xmin:.0f}, {coupled.wind.xmax:.0f}] m (x)")
        print(f"               [{coupled.wind.ymin:.0f}, {coupled.wind.ymax:.0f}] m (y)")
        print(f"  Fire domain: [{coupled.fire.prob_lo[0]:.0f}, {coupled.fire.prob_hi[0]:.0f}] m (x)")
        print(f"               [{coupled.fire.prob_lo[1]:.0f}, {coupled.fire.prob_hi[1]:.0f}] m (y)")
        print(f"  Coupling: TWO-WAY (fire heating ↔ wind)")
        
        # Tracking callback
        heat_steps = []
        def track_heat(step, result):
            if result.get('heat_source_added', False):
                heat_steps.append(step)
        
        # Run simulation
        print("\nRunning two-way coupled simulation...")
        result = coupled.run(
            num_steps=100,
            wind_update_interval=5,
            callback=track_heat
        )
        
        if not result['success']:
            print("ERROR: Simulation failed")
            return 1
        
        print(f"✓ Simulation completed")
        print(f"  Steps executed: {result['steps']}")
        print(f"  Final time: {result['final_time']:.1f} seconds")
        print(f"  Heat source updates: {len(heat_steps)}")
        
        # Get status
        status = coupled.get_status()
        print(f"\nFinal Status:")
        print(f"  Fire simulation time: {status['fire_time']:.1f} s")
        print(f"  Wind solves: {status['wind_solves']}")
        print(f"  Coupled steps: {status['coupled_steps']}")
        print(f"  Domain compatible: {status['domain_compatible']}")
        
        if heat_steps:
            print(f"  Heat source feedback active:")
            print(f"    First update at step {heat_steps[0]}")
            print(f"    Total updates: {len(heat_steps)}")
        
        # Finalize
        coupled.finalize()
        
        print("\n" + "="*70)
        print("✓ TWO-WAY COUPLING SIMULATION COMPLETE")
        print("="*70 + "\n")
        
        print("Output files generated:")
        print("  - Wind field with fire heating (plotfile format)")
        print("  - Fire front and intensity (plotfile format)")
        print("  - Heat sources and temperature impacts")
        print("  - Rate of spread with wind feedback")
        print()
        
        return 0
        
    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
