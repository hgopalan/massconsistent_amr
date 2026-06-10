#!/usr/bin/env python3
"""
agricultural_drone.py - Agricultural Drone Operations & Pest Management

Provides a clean Python module for agricultural drone trajectory parsing,
nozzle flow-rate scaling, and dynamic moving-source dispersion modeling
using Gaussian Puff and Lagrangian Particle Dispersion Model (LPDM) approaches.

References:
    - Sherman, C. A. (1978). A mass-consistent model for wind fields over complex terrain.
    - Hanna, S. R., et al. (1982). Handbook on atmospheric diffusion.
    - Pasquill, F., & Gifford, F. A. (1961). Atmospheric dispersion estimation.
"""

import numpy as np
import os
import csv


def compute_settling_velocity(diameter, density=1000.0, gravity=9.81, air_viscosity=1.81e-5, air_mean_free_path=6.6e-8):
    """
    Calculates terminal settling velocity using Stokes' Law with Cunningham slip correction.
    
    Args:
        diameter (float): Droplet diameter [m]
        density (float): Droplet density [kg/m^3]
        gravity (float): Gravitational acceleration [m/s^2]
        air_viscosity (float): Dynamic viscosity of air [Pa*s]
        air_mean_free_path (float): Mean free path of air molecules [m]
        
    Returns:
        float: Terminal settling velocity [m/s]
    """
    if diameter <= 0.0:
        return 0.0
        
    # Cunningham slip correction factor
    A1 = 1.257
    A2 = 0.400
    A3 = 1.100
    
    kn = 2.0 * air_mean_free_path / diameter
    Cc = 1.0 + kn * (A1 + A2 * np.exp(-A3 / kn))
    
    # Stokes' velocity: v_s = (rho * g * d^2) / (18 * mu) * Cc
    v_s = (density * gravity * diameter**2) / (18.0 * air_viscosity) * Cc
    return float(v_s)


def compute_evaporative_shrinkage(diameter, initial_diameter, active_fraction, dt, temperature=20.0, relative_humidity=0.5, formulation_density=1000.0):
    """
    Calculates the new droplet diameter after evaporation over timestep dt.
    Using Tetens equation for vapor pressure and d^2-law.
    
    Args:
        diameter (float): Current droplet diameter [m]
        initial_diameter (float): Initial droplet diameter at nozzle exit [m]
        active_fraction (float): Non-volatile mass fraction (0.0 to 1.0)
        dt (float): Timestep [s]
        temperature (float): Ambient air temperature [C]
        relative_humidity (float): Ambient relative humidity (fraction, 0.0 to 1.0)
        formulation_density (float): Fluid density [kg/m^3]
        
    Returns:
        float: New droplet diameter [m]
    """
    if diameter <= 0.0 or dt <= 0.0:
        return float(diameter)
        
    # Minimum diameter is based on non-volatile volume fraction
    f_nv = max(1e-6, min(1.0, active_fraction))
    d_min = initial_diameter * (f_nv ** (1.0/3.0))
    
    if diameter <= d_min:
        return float(d_min)
        
    # Tetens equation for saturation vapor pressure e_s (Pa)
    e_s = 610.78 * np.exp(17.27 * temperature / (temperature + 237.3))
    
    # Vapor density difference delta_rho_v (kg/m^3)
    delta_rho_v = (e_s * max(0.0, 1.0 - relative_humidity)) / (461.5 * (temperature + 273.15))
    
    # Diffusion coefficient of water vapor in air (m^2/s)
    D_v = 2.4e-5
    
    # Evaporation constant beta (m^2/s) from d(d^2)/dt = -beta
    beta = (16.0 * D_v * delta_rho_v) / formulation_density
    
    # New diameter squared
    d_sq_new = max(d_min**2, diameter**2 - beta * dt)
    return float(np.sqrt(d_sq_new))


def compute_degradation_decay(mass, dt, temperature=20.0, solar_radiation=500.0,
                              t_half_chem_ref=3600.0, t_half_photo_ref=1800.0,
                              t_ref=20.0, i_ref=500.0, q10=2.0):
    """
    Calculates the remaining active mass after degradation over timestep dt.
    
    Args:
        mass (float): Current active mass [g]
        dt (float): Timestep [s]
        temperature (float): Ambient air temperature [C]
        solar_radiation (float): Solar radiation intensity [W/m^2]
        t_half_chem_ref (float): Chemical half-life reference [s]
        t_half_photo_ref (float): Photolytic half-life reference [s]
        t_ref (float): Reference temperature [C]
        i_ref (float): Reference solar radiation [W/m^2]
        q10 (float): Temperature sensitivity coefficient
        
    Returns:
        float: Decayed mass [g]
    """
    if mass <= 0.0 or dt <= 0.0:
        return float(mass)
        
    # Chemical degradation rate constant (Arrhenius/Q10-based)
    k_chem = (np.log(2.0) / t_half_chem_ref) * (q10 ** ((temperature - t_ref) / 10.0))
    
    # Photolytic degradation rate constant (linear solar radiation scaling)
    k_photo = (np.log(2.0) / t_half_photo_ref) * (solar_radiation / i_ref)
    
    # Combined degradation rate constant
    k_total = k_chem + k_photo
    
    # Exponential decay
    new_mass = mass * np.exp(-k_total * dt)
    return float(new_mass)


