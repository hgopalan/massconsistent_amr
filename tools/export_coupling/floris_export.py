#!/usr/bin/env python3
"""
floris_export.py - Command-line tool for exporting wind data to FLORIS format

Standalone tool to extract wind speeds from mass-consistent solver and export
in FLORIS-compatible format (CSV or JSON).

No FLORIS installation required - this is a standalone exporter that works
independently of FLORIS itself.

Usage:
    # Export wind at specific turbine locations to CSV
    python3 floris_export.py --solver inputs.i --turbines turbines.csv \\
        --hub-height 90.0 --output wind_data.csv
    
    # Export with speed-up ratios relative to reference wind
    python3 floris_export.py --solver inputs.i --turbines turbines.csv \\
        --hub-height 90.0 --reference-speed 10.0 --output wind_data.csv
    
    # Export to JSON format
    python3 floris_export.py --solver inputs.i --turbines turbines.csv \\
        --hub-height 90.0 --output wind_data.json

Turbine locations CSV format (turbines.csv):
    x,y
    100.0,200.0
    300.0,400.0
    500.0,600.0

Output CSV format (wind_data.csv):
    turbine_id,x,y,z_terrain,z_hub,u_ms,v_ms,speed_ms,direction_deg[,speedup_ratio]
    0,100.0,200.0,50.0,140.0,5.2,1.3,5.33,345.2[,1.05]
    ...
"""

import argparse
import sys
import csv
import os


def load_turbine_locations(csv_file: str) -> list:
    """Load turbine locations from CSV file."""
    locations = []
    try:
        with open(csv_file, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                x = float(row['x'])
                y = float(row['y'])
                locations.append((x, y))
        return locations
    except Exception as e:
        print(f"Error reading turbine locations from {csv_file}: {e}", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description="Export wind field from mass-consistent solver to FLORIS format",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic export to CSV
  python3 floris_export.py --solver inputs.i --turbines turbines.csv \\
      --output wind_data.csv
  
  # Export with speed-up ratios
  python3 floris_export.py --solver inputs.i --turbines turbines.csv \\
      --reference-speed 10.0 --output wind_data.csv
  
  # Export to JSON
  python3 floris_export.py --solver inputs.i --turbines turbines.csv \\
      --output wind_data.json --format json
        """
    )
    
    parser.add_argument('--solver', required=True,
                       help='Path to wind solver inputs file (e.g., inputs.i)')
    
    parser.add_argument('--turbines', required=True,
                       help='CSV file with turbine locations (columns: x, y)')
    
    parser.add_argument('--hub-height', type=float, default=90.0,
                       help='Hub height above ground level in meters (default: 90)')
    
    parser.add_argument('--reference-speed', type=float, default=None,
                       help='Reference wind speed for computing speed-up ratios (optional)')
    
    parser.add_argument('--output', required=True,
                       help='Output file (CSV or JSON based on extension)')
    
    parser.add_argument('--format', choices=['auto', 'csv', 'json'], default='auto',
                       help='Output format (default: auto-detect from extension)')
    
    parser.add_argument('--verbose', action='store_true',
                       help='Verbose output')
    
    args = parser.parse_args()
    
    # Validate inputs
    if not os.path.exists(args.solver):
        print(f"Error: Solver inputs file not found: {args.solver}", file=sys.stderr)
        sys.exit(1)
    
    if not os.path.exists(args.turbines):
        print(f"Error: Turbine locations file not found: {args.turbines}", file=sys.stderr)
        sys.exit(1)
    
    # Import wind solver and coupling module
    try:
        from wind_solver import WindSolver
        from floris_coupling import FLORISWindMap
    except ImportError as e:
        print(f"Error: Could not import required modules: {e}", file=sys.stderr)
        print("\nMake sure to set PYTHONPATH to point to massconsistent_amr build directory:")
        print("  export PYTHONPATH=/path/to/massconsistent_amr/build/python:$PYTHONPATH")
        sys.exit(1)
    
    if args.verbose:
        print(f"Loading wind solver from {args.solver}...")
    
    # Initialize and solve
    try:
        wind = WindSolver(args.solver)
        wind.solve()
    except Exception as e:
        print(f"Error initializing or solving wind field: {e}", file=sys.stderr)
        sys.exit(1)
    
    if args.verbose:
        print(f"Wind field solved: {wind.nx} x {wind.ny} x {wind.nz} grid")
    
    # Load turbine locations
    if args.verbose:
        print(f"Loading turbine locations from {args.turbines}...")
    
    turbines = load_turbine_locations(args.turbines)
    
    if args.verbose:
        print(f"Loaded {len(turbines)} turbine locations")
    
    # Create wind map and export
    try:
        wind_map = FLORISWindMap(wind)
        
        # Determine output format
        output_format = args.format
        if output_format == 'auto':
            if args.output.lower().endswith('.json'):
                output_format = 'json'
            else:
                output_format = 'csv'
        
        if args.verbose:
            print(f"Exporting wind data to {args.output} ({output_format})...")
            if args.reference_speed:
                print(f"  Including speed-up ratios relative to {args.reference_speed} m/s")
        
        if output_format == 'json':
            wind_map.export_to_json(turbines, args.hub_height, args.output, 
                                   args.reference_speed)
        else:
            wind_map.export_to_csv(turbines, args.hub_height, args.output, 
                                  args.reference_speed)
        
        if args.verbose:
            print(f"✓ Export completed successfully")
            print(f"  Output file: {args.output}")
            print(f"  Turbines: {len(turbines)}")
            print(f"  Hub height: {args.hub_height} m AGL")
        
    except Exception as e:
        print(f"Error during export: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)
    finally:
        wind.finalize()
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
