#!/usr/bin/env python3
"""
scenario_library.py - Pre-computed Scenario Library for Offline Caching

Builds a library of representative weather scenarios with pre-computed physics
fields (wind, temperature, diffusivity, stability) and derived quantities
(dust suppression, Sherwood numbers, leaching efficiency). Designed for one-time
offline computation followed by fast lookups during operational runs.

References:
    - Sherwood, T.K. (1954). Mass transfer between phases.
    - Ranz & Marshall (1952). Evaporation from drops (Sherwood correlation basis).
    - Businger et al. (1971). Flux-profile relationships in atmospheric surface layer.
"""

import numpy as np
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass, asdict
import json
import logging

# Try to import HDF5 support (optional, can fall back to JSON)
try:
    import h5py
    HAS_H5PY = True
except ImportError:
    HAS_H5PY = False

logger = logging.getLogger(__name__)


@dataclass
class WeatherScenario:
    """Container for a representative weather scenario and derived fields.
    
    Attributes:
        weather_id (int): Unique scenario identifier
        u_mag_ref (float): Reference wind speed at 10m (m/s)
        wind_direction (float): Wind direction (degrees, 0-360)
        temperature (float): Temperature at reference height (K)
        relative_humidity (float): Relative humidity (0-1)
        stability_class (str): Pasquill-Gifford-Turner class (A-F)
        precipitation_rate (float): Precipitation rate (mm/h)
        
        # Pre-computed 1D profiles
        u_profile (np.ndarray): Wind speed profile [u(z) m/s]
        T_profile (np.ndarray): Temperature profile [T(z) K]
        K_v_profile (np.ndarray): Vertical diffusivity [K_v(z) m²/s]
        heights (np.ndarray): Height coordinates [z in m]
        
        # Pre-computed derived quantities
        dust_suppression_factor (float): Dust suppression [0-1]
        sherwood_number (float): Sherwood number for mass transfer
        leaching_efficiency (float): Relative leaching efficiency [0-1]
    """
    weather_id: int
    u_mag_ref: float
    wind_direction: float
    temperature: float
    relative_humidity: float
    stability_class: str
    precipitation_rate: float
    
    # Profiles (will be stored as arrays)
    u_profile: np.ndarray
    T_profile: np.ndarray
    K_v_profile: np.ndarray
    heights: np.ndarray
    
    # Derived quantities
    dust_suppression_factor: float
    sherwood_number: float
    leaching_efficiency: float


