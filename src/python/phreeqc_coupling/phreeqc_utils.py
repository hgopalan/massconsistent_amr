#!/usr/bin/env python3
"""
phreeqc_utils.py - PHREEQC Input File Generation and Reactive Transport Setup

Provides utilities to generate PHREEQC input files with boundary conditions
derived from wind solver outputs. Supports reactive transport simulation
for mineral weathering, acid mine drainage, and critical mineral leaching.

References:
    - Parkhurst & Appelo (2013). Description of the PHREEQC III software
    - Bethke, C.M. (1996). Geochemical reaction modeling
    - Molins & Mayer (2007). Reactive transport modeling of biogeochemical 
      processes: A comparison of approaches, Journal of Contaminant Hydrology
"""

from typing import Dict, List, Tuple, Optional
from pathlib import Path
import re
from dataclasses import dataclass


@dataclass
class BoundaryCondition:
    """Container for PHREEQC boundary condition parameters.
    
    Attributes:
        name (str): BC identifier
        type (str): 'temperature', 'pressure', 'wind', 'precipitation', etc.
        value (float): Scalar value
        profile (List[Tuple]): Optional height-value pairs for vertical profiles
        units (str): Physical units
    """
    name: str
    type: str
    value: float
    profile: Optional[List[Tuple]] = None
    units: str = ""


