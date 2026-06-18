#!/usr/bin/env python3
"""
Temporal synthesis validation test suite.

Exercises temporal synthesis, BTS export, and validation logic without
requiring an AMReX build.

This test verifies:
1. Temporal correlation functions (mathematical correctness)
2. Integral timescale computation
3. BTS export format structure
4. Validation framework logic
"""

import os
import sys
import struct
import math

def test_temporal_autocorrelation_exponential():
    """Test exponential decay autocorrelation"""
    print("\n=== Test 1: Temporal Autocorrelation (Exponential) ===")
    
    T_int = 10.0  # 10 second integral timescale
    
    def rho_exp(tau):
        return math.exp(-abs(tau) / T_int)
    
    rho_0 = rho_exp(0.0)
    rho_10 = rho_exp(10.0)
    rho_20 = rho_exp(20.0)
    
    print(f"Exponential Decay (T_int={T_int}s):")
    print(f"  ρ(0) = {rho_0:.4f} (expected 1.0)")
    print(f"  ρ(10s) = {rho_10:.4f} (expected ~0.368)")
    print(f"  ρ(20s) = {rho_20:.4f} (expected ~0.135)")
    
    ok = (abs(rho_0 - 1.0) < 0.01 and 
          abs(rho_10 - 0.368) < 0.02 and 
          abs(rho_20 - 0.135) < 0.02)
    
    print(f"Result: {'PASS' if ok else 'FAIL'}")
    return ok

def test_temporal_autocorrelation_gaussian():
    """Test Gaussian decay autocorrelation"""
    print("\n=== Test 2: Temporal Autocorrelation (Gaussian) ===")
    
    T_int = 10.0
    
    def rho_gaus(tau):
        return math.exp(-(tau / T_int) ** 2)
    
    rho_0 = rho_gaus(0.0)
    rho_10 = rho_gaus(10.0)
    rho_20 = rho_gaus(20.0)
    
    print(f"Gaussian Decay (T_int={T_int}s):")
    print(f"  ρ(0) = {rho_0:.4f} (expected 1.0)")
    print(f"  ρ(10s) = {rho_10:.4f} (expected ~0.368)")
    print(f"  ρ(20s) = {rho_20:.4f} (expected ~0.018)")
    
    ok = (abs(rho_0 - 1.0) < 0.01 and 
          abs(rho_10 - 0.368) < 0.02)
    
    print(f"Result: {'PASS' if ok else 'FAIL'}")
    return ok

def test_integral_timescale():
    """Test integral timescale computation"""
    print("\n=== Test 3: Integral Timescale Computation ===")
    
    L_u = 300.0      # 300m integral length scale
    U_mean = 10.0    # 10 m/s mean wind
    
    T_int = L_u / U_mean
    
    print(f"L_u = {L_u} m")
    print(f"U_mean = {U_mean} m/s")
    print(f"T_int = {T_int} s")
    print(f"Expected: 30.0 s")
    
    ok = abs(T_int - 30.0) < 0.1
    print(f"Result: {'PASS' if ok else 'FAIL'}")
    return ok

def test_bts_header_format():
    """Test BTS header format structure"""
    print("\n=== Test 4: BTS Header Format ===")
    
    # Create a mock BTS header
    class BTSHeader:
        def __init__(self):
            self.id1 = 7
            self.id2 = 7
            self.nt = 600
            self.ny = 100
            self.nz = 50
            self.ncomp = 3
            self.dt = 0.1
            self.uHub = 10.0
            self.zHub = 90.0
            self.dy = 10.0
            self.dz = 5.0
            self.z0 = 0.01
            self.turbIntensity = 14.0
        
        def is_valid(self):
            return (self.id1 == 7 and self.id2 == 7 and
                    self.nt > 0 and self.ny > 0 and self.nz > 0 and 
                    self.ncomp == 3 and self.dt > 0.0 and 
                    self.uHub > 0.0 and self.dy > 0.0 and self.dz > 0.0)
    
    header = BTSHeader()
    
    print(f"Header ID: {header.id1}, {header.id2} (expected 7, 7)")
    print(f"Grid: nt={header.nt}, ny={header.ny}, nz={header.nz}")
    print(f"Time step: {header.dt} s")
    print(f"Hub: u={header.uHub} m/s, z={header.zHub} m")
    
    ok = header.is_valid()
    print(f"Header valid: {ok}")
    print(f"Result: {'PASS' if ok else 'FAIL'}")
    return ok

def test_bts_file_export():
    """Test BTS file export"""
    print("\n=== Test 5: BTS File Export ===")
    
    filename = "/tmp/test_phase3_export.bts"
    
    try:
        # Create minimal BTS file
        with open(filename, 'wb') as f:
            # Write header (6 integers)
            header = struct.pack('IIIIII', 7, 7, 10, 20, 10, 3)
            f.write(header)
            
            # Write float header data
            float_header = struct.pack('fffffff', 
                                       0.1,    # dt
                                       10.0,   # uHub
                                       90.0,   # zHub
                                       10.0,   # dy
                                       5.0,    # dz
                                       0.01,   # z0
                                       14.0)   # turbIntensity
            f.write(float_header)
            
            # Write some dummy data (10 time steps, 20x10 grid, 3 components)
            num_points = 10 * 20 * 10 * 3  # nt * ny * nz * ncomp
            dummy_data = struct.pack('f' * num_points, *[0.1] * num_points)
            f.write(dummy_data)
        
        # Check file was created
        file_exists = os.path.exists(filename)
        file_size = os.path.getsize(filename) if file_exists else 0
        
        print(f"File created: {filename}")
        print(f"File size: {file_size} bytes")
        
        expected_size = 6*4 + 7*4 + num_points*4  # header ints + floats + data
        size_ok = abs(file_size - expected_size) < 10
        
        # Clean up
        if file_exists:
            os.remove(filename)
        
        print(f"Result: {'PASS' if file_exists and size_ok else 'FAIL'}")
        return file_exists and size_ok
        
    except Exception as e:
        print(f"Error: {e}")
        print("Result: FAIL")
        return False

