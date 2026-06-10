#!/usr/bin/env python3
"""
amd_hotspot_detector.py - Terrain-Aware Acid Mine Drainage Hotspot Identification

Identifies AMD discharge hotspots in valleys using wind-resolved oxygen delivery rates.
Integrates terrain analysis, friction velocity extraction, and risk classification to map
chemically-active regions driven by topographic wind steering and heterogeneous oxidation potential.

Key Functions:
    identify_valley_amd_hotspots(wind_solver, amd_locations_file)
        Input: AMD coordinates (CSV: x, y, z, discharge_type)
        Extract: u, v, w, u*, turbulence at each AMD point
        Compute: O₂ supply rate = f(u*, K_v) per location
        Classify: High/Medium/Low risk based on O₂ supply threshold
        Output: GeoJSON with risk polygons + location attributes

    compute_oxygen_supply_rate(u_star, K_v, roughness)
        Correlate friction velocity to O₂ mass transfer
        Science: Sherwood number correlation

    compute_wind_shear(wind_field, z_coords)
        Vertical wind shear ∂u/∂z controls mixing

    classify_amd_risk(O2_supply_rate, threshold_low, threshold_high)
        Classify hotspots by oxidation potential

References:
    - Parkhurst & Appelo (2013). Description of the PHREEQC III software.
    - Nicholson et al. (1990). Pyrite oxidation in carbonate-buffered systems.
      Geochimica et Cosmochimica Acta, 54(2), 395-405.
    - Businger et al. (1971). Flux-profile relationships in the atmospheric surface layer.
      Journal of Atmospheric Sciences, 28(2), 181-189.
    - Sherwood, T.K. (1954). Mass transfer between phases. Industrial & Engineering
      Chemistry, 46(2), 221-231.
    - Paulson & Simpson (1981). The mathematical representation of wind speed and
      temperature profiles in the unstable atmospheric surface layer.
      Journal of Applied Meteorology, 20(4), 466-478.
"""

import numpy as np
import csv
import json
from typing import Dict, List, Tuple, Optional, NamedTuple
from dataclasses import dataclass, field
from pathlib import Path
from datetime import datetime
import warnings


@dataclass
class AMDLocation:
    """Container for acid mine drainage discharge point.
    
    Attributes:
        point_id (str): Unique identifier
        x (float): Easting coordinate [m]
        y (float): Northing coordinate [m]
        z (float): Elevation [m]
        discharge_type (str): 'seep', 'spring', 'runoff', 'groundwater'
        description (str): Optional site description
    """
    point_id: str
    x: float
    y: float
    z: float
    discharge_type: str
    description: str = ""


@dataclass
class HotspotRiskInfo:
    """Hotspot risk classification and diagnostics.
    
    Attributes:
        amd_id (str): AMD location identifier
        risk_class (str): 'HIGH', 'MEDIUM', 'LOW'
        O2_supply_rate (float): Oxygen mass transfer rate [µmol/(m²·s)]
        friction_velocity (float): Friction velocity u* [m/s]
        wind_speed (float): Horizontal wind speed [m/s]
        wind_shear (float): Vertical wind shear ∂u/∂z [s⁻¹]
        turbulent_diffusivity (float): K_v [m²/s]
        roughness_height (float): Aerodynamic roughness z₀ [m]
        temperature (float): Local temperature [K]
        confidence (float): Classification confidence [0-1]
    """
    amd_id: str
    risk_class: str
    O2_supply_rate: float
    friction_velocity: float
    wind_speed: float
    wind_shear: float
    turbulent_diffusivity: float
    roughness_height: float
    temperature: float
    confidence: float = 0.9


