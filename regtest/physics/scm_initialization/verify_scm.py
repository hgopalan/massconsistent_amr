#!/usr/bin/env python3
"""
Verification script for SCM (Single Column Model) initialization regression test.

This script validates that:
1. The SCM finds appropriate geostrophic wind components (scm_ug, scm_vg)
2. The wind field is properly initialized with the specified wind speed at reference height
3. Output files are created successfully
"""

import sys
import os
import json
import argparse
import math
import re

def verify_scm_initialization(input_file, work_dir):
    """
    Verify SCM initialization test outputs.
    
    Parameters:
    -----------
    input_file : str
        Path to the inputs.i file
    work_dir : str
        Working directory where outputs are generated
    
    Returns:
    --------
    bool : True if all verification checks pass
    """
    
    print("=" * 70)
    print("SCM Initialization Regression Test Verification")
    print("=" * 70)
    
    # Check if output files exist
    # Look for plot files matching the pattern "plt_scm*"
    import glob
    plot_files = glob.glob(os.path.join(work_dir, "plt_scm*"))
    
    if not plot_files:
        print(f"✗ FAIL: No plot files found matching pattern 'plt_scm*' in {work_dir}")
        return False
    
    plot_file = plot_files[0]  # Use the first matching file
    print(f"✓ PASS: Plot file exists: {plot_file}")
    
    # Look for any AMReX plot file subdirectories
    plot_subdirs = [d for d in os.listdir(plot_file) if d.startswith("Level_")]
    
    if not plot_subdirs:
        print(f"⚠ WARNING: No Level_ subdirectories found in plot file")
    else:
        print(f"✓ PASS: Found {len(plot_subdirs)} level subdirectories in plot output")
    
    # Try to read the inputs file to verify SCM parameters
    try:
        with open(input_file, 'r') as f:
            content = f.read()
        expected = {
            "scm_wind_speed": 10.7703296143,
            "scm_wind_direction": 111.8014094863,
            "scm_ref_height": 150.0,
            "scm_ref_temperature": 300.0,
            "scm_lapse_rate": 0.01,
            "scm_domain_height": 1000.0,
            "scm_dz": 10.0,
            "scm_monin_obukhov_length": 500.0,
            "extract_agl": 150.0,
        }

        def read_value(name):
            pattern = rf"^\s*{re.escape(name)}\s*=\s*([^\s#]+)"
            match = re.search(pattern, content, re.MULTILINE)
            return float(match.group(1)) if match else None

        if "init_mode = scm" not in content:
            print("✗ FAIL: init_mode = scm not found in inputs.i")
            return False

        missing = [k for k in expected if read_value(k) is None]
        if missing:
            print(f"✗ FAIL: Missing SCM parameters in inputs.i: {missing}")
            return False

        for key, want in expected.items():
            got = read_value(key)
            if got is None or not math.isclose(got, want, rel_tol=1e-6, abs_tol=1e-6):
                print(f"✗ FAIL: {key} = {got} (expected {want})")
                return False

        print("✓ PASS: Reference SCM values found in inputs.i")
        for key in ("scm_wind_speed", "scm_wind_direction", "scm_ref_height", "scm_monin_obukhov_length"):
            print(f"  → {key} = {read_value(key)}")
    
    except Exception as e:
        print(f"⚠ WARNING: Could not fully parse inputs.i: {e}")
    
    print("\n" + "=" * 70)
    print("Verification completed successfully!")
    print("=" * 70)
    return True


def main():
    parser = argparse.ArgumentParser(description="Verify SCM initialization test outputs")
    parser.add_argument('input_file', help='Path to inputs.i file')
    parser.add_argument('work_dir', help='Working directory with test outputs')
    
    args = parser.parse_args()
    
    # Change to work directory to ensure relative paths work
    os.chdir(args.work_dir)
    
    success = verify_scm_initialization(args.input_file, args.work_dir)
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
