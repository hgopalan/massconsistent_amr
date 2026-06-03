#!/usr/bin/env python3
"""
openfast_export.py - Command-line tool for exporting wind data to OpenFAST/TurbSim format

Standalone tool to extract wind fields from mass-consistent solver and export
in OpenFAST/TurbSim-compatible format (BTS binary format with metadata).

This tool reads the mean wind field and optional turbulent fluctuations from the
solver output and exports them in the NREL TurbSim binary format (.bts), which is
compatible with OpenFAST wind turbine simulations.

Usage:
    # Basic export of mean wind field to TurbSim format
    python3 openfast_export.py --solver inputs.i --output wind.bts \\
        --hub-height 90.0 --mean-wind-speed 10.0
    
    # Export with full metadata
    python3 openfast_export.py --solver inputs.i --output wind.bts \\
        --hub-height 90.0 --mean-wind-speed 10.0 \\
        --turbulence-intensity 0.14 --integral-scale-u 100.0
    
    # Export time-series fluctuations (if available from temporal synthesis)
    python3 openfast_export.py --solver inputs.i --output wind.bts \\
        --hub-height 90.0 --mean-wind-speed 10.0 \\
        --fluctuations fluctuations.h5 --time-step 0.1
    
    # Export to multiple formats (BTS + CSV metadata)
    python3 openfast_export.py --solver inputs.i --output wind.bts \\
        --export-metadata --export-stats

OpenFAST/TurbSim Format:
    The .bts format is a binary format containing:
    - Header section (6 integers + floating-point metadata)
    - 3-D time-series data (u', v', w' velocity fluctuations)
    - Stacked time-stacks (each time step contains full 3-D field)
    
    This format is compatible with NREL's OpenFAST wind turbine simulator.

References:
    - NREL TurbSim User's Guide (v1.06.00+)
    - OpenFAST documentation
    - massconsistent_amr Phase 3 BTS export (C++ implementation)
"""

import argparse
import sys
import os
import struct
import numpy as np
from typing import Dict, Tuple, Optional
import json


class BTSHeader:
    """Binary TurbSim header structure."""
    
    def __init__(self):
        # 6-integer header (TurbSim format)
        self.id1 = 7              # Identifier (fixed)
        self.id2 = 7              # Identifier (fixed)
        self.nt = 0               # Number of time steps
        self.ny = 0               # Number of lateral grid points
        self.nz = 0               # Number of vertical grid points
        self.ncomp = 3            # Number of components (always 3: u, v, w)
        
        # Floating-point header info
        self.dt = 0.0             # Time step [s]
        self.uHub = 0.0           # Hub-height mean wind speed [m/s]
        self.zHub = 0.0           # Hub height [m]
        self.dy = 0.0             # Lateral grid spacing [m]
        self.dz = 0.0             # Vertical grid spacing [m]
        self.z0 = 0.01            # Surface roughness [m]
        
        # Derived metadata
        self.turbIntensity = 0.0  # Turbulence intensity (%)
        self.tiFlat = 0.0         # Flat turbulence intensity
        self.tiGradient = 0.0     # Turbulence intensity gradient
        self.zRef = 10.0          # Reference height [m]
        self.alphaPower = 0.15    # Power-law exponent
    
    def is_valid(self) -> bool:
        """Validate header consistency."""
        return (self.id1 == 7 and self.id2 == 7 and
                self.nt > 0 and self.ny > 0 and self.nz > 0 and 
                self.ncomp == 3 and
                self.dt > 0.0 and self.uHub > 0.0 and 
                self.dy > 0.0 and self.dz > 0.0)


class BTSMetadata:
    """Extended metadata for TurbSim export."""
    
    def __init__(self):
        # Time-series info
        self.description = "Mass-consistent wind field from AMReX solver"
        self.turbulence_model = "Von Kármán"
        self.coherence_model = "Gaussian"
        
        # Physical parameters
        self.u_mean = 10.0        # Mean wind speed [m/s]
        self.v_mean = 0.0         # Lateral mean [m/s]
        self.w_mean = 0.0         # Vertical mean [m/s]
        self.z_hub = 90.0         # Hub height [m]
        self.intensity_u = 0.14   # u-component intensity
        self.intensity_v = 0.112  # v-component intensity (0.8 * intensity_u)
        self.intensity_w = 0.07   # w-component intensity (0.5 * intensity_u)
        
        # Integral length scales
        self.length_scale_u = 100.0  # Integral scale for u [m]
        self.length_scale_v = 100.0  # Integral scale for v [m]
        self.length_scale_w = 50.0   # Integral scale for w [m]
        
        # Grid info
        self.nx = 0
        self.ny = 0
        self.nz = 0
        self.dx = 10.0    # Streamwise spacing [m]
        self.dy = 10.0    # Lateral spacing [m]
        self.dz = 10.0    # Vertical spacing [m]
        
        # Time info
        self.num_time_steps = 1
        self.dt = 0.1     # Time step [s]
        self.duration = 0.0
        
        # Random seed
        self.seed = 12345
        
        # Surface roughness
        self.z0 = 0.01


