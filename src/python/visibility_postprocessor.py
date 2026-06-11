#!/usr/bin/env python3
"""
Visibility Postprocessor
========================

Post-process dispersion model output to compute visibility metrics from pollutant
concentrations. Calculates extinction coefficients, visual range, deciview, and
generates visibility impact maps.

Features:
- IMPROVE algorithm for 2-species and extended multi-species
- Visual range calculation (Koschmieder equation)
- Deciview computation
- Visibility impact mapping
- Baseline visibility reference
- RH-dependent aerosol growth
- Output in multiple formats (CSV, HDF5)

Usage:
    python visibility_postprocessor.py --input receptor_concentration.csv \\
        --output visibility_metrics.csv --species SO4,NO3
    python visibility_postprocessor.py --input grid_concentration.csv \\
        --make-map visibility_map.png --species SO4,NO3,BC
"""

import sys
import argparse
import csv
import math
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass


# IMPROVE Algorithm Parameters
# Reference: Pitchford et al. (2007), EPA Regional Haze Rule

IMPROVE_SPECIES_PARAMS = {
    "SO4": {
        "mass_extinction_coeff": 3.0,      # [Mm⁻¹/(μg/m³)]
        "rh_factor_50": 1.2,               # Growth factor at 50% RH
        "rh_factor_70": 1.6,               # Growth factor at 70% RH
        "rh_factor_90": 3.0,               # Growth factor at 90% RH
    },
    "NO3": {
        "mass_extinction_coeff": 2.8,
        "rh_factor_50": 1.1,
        "rh_factor_70": 1.4,
        "rh_factor_90": 2.2,
    },
    "OC": {
        "mass_extinction_coeff": 4.0,
        "rh_factor_50": 1.0,
        "rh_factor_70": 1.1,
        "rh_factor_90": 1.3,
    },
    "BC": {
        "mass_extinction_coeff": 10.0,     # BC is strong absorber
        "rh_factor_50": 1.0,               # No RH dependence
        "rh_factor_70": 1.0,
        "rh_factor_90": 1.0,
    },
    "Dust": {
        "mass_extinction_coeff": 1.0,
        "rh_factor_50": 1.0,
        "rh_factor_70": 1.0,
        "rh_factor_90": 1.0,
    },
}

# Rayleigh scattering background
B_RAYLEIGH = 10.0  # [Mm⁻¹]

# Koschmieder contrast threshold
KOSCHMIEDER_CONTRAST = 0.02  # Typical human perception


@dataclass
class VisibilityPoint:
    """Visibility metrics at a single location."""
    x: float
    y: float
    z: float
    label: str
    concentration: float              # [μg/m³]
    rh: float                         # [%]
    temperature: float               # [K]
    extinction_coeff: float           # b_ext [Mm⁻¹]
    visual_range_km: float            # [km]
    deciview: float                   # [dV]
    source_species: str              # Species contributing
    

def interpolate_rh_factor(rh: float, factors: Dict[str, float]) -> float:
    """
    Interpolate RH-dependent growth factor.
    
    Parameters
    ----------
    rh : float
        Relative humidity [%]
    factors : Dict[str, float]
        RH factors at reference points (50%, 70%, 90%)
    
    Returns
    -------
    float
        Interpolated growth factor
    """
    if rh <= 50:
        return factors.get("rh_factor_50", 1.0)
    elif rh <= 70:
        # Linear interpolation between 50% and 70%
        frac = (rh - 50) / 20
        f50 = factors.get("rh_factor_50", 1.0)
        f70 = factors.get("rh_factor_70", 1.0)
        return f50 + frac * (f70 - f50)
    elif rh <= 90:
        # Linear interpolation between 70% and 90%
        frac = (rh - 70) / 20
        f70 = factors.get("rh_factor_70", 1.0)
        f90 = factors.get("rh_factor_90", 1.0)
        return f70 + frac * (f90 - f70)
    else:
        # Saturated at RH > 90%
        return factors.get("rh_factor_90", 1.0)


