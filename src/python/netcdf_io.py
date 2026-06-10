#!/usr/bin/env python3
"""
netcdf_io.py - NetCDF Import/Export for Atmospheric Fields

Provides utilities for serializing AtmosphericField data to NetCDF format
for seamless data exchange between wind solver and PHREEQC reactive transport
simulations. NetCDF (Network Common Data Form) provides self-documenting,
portable binary storage with efficient compression.

References:
    - NetCDF Climate and Forecast (CF) Conventions
    - Unidata NetCDF Documentation: https://www.unidata.ucar.edu/software/netcdf/
"""

try:
    import netCDF4
    HAS_NETCDF4 = True
except ImportError:
    HAS_NETCDF4 = False

import numpy as np
from pathlib import Path
from datetime import datetime
from typing import Dict, Tuple, Optional
import warnings

from geochemical_coupling import AtmosphericField


class NetCDFHandler:
    """Manage NetCDF I/O for atmospheric fields.
    
    Provides CF-compliant metadata and efficient storage with compression.
    """
    
    # NetCDF compression settings
    COMPRESSION_LEVEL = 4
    CHUNKING_ENABLED = True
    
    def __init__(self, check_netcdf=True):
        """Initialize NetCDF handler.
        
        Parameters:
            check_netcdf (bool): Raise error if NetCDF4 unavailable
        
        Raises:
            ImportError: If NetCDF4 not available and check_netcdf=True
        """
        if not HAS_NETCDF4 and check_netcdf:
            raise ImportError(
                "NetCDF4 Python module required. Install via:\n"
                "  pip install netcdf4\n"
                "Or use conda: conda install -c conda-forge netcdf4"
            )
        self.available = HAS_NETCDF4
    
    def export_to_netcdf(self, fields: AtmosphericField, filename: str,
                        compression: bool = True) -> str:
        """Export atmospheric fields to NetCDF file.
        
        Parameters:
            fields (AtmosphericField): Atmospheric state to export
            filename (str): Output NetCDF filename
            compression (bool): Enable gzip compression
        
        Returns:
            str: Output filename
        
        Raises:
            ImportError: If NetCDF4 not available
        """
        if not HAS_NETCDF4:
            raise ImportError("NetCDF4 required for export. Install via: pip install netcdf4")
        
        filename = Path(filename)
        filename.parent.mkdir(parents=True, exist_ok=True)
        
        with netCDF4.Dataset(filename, 'w', format='NETCDF4') as nc:
            # Create dimensions
            nz, ny, nx = fields.u.shape
            
            nc.createDimension('x', nx)
            nc.createDimension('y', ny)
            nc.createDimension('z', nz)
            nc.createDimension('time', 1)
            
            # Global attributes (CF conventions)
            nc.Conventions = 'CF-1.9'
            nc.title = 'Mass-Consistent Wind Solver Output for Geochemical Coupling'
            nc.history = f'Created {datetime.now().isoformat()}'
            nc.institution = 'massconsistent_amr'
            
            # Spatial coordinates
            var_x = nc.createVariable('x', 'f4', ('x',), zlib=compression)
            var_x.units = 'm'
            var_x.long_name = 'x-coordinate of grid cell centers'
            var_x.standard_name = 'projection_x_coordinate'
            var_x[:] = fields.coord_x
            
            var_y = nc.createVariable('y', 'f4', ('y',), zlib=compression)
            var_y.units = 'm'
            var_y.long_name = 'y-coordinate of grid cell centers'
            var_y.standard_name = 'projection_y_coordinate'
            var_y[:] = fields.coord_y
            
            var_z = nc.createVariable('z', 'f4', ('z',), zlib=compression)
            var_z.units = 'm'
            var_z.long_name = 'z-coordinate (height above sea level)'
            var_z.standard_name = 'height'
            var_z[:] = fields.coord_z
            
            # Time variable
            var_time = nc.createVariable('time', 'f8', ('time',))
            var_time.units = f'seconds since {fields.timestamp.isoformat()}'
            var_time.standard_name = 'time'
            var_time[:] = [0]
            
            # Velocity components
            self._create_3d_var(nc, 'u', fields.u, 'm/s',
                              'eastward wind component',
                              compression=compression)
            self._create_3d_var(nc, 'v', fields.v, 'm/s',
                              'northward wind component',
                              compression=compression)
            self._create_3d_var(nc, 'w', fields.w, 'm/s',
                              'vertical wind component',
                              compression=compression)
            
            # Thermodynamic fields
            self._create_3d_var(nc, 'T', fields.T, 'K',
                              'temperature',
                              compression=compression)
            self._create_3d_var(nc, 'RH', fields.RH, '%',
                              'relative humidity',
                              compression=compression)
            self._create_3d_var(nc, 'P', fields.P, 'Pa',
                              'atmospheric pressure',
                              compression=compression)
            
            # Turbulence fields
            self._create_3d_var(nc, 'K_h', fields.K_h, 'm2/s',
                              'horizontal turbulent diffusivity',
                              compression=compression)
            self._create_3d_var(nc, 'K_v', fields.K_v, 'm2/s',
                              'vertical turbulent diffusivity',
                              compression=compression)
            
            # Surface fields
            self._create_2d_var(nc, 'u_star', fields.u_star, 'm/s',
                              'friction velocity',
                              compression=compression)
            self._create_2d_var(nc, 'terrain', fields.terrain, 'm',
                              'surface elevation',
                              compression=compression)
            
            # Stability class
            var_stab = nc.createVariable('stability_class', 'i1', ('y', 'x'),
                                        zlib=compression)
            var_stab.units = 'Pasquill-Gifford-Turner class'
            var_stab.long_name = 'atmospheric stability classification'
            var_stab.valid_range = [0, 5]
            var_stab.flag_meanings = 'A B C D E F'
            var_stab.flag_values = [0, 1, 2, 3, 4, 5]
            var_stab[:] = fields.stability_class
            
            # Mixing layer depth
            self._create_2d_var(nc, 'z_inv', fields.z_inv, 'm',
                              'atmospheric mixing layer depth',
                              compression=compression)
            
            # Precipitation (if available)
            if fields.precipitation is not None:
                self._create_2d_var(nc, 'precipitation', fields.precipitation, 'mm/h',
                                  'precipitation rate',
                                  compression=compression)
            
            # Metadata
            if fields.metadata:
                for key, value in fields.metadata.items():
                    if isinstance(value, (str, int, float)):
                        nc.setncattr(f'metadata_{key}', value)
        
        print(f"✓ Exported atmospheric fields to {filename}")
        return str(filename)
    
    def import_from_netcdf(self, filename: str) -> AtmosphericField:
        """Import atmospheric fields from NetCDF file.
        
        Parameters:
            filename (str): Input NetCDF filename
        
        Returns:
            AtmosphericField: Restored atmospheric state
        
        Raises:
            ImportError: If NetCDF4 not available
            FileNotFoundError: If file not found
        """
        if not HAS_NETCDF4:
            raise ImportError("NetCDF4 required for import. Install via: pip install netcdf4")
        
        filename = Path(filename)
        if not filename.exists():
            raise FileNotFoundError(f"NetCDF file not found: {filename}")
        
        with netCDF4.Dataset(filename, 'r') as nc:
            # Read coordinates
            coord_x = nc.variables['x'][:]
            coord_y = nc.variables['y'][:]
            coord_z = nc.variables['z'][:]
            
            # Read velocity
            u = nc.variables['u'][0, :, :]  # Remove time dimension
            v = nc.variables['v'][0, :, :]
            w = nc.variables['w'][0, :, :]
            
            # Read thermodynamics
            T = nc.variables['T'][0, :, :]
            RH = nc.variables['RH'][0, :, :]
            P = nc.variables['P'][0, :, :]
            
            # Read turbulence
            K_h = nc.variables['K_h'][0, :, :]
            K_v = nc.variables['K_v'][0, :, :]
            
            # Read surface fields
            u_star = nc.variables['u_star'][:]
            terrain = nc.variables['terrain'][:]
            stability_class = nc.variables['stability_class'][:]
            z_inv = nc.variables['z_inv'][:]
            
            # Read optional fields
            precip = nc.variables['precipitation'][:] if 'precipitation' in nc.variables else None
            
            # Read timestamp
            time_var = nc.variables['time']
            timestamp = datetime.fromisoformat(time_var.units.split(' since ')[1])
            
            # Read metadata
            metadata = {}
            for key in nc.ncattrs():
                if key.startswith('metadata_'):
                    metadata[key.replace('metadata_', '')] = nc.getncattr(key)
        
        fields = AtmosphericField(
            u=u, v=v, w=w,
            T=T, RH=RH, P=P,
            K_h=K_h, K_v=K_v,
            u_star=u_star,
            stability_class=stability_class,
            z_inv=z_inv,
            terrain=terrain,
            coord_x=coord_x,
            coord_y=coord_y,
            coord_z=coord_z,
            precipitation=precip,
            timestamp=timestamp,
            metadata=metadata
        )
        
        print(f"✓ Imported atmospheric fields from {filename}")
        return fields
    
    @staticmethod
    def _create_3d_var(nc, name: str, data: np.ndarray, units: str,
                      long_name: str, compression: bool = True):
        """Create 3D NetCDF variable with metadata.
        
        Parameters:
            nc: NetCDF4 dataset object
            name (str): Variable name
            data (ndarray): 3D data array (z, y, x)
            units (str): Variable units
            long_name (str): Descriptive name
            compression (bool): Enable compression
        """
        var = nc.createVariable(name, 'f4', ('z', 'y', 'x'),
                               zlib=compression, complevel=4)
        var.units = units
        var.long_name = long_name
        var[:] = data
    
    @staticmethod
    def _create_2d_var(nc, name: str, data: np.ndarray, units: str,
                      long_name: str, compression: bool = True):
        """Create 2D NetCDF variable with metadata.
        
        Parameters:
            nc: NetCDF4 dataset object
            name (str): Variable name
            data (ndarray): 2D data array (y, x)
            units (str): Variable units
            long_name (str): Descriptive name
            compression (bool): Enable compression
        """
        var = nc.createVariable(name, 'f4', ('y', 'x'),
                               zlib=compression, complevel=4)
        var.units = units
        var.long_name = long_name
        var[:] = data


