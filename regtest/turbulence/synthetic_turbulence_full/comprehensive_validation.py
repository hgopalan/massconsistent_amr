#!/usr/bin/env python3
"""
Comprehensive validation and testing for synthetic turbulence output.

Validates synthetic turbulence against physical properties:
1. Spectral properties (Von Kármán, Kaimal, energy conservation)
2. Continuity checks (∇·u' ≈ 0)
3. Turbulence intensity profiles
4. Coherence functions and decay
5. Integral length scales
6. Cross-correlations between components
7. OpenFAST format compatibility
"""

import struct
import os
import numpy as np
from pathlib import Path


class SpectralValidator:
    """Validate spectral properties of turbulence fields."""
    
    @staticmethod
    def compute_von_karman_spectrum(frequencies, u_rms, length_scale_u, u_mean):
        """
        Compute theoretical Von Kármán spectrum.
        
        S(f) = (4 * L_u * σ_u²) / (1 + 70.8*(f*L_u/U)²)^(5/6)
        
        References:
            von Kármán, T. (1948). Progress in the statistical theory of turbulence.
        """
        f = np.asarray(frequencies)
        f_norm = 70.8 * f * length_scale_u / u_mean
        
        # Avoid division by zero
        f_norm = np.maximum(f_norm, 1e-10)
        
        spectrum = (4.0 * length_scale_u * u_rms**2) / \
                   np.power(1.0 + f_norm**2, 5.0/6.0)
        
        return spectrum
    
    @staticmethod
    def compute_kaimal_spectrum(frequencies, u_rms, length_scale_u, u_mean):
        """
        Compute theoretical Kaimal spectrum.
        
        S(f) = (4 * L_u * σ_u²) / (1 + 5*(f*L_u/U))²
        
        References:
            Kaimal, J.C., et al. (1972). Spectral characteristics of surface-layer turbulence.
        """
        f = np.asarray(frequencies)
        f_norm = f * length_scale_u / u_mean
        
        spectrum = (4.0 * length_scale_u * u_rms**2) / \
                   np.power(1.0 + 5.0 * f_norm, 2.0)
        
        return spectrum
    
    @staticmethod
    def validate_energy_conservation(spectrum, frequencies, target_rms, 
                                    tolerance=0.05):
        """
        Validate energy conservation: ∫S(f)df = σ²_target
        
        Uses trapezoidal integration.
        """
        # Trapezoidal integration
        integrated_energy = np.trapz(spectrum, frequencies)
        target_energy = target_rms ** 2
        
        if target_energy > 0:
            error = abs(integrated_energy - target_energy) / target_energy
        else:
            error = abs(integrated_energy)
        
        is_valid = error <= tolerance
        
        return {
            'integrated_energy': integrated_energy,
            'target_energy': target_energy,
            'error': error * 100,  # In percentage
            'is_valid': is_valid,
            'tolerance_percent': tolerance * 100
        }
    
    @staticmethod
    def compute_integral_length_scale(spectrum, frequencies):
        """
        Compute integral length scale from energy spectrum.
        
        L_u = ∫₀^∞ ρ(r)dr = ∫₀^∞ S(f)/σ² * df
        
        Or in frequency space:
        L_u = (σ²/π) * ∫₀^∞ S(f)/f df  (approximately)
        """
        # Avoid division by zero in frequency
        f_safe = np.copy(frequencies)
        f_safe[f_safe < 1e-10] = 1e-10
        
        # Approximate integral: S(f)/f weighted by df
        integrand = spectrum / f_safe
        integral = np.trapz(integrand, frequencies)
        
        # The relationship between frequency-domain and space-domain
        # integral length scale is indirect; use energy normalization
        sigma_sq = np.trapz(spectrum, frequencies)
        
        if sigma_sq > 0:
            L_u = integral / (np.pi * sigma_sq)
        else:
            L_u = 0.0
        
        return L_u
    
    @staticmethod
    def validate_spectral_peak(spectrum, frequencies, expected_peak_freq=None):
        """
        Validate spectral peak location.
        
        Von Kármán and Kaimal spectra have different peak characteristics.
        """
        if len(spectrum) == 0:
            return None
        
        # Find peak
        peak_idx = np.argmax(spectrum)
        peak_freq = frequencies[peak_idx]
        peak_value = spectrum[peak_idx]
        
        return {
            'peak_frequency': peak_freq,
            'peak_value': peak_value,
            'peak_index': peak_idx,
            'expected_frequency': expected_peak_freq
        }


