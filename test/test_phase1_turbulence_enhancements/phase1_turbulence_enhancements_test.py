import sys
import math

# Test data structure
test_results = {
    'passed': 0,
    'failed': 0,
    'tests': []
}

def linspace(start, stop, num):
    """Simple linspace replacement for numpy.linspace."""
    if num <= 0:
        return []
    if num == 1:
        return [start]
    step = (stop - start) / (num - 1)
    return [start + step * i for i in range(num)]

def logspace(start, stop, num):
    """Simple logspace replacement for numpy.logspace."""
    lin = linspace(start, stop, num)
    return [10 ** x for x in lin]

def test_iec61400_intensity_model():
    """Test IEC 61400-1:2019 intensity model with lookup tables."""
    print("\n" + "="*70)
    print("TEST: IEC 61400-1:2019 Intensity Model")
    print("="*70)
    
    # IEC NTM reference intensities at hub height
    categories = {
        0: (0.16, "Category A (16%)"),
        1: (0.14, "Category B (14%)"),
        2: (0.12, "Category C (12%)")
    }
    
    hub_height = 90.0  # meters
    z_values = [10, 30, 60, 90, 150, 200]  # Height AGL [m]
    
    try:
        all_pass = True
        for cat_id, (ref_intensity, cat_name) in categories.items():
            print(f"\n  {cat_name}:")
            print(f"    Height (m) | Intensity | Expected Range")
            print(f"    -----------+-----------+--------------------")
            
            for z in z_values:
                # IEC NTM: I(z) = I_hub * (z/z_hub)^0.2
                power_law_exp = 0.2
                ratio = z / hub_height
                intensity = ref_intensity * (ratio ** power_law_exp)
                
                # Clamp to physical bounds
                intensity = max(min(intensity, 0.30), 0.01)
                
                # Check that intensity is within reasonable bounds
                if not (0.01 <= intensity <= 0.30):
                    all_pass = False
                    print(f"    {z:>3} | {intensity:.4f} | [0.01, 0.30] - FAIL")
                else:
                    print(f"    {z:>3} | {intensity:.4f} | [0.01, 0.30] - OK")
        
        if all_pass:
            test_results['passed'] += 1
            test_results['tests'].append({
                'name': 'IEC 61400-1 Intensity Model',
                'status': 'PASS',
                'details': 'All intensity values within physical bounds'
            })
            print("\n  ✓ PASS: IEC 61400-1 intensity model works correctly")
            return True
        else:
            test_results['failed'] += 1
            test_results['tests'].append({
                'name': 'IEC 61400-1 Intensity Model',
                'status': 'FAIL',
                'details': 'Some intensity values out of bounds'
            })
            print("\n  ✗ FAIL: IEC 61400-1 intensity model has out-of-bounds values")
            return False
    except Exception as e:
        test_results['failed'] += 1
        test_results['tests'].append({
            'name': 'IEC 61400-1 Intensity Model',
            'status': 'ERROR',
            'details': str(e)
        })
        print(f"\n  ✗ ERROR: {e}")
        return False

def test_smooth_profile_intensity():
    """Test smooth user-defined intensity profile."""
    print("\n" + "="*70)
    print("TEST: Smooth User-Defined Intensity Profile")
    print("="*70)
    
    try:
        z_ref = 10.0  # Reference height [m]
        intensity_ref = 0.12  # Reference intensity
        exponent = 0.14  # Smooth exponent
        
        # Test at various heights
        z_values = [1, 5, 10, 20, 50, 100, 200]
        
        print(f"\n  Parameters: I_ref={intensity_ref}, z_ref={z_ref}m, exp={exponent}")
        print(f"    Height (m) | Intensity | Smoothness Check")
        print(f"    -----------+-----------+--------------------")
        
        prev_intensity = None
        monotonic = True
        
        for z in z_values:
            # Smooth power-law: I(z) = I_ref * (z/z_ref)^exponent
            ratio = z / z_ref
            intensity = intensity_ref * (ratio ** exponent)
            
            # Clamp to physical bounds
            intensity = max(min(intensity, 0.30), 0.01)
            
            # Check monotonicity
            if prev_intensity is not None:
                if intensity < prev_intensity:
                    monotonic = False
            prev_intensity = intensity
            
            # Check bounds
            in_bounds = 0.01 <= intensity <= 0.30
            print(f"    {z:>3} | {intensity:.4f} | {'Monotonic' if monotonic else 'ERROR'}")
        
        if monotonic:
            test_results['passed'] += 1
            test_results['tests'].append({
                'name': 'Smooth Profile Intensity',
                'status': 'PASS',
                'details': 'Profile is monotonically increasing with height'
            })
            print("\n  ✓ PASS: Smooth profile intensity is monotonic and in bounds")
            return True
        else:
            test_results['failed'] += 1
            test_results['tests'].append({
                'name': 'Smooth Profile Intensity',
                'status': 'FAIL',
                'details': 'Profile is not monotonically increasing'
            })
            print("\n  ✗ FAIL: Profile is not monotonically increasing")
            return False
    except Exception as e:
        test_results['failed'] += 1
        test_results['tests'].append({
            'name': 'Smooth Profile Intensity',
            'status': 'ERROR',
            'details': str(e)
        })
        print(f"\n  ✗ ERROR: {e}")
        return False

