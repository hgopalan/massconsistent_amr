#!/usr/bin/env python3
"""Unit tests for terrain-aware synthetic turbulence."""

from __future__ import annotations

import math
import struct
import sys
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO_ROOT / 'src/python'))

from iec61400_models import NormalTurbulenceModel
from mann_box import MannBox

try:
    import cupy as cp  # type: ignore
except Exception:  # pragma: no cover - optional dependency
    cp = None

TOTAL_TESTS = 0
PASSED_TESTS = 0
FAILED_TESTS = 0


def compute_terrain_mask(terrain: np.ndarray, dz: float = 10.0, nz: int = 16):
    z_centers = (np.arange(nz) + 0.5) * dz
    transition_height = max(2, int(math.ceil(30.0 / dz))) * dz
    z_agl = z_centers[:, None, None] - terrain[None, :, :]
    mask = np.ones_like(z_agl, dtype=float)
    mask[z_agl <= 0.0] = 0.0
    transition = (z_agl > 0.0) & (z_agl < transition_height)
    normalized = z_agl[transition] / transition_height
    mask[transition] = 0.5 * (1.0 - np.cos(np.pi * normalized))
    return mask, z_agl


def build_gaussian_hill(nx: int = 9, ny: int = 9, spacing: float = 25.0, peak: float = 40.0, sigma: float = 55.0):
    xs = np.arange(nx) * spacing
    ys = np.arange(ny) * spacing
    x0 = xs.mean()
    y0 = ys.mean()
    xx, yy = np.meshgrid(xs, ys)
    rr2 = (xx - x0) ** 2 + (yy - y0) ** 2
    return peak * np.exp(-rr2 / (2.0 * sigma ** 2))


def smooth_horizontal(field, xp, passes: int = 3):
    out = field.copy()
    for _ in range(passes):
        out = (
            4.0 * out
            + xp.roll(out, 1, axis=1)
            + xp.roll(out, -1, axis=1)
            + xp.roll(out, 1, axis=2)
            + xp.roll(out, -1, axis=2)
        ) / 8.0
    return out


def generate_reference_case(device: str = 'cpu', seed: int = 77):
    terrain = build_gaussian_hill()
    mask, z_agl = compute_terrain_mask(terrain)
    ntm = NormalTurbulenceModel('II', terrain_category=2, z_hub=90.0)
    base = np.random.default_rng(seed).standard_normal((3,) + mask.shape)

    if device == 'gpu' and cp is not None:
        xp = cp
        base_backend = xp.asarray(base)
        mask_backend = xp.asarray(mask)
    else:
        xp = np
        base_backend = base
        mask_backend = mask

    smoothed = [smooth_horizontal(base_backend[i], xp=xp, passes=3) for i in range(3)]
    components = [xp.zeros_like(smoothed[0]) for _ in range(3)]

    dz = 10.0
    for k in range(mask.shape[0]):
        heights = z_agl[k][mask[k] > 0.0]
        rep_height = float(heights.mean()) if heights.size else float((k + 0.5) * dz)
        rep_height = max(rep_height, 1.0)
        mean_speed = float(ntm.power_law_profile(np.array([max(rep_height, 10.0)]), 10.0)[0])
        rms = ntm.compute_velocity_rms(rep_height, mean_speed)
        for idx, scale in enumerate((rms['u_rms'], rms['v_rms'], rms['w_rms'])):
            components[idx][k] = smoothed[idx][k] * scale * mask_backend[k]

    def asnumpy(arr):
        return arr.get() if cp is not None and hasattr(arr, 'get') else np.asarray(arr)

    return {
        'terrain': terrain,
        'mask': mask,
        'z_agl': z_agl,
        'u': asnumpy(components[0]),
        'v': asnumpy(components[1]),
        'w': asnumpy(components[2]),
    }


