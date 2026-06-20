#!/usr/bin/env python3
"""
netcdf_to_windfield.py

Converts 3D wind fields from external meteorological NetCDF files (e.g. WRF or GFS outputs)
to a 3D windfield CSV file (X Y Z U V W) compatible with the mass-consistent wind solver.
Includes support for terrain-aware horizontal/vertical interpolation and time interpolation.
"""

import os
import sys
import argparse
import numpy as np
import netCDF4 as nc

def parse_inputs_file(inputs_path):
    """Parse inputs.i file to extract grid settings."""
    params = {}
    if not os.path.exists(inputs_path):
        print(f"WARNING: Inputs file {inputs_path} does not exist.")
        return params
    
    with open(inputs_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if '=' in line:
                key, val = line.split('=', 1)
                key = key.strip()
                val = val.strip()
                params[key] = val
    return params

def read_terrain_csv(terrain_path):
    """Read terrain.csv file and return numpy array of points."""
    if not terrain_path or not os.path.exists(terrain_path):
        return None
    
    pts = []
    with open(terrain_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.replace(',', ' ').split()
            if len(parts) >= 3:
                try:
                    pts.append([float(p) for p in parts[:3]])
                except ValueError:
                    continue
    return np.array(pts) if pts else None

def idw_terrain_2d(xq, yq, x_terr, y_terr, z_terr, k=6):
    """Interpolate terrain height at target points using 2D IDW."""
    n = len(x_terr)
    k = min(k, n)
    
    # Vectorized computation of squared distances
    dx = x_terr - xq
    dy = y_terr - yq
    d2 = dx*dx + dy*dy
    
    # Get indices of k nearest neighbors
    nearest_idx = np.argpartition(d2, k)[:k]
    d2_near = d2[nearest_idx]
    
    # Check for exact matches
    exact_match = d2_near < 1e-12
    if np.any(exact_match):
        return z_terr[nearest_idx[exact_match][0]]
        
    w = 1.0 / d2_near
    wsum = np.sum(w)
    zval = np.sum(w * z_terr[nearest_idx])
    return zval / wsum

def idw_velocity_3d(xq, yq, zq, x_src, y_s_grid, z_s_grid, u_grid, v_grid, w_grid, k=6, gamma=1.0):
    """Compute 3D IDW velocity at query point (xq, yq, zq) from source grid points."""
    # Ensure x and y match the 3D shape of z
    if x_src.shape != z_s_grid.shape:
        nz_levels = z_s_grid.shape[0]
        x_3d = np.repeat(np.expand_dims(x_src, axis=0), nz_levels, axis=0)
        y_3d = np.repeat(np.expand_dims(y_s_grid, axis=0), nz_levels, axis=0)
    else:
        x_3d = x_src
        y_3d = y_s_grid

    # Flatten the grids for IDW
    x_flat = x_3d.flatten()
    y_flat = y_3d.flatten()
    z_flat = z_s_grid.flatten()
    u_flat = u_grid.flatten()
    v_flat = v_grid.flatten()
    w_flat = w_grid.flatten()
    
    n = len(x_flat)
    k = min(k, n)
    
    dx = x_flat - xq
    dy = y_flat - yq
    dz = z_flat - zq
    g_dz = gamma * dz
    d2 = dx*dx + dy*dy + g_dz*g_dz
    
    nearest_idx = np.argpartition(d2, k)[:k]
    d2_near = d2[nearest_idx]
    
    exact_match = d2_near < 1e-12
    if np.any(exact_match):
        idx = nearest_idx[exact_match][0]
        return u_flat[idx], v_flat[idx], w_flat[idx]
        
    w = 1.0 / d2_near
    wsum = np.sum(w)
    u_val = np.sum(w * u_flat[nearest_idx]) / wsum
    v_val = np.sum(w * v_flat[nearest_idx]) / wsum
    w_val = np.sum(w * w_flat[nearest_idx]) / wsum
    return u_val, v_val, w_val

def extract_netcdf_data(nc_file):
    """
    Extract coordinates, terrain, and velocity fields from a NetCDF file.
    Supports WRF standard output and simple Cartesian coordinate formats.
    """
    ds = nc.Dataset(nc_file)
    
    # Detect format
    is_wrf = 'XLONG' in ds.variables or 'XLAT' in ds.variables
    
    time_coords = None
    if 'time' in ds.variables:
        time_coords = ds.variables['time'][:]
    elif 'Times' in ds.variables:
        # Convert WRF character times to indices/seconds if needed
        time_coords = np.arange(len(ds.dimensions['Time']))
    else:
        time_coords = np.array([0.0])
        
    if is_wrf:
        # WRF formats
        lon = ds.variables['XLONG'][0, :, :]
        lat = ds.variables['XLAT'][0, :, :]
        
        # Center of coordinates as reference point for projection
        lon0 = np.mean(lon)
        lat0 = np.mean(lat)
        
        # Simple projection: 1 degree ≈ 111 km
        lat_rad = np.radians(lat0)
        x_src = (lon - lon0) * 111000.0 * np.cos(lat_rad)
        y_src = (lat - lat0) * 111000.0
        
        # Terrain HGT
        hgt = ds.variables['HGT'][0, :, :]
        
        # Geopotential height at staggered/unstaggered levels
        ph = ds.variables['PH'][:]
        phb = ds.variables['PHB'][:]
        geopot_h = (ph + phb) / 9.81  # staggered in z
        
        # Average geopotential height to unstaggered levels
        z_src = 0.5 * (geopot_h[:, :-1, :, :] + geopot_h[:, 1:, :, :])
        
        # Winds
        u = ds.variables['U'][:]  # staggered in x
        v = ds.variables['V'][:]  # staggered in y
        w = ds.variables['W'][:]  # staggered in z
        
        # Unstagger wind components
        u_unstag = 0.5 * (u[:, :, :, :-1] + u[:, :, :, 1:])
        v_unstag = 0.5 * (v[:, :, :-1, :] + v[:, :, 1:, :])
        w_unstag = 0.5 * (w[:, :-1, :, :] + w[:, 1:, :, :])
        
        # Return fields for time step 0
        ds.close()
        return time_coords, x_src, y_src, z_src, hgt, u_unstag, v_unstag, w_unstag
        
    else:
        # Simple Cartesian or generic format
        # Expecting coordinates: x, y, z (or height)
        x = ds.variables['x'][:] if 'x' in ds.variables else np.array([0.0])
        y = ds.variables['y'][:] if 'y' in ds.variables else np.array([0.0])
        z = ds.variables['z'][:] if 'z' in ds.variables else np.array([0.0])
        
        # Convert 1D coordinates to grids
        if len(x.shape) == 1 and len(y.shape) == 1:
            x_grid, y_grid = np.meshgrid(x, y)
        else:
            x_grid, y_grid = x, y
            
        # Terrain
        hgt = ds.variables['HGT'][:] if 'HGT' in ds.variables else (ds.variables['terrain'][:] if 'terrain' in ds.variables else np.zeros_like(x_grid))
        
        # Construct 3D coordinates
        if len(z.shape) == 1:
            z_src = np.zeros((len(time_coords), len(z), x_grid.shape[0], x_grid.shape[1]))
            for t in range(len(time_coords)):
                for k in range(len(z)):
                    # Terrain following coordinate z_src = hgt + z[k]
                    z_src[t, k, :, :] = hgt + z[k]
        else:
            z_src = z
            if len(z_src.shape) == 3:
                z_src = np.expand_dims(z_src, axis=0) # add time dimension
                
        # Winds
        u = ds.variables['U'][:]
        v = ds.variables['V'][:]
        w = ds.variables['W'][:] if 'W' in ds.variables else np.zeros_like(u)
        
        # Ensure 4D (Time, Z, Y, X)
        if len(u.shape) == 3:
            u = np.expand_dims(u, axis=0)
            v = np.expand_dims(v, axis=0)
            w = np.expand_dims(w, axis=0)
            
        ds.close()
        return time_coords, x_grid, y_grid, z_src, hgt, u, v, w

def main():
    parser = argparse.ArgumentParser(description="Convert NetCDF meteorological wind data to terrain-aware CSV windfield")
    parser.add_argument("--nc-files", nargs="+", help="One or more NetCDF files to parse")
    parser.add_argument("--file-list", help="A text file containing a list of NetCDF files, one per line")
    parser.add_argument("--inputs", required=True, help="Path to inputs.i solver configuration file")
    parser.add_argument("--output", default="windfield.csv", help="Output windfield CSV file")
    parser.add_argument("--time", type=float, default=0.0, help="Target time in seconds for interpolation")
    parser.add_argument("--terrain-file", help="Explicit terrain file to override what's in inputs.i")
    parser.add_argument("--idw-gamma", type=float, help="Anisotropic IDW scaling parameter for the vertical axis")
    
    args = parser.parse_args()
    
    # 1. Determine list of NetCDF files
    nc_files = []
    if args.nc_files:
        nc_files.extend(args.nc_files)
    if args.file_list:
        with open(args.file_list, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#'):
                    nc_files.append(line)
                    
    if not nc_files:
        print("ERROR: No NetCDF input files specified.")
        return 1
        
    print(f"Processing {len(nc_files)} NetCDF file(s)...")
    
    # 2. Parse inputs.i to determine the solver's target grid
    params = parse_inputs_file(args.inputs)
    terrain_file_name = args.terrain_file or params.get("terrain_file", "terrain.csv")
    
    # If the path is relative, resolve it relative to inputs file folder
    inputs_dir = os.path.dirname(os.path.abspath(args.inputs))
    if not os.path.isabs(terrain_file_name):
        terrain_path = os.path.join(inputs_dir, terrain_file_name)
    else:
        terrain_path = terrain_file_name
        
    print(f"Reading target terrain from {terrain_path}...")
    terrain_pts = read_terrain_csv(terrain_path)
    if terrain_pts is None:
        print(f"ERROR: Could not read terrain file at {terrain_path}")
        return 1
        
    x_terr = terrain_pts[:, 0]
    y_terr = terrain_pts[:, 1]
    z_terr = terrain_pts[:, 2]
    
    x_lo, x_hi = np.min(x_terr), np.max(x_terr)
    y_lo, y_hi = np.min(y_terr), np.max(y_terr)
    
    dx_req = float(params.get("dx", 30.0))
    dy_req = float(params.get("dy", 30.0))
    dz_req = float(params.get("dz", 30.0))
    domain_height = float(params.get("domain_height", 300.0))
    idw_gamma = args.idw_gamma if args.idw_gamma is not None else float(params.get("idw_gamma", 1.0))
    
    # Reconstruct AMReX grid sizing
    nx = max(1, int(np.round((x_hi - x_lo) / dx_req)))
    ny = max(1, int(np.round((y_hi - y_lo) / dy_req)))
    
    dx = (x_hi - x_lo) / nx
    dy = (y_hi - y_lo) / ny
    
    # Target terrain height at the regular structured grid
    terrain_h = np.zeros(nx * ny)
    for j in range(ny):
        yc = y_lo + (j + 0.5) * dy
        for i in range(nx):
            xc = x_lo + (i + 0.5) * dx
            terrain_h[j * nx + i] = idw_terrain_2d(xc, yc, x_terr, y_terr, z_terr)
            
    zs_min = np.min(terrain_h)
    zs_max = np.max(terrain_h)
    
    # Estimate buildings or use zs_max as obs_max
    obs_max = zs_max
    # Check if a building file is in inputs.i
    bldg_file = params.get("building_file", "")
    if bldg_file:
        if not os.path.isabs(bldg_file):
            bldg_path = os.path.join(inputs_dir, bldg_file)
        else:
            bldg_path = bldg_file
        
        if os.path.exists(bldg_path):
            print(f"Reading buildings from {bldg_path} to compute domain height...")
            with open(bldg_path, 'r') as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith('#'):
                        continue
                    parts = line.replace(',', ' ').split()
                    if len(parts) >= 6:
                        try:
                            bz2 = float(parts[5])
                            if bz2 > obs_max:
                                obs_max = bz2
                        except ValueError:
                            continue

    z_lo = zs_min
    z_hi = obs_max + domain_height
    nz = max(1, int(np.round((z_hi - z_lo) / dz_req)))
    dz = (z_hi - z_lo) / nz
    
    print(f"Target solver grid reconstructed successfully:")
    print(f"  Dimensions: {nx} x {ny} x {nz}")
    print(f"  Spacings: dx={dx:.2f} m, dy={dy:.2f} m, dz={dz:.2f} m")
    print(f"  Elevation range: [{z_lo:.2f}, {z_hi:.2f}] m")
    
    # 3. Parse all NetCDF datasets and extract their fields
    time_instances = []
    
    for nc_file in nc_files:
        if not os.path.isabs(nc_file):
            nc_path = os.path.join(inputs_dir, nc_file)
        else:
            nc_path = nc_file
            
        if not os.path.exists(nc_path):
            print(f"ERROR: NetCDF file {nc_path} not found.")
            return 1
            
        print(f"Reading {nc_file}...")
        times, x_src, y_src, z_src, hgt_src, u, v, w = extract_netcdf_data(nc_path)
        
        for t_idx, t_val in enumerate(times):
            time_instances.append({
                'time': t_val,
                'x_src': x_src,
                'y_src': y_src,
                'z_src': z_src[t_idx, :, :, :],
                'hgt_src': hgt_src,
                'u': u[t_idx, :, :, :],
                'v': v[t_idx, :, :, :],
                'w': w[t_idx, :, :, :]
            })
            
    # Sort time instances by time value
    time_instances.sort(key=lambda item: item['time'])
    
    # 4. Interpolate in time to find the two closest datasets
    t_target = args.time
    if len(time_instances) == 1:
        # Only one time instance, use it directly
        inst_1 = time_instances[0]
        inst_2 = time_instances[0]
        wt1, wt2 = 0.5, 0.5
        print(f"Using single time instance at t = {inst_1['time']} s")
    else:
        # Find surrounding time instances
        times_array = np.array([item['time'] for item in time_instances])
        print(f"Available NetCDF times: {times_array} s")
        
        if t_target <= times_array[0]:
            inst_1 = time_instances[0]
            inst_2 = time_instances[0]
            wt1, wt2 = 0.5, 0.5
            print(f"Target time {t_target} s is before/at first time; clamping to t = {inst_1['time']} s")
        elif t_target >= times_array[-1]:
            inst_1 = time_instances[-1]
            inst_2 = time_instances[-1]
            wt1, wt2 = 0.5, 0.5
            print(f"Target time {t_target} s is after/at last time; clamping to t = {inst_1['time']} s")
        else:
            idx2 = np.searchsorted(times_array, t_target)
            idx1 = idx2 - 1
            inst_1 = time_instances[idx1]
            inst_2 = time_instances[idx2]
            
            # Compute interpolation weights
            t1, t2 = inst_1['time'], inst_2['time']
            wt2 = (t_target - t1) / (t2 - t1)
            wt1 = 1.0 - wt2
            print(f"Interpolating in time: {wt1:.3f} * t={t1} s + {wt2:.3f} * t={t2} s to target t={t_target} s")
            
    # 5. Perform terrain-aware 3D spatial interpolation
    # Output arrays
    out_lines = []
    
    print("Performing terrain-aware horizontal and vertical interpolation to target grid...")
    
    # For efficiency, we perform interpolation on the source grid to create the final wind field.
    # At each query point, we do terrain-aware mapping.
    for k in range(nz):
        zc = z_lo + (k + 0.5) * dz
        for j in range(ny):
            yc = y_lo + (j + 0.5) * dy
            for i in range(nx):
                xc = x_lo + (i + 0.5) * dx
                
                # Height above target terrain
                h_t = terrain_h[j * nx + i]
                z_agl = zc - h_t
                
                if z_agl <= 0.0:
                    # Point is inside or below terrain, set wind to 0
                    out_lines.append(f"{xc:.4f} {yc:.4f} {zc:.4f} 0.0000 0.0000 0.0000\n")
                    continue
                
                # Perform IDW horizontal interpolation on the source datasets to find source terrain height at (xc, yc)
                # For inst_1
                hgt_src_near_1 = idw_terrain_2d(xc, yc, inst_1['x_src'].flatten(), inst_1['y_src'].flatten(), inst_1['hgt_src'].flatten())
                # For inst_2
                hgt_src_near_2 = idw_terrain_2d(xc, yc, inst_2['x_src'].flatten(), inst_2['y_src'].flatten(), inst_2['hgt_src'].flatten())
                
                # Source terrain-aware absolute heights
                z_src_1_req = hgt_src_near_1 + z_agl
                z_src_2_req = hgt_src_near_2 + z_agl
                
                # Interpolate wind velocities horizontally/vertically at the requested source coordinates
                # We can do 3D IDW directly over a local neighborhood around (xc, yc, z_src_req)
                u1, v1, w1_vel = idw_velocity_3d(xc, yc, z_src_1_req, inst_1['x_src'], inst_1['y_src'], inst_1['z_src'], inst_1['u'], inst_1['v'], inst_1['w'], gamma=idw_gamma)
                
                u2, v2, w2_vel = idw_velocity_3d(xc, yc, z_src_2_req, inst_2['x_src'], inst_2['y_src'], inst_2['z_src'], inst_2['u'], inst_2['v'], inst_2['w'], gamma=idw_gamma)
                
                # Combine times
                u_interp = wt1 * u1 + wt2 * u2
                v_interp = wt1 * v1 + wt2 * v2
                w_interp = wt1 * w1_vel + wt2 * w2_vel
                
                out_lines.append(f"{xc:.4f} {yc:.4f} {zc:.4f} {u_interp:.4f} {v_interp:.4f} {w_interp:.4f}\n")
                
    # 6. Write to output CSV file
    out_file_path = args.output
    if not os.path.isabs(out_file_path):
        out_file_path = os.path.join(inputs_dir, out_file_path)
        
    print(f"Writing interpolated 3D wind field to {out_file_path}...")
    with open(out_file_path, 'w') as f:
        f.write("# Mass-Consistent Wind Solver 3D Wind Field File\n")
        f.write(f"# Target inputs: {args.inputs}\n")
        f.write(f"# Interpolated at target time: {t_target} s\n")
        f.write("# Format: X[m] Y[m] Z[m] U[m/s] V[m/s] W[m/s]\n")
        f.writelines(out_lines)
        
    print("Successfully completed netcdf to windfield conversion!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