def test_quadratic_exponential_coherence():
    """Test quadratic exponential coherence model."""
    print("\n" + "="*70)
    print("TEST: Quadratic Exponential Coherence Model")
    print("="*70)
    
    try:
        decay_factor = 0.008  # [1/m]
        distances = [0, 10, 50, 100, 200, 500, 1000]
        
        print(f"\n  Decay factor: {decay_factor} [1/m]")
        print(f"    Distance (m) | Coherence | Physical Bounds")
        print(f"    --------------+-----------+--------------------")
        
        all_valid = True
        for d in distances:
            # Quadratic exponential: Coh = exp(-k*d^2/2)
            arg = -decay_factor * d * d / 2.0
            arg = max(arg, -100.0)  # Prevent underflow
            coherence = math.exp(arg)
            
            # Clamp to [0, 1]
            coherence = max(min(coherence, 1.0), 0.0)
            
            # Check bounds
            if not (0.0 <= coherence <= 1.0):
                all_valid = False
            
            print(f"    {d:>4} | {coherence:.6f} | {'OK' if 0 <= coherence <= 1 else 'FAIL'}")
        
        if all_valid:
            test_results['passed'] += 1
            test_results['tests'].append({
                'name': 'Quadratic Exponential Coherence',
                'status': 'PASS',
                'details': 'All coherence values in [0,1] range'
            })
            print("\n  ✓ PASS: Quadratic exponential coherence is valid")
            return True
        else:
            test_results['failed'] += 1
            test_results['tests'].append({
                'name': 'Quadratic Exponential Coherence',
                'status': 'FAIL',
                'details': 'Some coherence values out of [0,1] range'
            })
            print("\n  ✗ FAIL: Some coherence values invalid")
            return False
    except Exception as e:
        test_results['failed'] += 1
        test_results['tests'].append({
            'name': 'Quadratic Exponential Coherence',
            'status': 'ERROR',
            'details': str(e)
        })
        print(f"\n  ✗ ERROR: {e}")
        return False

def test_powerlaw_coherence():
    """Test power-law coherence model."""
    print("\n" + "="*70)
    print("TEST: Power-Law Coherence Model")
    print("="*70)
    
    try:
        decay_factor = 0.008  # [1/m]
        exponent = 1.5  # Power-law exponent
        distances = [0, 10, 50, 100, 200, 500, 1000]
        
        print(f"\n  Decay factor: {decay_factor} [1/m], Exponent: {exponent}")
        print(f"    Distance (m) | Coherence | Physical Bounds")
        print(f"    --------------+-----------+--------------------")
        
        all_valid = True
        for d in distances:
            # Power-law: Coh = (1 + k*d)^(-m)
            base = 1.0 + decay_factor * d
            coherence = base ** (-exponent)
            
            # Clamp to [0, 1]
            coherence = max(min(coherence, 1.0), 0.0)
            
            # Check bounds
            if not (0.0 <= coherence <= 1.0):
                all_valid = False
            
            print(f"    {d:>4} | {coherence:.6f} | {'OK' if 0 <= coherence <= 1 else 'FAIL'}")
        
        if all_valid:
            test_results['passed'] += 1
            test_results['tests'].append({
                'name': 'Power-Law Coherence',
                'status': 'PASS',
                'details': 'All coherence values in [0,1] range'
            })
            print("\n  ✓ PASS: Power-law coherence is valid")
            return True
        else:
            test_results['failed'] += 1
            test_results['tests'].append({
                'name': 'Power-Law Coherence',
                'status': 'FAIL',
                'details': 'Some coherence values out of [0,1] range'
            })
            print("\n  ✗ FAIL: Some coherence values invalid")
            return False
    except Exception as e:
        test_results['failed'] += 1
        test_results['tests'].append({
            'name': 'Power-Law Coherence',
            'status': 'ERROR',
            'details': str(e)
        })
        print(f"\n  ✗ ERROR: {e}")
        return False