def compute_rotor_downwash(px, py, pz, drone_x, drone_y, drone_z, speed, heading,
                           terrain=None, xmin=0.0, ymin=0.0, dx=10.0, dy=10.0,
                           drone_mass=15.0, rotor_radius=0.4, air_density=1.2,
                           alpha_jet=0.15, damp_scale=None, wall_jet_scale=None):
    """
    Computes the 3D analytical rotor downwash velocity field (u_wash, v_wash, w_wash)
    at a query point (px, py, pz) based on the drone's state and terrain.
    
    Args:
        px, py, pz (float): Coordinates of the query point [m]
        drone_x, drone_y, drone_z (float): Coordinates of the drone [m]
        speed (float): Flight speed of the drone [m/s]
        heading (float): Flight heading of the drone [degrees, 0 is +X]
        terrain (ndarray, optional): 2D array of terrain elevations [m]
        xmin, ymin (float): Domain origin for terrain lookup [m]
        dx, dy (float): Grid cell size for terrain lookup [m]
        drone_mass (float): Drone mass [kg] (default 15.0)
        rotor_radius (float): Rotor radius [m] (default 0.4)
        air_density (float): Density of air [kg/m^3] (default 1.2)
        alpha_jet (float): Jet expansion/entrainment coefficient (default 0.15)
        damp_scale (float, optional): Ground dampening scale [m]. Default is 1.5 * rotor_radius.
        wall_jet_scale (float, optional): Wall jet thickness decay scale [m]. Default is 1.0 * rotor_radius.
        
    Returns:
        tuple: (u_wash, v_wash, w_wash) velocity components [m/s]
    """
    # Above the drone, downwash is zero
    delta_z = drone_z - pz
    if delta_z < 0.0:
        return 0.0, 0.0, 0.0
        
    # Safeguard parameters
    if drone_mass <= 0.0 or rotor_radius <= 0.0 or air_density <= 0.0:
        return 0.0, 0.0, 0.0
        
    # 1. Induced velocity at the rotor disk (momentum theory)
    g = 9.81
    thrust = drone_mass * g
    # Area of rotor disk
    area = np.pi * rotor_radius**2
    v0 = np.sqrt(thrust / (2.0 * air_density * area))
    
    # 2. Flight velocity components
    heading_rad = np.radians(heading)
    vx = speed * np.cos(heading_rad)
    vy = speed * np.sin(heading_rad)
    
    # 3. Centerline deflection (advection of the jet downstream with altitude/distance)
    # Transit time estimate based on induced velocity (with small safeguard)
    v_transit = max(1e-4, v0)
    x_deflect = -vx * (delta_z / v_transit)
    y_deflect = -vy * (delta_z / v_transit)
    
    # Deflected jet center coordinates
    xc = drone_x + x_deflect
    yc = drone_y + y_deflect
    
    # Radial distance from the jet center
    r = np.sqrt((px - xc)**2 + (py - yc)**2)
    
    # 4. Jet expansion and velocity decay with distance delta_z
    R_j = rotor_radius + alpha_jet * delta_z
    W_c = v0 * (rotor_radius / R_j)
    
    # Downward velocity before ground effect
    w_wash_down = W_c * np.exp(-(r**2) / (R_j**2))
    
    # 5. Vortex interaction near terrain / ground effect
    # Look up terrain height at query point and drone position
    z_g_point = 0.0
    z_g_drone = 0.0
    if terrain is not None:
        ny, nx = terrain.shape
        # Helper to get terrain height at any (x, y)
        def get_z_g(x, y):
            i = int((x - xmin) / dx)
            j = int((y - ymin) / dy)
            i = max(0, min(nx - 1, i))
            j = max(0, min(ny - 1, j))
            return float(terrain[j, i])
        z_g_point = get_z_g(px, py)
        z_g_drone = get_z_g(drone_x, drone_y)
        
    # Height of query point above terrain
    h_point = pz - z_g_point
    if h_point <= 0.0:
        return 0.0, 0.0, 0.0
        
    # Ground dampening factor (decays vertically to 0 at the ground)
    d_damp = damp_scale if damp_scale is not None else 1.5 * rotor_radius
    f_damp = 1.0 - np.exp(-(h_point / d_damp)**2)
    
    # Vertical downwash velocity
    w_wash = -w_wash_down * f_damp
    
    # Outward radial wall-jet spreading
    d_wall = wall_jet_scale if wall_jet_scale is not None else 1.0 * rotor_radius
    # Magnitude of radial spreading velocity
    v_r = w_wash_down * (1.0 - f_damp) * (r / R_j) * np.exp(-h_point / d_wall)
    
    # Horizontal components of radial wall-jet
    if r > 1e-6:
        u_wash = v_r * ((px - xc) / r)
        v_wash = v_r * ((py - yc) / r)
    else:
        u_wash = 0.0
        v_wash = 0.0
        
    return float(u_wash), float(v_wash), float(w_wash)