def correlation(a: np.ndarray, b: np.ndarray) -> float:
    a = a.ravel() - np.mean(a)
    b = b.ravel() - np.mean(b)
    denom = np.linalg.norm(a) * np.linalg.norm(b)
    return float(np.dot(a, b) / denom) if denom > 0 else 0.0


def write_bts(path: Path, u: np.ndarray, v: np.ndarray, w: np.ndarray) -> None:
    data = np.stack([u, v, w], axis=-1)
    payload = np.transpose(data, (2, 1, 0, 3)).astype(np.float32)
    with path.open('wb') as handle:
        handle.write(struct.pack('6i', 7, 7, u.shape[2], u.shape[1], u.shape[0], 3))
        handle.write(struct.pack('6f', 0.5, 10.0, 90.0, 25.0, 10.0, 0.05))
        handle.write(struct.pack('f', 0.14))
        handle.write(payload.tobytes())


def validate_bts(path: Path):
    with path.open('rb') as handle:
        ints = struct.unpack('6i', handle.read(24))
        floats = struct.unpack('6f', handle.read(24))
        (ti,) = struct.unpack('f', handle.read(4))
        payload = handle.read()
    nt, ny, nz, ncomp = ints[2], ints[3], ints[4], ints[5]
    assert ints[0] == 7 and ints[1] == 7
    assert ncomp == 3
    assert len(payload) == nt * ny * nz * ncomp * 4
    return floats, ti


def test_terrain_masking_validity():
    """Verify no turbulence in solid cells"""
    case = generate_reference_case()
    solid = case['z_agl'] <= 0.0
    fluid = case['z_agl'] > 10.0
    assert np.allclose(case['u'][solid], 0.0, atol=1e-12)
    assert np.allclose(case['v'][solid], 0.0, atol=1e-12)
    assert np.allclose(case['w'][solid], 0.0, atol=1e-12)
    assert np.std(case['u'][fluid]) > 1.0e-3


def test_boundary_blending_smoothness():
    """Verify smooth transition at terrain boundary"""
    case = generate_reference_case()
    peak = np.unravel_index(np.argmax(case['terrain']), case['terrain'].shape)
    profile = case['mask'][:, peak[0], peak[1]]
    assert np.all(np.diff(profile) >= -1.0e-10)
    assert np.max(np.abs(np.diff(profile, n=2))) < 0.6
    transition = profile[(profile > 0.0) & (profile < 1.0)]
    assert transition.size >= 2


def test_height_dependent_spectrum():
    """Verify spectrum follows theory"""
    ntm = NormalTurbulenceModel('II', terrain_category=2, z_hub=90.0)
    freqs = np.logspace(-2, 1, 80)
    heights = np.array([20.0, 60.0, 120.0])
    result = ntm.compute_height_dependent_spectrum(freqs, heights, 10.0, spectrum_type='Kaimal', length_scale_u=220.0)
    for idx, height in enumerate(heights):
        mean_speed = 10.0
        u_rms = ntm.compute_velocity_rms(float(height), mean_speed)['u_rms']
        length_scale = float(result['height_scales'][idx])
        f_hat = freqs * length_scale / mean_speed
        kaimal = 4.0 * length_scale * u_rms ** 2 * f_hat / np.power(1.0 + 6.0 * f_hat, 5.0 / 3.0)
        assert np.allclose(kaimal, result['spectra_u'][idx], rtol=1e-12, atol=1e-12)
    vk = ntm.compute_height_dependent_spectrum(freqs, heights, 10.0, spectrum_type='VonKarman', length_scale_u=220.0)
    assert not np.allclose(vk['spectra_u'], result['spectra_u'], rtol=0.1)
    assert np.trapz(result['spectra_u'][0], freqs) != np.trapz(result['spectra_u'][-1], freqs)


def test_anisotropy_ratio():
    """Verify velocity component anisotropy"""
    case = generate_reference_case()
    fluid = case['z_agl'] > 10.0
    sigma_u = float(np.std(case['u'][fluid]))
    sigma_v = float(np.std(case['v'][fluid]))
    sigma_w = float(np.std(case['w'][fluid]))
    assert sigma_u > sigma_v > sigma_w
    assert 0.65 < sigma_v / sigma_u < 0.95
    assert 0.35 < sigma_w / sigma_u < 0.65