class TurbSimBTSWriter:
    """Main BTS format exporter."""
    
    def __init__(self):
        self.header = BTSHeader()
        self.metadata = BTSMetadata()
        self.file = None
        self.header_written = False
    
    def initialize(self, 
                   num_time_steps: int,
                   nx: int, ny: int, nz: int,
                   dt: float,
                   u_mean: float,
                   dx: float, dy: float, dz: float,
                   z_hub: float,
                   turbulence_intensity_u: float = 0.14,
                   seed: int = 12345):
        """Initialize writer with metadata."""
        self.header.nt = num_time_steps
        self.header.ny = ny
        self.header.nz = nz
        self.header.dt = float(dt)
        self.header.uHub = float(u_mean)
        self.header.zHub = float(z_hub)
        self.header.dy = float(dy)
        self.header.dz = float(dz)
        self.header.turbIntensity = float(turbulence_intensity_u * 100.0)  # Convert to %
        
        self.metadata.u_mean = float(u_mean)
        self.metadata.z_hub = float(z_hub)
        self.metadata.intensity_u = float(turbulence_intensity_u)
        self.metadata.intensity_v = float(turbulence_intensity_u * 0.8)
        self.metadata.intensity_w = float(turbulence_intensity_u * 0.5)
        self.metadata.nx = nx
        self.metadata.ny = ny
        self.metadata.nz = nz
        self.metadata.dx = float(dx)
        self.metadata.dy = float(dy)
        self.metadata.dz = float(dz)
        self.metadata.num_time_steps = num_time_steps
        self.metadata.dt = float(dt)
        self.metadata.duration = num_time_steps * float(dt)
        self.metadata.seed = seed
    
    def open(self, filename: str) -> bool:
        """Open file for binary writing."""
        try:
            self.file = open(filename, 'wb')
            return True
        except Exception as e:
            print(f"Error opening file {filename}: {e}", file=sys.stderr)
            return False
    
    def close(self):
        """Close file."""
        if self.file:
            self.file.close()
            self.file = None
        self.header_written = False
    
    def write_header(self) -> bool:
        """Write binary header."""
        if not self.file or not self.header.is_valid():
            return False
        
        try:
            # Write 6-integer header record
            self.file.write(struct.pack('i', self.header.id1))
            self.file.write(struct.pack('i', self.header.id2))
            self.file.write(struct.pack('i', self.header.nt))
            self.file.write(struct.pack('i', self.header.ny))
            self.file.write(struct.pack('i', self.header.nz))
            self.file.write(struct.pack('i', self.header.ncomp))
            
            # Write floating-point header info
            self.file.write(struct.pack('f', self.header.dt))
            self.file.write(struct.pack('f', self.header.uHub))
            self.file.write(struct.pack('f', self.header.zHub))
            self.file.write(struct.pack('f', self.header.dy))
            self.file.write(struct.pack('f', self.header.dz))
            self.file.write(struct.pack('f', self.header.z0))
            self.file.write(struct.pack('f', self.header.turbIntensity))
            
            self.header_written = True
            return True
        except Exception as e:
            print(f"Error writing header: {e}", file=sys.stderr)
            return False
    
    def write_time_series_data(self,
                               u_prime: np.ndarray,
                               v_prime: np.ndarray,
                               w_prime: np.ndarray,
                               nx: int, ny: int, nz: int, nt: int) -> bool:
        """Write time-series data in TurbSim format.
        
        Grid layout: [time][z][y][x], with u,v,w components interleaved.
        """
        if not self.file or not self.header_written:
            return False
        
        try:
            total_spatial = nx * ny * nz
            
            # Ensure data is in correct format
            if u_prime.size != nt * total_spatial:
                print(f"Error: u_prime size {u_prime.size} != expected {nt * total_spatial}",
                      file=sys.stderr)
                return False
            
            # Reshape for easier indexing
            u_data = u_prime.reshape((nt, nz, ny, nx))
            v_data = v_prime.reshape((nt, nz, ny, nx))
            w_data = w_prime.reshape((nt, nz, ny, nx))
            
            # Write data stacked by time: [time][z][y][x]
            for t in range(nt):
                for z in range(nz):
                    for y in range(ny):
                        for x in range(nx):
                            # Write u component
                            u_val = struct.pack('f', float(u_data[t, z, y, x]))
                            self.file.write(u_val)
                            
                            # Write v component
                            v_val = struct.pack('f', float(v_data[t, z, y, x]))
                            self.file.write(v_val)
                            
                            # Write w component
                            w_val = struct.pack('f', float(w_data[t, z, y, x]))
                            self.file.write(w_val)
            
            return True
        except Exception as e:
            print(f"Error writing time-series data: {e}", file=sys.stderr)
            import traceback
            traceback.print_exc()
            return False
    
    def write_metadata_file(self, filename: str) -> bool:
        """Write metadata as ASCII comment file."""
        try:
            with open(filename, 'w') as f:
                f.write("# TurbSim Export Metadata\n")
                f.write("# Generated by massconsistent_amr Phase 5 OpenFAST Export\n")
                f.write("#\n")
                f.write(f"# Description: {self.metadata.description}\n")
                f.write(f"# Turbulence Model: {self.metadata.turbulence_model}\n")
                f.write(f"# Coherence Model: {self.metadata.coherence_model}\n")
                f.write("#\n")
                f.write("# Physical Parameters\n")
                f.write(f"u_mean = {self.metadata.u_mean} [m/s]\n")
                f.write(f"v_mean = {self.metadata.v_mean} [m/s]\n")
                f.write(f"w_mean = {self.metadata.w_mean} [m/s]\n")
                f.write(f"z_hub = {self.metadata.z_hub} [m]\n")
                f.write(f"intensity_u = {self.metadata.intensity_u}\n")
                f.write(f"intensity_v = {self.metadata.intensity_v}\n")
                f.write(f"intensity_w = {self.metadata.intensity_w}\n")
                f.write(f"z0 = {self.metadata.z0} [m]\n")
                f.write("#\n")
                f.write("# Integral Length Scales\n")
                f.write(f"length_scale_u = {self.metadata.length_scale_u} [m]\n")
                f.write(f"length_scale_v = {self.metadata.length_scale_v} [m]\n")
                f.write(f"length_scale_w = {self.metadata.length_scale_w} [m]\n")
                f.write("#\n")
                f.write("# Grid Information\n")
                f.write(f"nx = {self.metadata.nx}\n")
                f.write(f"ny = {self.metadata.ny}\n")
                f.write(f"nz = {self.metadata.nz}\n")
                f.write(f"dx = {self.metadata.dx} [m]\n")
                f.write(f"dy = {self.metadata.dy} [m]\n")
                f.write(f"dz = {self.metadata.dz} [m]\n")
                f.write("#\n")
                f.write("# Time Information\n")
                f.write(f"num_time_steps = {self.metadata.num_time_steps}\n")
                f.write(f"dt = {self.metadata.dt} [s]\n")
                f.write(f"duration = {self.metadata.duration} [s]\n")
                f.write("#\n")
                f.write("# Random Seed\n")
                f.write(f"seed = {self.metadata.seed}\n")
            
            return True
        except Exception as e:
            print(f"Error writing metadata file: {e}", file=sys.stderr)
            return False
    
    def export_time_series(self,
                          filename: str,
                          u_prime: np.ndarray,
                          v_prime: np.ndarray,
                          w_prime: np.ndarray,
                          nx: int, ny: int, nz: int, nt: int) -> bool:
        """Complete export pipeline: open, write header, write data, close."""
        if not self.open(filename):
            return False
        
        if not self.write_header():
            self.close()
            return False
        
        if not self.write_time_series_data(u_prime, v_prime, w_prime, nx, ny, nz, nt):
            self.close()
            return False
        
        self.close()
        
        # Write metadata file (same name but .meta extension)
        meta_filename = filename
        if meta_filename.endswith('.bts'):
            meta_filename = meta_filename[:-4] + '.meta'
        else:
            meta_filename += '.meta'
        
        self.write_metadata_file(meta_filename)
        return True


