#!/usr/bin/env python3
"""
Chemistry Builder Utility
==========================

Interactive tool to construct chemistry matrices for atmospheric dispersion models.
Supports template-based reaction networks (SOx, NOx, tropospheric ozone, etc.) and
user-specified reaction rates.

Features:
- Pre-built chemistry templates (SOx-only, NOx-only, full tropospheric)
- Interactive reaction configuration
- Rate constant database with temperature/RH dependence
- CSV export compatible with puff model
- Validation against atmospheric chemistry literature

Usage:
    python chemistry_builder.py --template soxnox --output chemistry.csv
    python chemistry_builder.py --interactive    # Interactive mode
    python chemistry_builder.py --validate chemistry.csv
"""

import sys
import argparse
import csv
import json
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from enum import Enum


class ReactionType(Enum):
    """Supported reaction types."""
    OXIDATION = "oxidation"
    DECOMPOSITION = "decomposition"
    GAS_TO_PARTICLE = "gas_to_particle"
    SCAVENGING = "scavenging"
    EQUILIBRIUM = "equilibrium"


@dataclass
class Reaction:
    """Represents a single chemical reaction."""
    reaction_id: str
    reaction_type: str
    reactants: List[str]
    products: List[str]
    rate_constant: float           # [1/s]
    temp_coeff: float              # Temperature coefficient [1/K]
    rh_coeff: float                # Relative humidity coefficient [1/%]
    reference_temp: float = 298.15 # [K]
    reference_rh: float = 50.0     # [%]
    comments: str = ""
    
    def to_csv_row(self) -> List[str]:
        """Convert to CSV row format."""
        return [
            self.reaction_id,
            self.reaction_type,
            ",".join(self.reactants),
            ",".join(self.products),
            f"{self.rate_constant:.6e}",
            f"{self.temp_coeff:.6e}",
            f"{self.rh_coeff:.6e}",
            self.comments
        ]
    
    @classmethod
    def from_csv_row(cls, row: List[str]) -> "Reaction":
        """Create from CSV row format."""
        return cls(
            reaction_id=row[0].strip(),
            reaction_type=row[1].strip(),
            reactants=[s.strip() for s in row[2].split(",")],
            products=[s.strip() for s in row[3].split(",")],
            rate_constant=float(row[4]),
            temp_coeff=float(row[5]),
            rh_coeff=float(row[6]),
            comments=row[7] if len(row) > 7 else ""
        )


# Pre-built chemistry templates

TEMPLATE_SOX_ONLY = [
    Reaction(
        reaction_id="r1",
        reaction_type="oxidation",
        reactants=["SO2"],
        products=["SO4"],
        rate_constant=0.001,
        temp_coeff=0.04,
        rh_coeff=-0.005,
        comments="SO2 oxidation to sulfate (T and RH dependent)"
    ),
    Reaction(
        reaction_id="r2",
        reaction_type="decomposition",
        reactants=["SO4"],
        products=["SO2"],
        rate_constant=0.00001,
        temp_coeff=0.01,
        rh_coeff=0.0,
        comments="Sulfate decomposition (slow)"
    ),
]

TEMPLATE_NOX_ONLY = [
    Reaction(
        reaction_id="r1",
        reaction_type="oxidation",
        reactants=["NOx"],
        products=["HNO3"],
        rate_constant=0.0007,
        temp_coeff=0.035,
        rh_coeff=-0.003,
        comments="NOx oxidation to nitric acid"
    ),
    Reaction(
        reaction_id="r2",
        reaction_type="gas_to_particle",
        reactants=["HNO3"],
        products=["NO3"],
        rate_constant=0.002,
        temp_coeff=0.02,
        rh_coeff=0.01,
        comments="HNO3 to nitrate particle conversion (RH dependent)"
    ),
]

TEMPLATE_SOXNOX = TEMPLATE_SOX_ONLY + TEMPLATE_NOX_ONLY

TEMPLATE_FULL_TROPOSPHERIC = [
    # SOx pathway
    Reaction(
        reaction_id="r1",
        reaction_type="oxidation",
        reactants=["SO2"],
        products=["SO4"],
        rate_constant=0.001,
        temp_coeff=0.04,
        rh_coeff=-0.005,
        comments="SO2 oxidation"
    ),
    # NOx pathway
    Reaction(
        reaction_id="r2",
        reaction_type="oxidation",
        reactants=["NOx"],
        products=["HNO3"],
        rate_constant=0.0007,
        temp_coeff=0.035,
        rh_coeff=-0.003,
        comments="NOx oxidation"
    ),
    Reaction(
        reaction_id="r3",
        reaction_type="gas_to_particle",
        reactants=["HNO3"],
        products=["NO3"],
        rate_constant=0.002,
        temp_coeff=0.02,
        rh_coeff=0.01,
        comments="HNO3 to nitrate conversion"
    ),
    # Ozone destruction
    Reaction(
        reaction_id="r4",
        reaction_type="decomposition",
        reactants=["O3"],
        products=["O2"],
        rate_constant=0.0001,
        temp_coeff=0.01,
        rh_coeff=0.0,
        comments="Ozone destruction (slow)"
    ),
]