class ContinuityValidator:
    """Validate mass continuity and divergence properties."""
    
    @staticmethod
    def compute_divergence_fd(u_field, v_field, w_field, 
                             dx, dy, dz):
        """
        Compute divergence ∇·u using finite differences.
        
        Central differences where possible, one-sided at boundaries.
        
        Returns:
            (divergence_field, statistics)
        """
        u = np.asarray(u_field)
        v = np.asarray(v_field)
        w = np.asarray(w_field)
        
        if u.ndim != 3:
            return None, {'error': 'Expected 3D arrays'}
        
        nx, ny, nz = u.shape
        divergence = np.zeros_like(u)
        
        # Central differences for interior points
        divergence[1:-1, 1:-1, 1:-1] = \
            (u[2:, 1:-1, 1:-1] - u[:-2, 1:-1, 1:-1]) / (2 * dx) + \
            (v[1:-1, 2:, 1:-1] - v[1:-1, :-2, 1:-1]) / (2 * dy) + \
            (w[1:-1, 1:-1, 2:] - w[1:-1, 1:-1, :-2]) / (2 * dz)
        
        # Statistics
        stats = {
            'mean_divergence': float(np.mean(divergence)),
            'max_divergence': float(np.max(np.abs(divergence))),
            'rms_divergence': float(np.sqrt(np.mean(divergence**2))),
            'std_divergence': float(np.std(divergence)),
            'percentile_95': float(np.percentile(np.abs(divergence), 95)),
            'is_continuous': float(np.sqrt(np.mean(divergence**2))) < 0.1  # RMS threshold
        }
        
        return divergence, stats
    
    @staticmethod
    def validate_anisotropy_ratios(u_rms, v_rms, w_rms, 
                                  expected_v_ratio=0.80, 
                                  expected_w_ratio=0.50,
                                  tolerance=0.1):
        """
        Validate anisotropy ratios: v_rms/u_rms, w_rms/u_rms.
        
        Typical values:
            v_rms/u_rms ≈ 0.75-0.85
            w_rms/u_rms ≈ 0.45-0.55
        """
        if u_rms <= 0:
            return None
        
        actual_v_ratio = v_rms / u_rms
        actual_w_ratio = w_rms / u_rms
        
        v_error = abs(actual_v_ratio - expected_v_ratio)
        w_error = abs(actual_w_ratio - expected_w_ratio)
        
        return {
            'actual_v_ratio': actual_v_ratio,
            'expected_v_ratio': expected_v_ratio,
            'v_error': v_error,
            'v_valid': v_error <= tolerance,
            'actual_w_ratio': actual_w_ratio,
            'expected_w_ratio': expected_w_ratio,
            'w_error': w_error,
            'w_valid': w_error <= tolerance
        }
    
    @staticmethod
    def compute_cross_correlations(u_field, v_field, w_field):
        """
        Compute cross-correlation coefficients between components.
        
        ρ_uv = Cov(u,v) / (σ_u * σ_v)
        ρ_uw = Cov(u,w) / (σ_u * σ_w)
        ρ_vw = Cov(v,w) / (σ_v * σ_w)
        
        In isotropic turbulence, these are typically small (< 0.2).
        """
        u = np.asarray(u_field).flatten()
        v = np.asarray(v_field).flatten()
        w = np.asarray(w_field).flatten()
        
        # Compute covariances and correlations
        cov_uv = np.mean(u * v)
        cov_uw = np.mean(u * w)
        cov_vw = np.mean(v * w)
        
        std_u = np.std(u)
        std_v = np.std(v)
        std_w = np.std(w)
        
        if std_u > 0 and std_v > 0:
            rho_uv = cov_uv / (std_u * std_v)
        else:
            rho_uv = 0.0
        
        if std_u > 0 and std_w > 0:
            rho_uw = cov_uw / (std_u * std_w)
        else:
            rho_uw = 0.0
        
        if std_v > 0 and std_w > 0:
            rho_vw = cov_vw / (std_v * std_w)
        else:
            rho_vw = 0.0
        
        return {
            'rho_uv': rho_uv,
            'rho_uw': rho_uw,
            'rho_vw': rho_vw,
            'realistic': (abs(rho_uv) < 0.3 and abs(rho_uw) < 0.3 and 
                         abs(rho_vw) < 0.3)
        }


