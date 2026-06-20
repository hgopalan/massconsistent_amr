"""
Gorge Bridge Crossing Scenario - Complex Terrain Wind Deflection

This scenario demonstrates bridge loading assessment for a suspension/cable bridge
crossing a deep gorge with complex topographic steering and wind acceleration effects.

Real-world examples:
    - Royal Gorge Bridge (Colorado): 10 km above sea level, 315 m above canyon floor
    - Millau Viaduct (France): 343 m high, crosses Tarn Valley
    - Foresthill Bridge (California): 216 m above river, narrow canyon

Physical Context:
    - Canyon depth: 300-500 m below bridge deck
    - Canyon width: 500-1000 m at bridge location
    - Terrain aspect ratio: 3:1 (height:width) typical for gorge
    - Valley alignment: steering wind parallel to axis
    - Vertical wind shear: strong due to terrain obstruction
    - Vortex formation: downstream canyon eddies create oscillating winds
    - Aspect ratio effects: funnel/channeling increases wind speed 30-80%

Bridge specifications:
    - Span length: 1000-1500 m main span
    - Deck width: 30-40 m (multi-lane)
    - Height above canyon floor: 300 m
    - Cable-stayed or suspension: flexible structure
    - Natural frequencies: 0.1-0.5 Hz (low for long spans)
    - Damping ratio: 2-5% (typical for cables)
    - Critical concern: vortex-induced oscillation matching natural frequency

Why mass-consistent solver excels over NOAA/NREL:
    1. Enforces continuity around bridge obstruction
    2. Pressure-driven wind channeling through canyon
    3. Vertical wind shear properly captured (not just log-profile)
    4. Asymmetric canyon walls → asymmetric wind field
    5. Dynamic gust effects from vortex shedding
    6. Comfort assessment based on acceleration (ISO standards)
    7. Resonance prediction vs. static design loads

Expected outcomes:
    - 50-80% wind speed amplification in narrowest sections
    - Strong vertical wind shear (w-component variations)
    - Cross-wind gusts from canyon-wall deflection
    - Low-frequency oscillations matching bridge natural frequency
    - Lateral sway up to 1-2 m (long spans)
    - Comfort assessment: potentially unsafe (>0.5 m/s²) during extreme events
"""

import numpy as np
import csv
import os
from typing import List, Tuple

