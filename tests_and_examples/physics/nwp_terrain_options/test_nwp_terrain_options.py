#!/usr/bin/env python3
"""
test_nwp_terrain_options.py - Unit test suite for NWP/SRTM terrain construction options.
"""

import os
import sys
import unittest
import subprocess
import shutil
import netCDF4 as nc
import numpy as np


class TestNWPTerrainOptions(unittest.TestCase):
    """Verifies terrain construction options (i) and (ii) across the 4 tools."""
    
    def setUp(self):
        self.repo_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
        self.tools_dir = os.path.join(self.repo_dir, "tools", "data_ingestion")
        
        # Paths to scripts
        self.hrrr_script = os.path.join(self.tools_dir, "hrrr_to_surface_data.py")
        self.nam_script = os.path.join(self.tools_dir, "nam_ingestion.py")
        self.era5_script = os.path.join(self.tools_dir, "era5_to_windfield.py")
        self.climate_script = os.path.join(self.tools_dir, "download_climate_projection.py")
        
        # Test outputs
        self.test_terrain = "test_constructed_terrain.csv"
        self.test_surface_out = "test_surface_data.csv"
        self.test_era5_nc_out = "test_formatted_era5.nc"
        self.test_nam_windfield = "test_nam_windfield.csv"
        self.test_climate_rose = "test_future_wind_rose.csv"
        self.test_climate_profile = "test_future_scenarios.ini"
        
        self.cleanup_files = [
            self.test_terrain, self.test_surface_out, self.test_era5_nc_out,
            self.test_nam_windfield, self.test_climate_rose, self.test_climate_profile,
            "inputs.i"
        ]
        
        # Create a dummy inputs.i for nam_ingestion
        with open("inputs.i", "w") as f:
            f.write("terrain_file = test_constructed_terrain.csv\n")
            f.write("dx = 30.0\n")
            f.write("dy = 30.0\n")
            f.write("dz = 30.0\n")
            f.write("domain_height = 300.0\n")
            
        # Create a mock synthetic NetCDF file to act as our GRIB/NWP data
        self.mock_nwp_nc = "mock_nwp_dataset.nc"
        self.cleanup_files.append(self.mock_nwp_nc)
        
        with nc.Dataset(self.mock_nwp_nc, "w") as ds:
            ds.createDimension("longitude", 5)
            ds.createDimension("latitude", 5)
            ds.createDimension("level", 4)
            ds.createDimension("time", 1)
            
            lon_v = ds.createVariable("longitude", "f4", ("longitude",))
            lat_v = ds.createVariable("latitude", "f4", ("latitude",))
            lev_v = ds.createVariable("level", "f4", ("level",))
            t_v = ds.createVariable("time", "f4", ("time",))
            
            lon_v[:] = np.array([-105.02, -105.01, -105.00, -104.99, -104.98], dtype=np.float32)
            lat_v[:] = np.array([39.98, 39.99, 40.00, 40.01, 40.02], dtype=np.float32)
            lev_v[:] = np.array([1000.0, 850.0, 700.0, 500.0], dtype=np.float32)
            t_v[:] = np.array([0.0], dtype=np.float32)
            
            u_v = ds.createVariable("u", "f4", ("time", "level", "latitude", "longitude"))
            v_v = ds.createVariable("v", "f4", ("time", "level", "latitude", "longitude"))
            z_v = ds.createVariable("z", "f4", ("time", "level", "latitude", "longitude"))
            temp_v = ds.createVariable("t", "f4", ("time", "level", "latitude", "longitude"))
            q_v = ds.createVariable("q", "f4", ("time", "level", "latitude", "longitude"))
            
            # Elevation/Terrain
            hgt_v = ds.createVariable("HGT_M", "f4", ("latitude", "longitude"))
            hgt_v[:, :] = 123.45  # mock elevation
            
            u_v[:] = 10.0
            v_v[:] = 2.0
            z_v[:] = 100.0
            temp_v[:] = 290.0
            q_v[:] = 0.001

    def tearDown(self):
        for f in self.cleanup_files:
            if os.path.exists(f):
                try:
                    os.remove(f)
                except OSError:
                    pass

    def test_hrrr_option_i_nwp_terrain(self):
        """HRRR tool option (i): construct terrain.csv from NWP variables."""
        cmd = [
            sys.executable, self.hrrr_script,
            "--grib", self.mock_nwp_nc,
            "--output", self.test_surface_out,
            "--terrain-output", self.test_terrain,
            "--center-lonlat", "-105.0", "40.0",
            "--domain-size", "2000"
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Failed with stderr:\n{res.stderr}\nstdout:\n{res.stdout}")
        
        # Verify terrain output
        self.assertTrue(os.path.exists(self.test_terrain))
        with open(self.test_terrain, 'r') as f:
            lines = f.readlines()
        self.assertTrue(any("NWP" in l or "HRRR" in l for l in lines))
        data_lines = [l for l in lines if not l.startswith('#') and l.strip()]
        self.assertTrue(len(data_lines) > 0)
        # Verify Z value matches simulated HGT_M
        pt = [float(p) for p in data_lines[0].split()]
        self.assertAlmostEqual(pt[2], 123.45, places=2)

    def test_hrrr_option_ii_srtm_terrain(self):
        """HRRR tool option (ii): download SRTM terrain for the bounds."""
        # Use --srtm-terrain flag
        cmd = [
            sys.executable, self.hrrr_script,
            "--grib", self.mock_nwp_nc,
            "--output", self.test_surface_out,
            "--terrain-output", self.test_terrain,
            "--srtm-terrain",
            "--nx", "10",
            "--ny", "10",
            "--center-lonlat", "-105.0", "40.0",
            "--domain-size", "2000"
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Failed with stderr:\n{res.stderr}")
        
        self.assertTrue(os.path.exists(self.test_terrain))
        with open(self.test_terrain, 'r') as f:
            lines = f.readlines()
        self.assertTrue(any("Automated Geographic Elevation DEM" in l for l in lines))
        data_lines = [l for l in lines if not l.startswith('#') and l.strip()]
        self.assertEqual(len(data_lines), 100)

    def test_nam_option_i_nwp_terrain(self):
        """NAM tool option (i): construct terrain.csv from NAM variables."""
        cmd = [
            sys.executable, self.nam_script,
            "--inputs", "inputs.i",
            "--file", self.mock_nwp_nc,
            "--output", self.test_nam_windfield,
            "--create-terrain",
            "--terrain-output", self.test_terrain,
            "--lat-min", "39.98", "--lat-max", "40.02",
            "--lon-min", "-105.02", "--lon-max", "-104.98",
            "--nx", "10", "--ny", "10"
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Failed with stderr:\n{res.stderr}\nstdout:\n{res.stdout}")
        
        self.assertTrue(os.path.exists(self.test_terrain))
        with open(self.test_terrain, 'r') as f:
            lines = f.readlines()
        self.assertTrue(any("NAM" in l for l in lines))
        data_lines = [l for l in lines if not l.startswith('#') and l.strip()]
        self.assertEqual(len(data_lines), 100)
        # Verify Z value matches simulated HGT_M
        pt = [float(p) for p in data_lines[0].split()]
        self.assertAlmostEqual(pt[2], 123.45, places=2)

    def test_nam_option_ii_srtm_terrain(self):
        """NAM tool option (ii): download SRTM terrain for NAM bounds."""
        cmd = [
            sys.executable, self.nam_script,
            "--inputs", "inputs.i",
            "--file", self.mock_nwp_nc,
            "--output", self.test_nam_windfield,
            "--srtm-terrain",
            "--terrain-output", self.test_terrain,
            "--lat-min", "39.98", "--lat-max", "40.02",
            "--lon-min", "-105.02", "--lon-max", "-104.98",
            "--nx", "10", "--ny", "10"
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Failed with stderr:\n{res.stderr}")
        
        self.assertTrue(os.path.exists(self.test_terrain))
        with open(self.test_terrain, 'r') as f:
            lines = f.readlines()
        self.assertTrue(any("Automated Geographic Elevation" in l for l in lines))

    def test_era5_option_i_nwp_terrain(self):
        """ERA5 tool option (i): construct terrain.csv from ERA5 variables."""
        cmd = [
            sys.executable, self.era5_script,
            "--input", self.mock_nwp_nc,
            "--output", self.test_era5_nc_out,
            "--create-terrain",
            "--terrain-output", self.test_terrain
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Failed with stderr:\n{res.stderr}")
        
        self.assertTrue(os.path.exists(self.test_terrain))
        with open(self.test_terrain, 'r') as f:
            lines = f.readlines()
        self.assertTrue(any("ERA5" in l for l in lines))

    def test_era5_option_ii_srtm_terrain(self):
        """ERA5 tool option (ii): download SRTM terrain for ERA5 bounds."""
        cmd = [
            sys.executable, self.era5_script,
            "--input", self.mock_nwp_nc,
            "--output", self.test_era5_nc_out,
            "--srtm-terrain",
            "--terrain-output", self.test_terrain,
            "--nx", "10", "--ny", "10"
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Failed with stderr:\n{res.stderr}")
        
        self.assertTrue(os.path.exists(self.test_terrain))
        with open(self.test_terrain, 'r') as f:
            lines = f.readlines()
        self.assertTrue(any("Automated Geographic Elevation" in l for l in lines))

    def test_climate_projection_option_i_terrain(self):
        """Climate projection option (i): construct synthetic terrain.csv."""
        cmd = [
            sys.executable, self.climate_script,
            "--lat", "40.0", "--lon", "-105.0",
            "--output-rose", self.test_climate_rose,
            "--output-profile", self.test_climate_profile,
            "--create-terrain",
            "--terrain-output", self.test_terrain,
            "--nx", "10", "--ny", "10"
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Failed with stderr:\n{res.stderr}")
        
        self.assertTrue(os.path.exists(self.test_terrain))
        with open(self.test_terrain, 'r') as f:
            lines = f.readlines()
        self.assertTrue(any("climate projection" in l for l in lines))
        data_lines = [l for l in lines if not l.startswith('#') and l.strip()]
        self.assertEqual(len(data_lines), 100)

    def test_climate_projection_option_ii_srtm_terrain(self):
        """Climate projection option (ii): download SRTM terrain."""
        cmd = [
            sys.executable, self.climate_script,
            "--lat", "40.0", "--lon", "-105.0",
            "--output-rose", self.test_climate_rose,
            "--output-profile", self.test_climate_profile,
            "--srtm-terrain",
            "--terrain-output", self.test_terrain,
            "--nx", "10", "--ny", "10"
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, f"Failed with stderr:\n{res.stderr}")
        
        self.assertTrue(os.path.exists(self.test_terrain))
        with open(self.test_terrain, 'r') as f:
            lines = f.readlines()
        self.assertTrue(any("Automated Geographic" in l for l in lines))


if __name__ == "__main__":
    unittest.main()
