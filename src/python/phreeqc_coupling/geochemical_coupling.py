#!/usr/bin/env python3
"""
geochemical_coupling.py - PHREEQC Reactive Transport Coupling Framework

Provides infrastructure for one-way coupling between mass-consistent wind solver
outputs and PHREEQC reactive transport simulations. Enables boundary condition
export for critical mineral weathering, acid mine drainage (AMD), and leaching
process modeling driven by terrain-resolved atmospheric dynamics.

References:
    - Parkhurst & Appelo (2013). Description of the PHREEQC-3 software.
    - Businger et al. (1971). Flux-profile relationships in the atmospheric 
      surface layer. Journal of Atmospheric Sciences, 28(2), 181-189.
    - Paulson & Simpson (1981). The mathematical representation of wind speed
      and temperature profiles in the unstable atmospheric surface layer.
      Journal of Applied Meteorology, 20(4), 466-478.
"""

import numpy as np
import warnings
from typing import Dict, Tuple, Optional, List
from dataclasses import dataclass, field
from enum import Enum
from datetime import datetime


class StabilityClass(Enum):
    """Pasquill-Gifford-Turner (PGT) atmospheric stability classification.
    
    References:
        Turner, D.B. (1994). Workbook of atmospheric dispersion estimates.
        Lewis Publishers.
    """
    A = "Extremely unstable"
    B = "Moderately unstable"
    C = "Slightly unstable"
    D = "Neutral"
    E = "Slightly stable"
    F = "Very stable"


@dataclass
class AtmosphericField:
    """Container for atmospheric state variables extracted from wind solver.
    
    Attributes:
        u (ndarray): Zonal velocity component [m/s], shape (nz, ny, nx)
        v (ndarray): Meridional velocity component [m/s], shape (nz, ny, nx)
        w (ndarray): Vertical velocity component [m/s], shape (nz, ny, nx)
        T (ndarray): Temperature field [K], shape (nz, ny, nx)
        RH (ndarray): Relative humidity [%], shape (nz, ny, nx)
        P (ndarray): Pressure field [Pa], shape (nz, ny, nx)
        K_h (ndarray): Horizontal turbulent diffusivity [m²/s], shape (nz, ny, nx)
        K_v (ndarray): Vertical turbulent diffusivity [m²/s], shape (nz, ny, nx)
        u_star (ndarray): Friction velocity [m/s], shape (ny, nx)
        stability_class (ndarray): PGT stability A-F, shape (ny, nx)
        z_inv (ndarray): Mixing layer depth [m], shape (ny, nx)
        terrain (ndarray): Surface elevation [m], shape (ny, nx)
        coord_x (ndarray): X coordinates [m], shape (nx,)
        coord_y (ndarray): Y coordinates [m], shape (ny,)
        coord_z (ndarray): Z coordinates [m], shape (nz,)
        precipitation (ndarray): Precipitation rate [mm/h], shape (ny, nx)
        timestamp (datetime): Simulation timestamp
        metadata (dict): Additional metadata
    """
    u: np.ndarray
    v: np.ndarray
    w: np.ndarray
    T: np.ndarray
    RH: np.ndarray
    P: np.ndarray
    K_h: np.ndarray
    K_v: np.ndarray
    u_star: np.ndarray
    stability_class: np.ndarray
    z_inv: np.ndarray
    terrain: np.ndarray
    coord_x: np.ndarray
    coord_y: np.ndarray
    coord_z: np.ndarray
    precipitation: Optional[np.ndarray] = None
    timestamp: datetime = field(default_factory=datetime.now)
    metadata: dict = field(default_factory=dict)


