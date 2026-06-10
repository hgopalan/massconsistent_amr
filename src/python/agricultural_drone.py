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
                 t_half_chem_ref=3600.0, t_half_photo_ref=1800.0):
        """
        Executes the moving-source Gaussian Puff advection-dispersion simulation loop.
        """
        self.puffs = []
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
                        'bin_name': bin_name
                    }
                    self.puffs.append(new_puff)
                
            # 2. Advect and grow existing puffs
            for puff in self.puffs:
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
                
                # Photolytic & Chemical Degradation
                if enable_degradation:
                    puff['mass'] = compute_degradation_decay(
                        mass=puff['mass'],
                        dt=dt,
                        temperature=temperature,
                        solar_radiation=solar_radiation,
                        t_half_chem_ref=t_half_chem_ref,
                        t_half_photo_ref=t_half_photo_ref
                    )
                
                # Interpolate wind velocity at puff center
                u, v, w = self._interpolate_wind_velocity(
                    puff['x'], puff['y'], puff['z'], u_field, v_field, w_field
                )
                
                # Advection drift
                puff['x'] += u * dt
                puff['y'] += v * dt
                puff['z'] += (w - v_s) * dt
                puff['age'] += dt
                
        # 3. Compile concentration grid C(x, y, z) by superposition
        # Define 3D coordinate grids for vectorized calculations
        X, Y, Z = np.meshgrid(self.x_coords, self.y_coords, self.z_coords, indexing='ij')
        # Meshgrid output indexing='ij' gives shapes (nx, ny, nz)
        
        for puff in self.puffs:
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
                 t_half_chem_ref=3600.0, t_half_photo_ref=1800.0):
        """
        Executes LPDM simulation loop along the drone trajectory.
        """
        self.particles = []
        np.random.seed(random_seed)
        
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
                
                for idx, bin_name in enumerate(bin_names):
                    num_parts = particles_per_bin[idx]
                    if num_parts <= 0:
                        continue
                    
                    bin_info = droplet_bins[bin_name]
                    bin_diameter = bin_info['diameter']
                    bin_fraction = bin_info['fraction']
                    
                    part_mass = (total_step_mass * bin_fraction) / num_parts
                    
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
                    
                # Deactivate if out of domain
                if not (self.xmin <= p['x'] <= self.xmax and
                        self.ymin <= p['y'] <= self.ymax and
                        self.zmin <= p['z'] <= self.zmax):
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
                
                # Photolytic & Chemical Degradation
                if enable_degradation:
                    p['mass'] = compute_degradation_decay(
                        mass=p['mass'],
                        dt=dt,
                        temperature=temperature,
                        solar_radiation=solar_radiation,
                        t_half_chem_ref=t_half_chem_ref,
                        t_half_photo_ref=t_half_photo_ref
                    )
                    
                # Interpolate wind velocity
                u, v, w = self._interpolate_wind_velocity(
                    p['x'], p['y'], p['z'], u_field, v_field, w_field
                )
                
                # Stochastic random walk step
                rand_h = np.random.normal(0.0, sigma_step_h, 2)
                rand_v = np.random.normal(0.0, sigma_step_v)
                
                p['x'] += u * dt + rand_h[0]
                p['y'] += v * dt + rand_h[1]
                p['z'] += (w - v_s) * dt + rand_v
                
                # Clamp boundary reflection (simple elastic bounce at ground)
                if p['z'] < self.zmin:
                    p['z'] = self.zmin + (self.zmin - p['z'])
                if p['z'] > self.zmax:
                    p['z'] = self.zmax - (p['z'] - self.zmax)
                    
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
