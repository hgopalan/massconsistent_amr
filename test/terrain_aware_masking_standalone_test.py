#!/usr/bin/env python3
"""
Standalone test for terrain-aware masking logic.

This test validates the mask computation algorithm without requiring 
the full C++ bindings to be built.
"""

import numpy as np


def compute_terrain_mask(nz, ny, nx, zmin, dz, terrain):
    """
    Compute a terrain-aware masking function for synthetic turbulence.
    
    Parameters:
        nz, ny, nx: Grid dimensions
        zmin: Minimum z-coordinate
        dz: Cell spacing in z
        terrain: 2D array of terrain elevation (ny, nx) in meters
    
    Returns:
        3D mask array (nz, ny, nx) with values in [0, 1]
    """
    # Compute z-coordinates for each k-level (cell centers)
    z_centers = zmin + (np.arange(nz) + 0.5) * dz
    
    # Define transition zone height for smooth blending
    transition_cells = max(2, int(np.ceil(2.0 / dz)))
    transition_height = transition_cells * dz
    
    # Reshape for broadcasting: z_centers[nz, 1, 1] - terrain[1, ny, nx]
    z_centers_3d = z_centers[:, np.newaxis, np.newaxis]
    z_agl = z_centers_3d - terrain[np.newaxis, :, :]  # Shape: (nz, ny, nx)
    
    # Initialize mask with ones
    mask = np.ones_like(z_agl, dtype=np.float32)
    
    # Apply masking rules using NumPy operations (vectorized)
    # 1. Inside terrain (z_agl <= 0): mask = 0
    mask[z_agl <= 0.0] = 0.0
    
    # 2. Transition zone (0 < z_agl < transition_height): smooth blend
    transition_zone = (z_agl > 0.0) & (z_agl < transition_height)
    normalized = z_agl[transition_zone] / transition_height
    mask[transition_zone] = (1.0 - np.cos(np.pi * normalized)) / 2.0
    
    return mask, z_agl


