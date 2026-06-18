#!/usr/bin/env python3
"""
Data Center Heat Island Plume Analysis Module

Utilities for analyzing and visualizing the atmospheric effects of data center
heat sources. This module extracts thermal plume characteristics from wind solver
output and computes diagnostic metrics.

Key features:
  - Temperature anomaly extraction and visualization
  - Plume rise height estimation
  - Downwind thermal profile analysis
  - Briggs plume rise model comparison
  - Particle trajectory visualization for plume extent

Example usage:
    >>> from datacenter_heat_source import DataCenterPlume
    >>> 
    >>> # Load solver output
    >>> plume = DataCenterPlume.from_amrex_plotfile("plt_datacenter_00050")
    >>> 
    >>> # Compute metrics
    >>> metrics = plume.compute_plume_metrics(
    ...     facility_x=100000, facility_y=200000,
    ...     wind_direction=270.0  # From west
    ... )
    >>> 
    >>> # Visualize
    >>> plume.plot_horizontal_slice(height_agl=100.0, filename="plume_100m.png")
    >>> plume.plot_vertical_slice(y_coord=200000, filename="plume_vertical.png")
"""

import os
import sys
import numpy as np
import pandas as pd
from dataclasses import dataclass, field
from typing import Dict, List, Tuple, Optional
import warnings

try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as patches
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    warnings.warn("Matplotlib not available; plotting functions will be disabled.")

try:
    import yt
    HAS_YT = True
except ImportError:
    HAS_YT = False


# ============================================================================
# Data structures
# ============================================================================

@dataclass
class PlumeMetrics:
    """
    Diagnostic metrics for a data center thermal plume.
    
    Attributes:
        plume_rise_height: Maximum rise height above source [m]
        plume_extent_horizontal: Horizontal extent of ΔT>0.5K plume [m]
        plume_extent_vertical: Vertical extent of plume [m]
        max_temperature_excess: Maximum temperature anomaly observed [K]
        mean_temperature_excess: Mean temperature within plume [K]
        downwind_distance_max: Distance to maximum temperature anomaly [m]
        integrated_heat_area: Integrated ΔT*A over plume [K·m²]
    """
    plume_rise_height: float = 0.0
    plume_extent_horizontal: float = 0.0
    plume_extent_vertical: float = 0.0
    max_temperature_excess: float = 0.0
    mean_temperature_excess: float = 0.0
    downwind_distance_max: float = 0.0
    integrated_heat_area: float = 0.0
    
    # Per-distance metrics (for downwind profiles)
    downwind_distances: List[float] = field(default_factory=list)
    downwind_temps: List[float] = field(default_factory=list)
    downwind_heights: List[float] = field(default_factory=list)


@dataclass
class DataCenterFacility:
    """
    Data center facility specification.
    
    Attributes:
        x, y, z: Facility location [m]
        area: Footprint area [m²]
        heat_release: Waste heat output [W]
        name: Facility identifier/name
    """
    x: float = 0.0
    y: float = 0.0
    z: float = 10.0
    area: float = 1000.0
    heat_release: float = 1e7  # 10 MW default
    name: str = "DataCenter"


# ============================================================================
# Plume analysis class
# ============================================================================

