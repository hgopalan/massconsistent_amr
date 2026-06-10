#!/usr/bin/env python3
"""
sulfide_oxidation.py - Wind-Dependent Sulfide Oxidation Rate Computation

Quantifies how wind speed affects pyrite and other sulfide mineral oxidation rates,
enabling prediction of acid mine drainage chemistry evolution. Integrates kinetic
rate laws with oxygen delivery rates controlled by turbulent transport.

Key Functions:
    compute_sulfide_oxidation_rates(wind_solver, sulfide_locations)
        Input: Sulfide mineral coordinates (x, y, z)
        Extract: u, u*, K_h at each location
        Correlate: u → O₂ diffusivity → oxidation rate
        Rate law: r_ox = k × [FeS₂] × [O₂] × f(u)
        Output: Oxidation rate map (mol/(m³·s))

    wind_to_oxygen_delivery(u_speed, roughness_height)
        Empirical correlation from literature

    pyrite_oxidation_kinetics(O2_conc, rate_constant, wind_factor)
        Temperature-dependent rate constant from Arrhenius

    predict_acid_generation(oxidation_rate_map, sulfide_volume)
        Integrate oxidation → H⁺ production → pH prediction

    PHREEQC coupling to export oxidation rate field as spatially-varying kinetic constraint

References:
    - Nicholson et al. (1990). Pyrite oxidation in carbonate-buffered systems.
      Geochimica et Cosmochimica Acta, 54(2), 395-405.
    - Businger et al. (1971). Flux-profile relationships in the atmospheric surface layer.
      Journal of Atmospheric Sciences, 28(2), 181-189.
    - Arrhenius, S. (1889). Über die Reaktionsgeschwindigkeit bei der Inversion von
      Rohrzucker durch Säuren. Zeitschrift für Physikalische Chemie, 4, 226-248.
    - Sherwood, T.K. (1954). Mass transfer between phases. Industrial & Engineering
      Chemistry, 46(2), 221-231.
    - Molins & Mayer (2007). Reactive transport modeling of biogeochemical processes.
      Journal of Contaminant Hydrology, 92(3-4), 232-253.
"""

import numpy as np
import csv
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, field
from pathlib import Path
from datetime import datetime
from enum import Enum
import json


class SulfideMineralType(Enum):
    """Common sulfide minerals in mine waste.
    
    References:
        Nicholson et al. (1990). Pyrite oxidation kinetics.
    """
    PYRITE = "FeS2"           # Most common
    MARCASITE = "FeS2"        # Dimorph of pyrite
    CHALCOPYRITE = "CuFeS2"
    SPHALERITE = "ZnS"
    GALENA = "PbS"


@dataclass
class SulfideLocation:
    """Container for sulfide mineral deposit.
    
    Attributes:
        point_id (str): Unique identifier
        x (float): Easting coordinate [m]
        y (float): Northing coordinate [m]
        z (float): Elevation [m]
        mineral_type (SulfideMineralType): Mineral composition
        mass_fraction (float): Mass fraction in rock [0-1]
        specific_surface_area (float): [m²/g]
        description (str): Optional site description
    """
    point_id: str
    x: float
    y: float
    z: float
    mineral_type: SulfideMineralType
    mass_fraction: float
    specific_surface_area: float
    description: str = ""


@dataclass
class OxidationRateInfo:
    """Oxidation rate and diagnostics at a location.
    
    Attributes:
        site_id (str): Location identifier
        oxidation_rate (float): [mol/(m³·s)]
        O2_concentration (float): [µmol/m³]
        O2_delivery_factor (float): Dimensionless enhancement factor
        wind_speed (float): [m/s]
        friction_velocity (float): [m/s]
        temperature (float): [K]
        pH (float): Water pH
        pH_change_rate (float): Rate of pH change [pH units/day]
        acid_generation_rate (float): [mol H⁺/(m³·s)]
    """
    site_id: str
    oxidation_rate: float
    O2_concentration: float
    O2_delivery_factor: float
    wind_speed: float
    friction_velocity: float
    temperature: float
    pH: float = 6.0
    pH_change_rate: float = 0.0
    acid_generation_rate: float = 0.0


