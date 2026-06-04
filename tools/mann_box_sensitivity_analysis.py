#!/usr/bin/env python3
"""Mann Box Phase 6: Parameter Sensitivity Analysis Tool"""

import sys
import json
import math
import random
from typing import Dict, List, Tuple
from dataclasses import dataclass
from pathlib import Path

@dataclass
class ParameterRange:
    name: str
    min_val: float
    max_val: float
    default: float
    unit: str = ""
    description: str = ""

PARAMETER_RANGES = {
    'U_ref': ParameterRange('U_ref', 5.0, 20.0, 10.0, 'm/s', 'Reference wind speed'),
    'z0': ParameterRange('z0', 0.01, 5.0, 0.1, 'm', 'Aerodynamic roughness'),
    'L_u': ParameterRange('L_u', 100.0, 800.0, 300.0, 'm', 'Length scale u'),
    'L_v': ParameterRange('L_v', 50.0, 500.0, 200.0, 'm', 'Length scale v'),
    'L_w': ParameterRange('L_w', 20.0, 300.0, 120.0, 'm', 'Length scale w'),
    'anisotropy_v': ParameterRange('anisotropy_v', 0.5, 1.0, 0.8, '-', 'v/u ratio'),
    'anisotropy_w': ParameterRange('anisotropy_w', 0.2, 0.8, 0.5, '-', 'w/u ratio'),
    'decay_rate_uy': ParameterRange('decay_rate_uy', 5.0, 30.0, 10.0, 'm', 'Lateral decay'),
    'decay_rate_uz': ParameterRange('decay_rate_uz', 2.0, 15.0, 5.0, 'm', 'Vertical decay'),
    'mann_a': ParameterRange('mann_a', 0.8, 1.2, 1.0, '-', 'Mann param a'),
    'mann_k': ParameterRange('mann_k', 0.8, 1.2, 1.0, '-', 'Mann param k'),
}

class SimplifiedMannBox:
    def __init__(self):
        self.params = {k: v.default for k, v in PARAMETER_RANGES.items()}
    
    def set_parameters(self, **kwargs):
        for key, value in kwargs.items():
            if key in self.params:
                self.params[key] = value
    
    def compute_turbulence_intensity(self) -> float:
        z0 = self.params['z0']
        TI_base = 0.12
        z0_ref = 0.1
        if z0 > 0 and z0_ref > 0:
            log_ratio = math.log(z0 / z0_ref) / math.log(1.0 / z0_ref)
            TI = TI_base * (1.0 + 0.15 * log_ratio)
        else:
            TI = TI_base
        return max(0.05, min(0.40, TI))
    
    def compute_output_metrics(self) -> Dict[str, float]:
        return {
            'turbulence_intensity': self.compute_turbulence_intensity(),
            'integral_length_scale': ((self.params['L_u'] * self.params['L_v'] * self.params['L_w'])**(1.0/3.0)),
            'anisotropy_trace': 1.0 + self.params['anisotropy_v']**2 + self.params['anisotropy_w']**2,
        }

def compute_morris_sensitivity(n_trajectories: int = 20):
    results = {}
    param_list = list(PARAMETER_RANGES.keys())
    random.seed(42)
    
    for param_name in param_list:
        effects = []
        for traj in range(n_trajectories):
            model = SimplifiedMannBox()
            baseline = {p: random.uniform(PARAMETER_RANGES[p].min_val, PARAMETER_RANGES[p].max_val) for p in param_list}
            model.set_parameters(**baseline)
            f_baseline = model.compute_turbulence_intensity()
            
            p_range = PARAMETER_RANGES[param_name]
            delta = (p_range.max_val - p_range.min_val) / 8.0
            perturbed = baseline.copy()
            perturbed[param_name] = max(p_range.min_val, min(p_range.max_val, perturbed[param_name] + delta))
            
            model.set_parameters(**perturbed)
            f_perturbed = model.compute_turbulence_intensity()
            effect = (f_perturbed - f_baseline) / delta if delta > 0 else 0.0
            effects.append(effect)
        
        mu = sum(effects) / len(effects)
        variance = sum((e - mu)**2 for e in effects) / len(effects)
        sigma = math.sqrt(variance)
        mu_star = sum(abs(e) for e in effects) / len(effects)
        results[param_name] = {'mu': mu, 'sigma': sigma, 'mu_star': mu_star}
    
    return results

def main():
    print("\n" + "="*80)
    print("MANN BOX PHASE 6: PARAMETER SENSITIVITY ANALYSIS")
    print("="*80)
    
    print("\nComputing Morris global sensitivity indices...")
    morris_results = compute_morris_sensitivity(n_trajectories=20)
    
    rankings = sorted([(k, v['mu_star'] + 0.5*v['sigma']) for k,v in morris_results.items()], key=lambda x: x[1], reverse=True)
    
    for param_name, indices in morris_results.items():
        print(f"  ✓ {param_name:20s}: μ*={indices['mu_star']:8.4f}, σ={indices['sigma']:8.4f}")
    
    print("\nRanked Parameters (Most to Least Sensitive):")
    for rank, (param_name, score) in enumerate(rankings, 1):
        print(f"  {rank:2d}. {param_name:20s} ({score:.4f})")
    
    output_data = {
        'summary': {'analysis_type': 'Morris GSA', 'n_parameters': len(PARAMETER_RANGES), 'n_trajectories': 20},
        'rankings': [{'parameter': p[0], 'score': p[1]} for p in rankings],
        'morris_indices': {k: {'mu': v['mu'], 'sigma': v['sigma'], 'mu_star': v['mu_star']} for k,v in morris_results.items()},
    }
    
    output_file = Path(__file__).parent / "sensitivity_analysis_results.json"
    with open(output_file, 'w') as f:
        json.dump(output_data, f, indent=2)
    
    print(f"\n✓ Results saved to: {output_file}")
    print("\nSensitivity analysis complete!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
