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

from agricultural_drone import DroneTrajectory, MassEmissionRegulator, DronePuffDispersion, DroneLpdDispersion


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


if __name__ == "__main__":
    unittest.main()
