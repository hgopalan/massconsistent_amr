# Terrain-Aware Turbulence Tests

## Overview

These regression and unit tests cover terrain-aware synthetic turbulence generation over topography. The focus is on masking, smooth near-surface blending, spectral consistency, coherence behavior, anisotropy, export integrity, and CPU/GPU parity.

## How Terrain Masking Works

The masking workflow computes local height above ground for each turbulence cell. Cells inside terrain are forced to zero. Cells in a thin blending layer above terrain are damped smoothly with a cosine ramp so synthetic fluctuations transition continuously from solid to fluid space.

Key outcomes:

- terrain masking prevents unphysical turbulence in solid zones,
- smooth boundary blending avoids sharp discontinuities,
- the fully fluid region preserves the intended turbulence amplitudes.

## Visualization Guide

The terrain-aware scenario is easiest to interpret using three diagnostics:

1. **Mask field** – should be zero inside terrain and approach one aloft.
2. **Component slices (`u`, `v`, `w`)** – should show strongest streamwise variance and suppressed vertical variance.
3. **Height-dependent spectra/coherence** – should demonstrate scale changes with AGL and monotonic coherence decay.

## Troubleshooting Common Issues

- **Turbulence appears inside hills:** verify `terrain_aware_masking = true` and confirm the terrain grid is aligned with the computational mesh.
- **Sharp spikes near the surface:** increase or verify the blending zone thickness and ensure cosine-style blending is applied.
- **Unexpected anisotropy:** check the RMS ratios and compare `σu > σv > σw` after masking, not before.
- **BTS header mismatch:** confirm `nt`, `ny`, `nz`, and component count are written consistently.
- **CPU/GPU mismatch:** use deterministic seeds and compare after host/device conversion.

## Performance Expectations

Small regression cases should complete in a few seconds. Large production boxes are the real target for acceleration; when the turbulence synthesis and masking stay on-device, GPU execution can provide roughly **10-100×** speedup relative to CPU-only workflows.
