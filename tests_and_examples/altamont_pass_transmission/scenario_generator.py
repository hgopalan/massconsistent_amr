"""
Altamont Pass 500 kV Transmission Line Scenario

This scenario demonstrates wind loading assessment for a high-voltage transmission line
crossing Altamont Pass, California - one of the windiest locations in North America,
famous for strong gap flow acceleration due to pressure-driven channeling through
the narrow valley.

Physical Context:
    - Altamont Pass elevation: ~400 m
    - Gap width: ~5-8 km (narrows through pass)
    - Typical gap flow speeds: 1.5-3x ambient wind speed
    - Terrain elevation change: 300+ m
    - Dominant wind direction: W-NW from Pacific high-pressure systems
    - Gap flow alignment: Nearly parallel to transmission line corridor

Line specifications (typical 500 kV tower configuration):
    - Tower height: 50-60 m
    - Phase spacing: 12-15 m (horizontal)
    - Conductor diameter: 28 mm (ACSR bundled)
    - Weight: ~2 kg/m per phase (3 phases + ground wires)
    - Operating current: 500-1500 A
    - Resistance: ~0.03 ohm/km at 75°C

Why mass-consistent solver excels over NOAA/NREL:
    1. Gap flow acceleration properly resolved via pressure gradient & continuity
    2. Local terrain steering (valley orientation effects)
    3. Spatially-varying wind speed over transmission line path
    4. Dynamic ampacity ratings coupled to local thermal wind effects
    5. Real-time sag/tension assessment vs. static conservative estimates

Expected outcomes:
    - 40-60% wind speed amplification in pass core
    - Higher sag (catenary drop) during high wind periods
    - Localized conductor heating from wind velocity changes
    - Resonance risk near vortex shedding frequencies on bundled phases
"""

import numpy as np
import csv
import os
from typing import List, Tuple

