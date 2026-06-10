"""
Urban Heat Island Building Scenario - Wind Channeling & Thermal Effects

This scenario demonstrates structure vulnerability assessment for a tall building
in an urban environment where street canyon geometry and thermal effects interact
to modify wind loading and building response.

Physical Context:
    - Urban heat island effect: +2-8°C above rural surroundings
    - Street canyon geometry: tall buildings create corridor effects
    - Wind channeling: funneling through street canyons
    - Vorticity generation: corner vortices and shear layers
    - Thermal plumes: buoyant air rising above heated surfaces
    - Boundary layer modification: roughness and thermal effects
    - Building clustering: multiple tall structures interact

Example scenarios:
    - Tokyo: canyon winds 2-4x ambient (10 m/s → 25+ m/s)
    - Manhattan: 400+ m buildings with UHI ΔT = +5°C
    - Hong Kong: typhoon channeling through Victoria Gap
    - London: street-level winds 40% higher than suburban

Building specifications (example: 200 m commercial tower):
    - Height: 200 m (50 stories)
    - Base: 50×50 m
    - Mass: 50,000 tons
    - Fundamental frequency: 0.2-0.3 Hz
    - Damping ratio: 1-3% (tuned mass damper)
    - Yield stress: 350 MPa (modern steel)
    - Critical concern: wind-induced sway comfort, vortex resonance

Why mass-consistent solver excels over NOAA/NREL:
    1. Street canyon pressure-driven wind acceleration
    2. Buoyancy effects from urban heat island (ΔT-driven)
    3. Building-induced flow modification (wake interaction)
    4. Vertical wind shear from thermal plumes
    5. Terrain-dependent roughness and displacement height
    6. Coupled thermal-mechanical effects on structural response
    7. Real-time building comfort/safety metrics

Expected outcomes:
    - 40-100% wind speed amplification in street canyons
    - Thermal buoyancy reducing vertical wind speed
    - Corner vortices causing lateral gust peaks
    - Lateral sway: 0.3-0.8 m on 200 m tower
    - Peak acceleration: 0.1-0.3 m/s² (comfort limit ~0.2 m/s²)
    - Resonance risk: gust frequency matching building frequency (0.2-0.3 Hz)
"""

import numpy as np
import csv
import os
from typing import List, Tuple

