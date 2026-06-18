#!/usr/bin/env python3
"""
floris_extensions.py - Extended FLORIS wind farm integration utilities

Provides advanced FLORIS export capabilities for wind farm simulation and analysis:
- FLORISConfigExporter: Native farm configuration file generation
- EnhancedCSVExporter: Wind data export with meteorological metadata
- PowerCurveGenerator: Turbine power and thrust coefficient curve generation
- WindRoseFormatter: Wind resource frequency distribution formatting
- YawOptimizationFormatter: Yaw control optimization results formatting

All modules are designed to work with massconsistent_amr wind field outputs.
"""

import json
import csv
import numpy as np
from typing import Dict, List, Tuple, Optional, Any
from pathlib import Path


class FLORISConfigExporter:
    """
    Export wind farm layout and parameters to FLORIS native JSON format.
    
    Generates a complete FLORIS configuration file that can be directly loaded
    into FLORIS for wind farm simulations.
    """
    
    @staticmethod
    def export_farm_config(
        turbine_locations: List[Tuple[float, float]],
        turbine_types: Optional[List[str]] = None,
        hub_heights: Optional[List[float]] = None,
        rotor_diameters: Optional[List[float]] = None,
        power_curve_files: Optional[List[str]] = None,
        yaw_angles: Optional[List[float]] = None,
        wind_speed: float = 10.0,
        wind_direction: float = 270.0,
        turbulence_intensity: float = 0.05,
        air_density: float = 1.225,
        output_file: str = "floris_config.json"
    ) -> Dict[str, Any]:
        """
        Export farm configuration to FLORIS JSON format.
        
        Parameters:
            turbine_locations: List of (x, y) tuples in meters
            turbine_types: List of turbine type names (default: all "generic")
            hub_heights: List of hub heights AGL in meters (default: all 90.0)
            rotor_diameters: List of rotor diameters in meters (default: all 100.0)
            power_curve_files: List of power curve file paths (optional)
            yaw_angles: List of yaw angles in degrees (optional)
            wind_speed: Wind speed in m/s (default: 10.0)
            wind_direction: Wind direction in degrees (default: 270.0)
            turbulence_intensity: Turbulence intensity (default: 0.05)
            air_density: Air density in kg/m³ (default: 1.225)
            output_file: Output JSON filename
        
        Returns:
            dict: Generated FLORIS config
        """
        n_turbines = len(turbine_locations)
        
        # Set defaults
        if turbine_types is None:
            turbine_types = ["generic"] * n_turbines
        if hub_heights is None:
            hub_heights = [90.0] * n_turbines
        if rotor_diameters is None:
            rotor_diameters = [100.0] * n_turbines
        if power_curve_files is None:
            power_curve_files = [None] * n_turbines
        if yaw_angles is None:
            yaw_angles = [0.0] * n_turbines
        
        # Build FLORIS config structure
        config = {
            "version": "3.0",
            "meta": {
                "name": "massconsistent_amr export",
                "description": "Wind farm config exported from massconsistent_amr",
                "floris_version": "3.0"
            },
            "farm": {
                "layout_x": [loc[0] for loc in turbine_locations],
                "layout_y": [loc[1] for loc in turbine_locations],
                "turbines": []
            },
            "wind_speed": wind_speed,
            "wind_direction": wind_direction,
            "turbulence_intensity": turbulence_intensity,
            "air_density": air_density,
            "wake_model": "cc",
            "wind_farm_controls": {
                "enable_active_wake_control": False,
                "enable_yaw_control": True if any(y != 0.0 for y in yaw_angles) else False,
                "yaw_angles_deg": yaw_angles
            }
        }
        
        # Add turbine objects
        for i, (x, y) in enumerate(turbine_locations):
            turbine = {
                "turbine_type": turbine_types[i],
                "hub_height": hub_heights[i],
                "rotor_diameter": rotor_diameters[i],
                "yaw_angle": yaw_angles[i]
            }
            if power_curve_files[i] is not None:
                turbine["power_curve_file"] = power_curve_files[i]
            config["farm"]["turbines"].append(turbine)
        
        # Write to file
        with open(output_file, 'w') as f:
            json.dump(config, f, indent=2)
        
        print(f"✓ Exported FLORIS config to {output_file}")
        return config


