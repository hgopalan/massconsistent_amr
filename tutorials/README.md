# Tutorials - Synthetic Turbulence Framework

This directory contains tutorials and examples for using the synthetic turbulence framework to generate OpenFAST-compatible wind fields.

## Files

### PHASE5_TUTORIAL_SYNTHETIC_TURBULENCE.md
Comprehensive tutorial covering all five phases of the synthetic turbulence framework:
1. **Phase 1**: Turbulence parameter configuration
2. **Phase 2**: Random field synthesis
3. **Phase 3**: Time-series generation
4. **Phase 4**: Validation and physical checks
5. **Phase 5**: Documentation and visualization

Includes:
- Step-by-step workflow
- Physics reference (spectral models, length scales)
- Parameter ranges and typical values
- Troubleshooting guide
- References to published literature

### example_synthetic_turbulence.i
Complete example input file with detailed comments explaining all parameters.

Usage:
```bash
cd /path/to/massconsistent_amr
mkdir -p build
cd build
cmake -S .. -B .
cmake --build . --parallel
cp ../regtest/gaussian_hill/terrain.csv .
./wind_solver ../tutorials/example_synthetic_turbulence.i
```

This generates:
- `turbulence_example.bts` - OpenFAST-compatible binary turbulence file
- `turbulence_example.bts.meta` - ASCII metadata
- `wind_extract_example.csv` - Extracted wind field
- `plt_turbulence_example*` - AMReX plotfiles for visualization

## Visualization

Convert BTS to VTK format for ParaView:
```bash
python3 tools/bts_to_vtk.py turbulence_example.bts turbulence.vtk
```

Open in ParaView:
1. File → Open → select `turbulence.pvd` (for time series)
2. Apply Glyph filter to visualize velocity vectors
3. Color by "magnitude" or "intensity"
4. Animate through time steps

## Next Steps

1. Read [PHASE5_TUTORIAL_SYNTHETIC_TURBULENCE.md](PHASE5_TUTORIAL_SYNTHETIC_TURBULENCE.md)
2. Run the example: `./wind_solver example_synthetic_turbulence.i`
3. Validate output: `ctest -L synthetic_turbulence_full -V`
4. Visualize: `python3 tools/bts_to_vtk.py turbulence_example.bts turbulence.vtk`
5. Load into OpenFAST with `TurbulenceFile = "turbulence_example.bts"`

## References

- von Kármán, T. (1948). Progress in the statistical theory of turbulence. Proc. Natl. Acad. Sci.
- Kaimal, J.C., et al. (1972). Spectral characteristics of surface-layer turbulence. Q. J. R. Meteor. Soc.
- IEC 61400-1 (2019). Wind energy generation systems - Design requirements.
- NREL TurbSim User's Guide
- Pope, S.B. (2000). Turbulent Flows. Cambridge University Press.