TEMPLATES = {
    "sox": TEMPLATE_SOX_ONLY,
    "nox": TEMPLATE_NOX_ONLY,
    "soxnox": TEMPLATE_SOXNOX,
    "full": TEMPLATE_FULL_TROPOSPHERIC,
}


def write_chemistry_csv(
    reactions: List[Reaction],
    output_file: str,
    description: str = "Chemistry matrix"
) -> None:
    """
    Write chemistry matrix to CSV file.
    
    Parameters
    ----------
    reactions : List[Reaction]
        List of reaction definitions
    output_file : str
        Output CSV file path
    description : str
        Optional description
    """
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        
        # Write metadata header
        writer.writerow(['# Chemistry Matrix for Puff Model'])
        writer.writerow(['# Description:', description])
        writer.writerow(['# Generated by chemistry_builder.py'])
        writer.writerow([])
        
        # Write column headers with units and descriptions
        writer.writerow([
            'reaction_id',
            'reaction_type',
            'reactants',
            'products',
            'rate_constant [1/s]',
            'temp_coeff [1/K]',
            'rh_coeff [1/%]',
            'comments'
        ])
        
        # Write reactions
        for reaction in reactions:
            writer.writerow(reaction.to_csv_row())
    
    print(f"✓ Wrote chemistry matrix to {output_file}")
    print(f"  {len(reactions)} reactions")


def read_chemistry_csv(filename: str) -> List[Reaction]:
    """
    Read chemistry matrix from CSV file.
    
    Parameters
    ----------
    filename : str
        Input CSV file path
    
    Returns
    -------
    List[Reaction]
        List of reaction definitions
    """
    reactions = []
    
    with open(filename, 'r') as f:
        reader = csv.reader(f)
        
        # Skip header comments
        for row in reader:
            if row and not row[0].startswith('#'):
                break
        
        # Skip column headers
        if row and not row[0].startswith('#'):
            # Process first data row
            if row[0] != 'reaction_id':
                reactions.append(Reaction.from_csv_row(row))
            else:
                # This was header, skip
                pass
        
        # Read remaining reactions
        for row in reader:
            if row and not row[0].startswith('#'):
                reactions.append(Reaction.from_csv_row(row))
    
    return reactions


def validate_chemistry_csv(filename: str) -> Tuple[bool, List[str]]:
    """
    Validate chemistry CSV file.
    
    Parameters
    ----------
    filename : str
        CSV file to validate
    
    Returns
    -------
    Tuple[bool, List[str]]
        (is_valid, list_of_errors)
    """
    errors = []
    
    try:
        reactions = read_chemistry_csv(filename)
    except Exception as e:
        return False, [f"Failed to read file: {str(e)}"]
    
    if not reactions:
        errors.append("No reactions found in file")
        return False, errors
    
    for i, rxn in enumerate(reactions):
        # Check reaction_id is unique
        if i > 0 and any(r.reaction_id == rxn.reaction_id for r in reactions[:i]):
            errors.append(f"Duplicate reaction_id: {rxn.reaction_id}")
        
        # Check reaction type is valid
        valid_types = [rt.value for rt in ReactionType]
        if rxn.reaction_type not in valid_types:
            errors.append(f"Reaction {rxn.reaction_id}: invalid type '{rxn.reaction_type}'")
        
        # Check rate constant is positive
        if rxn.rate_constant <= 0:
            errors.append(f"Reaction {rxn.reaction_id}: rate_constant must be positive")
        
        # Check reasonable range for rate constant
        if rxn.rate_constant > 1.0:
            errors.append(f"Reaction {rxn.reaction_id}: rate_constant > 1.0 s⁻¹ (unusually fast)")
        
        # Check temperature coefficient in reasonable range
        if abs(rxn.temp_coeff) > 0.1:
            errors.append(f"Reaction {rxn.reaction_id}: temp_coeff outside typical range ±0.1 K⁻¹")
        
        # Check RH coefficient in reasonable range
        if abs(rxn.rh_coeff) > 0.01:
            errors.append(f"Reaction {rxn.reaction_id}: rh_coeff outside typical range ±0.01 %-⁻¹")
        
        # Check reactants/products are defined
        if not rxn.reactants:
            errors.append(f"Reaction {rxn.reaction_id}: no reactants defined")
        
        if not rxn.products:
            errors.append(f"Reaction {rxn.reaction_id}: no products defined")
    
    is_valid = len(errors) == 0
    return is_valid, errors


