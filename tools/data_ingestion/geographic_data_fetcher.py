#!/usr/bin/env python3
"""
geographic_data_fetcher.py - Automated Geographic and Elevation Data Fetching.

Queries public web APIs (such as USGS or NASA databases) using latitude/longitude
bounding boxes to automatically download and format elevation DEMs (e.g., SRTM or
USGS 3DEP) and land-cover maps (e.g., USGS NLCD) into solver-compatible formats.

Provides high-quality synthetic offline fallback for sandboxed environments.
"""

import os
import sys
import argparse
import math
import tempfile
from typing import Tuple, List, Optional
import numpy as np

# Suppress warnings from Rasterio/PROJ if any
import warnings
warnings.filterwarnings('ignore')

try:
    import requests
except ImportError:
    print("ERROR: 'requests' library is required to query web APIs. Please install it with 'pip3 install requests'.", file=sys.stderr)
    sys.exit(1)

try:
    import rasterio
    import rasterio.enums
    RASTERIO_AVAILABLE = True
except ImportError:
    RASTERIO_AVAILABLE = False

try:
    import pyproj
    PYPROJ_AVAILABLE = True
except ImportError:
    PYPROJ_AVAILABLE = False


# NLCD Land Cover Categories -> Aerodynamic Roughness z0 [meters]
# Based on src/landuse_roughness.H lookup values
NLCD_Z0_MAPPING = {
    11: 0.0005,  # Open Water
    21: 0.15,    # Developed, Open Space
    22: 0.40,    # Developed, Low Intensity
    23: 0.80,    # Developed, Medium Intensity
    24: 1.50,    # Developed, High Intensity
    31: 0.02,    # Barren Land (Rock/Sand/Clay)
    41: 0.80,    # Deciduous Forest
    42: 1.20,    # Evergreen Forest
    43: 1.00,    # Mixed Forest
    52: 0.20,    # Shrub/Scrub
    71: 0.04,    # Grassland/Herbaceous
    81: 0.08,    # Pasture/Hay
    82: 0.10,    # Cultivated Crops
    90: 0.50,    # Woody Wetlands
    95: 0.10     # Herbaceous Wetlands
}

# Standard NLCD classification names
NLCD_NAMES = {
    11: "Open Water",
    21: "Developed, Open Space",
    22: "Developed, Low Intensity",
    23: "Developed, Medium Intensity",
    24: "Developed, High Intensity",
    31: "Barren Land",
    41: "Deciduous Forest",
    42: "Evergreen Forest",
    43: "Mixed Forest",
    52: "Shrub/Scrub",
    71: "Grassland/Herbaceous",
    81: "Pasture/Hay",
    82: "Cultivated Crops",
    90: "Woody Wetlands",
    95: "Herbaceous Wetlands"
}


def flat_earth_project(lat: float, lon: float, lat_ref: float, lon_ref: float) -> Tuple[float, float]:
    """
    Projects latitude/longitude to metric local coordinates using flat-earth approximation.
    Consistent with the conversion used in terrain_reader_srtm.py.
    """
    # 1 degree latitude ≈ 111 km
    # 1 degree longitude ≈ 111 km * cos(latitude)
    x = (lon - lon_ref) * 111000.0 * math.cos(math.radians(lat_ref))
    y = (lat - lat_ref) * 111000.0
    return x, y


def utm_project(lat: float, lon: float, lat_ref: float, lon_ref: float) -> Tuple[float, float]:
    """
    Projects latitude/longitude to UTM zone relative metric coordinates using pyproj.
    Falls back to flat-earth approximation if pyproj is unavailable.
    """
    if not PYPROJ_AVAILABLE:
        return flat_earth_project(lat, lon, lat_ref, lon_ref)

    # Determine UTM zone with Norway / Svalbard exceptions
    zone = int((lon_ref + 180.0) / 6.0) + 1
    # Limit to valid 1-60 zone range
    zone = max(1, min(60, zone))
    
    # Special exceptions for Norway and Svalbard
    if 56.0 <= lat_ref < 64.0 and 3.0 <= lon_ref < 12.0:
        zone = 32
    elif 72.0 <= lat_ref < 84.0:
        if 0.0 <= lon_ref < 9.0:
            zone = 31
        elif 9.0 <= lon_ref < 21.0:
            zone = 33
        elif 21.0 <= lon_ref < 33.0:
            zone = 35
        elif 33.0 <= lon_ref < 42.0:
            zone = 37
            
    hemisphere = 'north' if lat_ref >= 0 else 'south'
    
    try:
        proj = pyproj.Proj(proj='utm', zone=zone, ellps='WGS84', hemisphere=hemisphere)
        x_abs, y_abs = proj(lon, lat)
        x_ref, y_ref = proj(lon_ref, lat_ref)
        # Relative coordinates to center point
        return x_abs - x_ref, y_abs - y_ref
    except Exception as e:
        print(f"WARNING: UTM projection failed ({e}). Falling back to flat-earth.", file=sys.stderr)
        return flat_earth_project(lat, lon, lat_ref, lon_ref)


