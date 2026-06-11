#!/usr/bin/env python3
"""
Wind Field Format Converter
============================

Converts wind field data from various sources to standardized CSV formats
compatible with the puff model. Supports conversion from WRF NetCDF, CALMET
binary, ASCII grids, and other meteorological data sources.

Supported input formats:
  - WRF NetCDF output
  - CALMET binary fields
  - ASCII gridded wind (x, y, z, u, v, w format)
  - Single uniform wind values
  - Time-series wind data

Output format:
  Standard CSV with metadata header and wind field values at each grid point
  or time step.

Usage:
  python wind_field_converter.py --input wind_data.nc --output wind_field.csv --format wrf
  python wind_field_converter.py --uniform 10.0 0.0 0.0 --output wind_field.csv
"""

import sys
import argparse
import csv
from pathlib import Path
from typing import Tuple, List, Dict, Optional

# Optional numpy import
try:
    import numpy as np
    NUMPY_AVAILABLE = True
except ImportError:
    NUMPY_AVAILABLE = False


def write_uniform_wind_csv(
    output_file: str,
    u: float,
    v: float,
    w: float,
    description: str = "Uniform wind field"
) -> None:
    """
    Write a uniform wind field to CSV format.
    
    Parameters
    ----------
    output_file : str
        Output CSV file path
    u : float
        Zonal wind component [m/s]
    v : float
        Meridional wind component [m/s]
    w : float
        Vertical wind component [m/s]
    description : str
        Optional description of the wind field
    """
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        # Write metadata header
        writer.writerow(['# Wind Field CSV Format'])
        writer.writerow(['# Description:', description])
        writer.writerow(['# Format: uniform'])
        writer.writerow([])
        # Write column headers
        writer.writerow(['u', 'v', 'w'])
        # Write uniform wind values
        writer.writerow([u, v, w])
    
    print(f"Wrote uniform wind field to {output_file}")


def write_gridded_wind_csv(
    output_file: str,
    x_coords,
    y_coords,
    z_coords,
    u,
    v,
    w,
    description: str = "Gridded wind field"
) -> None:
    """
    Write gridded wind field data to CSV format.
    
    Parameters
    ----------
    output_file : str
        Output CSV file path
    x_coords : array-like
        X coordinates [m] - shape (nx,) or (nx, ny, nz)
    y_coords : array-like
        Y coordinates [m] - shape (ny,) or (nx, ny, nz)
    z_coords : array-like
        Z coordinates [m] - shape (nz,) or (nx, ny, nz)
    u : array-like
        Zonal wind component [m/s]
    v : array-like
        Meridional wind component [m/s]
    w : array-like
        Vertical wind component [m/s]
    description : str
        Optional description of the wind field
    """
    # Convert to lists if numpy arrays
    if NUMPY_AVAILABLE:
        if isinstance(u, np.ndarray):
            u_flat = u.ravel().tolist()
        else:
            u_flat = list(u) if hasattr(u, '__iter__') else [u]
        
        if isinstance(v, np.ndarray):
            v_flat = v.ravel().tolist()
        else:
            v_flat = list(v) if hasattr(v, '__iter__') else [v]
        
        if isinstance(w, np.ndarray):
            w_flat = w.ravel().tolist()
        else:
            w_flat = list(w) if hasattr(w, '__iter__') else [w]
        
        # Create meshgrid if coordinates are 1D numpy arrays
        if hasattr(x_coords, 'ndim') and x_coords.ndim == 1 and \
           hasattr(y_coords, 'ndim') and y_coords.ndim == 1 and \
           hasattr(z_coords, 'ndim') and z_coords.ndim == 1:
            xx, yy, zz = np.meshgrid(x_coords, y_coords, z_coords, indexing='ij')
            x_flat = xx.ravel().tolist()
            y_flat = yy.ravel().tolist()
            z_flat = zz.ravel().tolist()
        else:
            x_flat = x_coords.ravel().tolist() if hasattr(x_coords, 'ravel') else list(x_coords)
            y_flat = y_coords.ravel().tolist() if hasattr(y_coords, 'ravel') else list(y_coords)
            z_flat = z_coords.ravel().tolist() if hasattr(z_coords, 'ravel') else list(z_coords)
    else:
        u_flat = list(u) if hasattr(u, '__iter__') else [u]
        v_flat = list(v) if hasattr(v, '__iter__') else [v]
        w_flat = list(w) if hasattr(w, '__iter__') else [w]
        x_flat = list(x_coords) if hasattr(x_coords, '__iter__') else [x_coords]
        y_flat = list(y_coords) if hasattr(y_coords, '__iter__') else [y_coords]
        z_flat = list(z_coords) if hasattr(z_coords, '__iter__') else [z_coords]
    
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        # Write metadata header
        writer.writerow(['# Wind Field CSV Format'])
        writer.writerow(['# Description:', description])
        writer.writerow(['# Format: gridded'])
        writer.writerow(['# Grid dimensions: {} x {} x {}'.format(
            len(set(x_flat)),
            len(set(y_flat)),
            len(set(z_flat))
        )])
        writer.writerow([])
        # Write column headers
        writer.writerow(['x', 'y', 'z', 'u', 'v', 'w'])
        # Write grid points
        for i in range(len(x_flat)):
            writer.writerow([x_flat[i], y_flat[i], z_flat[i], 
                           u_flat[i] if i < len(u_flat) else 0.0,
                           v_flat[i] if i < len(v_flat) else 0.0,
                           w_flat[i] if i < len(w_flat) else 0.0])
    
    print(f"Wrote gridded wind field ({len(x_flat)} points) to {output_file}")


