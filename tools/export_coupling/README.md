# Export & Downstream Coupling Utilities

This directory contains converters and coupling adapters to export converged wind solver solutions to external visualization, wind farm, and dispersion software.

## Utility Scripts & Subfolders

* **`bts_to_vtk.py`**: Converts binary TurbSim/OpenFAST synthetic turbulence BTS files to XML VTK formats for visual profiling in ParaView.
* **`floris_export.py`**: Extracts converged velocities and speed-up ratios at turbine hub heights for FLORIS inputs.
* **`openfast_export.py`**: Standalone serializing driver packaging velocity fluctuations into binary TurbSim layouts.
* **`calpuff_coupling/`**: Complete suite of adapters, receptors statistics, deposition parameters, and meteorology loaders for CALPUFF dispersion modeling integration.
