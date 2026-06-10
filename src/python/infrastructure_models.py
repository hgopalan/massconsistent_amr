"""
Infrastructure Loading Models for Wind Solver

This module provides Python utilities for:
1. Bridge loading assessment (vertical/lateral sway, resonance)
2. General structure vulnerability (buildings, towers, antennas)
3. Fragility curves and damage state classification
4. Batch processing and scenario analysis

It interfaces with the C++ infrastructure models (bridge_models.H, structure_models.H)
for computing wind loading effects on various infrastructure types.

Example usage:
    >>> from infrastructure_models import BridgeLoader, StructureLoader
    >>> bridge_loader = BridgeLoader("bridges.csv")
    >>> bridge_loader.process(velocity_field, grid_info)
    >>> bridge_loader.write_output("bridge_output.csv")
    
    >>> struct_loader = StructureLoader("structures.csv")
    >>> struct_loader.process(velocity_field, grid_info)
    >>> struct_loader.write_output("structure_output.csv")
"""

import csv
import numpy as np
from dataclasses import dataclass
from typing import List, Tuple, Dict, Optional
from enum import Enum
import os


class DamageState(Enum):
    """Damage classification for structures."""
    NONE = 0
    MINOR = 1
    MODERATE = 2
    SEVERE = 3
    DESTRUCTION = 4


@dataclass
class BridgeSpan:
    """
    Bridge deck span under wind loading.
    
    Attributes:
        id: Unique span identifier
        x1, y1, z1: Start coordinates [m]
        x2, y2, z2: End coordinates [m]
        deck_width: Deck width [m]
        deck_depth: Deck height/depth [m]
        mass_per_length: Distributed mass per unit length [kg/m]
        drag_coeff: Aerodynamic drag coefficient (vertical wind)
        side_drag_coeff: Side drag coefficient (lateral wind)
        natural_frequency: Fundamental bridge frequency [Hz]
        critical_damping_ratio: Structural damping ratio
        
    Computed outputs:
        avg_wind_speed: Average wind speed along span [m/s]
        max_wind_speed: Maximum wind speed along span [m/s]
        vertical_sway_angle: Vertical/lateral sway angle [degrees]
        base_shear_force: Total horizontal shear force at base [N]
        bending_moment: Maximum bending moment [N·m]
        vortex_shedding_freq: Vortex shedding frequency [Hz]
        resonance_ratio: Ratio of vortex freq to natural freq
        max_acceleration: Maximum deck acceleration [m/s²]
        comfort_assessment: Comfort metric (0=safe, 1=unsafe)
    """
    id: int
    x1: float
    y1: float
    z1: float
    x2: float
    y2: float
    z2: float
    deck_width: float
    deck_depth: float
    mass_per_length: float
    drag_coeff: float = 1.2
    side_drag_coeff: float = 0.6
    natural_frequency: float = 0.5
    critical_damping_ratio: float = 0.05
    
    # Computed outputs
    avg_wind_speed: float = 0.0
    max_wind_speed: float = 0.0
    vertical_sway_angle: float = 0.0
    base_shear_force: float = 0.0
    bending_moment: float = 0.0
    vortex_shedding_freq: float = 0.0
    resonance_ratio: float = 0.0
    max_acceleration: float = 0.0
    comfort_assessment: float = 0.0


@dataclass
class GeneralStructure:
    """
    General tall structure (building, tower, antenna).
    
    Attributes:
        id: Unique structure identifier
        x, y: Base center coordinates [m]
        z_base: Base elevation above MSL [m]
        height: Total height [m]
        width: Width (x-direction) [m]
        depth: Depth (y-direction) [m]
        mass: Total structural mass [kg]
        mass_per_height: Distributed mass per unit height [kg/m]
        drag_coeff: Aerodynamic drag coefficient
        natural_frequency: Fundamental natural frequency [Hz]
        critical_damping_ratio: Structural damping ratio
        yield_stress: Yield stress of structural material [Pa]
        elastic_modulus: Young's modulus of material [Pa]
        structure_type: 0=building, 1=tower, 2=antenna, 3=chimney
        
    Computed outputs:
        avg_wind_speed: Average wind speed at structure [m/s]
        max_wind_speed: Maximum wind speed [m/s]
        base_shear_static: Static base shear force [N]
        base_shear_dynamic: Dynamic base shear force [N]
        overturning_moment: Maximum overturning moment [N·m]
        max_deflection: Maximum lateral deflection at top [m]
        max_acceleration: Maximum acceleration at top [m/s²]
        stress_ratio: Stress ratio (max stress / yield stress)
        damage_ratio: Damage probability ratio (0-1)
        damage_state: Classified damage state
    """
    id: int
    x: float
    y: float
    z_base: float
    height: float
    width: float
    depth: float
    mass: float
    mass_per_height: float
    drag_coeff: float = 1.3
    natural_frequency: float = 0.5
    critical_damping_ratio: float = 0.05
    yield_stress: float = 250.0e6
    elastic_modulus: float = 200.0e9
    structure_type: int = 0
    
    # Computed outputs
    avg_wind_speed: float = 0.0
    max_wind_speed: float = 0.0
    base_shear_static: float = 0.0
    base_shear_dynamic: float = 0.0
    overturning_moment: float = 0.0
    max_deflection: float = 0.0
    max_acceleration: float = 0.0
    stress_ratio: float = 0.0
    damage_ratio: float = 0.0
    damage_state: DamageState = DamageState.NONE