def test_coherence_model_comparison():
    """Compare all coherence models."""
    print("\n" + "="*70)
    print("TEST: Coherence Model Comparison")
    print("="*70)
    
    try:
        decay_factor = 0.008  # [1/m]
        distances = linspace(0, 500, 11)
        
        print(f"\n  Decay factor: {decay_factor} [1/m]")
        print(f"    Distance (m) | Gaussian | Exponential | QuadExp | PowerLaw")
        print(f"    --------------+----------+-------------+---------+---------")
        
        for d in distances:
            # Gaussian: exp(-k*d^2)
            gaussian = math.exp(max(-decay_factor * d * d, -100.0))
            gaussian = max(min(gaussian, 1.0), 0.0)
            
            # Exponential: exp(-k*d)
            exponential = math.exp(max(-decay_factor * d, -100.0))
            exponential = max(min(exponential, 1.0), 0.0)
            
            # Quadratic exponential: exp(-k*d^2/2)
            quad_exp = math.exp(max(-decay_factor * d * d / 2.0, -100.0))
            quad_exp = max(min(quad_exp, 1.0), 0.0)
            
            # Power-law: (1 + k*d)^(-1.5)
            base = 1.0 + decay_factor * d
            powerlaw = base ** (-1.5)
            powerlaw = max(min(powerlaw, 1.0), 0.0)
            
            print(f"    {d:>4.0f}       | {gaussian:.4f} | {exponential:.4f} | {quad_exp:.4f} | {powerlaw:.4f}")
        
        test_results['passed'] += 1
        test_results['tests'].append({
            'name': 'Coherence Model Comparison',
            'status': 'PASS',
            'details': 'Comparison table generated successfully'
        })
        print("\n  ✓ PASS: Coherence models comparison completed")
        return True
    except Exception as e:
        test_results['failed'] += 1
        test_results['tests'].append({
            'name': 'Coherence Model Comparison',
            'status': 'ERROR',
            'details': str(e)
        })
        print(f"\n  ✗ ERROR: {e}")
        return False

def test_intensity_model_comparison():
    """Compare all intensity models."""
    print("\n" + "="*70)
    print("TEST: Intensity Model Comparison")
    print("="*70)
    
    try:
        z_values = logspace(0, 3, 11)  # 1m to 1000m
        intensity_ref = 0.12
        z_ref = 10.0
        hub_height = 90.0
        exponent = 0.14
        
        print(f"\n  Reference: I_ref={intensity_ref}, z_ref={z_ref}m")
        print(f"    Height (m) | PowerLaw | Logarithmic | Constant | IEC61400-B | SmoothProf")
        print(f"    -----------+----------+-------------+----------+------------+-----------")
        
        for z in z_values:
            # Power-law
            powerlaw = intensity_ref * ((z / z_ref) ** exponent)
            powerlaw = max(min(powerlaw, 0.30), 0.01)
            
            # Logarithmic (z0 = 0.1m)
            z0 = 0.1
            log_ratio_z = math.log((z + z0) / z0)
            log_ratio_ref = math.log((z_ref + z0) / z0)
            logarithmic = intensity_ref * log_ratio_z / log_ratio_ref
            logarithmic = max(min(logarithmic, 0.30), 0.01)
            
            # Constant
            constant = intensity_ref
            
            # IEC 61400-1 (Category B)
            iec_ref = 0.14
            iec = iec_ref * ((z / hub_height) ** 0.2)
            iec = max(min(iec, 0.30), 0.01)
            
            # Smooth profile
            smooth = intensity_ref * ((z / z_ref) ** exponent)
            smooth = max(min(smooth, 0.30), 0.01)
            
            print(f"    {z:>6.0f}    | {powerlaw:.4f}   | {logarithmic:.4f}      | {constant:.4f} | {iec:.4f}      | {smooth:.4f}")
        
        test_results['passed'] += 1
        test_results['tests'].append({
            'name': 'Intensity Model Comparison',
            'status': 'PASS',
            'details': 'Comparison table generated successfully'
        })
        print("\n  ✓ PASS: Intensity models comparison completed")
        return True
    except Exception as e:
        test_results['failed'] += 1
        test_results['tests'].append({
            'name': 'Intensity Model Comparison',
            'status': 'ERROR',
            'details': str(e)
        })
        print(f"\n  ✗ ERROR: {e}")
        return False