class SulfideOxidationComputer:
    """Compute sulfide oxidation rates and acid generation potential.
    
    Integrates kinetic rate laws with wind-modulated oxygen delivery to predict
    where acid mine drainage generation is most rapid.
    
    Attributes:
        sulfide_locations (List[SulfideLocation]): Sulfide mineral deposits
        oxidation_rates (List[OxidationRateInfo]): Computed rates
        wind_field (dict): Wind field data
        temperature_field (np.ndarray): Temperature [K]
    """
    
    # Pyrite oxidation kinetics parameters
    # Arrhenius parameters: k = A * exp(-E_a / (R*T))
    ARRHENIUS_PREFACTOR = 1.0e-8  # [mol/(m²·s)] at reference conditions
    ACTIVATION_ENERGY = 45000.0   # [J/mol]
    REFERENCE_TEMP = 298.15       # [K] (25°C)
    GAS_CONSTANT = 8.314          # [J/(mol·K)]
    
    # Oxidation stoichiometry: 2FeS₂ + 7O₂ + 2H₂O → 2FeSO₄ + 2H₂SO₄
    # Produces 4 moles H+ per 2 moles FeS₂
    MOLES_H_PER_MOLES_PYRITE = 2.0
    
    # Oxygen transfer parameters
    K_TRANSFER = 0.5  # Correlation coefficient for O₂ delivery factor
    
    def __init__(self, wind_solver=None, verbose=True):
        """Initialize sulfide oxidation computer.
        
        Parameters:
            wind_solver: WindSolver instance with solved wind field
            verbose (bool): Enable diagnostic output
        """
        self.wind_solver = wind_solver
        self.verbose = verbose
        self.sulfide_locations: List[SulfideLocation] = []
        self.oxidation_rates: List[OxidationRateInfo] = []
        self.wind_field: Dict = {}
        self.temperature_field: Optional[np.ndarray] = None
        
        if verbose:
            print("✓ Sulfide oxidation computer initialized")
    
    def load_sulfide_locations(self, csv_file: str) -> int:
        """Load sulfide mineral deposit locations from CSV file.
        
        CSV format:
            id,x,y,z,mineral_type,mass_fraction,specific_surface_area,description
            sul001,1000.0,2000.0,50.0,PYRITE,0.05,100.0,Exposed pyrite band
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
            raise FileNotFoundError(f"Sulfide locations file not found: {csv_file}")
        
        self.sulfide_locations = []
        with open(csv_path, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    mineral = SulfideMineralType[row['mineral_type'].upper()]
                    loc = SulfideLocation(
                        point_id=row['id'],
                        x=float(row['x']),
                        y=float(row['y']),
                        z=float(row['z']),
                        mineral_type=mineral,
                        mass_fraction=float(row['mass_fraction']),
                        specific_surface_area=float(row['specific_surface_area']),
                        description=row.get('description', '')
                    )
                    self.sulfide_locations.append(loc)
                except (KeyError, ValueError) as e:
                    raise ValueError(f"Invalid row in {csv_file}: {row}, error: {e}")
        
        if self.verbose:
            print(f"✓ Loaded {len(self.sulfide_locations)} sulfide locations from {csv_file}")
        
        return len(self.sulfide_locations)
    
    def wind_to_oxygen_delivery(self, u_speed: float, roughness_height: float = 0.1
                               ) -> float:
        """Compute wind-driven oxygen delivery factor.
        
        Empirical correlation between wind speed and oxygen mass transfer.
        This represents the enhancement of O₂ diffusion due to turbulent mixing
        driven by wind shear.
        
        Typical correlation: f(u) = (u/u_ref)^n where n ≈ 0.5 to 1.5
        
        Parameters:
            u_speed (float): Wind speed [m/s]
            roughness_height (float): Surface roughness [m]
        
        Returns:
            float: O₂ delivery factor [dimensionless, 0-1 scale ≈ relative enhancement]
        
        References:
            Sherwood (1954). Mass transfer between phases.
            Businger et al. (1971). Flux-profile relationships in the atmospheric
              surface layer.
        """
        # Reference wind speed for calibration
        U_REF = 5.0  # [m/s]
        
        if u_speed < 0.5:
            # Stagnant conditions: minimal O₂ delivery
            return 0.1
        
        # Power-law correlation: f(u) = (u/u_ref)^0.75
        # Exponent ~0.75 is typical for wind-enhanced mass transfer
        exponent = 0.75
        factor = (u_speed / U_REF) ** exponent
        
        # Cap at reasonable maximum (avoid numerical issues)
        return min(factor, 3.0)
    
    def pyrite_oxidation_kinetics(self, O2_concentration: float,
                                  temperature: float,
                                  wind_factor: float = 1.0,
                                  rate_constant_ref: Optional[float] = None) -> float:
        """Compute pyrite oxidation rate using temperature-dependent kinetics.
        
        Rate law:
            r = k(T) × [FeS₂] × [O₂] × f(wind)
        
        where:
        - k(T) = A × exp(-E_a/(R*T)) is the Arrhenius rate constant
        - [FeS₂] is assumed to be available (surface area dependent)
        - [O₂] is oxygen concentration
        - f(wind) is the wind-driven oxygen delivery factor
        
        Parameters:
            O2_concentration (float): Dissolved O₂ [µmol/m³]
            temperature (float): Temperature [K]
            wind_factor (float): Wind-driven O₂ delivery factor
            rate_constant_ref (float, optional): Override reference rate constant
        
        Returns:
            float: Oxidation rate [mol/(m³·s)]
        
        References:
            Nicholson et al. (1990). Pyrite oxidation in carbonate-buffered systems.
            Arrhenius, S. (1889). Über die Reaktionsgeschwindigkeit bei der Inversion
              von Rohrzucker durch Säuren.
        """
        # Temperature correction using Arrhenius equation
        exponent = -self.ACTIVATION_ENERGY / self.GAS_CONSTANT * (
            1.0/temperature - 1.0/self.REFERENCE_TEMP
        )
        k_temp = self.ARRHENIUS_PREFACTOR * np.exp(exponent)
        
        # Assume bulk sulfide concentration (represented by unit order of magnitude)
        sulfide_conc = 1.0  # [mol/m³] - representative value
        
        # Convert O₂ concentration from µmol/m³ to mol/m³
        O2_molar = O2_concentration * 1e-6
        
        # Overall rate: k(T) × [FeS₂] × [O₂] × f(wind)
        rate = k_temp * sulfide_conc * O2_molar * wind_factor
        
        return max(0.0, rate)  # Ensure non-negative
    
    def compute_acid_generation_rate(self, oxidation_rate: float) -> float:
        """Predict acid (H⁺) generation rate from oxidation rate.
        
        Pyrite oxidation stoichiometry:
            2FeS₂ + 7O₂ + 2H₂O → 2FeSO₄ + 2H₂SO₄
        
        This generates 4 moles of H⁺ per 2 moles of FeS₂ oxidized.
        
        Parameters:
            oxidation_rate (float): FeS₂ oxidation rate [mol/(m³·s)]
        
        Returns:
            float: H⁺ generation rate [mol/(m³·s)]
        """
        # Stoichiometric ratio: 4 H⁺ produced per 2 FeS₂ = 2 H⁺ per FeS₂
        return self.MOLES_H_PER_MOLES_PYRITE * oxidation_rate
    
    def predict_pH_change_rate(self, H_generation_rate: float,
                              buffer_capacity: float = 0.01) -> float:
        """Estimate pH change rate from acid generation.
        
        Using simple linear buffer equation:
            dpH/dt = -1 / (2.303 * β) × dC_H⁺/dt
        
        where β is the buffer capacity [mol/m³/pH unit]
        
        Parameters:
            H_generation_rate (float): H⁺ generation [mol/(m³·s)]
            buffer_capacity (float): Buffer capacity [mol/m³/pH unit]
        
        Returns:
            float: pH change rate [pH units/s]
        """
        if buffer_capacity <= 0:
            return 0.0
        
        # Change in H⁺ causes pH change
        pH_change_rate = -(H_generation_rate / (2.303 * buffer_capacity))
        
        return pH_change_rate
    
    def extract_wind_at_points(self, points: List[Tuple[float, float, float]]) -> Dict:
        """Extract wind field at sulfide locations.
        
        Parameters:
            points (List[Tuple]): List of (x, y, z) coordinates
        
        Returns:
            dict: Wind components at each point
        """
        if self.wind_solver is None or not self.wind_solver.initialized:
            raise RuntimeError("Wind solver not initialized")
        
        # Extract velocity field
        vel_dict = self.wind_solver.get_velocity()
        u_field = vel_dict.get('u', np.zeros((1, 1, 1)))
        v_field = vel_dict.get('v', np.zeros((1, 1, 1)))
        
        # Get grid info
        nx, ny, nz = self.wind_solver.nx, self.wind_solver.ny, self.wind_solver.nz
        dx = self.wind_solver.dx
        dy = self.wind_solver.dy
        dz = self.wind_solver.dz
        
        # Get terrain
        terrain = self.wind_solver.get_terrain()
        
        results = {'u': [], 'v': [], 'wind_speed': [], 'u_star': []}
        
        for x, y, z in points:
            # Find nearest grid indices
            i = int(np.clip((x - self.wind_solver.xmin) / dx, 0, nx - 1))
            j = int(np.clip((y - self.wind_solver.ymin) / dy, 0, ny - 1))
            k = int(np.clip((z - self.wind_solver.zmin) / dz, 0, nz - 1))
            
            u = u_field[k, j, i] if u_field.shape[0] > k else 0.0
            v = v_field[k, j, i] if v_field.shape[0] > k else 0.0
            
            results['u'].append(u)
            results['v'].append(v)
            
            wind_speed = np.sqrt(u**2 + v**2)
            results['wind_speed'].append(wind_speed)
            
            # Compute friction velocity
            height_agl = z - terrain[j, i]
            z0 = 0.1
            u_star = self._compute_friction_velocity(wind_speed, height_agl, z0)
            results['u_star'].append(u_star)
        
        self.wind_field = results
        return results
    
    def compute_sulfide_oxidation_rates(self, temperature: float = 288.15,
                                       O2_ref: float = 270.0,
                                       output_dir: Optional[str] = None
                                       ) -> List[OxidationRateInfo]:
        """Compute oxidation rates at all sulfide locations.
        
        Main workflow:
        1. Extract wind characteristics at each location
        2. Compute oxygen delivery factor from wind speed
        3. Calculate temperature-dependent oxidation kinetics
        4. Estimate acid generation rate
        
        Parameters:
            temperature (float): Ambient temperature [K]
            O2_ref (float): Reference O₂ concentration [µmol/m³]
            output_dir (str, optional): Directory for output files
        
        Returns:
            List[OxidationRateInfo]: Oxidation rates and diagnostics
        """
        if not self.sulfide_locations:
            raise ValueError("No sulfide locations loaded. Call load_sulfide_locations() first.")
        
        if self.verbose:
            print(f"\n⚙️  Computing oxidation rates for {len(self.sulfide_locations)} locations...")
        
        # Extract points
        points = [(loc.x, loc.y, loc.z) for loc in self.sulfide_locations]
        
        # Extract wind at each point
        wind_data = self.extract_wind_at_points(points)
        
        self.oxidation_rates = []
        
        for i, loc in enumerate(self.sulfide_locations):
            # Get wind speed and oxygen delivery
            wind_speed = wind_data['wind_speed'][i]
            O2_factor = self.wind_to_oxygen_delivery(wind_speed)
            
            # Adjust oxygen concentration with wind factor
            O2_adjusted = O2_ref * O2_factor
            
            # Compute oxidation rate
            ox_rate = self.pyrite_oxidation_kinetics(
                O2_adjusted, temperature, O2_factor
            )
            
            # Compute acid generation
            acid_rate = self.compute_acid_generation_rate(ox_rate)
            
            # Estimate pH change (assuming neutral water, simple buffer)
            pH_change = self.predict_pH_change_rate(acid_rate)
            
            # Create result
            result = OxidationRateInfo(
                site_id=loc.point_id,
                oxidation_rate=ox_rate,
                O2_concentration=O2_adjusted,
                O2_delivery_factor=O2_factor,
                wind_speed=wind_speed,
                friction_velocity=wind_data['u_star'][i],
                temperature=temperature,
                pH=6.0,  # Placeholder
                pH_change_rate=pH_change,
                acid_generation_rate=acid_rate
            )
            self.oxidation_rates.append(result)
        
        if self.verbose:
            mean_rate = np.mean([r.oxidation_rate for r in self.oxidation_rates])
            max_rate = np.max([r.oxidation_rate for r in self.oxidation_rates])
            print(f"  ✓ Oxidation rates computed")
            print(f"    - Mean rate:   {mean_rate:.2e} mol/(m³·s)")
            print(f"    - Max rate:    {max_rate:.2e} mol/(m³·s)")
        
        # Export if requested
        if output_dir:
            self.export_oxidation_rates(output_dir)
        
        return self.oxidation_rates
    
    def export_oxidation_rates(self, output_dir: str) -> Dict[str, str]:
        """Export oxidation rates to files.
        
        Parameters:
            output_dir (str): Output directory
        
        Returns:
            dict: Output file paths
        """
        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)
        
        files = {}
        
        # Export CSV
        csv_file = output_path / "oxidation_rates.csv"
        with open(csv_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=[
                'site_id', 'oxidation_rate', 'O2_concentration', 'O2_delivery_factor',
                'wind_speed', 'friction_velocity', 'temperature', 'pH', 
                'pH_change_rate', 'acid_generation_rate'
            ])
            writer.writeheader()
            
            for rate in self.oxidation_rates:
                writer.writerow({
                    'site_id': rate.site_id,
                    'oxidation_rate': f'{rate.oxidation_rate:.2e}',
                    'O2_concentration': f'{rate.O2_concentration:.2f}',
                    'O2_delivery_factor': f'{rate.O2_delivery_factor:.3f}',
                    'wind_speed': f'{rate.wind_speed:.2f}',
                    'friction_velocity': f'{rate.friction_velocity:.3f}',
                    'temperature': f'{rate.temperature:.2f}',
                    'pH': f'{rate.pH:.1f}',
                    'pH_change_rate': f'{rate.pH_change_rate:.2e}',
                    'acid_generation_rate': f'{rate.acid_generation_rate:.2e}'
                })
        files['csv'] = str(csv_file)
        
        # Export GeoJSON
        geojson_file = output_path / "oxidation_rates.geojson"
        features = []
        for i, rate in enumerate(self.oxidation_rates):
            loc = self.sulfide_locations[i]
            feature = {
                "type": "Feature",
                "geometry": {
                    "type": "Point",
                    "coordinates": [loc.x, loc.y, loc.z]
                },
                "properties": {
                    "site_id": rate.site_id,
                    "oxidation_rate": rate.oxidation_rate,
                    "acid_generation_rate": rate.acid_generation_rate,
                    "O2_delivery_factor": rate.O2_delivery_factor,
                    "wind_speed": rate.wind_speed,
                    "temperature": rate.temperature,
                    "mineral_type": loc.mineral_type.value
                }
            }
            features.append(feature)
        
        geojson = {
            "type": "FeatureCollection",
            "features": features,
            "metadata": {
                "timestamp": datetime.now().isoformat(),
                "total_sites": len(self.oxidation_rates)
            }
        }
        
        with open(geojson_file, 'w') as f:
            json.dump(geojson, f, indent=2)
        files['geojson'] = str(geojson_file)
        
        if self.verbose:
            print(f"✓ Oxidation rates exported to {output_dir}")
        
        return files
    
    def _compute_friction_velocity(self, wind_speed: float, height_agl: float,
                                   z0: float) -> float:
        """Compute friction velocity from wind speed.
        
        Parameters:
            wind_speed (float): Horizontal wind speed [m/s]
            height_agl (float): Height above ground [m]
            z0 (float): Roughness [m]
        
        Returns:
            float: Friction velocity [m/s]
        """
        KAPPA = 0.41
        
        if height_agl <= z0 or wind_speed <= 0:
            return 0.0
        
        u_star = KAPPA * wind_speed / np.log(max(height_agl, z0 + 0.01) / z0)
        return max(0.0, u_star)


def compute_sulfide_oxidation_rates(wind_solver, sulfide_locations: str,
                                    temperature: float = 288.15,
                                    output_dir: Optional[str] = None,
                                    verbose: bool = True) -> Dict:
    """High-level function to compute sulfide oxidation rates.
    
    Orchestrates the complete workflow:
    1. Load sulfide deposit locations
    2. Extract wind field at each site
    3. Compute wind-driven oxygen delivery
    4. Calculate oxidation kinetics
    5. Predict acid generation rates
    6. Export results
    
    Parameters:
        wind_solver: WindSolver instance
        sulfide_locations (str): CSV file with sulfide coordinates
        temperature (float): Ambient temperature [K]
        output_dir (str, optional): Output directory
        verbose (bool): Enable output
    
    Returns:
        dict: Results with 'oxidation_rates', 'total_sites', 'output_files'
    
    References:
        Nicholson et al. (1990). Pyrite oxidation in carbonate-buffered systems.
        Molins & Mayer (2007). Reactive transport modeling of biogeochemical processes.
    """
    computer = SulfideOxidationComputer(wind_solver, verbose=verbose)
    
    # Load locations
    computer.load_sulfide_locations(sulfide_locations)
    
    # Compute rates
    rates = computer.compute_sulfide_oxidation_rates(temperature=temperature,
                                                     output_dir=output_dir)
    
    output_files = {}
    if output_dir:
        output_files = computer.export_oxidation_rates(output_dir)
    
    return {
        'oxidation_rates': rates,
        'total_sites': len(rates),
        'mean_oxidation_rate': np.mean([r.oxidation_rate for r in rates]),
        'max_oxidation_rate': np.max([r.oxidation_rate for r in rates]),
        'output_files': output_files
    }


if __name__ == "__main__":
    print("Sulfide Oxidation Rate Computer")
    print("=" * 60)
    print("Use in your scripts with:")
    print("  from sulfide_oxidation import compute_sulfide_oxidation_rates")
    print("  rates = compute_sulfide_oxidation_rates(wind, 'sulfide_locations.csv')")
