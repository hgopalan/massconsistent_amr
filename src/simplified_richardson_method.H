#ifndef SIMPLIFIED_RICHARDSON_METHOD_H
#define SIMPLIFIED_RICHARDSON_METHOD_H

#include <AMReX_REAL.H>
#include <cmath>

namespace {
    // Lookup table: Bulk Richardson number Ri_b bins and corresponding Obukhov lengths
    // from Holtslag & De Bruin (1988), Turner (1970) Pasquill-Gifford classification
    
    struct RichardsonToStabilityLUT {
        // Stability class boundaries (Turner 1970)
        // Class A (very unstable):  Ri_b < -0.5  => L ≈ -20 m
        // Class B (moderately unstable): -0.5 to -0.1  => L ≈ -50 m
        // Class C (slightly unstable): -0.1 to 0  => L ≈ -300 m
        // Class D (neutral):  0 to 0.5  => L → ∞
        // Class E (slightly stable): 0.5 to 1.0  => L ≈ 200 m
        // Class F (moderately stable):  > 1.0  => L ≈ 50 m
        
        // Ri_b bin boundaries [dimensionless]
        // Values: {very_unstable, mod_unstable, slight_unstable, neutral, slight_stable, mod_stable, very_stable}
        static constexpr amrex::Real Ri_b_bins[7] = {-1.0, -0.5, -0.1, 0.0, 0.5, 1.0, 2.0};
        
        // Obukhov length values [m] corresponding to each Ri_b bin
        // Negative values indicate unstable stratification, positive stable
        static constexpr amrex::Real L_values[7]   = {-20.0, -50.0, -300.0, 1e6, 200.0, 50.0, 20.0};
        
        // Stability function (stability class ID: A=1, B=2, C=3, D=4, E=5, F=6)
        // Maps Ri_b range to Pasquill-Gifford classification
        static constexpr amrex::Real stability_class[7] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 6.0};
    };
}

// Compute Obukhov length from bulk Richardson number using lookup table
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_obukhov_length_from_richardson(amrex::Real Ri_b)
{
    // Linear interpolation in Ri_b bins
    // Returns characteristic Obukhov length for given Richardson number
    
    if (Ri_b <= -1.0) {
        // Very unstable (Class A)
        return -20.0;
    } else if (Ri_b < -0.5) {
        // Interpolate between A and B
        amrex::Real frac = (Ri_b - (-1.0)) / (-0.5 - (-1.0));
        return -20.0 + frac * (-50.0 - (-20.0));
    } else if (Ri_b < -0.1) {
        // Interpolate between B and C
        amrex::Real frac = (Ri_b - (-0.5)) / (-0.1 - (-0.5));
        return -50.0 + frac * (-300.0 - (-50.0));
    } else if (Ri_b < 0.0) {
        // Interpolate between C and D (neutral)
        amrex::Real frac = (Ri_b - (-0.1)) / (0.0 - (-0.1));
        return -300.0 + frac * (1e6 - (-300.0));
    } else if (Ri_b < 0.5) {
        // Interpolate between D and E (slightly stable)
        amrex::Real frac = Ri_b / 0.5;
        return 1e6 * (1.0 - frac) + 200.0 * frac;
    } else if (Ri_b < 1.0) {
        // Interpolate between E and F
        amrex::Real frac = (Ri_b - 0.5) / (1.0 - 0.5);
        return 200.0 + frac * (50.0 - 200.0);
    } else {
        // Stable (Class F)
        return 20.0;
    }
}

// Get Pasquill-Gifford stability class (1-6) from bulk Richardson number
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_stability_class_from_richardson(amrex::Real Ri_b)
{
    // Returns PG stability class: 1=A (very unstable) ... 6=F (stable)
    
    if (Ri_b < -0.5) {
        return 1.0;  // Class A
    } else if (Ri_b < -0.1) {
        return 2.0;  // Class B
    } else if (Ri_b < 0.0) {
        return 3.0;  // Class C
    } else if (Ri_b < 0.5) {
        return 4.0;  // Class D (neutral)
    } else if (Ri_b < 1.0) {
        return 5.0;  // Class E
    } else {
        return 6.0;  // Class F
    }
}

#endif
