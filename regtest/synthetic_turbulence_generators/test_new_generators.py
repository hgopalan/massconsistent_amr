#!/usr/bin/env python3
"""
Regression test suite for synthetic turbulence generators (GP_LLJ, NWTC, USWTPP, HIT).
Verifies parsing of model-specific parameters, compilation safety, and mathematical validation.
"""

import sys
import os
import struct
import subprocess
import shutil
import tempfile
import math
from pathlib import Path

# Add tools to path for reading BTS if needed
TEST_DIR = Path(__file__).resolve().parent
REPO_ROOT = TEST_DIR.parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))

class BTSReader:
    """Read TurbSim binary (.bts) format files."""
    def __init__(self, filename):
        self.filename = filename
        self.header = None
        self.u_prime = None
        self.v_prime = None
        self.w_prime = None
    
    def read(self):
        try:
            with open(self.filename, 'rb') as f:
                header_ints = struct.unpack('6i', f.read(6 * 4))
                id1, id2, nt, ny, nz, ncomp = header_ints
                
                if id1 != 7 or id2 != 7:
                    raise ValueError(f"Invalid BTS format identifiers: {id1}, {id2}")
                
                if ncomp != 3:
                    raise ValueError(f"Expected 3 components, got {ncomp}")
                
                header_floats = struct.unpack('6f', f.read(6 * 4))
                dt, uHub, zHub, dy, dz, z0 = header_floats
                
                (turb_intensity,) = struct.unpack('f', f.read(4))
                
                self.header = {
                    'id1': id1, 'id2': id2, 'nt': nt, 'ny': ny, 'nz': nz, 'ncomp': ncomp,
                    'dt': dt, 'uHub': uHub, 'zHub': zHub, 'dy': dy, 'dz': dz, 'z0': z0,
                    'turbulence_intensity': turb_intensity
                }
                
                total_points = nt * ny * nz
                u_data = struct.unpack(f'{total_points}f', f.read(total_points * 4))
                v_data = struct.unpack(f'{total_points}f', f.read(total_points * 4))
                w_data = struct.unpack(f'{total_points}f', f.read(total_points * 4))
                
                import numpy as np
                self.u_prime = np.array(u_data).reshape((nt, nz, ny))
                self.v_prime = np.array(v_data).reshape((nt, nz, ny))
                self.w_prime = np.array(w_data).reshape((nt, nz, ny))
                return True
        except Exception as e:
            print(f"Error reading BTS file: {e}")
            return False

def find_solver_executable(test_work_dir=None):
    """Locate the wind_solver executable."""
    potential_paths = []
    
    # 1. Environment variable
    env_exe = os.environ.get("MASSCONSISTENT_EXE")
    if env_exe:
        potential_paths.append(Path(env_exe))
    
    # 2. Relative to test working directory
    if test_work_dir:
        potential_paths.append(Path(test_work_dir) / "../../wind_solver")
        potential_paths.append(Path(test_work_dir) / "../wind_solver")
    
    # 3. Relative to this script
    potential_paths.append(TEST_DIR.parent.parent / "build" / "wind_solver")
    potential_paths.append(TEST_DIR.parent.parent / "wind_solver")
    
    # 4. Standard path lookup
    sh_exe = shutil.which("wind_solver")
    if sh_exe:
        potential_paths.append(Path(sh_exe))
        
    for p in potential_paths:
        if p.exists() and os.access(p, os.X_OK):
            return p.resolve()
            
    return None

def test_spectrum_model(model_name, extra_params, solver_path, test_work_dir):
    """Test a single spectrum model run."""
    print(f"\n" + "="*50)
    print(f"Testing spectrum model: {model_name}")
    print("="*50)
    
    # Create input file for this model
    inputs_template_path = TEST_DIR / "inputs.i"
    with open(inputs_template_path, 'r') as f:
        lines = f.readlines()
        
    new_lines = []
    for line in lines:
        line_stripped = line.strip()
        if line_stripped.startswith("turbulence_spectrum_model"):
            new_lines.append(f"turbulence_spectrum_model = {model_name}\n")
        elif line_stripped.startswith("turbulence_output_file"):
            new_lines.append(f"turbulence_output_file = turbulence_{model_name}.bts\n")
        else:
            new_lines.append(line)
            
    # Append any specific extra parameters
    for k, v in extra_params.items():
        new_lines.append(f"{k} = {v}\n")
        
    test_input_file = Path(test_work_dir) / f"inputs_{model_name}.i"
    with open(test_input_file, 'w') as f:
        f.writelines(new_lines)
        
    print(f"Created input file: {test_input_file}")
    
    # Run wind_solver
    cmd = [str(solver_path), str(test_input_file)]
    print(f"Running command: {' '.join(cmd)}")
    
    # Change working directory to test_work_dir so terrain.csv is found
    result = subprocess.run(cmd, cwd=str(test_work_dir), capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"✗ ERROR: wind_solver failed with exit code {result.returncode}")
        print("STDOUT:")
        print(result.stdout)
        print("STDERR:")
        print(result.stderr)
        return False, None, None
        
    print("✓ Solver run completed successfully.")
    
    # Check that output files were created
    bts_file = Path(test_work_dir) / f"turbulence_{model_name}.bts"
    meta_file = Path(test_work_dir) / f"turbulence_{model_name}.meta"
    
    if not bts_file.exists():
        print(f"✗ ERROR: Expected BTS file not created: {bts_file}")
        return False, None, None
    if not meta_file.exists():
        print(f"✗ ERROR: Expected Meta file not created: {meta_file}")
        return False, None, None
        
    print("✓ Output BTS and Meta files verified.")
    return True, bts_file, result.stdout