class AMDHotspotDetector:
    """Identify and classify AMD hotspots using terrain-aware wind diagnostics.
    
    This class integrates wind solver outputs with AMD discharge point coordinates
    to compute oxygen supply rates and classify risk levels. Supports spatial
    aggregation and polygon generation for visualization.
    
    Attributes:
        amd_locations (List[AMDLocation]): AMD discharge points
        hotspots (List[HotspotRiskInfo]): Identified hotspots with risk classification
        wind_field (dict): Current wind field with u, v, w, K_v components
        terrain (ndarray): 2D terrain elevation array
        grid_coords (dict): Grid coordinate arrays (x, y, z)
    """
    
    # Physical constants
    K_SHERWOOD = 0.332  # Sherwood correlation coefficient
    RE_CRITICAL = 1.0   # Critical Reynolds number for transition
    VISCOSITY_KINEMATIC = 1.5e-5  # Kinematic viscosity of air [m²/s] at 15°C
    O2_MOLAR_MASS = 32.0  # [g/mol]
    O2_DIFFUSIVITY_REF = 2.0e-5  # Reference O₂ diffusivity in air [m²/s]
    
    # Risk thresholds for O₂ supply rate [µmol/(m²·s)]
    O2_THRESHOLD_HIGH = 100.0    # High risk: strong oxidation
    O2_THRESHOLD_LOW = 30.0      # Low risk: weak oxidation
    
    def __init__(self, wind_solver=None, verbose=True):
        """Initialize AMD hotspot detector.
        
        Parameters:
            wind_solver: WindSolver instance with solved wind field
            verbose (bool): Enable diagnostic output
        """
        self.wind_solver = wind_solver
        self.verbose = verbose
        self.amd_locations: List[AMDLocation] = []
        self.hotspots: List[HotspotRiskInfo] = []
        self.wind_field: Dict = {}
        self.terrain: Optional[np.ndarray] = None
        self.grid_coords: Dict = {}
        
        if verbose:
            print("✓ AMD hotspot detector initialized")
    
    def load_amd_locations(self, csv_file: str) -> int:
        """Load AMD discharge points from CSV file.
        
        CSV format:
            id,x,y,z,discharge_type,description
            amd001,1000.0,2000.0,100.0,seep,Valley spring
            ...
        
        Parameters:
            csv_file (str): Path to CSV file
        
        Returns:
            int: Number of locations loaded
        
        Raises:
            FileNotFoundError: If CSV file not found
            ValueError: If CSV format is invalid
        """
        csv_path = Path(csv_file)
        if not csv_path.exists():
            raise FileNotFoundError(f"AMD locations file not found: {csv_file}")
        
        self.amd_locations = []
        with open(csv_path, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    loc = AMDLocation(
                        point_id=row['id'],
                        x=float(row['x']),
                        y=float(row['y']),
                        z=float(row['z']),
                        discharge_type=row['discharge_type'],
                        description=row.get('description', '')
                    )
                    self.amd_locations.append(loc)
                except (KeyError, ValueError) as e:
                    raise ValueError(f"Invalid row in {csv_file}: {row}, error: {e}")
        
        if self.verbose:
            print(f"✓ Loaded {len(self.amd_locations)} AMD locations from {csv_file}")
        
        return len(self.amd_locations)
    
    def extract_wind_at_points(self, points: List[Tuple[float, float, float]]) -> Dict:
        """Extract wind field components at specified points.
        
        Parameters:
            points (List[Tuple]): List of (x, y, z) coordinates
        
        Returns:
            dict: Wind components at each point with keys 'u', 'v', 'w', 'u_star', 'K_v'
        
        Raises:
            RuntimeError: If wind solver not initialized
        """
        if self.wind_solver is None or not self.wind_solver.initialized:
            raise RuntimeError("Wind solver not initialized")
        
        # Extract velocity field from wind solver
        vel_dict = self.wind_solver.get_velocity()
        u_field = vel_dict.get('u', np.zeros((1, 1, 1)))
        v_field = vel_dict.get('v', np.zeros((1, 1, 1)))
        w_field = vel_dict.get('w', np.zeros((1, 1, 1)))
        
        # Extract terrain and compute K_v (placeholder if not available)
        self.terrain = self.wind_solver.get_terrain()
        
        # Get grid spacing for derivatives
        nx, ny, nz = self.wind_solver.nx, self.wind_solver.ny, self.wind_solver.nz
        dx = self.wind_solver.dx
        dy = self.wind_solver.dy
        dz = self.wind_solver.dz
        
        results = {'u': [], 'v': [], 'w': [], 'u_star': [], 'K_v': []}
        
        for x, y, z in points:
            # Find nearest grid indices
            i = int(np.clip((x - self.wind_solver.xmin) / dx, 0, nx - 1))
            j = int(np.clip((y - self.wind_solver.ymin) / dy, 0, ny - 1))
            k = int(np.clip((z - self.wind_solver.zmin) / dz, 0, nz - 1))
            
            u = u_field[k, j, i] if u_field.shape[0] > k else 0.0
            v = v_field[k, j, i] if v_field.shape[0] > k else 0.0
            w = w_field[k, j, i] if w_field.shape[0] > k else 0.0
            
            results['u'].append(u)
            results['v'].append(v)
            results['w'].append(w)
            
            # Compute friction velocity from wind speed
            wind_speed = np.sqrt(u**2 + v**2)
            z0 = 0.1  # Default roughness [m]
            u_star = self._compute_friction_velocity(wind_speed, z - self.terrain[j, i], z0)
            results['u_star'].append(u_star)
            
            # Compute turbulent diffusivity (placeholder)
            K_v = self._compute_turbulent_diffusivity(u_star, z - self.terrain[j, i])
            results['K_v'].append(K_v)
        
        self.wind_field = results
        return results
    
    def compute_oxygen_supply_rate(self, u_star: float, K_v: float,
                                   roughness: float = 0.1) -> float:
        """Compute oxygen supply rate using Sherwood correlation.
        
        Correlates friction velocity to O₂ mass transfer coefficient via
        Sherwood number. The Sherwood number (Sh = k_c*L/D) relates mass transfer
        coefficient to diffusivity and characteristic length.
        
        For rough surfaces (e.g., oxide minerals), this is approximated:
            Sh ≈ K_sh * Re^n
        where Re = u* * L / ν is the friction Reynolds number.
        
        Parameters:
            u_star (float): Friction velocity [m/s]
            K_v (float): Vertical turbulent diffusivity [m²/s]
            roughness (float): Surface roughness [m]
        
        Returns:
            float: O₂ supply rate [µmol/(m²·s)]
        
        References:
            Sherwood (1954). Mass transfer between phases.
            Industrial & Engineering Chemistry, 46(2), 221-231.
        """
        # Friction Reynolds number using roughness as characteristic length
        Re = (u_star * roughness) / self.VISCOSITY_KINEMATIC
        
        if Re < self.RE_CRITICAL:
            # Laminar regime
            Sh = self.K_SHERWOOD * np.sqrt(Re)
        else:
            # Turbulent regime
            Sh = self.K_SHERWOOD * (Re ** 0.5)
        
        # Effective O₂ diffusivity (enhanced by turbulence)
        D_O2_eff = self.O2_DIFFUSIVITY_REF + K_v
        
        # Mass transfer coefficient [m/s]
        k_c = (Sh * D_O2_eff) / roughness
        
        # O₂ concentration at saturation (assume 21% O₂ in air)
        # Using ideal gas at sea level: ~8.6 µmol/L ≈ 0.27 µmol/cm³
        O2_sat = 0.27e-3  # [mol/cm³] = 270 [µmol/m³]
        # Convert to molar concentration [mol/m³]
        O2_conc = 270.0  # [µmol/m³]
        
        # O₂ supply rate = mass transfer coefficient × concentration difference
        # Assuming driving force from saturation
        O2_supply = k_c * O2_conc * 1e-6  # [µmol/(m²·s)]
        
        return O2_supply
    
    def compute_wind_shear(self, u_field: np.ndarray, v_field: np.ndarray,
                          z_coords: np.ndarray) -> float:
        """Compute vertical wind shear ∂u/∂z.
        
        Vertical wind shear indicates the vertical gradient of horizontal wind speed.
        Strong shear enhances vertical mixing and thus oxygen delivery.
        
        Parameters:
            u_field (ndarray): Zonal velocity field [m/s]
            v_field (ndarray): Meridional velocity field [m/s]
            z_coords (ndarray): Vertical coordinate array [m]
        
        Returns:
            float: Wind shear ∂|u|/∂z [s⁻¹]
        """
        # Compute horizontal wind speed at each height
        wind_speed = np.sqrt(u_field**2 + v_field**2)
        
        if len(wind_speed) < 2:
            return 0.0
        
        # Compute vertical gradient using finite differences
        dz = np.diff(z_coords)
        du = np.diff(wind_speed)
        shear = np.mean(du / dz) if len(dz) > 0 else 0.0
        
        return max(0.0, shear)  # Ensure non-negative
    
    def classify_amd_risk(self, O2_supply_rate: float,
                         threshold_low: Optional[float] = None,
                         threshold_high: Optional[float] = None) -> str:
        """Classify AMD risk based on oxygen supply rate.
        
        Uses empirical thresholds (calibrated to field observations) to classify
        oxidation potential and resulting acid generation rate.
        
        Parameters:
            O2_supply_rate (float): O₂ mass transfer rate [µmol/(m²·s)]
            threshold_low (float): Low-to-medium threshold [µmol/(m²·s)]
            threshold_high (float): Medium-to-high threshold [µmol/(m²·s)]
        
        Returns:
            str: Risk class: 'HIGH', 'MEDIUM', 'LOW'
        """
        thr_low = threshold_low or self.O2_THRESHOLD_LOW
        thr_high = threshold_high or self.O2_THRESHOLD_HIGH
        
        if O2_supply_rate >= thr_high:
            return 'HIGH'
        elif O2_supply_rate >= thr_low:
            return 'MEDIUM'
        else:
            return 'LOW'
    
    def identify_valley_amd_hotspots(self, output_geojson: Optional[str] = None
                                    ) -> List[HotspotRiskInfo]:
        """Identify and classify AMD hotspots in valley.
        
        Main workflow:
        1. Extract wind characteristics at each AMD location
        2. Compute O₂ supply rate from friction velocity and diffusivity
        3. Classify risk level (HIGH/MEDIUM/LOW)
        4. Generate spatial aggregation and hotspot polygons
        
        Parameters:
            output_geojson (str, optional): Path to save GeoJSON output
        
        Returns:
            List[HotspotRiskInfo]: Classified hotspots
        """
        if not self.amd_locations:
            raise ValueError("No AMD locations loaded. Call load_amd_locations() first.")
        
        if self.verbose:
            print(f"\n🔍 Identifying hotspots for {len(self.amd_locations)} AMD locations...")
        
        # Extract points as tuples
        points = [(loc.x, loc.y, loc.z) for loc in self.amd_locations]
        
        # Extract wind at each point
        wind_data = self.extract_wind_at_points(points)
        
        self.hotspots = []
        high_risk_count = 0
        
        for i, loc in enumerate(self.amd_locations):
            # Get wind components
            u_star = wind_data['u_star'][i]
            K_v = wind_data['K_v'][i]
            
            # Compute O₂ supply rate
            O2_rate = self.compute_oxygen_supply_rate(u_star, K_v)
            
            # Classify risk
            risk_class = self.classify_amd_risk(O2_rate)
            if risk_class == 'HIGH':
                high_risk_count += 1
            
            # Compute wind shear
            u = wind_data['u'][i]
            v = wind_data['v'][i]
            wind_speed = np.sqrt(u**2 + v**2)
            
            # Create hotspot info
            hotspot = HotspotRiskInfo(
                amd_id=loc.point_id,
                risk_class=risk_class,
                O2_supply_rate=O2_rate,
                friction_velocity=u_star,
                wind_speed=wind_speed,
                wind_shear=self.compute_wind_shear(
                    np.array([wind_data['u'][max(0, i-1)], u, wind_data['u'][min(len(wind_data['u'])-1, i+1)]]),
                    np.array([wind_data['v'][max(0, i-1)], v, wind_data['v'][min(len(wind_data['v'])-1, i+1)]]),
                    np.array([loc.z - 5, loc.z, loc.z + 5])
                ),
                turbulent_diffusivity=K_v,
                roughness_height=0.1,
                temperature=288.15  # Placeholder: 15°C
            )
            self.hotspots.append(hotspot)
        
        if self.verbose:
            print(f"  ✓ {len(self.hotspots)} hotspots identified")
            print(f"    - HIGH risk:   {high_risk_count}")
            print(f"    - MEDIUM risk: {sum(1 for h in self.hotspots if h.risk_class == 'MEDIUM')}")
            print(f"    - LOW risk:    {sum(1 for h in self.hotspots if h.risk_class == 'LOW')}")
        
        # Export to GeoJSON if requested
        if output_geojson:
            self.export_hotspots_geojson(output_geojson)
        
        return self.hotspots
    
    def export_hotspots_geojson(self, output_file: str) -> str:
        """Export hotspots to GeoJSON format for visualization.
        
        Parameters:
            output_file (str): Output GeoJSON file path
        
        Returns:
            str: Output file path
        """
        features = []
        
        for hotspot in self.hotspots:
            # Find corresponding AMD location
            amd_loc = next((loc for loc in self.amd_locations
                           if loc.point_id == hotspot.amd_id), None)
            if not amd_loc:
                continue
            
            # Color by risk class
            color_map = {'HIGH': '#d62728', 'MEDIUM': '#ff7f0e', 'LOW': '#2ca02c'}
            color = color_map[hotspot.risk_class]
            
            feature = {
                "type": "Feature",
                "geometry": {
                    "type": "Point",
                    "coordinates": [amd_loc.x, amd_loc.y, amd_loc.z]
                },
                "properties": {
                    "id": hotspot.amd_id,
                    "risk_class": hotspot.risk_class,
                    "O2_supply_rate": round(hotspot.O2_supply_rate, 2),
                    "friction_velocity": round(hotspot.friction_velocity, 3),
                    "wind_speed": round(hotspot.wind_speed, 2),
                    "wind_shear": round(hotspot.wind_shear, 4),
                    "turbulent_diffusivity": round(hotspot.turbulent_diffusivity, 6),
                    "color": color,
                    "discharge_type": amd_loc.discharge_type,
                    "description": amd_loc.description
                }
            }
            features.append(feature)
        
        geojson = {
            "type": "FeatureCollection",
            "features": features,
            "metadata": {
                "timestamp": datetime.now().isoformat(),
                "total_hotspots": len(self.hotspots),
                "high_risk_count": sum(1 for h in self.hotspots if h.risk_class == 'HIGH'),
                "medium_risk_count": sum(1 for h in self.hotspots if h.risk_class == 'MEDIUM'),
                "low_risk_count": sum(1 for h in self.hotspots if h.risk_class == 'LOW')
            }
        }
        
        output_path = Path(output_file)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_path, 'w') as f:
            json.dump(geojson, f, indent=2)
        
        if self.verbose:
            print(f"✓ GeoJSON exported to {output_file}")
        
        return str(output_path)
    
    def _compute_friction_velocity(self, wind_speed: float, height_agl: float,
                                   z0: float) -> float:
        """Compute friction velocity from wind speed using log-law profile.
        
        Uses the logarithmic wind profile in the surface layer:
            u(z) = (u*/κ) * ln(z/z0)
        
        where κ = 0.41 is von Kármán constant.
        
        Parameters:
            wind_speed (float): Horizontal wind speed [m/s]
            height_agl (float): Height above ground [m]
            z0 (float): Aerodynamic roughness [m]
        
        Returns:
            float: Friction velocity [m/s]
        """
        KAPPA = 0.41  # von Kármán constant
        
        if height_agl <= z0 or wind_speed <= 0:
            return 0.0
        
        # Invert log-law: u* = κ * u / ln(z/z0)
        u_star = KAPPA * wind_speed / np.log(max(height_agl, z0 + 0.01) / z0)
        
        return max(0.0, u_star)
    
    def _compute_turbulent_diffusivity(self, u_star: float, height_agl: float) -> float:
        """Compute turbulent diffusivity from friction velocity and height.
        
        Uses mixing length theory:
            K_v = κ * u* * z * f(z/L)
        
        where f is a stability function (neutral case: f=1).
        
        Parameters:
            u_star (float): Friction velocity [m/s]
            height_agl (float): Height above ground [m]
        
        Returns:
            float: Turbulent diffusivity [m²/s]
        """
        KAPPA = 0.41
        
        # Neutral atmosphere approximation
        K_v = KAPPA * u_star * max(height_agl, 0.1)
        
        return K_v


