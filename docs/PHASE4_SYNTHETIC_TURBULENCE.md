# Phase 4 Terrain-Aware Synthetic Turbulence

## Overview

Phase 4 extends the synthetic turbulence framework from neutral, height-dependent spectral synthesis to terrain-aware, boundary-layer-aware turbulence generation suitable for complex topography, OpenFAST inflow preparation, and diagnostic visualization. The implementation combines:

- terrain masking so fluctuations exist only in fluid cells,
- smooth boundary blending to avoid discontinuities at terrain intersections,
- height-dependent spectral scaling,
- spatial coherence models for multi-point inflow synthesis,
- anisotropic component ratios for atmospheric boundary layers,
- GPU-capable execution paths for large 3-D domains.

The same framework supports Kaimal, Von Kármán, and Mann-style anisotropic spectral models while preserving the practical export workflows already used for `.bts` inflow generation.

## Quick Start

Add the following to `inputs.i` for a terrain-aware synthesis run:

```ini
terrain_file = terrain.csv
U_ref = 10.0
V_ref = 0.0
z_ref = 10.0
z0 = 0.05

enable_synthetic_turbulence = true
turbulence_model = kaimal
terrain_aware_masking = true
enable_boundary_blending = true
export_turbulence_bts = true
turbulence_bts_file = synthetic_turbulence.bts

# Compatible legacy/export controls
# turbulence_spectrum_model = Kaimal
# turbulence_export_format = bts
# turbulence_output_file = synthetic_turbulence.bts
```

Recommended first verification steps:

1. confirm the terrain grid and domain height cover the full hill and blending zone,
2. inspect extracted `u`, `v`, and `w` fluctuation statistics above the terrain crest and in valleys,
3. validate that solid cells remain zeroed before exporting the BTS inflow.

## Synthetic Turbulence Capabilities

The current turbulence stack supports:

- **Kaimal spectra** for engineering inflow and IEC-style surface-layer applications.
- **Von Kármán spectra** for classic isotropic spectral synthesis.
- **Mann box anisotropic spectra** for strongly directional, shear-sensitive inflow boxes.
- **Height-dependent intensity and correlation lengths** so turbulence evolves with height above ground level.
- **Coherence models** for vertical and lateral correlation decay across the rotor or terrain-following slices.
- **Terrain-aware masking** that prevents synthetic fluctuations from leaking into terrain or EB-solid regions.
- **OpenFAST/TurbSim export** through binary `.bts` output for downstream aeroelastic workflows.

## Terrain-Aware Masking Implementation

Terrain awareness is applied after the baseline spectral field is assembled and before export or visualization:

1. compute the local height above ground, `z_agl = z_cell - z_terrain(x, y)`,
2. set turbulence to zero for all cells where `z_agl <= 0`,
3. apply a smooth cosine blend in a thin transition layer above the terrain,
4. leave fully fluid cells unchanged once `z_agl` exceeds the blending height.

A practical blending factor is:

```text
alpha(z_agl) = 0                                  for z_agl <= 0
alpha(z_agl) = 0.5 * (1 - cos(pi z_agl / h_blend)) for 0 < z_agl < h_blend
alpha(z_agl) = 1                                  for z_agl >= h_blend
```

This approach avoids unphysical step changes in `u'`, `v'`, and `w'`, especially at hill crests, escarpments, and terrain-intersecting inflow planes.

### Terrain Masking Explanation

Terrain-aware masking is designed to solve two practical issues:

- **solid-zone contamination**: unmasked synthesis can place fluctuations inside terrain or embedded-boundary solids;
- **hard clipping artifacts**: abrupt truncation creates unrealistically large gradients and export noise.

The smooth blending zone acts like a numerical buffer layer. It preserves the intended spectral content above the surface while damping fluctuations continuously as the terrain boundary is approached.

## Height-Dependent Spectrum Models

## Kaimal

Use the Kaimal model when the site is surface-layer dominated, the inflow must resemble IEC/TurbSim practice, or engineering comparisons depend on standard low-frequency behavior.

Typical advantages:

- strong compatibility with wind-energy workflows,
- easy interpretation of length-scale parameters,
- good fit for atmospheric surface-layer measurements.

## Von Kármán

Use Von Kármán when a smoother isotropic reference spectrum is preferred or when comparing against historical wind-engineering benchmarks.

Typical advantages:

- simple isotropic baseline,
- robust behavior for neutral conditions,
- useful as a regression target for spectral-energy checks.

## Mann

Use the Mann model when anisotropy, shear sensitivity, and cross-component structure matter more than a simple scalar spectrum.

Typical advantages:

- realistic anisotropic tensor structure,
- useful for complex-terrain inflow studies,
- better alignment with turbulence-box workflows.

### Model Selection Guide

