#!/usr/bin/env python3
"""Regression checks for terrain-masked synthetic turbulence."""

from __future__ import annotations

import math
import os
import struct
import sys
import time
from pathlib import Path
from typing import Dict, Tuple

import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO_ROOT / 'src/python'))

from iec61400_models import NormalTurbulenceModel

try:
    import cupy as cp  # type: ignore
except Exception:  # pragma: no cover - optional dependency
    cp = None

HERE = Path(__file__).resolve().parent
TOTAL_TESTS = 0
PASSED_TESTS = 0
FAILED_TESTS = 0


def parse_inputs(path: Path) -> Dict[str, str]:
    data: Dict[str, str] = {}
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith('#') or '=' not in line:
            continue
        key, value = line.split('=', 1)
        data[key.strip()] = value.strip()
    return data


def load_terrain_grid(path: Path) -> np.ndarray:
    rows = []
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        parts = line.replace(',', ' ').split()
        rows.append(tuple(float(v) for v in parts[:3]))
    arr = np.array(rows, dtype=float)
    xs = np.unique(arr[:, 0])
    ys = np.unique(arr[:, 1])
    terrain = np.zeros((len(ys), len(xs)), dtype=float)
    x_map = {x: i for i, x in enumerate(xs)}
    y_map = {y: j for j, y in enumerate(ys)}
    for x, y, z in arr:
        terrain[y_map[y], x_map[x]] = z
    return terrain


def compute_terrain_mask(terrain: np.ndarray, dz: float, nz: int, zmin: float = 0.0) -> Tuple[np.ndarray, np.ndarray]:
    z_centers = zmin + (np.arange(nz) + 0.5) * dz
    transition_cells = max(2, int(math.ceil(30.0 / dz)))
    transition_height = transition_cells * dz
    z_agl = z_centers[:, None, None] - terrain[None, :, :]
    mask = np.ones_like(z_agl, dtype=float)
    mask[z_agl <= 0.0] = 0.0
    transition = (z_agl > 0.0) & (z_agl < transition_height)
    normalized = z_agl[transition] / transition_height
    mask[transition] = 0.5 * (1.0 - np.cos(np.pi * normalized))
    return mask, z_agl


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


def to_numpy(arr):
    if cp is not None and hasattr(arr, 'get'):
        return arr.get()
    return np.asarray(arr)


def correlation(a: np.ndarray, b: np.ndarray) -> float:
    a = np.asarray(a, dtype=float).ravel()
    b = np.asarray(b, dtype=float).ravel()
    a = a - a.mean()
    b = b - b.mean()
    denom = np.linalg.norm(a) * np.linalg.norm(b)
    return float(np.dot(a, b) / denom) if denom > 0 else 0.0


def generate_masked_field(device: str = 'cpu', seed: int = 2026) -> Dict[str, np.ndarray]:
    inputs = parse_inputs(HERE / 'inputs.i')
    terrain = load_terrain_grid(HERE / 'terrain.csv')
    dz = float(inputs.get('dz', '10.0'))
    nz = max(12, int(math.ceil(float(inputs.get('domain_height', '140.0')) / dz)))
    mask, z_agl = compute_terrain_mask(terrain, dz=dz, nz=nz)

    ntm = NormalTurbulenceModel('II', terrain_category=2, z_hub=90.0)
    z_centers = (np.arange(nz) + 0.5) * dz
    base = np.random.default_rng(seed).standard_normal((3, nz, terrain.shape[0], terrain.shape[1]))

    if device == 'gpu' and cp is not None:
        xp = cp
        backend = 'cupy'
        base_backend = xp.asarray(base)
        mask_backend = xp.asarray(mask)
    else:
        xp = np
        backend = 'numpy' if device == 'cpu' else 'numpy-emulated-gpu'
        base_backend = base
        mask_backend = mask

    smoothed = [smooth_horizontal(base_backend[i], xp=xp, passes=3) for i in range(3)]
    components = [xp.zeros_like(smoothed[0]) for _ in range(3)]

    for k, z_center in enumerate(z_centers):
        fluid_heights = z_agl[k][mask[k] > 0.0]
        rep_height = float(fluid_heights.mean()) if fluid_heights.size else float(z_center)
        rep_height = max(rep_height, 1.0)
        mean_speed = float(ntm.power_law_profile(np.array([max(rep_height, 10.0)]), 10.0)[0])
        rms = ntm.compute_velocity_rms(rep_height, mean_speed)
        scales = (rms['u_rms'], rms['v_rms'], rms['w_rms'])
        for idx, scale in enumerate(scales):
            components[idx][k] = smoothed[idx][k] * scale * mask_backend[k]

    u, v, w = (to_numpy(comp) for comp in components)
    return {
        'u': u,
        'v': v,
        'w': w,
        'mask': mask,
        'z_agl': z_agl,
        'terrain': terrain,
        'dz': dz,
        'backend': backend,
    }


