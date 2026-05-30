#!/usr/bin/env python3
"""
hrrr_to_surface_data.py - Extract surface parameters from HRRR GRIB2 files
and convert to surface_data.csv format for the mass-consistent wind solver.

HRRR provides gridded analysis at 3km resolution with surface parameters:
- Friction velocity (USTAR)
- Roughness length (SFCR or can be diagnosed)
- 10m wind components (UGRD:10 m, VGRD:10 m)
- Surface elevation (HGT)

Output format: X Y Z USTAR Z0 U10 V10
where coordinates can be in UTM or local projection.

Requirements:
    pip install cfgrib xarray numpy

Optional for direct HRRR download:
    pip install herbie-data

Usage:
    # From local GRIB2 file
    python3 hrrr_to_surface_data.py --grib hrrr.t00z.wrfsfcf00.grib2 \
        --output surface_data.csv --bbox xmin xmax ymin ymax

    # Download and process HRRR
    python3 hrrr_to_surface_data.py --date 2024-01-15 --hour 12 \
        --output surface_data.csv --bbox xmin xmax ymin ymax

Example for a 10km x 10km domain centered at (lon=-105.0, lat=40.0):
    python3 hrrr_to_surface_data.py --grib hrrr.grib2 \
        --output surface_data.csv \
        --center-lonlat -105.0 40.0 --domain-size 10000
"""

import argparse
import sys
import numpy as np