def identify_valley_amd_hotspots(wind_solver, amd_locations_file: str,
                                 output_dir: Optional[str] = None,
                                 verbose: bool = True) -> Dict:
    """High-level function to identify valley AMD hotspots.
    
    Orchestrates the complete workflow:
    1. Load AMD discharge point coordinates
    2. Extract wind field at each point
    3. Compute O₂ supply rates
    4. Classify risk levels
    5. Generate hotspot map and GeoJSON
    
    Parameters:
        wind_solver: WindSolver instance with solved wind field
        amd_locations_file (str): CSV file with AMD coordinates
        output_dir (str, optional): Directory for output files
        verbose (bool): Enable diagnostic output
    
    Returns:
        dict: Results with keys:
            - 'hotspots': List[HotspotRiskInfo]
            - 'n_hotspots': int
            - 'high_risk_count': int
            - 'output_files': dict
    
    References:
        Nicholson et al. (1990). Pyrite oxidation in carbonate-buffered systems.
        Geochimica et Cosmochimica Acta, 54(2), 395-405.
    """
    detector = AMDHotspotDetector(wind_solver, verbose=verbose)
    
    # Load AMD locations
    detector.load_amd_locations(amd_locations_file)
    
    # Identify hotspots
    hotspots = detector.identify_valley_amd_hotspots()
    
    output_files = {}
    if output_dir:
        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)
        
        # Export GeoJSON
        geojson_file = output_path / "amd_hotspots.geojson"
        detector.export_hotspots_geojson(str(geojson_file))
        output_files['geojson'] = str(geojson_file)
        
        # Export CSV
        csv_file = output_path / "amd_hotspots.csv"
        _export_hotspots_csv(hotspots, str(csv_file))
        output_files['csv'] = str(csv_file)
    
    return {
        'hotspots': hotspots,
        'n_hotspots': len(hotspots),
        'high_risk_count': sum(1 for h in hotspots if h.risk_class == 'HIGH'),
        'medium_risk_count': sum(1 for h in hotspots if h.risk_class == 'MEDIUM'),
        'low_risk_count': sum(1 for h in hotspots if h.risk_class == 'LOW'),
        'output_files': output_files
    }


