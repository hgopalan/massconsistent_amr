#!/usr/bin/env python3
"""
Validation script for general structure loading regression test.

This script verifies that the structure loading module:
1. Correctly computes base shear forces for different structure types
2. Properly applies dynamic amplification (gust response factors)
3. Accurately assesses damage states based on wind loading
4. Handles geometry consistency and physical bounds
"""

import csv
import sys
import math
import numpy as np

def validate_structure_output(output_file):
    """
    Validate structure loading output.
    
    Returns:
        (pass_count, fail_count, error_msg)
    """
    pass_count = 0
    fail_count = 0
    errors = []
    
    try:
        with open(output_file, 'r') as f:
            reader = csv.DictReader(f)
            structures = list(reader)
    except FileNotFoundError:
        return 0, 1, f"Output file not found: {output_file}"
    except Exception as e:
        return 0, 1, f"Error reading output file: {e}"
    
    if not structures:
        return 0, 1, "No structures in output file"
    
    print(f"Validating {len(structures)} structures...")
    
    for s in structures:
        try:
            # Convert to float
            struct_id = int(s['structure_id'])
            height = float(s['height'])
            width = float(s['width'])
            depth = float(s['depth'])
            mass = float(s['mass'])
            max_wind = float(s['max_wind_speed'])
            base_shear_static = float(s['base_shear_static'])
            base_shear_dynamic = float(s['base_shear_dynamic'])
            overturning_moment = float(s['overturning_moment'])
            max_deflection = float(s['max_deflection'])
            stress_ratio = float(s['stress_ratio'])
            damage_ratio = float(s['damage_ratio'])
            damage_state = s['damage_state']
            
            # Test 1: Geometry consistency
            if height <= 0 or width <= 0 or depth <= 0:
                errors.append(f"Structure {struct_id}: Invalid geometry (H={height}, W={width}, D={depth})")
                fail_count += 1
            else:
                pass_count += 1
            
            # Test 2: Mass should be positive
            if mass <= 0:
                errors.append(f"Structure {struct_id}: Mass must be positive (M={mass})")
                fail_count += 1
            else:
                pass_count += 1
            
            # Test 3: Wind speed should be non-negative
            if max_wind < 0:
                errors.append(f"Structure {struct_id}: Wind speed cannot be negative ({max_wind})")
                fail_count += 1
            else:
                pass_count += 1
            
            # Test 4: Dynamic shear should be >= static shear (gust amplification)
            if base_shear_dynamic < base_shear_static - 1e-6:
                errors.append(f"Structure {struct_id}: Dynamic shear < static shear ({base_shear_dynamic} < {base_shear_static})")
                fail_count += 1
            else:
                pass_count += 1
            
            # Test 5: Overturning moment should be positive when wind is present
            if max_wind > 1.0 and overturning_moment < 0:
                errors.append(f"Structure {struct_id}: Negative moment with wind ({overturning_moment})")
                fail_count += 1
            else:
                pass_count += 1
            
            # Test 6: Deflection should be non-negative
            if max_deflection < 0:
                errors.append(f"Structure {struct_id}: Negative deflection ({max_deflection})")
                fail_count += 1
            else:
                pass_count += 1
            
            # Test 7: Stress ratio should be non-negative
            if stress_ratio < 0:
                errors.append(f"Structure {struct_id}: Negative stress ratio ({stress_ratio})")
                fail_count += 1
            else:
                pass_count += 1
            
            # Test 8: Damage ratio should be in [0, 1]
            if damage_ratio < 0 or damage_ratio > 1:
                errors.append(f"Structure {struct_id}: Damage ratio out of bounds ({damage_ratio})")
                fail_count += 1
            else:
                pass_count += 1
            
            # Test 9: Damage state should be valid
            valid_states = ['NONE', 'MINOR', 'MODERATE', 'SEVERE', 'DESTRUCTION']
            if damage_state not in valid_states:
                errors.append(f"Structure {struct_id}: Invalid damage state ({damage_state})")
                fail_count += 1
            else:
                pass_count += 1
            
            # Test 10: Damage state should correspond to damage ratio
            if damage_ratio < 0.1 and damage_state != 'NONE':
                errors.append(f"Structure {struct_id}: Damage state mismatch (ratio={damage_ratio}, state={damage_state})")
                fail_count += 1
            elif 0.1 <= damage_ratio < 0.3 and damage_state != 'MINOR':
                errors.append(f"Structure {struct_id}: Damage state mismatch (ratio={damage_ratio}, state={damage_state})")
                fail_count += 1
            elif 0.3 <= damage_ratio < 0.6 and damage_state != 'MODERATE':
                errors.append(f"Structure {struct_id}: Damage state mismatch (ratio={damage_ratio}, state={damage_state})")
                fail_count += 1
            elif 0.6 <= damage_ratio < 0.9 and damage_state != 'SEVERE':
                errors.append(f"Structure {struct_id}: Damage state mismatch (ratio={damage_ratio}, state={damage_state})")
                fail_count += 1
            elif damage_ratio >= 0.9 and damage_state != 'DESTRUCTION':
                errors.append(f"Structure {struct_id}: Damage state mismatch (ratio={damage_ratio}, state={damage_state})")
                fail_count += 1
            else:
                pass_count += 1
            
        except (ValueError, KeyError) as e:
            errors.append(f"Structure parsing error: {e}")
            fail_count += 1
    
    return pass_count, fail_count, errors

def main():
    if len(sys.argv) < 2:
        output_file = "structure_output.csv"
    else:
        output_file = sys.argv[1]
    
    print(f"Validating structure loading output from: {output_file}")
    print("-" * 60)
    
    pass_count, fail_count, errors = validate_structure_output(output_file)
    
    print(f"\nValidation Results:")
    print(f"  Passed: {pass_count}")
    print(f"  Failed: {fail_count}")
    
    if errors:
        print(f"\nErrors:")
        for error in errors[:10]:  # Show first 10 errors
            print(f"  - {error}")
        if len(errors) > 10:
            print(f"  ... and {len(errors) - 10} more errors")
    
    if fail_count == 0:
        print("\n✓ All structure loading validations passed!")
        return 0
    else:
        print(f"\n✗ {fail_count} validations failed")
        return 1

if __name__ == '__main__':
    sys.exit(main())