class DroneTrajectory:
    """
    Represents and interpolates agricultural drone flight trajectories.
    
    Reads discrete flight telemetry and performs linear interpolation of
    3D positions, speeds, heading angles, nozzle flow rates, and active states.
    """
    
    def __init__(self, times=None, x_pts=None, y_pts=None, z_pts=None,
                 speeds=None, headings=None, flow_rates=None, active_flags=None):
        """
        Initialize DroneTrajectory with arrays of telemetry points.
        """
        self.times = np.array(times) if times is not None else np.array([])
        self.x_pts = np.array(x_pts) if x_pts is not None else np.array([])
        self.y_pts = np.array(y_pts) if y_pts is not None else np.array([])
        self.z_pts = np.array(z_pts) if z_pts is not None else np.array([])
        self.speeds = np.array(speeds) if speeds is not None else np.array([])
        self.headings = np.array(headings) if headings is not None else np.array([])
        self.flow_rates = np.array(flow_rates) if flow_rates is not None else np.array([])
        self.active_flags = np.array(active_flags) if active_flags is not None else np.array([])
        
        # Sort by time if provided to ensure correct interpolation
        if len(self.times) > 1:
            sort_indices = np.argsort(self.times)
            self.times = self.times[sort_indices]
            self.x_pts = self.x_pts[sort_indices]
            self.y_pts = self.y_pts[sort_indices]
            self.z_pts = self.z_pts[sort_indices]
            if len(self.speeds) == len(self.times):
                self.speeds = self.speeds[sort_indices]
            if len(self.headings) == len(self.times):
                self.headings = self.headings[sort_indices]
            if len(self.flow_rates) == len(self.times):
                self.flow_rates = self.flow_rates[sort_indices]
            if len(self.active_flags) == len(self.times):
                self.active_flags = self.active_flags[sort_indices]

    @classmethod
    def from_csv(cls, filepath):
        """
        Loads flight telemetry from a CSV file.
        
        Supported columns: time, x, y, z, speed, heading, flow_rate, active.
        """
        times, x_pts, y_pts, z_pts = [], [], [], []
        speeds, headings, flow_rates, active_flags = [], [], [], []
        
        if not os.path.exists(filepath):
            raise FileNotFoundError(f"Telemetry file not found: {filepath}")
            
        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                times.append(float(row.get('time', len(times))))
                x_pts.append(float(row.get('x', 0.0)))
                y_pts.append(float(row.get('y', 0.0)))
                z_pts.append(float(row.get('z', 10.0)))
                speeds.append(float(row.get('speed', 5.0)))
                headings.append(float(row.get('heading', 0.0)))
                flow_rates.append(float(row.get('flow_rate', 1.0)))
                # Active is True by default if not specified
                active_val = row.get('active', '1')
                active_flags.append(active_val.lower() in ('true', '1', 'yes') if isinstance(active_val, str) else bool(active_val))
                
        return cls(times, x_pts, y_pts, z_pts, speeds, headings, flow_rates, active_flags)

    def get_duration(self):
        """Returns the total duration of the flight trajectory in seconds."""
        if len(self.times) == 0:
            return 0.0
        return float(self.times[-1] - self.times[0])

    def interpolate(self, t):
        """
        Interpolate trajectory variables at any arbitrary time t.
        
        Clamps values to bounds if t is outside the trajectory time range.
        
        Returns:
            dict: Interpolated state {'x', 'y', 'z', 'speed', 'heading', 'flow_rate', 'active'}
        """
        if len(self.times) == 0:
            return {'x': 0.0, 'y': 0.0, 'z': 10.0, 'speed': 0.0, 'heading': 0.0, 'flow_rate': 0.0, 'active': False}
            
        # Clamp t to bounds
        t = max(self.times[0], min(self.times[-1], t))
        
        # Exact match or single point
        if len(self.times) == 1:
            return {
                'x': float(self.x_pts[0]),
                'y': float(self.y_pts[0]),
                'z': float(self.z_pts[0]),
                'speed': float(self.speeds[0]) if len(self.speeds) > 0 else 0.0,
                'heading': float(self.headings[0]) if len(self.headings) > 0 else 0.0,
                'flow_rate': float(self.flow_rates[0]) if len(self.flow_rates) > 0 else 0.0,
                'active': bool(self.active_flags[0]) if len(self.active_flags) > 0 else True
            }
            
        # Linear interpolation
        x = float(np.interp(t, self.times, self.x_pts))
        y = float(np.interp(t, self.times, self.y_pts))
        z = float(np.interp(t, self.times, self.z_pts))
        
        speed = float(np.interp(t, self.times, self.speeds)) if len(self.speeds) > 0 else 5.0
        heading = float(np.interp(t, self.times, self.headings)) if len(self.headings) > 0 else 0.0
        flow_rate = float(np.interp(t, self.times, self.flow_rates)) if len(self.flow_rates) > 0 else 0.0
        
        # Active is step-wise / nearest-neighbor or linear threshold
        if len(self.active_flags) > 0:
            idx = np.searchsorted(self.times, t)
            idx = max(0, min(len(self.times) - 1, idx))
            active = bool(self.active_flags[idx])
        else:
            active = True
            
        return {
            'x': x, 'y': y, 'z': z,
            'speed': speed, 'heading': heading,
            'flow_rate': flow_rate, 'active': active
        }


class MassEmissionRegulator:
    """
    Manages the regulation and scaling of pesticide active ingredient release.
    Converts nozzle volumetric flow rates to dynamic physical mass emissions.
    """
    
    def __init__(self, formulation_density=1000.0, active_fraction=0.1,
                 base_speed=5.0, speed_dependent=False, droplet_bins=None):
        """
        Args:
            formulation_density (float): Density of pesticide mixture [g/L] (default 1000.0, water-like)
            active_fraction (float): Mass fraction of active chemical pesticide ingredient [0.0 - 1.0]
            base_speed (float): Reference flight speed for deposition density calibration [m/s]
            speed_dependent (bool): If True, scales emission rate proportionally to maintain uniform ground coverage
            droplet_bins (dict): Optional custom droplet size bins classification profile
        """
        self.formulation_density = formulation_density
        self.active_fraction = active_fraction
        self.base_speed = base_speed
        self.speed_dependent = speed_dependent
        self.droplet_bins = droplet_bins if droplet_bins is not None else {
            'fine': {'diameter': 50e-6, 'fraction': 0.2},
            'medium': {'diameter': 150e-6, 'fraction': 0.5},
            'coarse': {'diameter': 350e-6, 'fraction': 0.3}
        }

    def compute_emission_rate(self, flow_rate_l_min, speed, active=True):
        """
        Calculates active pesticide mass emission rate in grams per second [g/s].
        
        Formula:
            mass_emission = flow_rate [L/min] * (1/60) [min/s] * formulation_density [g/L] * active_fraction
        
        If speed_dependent is enabled, scales emission by (speed / base_speed).
        """
        if not active or flow_rate_l_min <= 0.0:
            return 0.0
            
        # Base emission rate [g/s]
        base_emission = (flow_rate_l_min / 60.0) * self.formulation_density * self.active_fraction
        
        if self.speed_dependent and speed > 0.01:
            # Adjust flow rate/emission to match velocity differences to keep deposition constant
            return base_emission * (speed / self.base_speed)
            
        return base_emission