| Scenario | Recommended model | Why |
|---|---|---|
| Standard turbine inflow over moderate terrain | Kaimal | Closest to IEC/TurbSim practice |
| Neutral benchmark or academic comparison | Von Kármán | Clean isotropic reference |
| Complex terrain with directional anisotropy | Mann | Best tensor-style anisotropic representation |
| Rapid regression or parser validation | Kaimal or Von Kármán | Fast and deterministic |
| Terrain-masked inflow box reuse | Mann + terrain mask | Preserves box structure while clipping solids |

## Coherence and Phase Relationships

Synthetic turbulence is not only about one-point spectra. Terrain-following inflow requires coherent structure across space:

- **vertical coherence** controls how fast fluctuations decorrelate with height,
- **lateral coherence** controls spanwise inflow structure,
- **phase relationships** determine whether multi-point synthesis preserves realistic cross-component timing.

The implemented coherence options support Gaussian-, exponential-, and power-law-like decay behavior. For terrain-aware synthesis, coherence length scales should be interpreted in local AGL coordinates, not global elevation. That keeps ridge-top and valley cells physically comparable after masking.

Recommended checks:

- coherence should decrease monotonically with point separation,
- `u` coherence should remain stronger than `v` and `w` coherence,
- phase continuity should remain smooth across the terrain-blending layer.

## Boundary Layer Effects on Turbulence

Boundary-layer physics directly modify the synthetic field:

- turbulence intensity usually decreases or redistributes with height,
- integral length scales increase or decrease depending on stability,
- anisotropy typically follows `σu > σv > σw`,
- stable conditions shorten coherence scales,
- unstable conditions expand energetic structures aloft.

For terrain-aware runs, use AGL-based heights whenever possible. This is especially important over Gaussian hills, ridges, and valley floors where the same absolute elevation may belong to different parts of the surface layer.

## GPU Acceleration Support

The solver already supports GPU-oriented execution backends (CUDA/HIP/SYCL). Terrain-aware synthetic turbulence should use the same philosophy:

- compute masks and blending factors with element-wise kernels,
- keep spectral synthesis and masking on-device when possible,
- avoid unnecessary host/device transfers before BTS export,
- compare CPU and GPU output with tight tolerances on deterministic seeds.

Expected scaling depends on domain size. Small regression domains may show little benefit, while large 3-D inflow boxes commonly see order-of-magnitude speedups. As a rule of thumb:

- **small validation domains**: CPU often sufficient,
- **production inflow boxes**: GPU can provide roughly **10-100×** speedup when memory movement is controlled.

## Output Formats

| Format | Purpose | Notes |
|---|---|---|
| Binary `.bts` | Primary OpenFAST/TurbSim inflow export | Direct regression target and main supported interchange format |
| ASCII tables | Lightweight diagnostics and profile inspection | Useful for debug slices, spectra, and extracted fluctuation statistics |
| HDF5 | Structured post-processing and archival workflows | Best for large multi-field datasets or coupling pipelines |

When documenting results, clearly distinguish between:

- **native solver output**,
- **post-processed diagnostic exports**, and
- **downstream inflow products**.

## Example Workflows

### 1. Gaussian hill regression case

1. run a coarse terrain-following case,
2. enable synthetic turbulence with `terrain_aware_masking = true`,
3. export `synthetic_turbulence.bts`,
4. inspect a terrain-masked `u/v/w` slice near the hill crest.

### 2. Turbine inflow over complex topography

1. choose Kaimal or Mann depending on certification vs. anisotropic realism,
2. calibrate coherence lengths and anisotropy ratios,
3. validate zero turbulence in terrain/solid cells,
4. export `.bts` for OpenFAST.

### 3. Multi-backend regression

1. run identical seeds on CPU and GPU,
2. compare mask fields and BTS headers exactly,
3. compare fluctuation arrays within floating-point tolerance,
4. track runtime for scaling regressions.

## Performance Considerations

- Keep regression domains intentionally small; target runtime should remain below 30 seconds.
- Use deterministic seeds for CPU/GPU parity checks.
- Prefer coarse-but-physical blending zones during regression.
- Validate spectra and coherence on sampled heights instead of full high-resolution FFT diagnostics.
- Export only the fields needed for downstream checks during regtests.

## Physical Validation References

The following references are appropriate anchors for validation narratives:

1. Kaimal, J. C. et al. (1972), *Spectral characteristics of surface-layer turbulence*.
2. von Kármán, T. (1948), *Progress in the statistical theory of turbulence*.
3. Mann, J. (1994), *The spatial structure of neutral atmospheric surface-layer turbulence*.
4. IEC 61400-1:2019 wind turbine design requirements.
5. Panofsky, H. A. and Dutton, J. A. (1984), *Atmospheric Turbulence*.

For terrain-aware validation, combine these references with site-specific checks on masking, crest/valley coherence decay, and anisotropy retention after boundary blending.
