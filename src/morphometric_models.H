#ifndef MORPHOMETRIC_MODELS_H
#define MORPHOMETRIC_MODELS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <algorithm>
#include <cmath>

namespace MorphometricModels {

    // Macdonald (1998) model for displacement height (d) and roughness length (z0)
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    void compute_macdonald(
        amrex::Real H,
        amrex::Real lambda_p,
        amrex::Real lambda_f,
        amrex::Real Cd,
        amrex::Real z0_base,
        amrex::Real& d,
        amrex::Real& z0_eff)
    {
        if (H <= amrex::Real(0.0) || lambda_p <= amrex::Real(1.0e-10) || lambda_f <= amrex::Real(1.0e-10)) {
            d = amrex::Real(0.0);
            z0_eff = z0_base;
            return;
        }

        const amrex::Real alpha_mac = amrex::Real(4.43);
        const amrex::Real beta_mac = amrex::Real(1.0);

        // d/H = 1 + alpha^(-lambda_p) * (lambda_p - 1)
        amrex::Real d_over_H = amrex::Real(1.0) + std::pow(alpha_mac, -lambda_p) * (lambda_p - amrex::Real(1.0));
        d_over_H = std::max(amrex::Real(0.0), std::min(d_over_H, amrex::Real(0.9)));
        d = d_over_H * H;

        const amrex::Real one_minus_d_H = amrex::Real(1.0) - d_over_H;
        if (one_minus_d_H > amrex::Real(1.0e-10)) {
            const amrex::Real denom = amrex::Real(0.5) * beta_mac * Cd * one_minus_d_H * lambda_f;
            if (denom > amrex::Real(1.0e-10)) {
                const amrex::Real exponent = -std::sqrt(amrex::Real(1.0) / denom);
                amrex::Real z0_over_H = one_minus_d_H * std::exp(exponent);
                z0_over_H = std::max(amrex::Real(0.001), std::min(z0_over_H, amrex::Real(0.5)));
                z0_eff = z0_over_H * H;
            } else {
                z0_eff = amrex::Real(0.1) * H;
            }
        } else {
            z0_eff = amrex::Real(0.01) * H;
        }

        z0_eff = std::max(z0_eff, z0_base);
    }

    // Kutzbach (1961) / Lettau (1969) model
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    void compute_kutzbach(
        amrex::Real H,
        amrex::Real lambda_p,
        amrex::Real lambda_f,
        amrex::Real /*Cd*/, // unused in Kutzbach, but kept for interface consistency
        amrex::Real z0_base,
        amrex::Real& d,
        amrex::Real& z0_eff)
    {
        if (H <= amrex::Real(0.0) || lambda_f <= amrex::Real(1.0e-10)) {
            d = amrex::Real(0.0);
            z0_eff = z0_base;
            return;
        }

        // Standard representation: d = H * lambda_p
        amrex::Real d_over_H = lambda_p;
        d_over_H = std::max(amrex::Real(0.0), std::min(d_over_H, amrex::Real(0.9)));
        d = d_over_H * H;

        // z0 = 0.5 * H * lambda_f
        amrex::Real z0_over_H = amrex::Real(0.5) * lambda_f;
        z0_over_H = std::max(amrex::Real(0.001), std::min(z0_over_H, amrex::Real(0.5)));
        z0_eff = z0_over_H * H;

        z0_eff = std::max(z0_eff, z0_base);
    }

    // Bottema (1995/1997) model
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    void compute_bottema(
        amrex::Real H,
        amrex::Real lambda_p,
        amrex::Real lambda_f,
        amrex::Real Cd, // drag coefficient, typically 0.8
        amrex::Real z0_base,
        amrex::Real& d,
        amrex::Real& z0_eff)
    {
        if (H <= amrex::Real(0.0) || lambda_p <= amrex::Real(1.0e-10) || lambda_f <= amrex::Real(1.0e-10)) {
            d = amrex::Real(0.0);
            z0_eff = z0_base;
            return;
        }

        // d/H = lambda_p
        amrex::Real d_over_H = lambda_p;
        d_over_H = std::max(amrex::Real(0.0), std::min(d_over_H, amrex::Real(0.9)));
        d = d_over_H * H;

        const amrex::Real kappa = amrex::Real(0.41);
        const amrex::Real one_minus_d_H = amrex::Real(1.0) - d_over_H;

        if (one_minus_d_H > amrex::Real(1.0e-10)) {
            const amrex::Real denom = amrex::Real(0.5) * Cd * lambda_f;
            if (denom > amrex::Real(1.0e-10)) {
                const amrex::Real exponent = -kappa / std::sqrt(denom);
                amrex::Real z0_over_H = one_minus_d_H * std::exp(exponent);
                z0_over_H = std::max(amrex::Real(0.001), std::min(z0_over_H, amrex::Real(0.5)));
                z0_eff = z0_over_H * H;
            } else {
                z0_eff = amrex::Real(0.1) * H;
            }
        } else {
            z0_eff = amrex::Real(0.01) * H;
        }

        z0_eff = std::max(z0_eff, z0_base);
    }

} // namespace MorphometricModels

#endif // MORPHOMETRIC_MODELS_H