class DataCenterPlume:
    """
    Analyzer for data center thermal plume characteristics.
    
    Handles loading solver output, extracting temperature fields, computing
    plume metrics, and generating diagnostic visualizations.
    """
    
    def __init__(self, 
                 temperature_field: Optional[np.ndarray] = None,
                 coordinates: Optional[Dict[str, np.ndarray]] = None,
                 ambient_temp: float = 300.0):
        """
        Initialize plume analyzer.
        
        Parameters:
            temperature_field: 3D temperature array [K], shape (nx, ny, nz)
            coordinates: Dict with 'x', 'y', 'z' arrays [m]
            ambient_temp: Reference/far-field temperature [K]
        """
        self.temp = temperature_field
        self.coords = coordinates or {}
        self.T_ref = ambient_temp
        self.metrics = None
        
    @staticmethod
    def from_amrex_plotfile(plotfile_path: str) -> "DataCenterPlume":
        """
        Load temperature field from AMReX plotfile.
        
        Parameters:
            plotfile_path: Path to AMReX output directory
            
        Returns:
            DataCenterPlume instance with loaded data
        """
        if not HAS_YT:
            raise ImportError("yt is required for loading AMReX plotfiles. "
                            "Install with: pip install yt")
        
        try:
            ds = yt.load(plotfile_path)
            
            # Extract temperature field
            temp = ds[("boxlib", "temp")].to_array()
            
            # Get coordinates
            x = ds.coordinates.x.to_array()
            y = ds.coordinates.y.to_array()
            z = ds.coordinates.z.to_array()
            
            coords = {'x': x, 'y': y, 'z': z}
            
            # Get ambient temperature from metadata
            T_ref = 300.0  # default
            if "temperature_reference" in ds.parameters:
                T_ref = float(ds.parameters["temperature_reference"])
            
            return DataCenterPlume(temperature_field=temp, 
                                 coordinates=coords,
                                 ambient_temp=T_ref)
        except Exception as e:
            raise RuntimeError(f"Failed to load plotfile {plotfile_path}: {e}")
    
    def compute_temperature_anomaly(self, 
                                   background_temp: Optional[np.ndarray] = None) -> np.ndarray:
        """
        Compute temperature anomaly (ΔT) above ambient/background.
        
        Parameters:
            background_temp: Background temperature field. If None, uses T_ref scalar.
            
        Returns:
            Temperature anomaly array [K]
        """
        if self.temp is None:
            raise ValueError("Temperature field not loaded")
        
        if background_temp is None:
            # Use scalar reference temperature
            dT = self.temp - self.T_ref
        else:
            dT = self.temp - background_temp
        
        return np.maximum(dT, 0.0)  # Only positive anomalies
    
    def compute_plume_metrics(self,
                             facility: DataCenterFacility,
                             threshold_dT: float = 0.5) -> PlumeMetrics:
        """
        Compute plume extent and characteristics.
        
        Parameters:
            facility: Data center facility specification
            threshold_dT: Temperature excess threshold for plume definition [K]
            
        Returns:
            PlumeMetrics object with computed diagnostics
        """
        if self.temp is None:
            raise ValueError("Temperature field not loaded")
        
        dT = self.compute_temperature_anomaly()
        
        # Identify plume region (ΔT > threshold)
        plume_mask = dT > threshold_dT
        
        if not np.any(plume_mask):
            # No plume detected
            return PlumeMetrics()
        
        # Get coordinates
        x = self.coords.get('x')
        y = self.coords.get('y')
        z = self.coords.get('z')
        
        if x is None or y is None or z is None:
            raise ValueError("Coordinates not available")
        
        # Compute plume metrics
        metrics = PlumeMetrics()
        
        # Maximum temperature excess
        metrics.max_temperature_excess = float(np.max(dT))
        metrics.mean_temperature_excess = float(np.mean(dT[plume_mask]))
        
        # Plume rise height (vertical extent above source)
        if dT.ndim == 3:
            # 3D field: find max height with ΔT > threshold
            max_z_indices = np.any(plume_mask, axis=(0, 1))
            if np.any(max_z_indices):
                max_z_idx = np.max(np.where(max_z_indices))
                metrics.plume_rise_height = float(z[max_z_idx] - facility.z)
            
            # Vertical extent
            z_with_plume = np.any(plume_mask, axis=(0, 1))
            z_indices = np.where(z_with_plume)[0]
            if len(z_indices) > 0:
                metrics.plume_extent_vertical = float(z[z_indices[-1]] - z[z_indices[0]])
        
        # Horizontal extent (horizontal distance from facility center)
        plume_x, plume_y = np.where(np.any(plume_mask, axis=-1))
        if len(plume_x) > 0:
            distances = np.sqrt((x[plume_x] - facility.x)**2 + 
                              (y[plume_y] - facility.y)**2)
            metrics.plume_extent_horizontal = float(np.max(distances))
            
            # Distance to max temperature
            max_temp_idx = np.unravel_index(np.argmax(dT), dT.shape)
            max_x = x[max_temp_idx[0]] if x.ndim >= 1 else facility.x
            max_y = y[max_temp_idx[1]] if y.ndim >= 1 else facility.y
            metrics.downwind_distance_max = float(
                np.sqrt((max_x - facility.x)**2 + (max_y - facility.y)**2)
            )
        
        # Integrated heat area
        metrics.integrated_heat_area = float(np.sum(dT[plume_mask]))
        
        self.metrics = metrics
        return metrics
    
    def extract_downwind_profile(self,
                                facility: DataCenterFacility,
                                wind_direction: float = 270.0,
                                distance_range: Tuple[float, float] = (0, 5000),
                                height_agl: float = 50.0) -> pd.DataFrame:
        """
        Extract downwind temperature profile.
        
        Parameters:
            facility: Data center facility specification
            wind_direction: Wind direction in degrees (0=N, 90=E, 180=S, 270=W)
            distance_range: Downwind distance range [m]
            height_agl: Height above ground level for profile [m]
            
        Returns:
            DataFrame with (distance, temperature_excess, wind_speed) profiles
        """
        if self.temp is None:
            raise ValueError("Temperature field not loaded")
        
        # Get coordinates
        x = self.coords.get('x')
        y = self.coords.get('y')
        z = self.coords.get('z')
        
        if x is None or y is None or z is None:
            raise ValueError("Coordinates not available")
        
        # Convert wind direction to unit vector (downwind direction from facility)
        angle_rad = np.radians(wind_direction)
        dx_dir = np.cos(angle_rad)
        dy_dir = np.sin(angle_rad)
        
        # Sample downwind profile
        distances = np.linspace(distance_range[0], distance_range[1], 100)
        temps = []
        
        for dist in distances:
            # Point along downwind line
            x_sample = facility.x + dist * dx_dir
            y_sample = facility.y + dist * dy_dir
            
            # Find nearest grid point
            if x.ndim == 1 and y.ndim == 1:
                ix = np.argmin(np.abs(x - x_sample))
                iy = np.argmin(np.abs(y - y_sample))
                
                # Find nearest height
                iz = np.argmin(np.abs(z - height_agl))
                
                # Extract temperature
                if self.temp.ndim == 3:
                    temp_val = self.temp[ix, iy, iz]
                else:
                    temp_val = self.temp[ix, iy]
            else:
                continue
            
            dT = max(0, temp_val - self.T_ref)
            temps.append(dT)
        
        return pd.DataFrame({
            'downwind_distance_m': distances,
            'temperature_excess_K': temps,
            'height_agl_m': height_agl
        })
    
    def plot_horizontal_slice(self,
                             height_agl: float,
                             facility: Optional[DataCenterFacility] = None,
                             filename: Optional[str] = None,
                             vmin: float = 0.0,
                             vmax: Optional[float] = None):
        """
        Plot horizontal temperature slice at specified height.
        
        Parameters:
            height_agl: Height above ground level [m]
            facility: Facility location to mark on plot
            filename: Save figure to file if specified
            vmin, vmax: Color scale limits [K]
        """
        if not HAS_MATPLOTLIB:
            raise ImportError("Matplotlib required for plotting")
        
        if self.temp is None:
            raise ValueError("Temperature field not loaded")
        
        # Find nearest height index
        z = self.coords.get('z')
        if z is None:
            raise ValueError("Z-coordinates not available")
        
        if z.ndim == 1:
            iz = np.argmin(np.abs(z - height_agl))
        else:
            raise ValueError("Z-coordinates have unexpected shape")
        
        # Extract slice and compute anomaly
        dT = self.compute_temperature_anomaly()
        if dT.ndim == 3:
            temp_slice = dT[:, :, iz]
        else:
            temp_slice = dT
        
        # Create figure
        fig, ax = plt.subplots(figsize=(12, 8))
        
        x = self.coords.get('x', np.arange(temp_slice.shape[0]))
        y = self.coords.get('y', np.arange(temp_slice.shape[1]))
        
        # Plot temperature field
        if vmax is None:
            vmax = np.max(temp_slice)
        
        im = ax.contourf(x, y, temp_slice.T, levels=20, 
                        vmin=vmin, vmax=vmax, cmap='YlOrRd')
        
        # Mark facility if provided
        if facility:
            ax.plot(facility.x, facility.y, 'b*', markersize=15, 
                   label=f"Facility: {facility.name}")
            circle = patches.Circle((facility.x, facility.y), 
                                  np.sqrt(facility.area/np.pi),
                                  fill=False, edgecolor='blue', linestyle='--')
            ax.add_patch(circle)
            ax.legend()
        
        # Labels and colorbar
        ax.set_xlabel('Easting [m]')
        ax.set_ylabel('Northing [m]')
        ax.set_title(f'Horizontal Temperature Anomaly at {height_agl:.1f} m AGL')
        cbar = plt.colorbar(im, ax=ax)
        cbar.set_label('Temperature Excess [K]')
        ax.set_aspect('equal')
        
        if filename:
            plt.savefig(filename, dpi=150, bbox_inches='tight')
            print(f"Saved: {filename}")
        
        return fig, ax
    
    def plot_vertical_slice(self,
                           x_coord: Optional[float] = None,
                           y_coord: Optional[float] = None,
                           facility: Optional[DataCenterFacility] = None,
                           filename: Optional[str] = None):
        """
        Plot vertical temperature cross-section.
        
        Parameters:
            x_coord: X-coordinate of vertical section (if y_coord=None)
            y_coord: Y-coordinate of vertical section (if x_coord=None)
            facility: Facility location to mark
            filename: Save figure to file
        """
        if not HAS_MATPLOTLIB:
            raise ImportError("Matplotlib required for plotting")
        
        if self.temp is None:
            raise ValueError("Temperature field not loaded")
        
        # Get coordinates
        x = self.coords.get('x')
        y = self.coords.get('y')
        z = self.coords.get('z')
        
        if x is None or y is None or z is None:
            raise ValueError("Coordinates not available")
        
        # Select slice direction
        dT = self.compute_temperature_anomaly()
        
        if x_coord is not None and x.ndim == 1:
            # Slice at constant x
            ix = np.argmin(np.abs(x - x_coord))
            temp_slice = dT[ix, :, :]
            horiz_coord = y
            horiz_label = 'Northing [m]'
        elif y_coord is not None and y.ndim == 1:
            # Slice at constant y
            iy = np.argmin(np.abs(y - y_coord))
            temp_slice = dT[:, iy, :]
            horiz_coord = x
            horiz_label = 'Easting [m]'
        else:
            raise ValueError("Must specify either x_coord or y_coord")
        
        # Create figure
        fig, ax = plt.subplots(figsize=(12, 6))
        
        im = ax.contourf(horiz_coord, z, temp_slice.T, levels=20, cmap='YlOrRd')
        
        # Mark facility if provided
        if facility:
            if x_coord is not None:
                ax.axvline(facility.y, color='blue', linestyle='--', 
                          label=f"Facility: {facility.name}")
            else:
                ax.axvline(facility.x, color='blue', linestyle='--',
                          label=f"Facility: {facility.name}")
            ax.legend()
        
        # Labels
        ax.set_xlabel(horiz_label)
        ax.set_ylabel('Height [m]')
        ax.set_title(f'Vertical Temperature Anomaly Section')
        cbar = plt.colorbar(im, ax=ax)
        cbar.set_label('Temperature Excess [K]')
        
        if filename:
            plt.savefig(filename, dpi=150, bbox_inches='tight')
            print(f"Saved: {filename}")
        
        return fig, ax


