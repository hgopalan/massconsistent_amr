#!/usr/bin/env python3
"""
run_fire_one_way.py - Run one-way fire coupling scenario (California)

One-way coupling: Wind → Fire
Wind field is computed independently and provided to the fire solver.
Fire spread responds to wind dynamics, but fire does NOT affect wind.

California Scenario:
- Coastal terrain (400-700m)
- 5 m/s northwesterly wind
- Circular fire ignition at domain center, 256m radius

Date: June 2026
"""

import os
import sys
from pathlib import Path

def main():
    """Run one-way coupled wind-fire simulation"""
    
    print("\n" + "="*70)
    print("ONE-WAY FIRE COUPLING: CALIFORNIA")
    print("Wind → Fire (Fire does NOT affect wind)")
    print("="*70 + "\n")
    
    script_dir = Path(__file__).resolve().parent
    
    print("Configuration Summary:")
    print("-" * 70)
    print("Location:        Northern California (coastal)")
    print("Coupling Mode:   ONE-WAY (Wind → Fire)")
    print("Domain Size:     10 km × 10 km")
    print("Grid Resolution: 156 × 156 cells (64m horizontal)")
    print()
    print("Wind Configuration:")
    print("  Speed:    5.0 m/s northwesterly at 10m height")
    print("  Profile:  Powerlaw (α=0.2)")
    print("  z0:       0.08 m (coastal)")
    print()
    print("Fire Configuration:")
    print("  Fuel Model:      Rothermel model #1 (short grass)")
    print("  Fuel Moisture:   15% (0.15)")
    print("  Propagation:     FARSITE")
    print("  Ignition Type:   Circular sphere")
    print("  Ignition Center: (5000, 5000) m (domain center)")
    print("  Ignition Radius: 256 m")
    print("  Simulation Time: 1200 s (20 minutes)")
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
    
    print("Running one-way coupled simulation...")
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
        print("\nInitializing one-way coupled wind-fire solver...")
        coupled = CoupledWindFireSimulation(
            wind_inputs=str(wind_inputs),
            fire_inputs=str(fire_inputs),
            coupling_mode='one_way'
        )
        
        print("✓ Coupled solver initialized")
        print(f"  Wind domain: [{coupled.wind.xmin:.0f}, {coupled.wind.xmax:.0f}] m (x)")
        print(f"               [{coupled.wind.ymin:.0f}, {coupled.wind.ymax:.0f}] m (y)")
        print(f"  Fire domain: [{coupled.fire.prob_lo[0]:.0f}, {coupled.fire.prob_hi[0]:.0f}] m (x)")
        print(f"               [{coupled.fire.prob_lo[1]:.0f}, {coupled.fire.prob_hi[1]:.0f}] m (y)")
        
        # Run simulation
        print("\nRunning coupled simulation...")
        result = coupled.run(
            num_steps=100,
            wind_update_interval=5,
            callback=None
        )
        
        if not result['success']:
            print("ERROR: Simulation failed")
            return 1
        
        print(f"✓ Simulation completed")
        print(f"  Steps executed: {result['steps']}")
        print(f"  Final time: {result['final_time']:.1f} seconds")
        
        # Get status
        status = coupled.get_status()
        print(f"\nFinal Status:")
        print(f"  Fire simulation time: {status['fire_time']:.1f} s")
        print(f"  Wind solves: {status['wind_solves']}")
        print(f"  Coupled steps: {status['coupled_steps']}")
        print(f"  Domain compatible: {status['domain_compatible']}")
        
        # Finalize
        coupled.finalize()
        
        print("\n" + "="*70)
        print("✓ ONE-WAY COUPLING SIMULATION COMPLETE")
        print("="*70 + "\n")
        
        print("Output files generated:")
        print("  - Wind field (plotfile format)")
        print("  - Fire front and intensity (plotfile format)")
        print("  - Rate of spread data")
        print()
        
        return 0
        
    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