def generate_mock_data(lat_min: float, lat_max: float, lon_min: float, lon_max: float,
                       nx: int, ny: int) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """
    Generates realistic synthetic (mock) elevation and land-cover grid datasets.
    Used for offline/sandboxed execution or testing when public endpoints are blocked.
    """
    # Create uniform grids
    lats = np.linspace(lat_min, lat_max, ny)
    lons = np.linspace(lon_min, lon_max, nx)
    lon_grid, lat_grid = np.meshgrid(lons, lats)
    
    lat_ref = (lat_min + lat_max) / 2.0
    lon_ref = (lon_min + lon_max) / 2.0
    
    # Simple relative metric coordinates to construct realistic geographic layout
    x_grid = (lon_grid - lon_ref) * 111000.0 * np.cos(np.radians(lat_ref))
    y_grid = (lat_grid - lat_ref) * 111000.0
    
    # 1. Generate multi-peak terrain (Mt. Hood/Flatirons hybrid mockup)
    base_elev = 150.0  # base plain elevation
    
    # Primary peak (high, prominent mountain)
    r2_1 = (x_grid - 0.0)**2 + (y_grid - 0.0)**2
    sigma_1 = 1200.0
    z_grid = base_elev + 950.0 * np.exp(-r2_1 / (2.0 * sigma_1**2))
    
    # Secondary flanking peak
    r2_2 = (x_grid - 1000.0)**2 + (y_grid - 800.0)**2
    sigma_2 = 600.0
    z_grid += 400.0 * np.exp(-r2_2 / (2.0 * sigma_2**2))
    
    # Valley depressions (river bed)
    valley = 40.0 * np.sin(x_grid / 600.0) * np.exp(-y_grid**2 / (2.0 * 800.0**2))
    z_grid += valley
    
    # Small terrain roughness/random undulating terrain
    z_grid += 12.0 * np.sin(x_grid / 150.0) * np.cos(y_grid / 150.0)
    z_grid = np.maximum(z_grid, 0.0)  # non-negative elevation
    
    # 2. Determine gradients/slopes for smart land cover allocation
    dy_m = (lat_max - lat_min) * 111000.0 / (ny - 1) if ny > 1 else 30.0
    dx_m = (lon_max - lon_min) * 111000.0 * np.cos(np.radians(lat_ref)) / (nx - 1) if nx > 1 else 30.0
    grad_y, grad_x = np.gradient(z_grid, dy_m, dx_m)
    slope = np.sqrt(grad_x**2 + grad_y**2)
    
    # Default category is Evergreen Forest (42)
    lc_grid = np.full_like(z_grid, 42, dtype=np.int32)
    
    # Low elevations/rivers: Open water (11)
    lc_grid[z_grid < 140] = 11
    # Plain flatter areas: Crops (82) or Pasture (81)
    lc_grid[(z_grid >= 140) & (z_grid < 250) & (slope < 0.04)] = 81
    lc_grid[(z_grid >= 140) & (z_grid < 200) & (slope < 0.02) & (x_grid < 0)] = 82
    # Open developments/Suburban patches (21/22)
    lc_grid[(z_grid >= 140) & (z_grid < 180) & (slope < 0.015) & (y_grid < -500)] = 21
    lc_grid[(z_grid >= 140) & (z_grid < 160) & (slope < 0.01) & (y_grid < -700)] = 22
    # Deciduous forest at transitional foothill bands
    lc_grid[(z_grid >= 250) & (z_grid < 450) & (slope < 0.15)] = 41
    # Shrubland (52) on steeper dry/foothill slopes
    lc_grid[(z_grid >= 450) & (z_grid < 650) & (slope > 0.12)] = 52
    # High alpine grasslands (71)
    lc_grid[z_grid >= 750] = 71
    # Mountain peaks and rocky cliffs: Barren (31)
    lc_grid[z_grid >= 1100] = 31
    lc_grid[slope > 0.45] = 31
    
    return lon_grid, lat_grid, z_grid, lc_grid