class MovingSourceDispersionModel:
    """
    Base class for moving-source dispersion models.
    Provides coordinate grid structures and velocity interpolation helper functions.
    """
    
    def __init__(self, xmin=0.0, xmax=500.0, ymin=0.0, ymax=500.0, zmin=0.0, zmax=100.0,
                 dx=10.0, dy=10.0, dz=10.0):
        self.xmin = xmin
        self.xmax = xmax
        self.ymin = ymin
        self.ymax = ymax
        self.zmin = zmin
        self.zmax = zmax
        self.dx = dx
        self.dy = dy
        self.dz = dz
        
        # Coordinate grids
        self.nx = int((xmax - xmin) / dx)
        self.ny = int((ymax - ymin) / dy)
        self.nz = int((zmax - zmin) / dz)
        
        # Grid cell center coordinates
        self.x_coords = xmin + (np.arange(self.nx) + 0.5) * dx
        self.y_coords = ymin + (np.arange(self.ny) + 0.5) * dy
        self.z_coords = zmin + (np.arange(self.nz) + 0.5) * dz
        
        # Concentration grid [nz, ny, nx]
        self.concentration = np.zeros((self.nz, self.ny, self.nx))
        
        # Terrain height field [ny, nx]
        self.terrain = np.zeros((self.ny, self.nx))
        
        # Spatially distributed 2D deposition registers across three compartments
        self.canopy_top_deposition = np.zeros((self.ny, self.nx))
        self.lower_foliage_deposition = np.zeros((self.ny, self.nx))
        self.ground_deposition = np.zeros((self.ny, self.nx))
        
        # Mass tracking variables for conservation verification
        self.total_emitted_mass = 0.0
        self.out_of_bounds_mass = 0.0
        self.degraded_mass = 0.0

    def verify_mass_conservation(self, tolerance=1e-5):
        """
        Validates the conservation of pesticide mass by ensuring that the sum of dispersed mass
        (out-of-bounds mass), dynamic airborne mass, canopy-deposited mass, and ground-deposited
        mass balances with the total nozzle-emitted mass.
        
        Returns:
            bool: True if mass is conserved within the specified tolerance, False otherwise
            dict: Detailed registers of the mass balance compartments
        """
        # Calculate active airborne mass
        airborne_mass = 0.0
        if hasattr(self, 'particles'):
            airborne_mass = float(sum(p['mass'] for p in self.particles if p.get('active', True)))
        elif hasattr(self, 'puffs'):
            airborne_mass = float(sum(puff['mass'] for puff in self.puffs if puff.get('active', True)))
            
        canopy_deposited = float(self.canopy_top_deposition.sum() + self.lower_foliage_deposition.sum())
        ground_deposited = float(self.ground_deposition.sum())
        
        total_accounted = float(airborne_mass + canopy_deposited + ground_deposited + 
                                self.out_of_bounds_mass + self.degraded_mass)
        
        if self.total_emitted_mass > 0.0:
            rel_error = abs(total_accounted - self.total_emitted_mass) / self.total_emitted_mass
            conserved = rel_error <= tolerance
        else:
            rel_error = 0.0
            conserved = True
            
        balance = {
            'total_emitted_mass': self.total_emitted_mass,
            'airborne_mass': airborne_mass,
            'canopy_deposited_mass': canopy_deposited,
            'canopy_top_deposited': float(self.canopy_top_deposition.sum()),
            'lower_foliage_deposited': float(self.lower_foliage_deposition.sum()),
            'ground_deposited_mass': ground_deposited,
            'out_of_bounds_mass': self.out_of_bounds_mass,
            'degraded_mass': self.degraded_mass,
            'total_accounted': total_accounted,
            'relative_error': rel_error,
            'conserved': conserved
        }
        return conserved, balance
        
    def setup_grid_from_solver(self, wind_solver):
        """Configures computational grid matching the C++ WindSolver domain."""
        self.xmin = wind_solver.xmin
        self.xmax = wind_solver.xmax
        self.ymin = wind_solver.ymin
        self.ymax = wind_solver.ymax
        self.zmin = wind_solver.zmin
        self.zmax = wind_solver.zmax
        self.dx = wind_solver.dx
        self.dy = wind_solver.dy
        self.dz = wind_solver.dz
        
        self.nx = wind_solver.nx
        self.ny = wind_solver.ny
        self.nz = wind_solver.nz
        
        self.x_coords = self.xmin + (np.arange(self.nx) + 0.5) * self.dx
        self.y_coords = self.ymin + (np.arange(self.ny) + 0.5) * self.dy
        self.z_coords = self.zmin + (np.arange(self.nz) + 0.5) * self.dz
        self.concentration = np.zeros((self.nz, self.ny, self.nx))
        
        # Reset 2D deposition registers to new shape
        self.canopy_top_deposition = np.zeros((self.ny, self.nx))
        self.lower_foliage_deposition = np.zeros((self.ny, self.nx))
        self.ground_deposition = np.zeros((self.ny, self.nx))
        self.total_emitted_mass = 0.0
        self.out_of_bounds_mass = 0.0
        self.degraded_mass = 0.0
        
        # Set terrain from wind solver
        if hasattr(wind_solver, 'get_terrain'):
            self.terrain = wind_solver.get_terrain()
        elif hasattr(wind_solver, 'terrain'):
            self.terrain = wind_solver.terrain
        else:
            self.terrain = np.zeros((self.ny, self.nx))

    def _interpolate_wind_velocity(self, px, py, pz, u_field, v_field, w_field):
        """
        Trilinearly interpolates 3D wind velocity components at location (px, py, pz).
        Returns:
            tuple: (u, v, w) at (px, py, pz)
        """
        # Grid index search
        i = int((px - self.xmin) / self.dx - 0.5)
        j = int((py - self.ymin) / self.dy - 0.5)
        k = int((pz - self.zmin) / self.dz - 0.5)
        
        # Clamp indices to grid bounds
        i0 = max(0, min(self.nx - 1, i))
        i1 = max(0, min(self.nx - 1, i0 + 1))
        j0 = max(0, min(self.ny - 1, j))
        j1 = max(0, min(self.ny - 1, j0 + 1))
        k0 = max(0, min(self.nz - 1, k))
        k1 = max(0, min(self.nz - 1, k0 + 1))
        
        # Interpolation fractions
        xf = max(0.0, min(1.0, (px - (self.xmin + (i0 + 0.5) * self.dx)) / self.dx)) if i0 != i1 else 0.0
        yf = max(0.0, min(1.0, (py - (self.ymin + (j0 + 0.5) * self.dy)) / self.dy)) if j0 != j1 else 0.0
        zf = max(0.0, min(1.0, (pz - (self.zmin + (k0 + 0.5) * self.dz)) / self.dz)) if k0 != k1 else 0.0
        
        def interp_field(field):
            c000 = field[k0, j0, i0]
            c100 = field[k0, j0, i1]
            c010 = field[k0, j1, i0]
            c110 = field[k0, j1, i1]
            c001 = field[k1, j0, i0]
            c101 = field[k1, j0, i1]
            c011 = field[k1, j1, i0]
            c111 = field[k1, j1, i1]
            
            c00 = c000 * (1.0 - xf) + c100 * xf
            c10 = c010 * (1.0 - xf) + c110 * xf
            c01 = c001 * (1.0 - xf) + c101 * xf
            c11 = c011 * (1.0 - xf) + c111 * xf
            
            c0 = c00 * (1.0 - yf) + c10 * yf
            c1 = c01 * (1.0 - yf) + c11 * yf
            
            return c0 * (1.0 - zf) + c1 * zf

        u = interp_field(u_field)
        v = interp_field(v_field)
        w = interp_field(w_field)
        
        return u, v, w