def test_coherence_spatial_decay():
    """Verify coherence decreases with distance"""
    plane = generate_reference_case()['u'][-1]
    corrs = [1.0]
    for shift in (1, 2, 3, 4):
        corrs.append(correlation(plane[:, :-shift], plane[:, shift:]))
    corrs = np.array(corrs)
    assert np.all(np.diff(corrs) <= 1.0e-6)
    assert corrs[-1] < 0.8


def test_gpu_cpu_consistency():
    """Verify GPU and CPU produce same results"""
    cpu_case = generate_reference_case(device='cpu', seed=101)
    gpu_case = generate_reference_case(device='gpu', seed=101)
    for key in ('u', 'v', 'w', 'mask'):
        assert np.allclose(cpu_case[key], gpu_case[key], atol=1.0e-6)


def test_export_bts_format():
    """Verify .bts export format"""
    case = generate_reference_case()
    path = Path(__file__).resolve().parent / 'unit_test_terrain_aware.bts'
    try:
        write_bts(path, case['u'], case['v'], case['w'])
        header, ti = validate_bts(path)
        assert header[0] > 0.0 and header[3] > 0.0 and header[4] > 0.0
        assert ti > 0.0
    finally:
        if path.exists():
            path.unlink()


def test_mann_box_integration():
    """Verify Mann box can be used with terrain masking"""
    mann = MannBox(length_scale_u=220.0, length_scale_v=150.0, length_scale_w=90.0)
    spectrum = mann.compute_spectrum(np.logspace(-2, 1, 40), height=90.0, mean_wind_speed=10.0)
    case = generate_reference_case()
    box = np.stack([
        np.full_like(case['mask'], math.sqrt(spectrum['variance_u'])),
        np.full_like(case['mask'], math.sqrt(spectrum['variance_v'])),
        np.full_like(case['mask'], math.sqrt(spectrum['variance_w'])),
    ], axis=-1)
    masked_box = box * case['mask'][..., None]
    solid = case['z_agl'] <= 0.0
    assert masked_box.shape == box.shape
    assert np.allclose(masked_box[solid], 0.0, atol=1e-12)
    assert np.all(spectrum['S_uu'] > 0.0)


def print_result(name: str, passed: bool, details: str = '') -> None:
    global TOTAL_TESTS, PASSED_TESTS, FAILED_TESTS
    TOTAL_TESTS += 1
    if passed:
        PASSED_TESTS += 1
        print(f'✓ PASS: {name}')
    else:
        FAILED_TESTS += 1
        print(f'✗ FAIL: {name}')
    if details:
        print(f'  {details}')


def run_test(func) -> None:
    try:
        func()
        print_result(func.__name__, True)
    except AssertionError as exc:
        print_result(func.__name__, False, str(exc))
    except Exception as exc:  # pragma: no cover - defensive
        print_result(func.__name__, False, f'unexpected exception: {exc}')


def main() -> int:
    print('=' * 72)
    print('Terrain-Aware Synthetic Turbulence Unit Tests')
    print('=' * 72)
    for func in [
        test_terrain_masking_validity,
        test_boundary_blending_smoothness,
        test_height_dependent_spectrum,
        test_anisotropy_ratio,
        test_coherence_spatial_decay,
        test_gpu_cpu_consistency,
        test_export_bts_format,
        test_mann_box_integration,
    ]:
        run_test(func)
    print("\n" + '=' * 72)
    print(f'Total tests: {TOTAL_TESTS}')
    print(f'Passed:      {PASSED_TESTS}')
    print(f'Failed:      {FAILED_TESTS}')
    print('=' * 72)
    return 0 if FAILED_TESTS == 0 else 1


if __name__ == '__main__':
    raise SystemExit(main())