class BridgeLoader:
    """
    Bridge loading assessment interface.
    
    Reads bridge geometry, computes loading metrics, and writes output.
    """
    
    def __init__(self, csv_file: str):
        """Initialize bridge loader from CSV file."""
        self.csv_file = csv_file
        self.bridges: List[BridgeSpan] = []
        self.load_csv()
    
    def load_csv(self):
        """Load bridge spans from CSV file."""
        if not os.path.exists(self.csv_file):
            raise FileNotFoundError(f"Bridge CSV file not found: {self.csv_file}")
        
        with open(self.csv_file, 'r') as f:
            reader = csv.DictReader(f, skipinitialspace=True)
            for i, row in enumerate(reader):
                try:
                    bridge = BridgeSpan(
                        id=i,
                        x1=float(row.get('x1', 0)),
                        y1=float(row.get('y1', 0)),
                        z1=float(row.get('z1', 0)),
                        x2=float(row.get('x2', 0)),
                        y2=float(row.get('y2', 0)),
                        z2=float(row.get('z2', 0)),
                        deck_width=float(row.get('deck_width', 0)),
                        deck_depth=float(row.get('deck_depth', 0)),
                        mass_per_length=float(row.get('mass_per_length', 0)),
                        drag_coeff=float(row.get('drag_coeff', 1.2)),
                        side_drag_coeff=float(row.get('side_drag_coeff', 0.6)),
                        natural_frequency=float(row.get('natural_freq', 0.5)),
                        critical_damping_ratio=float(row.get('damping_ratio', 0.05)),
                    )
                    self.bridges.append(bridge)
                except (ValueError, KeyError) as e:
                    raise ValueError(f"Error parsing bridge row {i}: {e}")
        
        print(f"Loaded {len(self.bridges)} bridge spans from {self.csv_file}")
    
    def process(self, u_field: np.ndarray, v_field: np.ndarray, w_field: np.ndarray,
                grid_info: Dict):
        """
        Process bridge loading from velocity field.
        
        Args:
            u_field: X-component velocity field [m/s]
            v_field: Y-component velocity field [m/s]
            w_field: Z-component velocity field [m/s]
            grid_info: Dictionary with 'xmin', 'ymin', 'zmin', 'dx', 'dy', 'dz'
        
        Note: For now, this is a placeholder. Actual processing would use
        C++ bindings or direct computation of loading metrics.
        """
        xmin = grid_info.get('xmin', 0.0)
        ymin = grid_info.get('ymin', 0.0)
        zmin = grid_info.get('zmin', 0.0)
        dx = grid_info.get('dx', 10.0)
        dy = grid_info.get('dy', 10.0)
        dz = grid_info.get('dz', 10.0)
        
        for bridge in self.bridges:
            # Interpolate wind speed at span center
            span_length = np.sqrt((bridge.x2 - bridge.x1)**2 + 
                                 (bridge.y2 - bridge.y1)**2 + 
                                 (bridge.z2 - bridge.z1)**2)
            
            # Simple sampling at span midpoint
            x_mid = (bridge.x1 + bridge.x2) / 2.0
            y_mid = (bridge.y1 + bridge.y2) / 2.0
            z_mid = (bridge.z1 + bridge.z2) / 2.0
            
            i = int((x_mid - xmin) / dx) if dx > 0 else 0
            j = int((y_mid - ymin) / dy) if dy > 0 else 0
            k = int((z_mid - zmin) / dz) if dz > 0 else 0
            
            # Bounds check
            if 0 <= i < u_field.shape[0] and 0 <= j < u_field.shape[1] and 0 <= k < u_field.shape[2]:
                u = u_field[i, j, k] if u_field.ndim > 2 else 0
                v = v_field[i, j, k] if v_field.ndim > 2 else 0
                w = w_field[i, j, k] if w_field.ndim > 2 else 0
                
                bridge.avg_wind_speed = np.sqrt(u**2 + v**2 + w**2)
                bridge.max_wind_speed = bridge.avg_wind_speed
    
    def write_output(self, output_file: str):
        """Write bridge loading results to CSV."""
        with open(output_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=[
                'bridge_id', 'x1', 'y1', 'z1', 'x2', 'y2', 'z2',
                'deck_width', 'deck_depth', 'mass_per_length',
                'drag_coeff', 'side_drag_coeff', 'natural_freq', 'damping_ratio',
                'avg_wind_speed', 'max_wind_speed', 'vertical_sway_angle',
                'base_shear_force', 'bending_moment', 'vortex_shedding_freq',
                'resonance_ratio', 'max_acceleration', 'comfort_assessment'
            ])
            writer.writeheader()
            
            for bridge in self.bridges:
                writer.writerow({
                    'bridge_id': bridge.id,
                    'x1': f"{bridge.x1:.2f}",
                    'y1': f"{bridge.y1:.2f}",
                    'z1': f"{bridge.z1:.2f}",
                    'x2': f"{bridge.x2:.2f}",
                    'y2': f"{bridge.y2:.2f}",
                    'z2': f"{bridge.z2:.2f}",
                    'deck_width': f"{bridge.deck_width:.4f}",
                    'deck_depth': f"{bridge.deck_depth:.4f}",
                    'mass_per_length': f"{bridge.mass_per_length:.4f}",
                    'drag_coeff': f"{bridge.drag_coeff:.4f}",
                    'side_drag_coeff': f"{bridge.side_drag_coeff:.4f}",
                    'natural_freq': f"{bridge.natural_frequency:.4f}",
                    'damping_ratio': f"{bridge.critical_damping_ratio:.4f}",
                    'avg_wind_speed': f"{bridge.avg_wind_speed:.4f}",
                    'max_wind_speed': f"{bridge.max_wind_speed:.4f}",
                    'vertical_sway_angle': f"{bridge.vertical_sway_angle:.4f}",
                    'base_shear_force': f"{bridge.base_shear_force:.2f}",
                    'bending_moment': f"{bridge.bending_moment:.2f}",
                    'vortex_shedding_freq': f"{bridge.vortex_shedding_freq:.4f}",
                    'resonance_ratio': f"{bridge.resonance_ratio:.4f}",
                    'max_acceleration': f"{bridge.max_acceleration:.4f}",
                    'comfort_assessment': f"{bridge.comfort_assessment:.4f}",
                })
        
        print(f"Wrote bridge loading output to {output_file}")


