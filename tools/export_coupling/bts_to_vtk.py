#!/usr/bin/env python3
"""
BTS to VTK Converter

Converts TurbSim binary (.bts) format to VTK format for visualization
in ParaView, VisIt, or other VTK-compatible viewers.

Usage:
    python bts_to_vtk.py input.bts output.vtk
    
This script reads the binary TurbSim format and converts it to:
- VTK unstructured grid or structured grid format
- Multiple time steps as separate files or a time series
- Velocity vector fields (u, v, w components)
- Optional derived fields (magnitude, intensity)
"""

import struct
import sys
import os
from pathlib import Path
import numpy as np


class BTSReader:
    """Read TurbSim binary (.bts) format files."""
    
    def __init__(self, filename):
        """Initialize BTS reader with filename."""
        self.filename = filename
        self.header = None
        self.data = None
        self.u_prime = None
        self.v_prime = None
        self.w_prime = None
    
    def read(self):
        """Read BTS file and extract data."""
        try:
            with open(self.filename, 'rb') as f:
                # Read 6-integer header
                header_ints = struct.unpack('6i', f.read(6 * 4))
                id1, id2, nt, ny, nz, ncomp = header_ints
                
                if id1 != 7 or id2 != 7:
                    raise ValueError(f"Invalid BTS format identifiers: {id1}, {id2}")
                
                if ncomp != 3:
                    raise ValueError(f"Expected 3 components, got {ncomp}")
                
                # Read floating-point header
                header_floats = struct.unpack('6f', f.read(6 * 4))
                dt, uHub, zHub, dy, dz, z0 = header_floats
                
                # Read turbulence intensity
                (turb_intensity,) = struct.unpack('f', f.read(4))
                
                self.header = {
                    'id1': id1,
                    'id2': id2,
                    'nt': nt,
                    'ny': ny,
                    'nz': nz,
                    'ncomp': ncomp,
                    'dt': dt,
                    'uHub': uHub,
                    'zHub': zHub,
                    'dy': dy,
                    'dz': dz,
                    'z0': z0,
                    'turbulence_intensity': turb_intensity
                }
                
                print(f"BTS Header:")
                print(f"  Grid: {ny}(y) x {nz}(z) x {nt}(time)")
                print(f"  Spacing: dy={dy}m, dz={dz}m, dt={dt}s")
                print(f"  Hub: z={zHub}m, U={uHub}m/s, TI={turb_intensity}%")
                print(f"  Roughness: z0={z0}m")
                
                # Read velocity data
                total_points = nt * ny * nz
                u_data = struct.unpack(f'{total_points}f', 
                                      f.read(total_points * 4))
                v_data = struct.unpack(f'{total_points}f',
                                      f.read(total_points * 4))
                w_data = struct.unpack(f'{total_points}f',
                                      f.read(total_points * 4))
                
                # Store as numpy arrays
                self.u_prime = np.array(u_data).reshape((nt, nz, ny))
                self.v_prime = np.array(v_data).reshape((nt, nz, ny))
                self.w_prime = np.array(w_data).reshape((nt, nz, ny))
                
                print(f"  Data ranges:")
                print(f"    u': [{self.u_prime.min():.3f}, {self.u_prime.max():.3f}] m/s")
                print(f"    v': [{self.v_prime.min():.3f}, {self.v_prime.max():.3f}] m/s")
                print(f"    w': [{self.w_prime.min():.3f}, {self.w_prime.max():.3f}] m/s")
                
                return True
        
        except Exception as e:
            print(f"Error reading BTS file: {e}")
            return False
    
    def get_spatial_grid(self):
        """Generate spatial grid coordinates."""
        if self.header is None:
            return None
        
        ny = self.header['ny']
        nz = self.header['nz']
        dy = self.header['dy']
        dz = self.header['dz']
        zHub = self.header['zHub']
        
        # Create grid: y from 0 to (ny-1)*dy, z centered at zHub
        y = np.arange(ny) * dy
        z = np.arange(nz) * dz + (zHub - (nz - 1) * dz / 2.0)
        
        return y, z