class GorgeBridgeScenarioGenerator:
    """Generate gorge bridge crossing test case."""
    
    def __init__(self, output_dir: str = "."):
        """Initialize scenario generator."""
        self.output_dir = output_dir
        os.makedirs(output_dir, exist_ok=True)
    
    def generate_gorge_terrain(self, nx: int = 101, ny: int = 51) -> Tuple[List, np.ndarray]:
        """
        Generate gorge terrain with canyon walls and asymmetry.
        
        Characteristics:
            - Upstream: gentle slope leading to gorge
            - Canyon walls: steep (60-80 degree slopes)
            - Canyon asymmetry: one wall steeper than other
            - Canyon floor: narrow V-shape or U-shape
            - Downstream: gradual slope away
            - Bridge elevation: near top of canyon (300 m above floor)
        
        Args:
            nx, ny: Grid dimensions
        
        Returns:
            (points_list, elevation_array)
        """
        x = np.linspace(-5.0, 5.0, nx)   # ±5 km cross-canyon
        y = np.linspace(0.0, 10.0, ny)   # 10 km along-canyon
        
        terrain_points = []
        elevation_map = np.zeros((nx, ny))
        
        for i, xi in enumerate(x):
            for j, yj in enumerate(y):
                # Upstream approach (y < 2 km)
                if yj < 2.0:
                    z = 1000.0 - 50.0 * yj  # Gentle slope down
                
                # Gorge entrance (2 km < y < 4 km)
                elif yj < 4.0:
                    progress = (yj - 2.0) / 2.0
                    # Canyon deepens
                    canyon_depth = 300.0 * (progress ** 1.5)
                    # Walls become steeper
                    dist_from_center = np.abs(xi)
                    wall_slope = 60.0 * progress  # degrees
                    # Asymmetric canyon: left wall steeper
                    if xi < 0:
                        wall_height = 900.0 - canyon_depth - (canyon_depth/2.0) * min(1.0, dist_from_center / 2.0)
                    else:
                        wall_height = 900.0 - canyon_depth - (canyon_depth/3.0) * min(1.0, dist_from_center / 2.5)
                    z = wall_height
                
                # Bridge span (4 km < y < 6 km)
                elif yj < 6.0:
                    # Deepest canyon point
                    dist_from_center = np.abs(xi)
                    # Steeper left wall (xi < 0), gentler right wall
                    if xi < 0:
                        z = 600.0 + (100.0 * dist_from_center ** 1.2)  # Steep left
                    else:
                        z = 650.0 + (50.0 * dist_from_center ** 0.8)   # Gentler right
                    
                    # Bridge deck at ~900 m elevation
                    # (will be handled separately for loading calc)
                
                # Gorge exit (6 km < y < 8 km)
                elif yj < 8.0:
                    progress = (8.0 - yj) / 2.0
                    canyon_depth = 300.0 * (progress ** 1.5)
                    dist_from_center = np.abs(xi)
                    # Walls open up
                    if xi < 0:
                        z = 900.0 - canyon_depth - (canyon_depth/2.0) * min(1.0, dist_from_center / 2.0)
                    else:
                        z = 900.0 - canyon_depth - (canyon_depth/3.0) * min(1.0, dist_from_center / 2.5)
                
                # Downstream (y > 8 km)
                else:
                    z = 1000.0 - 50.0 * (10.0 - yj)  # Gradual slope up
                
                z = max(600.0, z)  # Enforce minimum elevation
                elevation_map[i, j] = z
                terrain_points.append([xi, yj, z])
        
        return terrain_points, elevation_map
    
    def generate_bridge_spans(self) -> List[dict]:
        """
        Generate bridge span configuration across gorge.
        
        Main span: 1200 m (from wall to wall)
        Approach spans: 2×400 m each side
        
        Returns:
            List of bridge span dictionaries
        """
        spans = []
        
        # Main span (crosses gorge)
        main_span = {
            'id': 0,
            'x1': -0.5,
            'y1': 4.9,
            'z1': 900.0,  # Bridge deck elevation
            'x2': 0.5,
            'y2': 5.1,
            'z2': 900.0,
            'deck_width': 35.0,     # m (4-6 lanes)
            'deck_depth': 3.5,      # m (composite/steel depth)
            'mass_per_length': 8000.0,  # kg/m (500-1000 ton total)
            'drag_coeff': 1.2,      # Streamlined bridge
            'side_drag_coeff': 0.8, # Lateral coefficient
            'natural_frequency': 0.15,  # Hz (low for long cable span)
            'critical_damping_ratio': 0.04,  # 4% structural damping
            'span_type': 'main',
        }
        spans.append(main_span)
        
        # Upstream approach span (right side)
        upstream_right = {
            'id': 1,
            'x1': 0.5,
            'y1': 2.0,
            'z1': 880.0,  # Slightly lower deck
            'x2': 2.0,
            'y2': 3.5,
            'z2': 885.0,
            'deck_width': 35.0,
            'deck_depth': 3.0,
            'mass_per_length': 7000.0,
            'drag_coeff': 1.2,
            'side_drag_coeff': 0.8,
            'natural_frequency': 0.25,
            'critical_damping_ratio': 0.045,
            'span_type': 'approach',
        }
        spans.append(upstream_right)
        
        # Upstream approach span (left side)
        upstream_left = {
            'id': 2,
            'x1': -2.0,
            'y1': 3.5,
            'z1': 885.0,
            'x2': -0.5,
            'y2': 2.0,
            'z2': 880.0,
            'deck_width': 35.0,
            'deck_depth': 3.0,
            'mass_per_length': 7000.0,
            'drag_coeff': 1.2,
            'side_drag_coeff': 0.8,
            'natural_frequency': 0.25,
            'critical_damping_ratio': 0.045,
            'span_type': 'approach',
        }
        spans.append(upstream_left)
        
        # Downstream span (right side)
        downstream_right = {
            'id': 3,
            'x1': 2.0,
            'y1': 6.5,
            'z1': 885.0,
            'x2': 0.5,
            'y2': 8.0,
            'z2': 880.0,
            'deck_width': 35.0,
            'deck_depth': 3.0,
            'mass_per_length': 7000.0,
            'drag_coeff': 1.2,
            'side_drag_coeff': 0.8,
            'natural_frequency': 0.25,
            'critical_damping_ratio': 0.045,
            'span_type': 'approach',
        }
        spans.append(downstream_right)
        
        # Downstream span (left side)
        downstream_left = {
            'id': 4,
            'x1': -0.5,
            'y1': 8.0,
            'z1': 880.0,
            'x2': -2.0,
            'y2': 6.5,
            'z2': 885.0,
            'deck_width': 35.0,
            'deck_depth': 3.0,
            'mass_per_length': 7000.0,
            'drag_coeff': 1.2,
            'side_drag_coeff': 0.8,
            'natural_frequency': 0.25,
            'critical_damping_ratio': 0.045,
            'span_type': 'approach',
        }
        spans.append(downstream_left)
        
        return spans
    
    def write_files(self):
        """Write terrain and bridge CSV files."""
        # Generate terrain
        terrain_points, _ = self.generate_gorge_terrain()
        terrain_file = os.path.join(self.output_dir, 'gorge_terrain.csv')
        with open(terrain_file, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['x', 'y', 'z'])
            writer.writerows(terrain_points)
        print(f"Wrote terrain to {terrain_file}")
        
        # Generate and write bridge
        bridges = self.generate_bridge_spans()
        bridges_file = os.path.join(self.output_dir, 'gorge_bridge.csv')
        with open(bridges_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=[
                'id', 'x1', 'y1', 'z1', 'x2', 'y2', 'z2',
                'deck_width', 'deck_depth', 'mass_per_length',
                'drag_coeff', 'side_drag_coeff', 'natural_frequency',
                'critical_damping_ratio', 'span_type'
            ])
            writer.writeheader()
            writer.writerows(bridges)
        print(f"Wrote bridge to {bridges_file}")
        
        # Write input file
        inputs_file = os.path.join(self.output_dir, 'inputs_gorge.i')
        inputs_content = """
# Gorge Bridge Crossing - Complex Terrain Wind Effects
# Deep canyon with asymmetric walls, vortex formation downstream
# Expected sway: 0.5-1.5 m, comfort assessment potentially unsafe

terrain_file = gorge_terrain.csv

# Enable bridge loading
enable_bridge_loading = true
bridge_file = gorge_bridge.csv
bridge_output_file = gorge_bridge_output.csv

# Reference wind: 10 m/s from valley-parallel direction
# Canyon alignment amplifies to 15-20 m/s in narrowest section
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Roughness: bare rock canyon walls
z0 = 0.05

# Valley/canyon channeling model
enable_valley_channeling = true
enable_gap_flow = false

# Orographic speedup for terrain-aligned wind
enable_orographic_speedup = true

# Grid spacing [m]
# Fine resolution to resolve canyon geometry
dx = 100.0
dy = 100.0
dz = 50.0

# Domain extent
domain_height = 600.0

# Anisotropy: strong horizontal preferencing (valley alignment)
alpha_h = 1.5
alpha_v = 0.7
enable_cell_local_anisotropy = true

# Stability: assume neutral (typical high wind conditions)
enable_stability_correction = false

# MLMG solver
mlmg_verbose = 1
max_grid_size = 32

# Output
plot_file = plt_gorge
num_time_steps = 1
"""
        with open(inputs_file, 'w') as f:
            f.write(inputs_content)
        print(f"Wrote input file to {inputs_file}")
        
        print("\n" + "="*70)
        print("Gorge Bridge Scenario Generated Successfully")
        print("="*70)
        print("\nKey Features:")
        print("  - 10 km domain covering gorge (upstream-crossing-downstream)")
        print("  - Asymmetric canyon walls (300 m depth variation)")
        print("  - Main span: 1200 m (cable-stayed)")
        print("  - 4 approach spans: 2×400 m on each side")
        print("  - Deck elevation: 900 m (300 m above canyon floor)")
        print("\nPhysics Capabilities:")
        print("  - Valley channeling wind acceleration")
        print("  - Vertical wind shear from canyon walls")
        print("  - Vortex shedding downstream canyon")
        print("  - Asymmetric load distribution")
        print("  - Comfort/safety assessment based on acceleration")
        print("  - Resonance detection vs. cable natural frequencies")
        print("\nAdvantages vs. NOAA/NREL:")
        print("  1. Resolves canyon geometry explicitly (vs. flat terrain)")
        print("  2. Continuous wind profile along bridge route")
        print("  3. Vertical wind shear and gusts captured")
        print("  4. Cable resonance prediction (0.1-0.3 Hz range)")
        print("  5. Comfort metric: ISO 6954 acceleration thresholds")
        print("="*70)


if __name__ == '__main__':
    import sys
    output_dir = sys.argv[1] if len(sys.argv) > 1 else '.'
    generator = GorgeBridgeScenarioGenerator(output_dir)
    generator.write_files()