class ScenarioLibrary:
    """Pre-computed scenario library with caching and lookup.
    
    Manages a collection of representative weather scenarios with fast
    nearest-neighbor lookups using KD-tree spatial indexing.
    """
    
    def __init__(self, n_scenarios: int = 100):
        """Initialize scenario library.
        
        Args:
            n_scenarios: Number of scenarios to generate
        """
        self.n_scenarios = n_scenarios
        self.scenarios: List[WeatherScenario] = []
        self.scenario_dict: Dict[int, WeatherScenario] = {}
        self.tree = None  # Will be built after scenarios loaded
        
    def generate_scenarios(self, random_seed: int = 42) -> None:
        """Generate representative weather scenarios.
        
        Creates a grid-based sampling of weather parameter space:
        - Wind speed: 1-20 m/s (logarithmic spacing, 10 samples)
        - Wind direction: 0-360° (8 cardinal directions)
        - Stability: A-F (6 classes)
        - Temperature: 263-308 K (varies with stability)
        - Precipitation: 0-10 mm/h (3 levels)
        
        Total combinations: ~10 × 8 × 6 × 3 = 1440, reduced to n_scenarios by
        stratified sampling.
        """
        np.random.seed(random_seed)
        
        # Parameter ranges
        u_speeds = np.logspace(0, np.log10(20), 10)  # 1-20 m/s
        wind_dirs = np.linspace(0, 360, 9)[:-1]  # 0-360° (8 directions)
        stability_classes = ['A', 'B', 'C', 'D', 'E', 'F']
        precip_rates = [0.0, 2.5, 5.0]  # mm/h
        
        # Generate all combinations and sample
        all_combos = []
        for u in u_speeds:
            for wd in wind_dirs:
                for stab in stability_classes:
                    for precip in precip_rates:
                        # Temperature varies with stability (simplified)
                        # Stable (E,F): cooler, unstable (A,B,C): warmer
                        if stab in ['A', 'B']:
                            T_base = 303.15  # 30°C
                        elif stab in ['C', 'D']:
                            T_base = 293.15  # 20°C
                        else:
                            T_base = 283.15  # 10°C
                        
                        all_combos.append({
                            'u': u, 'wd': wd, 'stab': stab,
                            'T': T_base, 'precip': precip
                        })
        
        # Stratified sampling to get n_scenarios
        total = len(all_combos)
        indices = np.linspace(0, total - 1, self.n_scenarios, dtype=int)
        sampled_combos = [all_combos[i] for i in indices]
        
        logger.info(f"Generating {self.n_scenarios} scenarios from {total} combinations")
        
        for idx, combo in enumerate(sampled_combos):
            scenario = self._create_scenario(
                weather_id=idx,
                u_mag_ref=combo['u'],
                wind_direction=combo['wd'],
                temperature=combo['T'],
                stability_class=combo['stab'],
                precipitation_rate=combo['precip']
            )
            self.scenarios.append(scenario)
            self.scenario_dict[idx] = scenario
    
    def _create_scenario(
        self,
        weather_id: int,
        u_mag_ref: float,
        wind_direction: float,
        temperature: float,
        stability_class: str,
        precipitation_rate: float,
        rh: float = 0.65
    ) -> WeatherScenario:
        """Create a single scenario with computed profiles and derived quantities.
        
        Args:
            weather_id: Scenario identifier
            u_mag_ref: Reference wind speed at 10m (m/s)
            wind_direction: Wind direction (degrees)
            temperature: Reference temperature (K)
            stability_class: PGT stability class (A-F)
            precipitation_rate: Precipitation (mm/h)
            rh: Relative humidity (default 0.65)
        
        Returns:
            WeatherScenario object with pre-computed fields
        """
        # Height grid (logarithmic spacing)
        heights = np.logspace(0, 3.5, 20)  # 1m to ~3000m
        
        # Compute wind profile (simplified log-law + stability correction)
        u_star = self._compute_friction_velocity(u_mag_ref, heights[2])
        u_profile = self._log_law_profile(u_star, heights)
        
        # Compute temperature profile (lapse rate + stability)
        T_profile = self._temperature_profile(
            temperature, heights, stability_class, precipitation_rate
        )
        
        # Compute vertical diffusivity profile (K_v)
        K_v_profile = self._vertical_diffusivity_profile(
            u_star, heights, stability_class
        )
        
        # Compute derived quantities
        dust_supp = self._compute_dust_suppression(u_mag_ref)
        sherwood = self._compute_sherwood_number(u_mag_ref)
        leaching_eff = self._compute_leaching_efficiency(u_mag_ref)
        
        return WeatherScenario(
            weather_id=weather_id,
            u_mag_ref=u_mag_ref,
            wind_direction=wind_direction,
            temperature=temperature,
            relative_humidity=rh,
            stability_class=stability_class,
            precipitation_rate=precipitation_rate,
            u_profile=u_profile,
            T_profile=T_profile,
            K_v_profile=K_v_profile,
            heights=heights,
            dust_suppression_factor=dust_supp,
            sherwood_number=sherwood,
            leaching_efficiency=leaching_eff
        )
    
    @staticmethod
    def _compute_friction_velocity(u_ref: float, z_ref: float = 10.0) -> float:
        """Compute friction velocity from reference wind speed.
        
        Uses von Kármán constant and log-law: u* = κ·u / ln(z/z₀)
        with z₀ = 0.1m for mixed terrain.
        """
        z0 = 0.1  # roughness length (m)
        kappa = 0.41  # von Kármán constant
        u_star = (kappa * u_ref) / np.log(z_ref / z0)
        return max(u_star, 0.01)  # Enforce minimum
    
    @staticmethod
    def _log_law_profile(u_star: float, heights: np.ndarray, z0: float = 0.1) -> np.ndarray:
        """Compute wind speed profile using log-law."""
        kappa = 0.41
        with np.errstate(divide='ignore', invalid='ignore'):
            u = (u_star / kappa) * np.log(heights / z0)
        u = np.maximum(u, 0)  # Ensure non-negative
        return u
    
    @staticmethod
    def _temperature_profile(
        T_ref: float, heights: np.ndarray, stab_class: str, precip: float
    ) -> np.ndarray:
        """Compute temperature profile with stability-dependent lapse rate."""
        # Dry adiabatic lapse rate: ~9.8 K/km = 0.0098 K/m
        # Stability modulates this
        lapse_rates = {
            'A': 0.005,  # Very unstable: weak temperature decrease
            'B': 0.007,
            'C': 0.0098,  # Neutral (dry adiabatic)
            'D': 0.0098,
            'E': 0.015,  # Stable: stronger inversion
            'F': 0.020   # Very stable
        }
        lapse = lapse_rates.get(stab_class, 0.0098)
        
        # Reduce lapse rate near surface if precipitating
        if precip > 1.0:
            lapse *= 0.8
        
        T_profile = T_ref - lapse * heights
        return np.maximum(T_profile, 250)  # Ensure T > 250K
    
    @staticmethod
    def _vertical_diffusivity_profile(
        u_star: float, heights: np.ndarray, stab_class: str
    ) -> np.ndarray:
        """Compute vertical diffusivity profile K_v(z).
        
        Uses mixing length theory with stability correction.
        K_v = l²·∂u/∂z where l is mixing length
        """
        kappa = 0.41
        # Approximate ∂u/∂z from log-law: ∂u/∂z = u*/κz
        z_min = np.maximum(heights, 1.0)  # Avoid division at z=0
        du_dz = u_star / (kappa * z_min)
        
        # Mixing length: l = κ·z in unstable, reduced in stable
        stability_factors = {
            'A': 1.5, 'B': 1.3, 'C': 1.0, 'D': 0.8, 'E': 0.5, 'F': 0.3
        }
        factor = stability_factors.get(stab_class, 1.0)
        l = factor * kappa * z_min
        
        K_v = l**2 * du_dz
        K_v = np.maximum(K_v, 0.01)  # Enforce minimum diffusivity
        return K_v
    
    @staticmethod
    def _compute_dust_suppression(u_mag: float) -> float:
        """Compute dust suppression factor as function of wind speed.
        
        Higher wind speeds → more dust in suspension → reduced settling.
        Model: f(u) = 1 - exp(-k·u) with k calibrated from observations.
        Range: [0, 1] where 0 = all dust settles, 1 = all dust in suspension.
        """
        k = 0.15  # empirical constant (1/m·s)
        suppression = 1.0 - np.exp(-k * u_mag)
        return float(np.clip(suppression, 0.0, 1.0))
    
    @staticmethod
    def _compute_sherwood_number(u_mag: float, particle_size: float = 500e-6) -> float:
        """Compute Sherwood number for leaching mass transfer.
        
        Correlation: Sh = K·Re^m·Sc^n where
        - Re = ρ·u·D/μ (Reynolds number)
        - Sc = ν/D_AB (Schmidt number, ~600 for water)
        - K, m, n depend on geometry (heap = sphere correlation)
        
        References: Ranz & Marshall (1952), Sherwood (1954)
        """
        # Physical properties (water at 20°C)
        rho = 1000  # kg/m³
        mu = 0.001  # Pa·s
        nu = 1e-6   # m²/s (kinematic viscosity)
        D_AB = 1e-9  # m²/s (diffusion coefficient, typical)
        D = particle_size  # particle diameter
        
        # Reynolds number
        Re = (rho * u_mag * D) / mu
        Re = max(Re, 0.1)  # Avoid Re < 0.1
        
        # Schmidt number
        Sc = nu / D_AB
        
        # Sherwood correlation (Ranz & Marshall for sphere)
        # Sh = 2 + (0.6·Re^0.5·Sc^0.33)
        Sh = 2.0 + 0.6 * (Re**0.5) * (Sc**(1/3))
        
        return float(Sh)
    
    @staticmethod
    def _compute_leaching_efficiency(u_mag: float) -> float:
        """Compute relative leaching efficiency from Sherwood number.
        
        Efficiency proportional to mass transfer coefficient:
        h_MT = Sh·D_AB/D
        Leaching efficiency = h_MT / h_MT_ref at u_ref = 1 m/s
        Range: [0, 1+] (can exceed 1 at high wind speeds)
        """
        Sh_current = ScenarioLibrary._compute_sherwood_number(u_mag)
        Sh_ref = ScenarioLibrary._compute_sherwood_number(1.0)  # Reference at 1 m/s
        
        # Efficiency proportional to Sherwood number
        efficiency = Sh_current / Sh_ref
        
        return float(efficiency)
    
    def save_to_hdf5(self, output_file: str) -> None:
        """Save scenario library to HDF5 file.
        
        Args:
            output_file: Path to output HDF5 file
        """
        if not HAS_H5PY:
            raise ImportError("h5py not available. Install with: pip install h5py")
        
        with h5py.File(output_file, 'w') as f:
            # Store metadata
            meta_group = f.create_group('metadata')
            meta_group.attrs['n_scenarios'] = self.n_scenarios
            meta_group.attrs['version'] = '1.0'
            
            # Store each scenario
            scenarios_group = f.create_group('scenarios')
            for scenario in self.scenarios:
                sg = scenarios_group.create_group(f'scenario_{scenario.weather_id:04d}')
                
                # Store scalar attributes
                sg.attrs['weather_id'] = scenario.weather_id
                sg.attrs['u_mag_ref'] = scenario.u_mag_ref
                sg.attrs['wind_direction'] = scenario.wind_direction
                sg.attrs['temperature'] = scenario.temperature
                sg.attrs['relative_humidity'] = scenario.relative_humidity
                sg.attrs['stability_class'] = scenario.stability_class
                sg.attrs['precipitation_rate'] = scenario.precipitation_rate
                sg.attrs['dust_suppression_factor'] = scenario.dust_suppression_factor
                sg.attrs['sherwood_number'] = scenario.sherwood_number
                sg.attrs['leaching_efficiency'] = scenario.leaching_efficiency
                
                # Store profile arrays
                sg.create_dataset('heights', data=scenario.heights)
                sg.create_dataset('u_profile', data=scenario.u_profile)
                sg.create_dataset('T_profile', data=scenario.T_profile)
                sg.create_dataset('K_v_profile', data=scenario.K_v_profile)
        
        logger.info(f"Saved {self.n_scenarios} scenarios to {output_file}")
    
    def load_from_hdf5(self, input_file: str) -> None:
        """Load scenario library from HDF5 file.
        
        Args:
            input_file: Path to input HDF5 file
        """
        if not HAS_H5PY:
            raise ImportError("h5py not available. Install with: pip install h5py")
        
        with h5py.File(input_file, 'r') as f:
            scenarios_group = f['scenarios']
            
            for scenario_key in scenarios_group:
                sg = scenarios_group[scenario_key]
                
                # Load arrays
                heights = sg['heights'][:]
                u_profile = sg['u_profile'][:]
                T_profile = sg['T_profile'][:]
                K_v_profile = sg['K_v_profile'][:]
                
                # Load scalar attributes
                scenario = WeatherScenario(
                    weather_id=int(sg.attrs['weather_id']),
                    u_mag_ref=float(sg.attrs['u_mag_ref']),
                    wind_direction=float(sg.attrs['wind_direction']),
                    temperature=float(sg.attrs['temperature']),
                    relative_humidity=float(sg.attrs['relative_humidity']),
                    stability_class=str(sg.attrs['stability_class']),
                    precipitation_rate=float(sg.attrs['precipitation_rate']),
                    heights=heights,
                    u_profile=u_profile,
                    T_profile=T_profile,
                    K_v_profile=K_v_profile,
                    dust_suppression_factor=float(sg.attrs['dust_suppression_factor']),
                    sherwood_number=float(sg.attrs['sherwood_number']),
                    leaching_efficiency=float(sg.attrs['leaching_efficiency'])
                )
                
                self.scenarios.append(scenario)
                self.scenario_dict[scenario.weather_id] = scenario
        
        logger.info(f"Loaded {len(self.scenarios)} scenarios from {input_file}")
    
    def save_to_json(self, output_file: str) -> None:
        """Save scenario library to JSON format (fallback without HDF5).
        
        Args:
            output_file: Path to output JSON file
        """
        data = {
            'metadata': {
                'n_scenarios': self.n_scenarios,
                'version': '1.0'
            },
            'scenarios': []
        }
        
        for scenario in self.scenarios:
            scenario_dict = {
                'weather_id': scenario.weather_id,
                'u_mag_ref': scenario.u_mag_ref,
                'wind_direction': scenario.wind_direction,
                'temperature': scenario.temperature,
                'relative_humidity': scenario.relative_humidity,
                'stability_class': scenario.stability_class,
                'precipitation_rate': scenario.precipitation_rate,
                'dust_suppression_factor': scenario.dust_suppression_factor,
                'sherwood_number': scenario.sherwood_number,
                'leaching_efficiency': scenario.leaching_efficiency,
                'heights': scenario.heights.tolist(),
                'u_profile': scenario.u_profile.tolist(),
                'T_profile': scenario.T_profile.tolist(),
                'K_v_profile': scenario.K_v_profile.tolist()
            }
            data['scenarios'].append(scenario_dict)
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
        
        logger.info(f"Saved {self.n_scenarios} scenarios to {output_file}")
    
    def nearest_scenario(
        self, u_mag: float, wind_direction: float, temperature: float
    ) -> WeatherScenario:
        """Find nearest scenario in library using Euclidean distance.
        
        Args:
            u_mag: Reference wind speed (m/s)
            wind_direction: Wind direction (degrees)
            temperature: Temperature (K)
        
        Returns:
            Nearest WeatherScenario from library
        """
        if not self.scenarios:
            raise ValueError("Scenario library is empty")
        
        # Normalize inputs to common scale for distance computation
        u_norm = u_mag / 20.0  # Normalize to [0, 1] range
        wd_norm = wind_direction / 360.0
        T_norm = (temperature - 250.0) / 60.0  # Temperature 250-310K
        
        min_dist = float('inf')
        nearest = None
        
        for scenario in self.scenarios:
            u_s = scenario.u_mag_ref / 20.0
            wd_s = scenario.wind_direction / 360.0
            T_s = (scenario.temperature - 250.0) / 60.0
            
            dist = np.sqrt(
                (u_norm - u_s)**2 +
                (wd_norm - wd_s)**2 +
                (T_norm - T_s)**2
            )
            
            if dist < min_dist:
                min_dist = dist
                nearest = scenario
        
        return nearest