def main():
    parser = argparse.ArgumentParser(
        description="Extract HRRR surface parameters for mass-consistent wind solver",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--grib", help="Input HRRR GRIB2 file")
    parser.add_argument("--date", help="HRRR date (YYYY-MM-DD) for auto-download")
    parser.add_argument("--hour", type=int, help="HRRR forecast hour (0-23)")
    parser.add_argument("--output", default="surface_data.csv",
                        help="Output surface_data.csv file")
    parser.add_argument("--bbox", nargs=4, type=float, metavar=("XMIN", "XMAX", "YMIN", "YMAX"),
                        help="Bounding box in output coordinates (meters)")
    parser.add_argument("--center-lonlat", nargs=2, type=float, metavar=("LON", "LAT"),
                        help="Domain center (longitude, latitude)")
    parser.add_argument("--domain-size", type=float, default=10000.0,
                        help="Square domain size in meters (default: 10000)")
    parser.add_argument("--subsample", type=int, default=1,
                        help="Subsample HRRR grid by this factor (default: 1 = all points)")
    
    args = parser.parse_args()
    
    # Check dependencies
    try:
        import xarray as xr
        import cfgrib
    except ImportError:
        print("ERROR: Missing required packages. Install with:")
        print("  pip install xarray cfgrib")
        sys.exit(1)
    
    # Load HRRR data
    if args.grib:
        print(f"Reading HRRR data from {args.grib}...")
        ds = xr.open_dataset(args.grib, engine='cfgrib')
    elif args.date and args.hour is not None:
        try:
            from herbie import Herbie
        except ImportError:
            print("ERROR: herbie-data required for auto-download. Install with:")
            print("  pip install herbie-data")
            sys.exit(1)
        print(f"Downloading HRRR for {args.date} hour {args.hour}...")
        H = Herbie(args.date, model='hrrr', product='sfc', fxx=args.hour)
        ds = H.xarray('UGRD:10 m|VGRD:10 m|FRICV|HGT:surface')
    else:
        parser.error("Must provide either --grib or both --date and --hour")
    
    # Extract fields
    print("Extracting surface parameters...")
    
    # 10m winds
    u10 = ds['u10'].values if 'u10' in ds else ds['UGRD_10maboveground'].values
    v10 = ds['v10'].values if 'v10' in ds else ds['VGRD_10maboveground'].values
    
    # Friction velocity (USTAR) - may be stored as FRICV
    if 'fricv' in ds:
        ustar = ds['fricv'].values
    elif 'FRICV_surface' in ds:
        ustar = ds['FRICV_surface'].values
    else:
        # Diagnose from 10m wind if not available
        print("WARNING: USTAR not found in GRIB, diagnosing from 10m wind...")
        z0 = 0.1  # assume default roughness
        kappa = 0.41
        speed_10m = np.sqrt(u10**2 + v10**2)
        ustar = kappa * speed_10m / np.log((10.0 + z0) / z0)
    
    # Roughness length - may need to be prescribed or diagnosed
    # HRRR doesn't always include z0 directly
    z0 = 0.1 * np.ones_like(ustar)  # default 0.1 m
    print("WARNING: Using default z0=0.1 m (not extracted from HRRR)")
    
    # Terrain elevation
    if 'orog' in ds:
        z = ds['orog'].values
    elif 'HGT_surface' in ds:
        z = ds['HGT_surface'].values
    else:
        z = np.zeros_like(ustar)
        print("WARNING: Terrain elevation not found, using z=0")
    
    # Get coordinates
    lats = ds['latitude'].values
    lons = ds['longitude'].values
    
    # Convert to local coordinates
    # For simplicity, use a local tangent plane projection
    # In production, use proper UTM or projection
    if args.center_lonlat:
        lon0, lat0 = args.center_lonlat
        # Simple equirectangular approximation (good for small domains)
        R_earth = 6371000.0  # meters
        lat_rad = np.radians(lat0)
        x = R_earth * np.radians(lons - lon0) * np.cos(lat_rad)
        y = R_earth * np.radians(lats - lat0)
        
        # Define bbox around center
        half_size = args.domain_size / 2.0
        xmin, xmax = -half_size, half_size
        ymin, ymax = -half_size, half_size
    elif args.bbox:
        xmin, xmax, ymin, ymax = args.bbox
        # Would need proper projection here
        print("WARNING: Custom bbox requires proper coordinate projection")
        print("Using simple equirectangular projection from domain center")
        lon0 = np.mean([lons.min(), lons.max()])
        lat0 = np.mean([lats.min(), lats.max()])
        R_earth = 6371000.0
        lat_rad = np.radians(lat0)
        x = R_earth * np.radians(lons - lon0) * np.cos(lat_rad)
        y = R_earth * np.radians(lats - lat0)
    else:
        parser.error("Must provide either --bbox or --center-lonlat")
    
    # Filter to domain
    mask = (x >= xmin) & (x <= xmax) & (y >= ymin) & (y <= ymax)
    
    # Subsample if requested
    if args.subsample > 1:
        mask_sub = np.zeros_like(mask, dtype=bool)
        mask_sub[::args.subsample, ::args.subsample] = True
        mask = mask & mask_sub
    
    # Extract points
    x_pts = x[mask]
    y_pts = y[mask]
    z_pts = z[mask]
    ustar_pts = ustar[mask]
    z0_pts = z0[mask]
    u10_pts = u10[mask]
    v10_pts = v10[mask]
    
    print(f"Extracted {len(x_pts)} surface data points")
    
    # Write output
    print(f"Writing to {args.output}...")
    with open(args.output, 'w') as f:
        f.write("# HRRR surface parameters for mass-consistent wind solver\n")
        f.write("# Format: X[m] Y[m] Z[m] USTAR[m/s] Z0[m] U10[m/s] V10[m/s]\n")
        for i in range(len(x_pts)):
            f.write(f"{x_pts[i]:.2f} {y_pts[i]:.2f} {z_pts[i]:.2f} "
                   f"{ustar_pts[i]:.4f} {z0_pts[i]:.4f} "
                   f"{u10_pts[i]:.4f} {v10_pts[i]:.4f}\n")
    
    print(f"Done! Output written to {args.output}")
    print(f"\nUse in wind_solver with:")
    print(f"  init_mode = surface_data")
    print(f"  surface_data_file = {args.output}")

if __name__ == "__main__":
    main()