class DronePuffDispersion(MovingSourceDispersionModel):
    """
    Simulates dynamic pesticide transport using moving-source Gaussian Puffs.
    Puffs are emitted from the drone's coordinates and advected with the wind.
    """
    
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.puffs = []  # List of dicts representing puffs: {x, y, z, mass, age}
        
    def simulate(self, trajectory, regulator, wind_solver=None, dt=1.0,
                 u_uniform=1.0, v_uniform=0.0, w_uniform=0.0,
                 K_h=1.0, K_v=0.5, sigma_y0=0.5, sigma_z0=0.5,
                 enable_ground_reflection=True,
                 temperature=20.0, relative_humidity=0.5, solar_radiation=500.0,
                 enable_settling=False, enable_evaporation=False, enable_degradation=False,
                 t_half_chem_ref=3600.0, t_half_photo_ref=1800.0,
                 enable_rotor_downwash=False,
                 drone_mass=15.0, rotor_radius=0.4, air_density=1.2,
                 alpha_jet=0.15, damp_scale=None, wall_jet_scale=None,
                 enable_canopy_interception=False,
                 canopy_height=2.0, leaf_area_index=3.0, frontal_area_index=1.0):
        """
        Executes the moving-source Gaussian Puff advection-dispersion simulation loop.
        """
        self.puffs = []
        
        # Reset 2D deposition registers and mass tracking variables
        self.canopy_top_deposition.fill(0.0)
        self.lower_foliage_deposition.fill(0.0)
        self.ground_deposition.fill(0.0)
        self.total_emitted_mass = 0.0
        self.out_of_bounds_mass = 0.0
        self.degraded_mass = 0.0
        
        duration = trajectory.get_duration()
        steps = int(np.ceil(duration / dt))
        
        # Extract 3D wind velocity fields if wind solver is provided
        if wind_solver is not None:
            self.setup_grid_from_solver(wind_solver)
            vel = wind_solver.get_velocity()
            u_field, v_field, w_field = vel['u'], vel['v'], vel['w']
        else:
            u_field = np.ones((self.nz, self.ny, self.nx)) * u_uniform
            v_field = np.ones((self.nz, self.ny, self.nx)) * v_uniform
            w_field = np.ones((self.nz, self.ny, self.nx)) * w_uniform
            
        # Reset concentration grid
        self.concentration.fill(0.0)
        
        # Step through flight timeline
        for step in range(steps + 1):
            t = step * dt
            drone_state = trajectory.interpolate(t)
            
            # 1. Mass release from nozzle source
            emission_rate = regulator.compute_emission_rate(
                drone_state['flow_rate'], drone_state['speed'], drone_state['active']
            )
            
            if emission_rate > 1.e-8:
                total_step_mass = emission_rate * dt
                self.total_emitted_mass += total_step_mass
                
                # Droplet Size Binning
                droplet_bins = getattr(regulator, 'droplet_bins', None)
                if droplet_bins is None:
                    droplet_bins = {
                        'default': {'diameter': 150e-6, 'fraction': 1.0}
                    }
                
                for bin_name, bin_info in droplet_bins.items():
                    bin_fraction = bin_info['fraction']
                    bin_diameter = bin_info['diameter']
                    new_puff = {
                        'x': drone_state['x'],
                        'y': drone_state['y'],
                        'z': drone_state['z'],
                        'mass': emission_rate * dt * bin_fraction,
                        'age': 0.0,
                        'diameter': bin_diameter,
                        'initial_diameter': bin_diameter,
                        'bin_name': bin_name,
                        'active': True
                    }
                    self.puffs.append(new_puff)
                
            # 2. Advect and grow existing puffs
            for puff in self.puffs:
                if not puff.get('active', True):
                    continue
                    
                # Identify local horizontal cell index
                i_cell = int((puff['x'] - self.xmin) / self.dx)
                j_cell = int((puff['y'] - self.ymin) / self.dy)
                
                # Check bounds first
                if not (0 <= i_cell < self.nx and 0 <= j_cell < self.ny):
                    self.out_of_bounds_mass += puff['mass']
                    puff['active'] = False
                    continue
                    
                terrain_h = self.terrain[j_cell, i_cell]
                z_agl = puff['z'] - terrain_h
                
                # Check ground impact
                if puff['z'] <= terrain_h or puff['z'] <= self.zmin:
                    self.ground_deposition[j_cell, i_cell] += puff['mass']
                    puff['active'] = False
                    continue
                    
                # Check upper vertical bound
                if puff['z'] > self.zmax:
                    self.out_of_bounds_mass += puff['mass']
                    puff['active'] = False
                    continue
                    
                # Active fraction of regulator
                active_frac = getattr(regulator, 'active_fraction', 0.1)
                
                # Evaporative Size Reduction
                if enable_evaporation:
                    puff['diameter'] = compute_evaporative_shrinkage(
                        diameter=puff['diameter'],
                        initial_diameter=puff['initial_diameter'],
                        active_fraction=active_frac,
                        dt=dt,
                        temperature=temperature,
                        relative_humidity=relative_humidity,
                        formulation_density=regulator.formulation_density
                    )
                
                # Size-Dependent Gravitational Settling
                v_s = 0.0
                if enable_settling:
                    v_s = compute_settling_velocity(
                        diameter=puff['diameter'],
                        density=regulator.formulation_density
                    )
                    
                # Look up local canopy parameters
                local_h = canopy_height[j_cell, i_cell] if isinstance(canopy_height, np.ndarray) else canopy_height
                local_lai = leaf_area_index[j_cell, i_cell] if isinstance(leaf_area_index, np.ndarray) else leaf_area_index
                local_fai = frontal_area_index[j_cell, i_cell] if isinstance(frontal_area_index, np.ndarray) else frontal_area_index
                
                # Apply foliage interception
                if enable_canopy_interception and local_h > 0.0 and 0.0 <= z_agl < local_h:
                    d_ref = 100e-6
                    eta_d = 1.0 - np.exp(-puff['diameter'] / d_ref)
                    
                    v_s_calc = compute_settling_velocity(puff['diameter'], regulator.formulation_density)
                    
                    u_loc, v_loc, w_loc = self._interpolate_wind_velocity(
                        puff['x'], puff['y'], puff['z'], u_field, v_field, w_field
                    )
                    U_h = np.sqrt(u_loc**2 + v_loc**2)
                    
                    k_dep_vert = (v_s_calc / max(0.1, local_h)) * local_lai * eta_d * 0.5
                    k_dep_horiz = (U_h / max(0.1, local_h)) * local_fai * eta_d * 0.5
                    k_foliage = k_dep_vert + k_dep_horiz
                    
                    intercept_fraction = 1.0 - np.exp(-k_foliage * dt)
                    delta_m = puff['mass'] * intercept_fraction
                    
                    puff['mass'] -= delta_m
                    
                    if z_agl >= 0.5 * local_h:
                        self.canopy_top_deposition[j_cell, i_cell] += delta_m
                    else:
                        self.lower_foliage_deposition[j_cell, i_cell] += delta_m
                
                # Photolytic & Chemical Degradation
                if enable_degradation:
                    old_mass = puff['mass']
                    puff['mass'] = compute_degradation_decay(
                        mass=old_mass,
                        dt=dt,
                        temperature=temperature,
                        solar_radiation=solar_radiation,
                        t_half_chem_ref=t_half_chem_ref,
                        t_half_photo_ref=t_half_photo_ref
                    )
                    self.degraded_mass += (old_mass - puff['mass'])
                    
                if puff['mass'] < 1.0e-12:
                    puff['active'] = False
                    continue
                
                # Interpolate wind velocity at puff center
                u, v, w = self._interpolate_wind_velocity(
                    puff['x'], puff['y'], puff['z'], u_field, v_field, w_field
                )
                
                # Rotor downwash velocity field superposition
                u_wash, v_wash, w_wash = 0.0, 0.0, 0.0
                if enable_rotor_downwash:
                    u_wash, v_wash, w_wash = compute_rotor_downwash(
                        px=puff['x'], py=puff['y'], pz=puff['z'],
                        drone_x=drone_state['x'], drone_y=drone_state['y'], drone_z=drone_state['z'],
                        speed=drone_state['speed'], heading=drone_state['heading'],
                        terrain=self.terrain, xmin=self.xmin, ymin=self.ymin, dx=self.dx, dy=self.dy,
                        drone_mass=drone_mass, rotor_radius=rotor_radius, air_density=air_density,
                        alpha_jet=alpha_jet, damp_scale=damp_scale, wall_jet_scale=wall_jet_scale
                    )
                
                # Advection drift
                puff['x'] += (u + u_wash) * dt
                puff['y'] += (v + v_wash) * dt
                puff['z'] += (w + w_wash - v_s) * dt
                puff['age'] += dt
                
        # 3. Compile concentration grid C(x, y, z) by superposition
        # Define 3D coordinate grids for vectorized calculations
        X, Y, Z = np.meshgrid(self.x_coords, self.y_coords, self.z_coords, indexing='ij')
        # Meshgrid output indexing='ij' gives shapes (nx, ny, nz)
        
        for puff in self.puffs:
            if not puff.get('active', True):
                continue
                
            # Puff width and height expansion based on diffusivities
            sigma_y = np.sqrt(sigma_y0**2 + 2.0 * K_h * puff['age'])
            sigma_z = np.sqrt(sigma_z0**2 + 2.0 * K_v * puff['age'])
            
            # Gaussian contribution
            denom = (2.0 * np.pi)**1.5 * (sigma_y**2) * sigma_z
            r_sq_h = (X - puff['x'])**2 + (Y - puff['y'])**2
            
            # Primary puff concentration
            exponent = -r_sq_h / (2.0 * sigma_y**2) - (Z - puff['z'])**2 / (2.0 * sigma_z**2)
            c_contrib = (puff['mass'] / denom) * np.exp(exponent)
            
            # Ground reflection via image source
            if enable_ground_reflection:
                exponent_refl = -r_sq_h / (2.0 * sigma_y**2) - (Z + puff['z'])**2 / (2.0 * sigma_z**2)
                c_contrib += (puff['mass'] / denom) * np.exp(exponent_refl)
                
            # Accumulate on grid [nz, ny, nx]
            # Transpose c_contrib from shape (nx, ny, nz) to (nz, ny, nx) to align with self.concentration
            self.concentration += c_contrib.transpose(2, 1, 0)