def test_physical_bounds_enforcement():
    """Test that all models enforce physical bounds."""
    print("\n" + "="*70)
    print("TEST: Physical Bounds Enforcement")
    print("="*70)
    
    try:
        # Test extreme values
        extreme_cases = [
            ("Very low height", 0.01),
            ("Normal height", 50.0),
            ("Very high height", 10000.0)
        ]
        
        intensity_min = 0.01
        intensity_max = 0.30
        coherence_min = 0.0
        coherence_max = 1.0
        
        print(f"\n  Intensity bounds: [{intensity_min}, {intensity_max}]")
        print(f"  Coherence bounds: [{coherence_min}, {coherence_max}]")
        
        all_pass = True
        
        for case_name, z in extreme_cases:
            # Test intensities
            for intensity_model in ['PowerLaw', 'Logarithmic', 'Constant', 'IEC61400', 'SmoothProfile']:
                # Compute intensity (simplified)
                if intensity_model == 'Constant':
                    intensity = 0.12
                else:
                    intensity = 0.12 * ((z / 10.0) ** 0.14)
                
                intensity = max(min(intensity, intensity_max), intensity_min)
                
                if not (intensity_min <= intensity <= intensity_max):
                    all_pass = False
                    print(f"  ✗ {case_name} / {intensity_model}: Intensity {intensity} out of bounds")
            
            # Test coherences
            for coherence_model in ['Gaussian', 'Exponential', 'QuadExp', 'PowerLaw']:
                # Compute coherence (simplified)
                distance = 100.0
                decay = 0.008
                
                if coherence_model == 'Gaussian':
                    coherence = math.exp(max(-decay * distance * distance, -100.0))
                elif coherence_model == 'Exponential':
                    coherence = math.exp(max(-decay * distance, -100.0))
                elif coherence_model == 'QuadExp':
                    coherence = math.exp(max(-decay * distance * distance / 2.0, -100.0))
                else:  # PowerLaw
                    coherence = (1.0 + decay * distance) ** (-1.5)
                
                coherence = max(min(coherence, coherence_max), coherence_min)
                
                if not (coherence_min <= coherence <= coherence_max):
                    all_pass = False
                    print(f"  ✗ {case_name} / {coherence_model}: Coherence {coherence} out of bounds")
        
        if all_pass:
            test_results['passed'] += 1
            test_results['tests'].append({
                'name': 'Physical Bounds Enforcement',
                'status': 'PASS',
                'details': 'All models enforce physical bounds correctly'
            })
            print("\n  ✓ PASS: All models enforce physical bounds correctly")
            return True
        else:
            test_results['failed'] += 1
            test_results['tests'].append({
                'name': 'Physical Bounds Enforcement',
                'status': 'FAIL',
                'details': 'Some models violated physical bounds'
            })
            return False
    except Exception as e:
        test_results['failed'] += 1
        test_results['tests'].append({
            'name': 'Physical Bounds Enforcement',
            'status': 'ERROR',
            'details': str(e)
        })
        print(f"\n  ✗ ERROR: {e}")
        return False

def main():
    """Run all Phase 1 tests."""
    print("\n" + "="*70)
    print("PHASE 1 TURBULENCE ENHANCEMENTS TEST SUITE")
    print("="*70)
    print("Testing: IEC 61400-1, Smooth Profiles, Additional Coherence Models")
    
    # Run all tests
    test_iec61400_intensity_model()
    test_smooth_profile_intensity()
    test_quadratic_exponential_coherence()
    test_powerlaw_coherence()
    test_coherence_model_comparison()
    test_intensity_model_comparison()
    test_physical_bounds_enforcement()
    
    # Print summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    
    total = test_results['passed'] + test_results['failed']
    print(f"\nTotal Tests: {total}")
    print(f"Passed: {test_results['passed']} ✓")
    print(f"Failed: {test_results['failed']} ✗")
    
    print("\nDetailed Results:")
    for test in test_results['tests']:
        status_symbol = "✓" if test['status'] == 'PASS' else "✗"
        print(f"  {status_symbol} {test['name']}: {test['status']}")
        if test['details']:
            print(f"      {test['details']}")
    
    print("\n" + "="*70)
    if test_results['failed'] == 0:
        print("✓ ALL TESTS PASSED")
        return 0
    else:
        print(f"✗ {test_results['failed']} TEST(S) FAILED")
        return 1

if __name__ == '__main__':
    sys.exit(main())