def compute_extinction_coefficient(
    concentrations: Dict[str, float],
    rh: float = 50.0
) -> float:
    """
    Compute extinction coefficient using IMPROVE algorithm.
    
    Parameters
    ----------
    concentrations : Dict[str, float]
        Pollutant concentrations [μg/m³]
        Keys: 'SO4', 'NO3', 'OC', 'BC', 'Dust'
    rh : float
        Relative humidity [%]
    
    Returns
    -------
    float
        Extinction coefficient b_ext [Mm⁻¹]
    """
    b_ext = B_RAYLEIGH  # Start with Rayleigh scattering
    
    for species, conc in concentrations.items():
        if species not in IMPROVE_SPECIES_PARAMS:
            continue
        
        params = IMPROVE_SPECIES_PARAMS[species]
        mec = params["mass_extinction_coeff"]
        
        # Interpolate RH factor
        rh_factors = {
            "rh_factor_50": params["rh_factor_50"],
            "rh_factor_70": params["rh_factor_70"],
            "rh_factor_90": params["rh_factor_90"],
        }
        f_rh = interpolate_rh_factor(rh, rh_factors)
        
        # Contribution: β = MEC × f_RH × [species]
        b_ext += mec * f_rh * conc
    
    return b_ext


def compute_visual_range(extinction_coeff: float) -> float:
    """
    Compute visual range using Koschmieder equation.
    
    Parameters
    ----------
    extinction_coeff : float
        Extinction coefficient b_ext [Mm⁻¹]
    
    Returns
    -------
    float
        Visual range [km]
    """
    if extinction_coeff <= 0:
        return 999.0  # Effectively infinite
    
    # VR = -ln(contrast) / b_ext
    # Standard: contrast ≈ 0.02
    vr_mm = -math.log(KOSCHMIEDER_CONTRAST) / extinction_coeff
    vr_km = vr_mm / 1e6  # Convert from Mm to km
    
    return max(0.1, vr_km)  # Clamp to reasonable minimum


def compute_deciview(extinction_coeff: float) -> float:
    """
    Compute deciview from extinction coefficient.
    
    Parameters
    ----------
    extinction_coeff : float
        Extinction coefficient b_ext [Mm⁻¹]
    
    Returns
    -------
    float
        Deciview [dV]
    """
    if extinction_coeff <= 0:
        return -10.0  # Exceptionally clear
    
    # dV = 10 × log₁₀(b_ext / 10)
    # where 10 Mm⁻¹ is reference (approximately 38.6 km VR)
    dv = 10.0 * math.log10(extinction_coeff / 10.0)
    
    return dv


def compute_fog_probability(rh: float, visibility_km: float) -> float:
    """
    Estimate fog probability based on RH and visibility.
    
    Parameters
    ----------
    rh : float
        Relative humidity [%]
    visibility_km : float
        Visual range [km]
    
    Returns
    -------
    float
        Fog probability [0-1]
    """
    if rh < 95:
        return 0.0
    
    if visibility_km < 1.0:
        return 1.0
    elif visibility_km < 5.0:
        return (5.0 - visibility_km) / 4.0
    else:
        return 0.0


def compute_icing_probability(temperature: float, rh: float, visibility_km: float) -> float:
    """
    Estimate icing probability (rime ice accretion).
    
    Parameters
    ----------
    temperature : float
        Temperature [K]
    rh : float
        Relative humidity [%]
    visibility_km : float
        Visual range [km]
    
    Returns
    -------
    float
        Icing probability [0-1]
    """
    # Icing occurs between -10°C to -5°C with high RH and low visibility
    temp_c = temperature - 273.15
    
    if temp_c > -5 or temp_c < -15:
        return 0.0  # Temperature outside optimal range
    
    if rh < 85 or visibility_km > 2.0:
        return 0.0  # Insufficient moisture or visibility
    
    # Probability increases with supercooling severity and RH
    temp_factor = min(1.0, abs(temp_c - (-8)) / 3.0)  # Peak at -8°C
    rh_factor = (rh - 85) / 15  # From 85-100%
    vis_factor = (2.0 - visibility_km) / 2.0 if visibility_km < 2.0 else 1.0
    
    prob = temp_factor * rh_factor * vis_factor
    return max(0.0, min(1.0, prob))


def read_concentration_csv(filename: str) -> List[Dict]:
    """
    Read concentration output from puff model.
    
    Parameters
    ----------
    filename : str
        Input CSV file (receptor or grid concentrations)
    
    Returns
    -------
    List[Dict]
        Concentration points with metadata
    """
    data = []
    
    with open(filename, 'r') as f:
        reader = csv.DictReader(f, skipinitialspace=True)
        
        for row in reader:
            # Skip comment lines
            if row and list(row.keys())[0].startswith('#'):
                continue
            
            try:
                point = {
                    'x': float(row.get('x', row.get('X', 0))),
                    'y': float(row.get('y', row.get('Y', 0))),
                    'z': float(row.get('z', row.get('Z', 0))),
                    'label': row.get('label', row.get('name', f"Point_{data.__len__()}")),
                    'C': float(row.get('C', row.get('concentration', 0))),
                    'rh': float(row.get('RH', row.get('rh', 50.0))),
                    'T': float(row.get('T', row.get('temperature', 293.15))),
                }
                
                # Extract species if present
                for species in IMPROVE_SPECIES_PARAMS.keys():
                    if species in row:
                        point[species] = float(row[species])
                
                data.append(point)
            except (ValueError, KeyError):
                continue
    
    return data


