#!/usr/bin/env python3
"""
fetch_uswtb_turbines.py - Fetch and process actual USWTB turbine data for Alta Wind Energy Center

This script downloads the USGS Wind Turbine Database (USWTB) from:
https://energy.usgs.gov/uswtdb/assets/data/uswtdbCSV.zip

And extracts turbine coordinates for the Alta Wind Energy Center and related facilities.

Usage:
    python3 fetch_uswtb_turbines.py --output turbines_uswtb.csv --project "Alta Wind"

The script filters USWTB data to find all turbines in the Alta Wind Energy Center region
and exports them in the format required by the massconsistent_amr solver.
"""

import sys
import urllib.request
import zipfile
import io
import csv
import argparse
from pathlib import Path
from typing import List, Dict, Optional

def download_uswtb_data() -> Optional[Dict]:
    """Download USWTB data from USGS server."""
    print("Downloading USWTB database from USGS...")
    url = "https://energy.usgs.gov/uswtdb/assets/data/uswtdbCSV.zip"
    
    try:
        response = urllib.request.urlopen(url, timeout=60)
        zip_data = response.read()
        print(f"✓ Downloaded {len(zip_data) / 1024 / 1024:.2f} MB")
        return zip_data
    except Exception as e:
        print(f"✗ Error downloading USWTB data: {e}")
        print("\nThe database may not be accessible in this environment.")
        print("Please download manually from: https://energy.usgs.gov/uswtdb/")
        return None

def extract_turbines_from_zip(zip_data: bytes) -> List[Dict]:
    """Extract turbine data from USWTB ZIP file."""
    turbines = []
    
    try:
        with zipfile.ZipFile(io.BytesIO(zip_data)) as z:
            csv_files = [f for f in z.namelist() if f.endswith('.csv')]
            
            if not csv_files:
                print("✗ No CSV files found in ZIP archive")
                return turbines
            
            csv_file = csv_files[0]
            print(f"✓ Reading {csv_file}")
            
            with z.open(csv_file) as f:
                reader = csv.DictReader(io.TextIOWrapper(f, encoding='utf-8'))
                turbines = list(reader)
                print(f"✓ Loaded {len(turbines)} total turbines from database")
    
    except Exception as e:
        print(f"✗ Error extracting turbines: {e}")
    
    return turbines

def filter_alta_turbines(turbines: List[Dict], project_keywords: List[str] = None) -> List[Dict]:
    """Filter turbines belonging to Alta Wind Energy Center."""
    if project_keywords is None:
        project_keywords = [
            'Alta Wind',
            'Antelope Valley',
            'Mojave Wind',
            'Tehachapi'
        ]
    
    filtered = []
    for turbine in turbines:
        project_name = turbine.get('ProjectName', '').lower()
        if any(keyword.lower() in project_name for keyword in project_keywords):
            filtered.append(turbine)
    
    return filtered

def convert_to_solver_format(turbines: List[Dict]) -> List[Dict]:
    """Convert USWTB turbine data to massconsistent_amr solver format."""
    solver_format = []
    
    for turbine in turbines:
        try:
            # Extract relevant fields from USWTB
            x = float(turbine.get('Longitude', 0))  # Will need UTM conversion
            y = float(turbine.get('Latitude', 0))
            hub_height = float(turbine.get('HubHeight', 80.0))
            rotor_diameter = float(turbine.get('RotorDiameter', 80.0))
            
            # Estimate thrust coefficient from turbine type if available
            ct = 0.8  # Default value
            
            solver_format.append({
                'id': turbine.get('Turbine', ''),
                'project': turbine.get('ProjectName', ''),
                'latitude': y,
                'longitude': x,
                'hub_height': hub_height,
                'rotor_diameter': rotor_diameter,
                'ct': ct,
                'manufacturer': turbine.get('Manufacturer', ''),
                'model': turbine.get('Model', '')
            })
        except (ValueError, TypeError) as e:
            print(f"Warning: Skipping turbine {turbine.get('Turbine', 'Unknown')}: {e}")
            continue
    
    return solver_format

def write_turbines_csv(turbines: List[Dict], output_path: Path):
    """Write turbines in solver format to CSV."""
    with open(output_path, 'w') as f:
        f.write("# x, y, hub_height, rotor_diameter, default_ct, yaw, orientation, power_curve_file\n")
        
        # Note: x, y should be in UTM coordinates
        # This requires coordinate transformation which should be done in the main test file
        for t in turbines:
            # Write as lat/lon for now; test file will convert to UTM
            f.write(f"{t['longitude']:.6f}, {t['latitude']:.6f}, {t['hub_height']:.1f}, "
                   f"{t['rotor_diameter']:.1f}, {t['ct']:.2f}, 0.0, 0.0, nrel_5mw.csv\n")

def main():
    parser = argparse.ArgumentParser(description='Fetch and process USWTB turbine data')
    parser.add_argument('--output', type=str, default='turbines_uswtb.csv',
                       help='Output CSV file for turbine coordinates')
    parser.add_argument('--project', type=str, default=None,
                       help='Project name keyword to filter (default: Alta Wind)')
    parser.add_argument('--list-projects', action='store_true',
                       help='List all projects in database')
    
    args = parser.parse_args()
    
    # Try to download data
    zip_data = download_uswtb_data()
    if not zip_data:
        print("\n⚠ USWTB data download failed. Please ensure internet access is available.")
        sys.exit(1)
    
    # Extract turbines
    all_turbines = extract_turbines_from_zip(zip_data)
    if not all_turbines:
        print("✗ Failed to extract turbine data")
        sys.exit(1)
    
    # List projects if requested
    if args.list_projects:
        projects = set()
        for t in all_turbines:
            projects.add(t.get('ProjectName', 'Unknown'))
        
        print("\nProjects in USWTB database:")
        for proj in sorted(projects):
            count = sum(1 for t in all_turbines if t.get('ProjectName') == proj)
            print(f"  {proj}: {count} turbines")
        return
    
    # Filter Alta turbines
    project_filter = [args.project] if args.project else None
    alta_turbines = filter_alta_turbines(all_turbines, project_filter)
    print(f"\n✓ Found {len(alta_turbines)} turbines for Alta Wind Energy Center")
    
    if not alta_turbines:
        print("✗ No turbines found for specified project")
        sys.exit(1)
    
    # Convert format
    solver_turbines = convert_to_solver_format(alta_turbines)
    
    # Write output
    output_path = Path(args.output)
    write_turbines_csv(solver_turbines, output_path)
    print(f"✓ Wrote {len(solver_turbines)} turbines to {output_path}")

if __name__ == '__main__':
    main()