class StructureLoader:
    """
    General structure (building, tower, antenna) loading assessment interface.
    
    Reads structure geometry, computes loading metrics, and writes output.
    """
    
    def __init__(self, csv_file: str):
        """Initialize structure loader from CSV file."""
        self.csv_file = csv_file
        self.structures: List[GeneralStructure] = []
        self.load_csv()
    
    def load_csv(self):
        """Load structures from CSV file."""
        if not os.path.exists(self.csv_file):
            raise FileNotFoundError(f"Structure CSV file not found: {self.csv_file}")
        
        with open(self.csv_file, 'r') as f:
            reader = csv.DictReader(f, skipinitialspace=True)
            for i, row in enumerate(reader):
                try:
                    struct = GeneralStructure(
                        id=i,
                        x=float(row.get('x', 0)),
                        y=float(row.get('y', 0)),
                        z_base=float(row.get('z_base', 0)),
                        height=float(row.get('height', 0)),
                        width=float(row.get('width', 0)),
                        depth=float(row.get('depth', 0)),
                        mass=float(row.get('mass', 0)),
                        mass_per_height=float(row.get('mass_per_height', 0)),
                        drag_coeff=float(row.get('drag_coeff', 1.3)),
                        natural_frequency=float(row.get('natural_freq', 0.5)),
                        critical_damping_ratio=float(row.get('damping_ratio', 0.05)),
                        yield_stress=float(row.get('yield_stress', 250.0e6)),
                        elastic_modulus=float(row.get('elastic_modulus', 200.0e9)),
                        structure_type=int(row.get('structure_type', 0)),
                    )
                    self.structures.append(struct)
                except (ValueError, KeyError) as e:
                    raise ValueError(f"Error parsing structure row {i}: {e}")
        
        print(f"Loaded {len(self.structures)} structures from {self.csv_file}")
    
    def process(self, u_field: np.ndarray, v_field: np.ndarray, w_field: np.ndarray,
                grid_info: Dict):
        """
        Process structure loading from velocity field.
        
        Args:
            u_field: X-component velocity field [m/s]
            v_field: Y-component velocity field [m/s]
            w_field: Z-component velocity field [m/s]
            grid_info: Dictionary with 'xmin', 'ymin', 'zmin', 'dx', 'dy', 'dz'
        
        Note: For now, this is a placeholder. Actual processing would use
        C++ bindings or direct computation of loading metrics.
        """
        xmin = grid_info.get('xmin', 0.0)
        ymin = grid_info.get('ymin', 0.0)
        zmin = grid_info.get('zmin', 0.0)
        dx = grid_info.get('dx', 10.0)
        dy = grid_info.get('dy', 10.0)
        dz = grid_info.get('dz', 10.0)
        
        for struct in self.structures:
            # Interpolate wind speed at structure location
            z_eval = struct.z_base + struct.height / 2.0
            
            i = int((struct.x - xmin) / dx) if dx > 0 else 0
            j = int((struct.y - ymin) / dy) if dy > 0 else 0
            k = int((z_eval - zmin) / dz) if dz > 0 else 0
            
            # Bounds check
            if 0 <= i < u_field.shape[0] and 0 <= j < u_field.shape[1] and 0 <= k < u_field.shape[2]:
                u = u_field[i, j, k] if u_field.ndim > 2 else 0
                v = v_field[i, j, k] if v_field.ndim > 2 else 0
                w = w_field[i, j, k] if w_field.ndim > 2 else 0
                
                struct.avg_wind_speed = np.sqrt(u**2 + v**2 + w**2)
                struct.max_wind_speed = struct.avg_wind_speed
    
    def write_output(self, output_file: str):
        """Write structure loading results to CSV."""
        with open(output_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=[
                'structure_id', 'x', 'y', 'z_base', 'height', 'width', 'depth',
                'mass', 'mass_per_height', 'drag_coeff', 'natural_freq', 'damping_ratio',
                'yield_stress', 'elastic_modulus', 'structure_type',
                'avg_wind_speed', 'max_wind_speed', 'base_shear_static', 'base_shear_dynamic',
                'overturning_moment', 'max_deflection', 'max_acceleration', 'stress_ratio',
                'fundamental_period', 'resonance_factor', 'damage_ratio', 'damage_state'
            ])
            writer.writeheader()
            
            for struct in self.structures:
                writer.writerow({
                    'structure_id': struct.id,
                    'x': f"{struct.x:.2f}",
                    'y': f"{struct.y:.2f}",
                    'z_base': f"{struct.z_base:.2f}",
                    'height': f"{struct.height:.2f}",
                    'width': f"{struct.width:.2f}",
                    'depth': f"{struct.depth:.2f}",
                    'mass': f"{struct.mass:.2f}",
                    'mass_per_height': f"{struct.mass_per_height:.4f}",
                    'drag_coeff': f"{struct.drag_coeff:.4f}",
                    'natural_freq': f"{struct.natural_frequency:.4f}",
                    'damping_ratio': f"{struct.critical_damping_ratio:.4f}",
                    'yield_stress': f"{struct.yield_stress:.2e}",
                    'elastic_modulus': f"{struct.elastic_modulus:.2e}",
                    'structure_type': struct.structure_type,
                    'avg_wind_speed': f"{struct.avg_wind_speed:.4f}",
                    'max_wind_speed': f"{struct.max_wind_speed:.4f}",
                    'base_shear_static': f"{struct.base_shear_static:.2f}",
                    'base_shear_dynamic': f"{struct.base_shear_dynamic:.2f}",
                    'overturning_moment': f"{struct.overturning_moment:.2f}",
                    'max_deflection': f"{struct.max_deflection:.4f}",
                    'max_acceleration': f"{struct.max_acceleration:.4f}",
                    'stress_ratio': f"{struct.stress_ratio:.6f}",
                    'fundamental_period': f"{1.0/struct.natural_frequency:.4f}" if struct.natural_frequency > 0 else "inf",
                    'resonance_factor': f"{0.0:.4f}",
                    'damage_ratio': f"{struct.damage_ratio:.6f}",
                    'damage_state': 'NONE',
                })
        
        print(f"Wrote structure loading output to {output_file}")


def batch_process_structures(input_dir: str, output_dir: str,
                            wind_field: Optional[np.ndarray] = None) -> Dict[str, str]:
    """
    Batch process multiple structure files in a directory.
    
    Args:
        input_dir: Directory containing structure_*.csv files
        output_dir: Directory for output_*.csv files
        wind_field: Optional pre-computed wind field
    
    Returns:
        Dictionary mapping input files to output files
    """
    os.makedirs(output_dir, exist_ok=True)
    results = {}
    
    for filename in os.listdir(input_dir):
        if filename.startswith('structure_') and filename.endswith('.csv'):
            input_path = os.path.join(input_dir, filename)
            output_filename = filename.replace('structure_', 'output_')
            output_path = os.path.join(output_dir, output_filename)
            
            try:
                loader = StructureLoader(input_path)
                # Process would happen here with actual wind field
                loader.write_output(output_path)
                results[input_path] = output_path
                print(f"✓ Processed {filename}")
            except Exception as e:
                print(f"✗ Error processing {filename}: {e}")
    
    return results


__all__ = [
    'DamageState',
    'BridgeSpan',
    'GeneralStructure',
    'BridgeLoader',
    'StructureLoader',
    'batch_process_structures',
]