def write_bts(path: Path, u: np.ndarray, v: np.ndarray, w: np.ndarray, dt: float, dy: float, dz: float) -> None:
    nt = u.shape[2]
    ny = u.shape[1]
    nz = u.shape[0]
    data = np.stack([u, v, w], axis=-1)
    packed = np.transpose(data, (2, 1, 0, 3)).astype(np.float32)
    with path.open('wb') as handle:
        handle.write(struct.pack('6i', 7, 7, nt, ny, nz, 3))
        handle.write(struct.pack('6f', dt, 10.0, 90.0, dy, dz, 0.05))
        handle.write(struct.pack('f', 0.14))
        handle.write(packed.tobytes())


def validate_bts(path: Path) -> Dict[str, float]:
    with path.open('rb') as handle:
        id1, id2, nt, ny, nz, ncomp = struct.unpack('6i', handle.read(24))
        dt, uhub, zhub, dy, dz, z0 = struct.unpack('6f', handle.read(24))
        (ti,) = struct.unpack('f', handle.read(4))
        payload = handle.read()
    expected_bytes = nt * ny * nz * ncomp * 4
    assert id1 == 7 and id2 == 7
    assert ncomp == 3
    assert len(payload) == expected_bytes
    return {
        'nt': nt,
        'ny': ny,
        'nz': nz,
        'dt': dt,
        'uHub': uhub,
        'zHub': zhub,
        'dy': dy,
        'dz': dz,
        'z0': z0,
        'turbulence_intensity': ti,
    }


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


def run_test(name: str, func) -> None:
    try:
        func()
        print_result(name, True)
    except AssertionError as exc:
        print_result(name, False, str(exc))
    except Exception as exc:  # pragma: no cover - defensive
        print_result(name, False, f'unexpected exception: {exc}')


def test_masking_validation(case: Dict[str, np.ndarray]) -> None:
    solid = case['z_agl'] <= 0.0
    fluid = case['z_agl'] > case['dz']
    for key in ('u', 'v', 'w'):
        assert np.allclose(case[key][solid], 0.0, atol=1e-12), f'{key} not zero in solid cells'
        assert float(np.std(case[key][fluid])) > 1.0e-3, f'{key} unexpectedly weak in fluid cells'


def test_boundary_blending(case: Dict[str, np.ndarray]) -> None:
    peak_j, peak_i = np.unravel_index(np.argmax(case['terrain']), case['terrain'].shape)
    profile = case['mask'][:, peak_j, peak_i]
    diffs = np.diff(profile)
    second_diffs = np.diff(profile, n=2)
    transition = profile[(profile > 0.0) & (profile < 1.0)]
    assert transition.size >= 2, 'expected a resolved transition zone above the hill crest'
    assert np.all(diffs >= -1.0e-10), 'mask must increase monotonically away from terrain'
    assert np.max(np.abs(second_diffs)) < 0.6, 'boundary blend is too sharp'


def test_spectrum_validation() -> None:
    ntm = NormalTurbulenceModel('II', terrain_category=2, z_hub=90.0)
    freqs = np.logspace(-2, 1, 64)
    heights = np.array([20.0, 60.0, 120.0])
    result = ntm.compute_height_dependent_spectrum(freqs, heights, 10.0, spectrum_type='Kaimal', length_scale_u=220.0)
    for idx, height in enumerate(heights):
        mean_speed = 10.0
        u_rms = ntm.compute_velocity_rms(float(height), mean_speed)['u_rms']
        length_scale = float(result['height_scales'][idx])
        f_hat = freqs * length_scale / mean_speed
        manual = 4.0 * length_scale * u_rms ** 2 * f_hat / np.power(1.0 + 6.0 * f_hat, 5.0 / 3.0)
        assert np.allclose(manual, result['spectra_u'][idx], rtol=1e-12, atol=1e-12), 'Kaimal spectrum mismatch'
    low_height_energy = np.trapz(result['spectra_u'][0], freqs)
    high_height_energy = np.trapz(result['spectra_u'][-1], freqs)
    assert not math.isclose(low_height_energy, high_height_energy, rel_tol=0.05), 'height-dependent spectrum should change with height'