def interactive_mode() -> List[Reaction]:
    """
    Interactive mode for building chemistry matrix.
    
    Returns
    -------
    List[Reaction]
        User-defined reactions
    """
    print("\n" + "="*70)
    print("INTERACTIVE CHEMISTRY MATRIX BUILDER")
    print("="*70)
    
    # Choose starting template
    print("\nAvailable templates:")
    for i, (name, template) in enumerate(TEMPLATES.items(), 1):
        print(f"  {i}. {name}: {len(template)} reactions")
    
    choice = input("\nSelect starting template (1-4) or 'new' for blank: ").strip().lower()
    
    if choice == 'new' or choice == '0':
        reactions = []
    else:
        try:
            idx = int(choice) - 1
            template_names = list(TEMPLATES.keys())
            reactions = list(TEMPLATES[template_names[idx]])
            print(f"\n✓ Loaded '{template_names[idx]}' template with {len(reactions)} reactions")
        except (ValueError, IndexError):
            print("Invalid choice, starting with blank")
            reactions = []
    
    # Add/edit reactions
    while True:
        print("\n" + "-"*70)
        print("Current reactions:")
        for i, rxn in enumerate(reactions, 1):
            print(f"  {i}. {rxn.reaction_id}: {'+'.join(rxn.reactants)} → {'+'.join(rxn.products)}")
        
        print("\nOptions:")
        print("  'a' - Add new reaction")
        print("  'e' - Edit reaction")
        print("  'd' - Delete reaction")
        print("  'v' - Validate matrix")
        print("  's' - Save and exit")
        print("  'q' - Quit without saving")
        
        choice = input("\nEnter choice: ").strip().lower()
        
        if choice == 'q':
            return None
        elif choice == 's':
            return reactions
        elif choice == 'a':
            rxn = _interactive_add_reaction()
            if rxn:
                reactions.append(rxn)
        elif choice == 'e':
            idx = input("Reaction number to edit: ").strip()
            try:
                idx = int(idx) - 1
                if 0 <= idx < len(reactions):
                    reactions[idx] = _interactive_edit_reaction(reactions[idx])
            except ValueError:
                print("Invalid number")
        elif choice == 'd':
            idx = input("Reaction number to delete: ").strip()
            try:
                idx = int(idx) - 1
                if 0 <= idx < len(reactions):
                    reactions.pop(idx)
                    print("✓ Reaction deleted")
            except ValueError:
                print("Invalid number")
        elif choice == 'v':
            is_valid, errors = validate_chemistry_csv_from_list(reactions)
            if is_valid:
                print("\n✓ Chemistry matrix is valid!")
            else:
                print("\n✗ Validation errors:")
                for error in errors:
                    print(f"  - {error}")


def _interactive_add_reaction() -> Optional[Reaction]:
    """Interactive dialog to add a new reaction."""
    print("\nAdd New Reaction")
    print("-"*70)
    
    reaction_id = input("Reaction ID (e.g., r1): ").strip()
    
    print("Reaction type:")
    for i, rt in enumerate(ReactionType, 1):
        print(f"  {i}. {rt.value}")
    rt_choice = int(input("Select type (1-5): ")) - 1
    reaction_type = list(ReactionType)[rt_choice].value
    
    reactants = input("Reactants (comma-separated, e.g., SO2,O3): ").strip().split(",")
    reactants = [r.strip() for r in reactants]
    
    products = input("Products (comma-separated, e.g., SO4): ").strip().split(",")
    products = [p.strip() for p in products]
    
    rate_constant = float(input("Rate constant k [1/s] (e.g., 0.001): ").strip())
    temp_coeff = float(input("Temperature coefficient α [1/K] (e.g., 0.04): ").strip())
    rh_coeff = float(input("RH coefficient β [1/%] (e.g., -0.005): ").strip())
    
    comments = input("Comments (optional): ").strip()
    
    return Reaction(
        reaction_id=reaction_id,
        reaction_type=reaction_type,
        reactants=reactants,
        products=products,
        rate_constant=rate_constant,
        temp_coeff=temp_coeff,
        rh_coeff=rh_coeff,
        comments=comments
    )


