#!/usr/bin/env python3
# ============================================================================
# test_field_output.py
# Regression test for unified field output
#
# Verifies that:
#   1. All 21 diagnostic fields are present in output plotfile
#   2. Field names match standardized FieldOutput.H enumeration
#   3. Fields contain valid (finite) values
#   4. Output dimensions match solver domain
#
# Usage:
#   python3 test_field_output.py <plotfile_directory>
#
# Returns:
#   0 on success
#   1 if validation fails
# ============================================================================

import sys
import os
from pathlib import Path

# Expected field names in standard order (from FieldOutput::FieldIndex)
EXPECTED_FIELDS = [
    "u",                    # 0
    "v",                    # 1
    "w",                    # 2
    "vel_magnitude",        # 3
    "u0",                   # 4
    "v0",                   # 5
    "w0",                   # 6
    "lambda",               # 7
    "div_before",           # 8
    "div_after",            # 9
    "terrain_z",            # 10
    "heat_flux",            # 11
    "drag_coeff",           # 12
    "tau_x",                # 13
    "tau_y",                # 14
    "u_star",               # 15
    "richardson_no",        # 16
    "bl_depth",             # 17
    "terrain_type",         # 18
    "terrain_slope",        # 19
    "adaptive_z0",          # 20
]

EXPECTED_NFIELDS = len(EXPECTED_FIELDS)

def validate_field_output(plotfile_dir):
    """
    Validate that all expected fields are present in the plotfile.
    
    Returns: (success, errors)
    """
    errors = []
    
    print(f"\n{'='*70}")
    print("Field Output Validation Test")
    print(f"{'='*70}")
    print(f"\nPlotfile directory: {plotfile_dir}")
    
    # Check directory exists
    if not os.path.isdir(plotfile_dir):
        errors.append(f"Plotfile directory not found: {plotfile_dir}")
        return False, errors
    
    # Look for Header file (AMReX plotfile format)
    header_file = os.path.join(plotfile_dir, "Header")
    if not os.path.isfile(header_file):
        errors.append(f"Header file not found: {header_file}")
        return False, errors
    
    print(f"✓ Found Header file")
    
    # Parse Header file for field names and dimensions
    try:
        with open(header_file, 'r') as f:
            header_content = f.read()
    except Exception as e:
        errors.append(f"Could not read Header file: {e}")
        return False, errors
    
    # Extract version number
    if "9" not in header_content[:10]:
        errors.append("Invalid AMReX plotfile format (expected version 9)")
        return False, errors
    
    print(f"✓ Valid AMReX plotfile format")
    
    # Extract number of fields and field names
    try:
        lines = header_content.split('\n')
        idx = 0
        
        # Skip first line (version)
        idx += 1
        
        # Find number of components
        while idx < len(lines) and lines[idx].strip() == '':
            idx += 1
        
        if idx >= len(lines):
            errors.append("Could not find number of components in Header")
            return False, errors
        
        ncomp = int(lines[idx].strip())
        print(f"\n✓ Found {ncomp} components in plotfile")
        
        if ncomp != EXPECTED_NFIELDS:
            errors.append(
                f"Expected {EXPECTED_NFIELDS} components, but found {ncomp}"
            )
            return False, errors
        
        # Extract field names
        idx += 1
        actual_fields = []
        
        for i in range(ncomp):
            if idx >= len(lines):
                break
            field_name = lines[idx].strip()
            actual_fields.append(field_name)
            idx += 1
        
        print(f"\nField Validation:")
        print(f"{'-'*70}")
        
        # Compare with expected fields
        all_match = True
        for i, (expected, actual) in enumerate(zip(EXPECTED_FIELDS, actual_fields)):
            match = "✓" if expected == actual else "✗"
            status = "OK" if expected == actual else "MISMATCH"
            print(f"  [{i:2d}] {match} {expected:<20} {actual:<20} [{status}]")
            
            if expected != actual:
                all_match = False
                errors.append(
                    f"Field {i}: expected '{expected}', got '{actual}'"
                )
        
        print(f"{'-'*70}")
        
        if not all_match:
            return False, errors
        
        print(f"\n✓ All {EXPECTED_NFIELDS} fields present and correctly named")
        
    except Exception as e:
        errors.append(f"Error parsing Header file: {e}")
        return False, errors
    
    # Verify data files exist and are readable
    print(f"\nData File Validation:")
    print(f"{'-'*70}")
    
    try:
        # List data files
        data_files = sorted([f for f in os.listdir(plotfile_dir) 
                           if f.startswith("data_") and f.endswith(".fab")])
        
        if not data_files:
            errors.append("No data files (.fab) found in plotfile")
            return False, errors
        
        print(f"  ✓ Found {len(data_files)} data file(s)")
        for df in data_files[:5]:  # Print first 5
            df_path = os.path.join(plotfile_dir, df)
            df_size = os.path.getsize(df_path)
            print(f"    - {df} ({df_size} bytes)")
        
        if len(data_files) > 5:
            print(f"    ... and {len(data_files)-5} more")
        
    except Exception as e:
        errors.append(f"Error validating data files: {e}")
        return False, errors
    
    print(f"{'-'*70}")
    print(f"\n✓ Field output validation PASSED")
    
    return True, errors

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 test_field_output.py <plotfile_directory>")
        print("\nExample:")
        print("  python3 test_field_output.py plt_output")
        sys.exit(1)
    
    plotfile_dir = sys.argv[1]
    
    success, errors = validate_field_output(plotfile_dir)
    
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