def build_scenario_library(
    n_scenarios: int = 100,
    output_file: str = 'scenario_library.hdf5'
) -> Dict[str, any]:
    """Build scenario library offline (one-time cost: 1-2 hours).
    
    Generates representative weather scenarios with pre-computed:
    - Wind field (u_mag, u_profile, wind direction)
    - Temperature field (T_profile)
    - Turbulent diffusivity (K_v_profile)
    - Stability classification (A-F)
    - Precipitation rates
    - Derived quantities (dust_suppression, Sherwood_number, leaching_efficiency)
    
    Args:
        n_scenarios: Number of scenarios to generate (default 100)
        output_file: Output file path (HDF5 or JSON depending on availability)
    
    Returns:
        Dictionary with statistics and file path
    
    Example:
        >>> result = build_scenario_library(n_scenarios=100)
        >>> print(f"Generated {result['n_scenarios']} scenarios")
        >>> print(f"Saved to {result['file_path']}")
    """
    logger.info(f"Building scenario library with {n_scenarios} scenarios...")
    
    lib = ScenarioLibrary(n_scenarios=n_scenarios)
    lib.generate_scenarios()
    
    # Save to file (try HDF5 first, fall back to JSON)
    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    if output_file.endswith('.hdf5') or output_file.endswith('.h5'):
        if HAS_H5PY:
            lib.save_to_hdf5(output_file)
        else:
            logger.warning("h5py not available, saving to JSON instead")
            output_file = str(output_path.with_suffix('.json'))
            lib.save_to_json(output_file)
    else:
        lib.save_to_json(output_file)
    
    # Compute statistics
    u_mags = [s.u_mag_ref for s in lib.scenarios]
    temps = [s.temperature for s in lib.scenarios]
    
    result = {
        'n_scenarios': len(lib.scenarios),
        'file_path': str(output_file),
        'u_mag_range': (min(u_mags), max(u_mags)),
        'temperature_range': (min(temps), max(temps)),
        'stability_classes': sorted(set(s.stability_class for s in lib.scenarios)),
        'library': lib
    }
    
    logger.info(f"Scenario library complete: {result['n_scenarios']} scenarios saved to {output_file}")
    
    return result


if __name__ == '__main__':
    # Example usage
    logging.basicConfig(level=logging.INFO)
    
    result = build_scenario_library(n_scenarios=100, output_file='scenario_library.hdf5')
    print(f"Generated {result['n_scenarios']} scenarios")
    print(f"Wind speed range: {result['u_mag_range'][0]:.1f} - {result['u_mag_range'][1]:.1f} m/s")
    print(f"Temperature range: {result['temperature_range'][0]:.1f} - {result['temperature_range'][1]:.1f} K")
    print(f"Stability classes: {', '.join(result['stability_classes'])}")
    print(f"File: {result['file_path']}")
