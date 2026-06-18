#!/usr/bin/env python3
"""
test_agricultural_drone.py - Comprehensive Unit Tests for Agricultural Drone Operations
"""

import os
import sys
import unittest
import tempfile
import numpy as np

# Add src/python to path
TEST_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_PYTHON_DIR = os.path.join(os.path.dirname(os.path.dirname(TEST_DIR)), 'src', 'python')
sys.path.insert(0, SRC_PYTHON_DIR)

from agricultural_drone import (
    DroneTrajectory, MassEmissionRegulator, DronePuffDispersion, DroneLpdDispersion,
    compute_settling_velocity, compute_evaporative_shrinkage, compute_degradation_decay,
    compute_rotor_downwash
)


class TestAgriculturalDrone(unittest.TestCase):
    
    def setUp(self):
        # Create a temporary directory for telemetry files
        self.temp_dir = tempfile.TemporaryDirectory()
        
        # Define mock trajectory telemetry arrays
        self.times = [0.0, 10.0, 20.0]
        self.x_pts = [10.0, 60.0, 110.0]
        self.y_pts = [20.0, 70.0, 120.0]
        self.z_pts = [5.0, 7.5, 10.0]
        self.speeds = [5.0, 5.0, 5.0]
        self.headings = [45.0, 45.0, 45.0]
        self.flow_rates = [1.2, 1.8, 0.0]  # Nozzle stops at end
        self.active_flags = [True, True, False]
        
    def tearDown(self):
        self.temp_dir.cleanup()

    def test_trajectory_initialization_and_interpolation(self):
        """Test explicit initialization and interpolation of drone trajectory."""
        traj = DroneTrajectory(
            times=self.times,
            x_pts=self.x_pts,
            y_pts=self.y_pts,
            z_pts=self.z_pts,
            speeds=self.speeds,
            headings=self.headings,
            flow_rates=self.flow_rates,
            active_flags=self.active_flags
        )
        
        self.assertEqual(traj.get_duration(), 20.0)
        
        # Test exact bounds interpolation
        state_0 = traj.interpolate(0.0)
        self.assertAlmostEqual(state_0['x'], 10.0)
        self.assertAlmostEqual(state_0['y'], 20.0)
        self.assertAlmostEqual(state_0['z'], 5.0)
        self.assertAlmostEqual(state_0['flow_rate'], 1.2)
        self.assertTrue(state_0['active'])
        
        # Test midpoint interpolation (t = 5.0)
        state_5 = traj.interpolate(5.0)
        self.assertAlmostEqual(state_5['x'], 35.0)
        self.assertAlmostEqual(state_5['y'], 45.0)
        self.assertAlmostEqual(state_5['z'], 6.25)
        self.assertAlmostEqual(state_5['flow_rate'], 1.5)
        self.assertTrue(state_5['active'])
        
        # Test inactive bounds interpolation (t = 20.0)
        state_20 = traj.interpolate(20.0)
        self.assertAlmostEqual(state_20['flow_rate'], 0.0)
        self.assertFalse(state_20['active'])
        
        # Test clamping outside boundaries
        state_neg = traj.interpolate(-5.0)
        self.assertEqual(state_neg['x'], 10.0)
        state_large = traj.interpolate(30.0)
        self.assertEqual(state_large['x'], 110.0)

    def test_trajectory_csv_loading(self):
        """Test loading flight trajectory telemetry from a CSV file."""
        csv_path = os.path.join(self.temp_dir.name, "drone_telemetry.csv")
        
        with open(csv_path, 'w') as f:
            f.write("time,x,y,z,speed,heading,flow_rate,active\n")
            f.write("0.0,10.0,20.0,5.0,5.0,45.0,1.2,true\n")
            f.write("10.0,60.0,70.0,7.5,5.0,45.0,1.8,true\n")
            f.write("20.0,110.0,120.0,10.0,5.0,45.0,0.0,false\n")
            
        traj = DroneTrajectory.from_csv(csv_path)
        self.assertEqual(traj.get_duration(), 20.0)
        
        state_5 = traj.interpolate(5.0)
        self.assertAlmostEqual(state_5['x'], 35.0)
        self.assertAlmostEqual(state_5['y'], 45.0)
        self.assertAlmostEqual(state_5['z'], 6.25)
        self.assertAlmostEqual(state_5['flow_rate'], 1.5)
        self.assertTrue(state_5['active'])

    def test_mass_emission_regulator(self):
        """Test volumetric flow-rate conversion and speed-dependent scaling."""
        # Active fraction = 10% (0.1), density = 1000 g/L (water-like)
        reg = MassEmissionRegulator(
            formulation_density=1000.0,
            active_fraction=0.1,
            base_speed=5.0,
            speed_dependent=False
        )
        
        # Test static emission rate: 1.2 L/min -> 0.02 L/s * 1000 g/L * 0.1 active = 2.0 g/s active pesticide
        rate_static = reg.compute_emission_rate(flow_rate_l_min=1.2, speed=5.0, active=True)
        self.assertAlmostEqual(rate_static, 2.0)
        
        # Test emission rate when spraying is inactive
        rate_inactive = reg.compute_emission_rate(flow_rate_l_min=1.2, speed=5.0, active=False)
        self.assertEqual(rate_inactive, 0.0)
        
        # Test speed-dependent scaling enabled
        reg_speed = MassEmissionRegulator(
            formulation_density=1000.0,
            active_fraction=0.1,
            base_speed=5.0,
            speed_dependent=True
        )
        
        # At base speed (5.0 m/s), emission rate should be unchanged (2.0 g/s)
        rate_at_base = reg_speed.compute_emission_rate(flow_rate_l_min=1.2, speed=5.0, active=True)
        self.assertAlmostEqual(rate_at_base, 2.0)
        
        # At double speed (10.0 m/s), emission rate should double to maintain deposition density
        rate_double = reg_speed.compute_emission_rate(flow_rate_l_min=1.2, speed=10.0, active=True)
        self.assertAlmostEqual(rate_double, 4.0)

    def test_moving_source_puff_dispersion(self):
        """Test moving-source Gaussian Puff simulation and concentration accumulation."""
        traj = DroneTrajectory(
            times=[0.0, 5.0],
            x_pts=[50.0, 100.0],
            y_pts=[50.0, 50.0],
            z_pts=[10.0, 10.0],
            speeds=[10.0, 10.0],
            headings=[0.0, 0.0],
            flow_rates=[1.2, 1.2],
            active_flags=[True, True]
        )
        
        reg = MassEmissionRegulator()
        
        # Domain 200m x 200m x 50m with cell size 10m
        model = DronePuffDispersion(
            xmin=0.0, xmax=200.0, ymin=0.0, ymax=200.0, zmin=0.0, zmax=50.0,
            dx=10.0, dy=10.0, dz=10.0
        )
        
        # Run simulation with 1 m/s crosswind in X direction
        model.simulate(
            trajectory=traj,
            regulator=reg,
            wind_solver=None,
            dt=1.0,
            u_uniform=1.0,
            v_uniform=0.0,
            w_uniform=0.0,
            K_h=0.5,
            K_v=0.2,
            sigma_y0=0.5,
            sigma_z0=0.5,
            enable_ground_reflection=True
        )
        
        # Check that puffs were generated
        self.assertGreater(len(model.puffs), 0)
        
        # Check concentration grid output shape
        self.assertEqual(model.concentration.shape, (5, 20, 20))  # (nz, ny, nx)
        
        # Concentration should be non-zero near the path of the drone and advected puffs
        total_conc = np.sum(model.concentration)
        self.assertGreater(total_conc, 0.0)

    def test_moving_source_lpd_dispersion(self):
        """Test moving-source Lagrangian Particle Dispersion Model simulation."""
        traj = DroneTrajectory(
            times=[0.0, 5.0],
            x_pts=[50.0, 100.0],
            y_pts=[50.0, 50.0],
            z_pts=[10.0, 10.0],
            speeds=[10.0, 10.0],
            headings=[0.0, 0.0],
            flow_rates=[1.2, 1.2],
            active_flags=[True, True]
        )
        
        reg = MassEmissionRegulator()
        
        model = DroneLpdDispersion(
            xmin=0.0, xmax=200.0, ymin=0.0, ymax=200.0, zmin=0.0, zmax=50.0,
            dx=10.0, dy=10.0, dz=10.0
        )
        
        # Run LPDM simulation with 5 particles per step
        model.simulate(
            trajectory=traj,
            regulator=reg,
            wind_solver=None,
            dt=1.0,
            u_uniform=1.0,
            v_uniform=0.0,
            w_uniform=0.0,
            K_h=0.5,
            K_v=0.2,
            particles_per_step=5,
            random_seed=42
        )
        
        # Check that particles were generated: (5 steps + 1) * 5 particles = 30 particles
        self.assertEqual(len(model.particles), 30)
        
        # Check concentration output grid
        self.assertEqual(model.concentration.shape, (5, 20, 20))
        total_conc = np.sum(model.concentration)
        self.assertGreater(total_conc, 0.0)

    def test_droplet_physics_helpers(self):
        """Test individual physical microphysics helper functions."""
        # 1. Settling velocity: check that coarser settles faster
        v_fine = compute_settling_velocity(50e-6)
        v_medium = compute_settling_velocity(150e-6)
        v_coarse = compute_settling_velocity(350e-6)
        
        self.assertGreater(v_coarse, v_medium)
        self.assertGreater(v_medium, v_fine)
        self.assertGreater(v_fine, 0.0)
        
        # 2. Evaporation: check shrinkage under dry/hot conditions
        d_initial = 150e-6
        active_fraction = 0.1
        dt = 1.0
        
        # High evaporation case (hot & dry)
        d_dry_hot = compute_evaporative_shrinkage(
            diameter=d_initial, initial_diameter=d_initial,
            active_fraction=active_fraction, dt=dt,
            temperature=35.0, relative_humidity=0.1
        )
        # Cooler & humid case
        d_cool_humid = compute_evaporative_shrinkage(
            diameter=d_initial, initial_diameter=d_initial,
            active_fraction=active_fraction, dt=dt,
            temperature=15.0, relative_humidity=0.8
        )
        
        self.assertLess(d_dry_hot, d_initial)
        self.assertLess(d_dry_hot, d_cool_humid)
        
        # Check minimum diameter core limit (d_min = d_initial * active_fraction^(1/3))
        d_min_expected = d_initial * (active_fraction ** (1.0/3.0))
        d_fully_evaporated = compute_evaporative_shrinkage(
            diameter=d_initial, initial_diameter=d_initial,
            active_fraction=active_fraction, dt=100.0,  # long time
            temperature=40.0, relative_humidity=0.0
        )
        self.assertAlmostEqual(d_fully_evaporated, d_min_expected, places=6)
        
        # 3. Chemical & photolytic degradation: check mass reduction
        initial_mass = 1.0
        # Hot and sunny (faster degradation)
        mass_hot_sunny = compute_degradation_decay(
            mass=initial_mass, dt=600.0,
            temperature=30.0, solar_radiation=1000.0
        )
        # Cooler and shady (slower degradation)
        mass_cool_shady = compute_degradation_decay(
            mass=initial_mass, dt=600.0,
            temperature=15.0, solar_radiation=100.0
        )
        
        self.assertLess(mass_hot_sunny, initial_mass)
        self.assertLess(mass_hot_sunny, mass_cool_shady)

    def test_puff_dispersion_with_microphysics(self):
        """Test DronePuffDispersion with all physical interactions enabled."""
        traj = DroneTrajectory(
            times=[0.0, 5.0],
            x_pts=[50.0, 100.0],
            y_pts=[50.0, 50.0],
            z_pts=[20.0, 20.0],
            speeds=[10.0, 10.0],
            headings=[0.0, 0.0],
            flow_rates=[1.2, 1.2],
            active_flags=[True, True]
        )
        
        reg = MassEmissionRegulator(
            formulation_density=1000.0,
            active_fraction=0.1
        )
        
        model = DronePuffDispersion(
            xmin=0.0, xmax=200.0, ymin=0.0, ymax=200.0, zmin=0.0, zmax=50.0,
            dx=10.0, dy=10.0, dz=10.0
        )
        
        # Run with settling, evaporation, and degradation
        model.simulate(
            trajectory=traj,
            regulator=reg,
            wind_solver=None,
            dt=1.0,
            u_uniform=1.0,
            v_uniform=0.0,
            w_uniform=0.0,
            K_h=0.5,
            K_v=0.2,
            enable_ground_reflection=True,
            temperature=30.0,
            relative_humidity=0.2,
            solar_radiation=800.0,
            enable_settling=True,
            enable_evaporation=True,
            enable_degradation=True
        )
        
        # Each active step (6 steps: 0, 1, 2, 3, 4, 5) releases 3 binned puffs -> 18 puffs total
        self.assertEqual(len(model.puffs), 18)
        
        # Verify puff attributes have changed
        for puff in model.puffs:
            self.assertIn(puff['bin_name'], ['fine', 'medium', 'coarse'])
            self.assertLess(puff['diameter'], puff['initial_diameter'])  # Evaporation
            self.assertLess(puff['z'], 20.0)  # Settling (since w_uniform is 0, they should fall)

    def test_lpd_dispersion_with_microphysics(self):
        """Test DroneLpdDispersion with all physical interactions enabled."""
        traj = DroneTrajectory(
            times=[0.0, 5.0],
            x_pts=[50.0, 100.0],
            y_pts=[50.0, 50.0],
            z_pts=[20.0, 20.0],
            speeds=[10.0, 10.0],
            headings=[0.0, 0.0],
            flow_rates=[1.2, 1.2],
            active_flags=[True, True]
        )
        
        reg = MassEmissionRegulator(
            formulation_density=1000.0,
            active_fraction=0.1
        )
        
        model = DroneLpdDispersion(
            xmin=0.0, xmax=200.0, ymin=0.0, ymax=200.0, zmin=0.0, zmax=50.0,
            dx=10.0, dy=10.0, dz=10.0
        )
        
        # Run with settling, evaporation, and degradation
        model.simulate(
            trajectory=traj,
            regulator=reg,
            wind_solver=None,
            dt=1.0,
            u_uniform=1.0,
            v_uniform=0.0,
            w_uniform=0.0,
            K_h=0.5,
            K_v=0.2,
            particles_per_step=10,
            random_seed=42,
            temperature=30.0,
            relative_humidity=0.2,
            solar_radiation=800.0,
            enable_settling=True,
            enable_evaporation=True,
            enable_degradation=True
        )
        
        # 6 steps * 10 particles = 60 particles
        self.assertEqual(len(model.particles), 60)
        
        # Verify particles have expected attributes and modified diameters
        for p in model.particles:
            self.assertIn(p['bin_name'], ['fine', 'medium', 'coarse'])
            # Since particles can advect out or remain active, check active ones
            if p['active'] and p['x'] > 50.0:  # age > 0
                self.assertLess(p['diameter'], p['initial_diameter'])  # Evaporation

    def test_rotor_downwash_parameterization(self):
        """Test analytical rotor downwash velocity field, ground dampening, and wall-jet."""
        # 1. Point above the drone should have no downwash
        u, v, w = compute_rotor_downwash(
           px=50.0, py=50.0, pz=12.0,
           drone_x=50.0, drone_y=50.0, drone_z=10.0,
           speed=0.0, heading=0.0
        )
        self.assertEqual(u, 0.0)
        self.assertEqual(v, 0.0)
        self.assertEqual(w, 0.0)

        # 2. Point directly below the drone should have downward vertical velocity
        u, v, w_high = compute_rotor_downwash(
           px=50.0, py=50.0, pz=9.0,
           drone_x=50.0, drone_y=50.0, drone_z=10.0,
           speed=0.0, heading=0.0
        )
        self.assertEqual(u, 0.0)
        self.assertEqual(v, 0.0)
        self.assertLess(w_high, 0.0)  # downward velocity is negative

        # 3. Downward centerline velocity should decay with distance
        _, _, w_low = compute_rotor_downwash(
           px=50.0, py=50.0, pz=5.0,
           drone_x=50.0, drone_y=50.0, drone_z=10.0,
           speed=0.0, heading=0.0
        )
        # Centerline velocity magnitude at 5.0m should be less than at 9.0m below drone (which is 1.0m away)
        self.assertLess(abs(w_low), abs(w_high))

        # 4. Ground effect dampening
        # Test flat terrain of height 0.0
        terrain = np.zeros((10, 10))
        # Point extremely close to the ground (pz = 0.01)
        _, _, w_ground = compute_rotor_downwash(
           px=50.0, py=50.0, pz=0.01,
           drone_x=50.0, drone_y=50.0, drone_z=10.0,
           speed=0.0, heading=0.0,
           terrain=terrain, xmin=0.0, ymin=0.0, dx=10.0, dy=10.0
        )
        # Vertical downwash should be heavily dampened (close to 0) near the ground
        self.assertLess(abs(w_ground), abs(w_low))

        # 5. Outward radial wall-jet spreading near terrain
        # Evaluate offset point near the ground
        u_wall, v_wall, w_wall = compute_rotor_downwash(
           px=51.0, py=51.0, pz=0.1,  # offset diagonally
           drone_x=50.0, drone_y=50.0, drone_z=10.0,
           speed=0.0, heading=0.0,
           terrain=terrain, xmin=0.0, ymin=0.0, dx=10.0, dy=10.0
        )
        # Horizontal velocities should be positive (flowing outwards towards (51, 51) from (50, 50))
        self.assertGreater(u_wall, 0.0)
        self.assertGreater(v_wall, 0.0)

        # 6. Execute simulator integrations with downwash enabled
        traj = DroneTrajectory(
           times=[0.0, 5.0],
           x_pts=[50.0, 100.0],
           y_pts=[50.0, 50.0],
           z_pts=[10.0, 10.0],
           speeds=[10.0, 10.0],
           headings=[0.0, 0.0],
           flow_rates=[1.2, 1.2],
           active_flags=[True, True]
        )
        reg = MassEmissionRegulator()
        
        # Test Puff Solver with downwash
        puff_model = DronePuffDispersion(
           xmin=0.0, xmax=200.0, ymin=0.0, ymax=200.0, zmin=0.0, zmax=50.0,
           dx=10.0, dy=10.0, dz=10.0
        )
        puff_model.simulate(
           trajectory=traj,
           regulator=reg,
           enable_rotor_downwash=True,
           drone_mass=18.0,
           rotor_radius=0.5
        )
        self.assertGreater(len(puff_model.puffs), 0)
        self.assertGreater(np.sum(puff_model.concentration), 0.0)

        # Test LPD Solver with downwash
        lpd_model = DroneLpdDispersion(
           xmin=0.0, xmax=200.0, ymin=0.0, ymax=200.0, zmin=0.0, zmax=50.0,
           dx=10.0, dy=10.0, dz=10.0
        )
        lpd_model.simulate(
           trajectory=traj,
           regulator=reg,
           particles_per_step=5,
           enable_rotor_downwash=True,
           drone_mass=18.0,
           rotor_radius=0.5
        )
        self.assertGreater(len(lpd_model.particles), 0)
        self.assertGreater(np.sum(lpd_model.concentration), 0.0)

    def test_foliage_interception_and_deposition_mapping_puff(self):
        """Test foliage interception and cumulative 2D registers for puff dispersion."""
        traj = DroneTrajectory(
            times=[0.0, 3.0],
            x_pts=[50.0, 80.0],
            y_pts=[50.0, 50.0],
            z_pts=[2.5, 2.5],  # close to canopy height of 3.0m
            speeds=[10.0, 10.0],
            headings=[0.0, 0.0],
            flow_rates=[1.2, 1.2],
            active_flags=[True, True]
        )
        reg = MassEmissionRegulator()
        model = DronePuffDispersion(
            xmin=0.0, xmax=150.0, ymin=0.0, ymax=150.0, zmin=0.0, zmax=30.0,
            dx=10.0, dy=10.0, dz=5.0
        )
        
        # Run with canopy interception enabled
        model.simulate(
            trajectory=traj,
            regulator=reg,
            dt=1.0,
            enable_settling=True,
            enable_canopy_interception=True,
            canopy_height=3.0,
            leaf_area_index=2.5,
            frontal_area_index=1.2
        )
        
        # Check that cumulative registers accumulated deposited mass
        self.assertGreater(np.sum(model.canopy_top_deposition), 0.0)
        self.assertGreater(np.sum(model.lower_foliage_deposition), 0.0)
        
        # Verify shape
        self.assertEqual(model.canopy_top_deposition.shape, (15, 15))
        self.assertEqual(model.lower_foliage_deposition.shape, (15, 15))
        self.assertEqual(model.ground_deposition.shape, (15, 15))
        
        # Verify mass conservation
        conserved, balance = model.verify_mass_conservation()
        self.assertTrue(conserved, f"Mass not conserved! Balance: {balance}")
        self.assertAlmostEqual(balance['total_emitted_mass'], balance['total_accounted'], places=5)

    def test_foliage_interception_and_deposition_mapping_lpd(self):
        """Test foliage interception, ground deposition, and registers for LPDM."""
        traj = DroneTrajectory(
            times=[0.0, 3.0],
            x_pts=[50.0, 80.0],
            y_pts=[50.0, 50.0],
            z_pts=[2.5, 2.5],
            speeds=[10.0, 10.0],
            headings=[0.0, 0.0],
            flow_rates=[1.2, 1.2],
            active_flags=[True, True]
        )
        reg = MassEmissionRegulator()
        model = DroneLpdDispersion(
            xmin=0.0, xmax=150.0, ymin=0.0, ymax=150.0, zmin=0.0, zmax=30.0,
            dx=10.0, dy=10.0, dz=5.0
        )
        
        # Run with canopy interception enabled
        model.simulate(
            trajectory=traj,
            regulator=reg,
            dt=1.0,
            particles_per_step=10,
            enable_settling=True,
            enable_canopy_interception=True,
            canopy_height=3.0,
            leaf_area_index=2.5,
            frontal_area_index=1.2
        )
        
        # Verify deposition in registers
        self.assertGreater(np.sum(model.canopy_top_deposition), 0.0)
        self.assertGreater(np.sum(model.lower_foliage_deposition), 0.0)
        
        # Verify mass conservation
        conserved, balance = model.verify_mass_conservation()
        self.assertTrue(conserved, f"Mass not conserved! Balance: {balance}")
        self.assertAlmostEqual(balance['total_emitted_mass'], balance['total_accounted'], places=5)

    def test_spatially_varying_canopy_fields(self):
        """Test lookup of spatially distributed two-dimensional canopy parameter arrays."""
        ny, nx = 15, 15
        can_h_arr = np.zeros((ny, nx))
        can_h_arr[4:7, 4:7] = 3.0  # localized crop patch
        
        lai_arr = np.zeros((ny, nx))
        lai_arr[4:7, 4:7] = 3.5
        
        fai_arr = np.zeros((ny, nx))
        fai_arr[4:7, 4:7] = 1.5
        
        traj = DroneTrajectory(
            times=[0.0, 3.0],
            x_pts=[55.0, 55.0],  # flies directly over the crop patch (cell index 5, 5)
            y_pts=[55.0, 55.0],
            z_pts=[2.0, 2.0],
            speeds=[5.0, 5.0],
            headings=[0.0, 0.0],
            flow_rates=[1.2, 1.2],
            active_flags=[True, True]
        )
        reg = MassEmissionRegulator()
        model = DroneLpdDispersion(
            xmin=0.0, xmax=150.0, ymin=0.0, ymax=150.0, zmin=0.0, zmax=30.0,
            dx=10.0, dy=10.0, dz=5.0
        )
        
        model.simulate(
            trajectory=traj,
            regulator=reg,
            dt=1.0,
            particles_per_step=15,
            enable_settling=True,
            enable_canopy_interception=True,
            canopy_height=can_h_arr,
            leaf_area_index=lai_arr,
            frontal_area_index=fai_arr
        )
        
        # Verify that deposition occurred only in/around the localized crop patch cells
        self.assertGreater(np.sum(model.canopy_top_deposition), 0.0)
        self.assertGreater(np.sum(model.lower_foliage_deposition), 0.0)
        
        # Ensure cells outside patch have zero canopy deposition
        self.assertEqual(model.canopy_top_deposition[0, 0], 0.0)
        self.assertEqual(model.lower_foliage_deposition[0, 0], 0.0)
        
        # Verify mass conservation
        conserved, balance = model.verify_mass_conservation()
        self.assertTrue(conserved)

    def test_mass_conservation_degradation_and_out_of_bounds(self):
        """Test mass conservation accounting with degradation and out-of-bounds loss."""
        traj = DroneTrajectory(
            times=[0.0, 2.0],
            x_pts=[145.0, 145.0],  # very close to +X boundary of 150m, so particles will drift out
            y_pts=[50.0, 50.0],
            z_pts=[10.0, 10.0],
            speeds=[5.0, 5.0],
            headings=[0.0, 0.0],
            flow_rates=[1.2, 1.2],
            active_flags=[True, True]
        )
        reg = MassEmissionRegulator()
        model = DroneLpdDispersion(
            xmin=0.0, xmax=150.0, ymin=0.0, ymax=150.0, zmin=0.0, zmax=30.0,
            dx=10.0, dy=10.0, dz=5.0
        )
        
        model.simulate(
            trajectory=traj,
            regulator=reg,
            dt=1.0,
            particles_per_step=10,
            u_uniform=10.0,  # strong wind blowing out of +X boundary
            enable_degradation=True,  # trigger photolytic & chemical decay
            solar_radiation=1000.0,
            temperature=35.0
        )
        
        # Verify that we had out-of-bounds mass and degraded mass
        self.assertGreater(model.out_of_bounds_mass, 0.0)
        self.assertGreater(model.degraded_mass, 0.0)
        
        # Verify 100% exact mass conservation down to tolerance
        conserved, balance = model.verify_mass_conservation()
        self.assertTrue(conserved, f"Mass not conserved with losses! Balance: {balance}")


if __name__ == "__main__":
    unittest.main()
