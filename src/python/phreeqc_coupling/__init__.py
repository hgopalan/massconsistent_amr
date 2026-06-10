"""
PHREEQC Reactive Transport Coupling Module

Provides one-way coupling infrastructure between massconsistent_amr wind solver
and PHREEQC geochemical reactive transport engine for critical mineral studies.

Key Components:
  - geochemical_coupling: Field extraction and atmospheric boundary conditions
  - phreeqc_utils: PHREEQC input generation and validation
  - reactive_transport_coupling: High-level orchestration workflow
  - netcdf_io: Data I/O (NetCDF4 and ASCII formats)

Usage:
  from phreeqc_coupling import ReactiveTransportCoupling, FieldExtractor
  
  coupling = ReactiveTransportCoupling(wind_solver)
  hotspots = coupling.compute_amd_hotspot_map()

References:
  - Parkhurst & Appelo (2013): PHREEQC (Version 3)
  - Businger et al. (1971): Flux-profile relationships in the atmospheric surface layer
  - Stull (2011): An Introduction to Boundary Layer Meteorology

Scientific foundation and architecture described in PHREEQC_COUPLING_GUIDE.md
"""

__version__ = "1.0.0"
__author__ = "massconsistent_amr Team"

# Core imports - wrapped with optional dependency handling
try:
    from .geochemical_coupling import FieldExtractor, AtmosphericField
    __all_core__ = ["FieldExtractor", "AtmosphericField"]
except ImportError as e:
    __all_core__ = []
    _import_error_geochemical = str(e)

try:
    from .phreeqc_utils import PHREEQCGenerator, BoundaryCondition
    __all_phreeqc__ = ["PHREEQCGenerator", "BoundaryCondition"]
except ImportError as e:
    __all_phreeqc__ = []
    _import_error_phreeqc = str(e)

try:
    from .reactive_transport_coupling import ReactiveTransportCoupling
    __all_orchestrator__ = ["ReactiveTransportCoupling"]
except ImportError as e:
    __all_orchestrator__ = []
    _import_error_orchestrator = str(e)

try:
    from .netcdf_io import NetCDFHandler, ASCIIExporter
    __all_io__ = ["NetCDFHandler", "ASCIIExporter"]
except ImportError as e:
    __all_io__ = []
    _import_error_io = str(e)

# Aggregate all successfully imported classes
__all__ = __all_core__ + __all_phreeqc__ + __all_orchestrator__ + __all_io__

# Provide helpful error messaging if critical imports fail
if not __all__:
    import warnings
    warnings.warn(
        "PHREEQC coupling module loaded but no components available. "
        "Check that all dependencies are installed.",
        ImportWarning
    )