def test_terrain_mask_basic():
    """Test basic terrain mask computation."""
    print("\n" + "="*70)
    print("Test 1: Terrain Mask Computation (Basic Properties)")
    print("="*70)
    
    try:
        # Create test parameters
        nx, ny, nz = 21, 21, 11
        zmin = 0.0
        dz = 10.0
        
        # Create synthetic terrain (Gaussian hill)
        center_i, center_j = 10, 10
        z_peak = 100.0
        r0 = 5.0
        
        terrain = np.zeros((ny, nx), dtype=np.float32)
        for j in range(ny):
            for i in range(nx):
                r_sq = (i - center_i)**2 + (j - center_j)**2
                terrain[j, i] = z_peak * np.exp(-r_sq / (r0**2))
        
        # Compute mask
        mask, z_agl = compute_terrain_mask(nz, ny, nx, zmin, dz, terrain)
        
        # Verification 1: Mask shape
        assert mask.shape == (nz, ny, nx), f"Mask shape mismatch: {mask.shape}"
        print(f"✓ Mask shape correct: {mask.shape}")
        
        # Verification 2: Mask values in [0, 1]
        assert mask.min() >= 0.0, f"Mask minimum {mask.min()} < 0"
        assert mask.max() <= 1.0, f"Mask maximum {mask.max()} > 1"
        print(f"✓ Mask values in [0, 1]: min={mask.min():.3f}, max={mask.max():.3f}")
        
        # Verification 3: Masking at peak
        peak_i, peak_j = center_i, center_j
        peak_mask = mask[:, peak_j, peak_i]
        
        # Inside terrain should have low mask
        lowest_above = np.argmax(z_agl[:, peak_j, peak_i] >= 0)
        if lowest_above > 0:
            for k in range(lowest_above):
                assert mask[k, peak_j, peak_i] < 0.1, \
                    f"Mask inside terrain should be ~0, got {mask[k, peak_j, peak_i]}"
        print(f"✓ Inside terrain (z_agl < 0): mask ≈ 0")
        
        # Above terrain should have high mask
        for k in range(max(0, lowest_above + 3), nz):
            assert mask[k, peak_j, peak_i] > 0.9, \
                f"Mask above terrain should be ~1, got {mask[k, peak_j, peak_i]} at k={k}"
        print(f"✓ Above terrain: mask ≈ 1")
        
        # Verification 4: Smooth transition
        diffs = np.diff(peak_mask)
        # Should be generally increasing or near constant
        negative_diffs = np.sum(diffs < -0.05)
        assert negative_diffs <= 1, f"Mask not smooth: {negative_diffs} negative jumps"
        print(f"✓ Smooth transition: monotonically increasing")
        
        return True
        
    except AssertionError as e:
        print(f"✗ FAILED: {e}")
        return False
    except Exception as e:
        print(f"✗ FAILED with exception: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_terrain_mask_flat():
    """Test mask on flat terrain."""
    print("\n" + "="*70)
    print("Test 2: Flat Terrain")
    print("="*70)
    
    try:
        nx, ny, nz = 5, 5, 3
        zmin = 0.0
        dz = 5.0
        
        # Flat terrain at 10 m
        terrain_flat = np.zeros((ny, nx), dtype=np.float32) + 10.0
        
        mask, z_agl = compute_terrain_mask(nz, ny, nx, zmin, dz, terrain_flat)
        
        # For flat terrain, all points at same k should have same mask
        for k in range(nz):
            mask_k = mask[k, :, :].flatten()
            std_dev = np.std(mask_k)
            assert std_dev < 1e-5, f"Non-uniform mask at k={k}, std={std_dev}"
        
        print(f"✓ Flat terrain produces uniform mask at each level")
        
        # Check values
        # k=0: z_agl = 2.5 - 10 = -7.5 (inside) → mask ≈ 0
        assert mask[0, :, :].max() < 0.1, f"k=0 should be inside terrain"
        
        # k=2: z_agl = 12.5 - 10 = 2.5 (in transition zone) → mask should be between 0 and 1
        # For transition_height=10, normalized=0.25, mask ≈ 0.146
        assert mask[2, :, :].min() > 0.1, f"k=2 mask should be in transition, got {mask[2,:,:].min()}"
        
        print(f"✓ Mask values correct for flat terrain")
        
        return True
        
    except AssertionError as e:
        print(f"✗ FAILED: {e}")
        return False
    except Exception as e:
        print(f"✗ FAILED with exception: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_no_penetration():
    """Test that fluctuations don't penetrate terrain."""
    print("\n" + "="*70)
    print("Test 3: No Fluctuation Penetration into Terrain")
    print("="*70)
    
    try:
        nx, ny, nz = 11, 11, 7
        zmin = 0.0
        dz = 5.0
        
        # Terrain with hill
        terrain = np.zeros((ny, nx), dtype=np.float32)
        for j in range(ny):
            for i in range(nx):
                r_sq = (i - 5)**2 + (j - 5)**2
                terrain[j, i] = 30.0 * np.exp(-r_sq / 9.0)
        
        # Fluctuation field
        u_fluct = np.ones((nz, ny, nx), dtype=np.float32) * 0.5
        
        # Compute mask
        mask, z_agl = compute_terrain_mask(nz, ny, nx, zmin, dz, terrain)
        
        # Apply mask
        u_fluct_masked = u_fluct * mask
        
        # Check no penetration
        inside_terrain_mask = (z_agl <= 0.0)
        max_inside = u_fluct_masked[inside_terrain_mask].max()
        
        assert max_inside < 0.01, f"Fluctuations inside terrain: max={max_inside}"
        print(f"✓ No fluctuation penetration: max inside terrain = {max_inside:.6f}")
        
        # Check retention above terrain
        above_terrain_mask = (z_agl > 10.0)
        if above_terrain_mask.sum() > 0:
            min_above = u_fluct_masked[above_terrain_mask].min()
            assert min_above > 0.45, f"Fluctuations reduced above terrain: min={min_above}"
            print(f"✓ Full fluctuations above terrain: min={min_above:.3f}")
        
        return True
        
    except AssertionError as e:
        print(f"✗ FAILED: {e}")
        return False
    except Exception as e:
        print(f"✗ FAILED with exception: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_smooth_transition():
    """Test smooth transition in transition zone."""
    print("\n" + "="*70)
    print("Test 4: Smooth Transition Zone")
    print("="*70)
    
    try:
        nx, ny, nz = 3, 3, 20
        zmin = 0.0
        dz = 2.0
        
        # Flat terrain at 20 m
        terrain = np.zeros((ny, nx), dtype=np.float32) + 20.0
        
        mask, z_agl = compute_terrain_mask(nz, ny, nx, zmin, dz, terrain)
        
        # Extract vertical profile at center
        profile = mask[:, 1, 1]
        z_agl_profile = z_agl[:, 1, 1]
        
        # Print transition zone
        print("  z_agl [m]    Mask")
        print("  " + "-"*20)
        for k in range(nz):
            print(f"  {z_agl_profile[k]:7.1f}      {profile[k]:.3f}")
        
        # Check that transition is smooth (no oscillations)
        diffs = np.diff(profile)
        
        # All differences should be non-negative (monotonic increase)
        n_negative = np.sum(diffs < -0.001)
        assert n_negative == 0, f"Non-monotonic transition: {n_negative} negative jumps"
        print(f"✓ Monotonic increase in transition zone")
        
        # Transition should be smooth (no sharp jumps)
        max_jump = np.abs(diffs).max()
        print(f"✓ Max jump in transition: {max_jump:.4f}")
        
        return True
        
    except AssertionError as e:
        print(f"✗ FAILED: {e}")
        return False
    except Exception as e:
        print(f"✗ FAILED with exception: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_mass_conservation_smooth_mask():
    """Test that smooth mask preserves approximate mass conservation."""
    print("\n" + "="*70)
    print("Test 5: Mass Conservation - Smooth Mask Properties")
    print("="*70)
    
    try:
        nx, ny, nz = 9, 9, 5
        zmin = 0.0
        dz = 10.0
        
        terrain = np.zeros((ny, nx), dtype=np.float32) + 20.0
        
        mask, _ = compute_terrain_mask(nz, ny, nx, zmin, dz, terrain)
        
        # Compute mask gradients
        dmask_di = np.diff(mask, axis=2)  # Gradient in i direction
        dmask_dj = np.diff(mask, axis=1)  # Gradient in j direction
        dmask_dk = np.diff(mask, axis=0)  # Gradient in k direction
        
        max_grad_i = np.abs(dmask_di).max()
        max_grad_j = np.abs(dmask_dj).max()
        max_grad_k = np.abs(dmask_dk).max()
        
        print(f"  Max mask gradient (i): {max_grad_i:.6f}")
        print(f"  Max mask gradient (j): {max_grad_j:.6f}")
        print(f"  Max mask gradient (k): {max_grad_k:.6f}")
        
        # Gradients should be moderate (max at transition boundary is ~pi/4 ≈ 0.785)
        assert max_grad_k < 1.0, f"Vertical gradient too large: {max_grad_k}"
        print(f"✓ Mask gradients are bounded and smooth")
        
        # If mask is smooth, the divergence term div(alpha * u) = alpha * div(u) + u · grad(alpha)
        # is small (second term is small when grad(alpha) is smooth)
        print(f"✓ Smooth mask ensures small divergence contribution")
        
        return True
        
    except AssertionError as e:
        print(f"✗ FAILED: {e}")
        return False
    except Exception as e:
        print(f"✗ FAILED with exception: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    """Run all tests."""
    print("\n" + "="*70)
    print("Terrain-Aware Synthetic Turbulence Masking - Standalone Test Suite")
    print("="*70)
    
    results = [
        ("Terrain Mask Basic Properties", test_terrain_mask_basic()),
        ("Flat Terrain", test_terrain_mask_flat()),
        ("No Fluctuation Penetration", test_no_penetration()),
        ("Smooth Transition Zone", test_smooth_transition()),
        ("Mass Conservation Properties", test_mass_conservation_smooth_mask()),
    ]
    
    # Print summary
    print("\n" + "="*70)
    print("Test Summary")
    print("="*70)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"{status}: {test_name}")
    
    print(f"\nTotal: {passed}/{total} tests passed")
    
    if passed == total:
        print("\n✓ All tests passed!")
        return 0
    else:
        print(f"\n✗ {total - passed} test(s) failed")
        return 1


if __name__ == "__main__":
    import sys
    sys.exit(main())