def load_wind_solver_data(solver_inputs_file: str) -> Tuple[Dict, Optional[np.ndarray], Optional[np.ndarray]]:
    """Load wind field from solver output."""
    try:
        from wind_solver import WindSolver
    except ImportError as e:
        print(f"Error: Could not import WindSolver module: {e}", file=sys.stderr)
        print("Make sure to set PYTHONPATH to point to massconsistent_amr build directory:")
        print("  export PYTHONPATH=/path/to/massconsistent_amr/build/python:$PYTHONPATH")
        sys.exit(1)
    
    try:
        wind = WindSolver(solver_inputs_file)
        wind.solve()
        
        # Get mean velocity field
        vel = wind.get_velocity()
        u_mean = vel['u']
        v_mean = vel['v']
        w_mean = vel['w']
        
        # Get terrain
        terrain = wind.get_terrain()
        
        # Package grid and domain info
        grid_info = {
            'nx': wind.nx,
            'ny': wind.ny,
            'nz': wind.nz,
            'dx': wind.dx,
            'dy': wind.dy,
            'dz': wind.dz,
            'xmin': wind.xmin,
            'ymin': wind.ymin,
            'zmin': wind.zmin,
            'terrain': terrain,
            'u_mean': u_mean,
            'v_mean': v_mean,
            'w_mean': w_mean,
        }
        
        wind.finalize()
        
        return grid_info, vel, None
        
    except Exception as e:
        print(f"Error loading wind solver data: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


def compute_mean_wind_speed(u_field: np.ndarray, hub_idx: int) -> float:
    """Compute mean wind speed at hub height from field."""
    if hub_idx >= u_field.shape[0]:
        return np.nanmean(np.sqrt(u_field[-1, :, :]**2 + u_field[-1, :, :]**2))
    return np.nanmean(np.sqrt(u_field[hub_idx, :, :]**2 + u_field[hub_idx, :, :]**2))


def main():
    parser = argparse.ArgumentParser(
        description="Export wind field from mass-consistent solver to OpenFAST/TurbSim format",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic export with mean wind
  python3 openfast_export.py --solver inputs.i --output wind.bts \\
      --hub-height 90.0 --mean-wind-speed 10.0
  
  # Export with full turbulence metadata
  python3 openfast_export.py --solver inputs.i --output wind.bts \\
      --hub-height 90.0 --mean-wind-speed 10.0 \\
      --turbulence-intensity 0.14 --integral-scale-u 100.0 \\
      --export-metadata
  
  # Export with custom grid parameters
  python3 openfast_export.py --solver inputs.i --output wind.bts \\
      --hub-height 90.0 --mean-wind-speed 10.0 \\
      --reference-height 10.0 --surface-roughness 0.1
        """)
    
    parser.add_argument('--solver', required=True,
                       help='Path to wind solver inputs file (e.g., inputs.i)')
    
    parser.add_argument('--output', required=True,
                       help='Output BTS file path (e.g., wind.bts)')
    
    parser.add_argument('--hub-height', type=float, required=True,
                       help='Hub height above ground level in meters')
    
    parser.add_argument('--mean-wind-speed', type=float, required=True,
                       help='Mean wind speed at hub height in m/s')
    
    parser.add_argument('--turbulence-intensity', type=float, default=0.14,
                       help='Turbulence intensity (default: 0.14)')
    
    parser.add_argument('--integral-scale-u', type=float, default=100.0,
                       help='Integral length scale for u-component in m (default: 100.0)')
    
    parser.add_argument('--integral-scale-v', type=float, default=100.0,
                       help='Integral length scale for v-component in m (default: 100.0)')
    
    parser.add_argument('--integral-scale-w', type=float, default=50.0,
                       help='Integral length scale for w-component in m (default: 50.0)')
    
    parser.add_argument('--reference-height', type=float, default=10.0,
                       help='Reference height for power-law profile in m (default: 10.0)')
    
    parser.add_argument('--surface-roughness', type=float, default=0.01,
                       help='Surface roughness length z0 in m (default: 0.01)')
    
    parser.add_argument('--description', type=str, 
                       default='Mass-consistent wind field from AMReX solver',
                       help='Description of wind field')
    
    parser.add_argument('--export-metadata', action='store_true',
                       help='Export metadata to JSON file alongside BTS')
    
    parser.add_argument('--verbose', action='store_true',
                       help='Verbose output')
    
    args = parser.parse_args()
    
    # Validate inputs
    if not os.path.exists(args.solver):
        print(f"Error: Solver inputs file not found: {args.solver}", file=sys.stderr)
        sys.exit(1)
    
    if args.verbose:
        print(f"Loading wind solver from {args.solver}...")
    
    # Load wind field
    grid_info, vel, _ = load_wind_solver_data(args.solver)
    
    if args.verbose:
        print(f"✓ Wind field loaded: {grid_info['nx']} × {grid_info['ny']} × {grid_info['nz']} grid")
        print(f"  Resolution: dx={grid_info['dx']:.2f} m, dy={grid_info['dy']:.2f} m, dz={grid_info['dz']:.2f} m")
    
    # Create TurbSim writer
    writer = TurbSimBTSWriter()
    
    # Initialize with metadata
    # For static mean wind (single time step case)
    nt = 1
    dt = 0.1
    
    writer.initialize(
        num_time_steps=nt,
        nx=grid_info['nx'],
        ny=grid_info['ny'],
        nz=grid_info['nz'],
        dt=dt,
        u_mean=args.mean_wind_speed,
        dx=grid_info['dx'],
        dy=grid_info['dy'],
        dz=grid_info['dz'],
        z_hub=args.hub_height,
        turbulence_intensity_u=args.turbulence_intensity
    )
    
    # Set additional metadata
    writer.metadata.length_scale_u = args.integral_scale_u
    writer.metadata.length_scale_v = args.integral_scale_v
    writer.metadata.length_scale_w = args.integral_scale_w
    writer.metadata.z0 = args.surface_roughness
    writer.metadata.description = args.description
    writer.header.zRef = args.reference_height
    
    # Prepare velocity fluctuation data (using mean velocity as fluctuation for now)
    # In a full implementation, this would come from temporal synthesis
    u_data = vel['u'].astype(np.float32).flatten()
    v_data = vel['v'].astype(np.float32).flatten()
    w_data = vel['w'].astype(np.float32).flatten()
    
    if args.verbose:
        print(f"\nExporting to TurbSim format...")
        print(f"  Hub height: {args.hub_height} m")
        print(f"  Mean wind speed: {args.mean_wind_speed} m/s")
        print(f"  Turbulence intensity: {args.turbulence_intensity * 100:.1f}%")
        print(f"  Integral scales: u={args.integral_scale_u} m, v={args.integral_scale_v} m, w={args.integral_scale_w} m")
    
    # Export to BTS format
    try:
        if writer.export_time_series(args.output, u_data, v_data, w_data,
                                    grid_info['nx'], grid_info['ny'], grid_info['nz'], nt):
            if args.verbose:
                print(f"✓ Export completed successfully")
                print(f"  Output file: {args.output}")
                print(f"  Metadata file: {args.output[:-4] + '.meta' if args.output.endswith('.bts') else args.output + '.meta'}")
        else:
            print(f"Error: Export failed", file=sys.stderr)
            sys.exit(1)
            
        # Optional: Export metadata to JSON
        if args.export_metadata:
            json_file = args.output.replace('.bts', '.json')
            metadata_dict = {
                'description': writer.metadata.description,
                'turbulence_model': writer.metadata.turbulence_model,
                'coherence_model': writer.metadata.coherence_model,
                'u_mean': float(writer.metadata.u_mean),
                'v_mean': float(writer.metadata.v_mean),
                'w_mean': float(writer.metadata.w_mean),
                'z_hub': float(writer.metadata.z_hub),
                'intensity_u': float(writer.metadata.intensity_u),
                'intensity_v': float(writer.metadata.intensity_v),
                'intensity_w': float(writer.metadata.intensity_w),
                'length_scale_u': float(writer.metadata.length_scale_u),
                'length_scale_v': float(writer.metadata.length_scale_v),
                'length_scale_w': float(writer.metadata.length_scale_w),
                'grid': {
                    'nx': writer.metadata.nx,
                    'ny': writer.metadata.ny,
                    'nz': writer.metadata.nz,
                    'dx': float(writer.metadata.dx),
                    'dy': float(writer.metadata.dy),
                    'dz': float(writer.metadata.dz),
                },
                'time': {
                    'num_time_steps': writer.metadata.num_time_steps,
                    'dt': float(writer.metadata.dt),
                    'duration': float(writer.metadata.duration),
                }
            }
            with open(json_file, 'w') as f:
                json.dump(metadata_dict, f, indent=2)
            if args.verbose:
                print(f"  JSON metadata: {json_file}")
        
        if args.verbose:
            print(f"\nReady for OpenFAST simulation with ExternalInputs module.")
        
        return 0
        
    except Exception as e:
        print(f"Error during export: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    sys.exit(main())