class DroneLpdDispersion(MovingSourceDispersionModel):
    """
    Simulates dynamic pesticide transport using a Lagrangian Particle Dispersion Model.
    Particles are emitted from the drone nozzle and follow a random-walk trajectory.
    """
    
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.particles = []  # List of dicts representing particles: {x, y, z, mass, active}
        
    def simulate(self, trajectory, regulator, wind_solver=None, dt=1.0,
                 u_uniform=1.0, v_uniform=0.0, w_uniform=0.0,
                 K_h=1.0, K_v=0.5, particles_per_step=10,
                 random_seed=42,
                 temperature=20.0, relative_humidity=0.5, solar_radiation=500.0,
                 enable_settling=False, enable_evaporation=False, enable_degradation=False,
                 t_half_chem_ref=3600.0, t_half_photo_ref=1800.0,
                 enable_rotor_downwash=False,
                 drone_mass=15.0, rotor_radius=0.4, air_density=1.2,
                 alpha_jet=0.15, damp_scale=None, wall_jet_scale=None,
                 enable_canopy_interception=False,
                 canopy_height=2.0, leaf_area_index=3.0, frontal_area_index=1.0):
        """
        Executes LPDM simulation loop along the drone trajectory.
        """
        self.particles = []
        np.random.seed(random_seed)
        
        # Reset 2D deposition registers and mass tracking variables
        self.canopy_top_deposition.fill(0.0)
        self.lower_foliage_deposition.fill(0.0)
        self.ground_deposition.fill(0.0)
        self.total_emitted_mass = 0.0
        self.out_of_bounds_mass = 0.0
        self.degraded_mass = 0.0
        
        duration = trajectory.get_duration()
        steps = int(np.ceil(duration / dt))
        
        # Configure wind velocity fields
        if wind_solver is not None:
            self.setup_grid_from_solver(wind_solver)
            vel = wind_solver.get_velocity()
            u_field, v_field, w_field = vel['u'], vel['v'], vel['w']
        else:
            u_field = np.ones((self.nz, self.ny, self.nx)) * u_uniform
            v_field = np.ones((self.nz, self.ny, self.nx)) * v_uniform
            w_field = np.ones((self.nz, self.ny, self.nx)) * w_uniform
            
        # Reset concentration grid
        self.concentration.fill(0.0)
        
        # Dispersion standard deviations for stochastic random walk steps
        sigma_step_h = np.sqrt(2.0 * K_h * dt)
        sigma_step_v = np.sqrt(2.0 * K_v * dt)
        
        # Step through timeline
        for step in range(steps + 1):
            t = step * dt
            drone_state = trajectory.interpolate(t)
            
            # 1. Emission: Release new particles
            emission_rate = regulator.compute_emission_rate(
                drone_state['flow_rate'], drone_state['speed'], drone_state['active']
            )
            
            if emission_rate > 1.e-8 and particles_per_step > 0:
                total_step_mass = emission_rate * dt
                self.total_emitted_mass += total_step_mass
                
                # Droplet Size Binning
                droplet_bins = getattr(regulator, 'droplet_bins', None)
                if droplet_bins is None:
                    droplet_bins = {
                        'default': {'diameter': 150e-6, 'fraction': 1.0}
                    }
                
                bin_names = list(droplet_bins.keys())
                bin_fractions = [droplet_bins[b]['fraction'] for b in bin_names]
                
                # Distribute particles_per_step to bins using multinomial
                particles_per_bin = np.random.multinomial(particles_per_step, bin_fractions)
                
                # Determine which bins actually received particles to prevent mass loss
                active_indices = [idx for idx in range(len(bin_names)) if particles_per_bin[idx] > 0]
                active_fraction_sum = sum(bin_fractions[idx] for idx in active_indices)
                
                for idx in active_indices:
                    bin_name = bin_names[idx]
                    num_parts = particles_per_bin[idx]
                    
                    bin_info = droplet_bins[bin_name]
                    bin_diameter = bin_info['diameter']
                    bin_fraction = bin_info['fraction']
                    
                    # Normalize fraction among active bins to preserve exact total mass
                    normalized_fraction = bin_fraction / active_fraction_sum if active_fraction_sum > 0.0 else bin_fraction
                    part_mass = (total_step_mass * normalized_fraction) / num_parts
                    
                    for _ in range(num_parts):
                        new_particle = {
                            'x': drone_state['x'],
                            'y': drone_state['y'],
                            'z': drone_state['z'],
                            'mass': part_mass,
                            'active': True,
                            'diameter': bin_diameter,
                            'initial_diameter': bin_diameter,
                            'bin_name': bin_name
                        }
                        self.particles.append(new_particle)
                    
            # 2. Advect particles with wind + stochastic random walk
            for p in self.particles:
                if not p['active']:
                    continue
                    
                # Identify local horizontal cell index
                i_cell = int((p['x'] - self.xmin) / self.dx)
                j_cell = int((p['y'] - self.ymin) / self.dy)
                
                # Check bounds first
                if not (0 <= i_cell < self.nx and 0 <= j_cell < self.ny):
                    self.out_of_bounds_mass += p['mass']
                    p['active'] = False
                    continue
                    
                terrain_h = self.terrain[j_cell, i_cell]
                z_agl = p['z'] - terrain_h
                
                # Check ground impact
                if p['z'] <= terrain_h or p['z'] <= self.zmin:
                    self.ground_deposition[j_cell, i_cell] += p['mass']
                    p['active'] = False
                    continue
                    
                # Check upper vertical bound
                if p['z'] > self.zmax:
                    self.out_of_bounds_mass += p['mass']
                    p['active'] = False
                    continue
                    
                # Active fraction of regulator
                active_frac = getattr(regulator, 'active_fraction', 0.1)
                
                # Evaporative Size Reduction
                if enable_evaporation:
                    p['diameter'] = compute_evaporative_shrinkage(
                        diameter=p['diameter'],
                        initial_diameter=p['initial_diameter'],
                        active_fraction=active_frac,
                        dt=dt,
                        temperature=temperature,
                        relative_humidity=relative_humidity,
                        formulation_density=regulator.formulation_density
                    )
                
                # Size-Dependent Gravitational Settling
                v_s = 0.0
                if enable_settling:
                    v_s = compute_settling_velocity(
                        diameter=p['diameter'],
                        density=regulator.formulation_density
                    )
                
                # Look up local canopy parameters
                local_h = canopy_height[j_cell, i_cell] if isinstance(canopy_height, np.ndarray) else canopy_height
                local_lai = leaf_area_index[j_cell, i_cell] if isinstance(leaf_area_index, np.ndarray) else leaf_area_index
                local_fai = frontal_area_index[j_cell, i_cell] if isinstance(frontal_area_index, np.ndarray) else frontal_area_index
                
                # Apply foliage interception
                if enable_canopy_interception and local_h > 0.0 and 0.0 <= z_agl < local_h:
                    d_ref = 100e-6
                    eta_d = 1.0 - np.exp(-p['diameter'] / d_ref)
                    
                    v_s_calc = compute_settling_velocity(p['diameter'], regulator.formulation_density)
                    
                    u_loc, v_loc, w_loc = self._interpolate_wind_velocity(
                        p['x'], p['y'], p['z'], u_field, v_field, w_field
                    )
                    U_h = np.sqrt(u_loc**2 + v_loc**2)
                    
                    k_dep_vert = (v_s_calc / max(0.1, local_h)) * local_lai * eta_d * 0.5
                    k_dep_horiz = (U_h / max(0.1, local_h)) * local_fai * eta_d * 0.5
                    k_foliage = k_dep_vert + k_dep_horiz
                    
                    intercept_fraction = 1.0 - np.exp(-k_foliage * dt)
                    delta_m = p['mass'] * intercept_fraction
                    
                    p['mass'] -= delta_m
                    
                    if z_agl >= 0.5 * local_h:
                        self.canopy_top_deposition[j_cell, i_cell] += delta_m
                    else:
                        self.lower_foliage_deposition[j_cell, i_cell] += delta_m
                
                # Photolytic & Chemical Degradation
                if enable_degradation:
                    old_mass = p['mass']
                    p['mass'] = compute_degradation_decay(
                        mass=old_mass,
                        dt=dt,
                        temperature=temperature,
                        solar_radiation=solar_radiation,
                        t_half_chem_ref=t_half_chem_ref,
                        t_half_photo_ref=t_half_photo_ref
                    )
                    self.degraded_mass += (old_mass - p['mass'])
                    
                if p['mass'] < 1.0e-12:
                    p['active'] = False
                    continue
                    
                # Interpolate wind velocity
                u, v, w = self._interpolate_wind_velocity(
                    p['x'], p['y'], p['z'], u_field, v_field, w_field
                )
                
                # Rotor downwash velocity field superposition
                u_wash, v_wash, w_wash = 0.0, 0.0, 0.0
                if enable_rotor_downwash:
                    u_wash, v_wash, w_wash = compute_rotor_downwash(
                        px=p['x'], py=p['y'], pz=p['z'],
                        drone_x=drone_state['x'], drone_y=drone_state['y'], drone_z=drone_state['z'],
                        speed=drone_state['speed'], heading=drone_state['heading'],
                        terrain=self.terrain, xmin=self.xmin, ymin=self.ymin, dx=self.dx, dy=self.dy,
                        drone_mass=drone_mass, rotor_radius=rotor_radius, air_density=air_density,
                        alpha_jet=alpha_jet, damp_scale=damp_scale, wall_jet_scale=wall_jet_scale
                    )
                
                # Stochastic random walk step
                rand_h = np.random.normal(0.0, sigma_step_h, 2)
                rand_v = np.random.normal(0.0, sigma_step_v)
                
                p['x'] += (u + u_wash) * dt + rand_h[0]
                p['y'] += (v + v_wash) * dt + rand_h[1]
                p['z'] += (w + w_wash - v_s) * dt + rand_v
                
        # Final boundary and deposition check for remaining active particles
        for p in self.particles:
            if not p['active']:
                continue
                
            i_cell = int((p['x'] - self.xmin) / self.dx)
            j_cell = int((p['y'] - self.ymin) / self.dy)
            
            if not (0 <= i_cell < self.nx and 0 <= j_cell < self.ny):
                self.out_of_bounds_mass += p['mass']
                p['active'] = False
                continue
                
            terrain_h = self.terrain[j_cell, i_cell]
            if p['z'] <= terrain_h or p['z'] <= self.zmin:
                self.ground_deposition[j_cell, i_cell] += p['mass']
                p['active'] = False
                continue
                
            if p['z'] > self.zmax:
                self.out_of_bounds_mass += p['mass']
                p['active'] = False
                continue
                    
        # 3. Grid-binning concentration compilation
        cell_volume = self.dx * self.dy * self.dz
        for p in self.particles:
            if not p['active']:
                continue
            
            # Identify which grid cell the particle occupies
            i = int((p['x'] - self.xmin) / self.dx)
            j = int((p['y'] - self.ymin) / self.dy)
            k = int((p['z'] - self.zmin) / self.dz)
            
            if 0 <= i < self.nx and 0 <= j < self.ny and 0 <= k < self.nz:
                # Add mass to grid cell concentration
                self.concentration[k, j, i] += p['mass'] / cell_volume