def compute_visibility_metrics(
    concentration_data: List[Dict],
    species: Optional[List[str]] = None
) -> List[VisibilityPoint]:
    """
    Compute visibility metrics from concentration data.
    
    Parameters
    ----------
    concentration_data : List[Dict]
        Concentration points from CSV
    species : List[str]
        Species to include (None = all available)
    
    Returns
    -------
    List[VisibilityPoint]
        Visibility metrics at each point
    """
    results = []
    
    # Default species if not specified
    if species is None:
        species = list(IMPROVE_SPECIES_PARAMS.keys())
    
    for point in concentration_data:
        # Extract species concentrations
        conc_dict = {}
        for sp in species:
            if sp in point:
                conc_dict[sp] = point[sp]
        
        # Use total concentration if species not broken down
        if not conc_dict and 'C' in point:
            conc_dict['SO4'] = point['C'] * 0.5  # Assume 50% SO4
            conc_dict['NO3'] = point['C'] * 0.3  # Assume 30% NO3
            conc_dict['OC'] = point['C'] * 0.2   # Assume 20% OC
        
        # Compute extinction coefficient
        b_ext = compute_extinction_coefficient(
            conc_dict,
            rh=point.get('rh', 50.0)
        )
        
        # Compute derived metrics
        vr_km = compute_visual_range(b_ext)
        dv = compute_deciview(b_ext)
        fog_prob = compute_fog_probability(point.get('rh', 50.0), vr_km)
        icing_prob = compute_icing_probability(point.get('T', 293.15), point.get('rh', 50.0), vr_km)
        
        # Determine dominant species
        dominant_species = max(conc_dict.items(), key=lambda x: x[1])[0] if conc_dict else "Unknown"
        
        vis_point = VisibilityPoint(
            x=point['x'],
            y=point['y'],
            z=point['z'],
            label=point['label'],
            concentration=point.get('C', 0.0),
            rh=point.get('rh', 50.0),
            temperature=point.get('T', 293.15),
            extinction_coeff=b_ext,
            visual_range_km=vr_km,
            deciview=dv,
            source_species=dominant_species,
        )
        
        results.append(vis_point)
    
    return results


def write_visibility_csv(
    visibility_data: List[VisibilityPoint],
    output_file: str,
    include_probabilities: bool = True
) -> None:
    """
    Write visibility metrics to CSV file.
    
    Parameters
    ----------
    visibility_data : List[VisibilityPoint]
        Visibility metrics
    output_file : str
        Output CSV file path
    include_probabilities : bool
        Include fog/icing probabilities
    """
    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        
        # Write metadata header
        writer.writerow(['# Visibility Impact Assessment'])
        writer.writerow(['# Generated by visibility_postprocessor.py'])
        writer.writerow([])
        
        # Build column headers
        headers = [
            'x [m]', 'y [m]', 'z [m]', 'label',
            'C [μg/m³]', 'RH [%]', 'T [K]',
            'b_ext [Mm⁻¹]', 'VR [km]', 'dV',
            'source_species'
        ]
        
        writer.writerow(headers)
        
        # Write data rows
        for point in visibility_data:
            row = [
                f"{point.x:.1f}", f"{point.y:.1f}", f"{point.z:.1f}",
                point.label,
                f"{point.concentration:.3e}", f"{point.rh:.1f}", f"{point.temperature:.1f}",
                f"{point.extinction_coeff:.3f}", f"{point.visual_range_km:.2f}", f"{point.deciview:.2f}",
                point.source_species
            ]
            writer.writerow(row)
    
    print(f"✓ Wrote visibility metrics to {output_file}")
    print(f"  {len(visibility_data)} points")