def fetch_usgs_3dep(lat_min: float, lat_max: float, lon_min: float, lon_max: float,
                    nx: int, ny: int) -> bytes:
    """Queries the USGS 3DEP Elevation ImageServer REST API."""
    url = "https://elevation.nationalmap.gov/arcgis/rest/services/3DEPElevation/ImageServer/exportImage"
    params = {
        "bbox": f"{lon_min},{lat_min},{lon_max},{lat_max}",
        "bboxSR": "4326",
        "size": f"{nx},{ny}",
        "format": "tiff",
        "pixelType": "F32",
        "f": "image"
    }
    response = requests.get(url, params=params, timeout=20)
    response.raise_for_status()
    return response.content


def fetch_usgs_nlcd(lat_min: float, lat_max: float, lon_min: float, lon_max: float,
                    nx: int, ny: int) -> bytes:
    """Queries the USGS NLCD Land Cover ImageServer REST API."""
    url = "https://www.mrlc.gov/arcgis/rest/services/NLCD/NLCD_2021_Land_Cover_L48/ImageServer/exportImage"
    params = {
        "bbox": f"{lon_min},{lat_min},{lon_max},{lat_max}",
        "bboxSR": "4326",
        "size": f"{nx},{ny}",
        "format": "tiff",
        "f": "image"
    }
    response = requests.get(url, params=params, timeout=20)
    response.raise_for_status()
    return response.content


def fetch_opentopo_srtm(lat_min: float, lat_max: float, lon_min: float, lon_max: float,
                        apikey: Optional[str] = None) -> bytes:
    """Queries the OpenTopography Global SRTM 1-arcsecond API."""
    url = "https://portal.opentopography.org/api/dem"
    params = {
        "demtype": "SRTMGL1",
        "west": str(lon_min),
        "east": str(lon_max),
        "south": str(lat_min),
        "north": str(lat_max),
        "outputFormat": "GTiff"
    }
    if apikey:
        params["apikey"] = apikey
    response = requests.get(url, params=params, timeout=25)
    response.raise_for_status()
    return response.content


def process_tiff_data(tiff_bytes: bytes, nx: int, ny: int, is_dem: bool) -> np.ndarray:
    """
    Writes downloaded binary TIFF data to a temporary file, opens it using rasterio,
    and resamples it directly to the exact target grid dimension nx * ny.
    """
    if not RASTERIO_AVAILABLE:
        raise RuntimeError("rasterio is not available to parse TIFF files. Use --mock flag.")

    with tempfile.NamedTemporaryFile(suffix=".tif", delete=False) as temp_file:
        temp_file.write(tiff_bytes)
        temp_path = temp_file.name

    try:
        resampling_alg = (rasterio.enums.Resampling.bilinear if is_dem 
                          else rasterio.enums.Resampling.nearest)
        
        with rasterio.open(temp_path) as src:
            # Check Y-axis resolution step in geotransform to validate spatial orientation.
            # Usually transform.e is negative, meaning row 0 represents the northern-most latitude.
            transform = src.transform
            if not transform:
                raise ValueError("Raster geotransform is missing, cannot determine spatial orientation.")
                
            y_resolution = transform.e
            
            data = src.read(
                1,
                out_shape=(ny, nx),
                resampling=resampling_alg
            )
            
            # If y_resolution is positive, row 0 represents the southern-most latitude (South at top).
            # Flip the array vertically so row 0 is always North (lat_max) to match the caller's assumption.
            if y_resolution > 0:
                data = np.flipud(data)
                
            return data.astype(np.float32)
    finally:
        if os.path.exists(temp_path):
            os.remove(temp_path)


def write_terrain_csv(output_file: str, points: List[Tuple[float, float, float, int]],
                      nx: int, ny: int, lat_min: float, lat_max: float,
                      lon_min: float, lon_max: float) -> bool:
    """Writes terrain coordinates to solver-compatible CSV format."""
    try:
        elevations = [p[2] for p in points]
        elev_min, elev_max = min(elevations), max(elevations)
        
        with open(output_file, 'w') as f:
            f.write("# Automated Geographic Elevation DEM data (lat/lon projected to meters)\n")
            f.write(f"# Grid: {nx}x{ny} points\n")
            f.write(f"# Latitude range: {lat_min:.4f}° to {lat_max:.4f}°\n")
            f.write(f"# Longitude range: {lon_min:.4f}° to {lon_max:.4f}°\n")
            f.write(f"# Elevation range: {elev_min:.1f}m to {elev_max:.1f}m\n")
            f.write("# X[m] Y[m] Z[m]\n")
            
            for x, y, z, _ in points:
                f.write(f"{x:.6f} {y:.6f} {z:.6f}\n")
                
        print(f"✓ Elevation DEM exported successfully: {output_file}", file=sys.stderr)
        return True
    except Exception as e:
        print(f"ERROR: Failed to write terrain CSV: {e}", file=sys.stderr)
        return False


