#ifndef NUMERICAL_DERIVATIVES_H
#define NUMERICAL_DERIVATIVES_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>

namespace NumericalDerivatives {

using amrex::Real;

// WENO 3 stencil for derivative at interior points
// Assumes uniform grid spacing, returns du/dx with WENO-3 approximation
// Uses 3-point stencil: f[-1], f[0], f[+1] with spacing h
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real weno3_deriv(Real fm1, Real f0, Real fp1, Real h) noexcept
{
    // WENO-3 is 3rd order accurate in smooth regions
    // Bias toward central difference with upwind bias for discontinuities
    const Real eps = Real(1.0e-12);
    
    // Compute smoothness indicators for each stencil
    Real IS_forward = (fp1 - f0) * (fp1 - f0);   // smoothness of [f0, fp1]
    Real IS_backward = (f0 - fm1) * (f0 - fm1);  // smoothness of [fm1, f0]
    
    // Weights
    const Real gamma_forward = Real(2.0) / Real(3.0);
    const Real gamma_backward = Real(1.0) / Real(3.0);
    Real w_forward = gamma_forward / ((eps + IS_forward) * (eps + IS_forward));
    Real w_backward = gamma_backward / ((eps + IS_backward) * (eps + IS_backward));
    Real w_sum = w_forward + w_backward;
    w_forward /= w_sum;
    w_backward /= w_sum;
    
    // Stencil derivatives (2nd order)
    Real d_forward = (fp1 - f0) / h;   // forward difference
    Real d_backward = (f0 - fm1) / h;  // backward difference
    
    // WENO combination
    return w_forward * d_forward + w_backward * d_backward;
}

// WENO 5 stencil for derivative at interior points
// Assumes uniform grid spacing, returns du/dx with WENO-5 approximation
// Uses 5-point stencil: f[-2], f[-1], f[0], f[+1], f[+2] with spacing h
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real weno5_deriv(Real fm2, Real fm1, Real f0, Real fp1, Real fp2, Real h) noexcept
{
    // WENO-5 is 5th order accurate in smooth regions, 3rd order near shocks
    const Real eps = Real(1.0e-12);
    
    // Compute smoothness indicators for each sub-stencil
    Real IS_forward = (fp2 - Real(2.0)*fp1 + f0)*(fp2 - Real(2.0)*fp1 + f0)*Real(0.25) + 
                      (Real(3.0)*fp2 - Real(4.0)*fp1 + f0)*(Real(3.0)*fp2 - Real(4.0)*fp1 + f0)/Real(12.0);
    
    Real IS_central = (fp1 - Real(2.0)*f0 + fm1)*(fp1 - Real(2.0)*f0 + fm1)*Real(0.25) + 
                      (fp1 - fm1)*(fp1 - fm1)/Real(12.0);
    
    Real IS_backward = (f0 - Real(2.0)*fm1 + fm2)*(f0 - Real(2.0)*fm1 + fm2)*Real(0.25) + 
                       (Real(3.0)*f0 - Real(4.0)*fm1 + fm2)*(Real(3.0)*f0 - Real(4.0)*fm1 + fm2)/Real(12.0);
    
    // Weights
    const Real gamma_forward = Real(0.1);
    const Real gamma_central = Real(0.6);
    const Real gamma_backward = Real(0.3);
    Real w_forward = gamma_forward / ((eps + IS_forward) * (eps + IS_forward));
    Real w_central = gamma_central / ((eps + IS_central) * (eps + IS_central));
    Real w_backward = gamma_backward / ((eps + IS_backward) * (eps + IS_backward));
    Real w_sum = w_forward + w_central + w_backward;
    w_forward /= w_sum;
    w_central /= w_sum;
    w_backward /= w_sum;
    
    // Stencil derivatives (2nd order accurate)
    Real d_forward = (fp2 - Real(4.0)*fp1 + Real(3.0)*f0) / (Real(2.0)*h);  // forward biased
    Real d_central = (fp1 - fm1) / (Real(2.0)*h);                           // central
    Real d_backward = (Real(-3.0)*f0 + Real(4.0)*fm1 - fm2) / (Real(2.0)*h); // backward biased
    
    // WENO combination
    return w_forward * d_forward + w_central * d_central + w_backward * d_backward;
}

} // namespace NumericalDerivatives

#endif // NUMERICAL_DERIVATIVES_H