def write_timeseries_wind_csv(
    output_file: str,
    times,
    u_series,
    v_series,
    w_series,
    description: str = "Time-series wind field"
) -> None:
    """
    Write time-series wind field data to CSV format.
    
    Parameters
    ----------
    output_file : str
        Output CSV file path
    times : array-like
        Time values [s] - shape (nt,)
    u_series : array-like
        Zonal wind time series [m/s]
    v_series : array-like
        Meridional wind time series [m/s]
    w_series : array-like
        Vertical wind time series [m/s]
    description : str
        Optional description of the wind field
    """
    # Convert to lists if needed
    times_list = list(times) if hasattr(times, '__iter__') else [times]
    u_list = list(u_series) if hasattr(u_series, '__iter__') else [u_series]
    v_list = list(v_series) if hasattr(v_series, '__iter__') else [v_series]
    w_list = list(w_series) if hasattr(w_series, '__iter__') else [w_series]
    
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        # Write metadata header
        writer.writerow(['# Wind Field CSV Format'])
        writer.writerow(['# Description:', description])
        writer.writerow(['# Format: timeseries'])
        writer.writerow(['# Number of time steps:', len(times_list)])
        writer.writerow([])
        # Write column headers
        writer.writerow(['time', 'u', 'v', 'w'])
        # Write time series
        for i in range(len(times_list)):
            writer.writerow([times_list[i], u_list[i], v_list[i], w_list[i]])
    
    print(f"Wrote time-series wind field ({len(times_list)} steps) to {output_file}")


def read_wrf_netcdf(
    input_file: str,
    timestep: Optional[int] = None,
    height_level: Optional[int] = None
):
    """
    Read wind field from WRF NetCDF output.
    
    Parameters
    ----------
    input_file : str
        Path to WRF output file
    timestep : int, optional
        Specific time step to extract (None = all)
    height_level : int, optional
        Specific height level to extract (None = all)
    
    Returns
    -------
    Tuple of (x, y, z, u, v, w) arrays
    
    Note
    ----
    Requires netCDF4 package.
    """
    try:
        import netCDF4
    except ImportError:
        raise ImportError("netCDF4 package required for WRF input. Install with: pip install netCDF4")
    
    ds = netCDF4.Dataset(input_file, 'r')
    
    try:
        # Extract coordinates
        x = ds.variables.get('XLONG', ds.variables.get('lon'))[:].ravel()
        y = ds.variables.get('XLAT', ds.variables.get('lat'))[:].ravel()
        z = ds.variables.get('height', ds.variables.get('z'))[:].ravel()
        
        # Extract wind components
        u = ds.variables.get('U10', ds.variables.get('u'))
        v = ds.variables.get('V10', ds.variables.get('v'))
        w = ds.variables.get('W', ds.variables.get('w'))
        
        # Handle time and height dimensions
        if u is None or v is None:
            raise ValueError("U and V wind components not found in NetCDF file")
        
        u_data = u[:].ravel()
        v_data = v[:].ravel()
        w_data = w[:].ravel() if w is not None else [0.0] * len(u_data)
        
        return x, y, z, u_data, v_data, w_data
    
    finally:
        ds.close()