def test_anisotropy(case: Dict[str, np.ndarray]) -> None:
    fluid = case['z_agl'] > case['dz']
    sigma_u = float(np.std(case['u'][fluid]))
    sigma_v = float(np.std(case['v'][fluid]))
    sigma_w = float(np.std(case['w'][fluid]))
    assert sigma_u > sigma_v > sigma_w, f'anisotropy ordering violated: {sigma_u}, {sigma_v}, {sigma_w}'
    assert 0.65 < sigma_v / sigma_u < 0.95, 'v/u ratio outside expected range'
    assert 0.35 < sigma_w / sigma_u < 0.65, 'w/u ratio outside expected range'


def test_coherence_decay(case: Dict[str, np.ndarray]) -> None:
    plane = case['u'][-1]
    corrs = [1.0]
    for shift in (1, 2, 3, 4):
        corrs.append(correlation(plane[:, :-shift], plane[:, shift:]))
    corrs = np.array(corrs)
    assert np.all(np.diff(corrs) <= 1.0e-6), f'coherence not decaying monotonically: {corrs}'
    assert corrs[-1] < 0.8, f'far-field coherence too large: {corrs[-1]}'


def test_output_format(case: Dict[str, np.ndarray]) -> None:
    output = HERE / parse_inputs(HERE / 'inputs.i').get('turbulence_bts_file', 'synthetic_turbulence.bts')
    try:
        write_bts(output, case['u'], case['v'], case['w'], dt=0.5, dy=25.0, dz=case['dz'])
        header = validate_bts(output)
        assert header['nt'] == case['u'].shape[2]
        assert header['ny'] == case['u'].shape[1]
        assert header['nz'] == case['u'].shape[0]
    finally:
        if output.exists():
            output.unlink()


def test_gpu_acceleration(case_cpu: Dict[str, np.ndarray]) -> None:
    case_gpu = generate_masked_field(device='gpu', seed=2026)
    for key in ('u', 'v', 'w', 'mask'):
        assert np.allclose(case_cpu[key], case_gpu[key], atol=1.0e-6), f'{key} mismatch between CPU and GPU paths'


def test_performance_metrics() -> None:
    start = time.perf_counter()
    cpu_case = generate_masked_field(device='cpu', seed=2027)
    cpu_time = time.perf_counter() - start

    start = time.perf_counter()
    gpu_case = generate_masked_field(device='gpu', seed=2027)
    gpu_time = time.perf_counter() - start

    assert cpu_time < 5.0, f'CPU generation too slow: {cpu_time:.3f}s'
    assert gpu_time < 5.0, f'GPU generation too slow: {gpu_time:.3f}s'
    assert cpu_case['u'].shape == gpu_case['u'].shape


def main() -> int:
    print('=' * 72)
    print('Terrain-Masked Synthetic Turbulence Regression Test')
    print('=' * 72)

    case_cpu = generate_masked_field(device='cpu', seed=2026)
    run_test('Masking validation', lambda: test_masking_validation(case_cpu))
    run_test('Boundary blending', lambda: test_boundary_blending(case_cpu))
    run_test('Spectrum validation', test_spectrum_validation)
    run_test('Anisotropy validation', lambda: test_anisotropy(case_cpu))
    run_test('Coherence decay', lambda: test_coherence_decay(case_cpu))
    run_test('Output format', lambda: test_output_format(case_cpu))
    run_test('GPU acceleration parity', lambda: test_gpu_acceleration(case_cpu))
    run_test('Performance metrics', test_performance_metrics)

    print("\n" + '=' * 72)
    print(f'Total tests: {TOTAL_TESTS}')
    print(f'Passed:      {PASSED_TESTS}')
    print(f'Failed:      {FAILED_TESTS}')
    print('=' * 72)
    return 0 if FAILED_TESTS == 0 else 1


if __name__ == '__main__':
    raise SystemExit(main())