class ASCIIExporter:
    """Export atmospheric fields to ASCII for PHREEQC boundary conditions.
    
    Provides lightweight text-based export for cases where binary NetCDF
    is unavailable or not preferred.
    """
    
    @staticmethod
    def export_temperature_profile(fields: AtmosphericField, filename: str):
        """Export 1D temperature profile as ASCII column data.
        
        Parameters:
            fields (AtmosphericField): Atmospheric state
            filename (str): Output filename
        """
        # Compute heights AGL (above ground level)
        z_agl = fields.coord_z - np.mean(fields.terrain)
        
        # Average temperature vertically
        T_profile = np.mean(fields.T, axis=(1, 2))
        
        # Write to ASCII
        with open(filename, 'w') as f:
            f.write("# Temperature profile for PHREEQC boundary condition\n")
            f.write("# Exported from massconsistent_amr wind solver\n")
            f.write("# Columns: Height_AGL[m] Temperature[K]\n")
            
            for z, T in zip(z_agl, T_profile):
                f.write(f"{z:12.2f} {T:12.4f}\n")
        
        print(f"✓ Exported temperature profile to {filename}")
    
    @staticmethod
    def export_wind_field(fields: AtmosphericField, filename: str, z_level: float = None):
        """Export 2D wind field as ASCII grid data.
        
        Parameters:
            fields (AtmosphericField): Atmospheric state
            filename (str): Output filename
            z_level (float, optional): Height level to export. If None, use surface.
        """
        if z_level is None:
            u = fields.u[0, :, :]
            v = fields.v[0, :, :]
        else:
            # Find nearest level
            k = np.argmin(np.abs(fields.coord_z - z_level))
            u = fields.u[k, :, :]
            v = fields.v[k, :, :]
        
        # Wind speed magnitude
        u_mag = np.sqrt(u**2 + v**2)
        
        # Wind direction (meteorological convention: from direction)
        u_dir = np.degrees(np.arctan2(-u, -v))
        u_dir = np.where(u_dir < 0, u_dir + 360, u_dir)
        
        # Write to ASCII grid format
        with open(filename, 'w') as f:
            f.write("# Wind field grid for PHREEQC boundary conditions\n")
            f.write("# X[m] Y[m] U[m/s] V[m/s] Speed[m/s] Direction[deg]\n")
            
            for j, y in enumerate(fields.coord_y):
                for i, x in enumerate(fields.coord_x):
                    f.write(f"{x:12.2f} {y:12.2f} {u[j,i]:8.3f} {v[j,i]:8.3f} "
                           f"{u_mag[j,i]:8.3f} {u_dir[j,i]:7.1f}\n")
        
        print(f"✓ Exported wind field to {filename}")
    
    @staticmethod
    def export_precipitation(fields: AtmosphericField, filename: str):
        """Export precipitation field as ASCII grid data.
        
        Parameters:
            fields (AtmosphericField): Atmospheric state
            filename (str): Output filename
        """
        if fields.precipitation is None:
            warnings.warn("Precipitation field not available")
            return
        
        with open(filename, 'w') as f:
            f.write("# Precipitation field for PHREEQC boundary conditions\n")
            f.write("# X[m] Y[m] Precipitation[mm/h]\n")
            
            for j, y in enumerate(fields.coord_y):
                for i, x in enumerate(fields.coord_x):
                    f.write(f"{x:12.2f} {y:12.2f} {fields.precipitation[j,i]:8.3f}\n")
        
        print(f"✓ Exported precipitation field to {filename}")