class UrbanBuildingScenarioGenerator:
    """Generate urban heat island building test case."""
    
    def __init__(self, output_dir: str = "."):
        """Initialize scenario generator."""
        self.output_dir = output_dir
        os.makedirs(output_dir, exist_ok=True)
    
    def generate_urban_terrain(self, nx: int = 101, ny: int = 101) -> Tuple[List, np.ndarray]:
        """
        Generate urban environment with buildings and street canyon geometry.
        
        Layout:
            - Regular grid of buildings: 10 m ground level, 100-200 m heights
            - Street canyons: 30-40 m width (N-S and E-W aligned)
            - Target building: 200 m tall at center (50×50 m footprint)
            - Surrounding buildings: 100-150 m (realistic Manhattan/London block)
            - Height variation: creates pressure-driven wind tunneling
        
        Args:
            nx, ny: Grid dimensions
        
        Returns:
            (points_list, elevation_array)
        """
        x = np.linspace(-2.5, 2.5, nx)   # ±2.5 km (urban block ~2×2 km)
        y = np.linspace(-2.5, 2.5, ny)
        
        terrain_points = []
        elevation_map = np.zeros((nx, ny))
        
        def building_height(xi, yj):
            """Compute building height at position."""
            # Ground level
            z = 0.0
            
            # Center building: 200 m tall, 50×50 m footprint
            if np.abs(xi) < 0.025 and np.abs(yj) < 0.025:
                return 200.0
            
            # Surrounding building grid: 100-150 m tall
            # Buildings arranged in ~300 m blocks (typical urban grid)
            building_spacing = 0.3  # km = 300 m
            building_height_base = 120.0
            
            # Snap to grid
            grid_i = np.round(xi / building_spacing)
            grid_j = np.round(yj / building_spacing)
            
            # Distance to nearest grid point
            dist_i = np.abs(xi - grid_i * building_spacing)
            dist_j = np.abs(yj - grid_j * building_spacing)
            
            # Building footprint: ~50×50 m = 0.05×0.05 km (half block in plan)
            building_width = 0.05
            
            if dist_i < building_width and dist_j < building_width:
                # Height varies by location (taller in some blocks)
                height_var = 20.0 * (np.sin(grid_i * np.pi) + np.cos(grid_j * np.pi)) / 2.0
                return building_height_base + height_var
            else:
                return 0.0  # Street level
        
        for i, xi in enumerate(x):
            for j, yj in enumerate(y):
                z = building_height(xi, yj)
                elevation_map[i, j] = z
                terrain_points.append([xi, yj, z])
        
        return terrain_points, elevation_map
    
    def generate_target_building(self) -> dict:
        """
        Generate target tall building (200 m commercial tower).
        
        Located at center of urban block with surrounding shorter buildings.
        
        Returns:
            Building specification dictionary
        """
        building = {
            'id': 0,
            'x': 0.0,           # Center of domain
            'y': 0.0,
            'z_base': 0.0,      # Ground level (10 m to accounting for street elevation)
            'height': 200.0,    # 200 m (50 stories, ~4 m per story)
            'width': 50.0,      # m (N-S extent)
            'depth': 50.0,      # m (E-W extent)
            'mass': 50000.0e3,  # 50,000 tons = 50 million kg
            'mass_per_height': 250.0e3,  # kg/m (distributed over height)
            'drag_coeff': 1.3,  # Typical for square building
            'natural_frequency': 0.25,  # Hz (0.2-0.3 Hz for 200m tower)
            'critical_damping_ratio': 0.02,  # 2% (with tuned mass damper)
            'yield_stress': 350.0e6,  # Pa (modern high-strength steel)
            'elastic_modulus': 210.0e9,  # Pa (steel)
            'structure_type': 0,  # Building
            'description': 'Tall commercial tower in urban block',
        }
        return building
    
    def write_files(self):
        """Write terrain and building CSV files."""
        # Generate terrain
        terrain_points, _ = self.generate_urban_terrain()
        terrain_file = os.path.join(self.output_dir, 'urban_terrain.csv')
        with open(terrain_file, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['x', 'y', 'z'])
            writer.writerows(terrain_points)
        print(f"Wrote terrain to {terrain_file}")
        
        # Generate and write building
        building = self.generate_target_building()
        structures_file = os.path.join(self.output_dir, 'urban_building.csv')
        with open(structures_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=[
                'id', 'x', 'y', 'z_base', 'height', 'width', 'depth',
                'mass', 'mass_per_height', 'drag_coeff', 'natural_freq',
                'damping_ratio', 'yield_stress', 'elastic_modulus', 'structure_type'
            ])
            writer.writeheader()
            writer.writerow({
                'id': building['id'],
                'x': building['x'],
                'y': building['y'],
                'z_base': building['z_base'],
                'height': building['height'],
                'width': building['width'],
                'depth': building['depth'],
                'mass': building['mass'],
                'mass_per_height': building['mass_per_height'],
                'drag_coeff': building['drag_coeff'],
                'natural_freq': building['natural_frequency'],
                'damping_ratio': building['critical_damping_ratio'],
                'yield_stress': building['yield_stress'],
                'elastic_modulus': building['elastic_modulus'],
                'structure_type': building['structure_type'],
            })
        print(f"Wrote building to {structures_file}")
        
        # Write input file
        inputs_file = os.path.join(self.output_dir, 'inputs_urban.i')
        inputs_content = """
# Urban Heat Island Building Scenario
# Street canyon wind channeling with thermal effects
# 200 m commercial tower in Manhattan/London-like urban block
# Expected sway: 0.3-0.8 m, peak acceleration 0.1-0.3 m/s²

terrain_file = urban_terrain.csv

# Enable structure (building) loading
enable_structure_loading = true
structure_file = urban_building.csv
structure_output_file = urban_building_output.csv

# Reference wind: 10 m/s ambient at 10 m AGL
# Street canyon can amplify to 15-25 m/s at building height
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0

# Urban roughness: tall building cluster
# Displacement height: ~30-40 m (effective zero for wind profile)
z0 = 1.0

# Street canyon geometry recognition
enable_street_canyon = true
street_canyon_reduction = 0.3

# Urban canopy roughness model
enable_canopy = true
canopy_height = 100.0      # Mean building height
frontal_area_index = 0.35  # Typical urban block (0.3-0.5)
plan_area_index = 0.25
canopy_drag_coeff = 0.2
canopy_attenuation = 2.5

# Thermal effects: urban heat island (+3°C above rural)
# Drives buoyancy and modifies vertical wind structure
enable_buoyancy = true
surface_sensible_heat_flux = 200.0  # W/m² (typical urban, daylight)

# Stability: near-neutral (urban heat reduces stability class)
enable_stability_correction = true
stability_length = 200.0  # Reduced for urban (shorter scaling length)

# Grid spacing [m]
# Fine resolution to resolve street canyons
dx = 50.0
dy = 50.0
dz = 25.0

# Domain extent
domain_height = 500.0

# Anisotropy: strong vertical effect from buildings
alpha_h = 1.2
alpha_v = 0.6
enable_cell_local_anisotropy = true

# MLMG solver
mlmg_verbose = 1
max_grid_size = 32

# Output
plot_file = plt_urban
num_time_steps = 1
"""
        with open(inputs_file, 'w') as f:
            f.write(inputs_content)
        print(f"Wrote input file to {inputs_file}")
        
        print("\n" + "="*70)
        print("Urban Heat Island Building Scenario Generated Successfully")
        print("="*70)
        print("\nKey Features:")
        print("  - 5 km×5 km urban block domain")
        print("  - Regular building grid: 100-150 m heights, 300 m spacing")
        print("  - Target tower: 200 m tall at domain center")
        print("  - Street canyons: 30-40 m width (channeling geometry)")
        print("  - Urban heat island effect: +3°C thermal buoyancy")
        print("\nPhysics Capabilities:")
        print("  - Street canyon wind acceleration modeling")
        print("  - Urban canopy drag parameterization")
        print("  - Thermal buoyancy effects (stability modification)")
        print("  - Building-induced pressure drag")
        print("  - Acceleration-based comfort/safety assessment")
        print("  - Wind-induced sway prediction (ISO 6954)")
        print("\nAdvantages vs. NOAA/NREL:")
        print("  1. Explicit street canyon geometry (vs. uniform roughness)")
        print("  2. Thermal coupling for urban heat island (vs. isothermal)")
        print("  3. Pressure-driven wind channeling (divergence enforced)")
        print("  4. Continuous building load profile (vs. point estimate)")
        print("  5. Real-time comfort/safety decision support")
        print("  6. Multi-building interaction effects captured")
        print("="*70)


if __name__ == '__main__':
    import sys
    output_dir = sys.argv[1] if len(sys.argv) > 1 else '.'
    generator = UrbanBuildingScenarioGenerator(output_dir)
    generator.write_files()
