#!/usr/bin/env python3
"""
test_openfast_export.py - Validation tests for OpenFAST/TurbSim export

Tests BTS format generation, data integrity, and compatibility with OpenFAST.
"""

import sys
import os
import struct
import numpy as np
import tempfile
import unittest


class TestBTSFormat(unittest.TestCase):
    """Tests for TurbSim BTS binary format."""
    
    def setUp(self):
        """Set up test fixtures."""
        sys.path.insert(0, "/tmp/workspace/hgopalan/massconsistent_amr/tools")
        from openfast_export import TurbSimBTSWriter
        self.TurbSimBTSWriter = TurbSimBTSWriter
        self.temp_dir = tempfile.mkdtemp()
    
    def tearDown(self):
        """Clean up test files."""
        import shutil
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)
    
    def test_bts_header_validity(self):
        """Test BTS header validation."""
        writer = self.TurbSimBTSWriter()
        
        # Initialize with valid parameters
        writer.initialize(
            num_time_steps=10,
            nx=5, ny=5, nz=5,
            dt=0.1,
            u_mean=10.0,
            dx=10.0, dy=10.0, dz=10.0,
            z_hub=90.0,
            turbulence_intensity_u=0.14
        )
        
        # Check header is valid
        self.assertTrue(writer.header.is_valid())
        self.assertEqual(writer.header.id1, 7)
        self.assertEqual(writer.header.id2, 7)
        self.assertEqual(writer.header.nt, 10)
        self.assertEqual(writer.header.nz, 5)
        self.assertEqual(writer.header.ny, 5)
        self.assertEqual(writer.header.ncomp, 3)
    
    def test_metadata_initialization(self):
        """Test metadata is correctly initialized."""
        writer = self.TurbSimBTSWriter()
        
        writer.initialize(
            num_time_steps=5,
            nx=10, ny=10, nz=10,
            dt=0.2,
            u_mean=12.0,
            dx=20.0, dy=20.0, dz=20.0,
            z_hub=100.0,
            turbulence_intensity_u=0.15,
            seed=54321
        )
        
        meta = writer.metadata
        self.assertEqual(meta.u_mean, 12.0)
        self.assertEqual(meta.z_hub, 100.0)
        self.assertEqual(meta.intensity_u, 0.15)
        self.assertAlmostEqual(meta.intensity_v, 0.12, places=4)  # 0.8 * 0.15
        self.assertAlmostEqual(meta.intensity_w, 0.075, places=4)  # 0.5 * 0.15
        self.assertEqual(meta.seed, 54321)
    
    def test_bts_file_creation(self):
        """Test BTS file creation and header writing."""
        writer = self.TurbSimBTSWriter()
        
        writer.initialize(
            num_time_steps=1,
            nx=3, ny=3, nz=3,
            dt=0.1,
            u_mean=10.0,
            dx=10.0, dy=10.0, dz=10.0,
            z_hub=90.0
        )
        
        # Create dummy data
        u_data = np.ones(1 * 3 * 3 * 3, dtype=np.float32)
        v_data = np.ones(1 * 3 * 3 * 3, dtype=np.float32) * 0.5
        w_data = np.zeros(1 * 3 * 3 * 3, dtype=np.float32)
        
        output_file = os.path.join(self.temp_dir, "test.bts")
        
        # Export
        result = writer.export_time_series(output_file, u_data, v_data, w_data,
                                          3, 3, 3, 1)
        self.assertTrue(result)
        
        # Check file exists
        self.assertTrue(os.path.exists(output_file))
        self.assertGreater(os.path.getsize(output_file), 0)
    
    def test_bts_header_binary_format(self):
        """Test BTS header binary format correctness."""
        writer = self.TurbSimBTSWriter()
        
        writer.initialize(
            num_time_steps=5,
            nx=2, ny=3, nz=4,
            dt=0.05,
            u_mean=11.5,
            dx=15.0, dy=15.0, dz=15.0,
            z_hub=85.0,
            turbulence_intensity_u=0.13
        )
        
        # Create dummy data
        u_data = np.random.randn(5 * 2 * 3 * 4).astype(np.float32)
        v_data = np.random.randn(5 * 2 * 3 * 4).astype(np.float32)
        w_data = np.random.randn(5 * 2 * 3 * 4).astype(np.float32)
        
        output_file = os.path.join(self.temp_dir, "test_header.bts")
        
        # Export
        writer.export_time_series(output_file, u_data, v_data, w_data,
                                 2, 3, 4, 5)
        
        # Read and verify header
        with open(output_file, 'rb') as f:
            id1 = struct.unpack('i', f.read(4))[0]
            id2 = struct.unpack('i', f.read(4))[0]
            nt = struct.unpack('i', f.read(4))[0]
            ny = struct.unpack('i', f.read(4))[0]
            nz = struct.unpack('i', f.read(4))[0]
            ncomp = struct.unpack('i', f.read(4))[0]
            
            self.assertEqual(id1, 7)
            self.assertEqual(id2, 7)
            self.assertEqual(nt, 5)
            self.assertEqual(ny, 3)
            self.assertEqual(nz, 4)
            self.assertEqual(ncomp, 3)
    
    def test_metadata_file_creation(self):
        """Test metadata file is created alongside BTS."""
        writer = self.TurbSimBTSWriter()
        
        writer.initialize(
            num_time_steps=1,
            nx=2, ny=2, nz=2,
            dt=0.1,
            u_mean=10.0,
            dx=10.0, dy=10.0, dz=10.0,
            z_hub=90.0
        )
        
        u_data = np.ones(1 * 2 * 2 * 2, dtype=np.float32)
        v_data = np.ones(1 * 2 * 2 * 2, dtype=np.float32) * 0.5
        w_data = np.zeros(1 * 2 * 2 * 2, dtype=np.float32)
        
        output_file = os.path.join(self.temp_dir, "test_meta.bts")
        
        writer.export_time_series(output_file, u_data, v_data, w_data,
                                 2, 2, 2, 1)
        
        # Check metadata file
        meta_file = output_file.replace('.bts', '.meta')
        self.assertTrue(os.path.exists(meta_file))
        
        # Check contents
        with open(meta_file, 'r') as f:
            content = f.read()
            self.assertIn('Turbulence Model', content)
            self.assertIn('u_mean', content)
            self.assertIn('intensity_u', content)
            self.assertIn('z_hub', content)