def write_landuse_csv(output_file: str, points: List[Tuple[float, float, float, int]],
                      nx: int, ny: int) -> bool:
    """Writes land cover classification data to solver-compatible CSV format."""
    try:
        with open(output_file, 'w') as f:
            f.write("# Automated Land-use classification data\n")
            f.write(f"# Grid: {nx}x{ny} points\n")
            f.write("# NLCD codes: 11=Water, 21=Developed Open, 31=Barren, 41=Deciduous, 42=Evergreen, 71=Grassland, etc.\n")
            f.write("# X[m] Y[m] NLCD_Code z0[m]\n")
            
            for x, y, _, lc_code in points:
                z0_val = NLCD_Z0_MAPPING.get(lc_code, 0.10)
                f.write(f"{x:.6f} {y:.6f} {lc_code} {z0_val:.4f}\n")
                
        print(f"✓ Land cover map exported successfully: {output_file}", file=sys.stderr)
        return True
    except Exception as e:
        print(f"ERROR: Failed to write landuse CSV: {e}", file=sys.stderr)
        return False


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Fetch global/US geographic and elevation datasets and format for AMR wind solver."
    )
    parser.add_argument('--lat-min', type=float, required=True, help='Minimum latitude of bounding box')
    parser.add_argument('--lat-max', type=float, required=True, help='Maximum latitude of bounding box')
    parser.add_argument('--lon-min', type=float, required=True, help='Minimum longitude of bounding box')
    parser.add_argument('--lon-max', type=float, required=True, help='Maximum longitude of bounding box')
    parser.add_argument('--nx', type=int, default=100, help='Number of grid cells in X/longitude (default: 100)')
    parser.add_argument('--ny', type=int, default=100, help='Number of grid cells in Y/latitude (default: 100)')
    parser.add_argument('--dem-output', '-o', default='terrain.csv', help='Output terrain elevation CSV filepath')
    parser.add_argument('--lc-output', '-l', default='landuse.csv', help='Output landuse classification CSV filepath')
    parser.add_argument('--projection', choices=['flat', 'utm'], default='flat',
                        help='Coordinate projection to use (default: flat)')
    parser.add_argument('--source', choices=['usgs', 'opentopo', 'auto'], default='auto',
                        help='Elevation dataset provider endpoint (default: auto)')
    parser.add_argument('--api-key', help='Optional OpenTopography API key if using opentopo source')
    parser.add_argument('--mock', action='store_true', help='Force generation of high-quality synthetic mock offline data')
    parser.add_argument('--no-fallback', action='store_true', help='Disable automatic offline fallback on network/DNS failure')

    args = parser.parse_args()

    # Validate coordinate box bounds
    if args.lat_min >= args.lat_max:
        print("ERROR: --lat-min must be strictly less than --lat-max", file=sys.stderr)
        return 1
    if args.lon_min >= args.lon_max:
        print("ERROR: --lon-min must be strictly less than --lon-max", file=sys.stderr)
        return 1
    if args.nx <= 1 or args.ny <= 1:
        print("ERROR: Grid dimension nx and ny must be greater than 1", file=sys.stderr)
        return 1

    # Check for library availability
    if not RASTERIO_AVAILABLE and not args.mock:
        print("WARNING: 'rasterio' package is not installed. Real GeoTIFF processing is disabled.", file=sys.stderr)
        if args.no_fallback:
            print("ERROR: Cannot continue without rasterio when fallback is disabled.", file=sys.stderr)
            return 1
        print("Switching to mock data mode automatically...", file=sys.stderr)
        args.mock = True

    dem_data = None
    lc_data = None

    # Reference center point for metric transformations
    lat_ref = (args.lat_min + args.lat_max) / 2.0
    lon_ref = (args.lon_min + args.lon_max) / 2.0

    if args.mock:
        _, _, dem_data, lc_data = generate_mock_data(
            args.lat_min, args.lat_max, args.lon_min, args.lon_max, args.nx, args.ny
        )
    else:
        # Attempt to query live API endpoints
        # Realize USGS services might be CONUS-only, OpenTopography is global
        print(f"Connecting to web API endpoints for latitude [{args.lat_min}, {args.lat_max}] "
              f"and longitude [{args.lon_min}, {args.lon_max}]...", file=sys.stderr)
        
        try:
            # 1. Fetch Elevation DEM
            if args.source == 'usgs':
                print("Fetching USGS 3DEP DEM...", file=sys.stderr)
                dem_bytes = fetch_usgs_3dep(args.lat_min, args.lat_max, args.lon_min, args.lon_max, args.nx, args.ny)
                dem_data = process_tiff_data(dem_bytes, args.nx, args.ny, is_dem=True)
            elif args.source == 'opentopo':
                print("Fetching OpenTopography SRTM DEM...", file=sys.stderr)
                dem_bytes = fetch_opentopo_srtm(args.lat_min, args.lat_max, args.lon_min, args.lon_max, args.api_key)
                dem_data = process_tiff_data(dem_bytes, args.nx, args.ny, is_dem=True)
            else:  # 'auto' source
                # Try USGS first, fall back to OpenTopography if outside US or USGS fails
                try:
                    print("Attempting USGS 3DEP Elevation API query...", file=sys.stderr)
                    dem_bytes = fetch_usgs_3dep(args.lat_min, args.lat_max, args.lon_min, args.lon_max, args.nx, args.ny)
                    dem_data = process_tiff_data(dem_bytes, args.nx, args.ny, is_dem=True)
                except Exception as ex:
                    print(f"USGS 3DEP elevation fetch failed: {ex}. Retrying via OpenTopography SRTM API...", file=sys.stderr)
                    dem_bytes = fetch_opentopo_srtm(args.lat_min, args.lat_max, args.lon_min, args.lon_max, args.api_key)
                    dem_data = process_tiff_data(dem_bytes, args.nx, args.ny, is_dem=True)

            # 2. Fetch Land-Cover Map
            try:
                print("Fetching USGS NLCD Land Cover classification map...", file=sys.stderr)
                lc_bytes = fetch_usgs_nlcd(args.lat_min, args.lat_max, args.lon_min, args.lon_max, args.nx, args.ny)
                lc_data = process_tiff_data(lc_bytes, args.nx, args.ny, is_dem=False)
            except Exception as lc_ex:
                print(f"WARNING: Land cover mapping fetch failed ({lc_ex}). Using default Grassland category.", file=sys.stderr)
                lc_data = np.full_like(dem_data, 71, dtype=np.int32) # Default NLCD Grassland category

        except Exception as e:
            print(f"\nAPI Query/Processing Error: {e}", file=sys.stderr)
            if args.no_fallback:
                print("ERROR: API queries failed and offline fallback is disabled.", file=sys.stderr)
                return 1
            print("Automatic Offline Fallback: Creating high-quality synthetic mockup datasets...", file=sys.stderr)
            _, _, dem_data, lc_data = generate_mock_data(
                args.lat_min, args.lat_max, args.lon_min, args.lon_max, args.nx, args.ny
            )

    # 3. Format and Project data into solver-compatible output points list
    # The grid coordinates:
    # j: 0 corresponds to lat_min, j: ny-1 corresponds to lat_max
    # i: 0 corresponds to lon_min, i: nx-1 corresponds to lon_max
    # Row index in downloaded rasters usually starts from North (lat_max), so we reverse the index
    points = []
    for j in range(args.ny):
        lat = args.lat_min + j * (args.lat_max - args.lat_min) / (args.ny - 1)
        row_idx = args.ny - 1 - j
        for i in range(args.nx):
            lon = args.lon_min + i * (args.lon_max - args.lon_min) / (args.nx - 1)
            
            # Extract elevation and land use code
            elev = float(dem_data[row_idx, i])
            if np.isnan(elev):
                elev = 0.0
            
            lc_code = int(np.round(lc_data[row_idx, i])) if lc_data is not None else 71
            
            # Apply chosen projection
            if args.projection == 'utm':
                x, y = utm_project(lat, lon, lat_ref, lon_ref)
            else:
                x, y = flat_earth_project(lat, lon, lat_ref, lon_ref)
                
            points.append((x, y, elev, lc_code))

    # Write files
    dem_success = write_terrain_csv(
        args.dem_output, points, args.nx, args.ny,
        args.lat_min, args.lat_max, args.lon_min, args.lon_max
    )
    lc_success = write_landuse_csv(
        args.lc_output, points, args.nx, args.ny
    )

    if dem_success and lc_success:
        print("\n🎉 Geographic and elevation fetching completed successfully!", file=sys.stderr)
        return 0
    else:
        return 1


if __name__ == "__main__":
    sys.exit(main())