# ============================================================================
# Briggs plume rise validation
# ============================================================================

def briggs_plume_rise(heat_flux: float,
                     wind_speed: float,
                     downwind_distance: float) -> float:
    """
    Analytical Briggs (1975) plume rise formula.
    
    Parameters:
        heat_flux: Buoyant heat flux [W]
        wind_speed: Ambient wind speed [m/s]
        downwind_distance: Distance downwind [m]
        
    Returns:
        Plume rise height [m]
    """
    if heat_flux <= 0 or wind_speed < 0.1:
        return 0.0
    
    # Briggs formula: Δh = 1.6 * F^(1/3) * x^(2/3) / u
    F_third = heat_flux ** (1.0/3.0)
    x_twothirds = max(downwind_distance, 1.0) ** (2.0/3.0)
    
    return 1.6 * F_third * x_twothirds / wind_speed


# ============================================================================
# Utility functions
# ============================================================================

def read_solver_temperature_csv(csv_file: str) -> pd.DataFrame:
    """
    Read temperature profile from CSV file (wind solver input format).
    
    Parameters:
        csv_file: Path to temperature.csv
        
    Returns:
        DataFrame with z [m] and T [K] columns
    """
    try:
        df = pd.read_csv(csv_file)
        # Standardize column names
        df.columns = df.columns.str.strip().str.lower()
        return df
    except Exception as e:
        raise IOError(f"Failed to read {csv_file}: {e}")


if __name__ == "__main__":
    print("Data Center Heat Island Plume Analysis Module")
    print("Available classes: DataCenterPlume, PlumeMetrics, DataCenterFacility")
    print("Available functions: briggs_plume_rise, read_solver_temperature_csv")