class TestOpenFASTIntegration(unittest.TestCase):
    """Tests for OpenFAST compatibility."""
    
    def setUp(self):
        """Set up test fixtures."""
        sys.path.insert(0, "/tmp/workspace/hgopalan/massconsistent_amr/tools")
        from openfast_export import TurbSimBTSWriter
        self.TurbSimBTSWriter = TurbSimBTSWriter
        self.temp_dir = tempfile.mkdtemp()
    
    def tearDown(self):
        """Clean up test files."""
        import shutil
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)
    
    def test_physical_parameter_ranges(self):
        """Test that physical parameters are in reasonable ranges."""
        writer = self.TurbSimBTSWriter()
        
        # Test neutral atmosphere parameters
        writer.initialize(
            num_time_steps=10,
            nx=5, ny=5, nz=5,
            dt=0.1,
            u_mean=10.0,
            dx=10.0, dy=10.0, dz=10.0,
            z_hub=90.0,
            turbulence_intensity_u=0.14
        )
        
        # Check ranges
        self.assertGreater(writer.metadata.u_mean, 0)
        self.assertLess(writer.metadata.intensity_u, 1.0)
        self.assertGreater(writer.metadata.intensity_u, 0.05)
        self.assertGreater(writer.metadata.z_hub, 0)
        self.assertGreater(writer.metadata.dt, 0)


def run_tests():
    """Run all tests."""
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # Add test suites
    suite.addTests(loader.loadTestsFromTestCase(TestBTSFormat))
    suite.addTests(loader.loadTestsFromTestCase(TestOpenFASTIntegration))
    
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(run_tests())