class AltamontScenarioGenerator:
    """Generate Altamont Pass transmission line test case."""
    
    def __init__(self, output_dir: str = "."):
        """Initialize scenario generator."""
        self.output_dir = output_dir
        os.makedirs(output_dir, exist_ok=True)
    
    def generate_terrain(self, nx: int = 121, ny: int = 121) -> Tuple[List, np.ndarray]:
        """
        Generate Altamont Pass terrain elevation profile.
        
        Characteristics:
            - Western slopes (upwind): gradual rise
            - Valley floor: narrow pass with ~5 km width
            - Eastern slopes (lee): steeper drop
            - Elevation range: 200-600 m
        
        Args:
            nx, ny: Grid dimensions
        
        Returns:
            (points_list, elevation_array)
        """
        x = np.linspace(0.0, 120.0, nx)  # 120 km domain
        y = np.linspace(-10.0, 10.0, ny)  # 20 km N-S extent
        
        terrain_points = []
        elevation_map = np.zeros((nx, ny))
        
        for i, xi in enumerate(x):
            for j, yj in enumerate(y):
                # West side: gradual rise toward pass
                if xi < 40.0:
                    z = 200.0 + (xi / 40.0) * 150.0
                # Pass core: narrow valley bottom with lateral variation
                elif xi < 80.0:
                    pass_factor = (xi - 40.0) / 40.0
                    # Valley depth - minimum at center (y=0)
                    y_normalized = np.abs(yj) / 10.0
                    valley_depth = 350.0 * (1.0 - y_normalized * y_normalized)
                    # Smooth transition through pass
                    z = 200.0 + 150.0 + (1.0 - pass_factor * pass_factor) * valley_depth
                # East side: lee-side slopes
                else:
                    lee_factor = (xi - 80.0) / 40.0
                    z = 200.0 + 150.0 - (lee_factor * lee_factor) * 150.0 + np.random.normal(0, 10)
                
                z = max(200.0, z)  # Enforce minimum elevation
                elevation_map[i, j] = z
                terrain_points.append([xi, yj, z])
        
        return terrain_points, elevation_map
    
    def generate_transmission_line(self) -> List[dict]:
        """
        Generate 500 kV transmission line route through Altamont Pass.
        
        Configuration:
            - 3 phase conductors in vertical triangle or horizontal configuration
            - 2-4 ground wires for lightning protection
            - Towers every 300-400 m
            - Line runs E-W through pass (along dominant wind direction)
        
        Returns:
            List of wire span dictionaries
        """
        towers_x = np.arange(10.0, 110.0, 0.35)  # Towers every 350 m
        num_towers = len(towers_x)
        
        wires = []
        wire_id = 0
        
        # Standard spacing for 500 kV line
        # Horizontal phase spacing: 13.5 m
        # Vertical phase spacing: 8-10 m
        phase_spacings = [
            (0.0, 0.0),      # Phase A (bottom)
            (13.5, 7.5),     # Phase B (middle)
            (6.75, 15.0),    # Phase C (top)
        ]
        
        ground_spacings = [
            (-2.0, 18.0),    # Ground wire 1
            (16.5, 18.0),    # Ground wire 2
        ]
        
        # Conductor properties (ACSR 795 kcmil)
        diameter = 0.0284  # m (28.4 mm)
        mass_per_m = 2.04  # kg/m
        drag_coeff = 1.1   # For bundled phases
        resistance = 0.03 / 1000  # ohm/m at 75°C
        emissivity = 0.5
        absorptivity = 0.5
        current = 800.0    # A (typical operating)
        
        # Generate phase wires
        for phase_idx, (dx, dy) in enumerate(phase_spacings):
            for tower_idx in range(len(towers_x) - 1):
                x1 = towers_x[tower_idx]
                x2 = towers_x[tower_idx + 1]
                y = dy  # Lateral position (relative to line centerline)
                
                # Height above terrain at this x location
                # Assume tower height: 55 m
                tower_height = 55.0
                
                # For gap flow scenario: phase at mid-span height
                z_span = 100.0 + dy  # ~100 m AGL typical
                
                wire = {
                    'id': wire_id,
                    'x1': x1,
                    'y1': 0.0,  # Centerline
                    'z1': z_span,
                    'x2': x2,
                    'y2': 0.0,
                    'z2': z_span,
                    'diameter': diameter,
                    'mass_density': mass_per_m,
                    'drag_coeff': drag_coeff,
                    'resistance': resistance,
                    'emissivity': emissivity,
                    'absorptivity': absorptivity,
                    'current': current,
                    'type': f'Phase_{["A", "B", "C"][phase_idx]}',
                }
                wires.append(wire)
                wire_id += 1
        
        # Generate ground wires (fewer, coarser resolution)
        ground_mass_per_m = 0.5  # kg/m (much lighter)
        for gnd_idx, (dx, dy) in enumerate(ground_spacings):
            for tower_idx in range(0, len(towers_x) - 1, 2):  # Every other span
                x1 = towers_x[tower_idx]
                x2 = towers_x[tower_idx + 2] if tower_idx + 2 < len(towers_x) else towers_x[-1]
                
                z_span = 115.0 + dy  # Ground wires at top of tower
                
                wire = {
                    'id': wire_id,
                    'x1': x1,
                    'y1': 0.0,
                    'z1': z_span,
                    'x2': x2,
                    'y2': 0.0,
                    'z2': z_span,
                    'diameter': 0.01,  # 10 mm
                    'mass_density': ground_mass_per_m,
                    'drag_coeff': 1.0,
                    'resistance': 0.05 / 1000,
                    'emissivity': 0.5,
                    'absorptivity': 0.5,
                    'current': 0.0,  # Carries fault current only
                    'type': f'Ground_{gnd_idx}',
                }
                wires.append(wire)
                wire_id += 1
        
        return wires
    
    def write_files(self):
        """Write terrain and transmission line CSV files."""
        # Generate terrain
        terrain_points, _ = self.generate_terrain()
        terrain_file = os.path.join(self.output_dir, 'altamont_terrain.csv')
        with open(terrain_file, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['x', 'y', 'z'])
            writer.writerows(terrain_points)
        print(f"Wrote terrain to {terrain_file}")
        
        # Generate and write transmission line
        wires = self.generate_transmission_line()
        wires_file = os.path.join(self.output_dir, 'altamont_wires.csv')
        with open(wires_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=[
                'id', 'x1', 'y1', 'z1', 'x2', 'y2', 'z2',
                'diameter', 'mass_density', 'drag_coeff',
                'resistance', 'emissivity', 'absorptivity', 'current', 'type'
            ])
            writer.writeheader()
            writer.writerows(wires)
        print(f"Wrote transmission line to {wires_file}")
        
        # Write input file for wind solver
        inputs_file = os.path.join(self.output_dir, 'inputs_altamont.i')
        inputs_content = """
# Altamont Pass 500 kV Transmission Line Scenario
# Gap flow wind acceleration through narrow valley pass
# Expected wind speeds: 1.5-3x ambient (40+ m/s peak in pass core)

terrain_file = altamont_terrain.csv

# Enable wire (transmission line) loading assessment
enable_wire_loading = true
wire_file = altamont_wires.csv
wire_output_file = altamont_wire_output.csv

# Reference wind: 12 m/s from W-NW at 10 m AGL (typical afternoon condition)
# Gap flow can amplify this to 25-30+ m/s at mid-pass
U_ref = 12.0
V_ref = 0.0
z_ref = 10.0

# Aerodynamic roughness length [m]
# Altamont grassland: z0 ~ 0.15 m (intermediate)
z0 = 0.15

# Gap flow model enables pressure-driven channeling
enable_gap_flow = true
gap_flow_width = 5000.0
gap_flow_length = 50000.0

# Grid spacing [m]
# Finer in horizontal to resolve gap geometry
dx = 500.0
dy = 200.0
dz = 50.0

# Domain extent
domain_height = 500.0

# Anisotropy for terrain steering
alpha_h = 1.2
alpha_v = 0.8
enable_cell_local_anisotropy = true

# Stability: assume neutral to slightly unstable (afternoon heating)
enable_stability_correction = false

# MLMG solver
mlmg_verbose = 1
max_grid_size = 64

# Output
plot_file = plt_altamont
num_time_steps = 1
"""
        with open(inputs_file, 'w') as f:
            f.write(inputs_content)
        print(f"Wrote input file to {inputs_file}")
        
        print("\n" + "="*70)
        print("Altamont Pass Scenario Generated Successfully")
        print("="*70)
        print("\nKey Features:")
        print("  - 120 km domain covering gap flow region")
        print("  - 500 kV transmission line with 3 phases + 2 ground wires")
        print("  - ~300 transmission line spans (350 m spacing)")
        print("  - Realistic Altamont Pass terrain (gap constriction)")
        print("  - Gap flow model for pressure-driven wind acceleration")
        print("\nPhysics Capabilities:")
        print("  - Mass-consistent divergence-free flow enforcement")
        print("  - Local wind speed amplification in narrow pass")
        print("  - Conductor temperature coupling to wind speed")
        print("  - Dynamic ampacity rating estimation")
        print("  - Sag and tension calculation under wind load")
        print("\nAdvantages vs. NOAA/NREL:")
        print("  1. Resolves gap flow physics explicitly (vs. 10x10 km NOAA grid)")
        print("  2. Continuous along transmission line route (vs. point estimates)")
        print("  3. Thermally coupled conductor analysis (vs. static ratings)")
        print("  4. Real-time operational decision support")
        print("="*70)


if __name__ == '__main__':
    import sys
    output_dir = sys.argv[1] if len(sys.argv) > 1 else '.'
    generator = AltamontScenarioGenerator(output_dir)
    generator.write_files()