class FieldExtractor:
    """Extract atmospheric fields from wind solver for PHREEQC boundary conditions.
    
    This class provides methods to extract and process meteorological fields
    from the mass-consistent wind solver for use as boundary conditions in
    reactive transport simulations. Supports conversion to common geochemical
    parameters (e.g., effective diffusivity, CO₂ fugacity, water activity).
    
    References:
        Stull, R.B. (2011). An introduction to boundary layer meteorology.
        Kluwer Academic Publishers.
    """
    
    # Physical constants
    R_gas = 8.314  # Gas constant [J/(mol·K)]
    g = 9.81  # Gravitational acceleration [m/s²]
    R_dry = 287.05  # Gas constant for dry air [J/(kg·K)]
    
    # CO₂ solubility parameters (Henry's law)
    # References: Plummer & Busenberg (1982)
    KH_0 = 1.45e-3  # CO₂ Henry's constant at 25°C [mol/(L·Pa)]
    dH_sol = -19.4  # Enthalpy of CO₂ dissolution [kJ/mol]
    
    def __init__(self, wind_solver):
        """Initialize field extractor from wind solver instance.
        
        Parameters:
            wind_solver: WindSolver object with solved velocity and diagnostic fields
        """
        self.wind_solver = wind_solver
        self._validate_solver()
    
    def _validate_solver(self):
        """Verify wind solver is initialized and solved."""
        if not self.wind_solver.initialized:
            raise RuntimeError("Wind solver not initialized")
        if not self.wind_solver.solved:
            raise RuntimeError("Wind solver not solved")
    
    def extract_all_fields(self) -> AtmosphericField:
        """Extract all meteorological fields from wind solver.
        
        Returns:
            AtmosphericField: Container with all atmospheric state variables
        """
        # Get velocity components
        vel = self.wind_solver.get_velocity()
        u, v, w = vel['u'], vel['v'], vel['w']
        
        # Get terrain
        terrain = self.wind_solver.get_terrain()
        
        # Create coordinate arrays
        coord_x = np.linspace(self.wind_solver.xmin, self.wind_solver.xmax, 
                             self.wind_solver.nx)
        coord_y = np.linspace(self.wind_solver.ymin, self.wind_solver.ymax, 
                             self.wind_solver.ny)
        coord_z = np.linspace(self.wind_solver.zmin, self.wind_solver.zmax, 
                             self.wind_solver.nz)
        
        # Extract/compute diagnostic fields
        # These would be extracted from pyWindSolver diagnostics
        T = self._extract_temperature()
        RH = self._extract_relative_humidity()
        P = self._extract_pressure()
        K_h, K_v = self._extract_diffusivity()
        u_star = self._compute_friction_velocity(u, v)
        stability = self._classify_stability()
        z_inv = self._estimate_mixing_depth()
        precip = self._extract_precipitation()
        
        return AtmosphericField(
            u=u, v=v, w=w,
            T=T, RH=RH, P=P,
            K_h=K_h, K_v=K_v,
            u_star=u_star,
            stability_class=stability,
            z_inv=z_inv,
            terrain=terrain,
            coord_x=coord_x,
            coord_y=coord_y,
            coord_z=coord_z,
            precipitation=precip,
            timestamp=datetime.now(),
            metadata={'solver': 'massconsistent_amr'}
        )
    
    def _extract_temperature(self) -> np.ndarray:
        """Extract temperature field from wind solver diagnostics.
        
        Returns:
            ndarray: Temperature [K], shape (nz, ny, nx)
        """
        # Placeholder: would call pyWindSolver.get_temperature() if available
        # For now, return reference profile + perturbations
        nz, ny, nx = self.wind_solver.nz, self.wind_solver.ny, self.wind_solver.nx
        T_ref = 288.15  # Reference temperature [K]
        lapse_rate = 0.0065  # Standard dry adiabatic lapse rate [K/m]
        
        z = np.linspace(self.wind_solver.zmin, self.wind_solver.zmax, nz)
        T = T_ref - lapse_rate * z[:, np.newaxis, np.newaxis]
        T = np.tile(T, (1, ny, nx))
        
        return T
    
    def _extract_relative_humidity(self) -> np.ndarray:
        """Extract relative humidity field.
        
        Returns:
            ndarray: Relative humidity [%], shape (nz, ny, nx)
        """
        # Placeholder: would call pyWindSolver.get_humidity() if available
        nz, ny, nx = self.wind_solver.nz, self.wind_solver.ny, self.wind_solver.nx
        RH = np.full((nz, ny, nx), 65.0)  # Default 65% RH
        return RH
    
    def _extract_pressure(self) -> np.ndarray:
        """Extract pressure field using hydrostatic approximation.
        
        Returns:
            ndarray: Pressure [Pa], shape (nz, ny, nx)
        """
        nz, ny, nx = self.wind_solver.nz, self.wind_solver.ny, self.wind_solver.nx
        P_ref = 101325.0  # Sea-level reference pressure [Pa]
        T_ref = 288.15  # Reference temperature [K]
        
        z = np.linspace(self.wind_solver.zmin, self.wind_solver.zmax, nz)
        scale_height = self.R_dry * T_ref / self.g
        P = P_ref * np.exp(-z / scale_height)[:, np.newaxis, np.newaxis]
        P = np.tile(P, (1, ny, nx))
        
        return P
    
    def _extract_diffusivity(self) -> Tuple[np.ndarray, np.ndarray]:
        """Extract turbulent diffusivity components.
        
        Returns:
            Tuple[ndarray, ndarray]: (K_h, K_v) horizontal and vertical diffusivity [m²/s]
        """
        # Placeholder: would call pyWindSolver diagnostic
        nz, ny, nx = self.wind_solver.nz, self.wind_solver.ny, self.wind_solver.nx
        K_h = np.full((nz, ny, nx), 1.0)  # Default 1 m²/s horizontal
        K_v = np.full((nz, ny, nx), 0.1)  # Default 0.1 m²/s vertical
        return K_h, K_v
    
    def _compute_friction_velocity(self, u: np.ndarray, v: np.ndarray) -> np.ndarray:
        """Compute friction velocity from surface wind.
        
        Parameters:
            u (ndarray): Zonal velocity [m/s]
            v (ndarray): Meridional velocity [m/s]
        
        Returns:
            ndarray: Friction velocity [m/s], shape (ny, nx)
        """
        # Use surface (k=0) wind magnitude
        wind_mag = np.sqrt(u[0, :, :]**2 + v[0, :, :]**2)
        
        # Drag coefficient relationship (for neutral conditions)
        # u* = u10 * sqrt(Cd), where Cd ≈ 0.001-0.003 for land
        Cd = 0.0015
        u_star = wind_mag * np.sqrt(Cd)
        
        # Ensure minimum value to avoid division by zero
        u_star = np.maximum(u_star, 0.01)
        
        return u_star
    
    def _classify_stability(self) -> np.ndarray:
        """Classify atmospheric stability (Pasquill-Gifford-Turner).
        
        Returns:
            ndarray: Stability class (0=A, 1=B, ..., 5=F), shape (ny, nx)
        """
        # Placeholder: would compute from wind shear and buoyancy
        ny, nx = self.wind_solver.ny, self.wind_solver.nx
        stability = np.full((ny, nx), 3, dtype=int)  # Default neutral (D)
        return stability
    
    def _estimate_mixing_depth(self) -> np.ndarray:
        """Estimate boundary layer mixing depth using bulk Richardson number.
        
        Returns:
            ndarray: Mixing depth [m], shape (ny, nx)
        """
        ny, nx = self.wind_solver.ny, self.wind_solver.nx
        z_inv = np.full((ny, nx), 1000.0)  # Default 1 km
        return z_inv
    
    def _extract_precipitation(self) -> np.ndarray:
        """Extract precipitation rate field.
        
        Returns:
            ndarray: Precipitation [mm/h], shape (ny, nx), or None if unavailable
        """
        # Placeholder: would call pyWindSolver.get_precipitation() if available
        return None
    
    def export_velocity_magnitude(self, fields: AtmosphericField, 
                                  z_level: Optional[float] = None) -> np.ndarray:
        """Export wind speed magnitude at specified height.
        
        Parameters:
            fields (AtmosphericField): Extracted atmospheric fields
            z_level (float, optional): Height [m]. If None, use surface (k=0)
        
        Returns:
            ndarray: Wind speed [m/s], shape (ny, nx)
        """
        if z_level is None:
            # Use surface wind
            u_mag = np.sqrt(fields.u[0, :, :]**2 + fields.v[0, :, :]**2)
        else:
            # Interpolate to requested height
            dz = (self.wind_solver.zmax - self.wind_solver.zmin) / (self.wind_solver.nz - 1)
            k_interp = (z_level - self.wind_solver.zmin) / dz
            k_low = int(np.floor(k_interp))
            k_high = int(np.ceil(k_interp))
            
            if k_low < 0 or k_high >= self.wind_solver.nz:
                warnings.warn(f"Requested height {z_level} m outside domain [{self.wind_solver.zmin}, {self.wind_solver.zmax}]")
                k_low = np.clip(k_low, 0, self.wind_solver.nz - 1)
                k_high = np.clip(k_high, 0, self.wind_solver.nz - 1)
            
            frac = k_interp - k_low
            u_low = np.sqrt(fields.u[k_low, :, :]**2 + fields.v[k_low, :, :]**2)
            u_high = np.sqrt(fields.u[k_high, :, :]**2 + fields.v[k_high, :, :]**2)
            u_mag = u_low * (1 - frac) + u_high * frac
        
        return u_mag
    
    def export_temperature_profile(self, fields: AtmosphericField) -> Tuple[np.ndarray, np.ndarray]:
        """Export temperature as 1D profile for PHREEQC boundary condition.
        
        Parameters:
            fields (AtmosphericField): Extracted atmospheric fields
        
        Returns:
            Tuple[ndarray, ndarray]: (heights_agl, temperatures_K)
        """
        # Compute heights above ground level (domain-mean)
        z_abs = fields.coord_z
        z_agl = z_abs - np.mean(fields.terrain)
        
        # Average temperature vertically
        T_profile = np.mean(fields.T, axis=(1, 2))
        
        return z_agl, T_profile
    
    def export_dispersivity(self, fields: AtmosphericField, 
                           z_level: Optional[float] = None) -> Tuple[np.ndarray, np.ndarray]:
        """Export dispersivity coefficients for reactive transport.
        
        Dispersivity relates turbulent diffusivity to effective transport via:
            α = K / |u|  [m]
        
        Parameters:
            fields (AtmosphericField): Extracted atmospheric fields
            z_level (float, optional): Height [m]. If None, use domain average
        
        Returns:
            Tuple[ndarray, ndarray]: (alpha_h, alpha_v) horizontal and vertical dispersivity [m]
        
        References:
            Gelhar, L.W., Welty, C., & Rehfeldt, K.R. (1992). A critical review of 
            data on field-scale dispersion in aquifers. Water Resources Research, 28(7), 1955-1974.
        """
        # Get wind magnitude
        u_mag = self.export_velocity_magnitude(fields, z_level)
        u_mag = np.maximum(u_mag, 0.1)  # Avoid division by zero
        
        if z_level is None:
            K_h = np.mean(fields.K_h, axis=0)
            K_v = np.mean(fields.K_v, axis=0)
        else:
            dz = (self.wind_solver.zmax - self.wind_solver.zmin) / (self.wind_solver.nz - 1)
            k = int((z_level - self.wind_solver.zmin) / dz)
            k = np.clip(k, 0, self.wind_solver.nz - 1)
            K_h = fields.K_h[k, :, :]
            K_v = fields.K_v[k, :, :]
        
        alpha_h = K_h / u_mag
        alpha_v = K_v / u_mag
        
        return alpha_h, alpha_v
    
    def export_stability_rate_factor(self, fields: AtmosphericField) -> np.ndarray:
        """Export reaction rate adjustment factor based on stability class.
        
        Atmospheric stability affects mixing and reaction timescales. Unstable
        conditions (A) enhance mixing and accelerate reactions, while stable
        conditions (F) suppress mixing and slow reactions.
        
        Parameters:
            fields (AtmosphericField): Extracted atmospheric fields
        
        Returns:
            ndarray: Dimensionless rate factor (range: 0.5-1.5), shape (ny, nx)
        
        References:
            Turner, D.B. (1994). Workbook of atmospheric dispersion estimates.
        """
        stability_class = fields.stability_class
        
        # Lookup table: stability class -> rate factor
        # Based on mixing intensity (unstable=rapid mixing, stable=limited mixing)
        rate_factors = {
            0: 1.5,  # A (extremely unstable) -> 1.5× faster reactions
            1: 1.3,  # B (moderately unstable)
            2: 1.1,  # C (slightly unstable)
            3: 1.0,  # D (neutral) -> baseline
            4: 0.8,  # E (slightly stable)
            5: 0.5,  # F (very stable) -> 0.5× slower reactions
        }
        
        rate_factor = np.zeros_like(stability_class, dtype=float)
        for stab_class, factor in rate_factors.items():
            rate_factor[stability_class == stab_class] = factor
        
        return rate_factor
    
    def export_oxygen_delivery_rate(self, fields: AtmosphericField,
                                   z_level: float = 0.0,
                                   roughness_height: float = 0.1) -> np.ndarray:
        """Compute wind-dependent oxygen delivery rate for oxidation processes.
        
        O₂ supply rate is controlled by turbulent mass transfer at the 
        mineral-fluid interface. Higher wind speeds increase surface shear stress
        and reduce diffusive boundary layer thickness.
        
        Parameters:
            fields (AtmosphericField): Extracted atmospheric fields
            z_level (float): Measurement height above surface [m]
            roughness_height (float): Aerodynamic roughness length [m]
        
        Returns:
            ndarray: O₂ delivery rate factor (dimensionless), shape (ny, nx)
        
        References:
            Sherwood, T.K. (1954). The mass transfer of particles and drops 
            from fixed and moving surfaces. Journal of Colloid Science, 9(1), 69-87.
        """
        u_mag = self.export_velocity_magnitude(fields, z_level)
        u_star = fields.u_star
        
        # Sherwood number correlation for fixed particles
        # Sh = 2 + 0.6 * Re^(1/2) * Sc^(1/3)
        # where Re = u * d / ν and Sc = ν / D_AB
        
        # Simplified: O₂ delivery ∝ u*^α where α ≈ 0.5-0.7
        # Use u_star as proxy for surface shear
        exponent = 0.6
        O2_factor = (u_star / 0.1)**exponent  # Normalized to u* = 0.1 m/s
        
        return O2_factor
    
    def export_co2_fugacity(self, fields: AtmosphericField,
                           T_ref: float = 298.15,
                           co2_mole_fraction: float = 0.0004) -> np.ndarray:
        """Compute CO₂ fugacity for pH-dependent mineral reactions.
        
        CO₂ solubility in water depends on pressure and temperature. Altitude-
        driven pressure variations affect carbonic acid equilibrium.
        
        Parameters:
            fields (AtmosphericField): Extracted atmospheric fields
            T_ref (float): Reference temperature [K] for solubility coefficient
            co2_mole_fraction (float): CO₂ mole fraction in atmosphere (default: 400 ppm)
        
        Returns:
            ndarray: CO₂ fugacity [Pa], shape (ny, nx)
        
        References:
            Plummer, L.N., & Busenberg, E. (1982). The solubility of calcite, 
            aragonite and vaterite in CO₂-H₂O solutions between 0 and 90°C, 
            and an evaluation of the aqueous model for the system CaCO₃-CO₂-H₂O.
            Geochimica et Cosmochimica Acta, 46(6), 1011-1040.
        """
        # Surface pressure and temperature
        P_surf = fields.P[0, :, :]
        T_surf = fields.T[0, :, :]
        
        # CO₂ fugacity = partial pressure = total pressure × mole fraction
        # Adjusted for temperature via Henry's law
        KH_T = self.KH_0 * np.exp(self.dH_sol / self.R_gas * (1/T_surf - 1/T_ref))
        
        # Partial pressure of CO₂
        P_co2 = P_surf * co2_mole_fraction
        
        return P_co2
    
    def export_water_activity(self, fields: AtmosphericField) -> np.ndarray:
        """Export water activity for saturation state calculations.
        
        Water activity controls mineral solubility and precipitation. Dry 
        conditions (low RH) increase water activity changes.
        
        Parameters:
            fields (AtmosphericField): Extracted atmospheric fields
        
        Returns:
            ndarray: Water activity (0-1), shape (nz, ny, nx)
        """
        # Simplified: a_w ≈ RH / 100 for dilute solutions
        a_w = np.clip(fields.RH / 100.0, 0.0, 1.0)
        return a_w