class EnhancedCSVExporter:
    """
    Enhanced CSV export with turbulence intensity, wind shear, and air density.
    
    Extends the basic FLORIS CSV export with additional meteorological metadata
    required for advanced wind farm simulations.
    """
    
    @staticmethod
    def export_with_metadata(
        turbine_locations: List[Tuple[float, float]],
        wind_data: List[Dict[str, float]],
        hub_heights: List[float],
        turbulence_intensities: Optional[List[float]] = None,
        wind_shear_exponents: Optional[List[float]] = None,
        air_densities: Optional[List[float]] = None,
        power_outputs: Optional[List[float]] = None,
        yaw_angles: Optional[List[float]] = None,
        output_file: str = "enhanced_wind_data.csv"
    ) -> None:
        """
        Export wind data with full meteorological metadata.
        
        Parameters:
            turbine_locations: List of (x, y) tuples
            wind_data: List of wind dicts with 'u', 'v', 'speed', 'direction'
            hub_heights: List of hub heights AGL
            turbulence_intensities: List of TI values (default: 0.05 for all)
            wind_shear_exponents: List of shear exponents alpha (default: 0.2)
            air_densities: List of air density values (default: 1.225)
            power_outputs: List of power outputs in W (optional)
            yaw_angles: List of yaw angles in degrees (optional)
            output_file: Output CSV filename
        
        Returns:
            None (writes to file)
        """
        n_turbines = len(turbine_locations)
        
        # Set defaults
        if turbulence_intensities is None:
            turbulence_intensities = [0.05] * n_turbines
        if wind_shear_exponents is None:
            wind_shear_exponents = [0.2] * n_turbines
        if air_densities is None:
            air_densities = [1.225] * n_turbines
        if power_outputs is None:
            power_outputs = [0.0] * n_turbines
        if yaw_angles is None:
            yaw_angles = [0.0] * n_turbines
        
        fieldnames = [
            'turbine_id', 'x_m', 'y_m', 'z_terrain_m', 'z_hub_m',
            'u_ms', 'v_ms', 'wind_speed_ms', 'wind_direction_deg',
            'hub_height_agl_m', 'turbulence_intensity',
            'wind_shear_exponent', 'air_density_kg_m3',
            'power_output_w', 'yaw_angle_deg'
        ]
        
        with open(output_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            
            for i, (x, y) in enumerate(turbine_locations):
                wind = wind_data[i]
                z_terrain = wind['z'] - hub_heights[i]
                
                row = {
                    'turbine_id': i,
                    'x_m': f"{x:.2f}",
                    'y_m': f"{y:.2f}",
                    'z_terrain_m': f"{z_terrain:.2f}",
                    'z_hub_m': f"{wind['z']:.2f}",
                    'u_ms': f"{wind['u']:.3f}",
                    'v_ms': f"{wind['v']:.3f}",
                    'wind_speed_ms': f"{wind['speed']:.3f}",
                    'wind_direction_deg': f"{wind['direction']:.1f}",
                    'hub_height_agl_m': f"{hub_heights[i]:.1f}",
                    'turbulence_intensity': f"{turbulence_intensities[i]:.4f}",
                    'wind_shear_exponent': f"{wind_shear_exponents[i]:.3f}",
                    'air_density_kg_m3': f"{air_densities[i]:.4f}",
                    'power_output_w': f"{power_outputs[i]:.2f}",
                    'yaw_angle_deg': f"{yaw_angles[i]:.2f}"
                }
                writer.writerow(row)
        
        print(f"✓ Exported enhanced wind data to {output_file}")


class PowerCurveGenerator:
    """
    Generate power and thrust coefficient curves from simulation results.
    
    Post-processes wind farm simulations to extract power curves in FLORIS format.
    """
    
    @staticmethod
    def generate_from_turbine_data(
        turbine_id: int,
        wind_speeds: List[float],
        power_outputs: List[float],
        thrust_coefficients: List[float],
        output_file: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Generate power/thrust curve JSON from simulation results.
        
        Parameters:
            turbine_id: Turbine identifier
            wind_speeds: Wind speeds at which data was collected (m/s)
            power_outputs: Power outputs at each wind speed (W)
            thrust_coefficients: Thrust coefficients at each wind speed
            output_file: Optional output JSON filename
        
        Returns:
            dict: Power curve data in FLORIS format
        """
        # Sort by wind speed
        sorted_indices = np.argsort(wind_speeds)
        ws_sorted = [wind_speeds[i] for i in sorted_indices]
        p_sorted = [power_outputs[i] for i in sorted_indices]
        ct_sorted = [thrust_coefficients[i] for i in sorted_indices]
        
        curve_data = {
            "turbine_type": f"turbine_{turbine_id}",
            "power_thrust_table": {
                "wind_speed": ws_sorted,
                "power": p_sorted,
                "thrust_coefficient": ct_sorted
            },
            "generator_efficiency": 1.0,
            "hub_height": 90.0,
            "rotor_diameter": 100.0
        }
        
        if output_file:
            with open(output_file, 'w') as f:
                json.dump(curve_data, f, indent=2)
            print(f"✓ Generated power curve to {output_file}")
        
        return curve_data
    
    @staticmethod
    def generate_from_wind_field(
        wind_speeds: List[float],
        power_curve_lookup: Dict[float, float],
        ct_curve_lookup: Dict[float, float],
        output_file: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Generate power curve from wind field statistics.
        
        Parameters:
            wind_speeds: List of wind speeds to sample (m/s)
            power_curve_lookup: Function or dict mapping wind speed to power
            ct_curve_lookup: Function or dict mapping wind speed to Ct
            output_file: Optional output JSON filename
        
        Returns:
            dict: Power curve in FLORIS format
        """
        # Extract power and Ct for each wind speed
        power_values = []
        ct_values = []
        
        for ws in wind_speeds:
            if isinstance(power_curve_lookup, dict):
                power_values.append(power_curve_lookup.get(ws, 0.0))
            else:
                power_values.append(power_curve_lookup(ws))
            
            if isinstance(ct_curve_lookup, dict):
                ct_values.append(ct_curve_lookup.get(ws, 0.8))
            else:
                ct_values.append(ct_curve_lookup(ws))
        
        curve_data = {
            "power_thrust_table": {
                "wind_speed": wind_speeds,
                "power": power_values,
                "thrust_coefficient": ct_values
            }
        }
        
        if output_file:
            with open(output_file, 'w') as f:
                json.dump(curve_data, f, indent=2)
            print(f"✓ Generated power curve to {output_file}")
        
        return curve_data


class WindRoseFormatter:
    """
    Export wind resource as FLORIS-compatible wind rose (frequency distribution).
    
    Bins wind speeds and directions from simulation or measurements into
    a frequency matrix suitable for FLORIS wind resource analysis.
    """
    
    @staticmethod
    def create_from_simulation(
        wind_speeds: List[float],
        wind_directions: List[float],
        frequencies: Optional[List[float]] = None,
        speed_bins: Optional[List[float]] = None,
        direction_bins: Optional[List[float]] = None,
        output_file: str = "wind_rose.json"
    ) -> Dict[str, Any]:
        """
        Create wind rose frequency distribution from simulation data.
        
        Parameters:
            wind_speeds: List of wind speeds (m/s)
            wind_directions: List of wind directions (degrees 0-360)
            frequencies: List of frequency weights (default: uniform)
            speed_bins: Wind speed bin edges (default: 0-30 m/s in 1 m/s steps)
            direction_bins: Direction bin edges (default: 16 cardinal directions)
            output_file: Output JSON filename
        
        Returns:
            dict: Wind rose in FLORIS format
        """
        wind_speeds = np.array(wind_speeds)
        wind_directions = np.array(wind_directions)
        
        # Default bins
        if speed_bins is None:
            speed_bins = np.arange(0, 31, 1)  # 0-30 m/s in 1 m/s steps
        else:
            speed_bins = np.array(speed_bins)
        
        if direction_bins is None:
            # 16 cardinal directions (22.5° each)
            direction_bins = np.arange(0, 361, 22.5)
        else:
            direction_bins = np.array(direction_bins)
        
        if frequencies is None:
            frequencies = np.ones_like(wind_speeds) / len(wind_speeds)
        else:
            frequencies = np.array(frequencies)
            frequencies = frequencies / np.sum(frequencies)  # Normalize
        
        # Create 2D histogram
        freq_matrix, speed_edges, dir_edges = np.histogram2d(
            wind_speeds, wind_directions,
            bins=[speed_bins, direction_bins],
            weights=frequencies
        )
        
        # Convert to FLORIS format
        wind_rose = {
            "wind_speed_bins": speed_bins.tolist(),
            "wind_direction_bins": direction_bins.tolist(),
            "frequency_matrix": freq_matrix.tolist(),
            "wind_speed_bin_centers": (speed_bins[:-1] + speed_bins[1:]) / 2,
            "wind_direction_bin_centers": (direction_bins[:-1] + direction_bins[1:]) / 2,
            "metadata": {
                "num_samples": len(wind_speeds),
                "wind_speed_mean_ms": float(np.mean(wind_speeds)),
                "wind_speed_std_ms": float(np.std(wind_speeds)),
                "wind_direction_mean_deg": float(np.mean(wind_directions)),
                "wind_direction_std_deg": float(np.std(wind_directions))
            }
        }
        
        with open(output_file, 'w') as f:
            json.dump(wind_rose, f, indent=2)
        
        print(f"✓ Generated wind rose to {output_file}")
        return wind_rose
    
    @staticmethod
    def export_frequency_table(
        wind_rose_data: Dict[str, Any],
        output_file: str = "wind_rose_frequencies.csv"
    ) -> None:
        """
        Export wind rose as CSV frequency table.
        
        Parameters:
            wind_rose_data: Wind rose dict from create_from_simulation
            output_file: Output CSV filename
        
        Returns:
            None (writes to file)
        """
        speed_bins = wind_rose_data['wind_speed_bins']
        direction_bins = wind_rose_data['wind_direction_bins']
        freq_matrix = np.array(wind_rose_data['frequency_matrix'])
        
        with open(output_file, 'w', newline='') as f:
            writer = csv.writer(f)
            
            # Header with direction bins
            header = ['wind_speed_ms'] + [f"{d:.1f}°" for d in direction_bins[:-1]]
            writer.writerow(header)
            
            # Rows with frequency data
            for i, ws in enumerate(speed_bins[:-1]):
                row = [f"{ws:.1f}"] + [f"{freq_matrix[i, j]:.6f}" for j in range(len(direction_bins)-1)]
                writer.writerow(row)
        
        print(f"✓ Exported wind rose frequency table to {output_file}")


class YawOptimizationFormatter:
    """
    Format yaw angle optimization results for FLORIS.
    
    Takes turbine yaw optimization results and formats them as FLORIS-compatible
    configuration for wind farm control implementation.
    """
    
    @staticmethod
    def export_yaw_results(
        yaw_angles: List[float],
        turbine_ids: Optional[List[int]] = None,
        wind_speed: float = 10.0,
        wind_direction: float = 270.0,
        improvement_pct: Optional[float] = None,
        output_file: str = "yaw_optimization.json"
    ) -> Dict[str, Any]:
        """
        Export yaw optimization results to FLORIS format.
        
        Parameters:
            yaw_angles: List of optimized yaw angles in degrees
            turbine_ids: Turbine IDs (default: 0, 1, 2, ...)
            wind_speed: Wind speed for this optimization (m/s)
            wind_direction: Wind direction for this optimization (degrees)
            improvement_pct: Power improvement percentage (optional)
            output_file: Output JSON filename
        
        Returns:
            dict: Yaw optimization results in FLORIS format
        """
        n_turbines = len(yaw_angles)
        
        if turbine_ids is None:
            turbine_ids = list(range(n_turbines))
        
        results = {
            "optimization_type": "yaw_control",
            "wind_speed_ms": wind_speed,
            "wind_direction_deg": wind_direction,
            "yaw_angles_deg": yaw_angles,
            "turbine_ids": turbine_ids,
            "turbine_yaw_mapping": {
                str(tid): float(yaw) for tid, yaw in zip(turbine_ids, yaw_angles)
            },
            "metadata": {
                "num_turbines": n_turbines,
                "max_yaw_deg": float(max(yaw_angles)),
                "min_yaw_deg": float(min(yaw_angles)),
                "mean_yaw_deg": float(np.mean(yaw_angles))
            }
        }
        
        if improvement_pct is not None:
            results["power_improvement_pct"] = improvement_pct
        
        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2)
        
        print(f"✓ Exported yaw optimization results to {output_file}")
        return results
    
    @staticmethod
    def export_yaw_control_config(
        yaw_angles: List[float],
        turbine_locations: List[Tuple[float, float]],
        hub_heights: List[float],
        rotor_diameters: List[float],
        wind_speed: float = 10.0,
        wind_direction: float = 270.0,
        output_file: str = "floris_yaw_control_config.json"
    ) -> Dict[str, Any]:
        """
        Export complete FLORIS farm config with yaw control.
        
        Parameters:
            yaw_angles: List of yaw angles in degrees
            turbine_locations: List of (x, y) tuples
            hub_heights: List of hub heights
            rotor_diameters: List of rotor diameters
            wind_speed: Wind speed (m/s)
            wind_direction: Wind direction (degrees)
            output_file: Output JSON filename
        
        Returns:
            dict: Complete FLORIS config with yaw control
        """
        config = FLORISConfigExporter.export_farm_config(
            turbine_locations=turbine_locations,
            hub_heights=hub_heights,
            rotor_diameters=rotor_diameters,
            yaw_angles=yaw_angles,
            wind_speed=wind_speed,
            wind_direction=wind_direction,
            output_file=output_file
        )
        
        print(f"✓ Exported FLORIS yaw control config to {output_file}")
        return config


# Convenience functions for quick access
def quick_floris_export(
    turbine_locations: List[Tuple[float, float]],
    wind_data: List[Dict[str, float]],
    hub_heights: List[float],
    output_prefix: str = "floris_export"
) -> Dict[str, str]:
    """
    Quick export of all FLORIS formats in one call.
    
    Parameters:
        turbine_locations: List of (x, y) tuples
        wind_data: Wind data at each turbine
        hub_heights: Hub heights
        output_prefix: Prefix for output files
    
    Returns:
        dict: Mapping of export type to filename
    """
    files = {}
    
    # Config export
    config_file = f"{output_prefix}_config.json"
    FLORISConfigExporter.export_farm_config(
        turbine_locations=turbine_locations,
        hub_heights=hub_heights,
        output_file=config_file
    )
    files['config'] = config_file
    
    # Enhanced CSV export
    csv_file = f"{output_prefix}_enhanced.csv"
    EnhancedCSVExporter.export_with_metadata(
        turbine_locations=turbine_locations,
        wind_data=wind_data,
        hub_heights=hub_heights,
        output_file=csv_file
    )
    files['csv'] = csv_file
    
    return files
