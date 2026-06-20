#!/usr/bin/env python3
"""
csv_chemistry_loader.py

Part of the CALPUFF model enhancement suite.
Parses CSV species definitions and reaction matrices to simulate complex chemical kinetics.
Generates callable rate functions and ODE integration steps for the multi-species puff model.
"""

import os
import csv
import numpy as np


class CSVChemistryLoader:
    def __init__(self, species_file=None, reactions_file=None):
        self.species = {}
        self.reactions = []
        if species_file:
            self.load_species(species_file)
        if reactions_file:
            self.load_reactions(reactions_file)

    def load_species(self, filepath):
        """
        Loads species from a CSV file.
        Expected format: species_id,name,molar_mass,henry_coeff,diff_coeff,lifetime_hours
        """
        if not os.path.exists(filepath):
            raise FileNotFoundError(f"Species file not found: {filepath}")

        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                # Strip keys and values
                row = {k.strip(): v.strip() for k, v in row.items()}
                sp_id = int(row['species_id'])
                self.species[sp_id] = {
                    'name': row['name'],
                    'molar_mass': float(row['molar_mass']),
                    'henry_coeff': float(row['henry_coeff']),
                    'diff_coeff': float(row['diff_coeff']),
                    'lifetime_hours': float(row['lifetime_hours'])
                }

    def load_reactions(self, filepath):
        """
        Loads chemical reactions from a CSV file.
        Expected format: rxn_id,reactant_1,reactant_2,product_1,product_2,rate_const_ref,temp_coeff
        """
        if not os.path.exists(filepath):
            raise FileNotFoundError(f"Reactions file not found: {filepath}")

        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                row = {k.strip(): v.strip() for k, v in row.items()}
                rxn = {
                    'rxn_id': int(row['rxn_id']),
                    'reactant_1': int(row['reactant_1']) if row.get('reactant_1') else None,
                    'reactant_2': int(row['reactant_2']) if row.get('reactant_2') else None,
                    'product_1': int(row['product_1']) if row.get('product_1') else None,
                    'product_2': int(row['product_2']) if row.get('product_2') else None,
                    'rate_const_ref': float(row['rate_const_ref']),
                    'temp_coeff': float(row['temp_coeff']) if row.get('temp_coeff') else 0.0
                }
                self.reactions.append(rxn)

    def compute_reaction_rates(self, concentrations, temp_k):
        """
        Computes the reaction rates (dC/dt) for each species given current concentrations.
        concentrations: dict of {species_id: concentration}
        temp_k: temperature in Kelvin
        """
        dcdt = {sp_id: 0.0 for sp_id in self.species}
        
        # Add basic lifetime decay
        for sp_id, info in self.species.items():
            lifetime_s = info['lifetime_hours'] * 3600.0
            if lifetime_s > 0:
                decay_rate = concentrations.get(sp_id, 0.0) / lifetime_s
                dcdt[sp_id] -= decay_rate

        # Process reaction matrix
        for rxn in self.reactions:
            k_ref = rxn['rate_const_ref']
            t_coeff = rxn['temp_coeff']
            # Arrhenius/Exponential temperature dependence: k = k_ref * exp(t_coeff * (T - 298.15))
            k = k_ref * np.exp(t_coeff * (temp_k - 298.15))

            r1 = rxn['reactant_1']
            r2 = rxn['reactant_2']
            p1 = rxn['product_1']
            p2 = rxn['product_2']

            # Rate determination
            if r1 is not None and r2 is not None:
                rate = k * concentrations.get(r1, 0.0) * concentrations.get(r2, 0.0)
            elif r1 is not None:
                rate = k * concentrations.get(r1, 0.0)
            else:
                continue

            # Apply rates to reactants
            if r1 is not None:
                dcdt[r1] -= rate
            if r2 is not None:
                dcdt[r2] -= rate

            # Apply rates to products
            if p1 is not None:
                dcdt[p1] += rate
            if p2 is not None:
                dcdt[p2] += rate

        return dcdt

    def integrate_reactions(self, initial_concs, temp_k, dt, steps=10):
        """
        Integrates the chemical system over dt using simple sub-stepping (forward Euler method).
        """
        concs = dict(initial_concs)
        sub_dt = dt / steps
        for _ in range(steps):
            dcdt = self.compute_reaction_rates(concs, temp_k)
            for sp_id in concs:
                concs[sp_id] = max(0.0, concs[sp_id] + dcdt[sp_id] * sub_dt)
        return concs

    def generate_cpp_header(self, output_path):
        """
        Generates a C++ header containing hardcoded reaction and species properties
        to be included directly in the PUFF C++ solver for high-performance execution.
        """
        with open(output_path, 'w') as f:
            f.write("// Auto-generated by csv_chemistry_loader.py. DO NOT EDIT.\n")
            f.write("#ifndef AUTO_CHEMISTRY_H\n")
            f.write("#define AUTO_CHEMISTRY_H\n\n")
            f.write("#include <AMReX_REAL.H>\n\n")
            f.write("namespace Chemistry {\n")
            
            # Species constants
            f.write("    struct SpeciesInfo {\n")
            f.write("        int id;\n")
            f.write("        const char* name;\n")
            f.write("        amrex::Real molar_mass;\n")
            f.write("        amrex::Real henry_coeff;\n")
            f.write("    };\n\n")
            
            f.write(f"    static constexpr int NUM_SPECIES = {len(self.species)};\n")
            f.write("    static const SpeciesInfo SPECIES_TABLE[NUM_SPECIES] = {\n")
            for sp_id, info in self.species.items():
                f.write(f'        {{ {sp_id}, "{info["name"]}", {info["molar_mass"]}, {info["henry_coeff"]} }},\n')
            f.write("    };\n\n")
            
            # Reaction structs
            f.write("    struct Reaction {\n")
            f.write("        int reactant_1;\n")
            f.write("        int reactant_2;\n")
            f.write("        int product_1;\n")
            f.write("        int product_2;\n")
            f.write("        amrex::Real rate_const_ref;\n")
            f.write("        amrex::Real temp_coeff;\n")
            f.write("    };\n\n")
            
            f.write(f"    static constexpr int NUM_REACTIONS = {len(self.reactions)};\n")
            f.write("    static const Reaction REACTION_TABLE[NUM_REACTIONS] = {\n")
            for rxn in self.reactions:
                r1 = rxn['reactant_1'] if rxn['reactant_1'] is not None else -1
                r2 = rxn['reactant_2'] if rxn['reactant_2'] is not None else -1
                p1 = rxn['product_1'] if rxn['product_1'] is not None else -1
                p2 = rxn['product_2'] if rxn['product_2'] is not None else -1
                f.write(f"        {{ {r1}, {r2}, {p1}, {p2}, {rxn['rate_const_ref']}, {rxn['temp_coeff']} }},\n")
            f.write("    };\n")
            f.write("}\n\n")
            f.write("#endif\n")


if __name__ == "__main__":
    loader = CSVChemistryLoader()
    print("CSVChemistryLoader class defined successfully.")
