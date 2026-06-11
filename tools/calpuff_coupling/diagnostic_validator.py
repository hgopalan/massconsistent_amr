#!/usr/bin/env python3
"""
diagnostic_validator.py

Part of the CALPUFF model enhancement suite.
Performs mass balance tracking (emitted vs. deposited + airborne) and calculates
model validation/intercomparison statistics (Bias, RMSE, Correlation) against observations.
"""

import os
import numpy as np


class DiagnosticValidator:
    def __init__(self):
        pass

    @staticmethod
    def compute_mass_balance(emissions_profile, puffs_state, deposited_mass):
        """
        Computes the mass balance closure fraction of the puff model:
        Closure = (Airborne Mass + Deposited Mass) / Total Emitted Mass
        
        emissions_profile: list of dictionaries {'time': t, 'rate': r}
        puffs_state: list of active puffs (with 'mass' key)
        deposited_mass: total mass deposited on the ground
        """
        # Integrate emissions profile to get total emitted mass
        total_emitted = 0.0
        if emissions_profile:
            for i in range(len(emissions_profile) - 1):
                t0, r0 = emissions_profile[i]['time'], emissions_profile[i]['rate']
                t1, r1 = emissions_profile[i+1]['time'], emissions_profile[i+1]['rate']
                total_emitted += 0.5 * (r0 + r1) * (t1 - t0)

        # Sum active airborne mass
        airborne_mass = sum([p['mass'] for p in puffs_state if 'mass' in p])

        total_tracked = airborne_mass + deposited_mass
        closure_fraction = total_tracked / total_emitted if total_emitted > 0.0 else 1.0

        return {
            'total_emitted': total_emitted,
            'airborne_mass': airborne_mass,
            'deposited_mass': deposited_mass,
            'tracked_mass': total_tracked,
            'closure_fraction': closure_fraction,
            'loss_or_unaccounted': total_emitted - total_tracked
        }

    @staticmethod
    def calculate_validation_metrics(predictions, observations):
        """
        Calculates standard EPA-recommended validation statistics:
        - Mean Bias (MB)
        - Mean Fractional Bias (MFB)
        - Root Mean Square Error (RMSE)
        - Correlation Coefficient (R)
        - Index of Agreement (IOA)
        """
        pred = np.array(predictions)
        obs = np.array(observations)

        if len(pred) != len(obs) or len(pred) == 0:
            raise ValueError("Predictions and observations must be non-empty and of identical length.")

        mean_pred = np.mean(pred)
        mean_obs = np.mean(obs)

        # Mean Bias
        mb = float(np.mean(pred - obs))

        # Root Mean Square Error
        rmse = float(np.sqrt(np.mean((pred - obs) ** 2)))

        # Mean Fractional Bias
        denom_mfb = (pred + obs) / 2.0
        # Avoid division by zero
        valid_mask = denom_mfb > 0.0
        if np.any(valid_mask):
            mfb = float(np.mean((pred[valid_mask] - obs[valid_mask]) / denom_mfb[valid_mask]))
        else:
            mfb = 0.0

        # Correlation Coefficient (Pearson R)
        if np.std(pred) > 0.0 and np.std(obs) > 0.0:
            r = float(np.corrcoef(pred, obs)[0, 1])
        else:
            r = 0.0

        # Index of Agreement (IOA)
        numerator_ioa = np.sum((pred - obs) ** 2)
        denominator_ioa = np.sum((np.abs(pred - mean_obs) + np.abs(obs - mean_obs)) ** 2)
        ioa = float(1.0 - (numerator_ioa / denominator_ioa)) if denominator_ioa > 0.0 else 0.0

        return {
            'mean_predictions': float(mean_pred),
            'mean_observations': float(mean_obs),
            'mean_bias': mb,
            'fractional_bias': mfb,
            'rmse': rmse,
            'correlation': r,
            'index_of_agreement': ioa
        }


if __name__ == "__main__":
    validator = DiagnosticValidator()
    print("DiagnosticValidator class defined successfully.")