class VTKWriter:
    """Write VTK format files for turbulence visualization."""
    
    @staticmethod
    def write_structured_grid(filename, bts_reader, time_step=0):
        """
        Write single time step as VTK structured grid.
        
        Args:
            filename: Output VTK filename
            bts_reader: BTSReader instance with loaded data
            time_step: Time step to write (0 to nt-1)
        """
        if bts_reader.u_prime is None:
            print("No BTS data to write")
            return False
        
        header = bts_reader.header
        ny = header['ny']
        nz = header['nz']
        
        if time_step >= header['nt']:
            print(f"Time step {time_step} out of range [0, {header['nt']-1}]")
            return False
        
        # Get spatial coordinates
        y, z = bts_reader.get_spatial_grid()
        
        # Extract time slice
        u_slice = bts_reader.u_prime[time_step, :, :]  # (nz, ny)
        v_slice = bts_reader.v_prime[time_step, :, :]
        w_slice = bts_reader.w_prime[time_step, :, :]
        
        # Create x grid (streamwise direction, use time as x for visualization)
        x = np.array([time_step * header['dt']])
        
        # Create 3D grid: (nx=1, ny, nz)
        X = np.ones((1, ny, nz)) * (time_step * header['dt'])
        Y = np.ones((1, ny, nz)) * y[np.newaxis, :, np.newaxis]
        Z = np.ones((1, ny, nz)) * z[np.newaxis, np.newaxis, :]
        
        # Flatten for VTK
        x_flat = X.flatten()
        y_flat = Y.flatten()
        z_flat = Z.flatten()
        
        # Flatten velocity components
        u_flat = u_slice.flatten()
        v_flat = v_slice.flatten()
        w_flat = w_slice.flatten()
        
        # Calculate magnitude and intensity
        magnitude = np.sqrt(u_flat**2 + v_flat**2 + w_flat**2)
        intensity = magnitude / (header['uHub'] + 1e-10)  # Relative to hub wind
        
        return VTKWriter._write_vtk_file(
            filename, x_flat, y_flat, z_flat,
            u_flat, v_flat, w_flat, magnitude, intensity,
            ny * nz, header
        )
    
    @staticmethod
    def write_time_series(basename, bts_reader):
        """
        Write all time steps as separate VTK files.
        
        Creates:
            basename_000000.vtk, basename_000001.vtk, ...
            basename.pvd (ParaView time series collection)
        """
        if bts_reader.u_prime is None:
            print("No BTS data to write")
            return False
        
        nt = bts_reader.header['nt']
        dt = bts_reader.header['dt']
        
        # Remove extension if present
        base = basename
        if base.endswith('.pvd') or base.endswith('.vtk'):
            base = base.rsplit('.', 1)[0]
        
        # Write individual time steps
        pvd_data = []
        for t in range(nt):
            vtk_file = f"{base}_{t:06d}.vtk"
            
            if not VTKWriter.write_structured_grid(vtk_file, bts_reader, t):
                print(f"Failed to write {vtk_file}")
                return False
            
            pvd_data.append((t * dt, vtk_file))
            print(f"Wrote {vtk_file}")
        
        # Write PVD (ParaView Data) time series metadata
        pvd_file = f"{base}.pvd"
        if VTKWriter._write_pvd_file(pvd_file, pvd_data):
            print(f"Wrote {pvd_file}")
        
        return True
    
    @staticmethod
    def _write_vtk_file(filename, x, y, z, u, v, w, magnitude, intensity,
                       num_points, header):
        """Write VTK unstructured grid file."""
        try:
            with open(filename, 'w') as f:
                # VTK header
                f.write("# vtk DataFile Version 3.0\n")
                f.write(f"Synthetic Turbulence - Time {header.get('time', 0):.4f}s\n")
                f.write("ASCII\n")
                f.write("DATASET UNSTRUCTURED_GRID\n")
                f.write(f"POINTS {num_points} float\n")
                
                # Write points
                for i in range(num_points):
                    f.write(f"{x[i]:.6e} {y[i]:.6e} {z[i]:.6e}\n")
                
                # Write cells (one vertex per point for visualization)
                f.write(f"CELLS {num_points} {num_points * 2}\n")
                for i in range(num_points):
                    f.write(f"1 {i}\n")
                
                # Cell types (1 = VTK_VERTEX)
                f.write(f"CELL_TYPES {num_points}\n")
                for i in range(num_points):
                    f.write("1\n")
                
                # Point data
                f.write(f"POINT_DATA {num_points}\n")
                
                # Velocity vectors
                f.write("VECTORS velocity float\n")
                for i in range(num_points):
                    f.write(f"{u[i]:.6e} {v[i]:.6e} {w[i]:.6e}\n")
                
                # Magnitude
                f.write("SCALARS magnitude float\n")
                f.write("LOOKUP_TABLE default\n")
                for i in range(num_points):
                    f.write(f"{magnitude[i]:.6e}\n")
                
                # Intensity
                f.write("SCALARS intensity float\n")
                f.write("LOOKUP_TABLE default\n")
                for i in range(num_points):
                    f.write(f"{intensity[i]:.6e}\n")
                
                # Individual components
                f.write("SCALARS u_component float\n")
                f.write("LOOKUP_TABLE default\n")
                for i in range(num_points):
                    f.write(f"{u[i]:.6e}\n")
                
                f.write("SCALARS v_component float\n")
                f.write("LOOKUP_TABLE default\n")
                for i in range(num_points):
                    f.write(f"{v[i]:.6e}\n")
                
                f.write("SCALARS w_component float\n")
                f.write("LOOKUP_TABLE default\n")
                for i in range(num_points):
                    f.write(f"{w[i]:.6e}\n")
            
            return True
        
        except Exception as e:
            print(f"Error writing VTK file: {e}")
            return False
    
    @staticmethod
    def _write_pvd_file(filename, pvd_data):
        """Write PVD (ParaView Data collection) time series metadata."""
        try:
            with open(filename, 'w') as f:
                f.write('<?xml version="1.0"?>\n')
                f.write('<VTKFile type="Collection" version="0.1">\n')
                f.write('  <Collection>\n')
                
                for time, vtk_file in pvd_data:
                    # Use relative path if in same directory
                    rel_path = os.path.basename(vtk_file)
                    f.write(f'    <DataSet timestep="{time:.6f}" group="" '
                           f'part="0" file="{rel_path}"/>\n')
                
                f.write('  </Collection>\n')
                f.write('</VTKFile>\n')
            
            return True
        
        except Exception as e:
            print(f"Error writing PVD file: {e}")
            return False


