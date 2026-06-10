#!/usr/bin/env python3
"""
reactive_transport_coupling.py - Main Interface for Wind-PHREEQC Coupling

Provides high-level functions to execute one-way coupled simulations:
wind solver → atmospheric field extraction → PHREEQC reactive transport.

This module orchestrates the complete workflow from wind field computation
to geochemical prediction for mineral weathering, AMD, and leaching studies.

References:
    - Parkhurst & Appelo (2013). Description of the PHREEQC III software
    - Stull, R.B. (2011). An introduction to boundary layer meteorology
"""

import sys
from pathlib import Path
from typing import Dict, Optional, Tuple
import numpy as np
import warnings

from geochemical_coupling import FieldExtractor, AtmosphericField, StabilityClass
from phreeqc_utils import PHREEQCGenerator, BoundaryCondition
from netcdf_io import NetCDFHandler, ASCIIExporter

try:
    import subprocess
    HAS_SUBPROCESS = True
except ImportError:
    HAS_SUBPROCESS = False


class ReactiveTransportCoupling:
    """High-level interface for wind-PHREEQC reactive transport coupling.
    
    Orchestrates extraction of atmospheric boundary conditions from wind solver
    and execution of coupled PHREEQC simulations for geochemical analysis.
    
    Example:
        >>> from wind_solver import WindSolver
        >>> from reactive_transport_coupling import ReactiveTransportCoupling
        >>>
        >>> wind = WindSolver("inputs.i")
        >>> wind.solve()
        >>>
        >>> coupling = ReactiveTransportCoupling(wind)
        >>> result = coupling.run_amd_simulation(
        ...     output_dir="amd_results/",
        ...     bc_config={"temperature": 25.0, "precipitation": 100.0}
        ... )
    """
    
    def __init__(self, wind_solver, verbose=True):
        """Initialize coupling interface.
        
        Parameters:
            wind_solver: Solved WindSolver instance
            verbose (bool): Enable diagnostic output
        """
        self.wind_solver = wind_solver
        self.verbose = verbose
        self.field_extractor = FieldExtractor(wind_solver)
        self.phreeqc_gen = PHREEQCGenerator()
        self.netcdf_io = NetCDFHandler(check_netcdf=False)
        self.fields = None  # Cached atmospheric fields
    
    def extract_fields(self, recache=False) -> AtmosphericField:
        """Extract atmospheric fields from wind solver.
        
        Parameters:
            recache (bool): Force re-extraction even if cached
        
        Returns:
            AtmosphericField: Extracted atmospheric state
        """
        if self.fields is not None and not recache:
            if self.verbose:
                print("Using cached atmospheric fields")
            return self.fields
        
        if self.verbose:
            print("Extracting atmospheric fields from wind solver...")
        
        self.fields = self.field_extractor.extract_all_fields()
        
        if self.verbose:
            print(f"✓ Extracted fields: u/v/w, T, RH, P, K_h/K_v, u*, stability")
        
        return self.fields
    
    def export_fields(self, output_dir: str, format: str = "netcdf") -> Dict[str, str]:
        """Export atmospheric fields for external use.
        
        Parameters:
            output_dir (str): Output directory
            format (str): 'netcdf' or 'ascii'
        
        Returns:
            dict: Dictionary of exported file paths
        """
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        fields = self.extract_fields()
        exports = {}
        
        if format == "netcdf" and self.netcdf_io.available:
            if self.verbose:
                print("Exporting fields to NetCDF...")
            netcdf_file = output_dir / "wind_fields.nc"
            self.netcdf_io.export_to_netcdf(fields, str(netcdf_file))
            exports["netcdf"] = str(netcdf_file)
        
        # Always export ASCII
        if self.verbose:
            print("Exporting fields to ASCII...")
        
        temp_file = output_dir / "temperature_profile.dat"
        ASCIIExporter.export_temperature_profile(fields, str(temp_file))
        exports["temperature"] = str(temp_file)
        
        wind_file = output_dir / "wind_field.dat"
        ASCIIExporter.export_wind_field(fields, str(wind_file))
        exports["wind"] = str(wind_file)
        
        if fields.precipitation is not None:
            precip_file = output_dir / "precipitation.dat"
            ASCIIExporter.export_precipitation(fields, str(precip_file))
            exports["precipitation"] = str(precip_file)
        
        return exports
    
    def run_amd_simulation(self, output_dir: str = "amd_results",
                         bc_config: Optional[Dict] = None,
                         run_phreeqc: bool = False) -> Dict:
        """Run acid mine drainage reactive transport coupling.
        
        Parameters:
            output_dir (str): Output directory
            bc_config (dict, optional): Override boundary condition config
            run_phreeqc (bool): Execute PHREEQC simulation (requires installation)
        
        Returns:
            dict: Results including generated input file path
        
        References:
            Nicholson, R.V., Gillham, R.W., & Reardon, E.J. (1990). 
            Pyrite oxidation in carbonate-buffered systems.
        """
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        if self.verbose:
            print("=== AMD Reactive Transport Simulation ===")
        
        # Extract fields
        fields = self.extract_fields()
        
        # Build boundary conditions from wind output
        if bc_config is None:
            bc_config = {}
        
        bcs = self._build_amd_boundary_conditions(fields, bc_config)
        
        # Generate PHREEQC input
        input_file = output_dir / "amd_phreeqc.dat"
        self.phreeqc_gen.generate_amd_simulation(
            str(input_file),
            boundary_conditions=bcs
        )
        
        # Validate
        is_valid, errors = self.phreeqc_gen.validate_phreeqc_input(str(input_file))
        if not is_valid and self.verbose:
            print("⚠ PHREEQC input validation warnings:")
            for err in errors:
                print(f"  - {err}")
        
        results = {
            "input_file": str(input_file),
            "output_dir": str(output_dir),
            "boundary_conditions": {k: v.value for k, v in bcs.items()},
            "fields_extracted": True,
        }
        
        if run_phreeqc:
            output_file = self._run_phreeqc(input_file, output_dir)
            results["output_file"] = output_file
        
        if self.verbose:
            print(f"✓ AMD simulation setup complete: {input_file}")
        
        return results
    
    def run_leaching_simulation(self, output_dir: str = "leaching_results",
                               mineral_type: str = "Fe2O3",
                               bc_config: Optional[Dict] = None,
                               run_phreeqc: bool = False) -> Dict:
        """Run critical mineral leaching reactive transport coupling.
        
        Parameters:
            output_dir (str): Output directory
            mineral_type (str): Primary ore mineral
            bc_config (dict, optional): Override boundary condition config
            run_phreeqc (bool): Execute PHREEQC simulation
        
        Returns:
            dict: Results including generated input file path
        """
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        if self.verbose:
            print(f"=== Mineral Leaching Simulation ({mineral_type}) ===")
        
        fields = self.extract_fields()
        
        if bc_config is None:
            bc_config = {}
        
        bcs = self._build_leaching_boundary_conditions(fields, bc_config)
        
        input_file = output_dir / "leaching_phreeqc.dat"
        self.phreeqc_gen.generate_leaching_simulation(
            str(input_file),
            boundary_conditions=bcs,
            mineral_type=mineral_type
        )
        
        is_valid, errors = self.phreeqc_gen.validate_phreeqc_input(str(input_file))
        if not is_valid and self.verbose:
            print("⚠ PHREEQC input validation warnings:")
            for err in errors:
                print(f"  - {err}")
        
        results = {
            "input_file": str(input_file),
            "output_dir": str(output_dir),
            "mineral_type": mineral_type,
            "boundary_conditions": {k: v.value for k, v in bcs.items()},
            "fields_extracted": True,
        }
        
        if run_phreeqc:
            output_file = self._run_phreeqc(input_file, output_dir)
            results["output_file"] = output_file
        
        if self.verbose:
            print(f"✓ Leaching simulation setup complete: {input_file}")
        
        return results
    
    def compute_amd_hotspot_map(self, output_dir: str = "hotspot_analysis",
                               threshold_O2: float = 100.0) -> Dict:
        """Identify AMD hotspots based on wind-modulated oxygen delivery.
        
        Parameters:
            output_dir (str): Output directory
            threshold_O2 (float): O₂ delivery threshold [umol/kg/s]
        
        Returns:
            dict: Hotspot analysis results
        """
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        if self.verbose:
            print("=== AMD Hotspot Identification ===")
        
        fields = self.extract_fields()
        
        # Compute oxygen delivery factor
        O2_factor = self.field_extractor.export_oxygen_delivery_rate(fields, z_level=0.0)
        
        # Classify hotspots (surface)
        hotspot_mask = O2_factor > (threshold_O2 / 0.1)  # Normalize
        n_hotspots = np.sum(hotspot_mask)
        
        if self.verbose:
            print(f"Identified {n_hotspots} hotspot cells")
            print(f"  Max O₂ delivery factor: {np.max(O2_factor):.2f}")
            print(f"  Min O₂ delivery factor: {np.min(O2_factor):.2f}")
        
        # Export hotspot map
        hotspot_file = output_dir / "hotspot_map.csv"
        self._export_hotspot_map(fields, O2_factor, hotspot_file)
        
        results = {
            "hotspot_map": str(hotspot_file),
            "n_hotspots": int(n_hotspots),
            "O2_factor_mean": float(np.mean(O2_factor)),
            "O2_factor_max": float(np.max(O2_factor)),
            "O2_factor_min": float(np.min(O2_factor)),
        }
        
        if self.verbose:
            print(f"✓ Hotspot analysis complete: {hotspot_file}")
        
        return results
    
    def _build_amd_boundary_conditions(self, fields: AtmosphericField,
                                      user_config: Dict) -> Dict[str, BoundaryCondition]:
        """Build AMD boundary conditions from wind fields and user config.
        
        Parameters:
            fields (AtmosphericField): Extracted atmospheric fields
            user_config (dict): User overrides
        
        Returns:
            dict: PHREEQC boundary conditions
        """
        # Temperature (surface)
        T_c = user_config.get('temperature', np.mean(fields.T[0, :, :]) - 273.15)
        
        # O₂ concentration (wind-dependent)
        O2_factor = self.field_extractor.export_oxygen_delivery_rate(fields)
        O2_conc = user_config.get('O2_concentration', 240.0) * np.mean(O2_factor)
        
        # pe (redox potential, derived from O₂)
        pe = user_config.get('pe', 12.0 + 2 * np.log10(O2_conc / 240.0))
        
        bcs = {
            'temperature': BoundaryCondition('temperature', 'float', T_c, units='C'),
            'O2_concentration': BoundaryCondition('O2', 'float', O2_conc, units='umol/kgw'),
            'pe': BoundaryCondition('pe', 'float', pe, units=''),
        }
        
        return bcs
    
    def _build_leaching_boundary_conditions(self, fields: AtmosphericField,
                                           user_config: Dict) -> Dict[str, BoundaryCondition]:
        """Build leaching boundary conditions from wind fields and user config.
        
        Parameters:
            fields (AtmosphericField): Extracted atmospheric fields
            user_config (dict): User overrides
        
        Returns:
            dict: PHREEQC boundary conditions
        """
        # Temperature (surface)
        T_c = user_config.get('temperature', np.mean(fields.T[0, :, :]) - 273.15)
        
        # CO₂ fugacity (pressure and altitude dependent)
        P_co2 = self.field_extractor.export_co2_fugacity(fields)
        co2_fug = user_config.get('co2_fugacity', np.mean(P_co2) / 101325.0)  # Convert to atm
        
        bcs = {
            'temperature': BoundaryCondition('temperature', 'float', T_c, units='C'),
            'co2_fugacity': BoundaryCondition('CO2', 'float', co2_fug, units='atm'),
        }
        
        return bcs
    
    @staticmethod
    def _run_phreeqc(input_file: str, output_dir: Path) -> str:
        """Execute PHREEQC simulation (requires PHREEQC installed).
        
        Parameters:
            input_file (str): PHREEQC input DAT file
            output_dir (Path): Output directory
        
        Returns:
            str: Output file path
        
        Raises:
            RuntimeError: If PHREEQC not found or execution fails
        """
        if not HAS_SUBPROCESS:
            raise RuntimeError("subprocess module required")
        
        # Try to find PHREEQC executable
        phreeqc_exe = None
        for candidate in ['phreeqc', 'PHREEQC', 'phreeqc.exe', 'PHREEQC.exe']:
            result = subprocess.run(['which', candidate], capture_output=True)
            if result.returncode == 0:
                phreeqc_exe = candidate
                break
        
        if phreeqc_exe is None:
            raise RuntimeError(
                "PHREEQC executable not found. Install PHREEQC from:\n"
                "  https://www.usgs.gov/mission-areas/water-resources/science/phreeqc"
            )
        
        # Run PHREEQC
        output_file = output_dir / (Path(input_file).stem + ".out")
        cmd = [phreeqc_exe, input_file, str(output_file)]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            raise RuntimeError(f"PHREEQC execution failed:\n{result.stderr}")
        
        return str(output_file)
    
    @staticmethod
    def _export_hotspot_map(fields: AtmosphericField, O2_factor: np.ndarray,
                           output_file: Path):
        """Export AMD hotspot map to CSV.
        
        Parameters:
            fields (AtmosphericField): Atmospheric fields
            O2_factor (ndarray): O₂ delivery factor map
            output_file (Path): Output CSV filename
        """
        with open(output_file, 'w') as f:
            f.write("X[m],Y[m],Elevation[m],O2_Factor[-],Wind_Speed[m/s],Stability_Class\n")
            
            ny, nx = O2_factor.shape
            u_mag_surface = np.sqrt(fields.u[0, :, :]**2 + fields.v[0, :, :]**2)
            
            for j in range(ny):
                for i in range(nx):
                    x = fields.coord_x[i]
                    y = fields.coord_y[j]
                    elev = fields.terrain[j, i]
                    O2_fact = O2_factor[j, i]
                    wind = u_mag_surface[j, i]
                    stab_class = fields.stability_class[j, i]
                    
                    f.write(f"{x:.2f},{y:.2f},{elev:.2f},{O2_fact:.4f},{wind:.3f},{stab_class}\n")
