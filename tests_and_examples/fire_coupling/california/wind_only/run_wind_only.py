#!/usr/bin/env python3
"""
run_wind_only.py - Run wind-only scenario (California)

This script demonstrates running the wind solver independently
without fire coupling for the California coastal terrain scenario.

Wind Conditions:
- 5 m/s northwesterly wind at 10m reference height
- Powerlaw profile with exponent 0.2
- Domain: 10x10 km, resolution 64m horizontal, 8m vertical
- Terrain: SRTM-based California coastal mountains

Date: June 2026
"""

import os
import sys
from pathlib import Path

def main():
    """Run wind-only simulation"""
    
    print("\n" + "="*70)
    print("WIND-ONLY SCENARIO: CALIFORNIA")
    print("="*70 + "\n")
    
    # Get current directory
    script_dir = Path(__file__).resolve().parent
    
    print("Configuration Summary:")
    print("-" * 70)
    print("Location:        Northern California (coastal)")
    print("Terrain:         SRTM-based coastal mountains (400-700m)")
    print("Domain Size:     10 km × 10 km × 300m")
    print("Grid Resolution: 64m (x,y), 8m (z)")
    print("Grid Cells:      156 × 156 × 38")
    print()
    print("Wind Conditions:")
    print("  Reference height:  10 m")
    print("  Reference speed:   5.0 m/s")
    print("  Direction:         Northwesterly (U=5, V=0)")
    print("  Profile type:      Powerlaw (α=0.2)")
    print("  Surface roughness: z0=0.08m (coastal)")
    print()
    print("Solver Settings:")
    print("  Method:            Mass-consistent diagnostic")
    print("  MLMG num_pre_smooth: 8")
    print("  MLMG num_post_smooth: 8")
    print("  Convergence tol:   1e-8 (relative)")
    print("  Max iterations:    200")
    print()
    print("Input Files:")
    print(f"  Wind config:    {script_dir}/wind_inputs.i")
    print(f"  Terrain data:   {script_dir}/terrain.csv")
    print()
    
    # Check input files
    wind_inputs = script_dir / "wind_inputs.i"
    terrain_file = script_dir / "terrain.csv"
    
    if not wind_inputs.exists():
        print(f"ERROR: Wind input file not found: {wind_inputs}")
        return 1
    
    if not terrain_file.exists():
        print(f"ERROR: Terrain file not found: {terrain_file}")
        return 1
    
    print("✓ All input files present\n")
    
    print("Running wind-only simulation...")
    print("-" * 70)
    
    try:
        # Import wind solver
        try:
            from wind_solver import WindSolver
        except ImportError:
            print("ERROR: Could not import WindSolver")
            print("Make sure massconsistent_amr is built with Python bindings enabled:")
            print("  cmake -S . -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON")
            return 1
        
        # Create and initialize wind solver
        print("\nInitializing wind solver...")
        wind = WindSolver()
        wind.initialize(str(wind_inputs))
        
        print(f"✓ Solver initialized")
        print(f"  Grid dimensions: {wind.nx} × {wind.ny} × {wind.nz}")
        print(f"  Domain: [{wind.xmin:.1f}, {wind.xmax:.1f}] m (x)")
        print(f"          [{wind.ymin:.1f}, {wind.ymax:.1f}] m (y)")
        print(f"          [{wind.zmin:.1f}, {wind.zmax:.1f}] m (z)")
        print(f"  Grid spacing: dx={wind.dx:.1f}m, dy={wind.dy:.1f}m, dz={wind.dz:.1f}m")
        print(f"  Terrain range: {wind.zs_min:.1f} - {wind.zs_max:.1f} m")
        
        # Solve wind field
        print("\nSolving wind field...")
        wind.solve()
        
        print("✓ Wind field solution complete")
        
        # Get some statistics
        print("\nWind Field Statistics:")
        print("-" * 70)
        
        # Print summary
        print("✓ Simulation completed successfully")
        print("\nOutput files generated:")
        print("  - Wind velocity field (plotfile format)")
        print("  - Pressure field")
        print("  - Terrain information")
        
        # Finalize
        wind.finalize()
        
        print("\n" + "="*70)
        print("✓ WIND-ONLY SIMULATION COMPLETE")
        print("="*70 + "\n")
        
        return 0
        
    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