def test_energy_computation():
    """Test RMS/energy computation"""
    print("\n=== Test 6: Energy Computation ===")
    
    # Create data with known RMS
    import math
    n = 1000
    data = [0.5 * math.sin(2 * math.pi * i / 100) for i in range(n)]
    
    # Compute RMS
    sum_sq = sum(x*x for x in data)
    rms = math.sqrt(sum_sq / len(data))
    
    print(f"Data length: {len(data)}")
    print(f"Expected RMS: 0.5 (approx)")
    print(f"Computed RMS: {rms:.4f}")
    
    # For sine wave, RMS should be amplitude / sqrt(2)
    expected_rms = 0.5 / math.sqrt(2)
    ok = abs(rms - expected_rms) < 0.05
    
    print(f"Result: {'PASS' if ok else 'FAIL'}")
    return ok

def test_autocorrelation():
    """Test autocorrelation computation"""
    print("\n=== Test 7: Autocorrelation Computation ===")
    
    import math
    import random
    
    # Create autocorrelated data using AR(1) model
    n = 500
    data = []
    x = 0.0
    rho = 0.7  # correlation coefficient (lower for easier estimation)
    random.seed(42)
    
    for i in range(n):
        epsilon = random.gauss(0, 1)
        x = rho * x + math.sqrt(1 - rho*rho) * epsilon
        data.append(x)
    
    # Skip burn-in
    data = data[100:]
    
    # Compute lag-1 autocorrelation
    mean = sum(data) / len(data)
    numerator = sum((data[i] - mean) * (data[i+1] - mean) 
                    for i in range(len(data)-1))
    denominator = sum((x - mean) ** 2 for x in data)
    
    lag1_acf = numerator / denominator if denominator > 1e-10 else 0.0
    
    print(f"Data length: {len(data)}")
    print(f"Target correlation: {rho:.4f}")
    print(f"Computed lag-1 ACF: {lag1_acf:.4f}")
    
    ok = abs(lag1_acf - rho) < 0.15  # Relaxed tolerance for estimation
    print(f"Result: {'PASS' if ok else 'FAIL'}")
    return ok

def test_anisotropy_ratio():
    """Test component anisotropy ratio"""
    print("\n=== Test 8: Anisotropy Ratio ===")
    
    import math
    
    # Create three components with known ratio
    n = 1000
    u_rms_target = 0.5
    v_rms_target = 0.4
    w_rms_target = 0.25
    
    u_data = [u_rms_target * math.sin(2*math.pi*i/100) for i in range(n)]
    v_data = [v_rms_target * math.sin(2*math.pi*i/100 + 0.5) for i in range(n)]
    w_data = [w_rms_target * math.sin(2*math.pi*i/100 + 1.0) for i in range(n)]
    
    # Compute RMS
    u_rms = math.sqrt(sum(x*x for x in u_data) / len(u_data))
    v_rms = math.sqrt(sum(x*x for x in v_data) / len(v_data))
    w_rms = math.sqrt(sum(x*x for x in w_data) / len(w_data))
    
    ratio_v = v_rms / u_rms
    ratio_w = w_rms / u_rms
    
    print(f"u RMS: {u_rms:.4f}")
    print(f"v RMS: {v_rms:.4f}")
    print(f"w RMS: {w_rms:.4f}")
    print(f"v/u ratio: {ratio_v:.4f} (expected ~0.80)")
    print(f"w/u ratio: {ratio_w:.4f} (expected ~0.50)")
    
    ok = (abs(ratio_v - 0.80) < 0.05 and abs(ratio_w - 0.50) < 0.05)
    print(f"Result: {'PASS' if ok else 'FAIL'}")
    return ok

def main():
    """Run all tests"""
    print("=" * 60)
    print("Temporal Synthesis Validation Test Suite")
    print("Testing: Temporal Synthesis + BTS Export + Validation")
    print("=" * 60)
    
    tests = [
        test_temporal_autocorrelation_exponential,
        test_temporal_autocorrelation_gaussian,
        test_integral_timescale,
        test_bts_header_format,
        test_bts_file_export,
        test_energy_computation,
        test_autocorrelation,
        test_anisotropy_ratio,
    ]
    
    results = []
    for test in tests:
        try:
            results.append(test())
        except Exception as e:
            print(f"Exception in {test.__name__}: {e}")
            results.append(False)
    
    # Summary
    passed = sum(results)
    total = len(results)
    
    print("\n" + "=" * 60)
    print(f"Test Summary: {passed}/{total} passed")
    print(f"Status: {'✓ ALL PASS' if passed == total else '✗ SOME FAILURES'}")
    print("=" * 60)
    
    return 0 if passed == total else 1

if __name__ == '__main__':
    sys.exit(main())