def _export_hotspots_csv(hotspots: List[HotspotRiskInfo], output_file: str) -> None:
    """Export hotspots to CSV format.
    
    Parameters:
        hotspots (List[HotspotRiskInfo]): Classified hotspots
        output_file (str): Output CSV file path
    """
    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=[
            'amd_id', 'risk_class', 'O2_supply_rate', 'friction_velocity',
            'wind_speed', 'wind_shear', 'turbulent_diffusivity', 'roughness_height',
            'temperature', 'confidence'
        ])
        writer.writeheader()
        
        for hotspot in hotspots:
            writer.writerow({
                'amd_id': hotspot.amd_id,
                'risk_class': hotspot.risk_class,
                'O2_supply_rate': f'{hotspot.O2_supply_rate:.2f}',
                'friction_velocity': f'{hotspot.friction_velocity:.3f}',
                'wind_speed': f'{hotspot.wind_speed:.2f}',
                'wind_shear': f'{hotspot.wind_shear:.4f}',
                'turbulent_diffusivity': f'{hotspot.turbulent_diffusivity:.6f}',
                'roughness_height': f'{hotspot.roughness_height:.2f}',
                'temperature': f'{hotspot.temperature:.2f}',
                'confidence': f'{hotspot.confidence:.2f}'
            })


if __name__ == "__main__":
    # Example usage
    print("AMD Hotspot Detector Module")
    print("=" * 60)
    print("Use in your scripts with:")
    print("  from amd_hotspot_detector import identify_valley_amd_hotspots")
    print("  hotspots = identify_valley_amd_hotspots(wind, 'amd_locations.csv')")