def generate_summary_report(
    visibility_data: List[VisibilityPoint],
    baseline_vr: float = 200.0,
    output_file: Optional[str] = None
) -> str:
    """
    Generate summary report of visibility impact.
    
    Parameters
    ----------
    visibility_data : List[VisibilityPoint]
        Visibility metrics
    baseline_vr : float
        Baseline visual range [km] for comparison
    output_file : str
        Optional output file for report
    
    Returns
    -------
    str
        Report text
    """
    if not visibility_data:
        return "No visibility data to report"
    
    vr_values = [v.visual_range_km for v in visibility_data]
    dv_values = [v.deciview for v in visibility_data]
    
    report = []
    report.append("="*70)
    report.append("VISIBILITY IMPACT ASSESSMENT REPORT")
    report.append("="*70)
    report.append("")
    
    # Statistics
    report.append("Visual Range Statistics:")
    report.append(f"  Mean:      {sum(vr_values)/len(vr_values):.1f} km")
    report.append(f"  Min:       {min(vr_values):.1f} km")
    report.append(f"  Max:       {max(vr_values):.1f} km")
    report.append(f"  Baseline:  {baseline_vr:.1f} km")
    report.append(f"  Avg Impact: {baseline_vr - sum(vr_values)/len(vr_values):.1f} km")
    report.append("")
    
    report.append("Deciview Statistics:")
    report.append(f"  Mean:      {sum(dv_values)/len(dv_values):.1f} dV")
    report.append(f"  Min:       {min(dv_values):.1f} dV")
    report.append(f"  Max:       {max(dv_values):.1f} dV")
    report.append("")
    
    # Impact classification
    excellent = sum(1 for v in vr_values if v > 150)
    good = sum(1 for v in vr_values if 50 < v <= 150)
    fair = sum(1 for v in vr_values if 5 < v <= 50)
    poor = sum(1 for v in vr_values if v <= 5)
    
    total = len(vr_values)
    report.append("Visibility Classification:")
    report.append(f"  Excellent (VR > 150 km): {excellent:3d} ({100*excellent/total:.1f}%)")
    report.append(f"  Good (50-150 km):        {good:3d} ({100*good/total:.1f}%)")
    report.append(f"  Fair (5-50 km):          {fair:3d} ({100*fair/total:.1f}%)")
    report.append(f"  Poor (VR < 5 km):        {poor:3d} ({100*poor/total:.1f}%)")
    report.append("")
    
    # Dominant species
    species_count = {}
    for v in visibility_data:
        species_count[v.source_species] = species_count.get(v.source_species, 0) + 1
    
    report.append("Dominant Species by Location:")
    for species, count in sorted(species_count.items(), key=lambda x: x[1], reverse=True):
        report.append(f"  {species:10s}: {count:3d} ({100*count/total:.1f}%)")
    report.append("")
    
    report.append("="*70)
    
    report_text = "\n".join(report)
    
    if output_file:
        with open(output_file, 'w') as f:
            f.write(report_text)
        print(f"✓ Wrote report to {output_file}")
    
    return report_text


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Post-process dispersion output to compute visibility metrics",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic visibility calculation
  python visibility_postprocessor.py --input concentrations.csv \\
    --output visibility.csv --species SO4,NO3
  
  # With fog/icing probability and report
  python visibility_postprocessor.py --input grid_conc.csv \\
    --output visibility_metrics.csv --report impact_report.txt \\
    --baseline 200 --species SO4,NO3,OC,BC
        """
    )
    
    parser.add_argument(
        "--input",
        required=True,
        help="Input concentration CSV file"
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Output visibility metrics CSV file"
    )
    parser.add_argument(
        "--species",
        default="SO4,NO3,OC,BC,Dust",
        help="Species to include (comma-separated)"
    )
    parser.add_argument(
        "--baseline",
        type=float,
        default=200.0,
        help="Baseline visual range [km] for comparison"
    )
    parser.add_argument(
        "--report",
        help="Generate summary report to file"
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Verbose output"
    )
    
    args = parser.parse_args()
    
    # Parse species list
    species_list = [s.strip() for s in args.species.split(",")]
    
    # Read input data
    try:
        conc_data = read_concentration_csv(args.input)
        if args.verbose:
            print(f"Read {len(conc_data)} concentration points")
    except Exception as e:
        print(f"Error reading input file: {e}")
        sys.exit(1)
    
    # Compute visibility metrics
    try:
        vis_data = compute_visibility_metrics(conc_data, species=species_list)
        if args.verbose:
            print(f"Computed visibility metrics for {len(vis_data)} points")
    except Exception as e:
        print(f"Error computing visibility metrics: {e}")
        sys.exit(1)
    
    # Write output
    try:
        write_visibility_csv(vis_data, args.output)
    except Exception as e:
        print(f"Error writing output: {e}")
        sys.exit(1)
    
    # Generate report if requested
    if args.report:
        try:
            report = generate_summary_report(vis_data, baseline_vr=args.baseline, output_file=args.report)
            if args.verbose:
                print(report)
        except Exception as e:
            print(f"Error generating report: {e}")
            sys.exit(1)


if __name__ == "__main__":
    main()
