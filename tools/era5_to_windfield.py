#!/usr/bin/env python3
"""
era5_to_windfield.py

Converts ERA5 3D atmospheric reanalysis fields (U, V wind components and geopotential Z)
from geographic coordinates (latitude, longitude, pressure levels) into a Cartesian
NetCDF format compatible with the terrain-aware `netcdf_to_windfield.py` ingestion pipeline.

Additionally support:
- Computing and printing vertical temperature profile averages.
- Extracting 3D temperature (T), 3D specific/relative humidity (Q/RH), and 2D surface fields:
  roughness length (Z0), friction velocity (USTAR), and boundary layer height (BLH)
  which are used/supported by the wind solver and assimilation tools.
"""

import os
import sys
import argparse
import numpy as np
import netCDF4 as nc

def detect_variable(ds, possible_names):
    """Robustly detect a variable name from a list of possible names."""
    for name in possible_names:
        if name in ds.variables:
            return name
    return None

def main():
    parser = argparse.ArgumentParser(
        description="Convert ERA5 NetCDF meteorological data to Cartesian format for the wind solver",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--input", "-i", required=True, help="Input ERA5 NetCDF file")
    parser.add_argument("--output", "-o", default="formatted_era5.nc", help="Output Cartesian NetCDF file")
    parser.add_argument("--center-lonlat", nargs=2, type=float, metavar=("LON", "LAT"),
                        help="Domain center (longitude, latitude) for coordinate projection. Defaults to mean of data.")
    parser.add_argument("--lat-range", nargs=2, type=float, metavar=("LAT_MIN", "LAT_MAX"),
                        help="Optional latitude range to crop input data")
    parser.add_argument("--lon-range", nargs=2, type=float, metavar=("LON_MIN", "LON_MAX"),
                        help="Optional longitude range to crop input data")
    parser.add_argument("--zero-w", action="store_true", help="Force vertical wind component W to be zero")

    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"ERROR: Input file {args.input} not found.")
        sys.exit(1)

    print(f"Reading ERA5 dataset from {args.input}...")
    ds_in = nc.Dataset(args.input, "r")

    # 1. Detect dimensions and coordinates
    lon_name = detect_variable(ds_in, ["longitude", "lon", "LONGITUDE", "LON", "lons"])
    lat_name = detect_variable(ds_in, ["latitude", "lat", "LATITUDE", "LAT", "lats"])
    level_name = detect_variable(ds_in, ["level", "plevel", "lev", "LEVEL", "pressure_level"])
    time_name = detect_variable(ds_in, ["time", "valid_time", "t", "TIME"])

    if not lon_name or not lat_name:
        print("ERROR: Could not detect longitude or latitude coordinates in dataset.")
        print(f"Available variables: {list(ds_in.variables.keys())}")
        sys.exit(1)

    print(f"Detected coordinates: lon='{lon_name}', lat='{lat_name}', level='{level_name}', time='{time_name}'")

    # Extract coordinates
    lons = ds_in.variables[lon_name][:]
    lats = ds_in.variables[lat_name][:]
    levels = ds_in.variables[level_name][:] if level_name else np.array([1000.0])
    times = ds_in.variables[time_name][:] if time_name else np.array([0.0])

    # Convert masked arrays or list arrays to standard numpy floats
    lons = np.array(lons, dtype=np.float32)
    lats = np.array(lats, dtype=np.float32)
    levels = np.array(levels, dtype=np.float32)
    times = np.array(times, dtype=np.float32)

    # Clean up longitudes if they are on 0-360 scale instead of -180 to 180
    if np.max(lons) > 180.0:
        print("Adjusting longitude range from [0, 360] to [-180, 180]...")
        lons = np.where(lons > 180.0, lons - 360.0, lons)

    # 2. Crop/Filter by lat/lon ranges if specified
    lat_mask = np.ones_like(lats, dtype=bool)
    if args.lat_range:
        lat_min, lat_max = sorted(args.lat_range)
        lat_mask = (lats >= lat_min) & (lats <= lat_max)
    
    lon_mask = np.ones_like(lons, dtype=bool)
    if args.lon_range:
        lon_min, lon_max = sorted(args.lon_range)
        lon_mask = (lons >= lon_min) & (lons <= lon_max)

    if not np.any(lat_mask) or not np.any(lon_mask):
        print("ERROR: No grid points found within specified latitude/longitude ranges.")
        sys.exit(1)

    # Apply crop to coordinates
    lons_cropped = lons[lon_mask]
    lats_cropped = lats[lat_mask]

    # Ensure cropped longitudes and latitudes are strictly sorted in increasing order
    sort_idx = np.argsort(lons_cropped)
    lons_cropped = lons_cropped[sort_idx]
    
    lat_sort_idx = np.argsort(lats_cropped)
    lats_cropped = lats_cropped[lat_sort_idx]

    print(f"Grid shape after cropping: lats={len(lats_cropped)}, lons={len(lons_cropped)}")

    # Determine center longitude and latitude for projection
    if args.center_lonlat:
        lon0, lat0 = args.center_lonlat
    else:
        lon0 = float(np.mean(lons_cropped))
        lat0 = float(np.mean(lats_cropped))
    print(f"Using projection reference point: center_lon={lon0:.4f}, center_lat={lat0:.4f}")

    # Project to Cartesian coordinates
    lon_grid, lat_grid = np.meshgrid(lons_cropped, lats_cropped)
    lat_rad = np.radians(lat0)
    R_earth = 6371000.0  # Earth's radius in meters
    x_grid = R_earth * np.radians(lon_grid - lon0) * np.cos(lat_rad)
    y_grid = R_earth * np.radians(lat_grid - lat0)

    # 3. Robustly detect and extract physical variables (U, V, Z)
    u_name = detect_variable(ds_in, ["u", "U", "u_component_of_wind", "UGRD"])
    v_name = detect_variable(ds_in, ["v", "V", "v_component_of_wind", "VGRD"])
    z_name = detect_variable(ds_in, ["z", "Z", "geopotential", "HGT"])

    if not u_name or not v_name or not z_name:
        print("ERROR: Could not detect wind components (U, V) or geopotential (Z) in dataset.")
        print(f"Available variables: {list(ds_in.variables.keys())}")
        sys.exit(1)

    print(f"Detected variables: U='{u_name}', V='{v_name}', Z='{z_name}'")

    # Read variables
    u_var = ds_in.variables[u_name]
    v_var = ds_in.variables[v_name]
    z_var = ds_in.variables[z_name]

    # Crop indices
    lat_indices = np.where(lat_mask)[0]
    lon_indices = np.where(lon_mask)[0]

    # Helper function to crop and read variable, then sort along latitude and longitude axes
    def read_cropped_and_sorted_var(var):
        dims = var.dimensions
        slice_list = []
        for d in dims:
            if d == time_name:
                slice_list.append(slice(None))
            elif d == level_name:
                slice_list.append(slice(None))
            elif d == lat_name:
                slice_list.append(lat_indices)
            elif d == lon_name:
                slice_list.append(lon_indices)
            else:
                slice_list.append(slice(None))
        
        data = var[tuple(slice_list)]
        if hasattr(data, "filled"):
            data = data.filled(np.nan)
        
        # Centralized sorting logic along latitude and longitude dimensions
        if lat_name in dims:
            lat_dim_idx = dims.index(lat_name)
            data = np.take(data, lat_sort_idx, axis=lat_dim_idx)
            
        if lon_name in dims:
            lon_dim_idx = dims.index(lon_name)
            data = np.take(data, sort_idx, axis=lon_dim_idx)
            
        return data

    print("Extracting and cropping variables...")
    u_data = np.array(read_cropped_and_sorted_var(u_var), dtype=np.float32)
    v_data = np.array(read_cropped_and_sorted_var(v_var), dtype=np.float32)
    z_data = np.array(read_cropped_and_sorted_var(z_var), dtype=np.float32)

    # 4. Convert pressure level geopotential (m^2/s^2) to geopotential height (meters)
    print("Converting geopotential to geopotential height above sea level...")
    g_standard = 9.80665
    z_height = z_data / g_standard

    # Enforce 4D (Time, Level, Lat, Lon)
    def enforce_4d(data, var_dims):
        has_time = time_name in var_dims if time_name else False
        has_level = level_name in var_dims if level_name else False
        if not has_level:
            idx = 1 if has_time else 0
            data = np.expand_dims(data, axis=idx)
        if not has_time:
            data = np.expand_dims(data, axis=0)
        return data

    u_data = enforce_4d(u_data, u_var.dimensions)
    v_data = enforce_4d(v_data, v_var.dimensions)
    z_height = enforce_4d(z_height, z_var.dimensions)

    # Detect optional vertical velocity W
    w_name = None if args.zero_w else detect_variable(ds_in, ["w", "W", "vertical_velocity", "VVEL"])
    if w_name:
        print(f"Reading vertical wind component from '{w_name}'...")
        w_var = ds_in.variables[w_name]
        w_data = np.array(read_cropped_and_sorted_var(w_var), dtype=np.float32)
        w_data = enforce_4d(w_data, w_var.dimensions)
    else:
        print("Initializing vertical velocity component W with zeros (standard for initial solver guess).")
        w_data = np.zeros_like(u_data)

    # NEW: Detect and process 3D temperature
    temp_name = detect_variable(ds_in, ["t", "T", "temperature", "temp", "TMP"])
    t_data = None
    if temp_name:
        print(f"Detected 3D Temperature variable: '{temp_name}'")
        temp_var = ds_in.variables[temp_name]
        t_raw = np.array(read_cropped_and_sorted_var(temp_var), dtype=np.float32)
        t_data = enforce_4d(t_raw, temp_var.dimensions)
        
        # Calculate and print horizontally-averaged vertical temperature profiles
        print("\n" + "="*80)
        print("                 AVERAGED VERTICAL TEMPERATURE PROFILE (ERA5)")
        print("="*80)
        for t_idx in range(u_data.shape[0]):
            time_val = times[t_idx] if len(times) > t_idx else 0.0
            print(f"\nTime Step {t_idx} (Time Value: {time_val}):")
            header_str = f" {'Level (hPa)':<12} | {'Avg Hgt (m ASL)':<15} | {'Avg Temp (Kelvin)':<16} | {'Avg Temp (Celsius)':<16}"
            print(header_str)
            print("-" * len(header_str))
            for l_idx in range(u_data.shape[1]):
                avg_temp_k = float(np.nanmean(t_data[t_idx, l_idx, :, :]))
                avg_temp_c = avg_temp_k - 273.15
                avg_hgt = float(np.nanmean(z_height[t_idx, l_idx, :, :]))
                lvl_val = levels[l_idx] if len(levels) > l_idx else 0.0
                print(f" {lvl_val:<12.1f} | {avg_hgt:<15.1f} | {avg_temp_k:<16.2f} | {avg_temp_c:<16.2f}")
        print("="*80 + "\n")
    else:
        print("WARNING: No 3D temperature variable ('t', 'temperature') found in input dataset.")

    # NEW: Detect and process 3D Humidity (Specific or Relative)
    hum_name = detect_variable(ds_in, ["q", "Q", "specific_humidity", "SPFH", "r", "R", "relative_humidity", "RH"])
    hum_data = None
    is_relative = False
    if hum_name:
        print(f"Detected 3D Humidity variable: '{hum_name}'")
        hum_var = ds_in.variables[hum_name]
        hum_raw = np.array(read_cropped_and_sorted_var(hum_var), dtype=np.float32)
        hum_data = enforce_4d(hum_raw, hum_var.dimensions)
        if "relative_humidity" in hum_name.lower() or hum_name.lower() in ["r", "rh"]:
            is_relative = True
            print("Extracted variable as Relative Humidity (RH).")
        else:
            print("Extracted variable as Specific Humidity (Q).")

    # NEW: Detect other 2D surface variables supported by solver / assimilation
    # 2D surface roughness
    sr_name = detect_variable(ds_in, ["sr", "fsr", "roughness", "roughness_length", "z0", "SFCR"])
    sr_data = None
    if sr_name:
        print(f"Detected surface roughness variable '{sr_name}'")
        sr_var = ds_in.variables[sr_name]
        sr_data = np.array(read_cropped_and_sorted_var(sr_var), dtype=np.float32)
        if len(sr_data.shape) > 2:
            sr_data = sr_data[0, ...]
            if len(sr_data.shape) > 2:
                sr_data = sr_data[0, ...]

    # 2D friction velocity
    ustar_name = detect_variable(ds_in, ["ustar", "fricv", "friction_velocity", "FRICV"])
    ustar_data = None
    if ustar_name:
        print(f"Detected friction velocity variable '{ustar_name}'")
        ustar_var = ds_in.variables[ustar_name]
        ustar_data = np.array(read_cropped_and_sorted_var(ustar_var), dtype=np.float32)
        if len(ustar_data.shape) > 2:
            ustar_data = ustar_data[0, ...]
            if len(ustar_data.shape) > 2:
                ustar_data = ustar_data[0, ...]

    # 2D boundary layer height
    blh_name = detect_variable(ds_in, ["blh", "boundary_layer_height", "mixing_height", "zi", "HPBL"])
    blh_data = None
    if blh_name:
        print(f"Detected boundary layer height variable '{blh_name}'")
        blh_var = ds_in.variables[blh_name]
        blh_data = np.array(read_cropped_and_sorted_var(blh_var), dtype=np.float32)
        if len(blh_data.shape) > 2:
            blh_data = blh_data[0, ...]
            if len(blh_data.shape) > 2:
                blh_data = blh_data[0, ...]

    # 5. Extract/Formulate Terrain Elevation (HGT)
    hgt_name = detect_variable(ds_in, ["orography", "surface_geopotential", "HGT_surface", "topo", "elevation"])
    if not hgt_name and "z" in ds_in.variables:
        z_dims = ds_in.variables["z"].dimensions
        # In a surface file, "z" is typically 2D (lat, lon) or 3D (time, lat, lon) and has no level dimension
        if level_name not in z_dims:
            hgt_name = "z"

    if hgt_name:
        print(f"Detected surface geopotential/elevation variable '{hgt_name}'")
        hgt_var = ds_in.variables[hgt_name]
        hgt_data = np.array(read_cropped_and_sorted_var(hgt_var), dtype=np.float32)
        if "geopotential" in hgt_name.lower() or hgt_name.lower() == "orography" or hgt_name.lower() == "z":
            print("Converting surface geopotential to terrain elevation (HGT)...")
            hgt_data = hgt_data / g_standard
    else:
        print("No surface elevation variable found. Setting terrain elevation HGT to 0.0.")
        hgt_data = np.zeros((len(lats_cropped), len(lons_cropped)), dtype=np.float32)

    if len(hgt_data.shape) > 2:
        hgt_data = hgt_data[0, ...]
        if len(hgt_data.shape) > 2:
            hgt_data = hgt_data[0, ...]

    ds_in.close()

    # 6. Write output Cartesian NetCDF
    print(f"Writing formatted Cartesian dataset to {args.output}...")
    ds_out = nc.Dataset(args.output, "w", format="NETCDF4")

    # Dimensions
    nt, nlev, nlat, nlon = u_data.shape
    ds_out.createDimension("time", nt)
    ds_out.createDimension("level", nlev)
    ds_out.createDimension("y", nlat)
    ds_out.createDimension("x", nlon)

    # Coordinate Variables
    time_v = ds_out.createVariable("time", "f4", ("time",))
    level_v = ds_out.createVariable("level", "f4", ("level",))
    x_v = ds_out.createVariable("x", "f4", ("y", "x"))
    y_v = ds_out.createVariable("y", "f4", ("y", "x"))

    time_v[:] = times
    level_v[:] = levels
    x_v[:, :] = x_grid
    y_v[:, :] = y_grid

    # Required Physical Variables
    hgt_v = ds_out.createVariable("HGT", "f4", ("y", "x"))
    hgt_v[:, :] = hgt_data

    u_v = ds_out.createVariable("U", "f4", ("time", "level", "y", "x"))
    v_v = ds_out.createVariable("V", "f4", ("time", "level", "y", "x"))
    w_v = ds_out.createVariable("W", "f4", ("time", "level", "y", "x"))
    z_v = ds_out.createVariable("z", "f4", ("time", "level", "y", "x"))

    u_v[:] = u_data
    v_v[:] = v_data
    w_v[:] = w_data
    z_v[:] = z_height

    # Write optional variables if detected
    if t_data is not None:
        t_v = ds_out.createVariable("T", "f4", ("time", "level", "y", "x"))
        t_v[:] = t_data
        t_v.description = "Air Temperature (Kelvin)"

    if hum_data is not None:
        v_name_out = "RH" if is_relative else "Q"
        hum_v = ds_out.createVariable(v_name_out, "f4", ("time", "level", "y", "x"))
        hum_v[:] = hum_data
        hum_v.description = "Relative Humidity (%)" if is_relative else "Specific Humidity (kg/kg)"

    if sr_data is not None:
        sr_v = ds_out.createVariable("Z0", "f4", ("y", "x"))
        sr_v[:, :] = sr_data
        sr_v.description = "Surface Roughness Length (meters)"

    if ustar_data is not None:
        ustar_v = ds_out.createVariable("USTAR", "f4", ("y", "x"))
        ustar_v[:, :] = ustar_data
        ustar_v.description = "Friction Velocity (m/s)"

    if blh_data is not None:
        blh_v = ds_out.createVariable("BLH", "f4", ("y", "x"))
        blh_v[:, :] = blh_data
        blh_v.description = "Boundary Layer Height / Mixing Depth (meters)"

    # Attributes
    ds_out.description = "ERA5 meteorological wind, geopotential and optional physical/surface fields converted to local Cartesian coordinates"
    ds_out.center_longitude = lon0
    ds_out.center_latitude = lat0

    ds_out.close()
    print("Successfully completed ERA5 conversion!")
    print(f"You can now feed {args.output} into netcdf_to_windfield.py")

if __name__ == "__main__":
    main()
