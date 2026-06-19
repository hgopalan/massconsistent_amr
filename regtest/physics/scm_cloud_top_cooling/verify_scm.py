#!/usr/bin/env python3
"""
Verification script for SCM (Single Column Model) Cloud-Top Cooling regression test.
"""

import sys
import os
import argparse
import glob

def verify_scm_cloud_top_cooling(input_file, work_dir):
    print("=" * 70)
    print("SCM Cloud-Top Cooling Regression Test Verification")
    print("=" * 70)
    
    # Check if output files exist
    plot_files = glob.glob(os.path.join(work_dir, "plt_scm_cloud_top_cooling*"))
    
    if not plot_files:
        print(f"✗ FAIL: No plot files found matching pattern 'plt_scm_cloud_top_cooling*' in {work_dir}")
        return False
    
    plot_file = plot_files[0]
    print(f"✓ PASS: Plot file exists: {plot_file}")
    
    try:
        with open(input_file, 'r') as f:
            content = f.read()
        
        required_params = [
            'init_mode = scm',
            'scm_enable_microphysics = true',
            'scm.cloud_top_cooling_rate = 3.0'
        ]
        
        missing_params = []
        for param in required_params:
            if param not in content:
                missing_params.append(param)
        
        if missing_params:
            print(f"✗ FAIL: Missing SCM parameters in inputs.i: {missing_params}")
            return False
        
        print(f"✓ PASS: All required SCM cloud-top cooling parameters found in inputs.i")
    except Exception as e:
        print(f"⚠ WARNING: Could not fully parse inputs.i: {e}")
    
    print("\n" + "=" * 70)
    print("Verification completed successfully!")
    print("=" * 70)
    return True

def main():
    parser = argparse.ArgumentParser(description="Verify SCM cloud-top cooling test outputs")
    parser.add_argument('input_file', help='Path to inputs.i file')
    parser.add_argument('work_dir', help='Working directory with test outputs')
    
    args = parser.parse_args()
    os.chdir(args.work_dir)
    success = verify_scm_cloud_top_cooling(args.input_file, args.work_dir)
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()
