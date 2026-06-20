#!/usr/bin/env python3
"""
test_geographic_data_fetching.py - Unit test suite for automated geographic
and elevation data fetching utility.
"""

import os
import sys
import unittest
import subprocess


class TestGeographicDataFetching(unittest.TestCase):
    """Verifies correct functional execution and output structure of the geographic_data_fetcher.py script."""
    
    def setUp(self):
        self.script_path = os.path.abspath(
            os.path.join(os.path.dirname(__file__), "..", "..", "..", "tools", "data_ingestion", "geographic_data_fetcher.py")
        )
        self.test_dem = "test_temp_terrain.csv"
        self.test_lc = "test_temp_landuse.csv"
        self.cleanup_files = [self.test_dem, self.test_lc]

    def tearDown(self):
        for f in self.cleanup_files:
            if os.path.exists(f):
                try:
                    os.remove(f)
                except OSError:
                    pass

    def test_mock_fetcher_flat_earth(self):
        """Verifies that the fetcher runs successfully in mock mode with flat projection and outputs valid files."""
        cmd = [
            sys.executable, self.script_path,
            "--lat-min", "39.9",
            "--lat-max", "40.1",
            "--lon-min", "-105.3",
            "--lon-max", "-105.2",
            "--nx", "15",
            "--ny", "15",
            "--dem-output", self.test_dem,
            "--lc-output", self.test_lc,
            "--projection", "flat",
            "--mock"
        ]
        
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(result.returncode, 0, f"Script failed with stderr:\n{result.stderr}")
        
        # Verify elevation file
        self.assertTrue(os.path.exists(self.test_dem))
        with open(self.test_dem, 'r') as f:
            lines = f.readlines()
            
        # Check standard headers
        self.assertTrue(any("Automated Geographic Elevation DEM" in l for l in lines))
        self.assertTrue(any("Grid: 15x15 points" in l for l in lines))
        self.assertTrue(any("X[m] Y[m] Z[m]" in l for l in lines))
        
        # Check data lines count (should have 15*15 = 225 data rows)
        data_lines = [l for l in lines if not l.startswith("#") and l.strip()]
        self.assertEqual(len(data_lines), 225)
        
        # Verify coordinate columns
        for row in data_lines[:10]:
            parts = row.split()
            self.assertEqual(len(parts), 3)
            # Make sure they are float values
            float(parts[0])
            float(parts[1])
            float(parts[2])

        # Verify landuse file
        self.assertTrue(os.path.exists(self.test_lc))
        with open(self.test_lc, 'r') as f:
            lc_lines = f.readlines()
            
        self.assertTrue(any("Automated Land-use classification data" in l for l in lc_lines))
        self.assertTrue(any("Grid: 15x15 points" in l for l in lc_lines))
        self.assertTrue(any("X[m] Y[m] NLCD_Code z0[m]" in l for l in lc_lines))
        
        lc_data_lines = [l for l in lc_lines if not l.startswith("#") and l.strip()]
        self.assertEqual(len(lc_data_lines), 225)
        
        for row in lc_data_lines[:10]:
            parts = row.split()
            self.assertEqual(len(parts), 4)
            float(parts[0])
            float(parts[1])
            int(parts[2])  # NLCD_Code is integer
            float(parts[3])  # z0 is float

    def test_mock_fetcher_utm(self):
        """Verifies that the fetcher runs successfully in mock mode with UTM projection."""
        cmd = [
            sys.executable, self.script_path,
            "--lat-min", "45.0",
            "--lat-max", "45.1",
            "--lon-min", "-121.8",
            "--lon-max", "-121.7",
            "--nx", "10",
            "--ny", "10",
            "--dem-output", self.test_dem,
            "--lc-output", self.test_lc,
            "--projection", "utm",
            "--mock"
        ]
        
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(result.returncode, 0, f"Script failed with stderr:\n{result.stderr}")
        
        self.assertTrue(os.path.exists(self.test_dem))
        self.assertTrue(os.path.exists(self.test_lc))
        
        with open(self.test_dem, 'r') as f:
            lines = [l for l in f if not l.startswith("#") and l.strip()]
        self.assertEqual(len(lines), 100)


if __name__ == "__main__":
    unittest.main()
