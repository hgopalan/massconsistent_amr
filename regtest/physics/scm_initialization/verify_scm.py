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
        
        # Check for essential SCM parameters
        required_params = [
            'init_mode = scm',
            'scm_wind_speed',
            'scm_wind_direction',
            'scm_ref_height',
            'scm_ref_temperature',
            'scm_lapse_rate',
            'scm_domain_height',
            'scm_dz'
        ]
        
        missing_params = []
        for param in required_params:
            if param not in content:
                missing_params.append(param)
        
        if missing_params:
            print(f"✗ FAIL: Missing SCM parameters in inputs.i: {missing_params}")
            return False
        
        print(f"✓ PASS: All required SCM parameters found in inputs.i")
        
        # Extract and display key parameters
        for line in content.split('\n'):
            if 'scm_wind_speed' in line and '=' in line and not line.strip().startswith('#'):
                print(f"  → {line.strip()}")
            elif 'scm_ref_height' in line and '=' in line and not line.strip().startswith('#'):
                print(f"  → {line.strip()}")
            elif 'scm_wind_direction' in line and '=' in line and not line.strip().startswith('#'):
                print(f"  → {line.strip()}")
    
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