class PHREEQCGenerator:
    """Generate PHREEQC input files with wind-derived boundary conditions.
    
    This class provides templating and parameter substitution to create
    PHREEQC DAT files with boundary conditions from wind solver outputs.
    """
    
    # PHREEQC template strings with placeholders
    TEMPLATE_AMD_BASE = """TITLE AMD Formation with Wind-Modulated Oxidation
REM Acid mine drainage reactive transport simulation
REM Boundary conditions from massconsistent_amr wind solver

SOLUTION_MASTER_SPECIES
 H H+ 1 1 1
 O H2O 1 -1 1
 Fe Fe+2 1 1 1
 S S+6 1 1 1
 C C+4 1 1 1
 Cl Cl- 1 1 1

SOLUTION 0  Initial solution (background groundwater)
 temp [TEMP_INIT_C]
 pH 6.0
 pe 4.0
 O(0) 1.0 O2(g)
 Fe 1.0e-3
 S 2.0e-3
 Cl 1.0e-2

SOLUTION 1  Surface infiltration (oxidizing)
 temp [TEMP_BC_C]
 pH 5.5
 pe [PE_BC]
 O(0) [O2_CONC_UMOL]  umol/kgw
 Fe [FE_CONC_UMOL]
 S [SO4_CONC_UMOL]
 Cl 1.0e-2

SELECTED_OUTPUT
 -reset false
 -file [OUTPUT_FILE]
 -pH
 -pe
 -charge_balance
 -SO4
 -Fe(+2)
 -Fe(+3)
 -Al
 -alkalinity
 -equilibrium_phases

EQUILIBRIUM_PHASES 0
 Pyrite 0 [PYRITE_MASS]
 Goethite 0
 Jarosite 0

KINETICS 0
 Pyrite
 -tol 1e-6
 -m [PYRITE_KIN_MASS]
 -m0 [PYRITE_KIN_MASS0]
 -parm [K_OXIDATION]

USER_PRINT
 [OUTPUT_CALCULATIONS]

END
"""

    TEMPLATE_LEACHING_BASE = """TITLE Critical Mineral Leaching with Atmospheric Control
REM Heap leach simulation with wind-dependent mass transfer
REM Temperature and humidity from massconsistent_amr

SOLUTION 0  Ore mineralogy
 temp [TEMP_C]
 pH 2.0
 pe 4.0
 C [CO2_FUGACITY]  CO2(g)
 Cl 0.1

SOLUTION 1  Leach solution
 temp [TEMP_C]
 pH 1.5
 pe 3.0
 Cl 0.1

SURFACE 1
 Ore   1e-5   1e-8   1e-3
 >FeOH 1e-5
 >AlOH 1e-5

KINETICS 0
 Mineral_dissolution
 -tol 1e-6
 -parm [KIN_RATE_CONSTANT]
 -parm [KIN_SURFACE_AREA]

USER_PRINT
 REM Output calculations driven by wind parameters
 [OUTPUT_CALCULATIONS]

END
"""

    TEMPLATE_REACTIVE_TRANSPORT = """TITLE 1D Reactive Transport with Wind-Driven Dispersion
REM Vertical leaching column with atmospheric diffusivity
REM Dispersivity from massconsistent_amr K_v field

SELECTED_OUTPUT
 -reset false
 -file [OUTPUT_FILE]
 -x true
 -pH true
 -totals Cl Fe S

TRANSPORT
 -cells [NCELLS]
 -shifts [SHIFTS]
 -punch_cells [PUNCH_CELLS]
 -punch_frequency [PUNCH_FREQ]
 -print_frequency 1000
 -time_step [TIME_STEP]
 -boundary_cond [BC_TYPE]

PRINT
 -reset true
 -status [PRINT_LEVEL]

END
"""

    def __init__(self):
        """Initialize PHREEQC generator."""
        pass
    
    def generate_amd_simulation(self, output_file: str,
                              boundary_conditions: Dict[str, BoundaryCondition],
                              database: str = "phreeqc.dat",
                              simulation_type: str = "batch") -> str:
        """Generate PHREEQC input for AMD formation simulation.
        
        Parameters:
            output_file (str): Output PHREEQC DAT filename
            boundary_conditions (dict): Dictionary of BoundaryCondition objects
                Expected keys: 'temperature', 'pe', 'O2_concentration', etc.
            database (str): PHREEQC database filename
            simulation_type (str): 'batch' or 'transport'
        
        Returns:
            str: Generated input file path
        
        References:
            Nicholson, R.V., Gillham, R.W., & Reardon, E.J. (1990). 
            Pyrite oxidation in carbonate-buffered systems: Experimental 
            kinetics and reaction mechanism. Geochimica et Cosmochimica Acta, 54(2), 395-402.
        """
        # Start with template
        content = self.TEMPLATE_AMD_BASE
        
        # Substitute boundary conditions
        temp_c = boundary_conditions.get('temperature', BoundaryCondition(
            'temperature', 'float', 20.0, units='C')).value
        temp_c_init = 15.0  # Initial condition
        
        o2_conc = boundary_conditions.get('O2_concentration', BoundaryCondition(
            'O2', 'float', 240.0, units='umol/kg')).value
        
        pe = boundary_conditions.get('pe', BoundaryCondition(
            'pe', 'float', 12.0, units='')).value
        
        pyrite_mass = 10.0  # Reference: kg/m³
        pyrite_kin_mass = 5.0
        pyrite_kin_mass0 = 6.0
        k_oxidation = 1e-8  # Reference rate constant
        
        # Perform substitutions
        replacements = {
            '[TEMP_INIT_C]': str(temp_c_init),
            '[TEMP_BC_C]': str(temp_c),
            '[PE_BC]': str(pe),
            '[O2_CONC_UMOL]': str(o2_conc),
            '[FE_CONC_UMOL]': '1.0e-3',
            '[SO4_CONC_UMOL]': '2.0e-3',
            '[PYRITE_MASS]': str(pyrite_mass),
            '[PYRITE_KIN_MASS]': str(pyrite_kin_mass),
            '[PYRITE_KIN_MASS0]': str(pyrite_kin_mass0),
            '[K_OXIDATION]': f'{k_oxidation:.2e}',
            '[OUTPUT_FILE]': f"'{output_file}.out'",
            '[OUTPUT_CALCULATIONS]': self._generate_amd_calculations(),
        }
        
        for placeholder, value in replacements.items():
            content = content.replace(placeholder, value)
        
        # Add database directive
        content = f"DATABASE {database}\n\n" + content
        
        # Write to file
        output_path = Path(output_file)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_path, 'w') as f:
            f.write(content)
        
        print(f"✓ Generated PHREEQC AMD input: {output_path}")
        return str(output_path)
    
    def generate_leaching_simulation(self, output_file: str,
                                    boundary_conditions: Dict[str, BoundaryCondition],
                                    mineral_type: str = "Fe2O3",
                                    database: str = "phreeqc.dat") -> str:
        """Generate PHREEQC input for critical mineral leaching.
        
        Parameters:
            output_file (str): Output PHREEQC DAT filename
            boundary_conditions (dict): Atmospheric BCs (temperature, humidity, etc.)
            mineral_type (str): Primary mineral (e.g., "Fe2O3", "CuCO3·Cu(OH)2")
            database (str): PHREEQC database filename
        
        Returns:
            str: Generated input file path
        
        References:
            Montes-Hernandez, G., et al. (2009). Multisite surface adsorption 
            equilibria of reactive and non-reactive metals on limestone surfaces 
            in aqueous solutions. Geochimica et Cosmochimica Acta, 73(5), 1241-1255.
        """
        content = self.TEMPLATE_LEACHING_BASE
        
        temp_c = boundary_conditions.get('temperature', BoundaryCondition(
            'temperature', 'float', 25.0, units='C')).value
        
        co2_fug = boundary_conditions.get('co2_fugacity', BoundaryCondition(
            'co2', 'float', 3.5, units='atm')).value
        
        kin_rate = 1e-6  # Rate constant [mol/m²/s]
        surface_area = 1000.0  # m²/L
        
        replacements = {
            '[TEMP_C]': str(temp_c),
            '[CO2_FUGACITY]': f'{co2_fug:.2f}',
            '[KIN_RATE_CONSTANT]': f'{kin_rate:.2e}',
            '[KIN_SURFACE_AREA]': str(surface_area),
            '[OUTPUT_FILE]': f"'{output_file}.out'",
            '[OUTPUT_CALCULATIONS]': self._generate_leaching_calculations(),
        }
        
        for placeholder, value in replacements.items():
            content = content.replace(placeholder, value)
        
        content = f"DATABASE {database}\n\n" + content
        
        output_path = Path(output_file)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_path, 'w') as f:
            f.write(content)
        
        print(f"✓ Generated PHREEQC leaching input: {output_path}")
        return str(output_path)
    
    def generate_transport_simulation(self, output_file: str,
                                     boundary_conditions: Dict[str, BoundaryCondition],
                                     ncells: int = 100,
                                     time_step: float = 86400.0,
                                     database: str = "phreeqc.dat") -> str:
        """Generate PHREEQC input for 1D reactive transport.
        
        Parameters:
            output_file (str): Output PHREEQC DAT filename
            boundary_conditions (dict): Transport parameters (dispersivity, etc.)
            ncells (int): Number of cells in 1D column
            time_step (float): Time step [seconds]
            database (str): PHREEQC database filename
        
        Returns:
            str: Generated input file path
        """
        content = self.TEMPLATE_REACTIVE_TRANSPORT
        
        replacements = {
            '[NCELLS]': str(ncells),
            '[SHIFTS]': str(ncells),
            '[PUNCH_CELLS]': ' '.join(str(i) for i in range(0, ncells, max(1, ncells//10))),
            '[PUNCH_FREQ]': '1',
            '[TIME_STEP]': str(time_step),
            '[BC_TYPE]': 'flux',
            '[PRINT_LEVEL]': '1',
            '[OUTPUT_FILE]': f"'{output_file}.out'",
        }
        
        for placeholder, value in replacements.items():
            content = content.replace(placeholder, value)
        
        content = f"DATABASE {database}\n\n" + content
        
        output_path = Path(output_file)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_path, 'w') as f:
            f.write(content)
        
        print(f"✓ Generated PHREEQC transport input: {output_path}")
        return str(output_path)
    
    def insert_boundary_condition(self, template_file: str,
                                 bc_name: str,
                                 bc_value: float,
                                 output_file: str) -> str:
        """Insert boundary condition into existing PHREEQC template.
        
        Parameters:
            template_file (str): Path to template PHREEQC input
            bc_name (str): Boundary condition name (e.g., "temperature")
            bc_value (float): BC value
            output_file (str): Output filename
        
        Returns:
            str: Output file path
        """
        with open(template_file, 'r') as f:
            content = f.read()
        
        # Replace placeholder
        placeholder = f"[{bc_name.upper()}]"
        content = content.replace(placeholder, str(bc_value))
        
        output_path = Path(output_file)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_path, 'w') as f:
            f.write(content)
        
        return str(output_path)
    
    def validate_phreeqc_input(self, input_file: str) -> Tuple[bool, List[str]]:
        """Validate PHREEQC input file syntax.
        
        Parameters:
            input_file (str): Path to PHREEQC DAT file
        
        Returns:
            Tuple[bool, List[str]]: (is_valid, error_messages)
        """
        errors = []
        
        try:
            with open(input_file, 'r') as f:
                content = f.read()
        except FileNotFoundError:
            return False, [f"File not found: {input_file}"]
        
        # Check for common issues
        if 'SOLUTION' not in content:
            errors.append("No SOLUTION block found")
        
        if 'END' not in content:
            errors.append("Missing END statement")
        
        # Check for unmatched brackets
        bracket_count = content.count('[') - content.count(']')
        if bracket_count != 0:
            errors.append(f"Unmatched brackets: {bracket_count}")
        
        # Check for required keywords for different simulation types
        if 'KINETICS' in content and 'SELECTED_OUTPUT' not in content:
            errors.append("KINETICS block found but no SELECTED_OUTPUT")
        
        is_valid = len(errors) == 0
        return is_valid, errors
    
    @staticmethod
    def _generate_amd_calculations() -> str:
        """Generate USER_PRINT calculations for AMD simulation.
        
        Returns:
            str: PHREEQC USER_PRINT code snippet
        """
        return """
 REM AMD calculations
 REM Print pH, eh, saturation indices
 10 PRINT "pH = ", -LA("H+"), "  Eh = ", CH("e-"), " V"
 20 PRINT "SO4 = ", TOT("S"), "  Fe(total) = ", TOT("Fe")
 30 PRINT "SI(Pyrite) = ", SI("Pyrite")
 40 PRINT "SI(Goethite) = ", SI("Goethite")
"""
    
    @staticmethod
    def _generate_leaching_calculations() -> str:
        """Generate USER_PRINT calculations for leaching simulation.
        
        Returns:
            str: PHREEQC USER_PRINT code snippet
        """
        return """
 REM Leaching efficiency calculations
 REM Print extraction yield, saturation state
 10 PRINT "pH = ", -LA("H+")
 20 PRINT "Metal extracted = ", TOT("Fe"), " mol/kgw"
 30 REM Saturation state of key minerals
 40 PRINT "SI(Fe(OH)3) = ", SI("Fe(OH)3")
"""
