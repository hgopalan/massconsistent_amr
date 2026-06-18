#!/usr/bin/env python3
import os
import sys
import subprocess
import unittest
import numpy as np

class TestMultispeciesCalpuff(unittest.TestCase):
    def setUp(self):
        # Set up paths
        self.script_dir = os.path.dirname(os.path.abspath(__file__))
        self.repo_dir = self.script_dir
        for _ in range(5):
            # Try different possible locations for the executable
            build_dir = os.path.join(self.repo_dir, "build")
            
            # Check for platform-specific executable in config subdirectories (Windows multi-config)
            for config in ["Debug", "Release"]:
                exe_candidate = os.path.join(build_dir, config, "puff_solver.exe")
                if os.path.exists(exe_candidate):
                    self.exe_path = exe_candidate
                    break
            else:
                # Check for Unix-style executable in build root
                exe_candidate = os.path.join(build_dir, "puff_solver")
                if os.path.exists(exe_candidate):
                    self.exe_path = exe_candidate
                else:
                    # Try .exe on Windows in build root
                    exe_candidate = os.path.join(build_dir, "puff_solver.exe")
                    if os.path.exists(exe_candidate):
                        self.exe_path = exe_candidate
                    else:
                        self.exe_path = None
            
            if self.exe_path is not None:
                break
            self.repo_dir = os.path.dirname(self.repo_dir)
            
        self.inputs_base = os.path.join(self.script_dir, "inputs.i")
        self.receptors_file = os.path.join(self.script_dir, "receptors.csv")
        self.test_work_dir = self.script_dir
        
        # Verify executable exists
        self.assertTrue(self.exe_path is not None and os.path.exists(self.exe_path), f"puff_solver not found in build directories")

    def run_puff_solver(self, config_updates):
        inputs_file = os.path.join(self.script_dir, "temp_inputs.i")
        
        # Read base inputs
        with open(self.inputs_base, "r") as f:
            lines = f.readlines()
            
        # Filter out lines we are overriding
        filtered_lines = []
        for line in lines:
            line_strip = line.strip()
            if not line_strip or line_strip.startswith("#"):
                filtered_lines.append(line)
                continue
            key = line_strip.split("=")[0].strip()
            if key not in config_updates:
                filtered_lines.append(line)
                
        # Append overrides
        for k, v in config_updates.items():
            filtered_lines.append(f"{k} = {v}\n")
            
        # Write temporary inputs
        with open(inputs_file, "w") as f:
            f.writelines(filtered_lines)
            
        # Run solver
        cmd = [self.exe_path, inputs_file]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=self.script_dir)
        
        # Clean up temporary inputs
        if os.path.exists(inputs_file):
            os.remove(inputs_file)
            
        return res

    def test_multispecies_chemistry_receptors(self):
        """Test multi-species puff model with chemistry, deposition, receptors, and visibility."""
        puff_out = os.path.join(self.script_dir, "test_multispecies_grid.csv")
        rec_out = os.path.join(self.script_dir, "test_multispecies_rec.csv")
        
        config = {
            "enable_chemistry": "true",
            "enable_puff_deposition": "true",
            "enable_wet_deposition": "true",
            "precipitation_rate": "2.5",
            "is_snow": "false",
            "enable_visibility": "true",
            "ambient_rh": "75.0",
            "ambient_temp": "285.0",
            "receptor_file": self.receptors_file,
            "receptor_output": rec_out,
            "puff_output": puff_out,
            "n_steps_puff": "10",
            "output_freq_puff": "5"
        }
        
        res = self.run_puff_solver(config)
        self.assertEqual(res.returncode, 0, f"puff_solver failed: {res.stderr}\nOutput: {res.stdout}")
        
        # Check grid output files are created and have correct columns
        grid_step5 = puff_out + "_step5"
        self.assertTrue(os.path.exists(grid_step5), f"Grid output {grid_step5} not found")
        
        with open(grid_step5, "r") as f:
            header = f.readline().strip()
            columns_line = f.readline().strip()
            
        self.assertIn("C_total", columns_line)
        self.assertIn("SO2", columns_line)
        self.assertIn("Sulfate", columns_line)
        self.assertIn("NOx", columns_line)
        self.assertIn("HNO3", columns_line)
        self.assertIn("Nitrate", columns_line)
        
        # Check receptors output files are created and have correct columns
        rec_step5 = rec_out + "_step5"
        self.assertTrue(os.path.exists(rec_step5), f"Receptor output {rec_step5} not found")
        
        with open(rec_step5, "r") as f:
            header = f.readline().strip()
            columns_line = f.readline().strip()
            
        self.assertIn("C_total", columns_line)
        self.assertIn("SO2", columns_line)
        self.assertIn("Sulfate", columns_line)
        self.assertIn("NOx", columns_line)
        self.assertIn("HNO3", columns_line)
        self.assertIn("Nitrate", columns_line)
        self.assertIn("b_ext", columns_line)
        self.assertIn("visual_range", columns_line)
        self.assertIn("deciview", columns_line)
        self.assertIn("fog_prob", columns_line)
        self.assertIn("icing_prob", columns_line)
        
        # Clean up output files
        for step in [0, 5, 10]:
            gf = f"{puff_out}_step{step}"
            rf = f"{rec_out}_step{step}"
            if os.path.exists(gf): os.remove(gf)
            if os.path.exists(rf): os.remove(rf)

    def test_line_source_emissions(self):
        """Test line source puff generation and transport."""
        puff_out = os.path.join(self.script_dir, "test_line_grid.csv")
        config = {
            "source_type": "line",
            "line_start_x": "50.0",
            "line_start_y": "150.0",
            "line_start_z": "10.0",
            "line_end_x": "150.0",
            "line_end_y": "150.0",
            "line_end_z": "20.0",
            "num_line_segments": "4",
            "puff_output": puff_out,
            "n_steps_puff": "2",
            "output_freq_puff": "1"
        }
        
        res = self.run_puff_solver(config)
        self.assertEqual(res.returncode, 0, f"puff_solver failed: {res.stderr}\nOutput: {res.stdout}")
        
        # Verify step 1 has grid output
        grid_step1 = puff_out + "_step1"
        self.assertTrue(os.path.exists(grid_step1), f"Grid output {grid_step1} not found")
        
        # Clean up
        for step in [0, 1, 2]:
            gf = f"{puff_out}_step{step}"
            if os.path.exists(gf): os.remove(gf)

    def test_area_source_lpdm(self):
        """Test area source with LPDM."""
        puff_out = os.path.join(self.script_dir, "test_area_lpdm_grid.csv")
        config = {
            "enable_lpdm": "true",
            "particles_per_step": "50",
            "source_type": "area",
            "area_xmin": "50.0",
            "area_xmax": "150.0",
            "area_ymin": "100.0",
            "area_ymax": "200.0",
            "area_z": "15.0",
            "puff_output": puff_out,
            "n_steps_puff": "2",
            "output_freq_puff": "1"
        }
        
        res = self.run_puff_solver(config)
        self.assertEqual(res.returncode, 0, f"puff_solver failed: {res.stderr}\nOutput: {res.stdout}")
        
        # Verify step 1 has grid output
        grid_step1 = puff_out + "_step1"
        self.assertTrue(os.path.exists(grid_step1), f"Grid output {grid_step1} not found")
        
        # Clean up
        for step in [0, 1, 2]:
            gf = f"{puff_out}_step{step}"
            if os.path.exists(gf): os.remove(gf)

    def test_volume_source(self):
        """Test volume source with Gaussian puff."""
        puff_out = os.path.join(self.script_dir, "test_volume_grid.csv")
        config = {
            "source_type": "volume",
            "volume_xmin": "50.0",
            "volume_xmax": "100.0",
            "volume_ymin": "50.0",
            "volume_ymax": "100.0",
            "volume_zmin": "5.0",
            "volume_zmax": "25.0",
            "num_volume_puffs_x": "2",
            "num_volume_puffs_y": "2",
            "num_volume_puffs_z": "2",
            "puff_output": puff_out,
            "n_steps_puff": "2",
            "output_freq_puff": "1"
        }
        
        res = self.run_puff_solver(config)
        self.assertEqual(res.returncode, 0, f"puff_solver failed: {res.stderr}\nOutput: {res.stdout}")
        
        # Verify step 1 has grid output
        grid_step1 = puff_out + "_step1"
        self.assertTrue(os.path.exists(grid_step1), f"Grid output {grid_step1} not found")
        
        # Clean up
        for step in [0, 1, 2]:
            gf = f"{puff_out}_step{step}"
            if os.path.exists(gf): os.remove(gf)

if __name__ == "__main__":
    import sys
    unittest.main(argv=[sys.argv[0]])