def _interactive_edit_reaction(reaction: Reaction) -> Reaction:
    """Interactive dialog to edit an existing reaction."""
    print(f"\nEdit Reaction: {reaction.reaction_id}")
    print("-"*70)
    
    print(f"Rate constant [{reaction.rate_constant:.6e}]: ", end="")
    val = input().strip()
    if val:
        reaction.rate_constant = float(val)
    
    print(f"Temperature coeff [{reaction.temp_coeff:.6e}]: ", end="")
    val = input().strip()
    if val:
        reaction.temp_coeff = float(val)
    
    print(f"RH coefficient [{reaction.rh_coeff:.6e}]: ", end="")
    val = input().strip()
    if val:
        reaction.rh_coeff = float(val)
    
    print(f"Comments [{reaction.comments}]: ", end="")
    val = input().strip()
    if val:
        reaction.comments = val
    
    return reaction


def validate_chemistry_csv_from_list(reactions: List[Reaction]) -> Tuple[bool, List[str]]:
    """Validate a list of reactions (same as CSV validation but from list)."""
    errors = []
    
    if not reactions:
        errors.append("No reactions found")
        return False, errors
    
    for i, rxn in enumerate(reactions):
        if i > 0 and any(r.reaction_id == rxn.reaction_id for r in reactions[:i]):
            errors.append(f"Duplicate reaction_id: {rxn.reaction_id}")
        
        valid_types = [rt.value for rt in ReactionType]
        if rxn.reaction_type not in valid_types:
            errors.append(f"Reaction {rxn.reaction_id}: invalid type '{rxn.reaction_type}'")
        
        if rxn.rate_constant <= 0:
            errors.append(f"Reaction {rxn.reaction_id}: rate_constant must be positive")
        
        if rxn.rate_constant > 1.0:
            errors.append(f"Reaction {rxn.reaction_id}: rate_constant > 1.0 s⁻¹ (unusually fast)")
        
        if abs(rxn.temp_coeff) > 0.1:
            errors.append(f"Reaction {rxn.reaction_id}: temp_coeff outside typical range")
        
        if abs(rxn.rh_coeff) > 0.01:
            errors.append(f"Reaction {rxn.reaction_id}: rh_coeff outside typical range")
        
        if not rxn.reactants:
            errors.append(f"Reaction {rxn.reaction_id}: no reactants defined")
        
        if not rxn.products:
            errors.append(f"Reaction {rxn.reaction_id}: no products defined")
    
    return len(errors) == 0, errors


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Chemistry matrix builder for atmospheric dispersion models",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python chemistry_builder.py --template soxnox --output chemistry.csv
  python chemistry_builder.py --interactive
  python chemistry_builder.py --validate chemistry.csv --verbose
        """
    )
    
    parser.add_argument(
        "--template",
        choices=list(TEMPLATES.keys()),
        help="Use pre-built chemistry template"
    )
    parser.add_argument(
        "--output",
        help="Output CSV filename"
    )
    parser.add_argument(
        "--interactive",
        action="store_true",
        help="Interactive mode to build chemistry matrix"
    )
    parser.add_argument(
        "--validate",
        help="Validate existing chemistry CSV file"
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Verbose output"
    )
    
    args = parser.parse_args()
    
    # Interactive mode
    if args.interactive:
        reactions = interactive_mode()
        if reactions is not None:
            output = args.output or "chemistry_interactive.csv"
            write_chemistry_csv(reactions, output)
    
    # Template mode
    elif args.template:
        if not args.output:
            print("Error: --output required with --template")
            sys.exit(1)
        
        reactions = TEMPLATES[args.template]
        write_chemistry_csv(reactions, args.output, description=f"Template: {args.template}")
    
    # Validation mode
    elif args.validate:
        is_valid, errors = validate_chemistry_csv(args.validate)
        
        if is_valid:
            reactions = read_chemistry_csv(args.validate)
            print(f"\n✓ Chemistry matrix '{args.validate}' is valid!")
            print(f"  {len(reactions)} reactions")
            
            if args.verbose:
                print("\nReactions:")
                for rxn in reactions:
                    print(f"  {rxn.reaction_id}: {'+'.join(rxn.reactants)} → {'+'.join(rxn.products)}")
                    print(f"    k={rxn.rate_constant:.3e} s⁻¹, α={rxn.temp_coeff:.3e} K⁻¹, β={rxn.rh_coeff:.3e} %-⁻¹")
        else:
            print(f"\n✗ Validation errors for '{args.validate}':")
            for error in errors:
                print(f"  - {error}")
            sys.exit(1)
    
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
