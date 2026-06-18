#!/usr/bin/env python3
"""
pyoptimization_export.py - Export wind farm results for PyOptimization

Provides utilities to export wind farm simulation results in formats compatible
with Floris-PyOptimization for layout and control optimization workflows.
"""

import json
import csv
from pathlib import Path
from typing import Dict, List, Any, Optional
import numpy as np


class PyOptimizationExporter:
    """
    Exporter for wind farm results to PyOptimization-compatible format.
    
    Supports JSON and CSV export of:
    - Farm-level aggregated power output and AEP
    - Per-turbine power, wind speed, direction, Ct, and yaw angles
    - Wind resource statistics
    """
    
    def __init__(self, farm_name: str = "wind_farm"):
        """
        Initialize exporter.
        
        Parameters:
            farm_name (str): Name of the wind farm for metadata
        """
        self.farm_name = farm_name
        self.turbines: List[Dict[str, Any]] = []
        self.farm_power_kw: float = 0.0
        self.farm_aep_gwh: float = 0.0
        self.wind_resource: Optional[Dict[str, Any]] = None
        self.metadata: Dict[str, Any] = {}
    
    def add_turbine_result(
        self,
        turbine_id: int,
        x: float,
        y: float,
        power_kw: float,
        wind_speed_ms: float,
        wind_direction_deg: float,
        thrust_coefficient: float = 0.8,
        yaw_deg: float = 0.0,
        hub_height: float = 90.0,
        rotor_diameter: float = 100.0,
        turbine_type: str = "generic",
        **kwargs
    ) -> None:
        """
        Add a turbine result to the export.
        
        Parameters:
            turbine_id (int): Unique turbine identifier
            x, y (float): Horizontal position in meters
            power_kw (float): Power output in kilowatts
            wind_speed_ms (float): Inflow wind speed at hub height (m/s)
            wind_direction_deg (float): Wind direction at hub (degrees)
            thrust_coefficient (float): Thrust coefficient (default: 0.8)
            yaw_deg (float): Yaw angle in degrees (default: 0.0)
            hub_height (float): Hub height AGL in meters (default: 90.0)
            rotor_diameter (float): Rotor diameter in meters (default: 100.0)
            turbine_type (str): Turbine model name (default: "generic")
            **kwargs: Additional turbine-specific fields
        """
        turbine = {
            'id': int(turbine_id),
            'location': {
                'x_m': float(x),
                'y_m': float(y)
            },
            'turbine_type': str(turbine_type),
            'rotor_diameter_m': float(rotor_diameter),
            'hub_height_agl_m': float(hub_height),
            'wind_conditions': {
                'speed_ms': float(wind_speed_ms),
                'direction_deg': float(wind_direction_deg)
            },
            'power': {
                'output_kw': float(power_kw),
                'thrust_coefficient': float(thrust_coefficient),
                'yaw_deg': float(yaw_deg)
            },
            **kwargs
        }
        self.turbines.append(turbine)
    
    def set_farm_power(
        self,
        total_power_kw: float,
        annual_energy_gwh: Optional[float] = None,
        capacity_factor: Optional[float] = None
    ) -> None:
        """
        Set farm-level power aggregates.
        
        Parameters:
            total_power_kw (float): Total farm power in kilowatts
            annual_energy_gwh (float, optional): Annual energy production in gigawatt-hours
            capacity_factor (float, optional): Capacity factor (0-1)
        """
        self.farm_power_kw = float(total_power_kw)
        if annual_energy_gwh is not None:
            self.farm_aep_gwh = float(annual_energy_gwh)
        if capacity_factor is not None:
            self.metadata['capacity_factor'] = float(capacity_factor)
    
    def set_wind_resource(
        self,
        mean_speed_ms: float,
        mean_direction_deg: float,
        std_speed_ms: float = 0.0,
        std_direction_deg: float = 0.0,
        height_agl: float = 90.0,
        turbulence_intensity: float = 0.1,
        **kwargs
    ) -> None:
        """
        Set wind resource statistics.
        
        Parameters:
            mean_speed_ms (float): Mean wind speed (m/s)
            mean_direction_deg (float): Mean wind direction (degrees)
            std_speed_ms (float): Standard deviation of wind speed (m/s)
            std_direction_deg (float): Standard deviation of direction (degrees)
            height_agl (float): Reference height AGL (m)
            turbulence_intensity (float): Turbulence intensity (%)
            **kwargs: Additional wind resource fields
        """
        self.wind_resource = {
            'mean_speed_ms': float(mean_speed_ms),
            'mean_direction_deg': float(mean_direction_deg),
            'std_speed_ms': float(std_speed_ms),
            'std_direction_deg': float(std_direction_deg),
            'height_agl_m': float(height_agl),
            'turbulence_intensity': float(turbulence_intensity),
            **kwargs
        }
    
    def to_dict(self) -> Dict[str, Any]:
        """
        Convert export to dictionary.
        
        Returns:
            dict: Complete wind farm results as dictionary
        """
        result = {
            'metadata': {
                'farm_name': self.farm_name,
                'version': '1.0',
                **self.metadata
            },
            'farm_summary': {
                'num_turbines': len(self.turbines),
                'total_power_kw': self.farm_power_kw,
                'annual_energy_gwh': self.farm_aep_gwh
            },
            'turbines': self.turbines
        }
        
        if self.wind_resource is not None:
            result['wind_resource'] = self.wind_resource
        
        return result
    
    def export_json(self, filename: str, pretty: bool = True) -> None:
        """
        Export results to JSON file.
        
        Parameters:
            filename (str): Output JSON filename
            pretty (bool): If True, format with indentation (default: True)
        
        Returns:
            None (writes to file)
        """
        data = self.to_dict()
        with open(filename, 'w') as f:
            if pretty:
                json.dump(data, f, indent=2)
            else:
                json.dump(data, f)
        print(f"✓ Exported PyOptimization results to {filename}")
    
    def export_turbine_csv(self, filename: str) -> None:
        """
        Export per-turbine results to CSV.
        
        Parameters:
            filename (str): Output CSV filename
        
        Returns:
            None (writes to file)
        """
        if not self.turbines:
            raise ValueError("No turbine results to export")
        
        fieldnames = [
            'turbine_id', 'x_m', 'y_m', 'hub_height_agl_m', 'rotor_diameter_m',
            'turbine_type', 'wind_speed_ms', 'wind_direction_deg',
            'power_kw', 'thrust_coefficient', 'yaw_deg'
        ]
        
        with open(filename, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction='ignore')
            writer.writeheader()
            
            for turbine in self.turbines:
                row = {
                    'turbine_id': turbine['id'],
                    'x_m': turbine['location']['x_m'],
                    'y_m': turbine['location']['y_m'],
                    'hub_height_agl_m': turbine['hub_height_agl_m'],
                    'rotor_diameter_m': turbine['rotor_diameter_m'],
                    'turbine_type': turbine['turbine_type'],
                    'wind_speed_ms': turbine['wind_conditions']['speed_ms'],
                    'wind_direction_deg': turbine['wind_conditions']['direction_deg'],
                    'power_kw': turbine['power']['output_kw'],
                    'thrust_coefficient': turbine['power']['thrust_coefficient'],
                    'yaw_deg': turbine['power']['yaw_deg']
                }
                writer.writerow(row)
        
        print(f"✓ Exported {len(self.turbines)} turbine results to {filename}")
    
    def export_summary_csv(self, filename: str) -> None:
        """
        Export farm summary statistics to CSV.
        
        Parameters:
            filename (str): Output CSV filename
        
        Returns:
            None (writes to file)
        """
        with open(filename, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['Farm Summary', self.farm_name])
            writer.writerow([])
            
            writer.writerow(['Metric', 'Value', 'Unit'])
            writer.writerow(['Number of Turbines', len(self.turbines), ''])
            writer.writerow(['Total Power Output', f"{self.farm_power_kw:.2f}", 'kW'])
            writer.writerow(['Annual Energy Production', f"{self.farm_aep_gwh:.2f}", 'GWh'])
            
            if len(self.turbines) > 0:
                mean_power = np.mean([t['power']['output_kw'] for t in self.turbines])
                writer.writerow(['Mean Turbine Power', f"{mean_power:.2f}", 'kW'])
            
            if self.wind_resource is not None:
                writer.writerow([])
                writer.writerow(['Wind Resource', 'Value', 'Unit'])
                writer.writerow(['Mean Wind Speed', 
                               f"{self.wind_resource['mean_speed_ms']:.2f}", 'm/s'])
                writer.writerow(['Mean Wind Direction', 
                               f"{self.wind_resource['mean_direction_deg']:.1f}", 'deg'])
        
        print(f"✓ Exported farm summary to {filename}")
    
    @staticmethod
    def from_wind_farm_result(
        wind_solver,
        turbine_locations: List[Dict[str, Any]],
        hub_height: float = 90.0,
        rotor_diameter: float = 100.0,
        turbine_type: str = "generic"
    ) -> 'PyOptimizationExporter':
        """
        Create exporter from wind solver results and turbine locations.
        
        Parameters:
            wind_solver: Solved WindSolver instance
            turbine_locations: List of dicts with keys 'id', 'x', 'y' (and optionally others)
            hub_height (float): Hub height AGL (m)
            rotor_diameter (float): Rotor diameter (m)
            turbine_type (str): Turbine model name
        
        Returns:
            PyOptimizationExporter: Populated exporter object
        """
        exporter = PyOptimizationExporter()
        
        # Extract velocity field at hub height
        vel_at_hub = wind_solver.get_velocity_at_agl(hub_height)
        u = vel_at_hub['u']  # shape: (ny, nx)
        v = vel_at_hub['v']
        
        # Compute speed and direction
        speed = np.sqrt(u**2 + v**2)
        direction = np.degrees(np.arctan2(u, v)) % 360.0
        
        # Grid info
        xmin, ymin = wind_solver.xmin, wind_solver.ymin
        dx, dy = wind_solver.dx, wind_solver.dy
        nx, ny = wind_solver.nx, wind_solver.ny
        
        # Total farm power (placeholder - would need power curve evaluation)
        total_power = 0.0
        
        # Add turbine results
        for turbine_loc in turbine_locations:
            tid = turbine_loc.get('id', 0)
            tx = turbine_loc['x']
            ty = turbine_loc['y']
            
            # Interpolate wind to turbine location
            i_x = (tx - xmin) / dx
            i_y = (ty - ymin) / dy
            
            i0_x = int(np.clip(np.floor(i_x), 0, nx - 2))
            i0_y = int(np.clip(np.floor(i_y), 0, ny - 2))
            i1_x = i0_x + 1
            i1_y = i0_y + 1
            
            fx = i_x - i0_x
            fy = i_y - i0_y
            
            # Bilinear interpolation
            u_interp = (
                u[i0_y, i0_x] * (1 - fx) * (1 - fy) +
                u[i0_y, i1_x] * fx * (1 - fy) +
                u[i1_y, i0_x] * (1 - fx) * fy +
                u[i1_y, i1_x] * fx * fy
            )
            v_interp = (
                v[i0_y, i0_x] * (1 - fx) * (1 - fy) +
                v[i0_y, i1_x] * fx * (1 - fy) +
                v[i1_y, i0_x] * (1 - fx) * fy +
                v[i1_y, i1_x] * fx * fy
            )
            
            speed_at_turbine = np.sqrt(u_interp**2 + v_interp**2)
            direction_at_turbine = np.degrees(np.arctan2(u_interp, v_interp)) % 360.0
            
            # Simplified power calculation (constant Ct, no wake losses)
            ct = 0.8
            air_density = 1.225
            rotor_area = np.pi * (rotor_diameter / 2.0)**2
            power_kw = 0.5 * ct * air_density * rotor_area * speed_at_turbine**3 / 1000.0
            
            exporter.add_turbine_result(
                turbine_id=tid,
                x=tx,
                y=ty,
                power_kw=power_kw,
                wind_speed_ms=float(speed_at_turbine),
                wind_direction_deg=float(direction_at_turbine),
                thrust_coefficient=ct,
                hub_height=hub_height,
                rotor_diameter=rotor_diameter,
                turbine_type=turbine_type
            )
            total_power += power_kw
        
        exporter.set_farm_power(total_power)
        
        return exporter
    
    def __repr__(self) -> str:
        """String representation."""
        return (
            f"PyOptimizationExporter(farm={self.farm_name}, "
            f"turbines={len(self.turbines)}, power={self.farm_power_kw:.1f}kW)"
        )
