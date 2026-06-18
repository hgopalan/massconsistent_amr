#!/usr/bin/env python3
"""
turbine_io.py - Turbine layout I/O utilities (CSV format support)

Provides functions to read and write turbine layouts in CSV format for
interoperability with wind farm optimization tools.

CSV Format:
    turbine_id, x_m, y_m, z_agl_m, turbine_type, hub_height, rotor_diameter, power_curve_file
    0, 100.0, 200.0, 0.0, DTU10MW, 90.0, 178.0, power_curves/dtu10mw.json
    1, 500.0, 200.0, 50.0, DTU10MW, 90.0, 178.0, power_curves/dtu10mw.json
"""

import csv
import json
from pathlib import Path
from typing import List, Dict, Any, Optional, Tuple


class TurbineLayout:
    """
    Container for turbine layout with I/O methods.
    
    Attributes:
        turbines (list): List of turbine dictionaries with keys:
                        id, x, y, z_agl, turbine_type, hub_height, rotor_diameter, power_curve_file
        domain_bounds (dict, optional): Domain bounds (xmin, xmax, ymin, ymax)
    """
    
    def __init__(self):
        """Initialize empty turbine layout."""
        self.turbines: List[Dict[str, Any]] = []
        self.domain_bounds: Optional[Dict[str, float]] = None
    
    def add_turbine(
        self,
        turbine_id: int,
        x: float,
        y: float,
        z_agl: float = 0.0,
        turbine_type: str = "generic",
        hub_height: float = 90.0,
        rotor_diameter: float = 100.0,
        power_curve_file: Optional[str] = None,
        **kwargs
    ) -> None:
        """
        Add a turbine to the layout.
        
        Parameters:
            turbine_id (int): Unique turbine identifier
            x, y (float): Horizontal position in meters
            z_agl (float): Height above ground level in meters (default: 0.0)
            turbine_type (str): Turbine model name (default: "generic")
            hub_height (float): Hub height AGL in meters (default: 90.0)
            rotor_diameter (float): Rotor diameter in meters (default: 100.0)
            power_curve_file (str, optional): Path to power curve JSON/CSV
            **kwargs: Additional turbine properties
        """
        turbine = {
            'id': turbine_id,
            'x': float(x),
            'y': float(y),
            'z_agl': float(z_agl),
            'turbine_type': str(turbine_type),
            'hub_height': float(hub_height),
            'rotor_diameter': float(rotor_diameter),
            'power_curve_file': power_curve_file,
            **kwargs
        }
        self.turbines.append(turbine)
    
    @staticmethod
    def read_csv(csv_file: str) -> 'TurbineLayout':
        """
        Read turbine layout from CSV file.
        
        Parameters:
            csv_file (str): Path to CSV file
        
        Returns:
            TurbineLayout: Populated layout object
        
        Raises:
            IOError: If file not found or parsing fails
            ValueError: If required columns missing or data invalid
        """
        layout = TurbineLayout()
        
        with open(csv_file, 'r', newline='') as f:
            reader = csv.DictReader(f)
            
            if not reader.fieldnames:
                raise ValueError("CSV file is empty or has no header")
            
            # Validate required columns
            required_cols = {'turbine_id', 'x_m', 'y_m'}
            if not required_cols.issubset(set(reader.fieldnames)):
                raise ValueError(
                    f"CSV must contain columns: {required_cols}. "
                    f"Found: {set(reader.fieldnames)}"
                )
            
            for row_num, row in enumerate(reader, start=2):
                try:
                    turbine_id = int(row['turbine_id'].strip())
                    x = float(row['x_m'].strip())
                    y = float(row['y_m'].strip())
                    
                    # Optional columns with defaults
                    z_agl = float(row.get('z_agl_m', '0.0').strip() or '0.0')
                    turbine_type = row.get('turbine_type', 'generic').strip() or 'generic'
                    hub_height = float(row.get('hub_height', '90.0').strip() or '90.0')
                    rotor_diameter = float(row.get('rotor_diameter', '100.0').strip() or '100.0')
                    power_curve_file = row.get('power_curve_file', '').strip() or None
                    
                    layout.add_turbine(
                        turbine_id=turbine_id,
                        x=x,
                        y=y,
                        z_agl=z_agl,
                        turbine_type=turbine_type,
                        hub_height=hub_height,
                        rotor_diameter=rotor_diameter,
                        power_curve_file=power_curve_file
                    )
                except (ValueError, KeyError) as e:
                    raise ValueError(f"Error parsing row {row_num}: {e}")
        
        return layout
    
    @staticmethod
    def write_csv(layout: 'TurbineLayout', csv_file: str) -> None:
        """
        Write turbine layout to CSV file.
        
        Parameters:
            layout (TurbineLayout): Turbine layout to write
            csv_file (str): Output CSV filename
        
        Returns:
            None (writes to file)
        """
        if not layout.turbines:
            raise ValueError("Layout has no turbines to write")
        
        fieldnames = [
            'turbine_id', 'x_m', 'y_m', 'z_agl_m',
            'turbine_type', 'hub_height', 'rotor_diameter', 'power_curve_file'
        ]
        
        with open(csv_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            
            for turbine in layout.turbines:
                row = {
                    'turbine_id': turbine.get('id', ''),
                    'x_m': f"{turbine.get('x', 0.0):.2f}",
                    'y_m': f"{turbine.get('y', 0.0):.2f}",
                    'z_agl_m': f"{turbine.get('z_agl', 0.0):.2f}",
                    'turbine_type': turbine.get('turbine_type', 'generic'),
                    'hub_height': f"{turbine.get('hub_height', 90.0):.1f}",
                    'rotor_diameter': f"{turbine.get('rotor_diameter', 100.0):.1f}",
                    'power_curve_file': turbine.get('power_curve_file', '') or ''
                }
                writer.writerow(row)
    
    def validate_spacing(self, min_spacing: float = 400.0) -> Tuple[bool, List[str]]:
        """
        Validate turbine spacing and domain bounds.
        
        Parameters:
            min_spacing (float): Minimum allowed spacing between turbines (meters)
        
        Returns:
            (bool, list): (is_valid, list_of_errors)
        """
        errors = []
        
        # Check pairwise spacing
        for i, t1 in enumerate(self.turbines):
            for j, t2 in enumerate(self.turbines):
                if i >= j:
                    continue
                dist = ((t1['x'] - t2['x'])**2 + (t1['y'] - t2['y'])**2)**0.5
                if dist < min_spacing:
                    errors.append(
                        f"Turbines {t1['id']} and {t2['id']} are too close: "
                        f"{dist:.1f}m < {min_spacing}m"
                    )
        
        # Check domain bounds if specified
        if self.domain_bounds:
            for turbine in self.turbines:
                x, y = turbine['x'], turbine['y']
                if not (self.domain_bounds['xmin'] <= x <= self.domain_bounds['xmax']):
                    errors.append(
                        f"Turbine {turbine['id']} x-coordinate {x} outside domain "
                        f"[{self.domain_bounds['xmin']}, {self.domain_bounds['xmax']}]"
                    )
                if not (self.domain_bounds['ymin'] <= y <= self.domain_bounds['ymax']):
                    errors.append(
                        f"Turbine {turbine['id']} y-coordinate {y} outside domain "
                        f"[{self.domain_bounds['ymin']}, {self.domain_bounds['ymax']}]"
                    )
        
        return len(errors) == 0, errors
    
    def __len__(self) -> int:
        """Return number of turbines in layout."""
        return len(self.turbines)
    
    def __repr__(self) -> str:
        """String representation."""
        return f"TurbineLayout(n_turbines={len(self.turbines)})"