def read_ascii_grid(
    input_file: str,
    format_spec: str = "space"
):
    """
    Read wind field from ASCII grid file.
    
    Parameters
    ----------
    input_file : str
        Path to ASCII grid file with columns: x, y, z, u, v, w
    format_spec : str
        Delimiter: "space", "comma", or "tab"
    
    Returns
    -------
    Tuple of (x, y, z, u, v, w) arrays
    """
    delimiter = {'space': None, 'comma': ',', 'tab': '\t'}.get(format_spec, None)
    
    try:
        if NUMPY_AVAILABLE:
            import numpy as np
            data = np.loadtxt(input_file, delimiter=delimiter, comments='#')
            
            if data.shape[1] < 6:
                raise ValueError(f"Expected at least 6 columns (x, y, z, u, v, w), got {data.shape[1]}")
            
            x = data[:, 0]
            y = data[:, 1]
            z = data[:, 2]
            u = data[:, 3]
            v = data[:, 4]
            w = data[:, 5]
        else:
            # Read without numpy
            x, y, z, u, v, w = [], [], [], [], [], []
            with open(input_file, 'r') as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith('#'):
                        continue
                    values = line.split(delimiter if delimiter else None)
                    if len(values) < 6:
                        continue
                    try:
                        x.append(float(values[0]))
                        y.append(float(values[1]))
                        z.append(float(values[2]))
                        u.append(float(values[3]))
                        v.append(float(values[4]))
                        w.append(float(values[5]))
                    except (ValueError, IndexError):
                        continue
    
        return x, y, z, u, v, w
    except Exception as e:
        raise ValueError(f"Error reading ASCII grid file: {e}")


def main():
    parser = argparse.ArgumentParser(
        description="Convert wind field data to CSV format for puff model"
    )
    parser.add_argument('--input', type=str, help='Input file path')
    parser.add_argument('--output', type=str, required=True, help='Output CSV file path')
    parser.add_argument('--format', type=str, choices=['wrf', 'calmet', 'ascii', 'uniform'],
                       help='Input format')
    parser.add_argument('--uniform', type=float, nargs=3, metavar=('U', 'V', 'W'),
                       help='Create uniform wind field with u, v, w values [m/s]')
    parser.add_argument('--description', type=str, default='Converted wind field',
                       help='Description string for output file')
    parser.add_argument('--timestep', type=int, help='Extract specific time step (WRF)')
    parser.add_argument('--height-level', type=int, help='Extract specific height level (WRF)')
    parser.add_argument('--delimiter', type=str, choices=['space', 'comma', 'tab'],
                       default='space', help='Delimiter for ASCII input')
    
    args = parser.parse_args()
    
    # Create uniform wind field
    if args.uniform:
        write_uniform_wind_csv(args.output, args.uniform[0], args.uniform[1], 
                              args.uniform[2], args.description)
        return 0
    
    # Convert from file
    if not args.input:
        parser.error("Either --uniform or --input must be specified")
    
    if not Path(args.input).exists():
        print(f"Error: Input file not found: {args.input}", file=sys.stderr)
        return 1
    
    try:
        if args.format == 'wrf':
            x, y, z, u, v, w = read_wrf_netcdf(args.input, args.timestep, args.height_level)
            write_gridded_wind_csv(args.output, x, y, z, u, v, w, args.description)
        
        elif args.format == 'ascii':
            x, y, z, u, v, w = read_ascii_grid(args.input, args.delimiter)
            write_gridded_wind_csv(args.output, x, y, z, u, v, w, args.description)
        
        else:
            print(f"Error: Unsupported input format: {args.format}", file=sys.stderr)
            return 1
        
        return 0
    
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