def main():
    """Main entry point for BTS to VTK conversion."""
    if len(sys.argv) < 3:
        print("Usage: python bts_to_vtk.py <input.bts> <output.vtk>")
        print("       python bts_to_vtk.py <input.bts> <output_base> --time-series")
        print("")
        print("Options:")
        print("  --time-series : Write all time steps as separate VTK files + PVD collection")
        print("  --time-step N : Write only time step N (0-based)")
        sys.exit(1)
    
    bts_file = sys.argv[1]
    output_file = sys.argv[2]
    
    # Parse options
    time_series = '--time-series' in sys.argv
    time_step = 0
    
    if '--time-step' in sys.argv:
        idx = sys.argv.index('--time-step')
        if idx + 1 < len(sys.argv):
            time_step = int(sys.argv[idx + 1])
    
    # Check input file exists
    if not os.path.exists(bts_file):
        print(f"Error: BTS file not found: {bts_file}")
        sys.exit(1)
    
    # Read BTS file
    print(f"Reading BTS file: {bts_file}")
    reader = BTSReader(bts_file)
    
    if not reader.read():
        print("Failed to read BTS file")
        sys.exit(1)
    
    # Write VTK file(s)
    print(f"\nConverting to VTK format...")
    
    if time_series:
        print(f"Writing time series to: {output_file}_*.vtk")
        if not VTKWriter.write_time_series(output_file, reader):
            print("Failed to write VTK files")
            sys.exit(1)
    else:
        print(f"Writing time step {time_step} to: {output_file}")
        if not VTKWriter.write_structured_grid(output_file, reader, time_step):
            print("Failed to write VTK file")
            sys.exit(1)
    
    print("\nConversion successful!")
    print(f"VTK file(s) ready for visualization in ParaView or VisIt")


if __name__ == '__main__':
    main()