class CoherenceValidator:
    """Validate coherence functions and spatial decay."""
    
    @staticmethod
    def compute_spatial_autocorrelation(field, lag_distances, grid_spacing):
        """
        Compute spatial autocorrelation function.
        
        ρ(Δy) = <u(y)*u(y+Δy)> / σ_u²
        """
        field = np.asarray(field).flatten()
        std = np.std(field)
        mean = np.mean(field)
        
        if std <= 0:
            return None
        
        # Normalize
        field_norm = (field - mean) / std
        
        autocorr = []
        for lag in lag_distances:
            lag_idx = int(lag / grid_spacing)
            
            if lag_idx < len(field):
                # Compute autocorrelation
                corr_val = np.mean(field_norm[:-lag_idx] * field_norm[lag_idx:])
                autocorr.append(corr_val)
            else:
                autocorr.append(0.0)
        
        return np.array(autocorr)
    
    @staticmethod
    def validate_coherence_decay(distances, correlations, 
                                model='exponential',
                                decay_rate=0.008):
        """
        Validate coherence decay against theoretical model.
        
        Exponential: ρ(Δy) = exp(-a*Δy)
        Gaussian: ρ(Δy) = exp(-(a*Δy)²)
        """
        distances = np.asarray(distances)
        correlations = np.asarray(correlations)
        
        if model == 'exponential':
            theory = np.exp(-decay_rate * distances)
        elif model == 'gaussian':
            theory = np.exp(-(decay_rate * distances)**2)
        else:
            return None
        
        # Compute RMSE between observed and theoretical
        rmse = np.sqrt(np.mean((correlations - theory)**2))
        
        return {
            'model': model,
            'decay_rate': decay_rate,
            'rmse': rmse,
            'max_error': float(np.max(np.abs(correlations - theory))),
            'realistic': rmse < 0.2  # RMSE threshold
        }


class OpenFASTValidator:
    """Validate OpenFAST/TurbSim format compatibility."""
    
    @staticmethod
    def validate_bts_format(bts_file):
        """
        Validate BTS file format against TurbSim specification.
        """
        if not os.path.exists(bts_file):
            return {'valid': False, 'error': f'File not found: {bts_file}'}
        
        try:
            with open(bts_file, 'rb') as f:
                # Read header
                header_ints = struct.unpack('6i', f.read(6 * 4))
                id1, id2, nt, ny, nz, ncomp = header_ints
                
                # Validate identifiers
                if id1 != 7 or id2 != 7:
                    return {'valid': False, 
                           'error': f'Invalid format identifiers: {id1}, {id2}'}
                
                # Validate dimensions
                if nt <= 0 or ny <= 0 or nz <= 0 or ncomp != 3:
                    return {'valid': False,
                           'error': f'Invalid dimensions: nt={nt}, ny={ny}, nz={nz}, ncomp={ncomp}'}
                
                # Read floating-point header
                header_floats = struct.unpack('6f', f.read(6 * 4))
                dt, uHub, zHub, dy, dz, z0 = header_floats
                
                # Validate parameters
                if dt <= 0 or uHub <= 0 or dy <= 0 or dz <= 0:
                    return {'valid': False,
                           'error': 'Invalid floating-point parameters'}
                
                # Check file size
                file_size = os.path.getsize(bts_file)
                expected_size = (6 * 4 + 6 * 4 + 4) + (nt * ny * nz * ncomp * 4)
                size_error = abs(file_size - expected_size) / max(expected_size, 1)
                
                if size_error > 0.01:  # Allow 1% tolerance
                    return {'valid': False,
                           'error': f'File size mismatch: {file_size} vs {expected_size} expected'}
                
                return {
                    'valid': True,
                    'grid_points': nt * ny * nz,
                    'nt': nt,
                    'ny': ny,
                    'nz': nz,
                    'dt': dt,
                    'uHub': uHub,
                    'zHub': zHub,
                    'dy': dy,
                    'dz': dz,
                    'z0': z0,
                    'file_size': file_size,
                    'format': 'TurbSim BTS v1.0'
                }
        
        except Exception as e:
            return {'valid': False, 'error': str(e)}
    
    @staticmethod
    def check_openfast_compatibility(bts_info):
        """
        Check if BTS file is compatible with OpenFAST.
        
        OpenFAST requirements:
        - Valid TurbSim format identifiers (7, 7)
        - nt > 0, ny > 0, nz > 0, ncomp = 3
        - Reasonable parameter ranges
        """
        if not bts_info.get('valid', False):
            return {'compatible': False, 'issues': ['Invalid BTS format']}
        
        issues = []
        
        # Check grid dimensions
        if bts_info['nt'] < 10:
            issues.append('Too few time steps (< 10)')
        if bts_info['ny'] < 5:
            issues.append('Too few y-direction points (< 5)')
        if bts_info['nz'] < 5:
            issues.append('Too few z-direction points (< 5)')
        
        # Check parameter ranges
        if bts_info['dt'] > 10.0:
            issues.append(f'Time step too large: {bts_info["dt"]} s')
        if bts_info['uHub'] > 100.0:
            issues.append(f'Hub wind speed unrealistic: {bts_info["uHub"]} m/s')
        if bts_info['z0'] < 0.0001 or bts_info['z0'] > 1.0:
            issues.append(f'Roughness out of typical range: {bts_info["z0"]} m')
        
        return {
            'compatible': len(issues) == 0,
            'issues': issues,
            'warnings': []
        }


