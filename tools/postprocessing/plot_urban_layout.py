#!/usr/bin/env python3
"""
plot_urban_layout.py

Generates a visualization of the Urban Layout scenario:
- Shows a top-down (plan) view of buildings with various geometries
- Displays height coloring for building elevations
- Includes terrain if present

Saves the generated image to docs/urban_layout.png.
"""

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon as MPLPolygon
from pathlib import Path

# Setup paths
POST_DIR = Path(__file__).resolve().parent
REPO_ROOT = POST_DIR.parent.parent
TEST_DIR = REPO_ROOT / "regtest" / "buildings" / "urban_layout"
DOCS_DIR = REPO_ROOT / "docs"

def parse_buildings_csv(filename):
    """Parse buildings.csv and return list of building geometries with heights."""
    buildings = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue
            
            # Parse building geometry
            if 'POLYGON:' in line:
                # Complex polygon format: POLYGON: x1 y1 x2 y2 ... xn yn | zmin zmax
                parts = line.split('|')
                coords_part = parts[0].replace('POLYGON:', '').strip()
                height_part = parts[1].strip()
                
                # Parse coordinates
                coords_list = [float(x) for x in coords_part.split()]
                coords = [(coords_list[i], coords_list[i+1]) for i in range(0, len(coords_list), 2)]
                
                # Parse heights
                zmin, zmax = map(float, height_part.split())
                buildings.append({
                    'type': 'polygon',
                    'coords': coords,
                    'zmin': zmin,
                    'zmax': zmax,
                    'height': zmax - zmin
                })
            else:
                # Regular box format: xmin xmax ymin ymax zmin zmax
                parts = [float(x) for x in line.split()]
                if len(parts) == 6:
                    xmin, xmax, ymin, ymax, zmin, zmax = parts
                    buildings.append({
                        'type': 'box',
                        'xmin': xmin,
                        'xmax': xmax,
                        'ymin': ymin,
                        'ymax': ymax,
                        'zmin': zmin,
                        'zmax': zmax,
                        'height': zmax - zmin
                    })
    return buildings

def main():
    print("Generating Urban Layout visualization...")
    
    # Change to test directory to read files
    os.chdir(TEST_DIR)
    
    # Parse buildings
    buildings = parse_buildings_csv("buildings.csv")
    
    if not buildings:
        print("ERROR: No buildings found in buildings.csv")
        return
    
    # Find domain extent from buildings
    all_x = []
    all_y = []
    heights = []
    
    for building in buildings:
        if building['type'] == 'box':
            all_x.extend([building['xmin'], building['xmax']])
            all_y.extend([building['ymin'], building['ymax']])
        elif building['type'] == 'polygon':
            coords = building['coords']
            all_x.extend([c[0] for c in coords])
            all_y.extend([c[1] for c in coords])
        heights.append(building['height'])
    
    # Add some padding
    xmin, xmax = min(all_x) - 20, max(all_x) + 20
    ymin, ymax = min(all_y) - 20, max(all_y) + 20
    
    # Normalize heights for coloring
    max_height = max(heights)
    min_height = min(heights)
    
    # Create figure
    fig, ax = plt.subplots(figsize=(10, 10))
    
    # Plot each building
    for i, building in enumerate(buildings):
        if building['type'] == 'box':
            # Create rectangle for box building
            rect = Rectangle(
                (building['xmin'], building['ymin']),
                building['xmax'] - building['xmin'],
                building['ymax'] - building['ymin'],
                linewidth=2,
                edgecolor='black',
                facecolor='lightblue',
                alpha=0.7
            )
            ax.add_patch(rect)
            
            # Add text with height info
            cx = (building['xmin'] + building['xmax']) / 2
            cy = (building['ymin'] + building['ymax']) / 2
            ax.text(cx, cy, f"{building['height']:.0f}m", 
                   ha='center', va='center', fontsize=9, fontweight='bold')
        
        elif building['type'] == 'polygon':
            # Create polygon for complex building
            coords = building['coords']
            polygon = MPLPolygon(coords, linewidth=2, edgecolor='black',
                                facecolor='coral', alpha=0.7)
            ax.add_patch(polygon)
            
            # Add text with height info
            coords_array = np.array(coords)
            cx = np.mean(coords_array[:, 0])
            cy = np.mean(coords_array[:, 1])
            ax.text(cx, cy, f"{building['height']:.0f}m",
                   ha='center', va='center', fontsize=9, fontweight='bold')
    
    # Set axis properties
    ax.set_xlim(xmin, xmax)
    ax.set_ylim(ymin, ymax)
    ax.set_aspect('equal')
    ax.grid(True, linestyle='--', alpha=0.3)
    ax.set_xlabel("X distance [m]", fontsize=12)
    ax.set_ylabel("Y distance [m]", fontsize=12)
    ax.set_title("Urban Layout: Building Geometries & Heights", fontsize=14, fontweight='bold')
    
    # Add legend
    from matplotlib.patches import Patch
    legend_elements = [
        Patch(facecolor='lightblue', edgecolor='black', label='Box Buildings'),
        Patch(facecolor='coral', edgecolor='black', label='Complex Geometry Buildings')
    ]
    ax.legend(handles=legend_elements, loc='upper right')
    
    plt.tight_layout()
    
    # Save figure
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    out_img = DOCS_DIR / "urban_layout.png"
    plt.savefig(out_img, dpi=150, bbox_inches='tight')
    print(f"✓ Saved Urban Layout plot to: {out_img}")
    plt.close()

if __name__ == '__main__':
    main()