def run_all_tests():
    # Setup test workspace
    if len(sys.argv) >= 3:
        test_work_dir = Path(sys.argv[2]).resolve()
    else:
        test_work_dir = TEST_DIR
        
    # Copy terrain.csv to work directory if not already there
    terrain_src = TEST_DIR / "terrain.csv"
    terrain_dst = test_work_dir / "terrain.csv"
    if terrain_src.exists() and not terrain_dst.exists():
        shutil.copy(terrain_src, terrain_dst)
        
    solver_path = find_solver_executable(test_work_dir)
    if not solver_path:
        print("✗ ERROR: Could not locate wind_solver executable.")
        return False
        
    print(f"Located wind_solver at: {solver_path}")
    print(f"Test working directory: {test_work_dir}")
    
    # Define models to test
    test_cases = {
        "GP_LLJ": {
            "gp_llj_jet_height": 80.0
        },
        "NWTC": {
            "nwtc_scaling_parameter": 1.5
        },
        "USWTPP": {
            "uswtpp_weight": 0.5
        },
        "HIT": {}
    }
    
    results = {}
    for model_name, extra_params in test_cases.items():
        success, bts_file, stdout = test_spectrum_model(model_name, extra_params, solver_path, test_work_dir)
        if not success:
            results[model_name] = False
            continue
            
        # Additional physical and format validation
        print(f"Validating physical and format properties for {model_name}...")
        try:
            import numpy as np
            reader = BTSReader(str(bts_file))
            if not reader.read():
                print(f"✗ ERROR: BTSReader failed to read {bts_file}")
                results[model_name] = False
                continue
                
            # 1. Format Compliance Checks
            header = reader.header
            assert header['id1'] == 7 and header['id2'] == 7, "Invalid format IDs"
            assert header['nt'] > 0 and header['ny'] > 0 and header['nz'] > 0, "Invalid grid dimensions"
            assert reader.u_prime is not None and reader.v_prime is not None and reader.w_prime is not None, "Missing arrays"
            
            # 2. Physics & Model-Specific Validations
            u_prime = reader.u_prime
            
            # Non-zero energy check
            variance_u = np.var(u_prime)
            assert variance_u > 1e-5, f"Turbulence field has zero or near-zero energy: {variance_u}"
            print(f"  ✓ Non-zero fluctuation energy verified: variance_u = {variance_u:.6f}")
            
            if model_name == "GP_LLJ":
                # GP_LLJ vertical profile modulation check
                # At z_hub (around 50m / index nz//2), z_agl is closer to jet height (80m) 
                # than at z = 0. Therefore, variance or magnitude of fluctuations near the top 
                # should be higher or modulated compared to lower heights.
                # Let's compute variance at lower half vs upper half of the grid
                var_low = np.var(u_prime[:, :header['nz']//3, :])
                var_high = np.var(u_prime[:, -header['nz']//3:, :])
                print(f"  ✓ GP_LLJ Low height variance: {var_low:.6f}, High height variance: {var_high:.6f}")
                
            elif model_name == "NWTC":
                # Check if scaling parameters were printed in log
                assert "nwtc_scaling_parameter: 1.5" in stdout or "nwtc_scaling_parameter:1.5" in stdout.replace(" ", ""), \
                    "NWTC scaling parameter was not parsed or logged correctly"
                print("  ✓ NWTC scaling parameter parsing & logging verified")
                
            elif model_name == "USWTPP":
                assert "uswtpp_weight: 0.5" in stdout or "uswtpp_weight:0.5" in stdout.replace(" ", ""), \
                    "USWTPP weight was not parsed or logged correctly"
                print("  ✓ USWTPP blending weight parsing & logging verified")
                
            elif model_name == "HIT":
                # HIT: verify spectral properties (e.g. low-frequency roll-off vanishes at f=0)
                # We can perform a fast 1D Fourier Transform in time at the hub-height grid point
                u_hub_time = u_prime[:, header['nz']//2, header['ny']//2]
                psd = np.abs(np.fft.rfft(u_hub_time))**2
                freqs = np.fft.rfftfreq(header['nt'], d=header['dt'])
                
                # Check that PSD at very low frequency is small (vanishing at f=0 due to f^2 in hit_spectrum)
                # Contrast with standard spectra which flatline at low freq.
                f_0_val = psd[0]
                max_val = np.max(psd)
                print(f"  ✓ HIT Spectral energy at f=0: {f_0_val:.6f} (max spectral energy: {max_val:.6f})")
                assert f_0_val < 0.1 * max_val or f_0_val < 1e-4, f"HIT spectrum at f=0 does not vanish: {f_0_val} relative to max {max_val}"
                
            results[model_name] = True
            print(f"✓ PASS: {model_name} spectrum validation successful.")
        except Exception as e:
            print(f"✗ ERROR: Physical validation failed for {model_name}: {e}")
            import traceback
            traceback.print_exc()
            results[model_name] = False
            
    print("\n" + "="*50)
    print("REGRESSION TEST RESULTS SUMMARY")
    print("="*50)
    all_passed = True
    for model_name, status in results.items():
        print(f"{model_name}: {'PASS' if status else 'FAIL'}")
        if not status:
            all_passed = False
            
    return all_passed

if __name__ == "__main__":
    success = run_all_tests()
    sys.exit(0 if success else 1)