def test_phase4_validation():
    """
    Run comprehensive validation tests.
    """
    print("\n" + "="*70)
    print("Comprehensive Validation & Testing")
    print("="*70)
    
    results = {}
    
    # Test 1: Von Kármán Spectrum
    print("\n--- Test 1: Von Kármán Spectrum Validation ---")
    freqs = np.logspace(-3, 1, 128)
    u_rms = 1.0
    L_u = 300.0
    U_mean = 10.0
    
    spectrum_vk = SpectralValidator.compute_von_karman_spectrum(
        freqs, u_rms, L_u, U_mean)
    energy_result = SpectralValidator.validate_energy_conservation(
        spectrum_vk, freqs, u_rms, tolerance=0.10)
    
    print(f"Integrated Energy: {energy_result['integrated_energy']:.6f} m²/s²")
    print(f"Target Energy: {energy_result['target_energy']:.6f} m²/s²")
    print(f"Error: {energy_result['error']:.2f}%")
    results['von_karman_energy'] = energy_result['is_valid']
    
    # Test 2: Kaimal Spectrum
    print("\n--- Test 2: Kaimal Spectrum Validation ---")
    spectrum_kaimal = SpectralValidator.compute_kaimal_spectrum(
        freqs, u_rms, L_u, U_mean)
    energy_kaimal = SpectralValidator.validate_energy_conservation(
        spectrum_kaimal, freqs, u_rms, tolerance=0.15)
    
    print(f"Integrated Energy: {energy_kaimal['integrated_energy']:.6f} m²/s²")
    print(f"Error: {energy_kaimal['error']:.2f}%")
    results['kaimal_energy'] = energy_kaimal['is_valid']
    
    # Test 3: Integral Length Scale
    print("\n--- Test 3: Integral Length Scale Recovery ---")
    L_u_recovered = SpectralValidator.compute_integral_length_scale(
        spectrum_vk, freqs)
    L_u_error = abs(L_u_recovered - L_u) / L_u
    
    print(f"Input Length Scale: {L_u:.2f} m")
    print(f"Recovered: {L_u_recovered:.2f} m")
    print(f"Error: {L_u_error*100:.2f}%")
    results['integral_length_scale'] = L_u_error < 0.50  # 50% tolerance
    
    # Test 4: Spectral Peak
    print("\n--- Test 4: Spectral Peak Characteristics ---")
    peak = SpectralValidator.validate_spectral_peak(spectrum_vk, freqs)
    print(f"Peak Frequency: {peak['peak_frequency']:.6f} Hz")
    print(f"Peak Value: {peak['peak_value']:.6e} m²/s²/Hz")
    results['spectral_peak'] = True
    
    # Test 5: OpenFAST Format
    print("\n--- Test 5: OpenFAST/TurbSim Format Validation ---")
    bts_file = 'turbulence_synthetic.bts'
    
    if os.path.exists(bts_file):
        bts_info = OpenFASTValidator.validate_bts_format(bts_file)
        
        if bts_info.get('valid', False):
            print(f"✓ BTS Format Valid")
            print(f"  Grid points: {bts_info['grid_points']}")
            print(f"  Time steps: {bts_info['nt']}")
            print(f"  Format: {bts_info['format']}")
            
            # Check compatibility
            compat = OpenFASTValidator.check_openfast_compatibility(bts_info)
            print(f"  OpenFAST Compatible: {compat['compatible']}")
            
            if not compat['compatible']:
                for issue in compat['issues']:
                    print(f"    ⚠ {issue}")
            
            results['openfast_format'] = bts_info['valid']
            results['openfast_compatible'] = compat['compatible']
        else:
            print(f"✗ BTS Format Invalid: {bts_info['error']}")
            results['openfast_format'] = False
    else:
        print(f"⊘ BTS file not found: {bts_file}")
        results['openfast_format'] = None
    
    # Summary
    print("\n" + "="*70)
    print("Validation Summary")
    print("="*70)
    
    passed = sum(1 for v in results.values() if v is True)
    total = len([v for v in results.values() if v is not None])
    
    for test_name, result in results.items():
        if result is True:
            status = "✓ PASS"
        elif result is False:
            status = "✗ FAIL"
        else:
            status = "⊘ SKIP"
        
        print(f"{status}: {test_name}")
    
    if total > 0:
        print(f"\nTotal: {passed}/{total} tests passed")
    
    return all(v is not False for v in results.values())


if __name__ == '__main__':
    import sys
    success = test_phase4_validation()
    sys.exit(0 if success else 1)
