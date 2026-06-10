# PHREEQC Coupling API Reference

Complete function and class documentation for the PHREEQC reactive transport coupling framework.

## Table of Contents

- [Core Classes](#core-classes)
- [FieldExtractor](#fieldextractor)
- [AMD Hotspot Detection](#amd-hotspot-detection)
- [Sulfide Oxidation](#sulfide-oxidation)
- [Scenario Library](#scenario-library)
- [Spatial Temperature Cache](#spatial-temperature-cache)
- [Dust Suppression](#dust-suppression)
- [Leaching Efficiency](#leaching-efficiency)
- [NetCDF I/O](#netcdf-io)

---

## Core Classes

### AtmosphericField

Data container for complete meteorological state extracted from wind solver.

```python
class AtmosphericField:
    """Container for atmospheric boundary conditions and diagnostics.
    
    Attributes:
        z_agl (array): Height above ground level [m]
        u (array): Wind speed components [m/s]
        v (array): Wind speed components [m/s]
        w (array): Vertical velocity [m/s]
        T (array): Temperature [K]
        K_v (array): Vertical diffusivity [m²/s]
        stability_class (str): PGT stability classification (A-F)
        friction_velocity (float): Friction velocity u* [m/s]
        wind_direction (float): Wind direction [degrees, 0-360]
        precipitation_rate (float): Precipitation rate [mm/hr]
        roughness_length (float): Aerodynamic roughness z₀ [m]
    """
```

**Example:**
```python
from phreeqc_coupling import FieldExtractor
from wind_solver import WindSolver

wind = WindSolver("inputs.i")
wind.solve()
extractor = FieldExtractor(wind)
fields = extractor.extract_all_fields()

print(f"Friction velocity: {fields.friction_velocity:.3f} m/s")
print(f"Stability class: {fields.stability_class}")
```

---

### FieldExtractor

High-level interface for extracting atmospheric boundary conditions from wind solver.

```python
class FieldExtractor:
    """Extract and process atmospheric fields from wind solver output.
    
    Parameters:
        wind_solver: Initialized and solved WindSolver instance
        verbose (bool): Enable diagnostic output (default: False)
    
    Methods:
    """
```

#### extract_all_fields()

```python
def extract_all_fields() -> AtmosphericField:
    """Extract complete meteorological state from wind solver.
    
    Returns:
        AtmosphericField: Container with all boundary conditions
    
    Raises:
        RuntimeError: If wind solver not solved or incomplete
    """
```

**Example:**
```python
extractor = FieldExtractor(wind)
fields = extractor.extract_all_fields()
```

---

#### export_velocity_magnitude(z_level)

```python
def export_velocity_magnitude(z_level: float) -> float:
    """Export wind speed magnitude at specific height.
    
    Parameters:
        z_level (float): Height above ground level [m]
    
    Returns:
        float: Wind speed magnitude [m/s]
    
    Physics:
        Interpolates from 3D velocity field at height z_level
    """
```

**Example:**
```python
u_10m = extractor.export_velocity_magnitude(z_level=10.0)
print(f"Wind speed at 10 m: {u_10m:.2f} m/s")
```

---

#### export_temperature_profile()

```python
def export_temperature_profile() -> tuple:
    """Export vertical temperature profile.
    
    Returns:
        tuple: (z_agl, T_profile) where
            z_agl (array): Heights [m]
            T_profile (array): Temperatures [K]
    
    Physics:
        Temperature from wind solver with moist adiabatic lapse rate
        correction for stable/unstable atmospheres
    """
```

**Example:**
```python
z_agl, T = extractor.export_temperature_profile()
for z, T_k in zip(z_agl, T):
    print(f"z={z:.1f} m: T = {T_k-273.15:.2f} °C")
```

---

#### export_vertical_diffusivity()

```python
def export_vertical_diffusivity() -> tuple:
    """Export vertical turbulent diffusivity profile.
    
    Returns:
        tuple: (z_agl, K_v) where
            z_agl (array): Heights [m]
            K_v (array): Vertical diffusivity [m²/s]
    
    Physics:
        K_v = u* × z × f(ζ) where ζ = z/L (Monin-Obukhov)
    """
```

**Example:**
```python
z_agl, K_v = extractor.export_vertical_diffusivity()
alpha = K_v / u_mag  # Dispersivity
```

---

#### export_precipitation_rate()

```python
def export_precipitation_rate() -> float:
    """Export spatial precipitation rate.
    
    Returns:
        float: Precipitation rate [mm/hr]
    
    Returns zero if no precipitation field available.
    """
```

---

#### export_stability_classification()

```python
def export_stability_classification() -> str:
    """Export Pasquill-Gifford-Turner stability class.
    
    Returns:
        str: Stability class 'A' (very unstable) through 'F' (very stable)
    
    Physics:
        Decision-tree based on wind speed, solar radiation, cloud cover
        (Turner 1994 implementation)
    """
```

**Stability classes:**
- **A**: Very unstable (maximum dispersion)
- **B**: Unstable
- **C**: Neutral (average conditions)
- **D**: Stable (neutral baseline)
- **E**: Very stable
- **F**: Extremely stable (minimum dispersion)

---

#### export_oxygen_delivery_rate()

```python
def export_oxygen_delivery_rate() -> float:
    """Export wind-dependent oxygen delivery factor.
    
    Returns:
        float: O₂ delivery enhancement factor (dimensionless, ~0.75 exponent)
    
    Physics:
        f(u) = (u/u_ref)^0.75 where u_ref = 5.0 m/s
    """
```

---

## AMD Hotspot Detection

### identify_valley_amd_hotspots()

High-level API for AMD hotspot detection and classification.

```python
def identify_valley_amd_hotspots(
    wind_solver,
    amd_locations_file: str,
    output_dir: str = ".",
    verbose: bool = True,
    risk_thresholds: dict = None
) -> dict:
    """Identify and classify acid mine drainage hotspots.
    
    Parameters:
        wind_solver: Solved WindSolver instance
        amd_locations_file (str): CSV with columns: id, x, y, z, discharge_type, description
        output_dir (str): Directory for GeoJSON and CSV outputs
        verbose (bool): Print diagnostics to console
        risk_thresholds (dict): Override default thresholds
            keys: 'high', 'medium' (µmol/(m²·s))
            default: {'medium': 30, 'high': 100}
    
    Returns:
        dict: Results including:
            'high_risk_count' (int): Number of HIGH risk hotspots
            'medium_risk_count' (int): Number of MEDIUM risk hotspots
            'low_risk_count' (int): Number of LOW risk hotspots
            'output_files' (list): Generated files
            'detector' (AMDHotspotDetector): Detector instance for further analysis
    
    Raises:
        FileNotFoundError: If amd_locations_file not found
        ValueError: If CSV format incorrect
    
    References:
        Sherwood (1954). Mass transfer between phases.
        Businger et al. (1971). Flux-profile relationships in atmospheric surface layer.
    """
```

**Example:**
```python
results = identify_valley_amd_hotspots(
    wind,
    'amd_sites.csv',
    output_dir='hotspot_analysis/',
    verbose=True
)

print(f"HIGH risk: {results['high_risk_count']}")
print(f"Output: {results['output_files']}")
```

---

### AMDHotspotDetector

Low-level class for detailed hotspot analysis.

```python
class AMDHotspotDetector:
    """Detect and classify AMD hotspots using wind-driven oxygen delivery.
    
    Parameters:
        wind_solver: Solved WindSolver instance
        risk_thresholds (dict): LOW/MEDIUM/HIGH boundary values [µmol/(m²·s)]
    
    Methods:
    """
```

#### compute_oxygen_supply_rate(amd_location)

```python
def compute_oxygen_supply_rate(amd_location: AMDLocation) -> float:
    """Compute oxygen supply rate at discharge point.
    
    Parameters:
        amd_location (AMDLocation): Point with x, y, z coordinates
    
    Returns:
        float: O₂ supply rate [µmol/(m²·s)]
    
    Physics:
        r_O₂ = k_c × [O₂]_sat where k_c from Sherwood correlation
    """
```

---

### AMDLocation

Data container for AMD discharge point.

```python
@dataclass
class AMDLocation:
    """AMD discharge point with location and type information.
    
    Attributes:
        id (str): Unique identifier
        x (float): Easting coordinate [m]
        y (float): Northing coordinate [m]
        z (float): Elevation [m asl]
        discharge_type (str): 'seep', 'spring', 'groundwater', 'runoff'
        description (str): Human-readable description
    """
```

---

### HotspotRiskInfo

Results container for single hotspot.

```python
@dataclass
class HotspotRiskInfo:
    """Classification and diagnostics for single hotspot.
    
    Attributes:
        amd_location (AMDLocation): Original location data
        risk_class (str): 'HIGH', 'MEDIUM', 'LOW'
        O2_supply_rate (float): [µmol/(m²·s)]
        friction_velocity (float): u* [m/s]
        wind_speed (float): [m/s]
        wind_shear (float): ∂u/∂z [s⁻¹]
        K_v (float): Vertical diffusivity [m²/s]
        stability_class (str): PGT A-F
        timestamp (str): ISO 8601 timestamp
    """
```

---

## Sulfide Oxidation

### compute_sulfide_oxidation_rates()

High-level API for sulfide oxidation kinetics.

```python
def compute_sulfide_oxidation_rates(
    wind_solver,
    sulfide_locations_file: str,
    temperature: float = 288.15,
    o2_concentration: float = 280.0,
    output_dir: str = ".",
    verbose: bool = True
) -> dict:
    """Compute wind-dependent sulfide oxidation rates.
    
    Parameters:
        wind_solver: Solved WindSolver instance
        sulfide_locations_file (str): CSV with columns: id, x, y, z, mineral_type, 
                                      mass_fraction, specific_surface_area, description
        temperature (float): Temperature [K] (default 288.15 K = 15°C)
        o2_concentration (float): Dissolved O₂ [µmol/L] (default: saturation at 25°C)
        output_dir (str): Directory for outputs
        verbose (bool): Print diagnostics
    
    Returns:
        dict: Results including:
            'mean_oxidation_rate' (float): [mol/(m³·s)]
            'max_oxidation_rate' (float): [mol/(m³·s)]
            'mean_acid_rate' (float): [mol H⁺/(m³·s)]
            'mean_pH_change_rate' (float): [pH units/day]
            'output_files' (list): Generated files
    
    References:
        Nicholson et al. (1990). Pyrite oxidation in carbonate-buffered systems.
    """
```

**Example:**
```python
results = compute_sulfide_oxidation_rates(
    wind,
    'pyrite_deposits.csv',
    temperature=288.15,
    output_dir='oxidation_rates/'
)

print(f"Max oxidation rate: {results['max_oxidation_rate']:.2e} mol/(m³·s)")
print(f"Mean pH change: {results['mean_pH_change_rate']:.2f} pH/day")
```

---

### SulfideOxidationComputer

Low-level class for detailed oxidation calculations.

```python
class SulfideOxidationComputer:
    """Compute wind-dependent sulfide oxidation kinetics.
    
    Parameters:
        wind_solver: Solved WindSolver instance
    
    Methods:
    """
```

#### wind_to_oxygen_delivery(u_speed, roughness)

```python
@staticmethod
def wind_to_oxygen_delivery(u_speed: float, roughness: float = 0.01) -> float:
    """Compute wind-dependent O₂ delivery enhancement factor.
    
    Parameters:
        u_speed (float): Wind speed [m/s]
        roughness (float): Aerodynamic roughness z₀ [m]
    
    Returns:
        float: Enhancement factor (dimensionless)
    
    Physics:
        f(u) = (u/u_ref)^n where u_ref = 5.0, n = 0.75
    """
```

---

#### pyrite_oxidation_kinetics()

```python
def pyrite_oxidation_kinetics(
    o2_concentration: float,
    temperature: float,
    wind_factor: float,
    specific_surface_area: float = 1000.0
) -> float:
    """Compute oxidation rate at single location.
    
    Parameters:
        o2_concentration (float): [µmol/L]
        temperature (float): [K]
        wind_factor (float): Enhancement factor from wind_to_oxygen_delivery()
        specific_surface_area (float): [cm²/g] (default: 1000)
    
    Returns:
        float: Oxidation rate [mol/(m³·s)]
    
    Physics:
        k(T) = A × exp(-E_a/(R×T))
        r_ox = k(T) × [FeS₂] × [O₂] × f(u)
        E_a = 45 kJ/mol, A = 1.0e-8 mol/(m²·s)
    """
```

---

### OxidationRateInfo

Results container for single sulfide location.

```python
@dataclass
class OxidationRateInfo:
    """Oxidation rate and diagnostics at single location.
    
    Attributes:
        mineral_type (SulfideMineralType): 'PYRITE', 'CHALCOPYRITE', etc.
        oxidation_rate (float): [mol/(m³·s)]
        acid_generation_rate (float): [mol H⁺/(m³·s)]
        o2_delivery_factor (float): Wind enhancement factor
        temperature (float): [K]
        wind_speed (float): [m/s]
        pH_change_rate (float): [pH units/day]
    """
```

---

## Scenario Library

### build_scenario_library()

Generate offline pre-computed scenario library.

```python
def build_scenario_library(
    n_scenarios: int = 100,
    output_dir: str = ".",
    wind_speeds: array = None,
    wind_directions: array = None,
    temperatures: array = None,
    parallel: bool = True,
    n_jobs: int = -1
) -> ScenarioLibrary:
    """Build representative weather scenario library for fast runtime lookups.
    
    Parameters:
        n_scenarios (int): Number of scenarios to generate (default: 100)
        output_dir (str): Output directory for library files
        wind_speeds (array): Custom wind speeds [m/s] (default: 0-20 m/s)
        wind_directions (array): Custom wind directions [degrees] (default: 8 sectors)
        temperatures (array): Custom temperatures [K] (default: 250-310 K)
        parallel (bool): Use parallelization (default: True)
        n_jobs (int): Number of parallel jobs (-1 = all cores)
    
    Returns:
        ScenarioLibrary: Generated library with fields cached
    
    Computation time:
        Single-threaded: ~1–2 hours for 100 scenarios
        Parallel (8 cores): ~20–30 minutes
    
    Storage:
        HDF5 format: ~200–500 MB for 100 scenarios
    """
```

**Example:**
```python
# One-time offline computation
lib = build_scenario_library(
    n_scenarios=100,
    output_dir='scenario_cache/',
    parallel=True,
    n_jobs=8
)
print(f"Library saved: {lib.output_file}")
```

---

### ScenarioLibrary

Container for pre-computed scenarios.

```python
class ScenarioLibrary:
    """Pre-computed weather scenario library for fast lookups.
    
    Methods:
    """
```

#### nearest_scenario()

```python
def nearest_scenario(
    u_mag: float,
    wind_dir: float,
    T: float,
    n_neighbors: int = 1
) -> WeatherScenario:
    """Find nearest scenario in library using KD-tree nearest neighbor.
    
    Parameters:
        u_mag (float): Wind speed [m/s]
        wind_dir (float): Wind direction [degrees, 0–360]
        T (float): Temperature [K]
        n_neighbors (int): Return n nearest scenarios
    
    Returns:
        WeatherScenario: Nearest scenario with all cached fields
    
    Runtime: <30 ms for 100 scenarios
    """
```

---

#### load()

```python
@staticmethod
def load(filepath: str) -> ScenarioLibrary:
    """Load pre-computed library from file.
    
    Parameters:
        filepath (str): Path to library file (HDF5 or JSON)
    
    Returns:
        ScenarioLibrary: Loaded library
    
    Runtime: <100 ms
    """
```

---

### WeatherScenario

Pre-computed scenario fields.

```python
@dataclass
class WeatherScenario:
    """Single pre-computed weather scenario.
    
    Attributes:
        u_mag (float): Wind speed [m/s]
        wind_dir (float): Wind direction [degrees]
        T (float): Temperature [K]
        K_v_profile (array): Vertical diffusivity [m²/s]
        u_star (float): Friction velocity [m/s]
        stability_class (str): PGT A-F
        dust_suppression_factor (float): [0–1]
        leaching_efficiency (float): Enhancement factor
        sherwood_numbers (array): Pre-computed Sh values
    """
```

---

## Spatial Temperature Cache

### SpatialTemperatureCache

Extract spatially-varying temperature fields.

```python
class SpatialTemperatureCache:
    """Extract localized temperature profiles from scenario library.
    
    Parameters:
        scenario_library (ScenarioLibrary): Loaded library
    
    Methods:
    """
```

#### export_phreeqc_boundary_conditions()

```python
def export_phreeqc_boundary_conditions(
    x: float,
    y: float,
    elevation_correction: bool = True,
    lapse_rate: float = 0.0065
) -> tuple:
    """Extract 1D temperature profile at specific location.
    
    Parameters:
        x (float): Easting coordinate [m]
        y (float): Northing coordinate [m]
        elevation_correction (bool): Apply elevation lapse rate correction
        lapse_rate (float): Temperature lapse rate [K/m] (default: 0.0065)
    
    Returns:
        tuple: (z_agl, T_profile) for PHREEQC input
    
    Physics:
        T(z, x, y) = T_ref - lapse_rate × z_agl
    """
```

---

## Dust Suppression

### compute_dust_suppression_factor()

```python
def compute_dust_suppression_factor(
    u_speed: float,
    particle_size: float
) -> float:
    """Compute wind-dependent dust suppression factor.
    
    Parameters:
        u_speed (float): Wind speed [m/s]
        particle_size (float): Particle diameter [µm]
    
    Returns:
        float: Suppression factor [0–1] where
            0 = complete settling (low wind)
            1 = complete suspension (high wind)
    
    Physics:
        Based on Stokes settling velocity vs. turbulent mixing
    """
```

---

### compute_dust_suppression_effect_on_ph()

```python
def compute_dust_suppression_effect_on_ph(
    u_speed: float,
    reference_pH: float,
    particle_size: float = 10.0
) -> float:
    """Adjust pH based on wind-dependent dust settling.
    
    Parameters:
        u_speed (float): Wind speed [m/s]
        reference_pH (float): Baseline pH
        particle_size (float): Particle diameter [µm]
    
    Returns:
        float: Adjusted pH
    
    Mechanism:
        High wind → dust in suspension → less pH acidification
        Low wind → dust settling → additional acidification
    """
```

---

## Leaching Efficiency

### compute_leaching_efficiency()

```python
def compute_leaching_efficiency(
    u_speed: float,
    particle_size: float
) -> float:
    """Compute wind-driven leaching efficiency via Sherwood correlation.
    
    Parameters:
        u_speed (float): Wind speed [m/s]
        particle_size (float): Ore particle diameter [µm]
    
    Returns:
        float: Efficiency enhancement factor [1–10 range typical]
    
    Physics:
        Sh = 2 + 0.6 × Re^0.5 × Sc^0.33 (Ranz & Marshall 1952)
        Efficiency ∝ Sh × diffusivity
    
    References:
        Sherwood (1954). Mass transfer between phases.
        Ranz & Marshall (1952). Evaporation from drops.
    """
```

---

### compute_leaching_rate_enhancement()

```python
def compute_leaching_rate_enhancement(
    u_speed: float,
    baseline_rate: float,
    particle_size: float = 500.0
) -> float:
    """Apply Sherwood-based enhancement to dissolution rate.
    
    Parameters:
        u_speed (float): Wind speed [m/s]
        baseline_rate (float): Dissolution rate without wind [mol/(m²·s)]
        particle_size (float): Particle diameter [µm]
    
    Returns:
        float: Enhanced dissolution rate [mol/(m²·s)]
    """
```

---

## NetCDF I/O

### NetCDFHandler

CF-compliant NetCDF serialization.

```python
class NetCDFHandler:
    """Export atmospheric fields to NetCDF4 with CF conventions.
    
    Methods:
    """
```

#### export_to_netcdf()

```python
def export_to_netcdf(
    filename: str,
    fields: AtmosphericField,
    compression: int = 4
) -> str:
    """Export fields to NetCDF4 file.
    
    Parameters:
        filename (str): Output filename
        fields (AtmosphericField): Fields to export
        compression (int): NetCDF compression level (0–9)
    
    Returns:
        str: Output filename
    
    Format:
        CF-1.9 compliant with standard dimension names and metadata
    """
```

---

### ASCIIExporter

Plain text export for PHREEQC input.

```python
class ASCIIExporter:
    """Export atmospheric fields to ASCII text for PHREEQC input.
    
    Methods:
    """
```

#### export_temperature_profile()

```python
def export_temperature_profile(
    filename: str,
    z_agl: array,
    T_profile: array,
    format_str: str = "# z (m)  T (K)  T (°C)\n"
) -> str:
    """Export temperature profile to ASCII file.
    
    Parameters:
        filename (str): Output filename
        z_agl (array): Heights [m]
        T_profile (array): Temperatures [K]
        format_str (str): Header format string
    
    Returns:
        str: Output filename
    """
```

---

## Error Handling

Common exceptions:

```python
# File not found
FileNotFoundError: "AMD locations CSV not found: {filename}"

# Invalid CSV format
ValueError: "CSV must contain columns: id, x, y, z, discharge_type, description"

# Wind solver not solved
RuntimeError: "Wind solver must be solved before field extraction"

# Missing library
ImportError: "netcdf4 not installed. ASCII export only. Install: pip install netcdf4"

# Scenario library query with no results
ValueError: "No scenarios found within tolerance. Increase n_neighbors."
```

---

## Physical Constants Used

| Constant | Value | Units | Source |
|----------|-------|-------|--------|
| Karman constant | 0.41 | dimensionless | Businger et al. (1971) |
| Gas constant | 8.314 | J/(mol·K) | Physical constant |
| Activation energy (pyrite) | 45,000 | J/mol | Nicholson et al. (1990) |
| Preexponential factor | 1.0e-8 | mol/(m²·s) | Calibrated |
| Sherwood prefactor | 0.332 | dimensionless | Ranz & Marshall (1952) |
| Oxidation wind exponent | 0.75 | dimensionless | Turbulence theory |
| O₂ saturation (25°C) | 280 | µmol/L | Henry's Law |

---

## References

1. **Businger, J.A., et al.** (1971). Flux-profile relationships in the atmospheric surface layer. *Journal of Atmospheric Sciences*, 28(2), 181–189.

2. **Nicholson, R.V., et al.** (1990). Pyrite oxidation in carbonate-buffered systems. *Geochimica et Cosmochimica Acta*, 54(2), 395–405.

3. **Sherwood, T.K.** (1954). Mass transfer between phases. *Industrial & Engineering Chemistry*, 46(2), 221–231.

4. **Ranz, W.E., & Marshall, W.R.** (1952). Evaporation from drops. *Chemical Engineering Progress*, 48(3), 141–146.

5. **Parkhurst, D.L., & Appelo, C.A.J.** (2013). PHREEQC (Version 3). *USGS Techniques and Methods*, Book 6, Chapter A43.

---

**Last Updated:** 2026-06-10  
**massconsistent_amr PHREEQC Coupling v1.0.0**
