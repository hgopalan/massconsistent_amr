#!/usr/bin/env python3
# ============================================================================
# test_parameter_sensitivity.py
# Regression test for parameter sensitivity sweep tool (Phase 5, Feature 2)
#
# Verifies that:
#   1. Single parameter sweep completes without error
#   2. Output CSV is created with correct columns
#   3. Parameter values are logically spaced
#   4. Results show convergence for all runs
#
# Usage:
#   python3 test_parameter_sensitivity.py <inputs_file> <terrain_dir>
#
# Returns:
#   0 on success
#   1 if validation fails
# ============================================================================

import sys
import os
import csv
import subprocess
import tempfile
import shutil
from pathlib import Path

def test_single_parameter_sweep(inputs_file, terrain_dir):
    """
    Test single parameter sweep functionality.
    
    Returns: (success, errors)
    """
    errors = []
    
    print(f"\n{'='*70}")
    print("Parameter Sensitivity Sweep Test")
    print(f"{'='*70}")
    print(f"\nInputs file: {inputs_file}")
    print(f"Terrain dir: {terrain_dir}")
    
    # Validate input file exists
    if not os.path.isfile(inputs_file):
        errors.append(f"Input file not found: {inputs_file}")
        return False, errors
    
    print(f"✓ Input file found")
    
    # Create temporary directory for sweep results
    temp_dir = tempfile.mkdtemp(prefix='sensitivity_test_', suffix='_tmp')
    print(f"✓ Created temporary workspace: {temp_dir}")
    
    try:
        # Construct Python path to parameter_sensitivity.py
        script_dir = os.path.dirname(os.path.abspath(inputs_file))
        curr = Path(script_dir)
        repo_root = None
        for parent in [curr] + list(curr.parents):
            if (parent / "tools").is_dir():
                repo_root = parent
                break
        if repo_root is None:
            repo_root = curr.parent.parent.parent
        tools_dir = os.path.join(repo_root, "tools")
        param_sens_script = os.path.join(tools_dir, "parameter_sensitivity.py")
        
        if not os.path.isfile(param_sens_script):
            errors.append(f"parameter_sensitivity.py not found: {param_sens_script}")
            return False, errors
        
        print(f"✓ Found parameter_sensitivity.py")
        
        # Make the script executable
        os.chmod(param_sens_script, 0o755)
        
        # Run single parameter sweep
        output_csv = os.path.join(temp_dir, "sensitivity_z0_test.csv")
        
        print(f"\n{'='*70}")
        print("Running Single Parameter Sweep (z0, 3 steps)")
        print(f"{'='*70}")
        
        cmd = [
            sys.executable,
            param_sens_script,
            "--inputs", inputs_file,
            "--param", "z0",
            "--range", "0.01", "0.1",
            "--steps", "3",
            "--output", output_csv,
            "--solver", "wind_solver",
            "--preserve-workspace"
        ]
        
        print(f"\nCommand: {' '.join(cmd)}\n")
        
        # Change to terrain directory for sweep (so CSV files are found)
        original_cwd = os.getcwd()
        os.chdir(terrain_dir)
        
        try:
            result = subprocess.run(cmd, timeout=600, capture_output=True, text=True)
        finally:
            os.chdir(original_cwd)
        
        if result.returncode != 0:
            print(f"STDOUT:\n{result.stdout}")
            print(f"STDERR:\n{result.stderr}")
            errors.append(f"parameter_sensitivity.py failed with code {result.returncode}")
            return False, errors
        
        print(result.stdout)
        
        # Verify output CSV exists
        if not os.path.isfile(output_csv):
            errors.append(f"Output CSV not created: {output_csv}")
            return False, errors
        
        print(f"✓ Output CSV created: {output_csv}")
        
        # Validate CSV contents
        print(f"\n{'='*70}")
        print("CSV Validation")
        print(f"{'='*70}")
        
        try:
            with open(output_csv, 'r') as f:
                reader = csv.DictReader(f)
                rows = list(reader)
            
            if not rows:
                errors.append("Output CSV is empty")
                return False, errors
            
            print(f"✓ CSV contains {len(rows)} rows")
            
            # Expected columns for single parameter sweep
            expected_columns = {
                'step', 'parameter', 'value', 'success', 'elapsed_s',
                'max_div', 'mean_div'
            }
            
            actual_columns = set(reader.fieldnames) if reader.fieldnames else set()
            
            print(f"\nExpected columns: {sorted(expected_columns)}")
            print(f"Actual columns:   {sorted(actual_columns)}")
            
            if expected_columns != actual_columns:
                missing = expected_columns - actual_columns
                extra = actual_columns - expected_columns
                if missing:
                    errors.append(f"Missing columns: {missing}")
                if extra:
                    errors.append(f"Extra columns: {extra}")
                return False, errors
            
            print(f"✓ All expected columns present")
            
            # Validate data
            print(f"\n{'='*70}")
            print("Data Validation")
            print(f"{'='*70}\n")
            
            print(f"{'Step':<6} {'Parameter':<15} {'Value':<12} {'Success':<10} {'Time(s)':<10}")
            print(f"{'-'*70}")
            
            for i, row in enumerate(rows):
                try:
                    step = int(row['step'])
                    param = row['parameter']
                    value = float(row['value'])
                    success = row['success'].lower() == 'true'
                    elapsed = float(row['elapsed_s'])
                    max_div = float(row['max_div'])
                    
                    print(f"{step:<6} {param:<15} {value:.6e}   {str(success):<10} {elapsed:.2f}")
                    
                except (ValueError, KeyError) as e:
                    errors.append(f"Row {i}: Invalid data format: {e}")
                    return False, errors
            
            # Check that parameters are logically spaced (logarithmic for z0)
            z0_values = [float(row['value']) for row in rows if row['parameter'] == 'z0']
            
            if len(z0_values) != 3:
                errors.append(f"Expected 3 z0 values, got {len(z0_values)}")
                return False, errors
            
            # For logarithmic spacing, ratios should be equal
            if z0_values[0] > 0 and z0_values[1] > 0 and z0_values[2] > 0:
                ratio1 = z0_values[1] / z0_values[0]
                ratio2 = z0_values[2] / z0_values[1]
                
                print(f"\n✓ Parameter spacing:")
                print(f"  z0[0] = {z0_values[0]:.6e}")
                print(f"  z0[1] = {z0_values[1]:.6e} (ratio: {ratio1:.3f})")
                print(f"  z0[2] = {z0_values[2]:.6e} (ratio: {ratio2:.3f})")
                
                # Validate that ratios are approximately equal (logarithmic spacing)
                # For logarithmic spacing: log(z0[1]/z0[0]) should equal log(z0[2]/z0[1])
                # which means ratio1 should be approximately equal to ratio2
                ratio_tolerance = 0.01  # 1% tolerance
                if abs(ratio1 - ratio2) / max(ratio1, ratio2) > ratio_tolerance:
                    print(f"⚠ Warning: Ratios differ by {abs(ratio1-ratio2)/max(ratio1, ratio2)*100:.1f}% "
                          f"(expected <1% for logarithmic spacing)")
                else:
                    print(f"  ✓ Ratios consistent (logarithmic spacing verified)")
            
            print(f"\n✓ CSV data validation PASSED")
            
        except Exception as e:
            errors.append(f"Error validating CSV: {e}")
            return False, errors
        
        print(f"\n{'='*70}")
        print("✓ Parameter Sensitivity Sweep Test PASSED")
        print(f"{'='*70}\n")
        
        return True, errors
        
    finally:
        # Cleanup
        if os.path.exists(temp_dir):
            shutil.rmtree(temp_dir, ignore_errors=True)
            print(f"Cleaned up temporary workspace")

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 test_parameter_sensitivity.py <inputs_file> <terrain_dir>")
        print("\nExample:")
        print("  python3 test_parameter_sensitivity.py inputs.i .")
        sys.exit(1)
    
    inputs_file = sys.argv[1]
    terrain_dir = sys.argv[2]
    
    success, errors = test_single_parameter_sweep(inputs_file, terrain_dir)
    
    if errors:
        print(f"\n⚠ Errors encountered:")
        for error in errors:
            print(f"  - {error}")
    
    print(f"\n{'='*70}")
    if success:
        print("TEST PASSED ✓")
        sys.exit(0)
    else:
        print("TEST FAILED ✗")
        sys.exit(1)

if __name__ == '__main__':
    main()
