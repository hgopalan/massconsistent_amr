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

AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_stability_class_from_richardson(amrex::Real Ri_b);

/**
 * @brief Map Pasquill-Gifford stability class (1-6) and local aerodynamic roughness length (z0)
 * directly to the Monin-Obukhov length (L) using standard empirical Golder (1972) curves.
 * 
 * @param pg_class Stability class ID: 1=A, 2=B, 3=C, 4=D, 5=E, 6=F
 * @param z0       Local aerodynamic roughness length [m]
 * @return         Corresponding Monin-Obukhov length L [m]
 */
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_obukhov_length_golder(int pg_class, amrex::Real z0)
{
    amrex::Real z0_val = std::max(z0, amrex::Real(1.0e-5));
    amrex::Real log_z0 = std::log10(z0_val);
    amrex::Real inv_L = amrex::Real(0.0);

    // 1=A, 2=B, 3=C, 4=D, 5=E, 6=F
    if (pg_class == 1) { // Class A
        inv_L = amrex::Real(0.035) * log_z0 - amrex::Real(0.08);
    } else if (pg_class == 2) { // Class B
        inv_L = amrex::Real(0.018) * log_z0 - amrex::Real(0.045);
    } else if (pg_class == 3) { // Class C
        inv_L = amrex::Real(0.007) * log_z0 - amrex::Real(0.015);
    } else if (pg_class == 4) { // Class D (Neutral)
        inv_L = amrex::Real(0.0);
    } else if (pg_class == 5) { // Class E
        inv_L = -amrex::Real(0.008) * log_z0 + amrex::Real(0.018);
    } else if (pg_class == 6) { // Class F
        inv_L = -amrex::Real(0.035) * log_z0 + amrex::Real(0.075);
    }

    if (std::abs(inv_L) < amrex::Real(1.0e-10)) {
        return amrex::Real(1.0e6); // Neutral
    } else {
        return amrex::Real(1.0) / inv_L;
    }
}

/**
 * @brief Compute Obukhov length from bulk Richardson number.
 * 
 * Maps Ri_b to Pasquill-Gifford stability class, then translates it either dynamically
 * via Golder (1972) curves with local roughness length z0 (if use_golder=true), or falls
 * back to the traditional lookup table of static L values.
 * 
 * @param Ri_b       Bulk Richardson number [dimensionless]
 * @param z0         Local aerodynamic roughness length [m] (defaults to 0.1 m)
 * @param use_golder Toggle to use Golder curves instead of static lookup table (defaults to true)
 * @return           Monin-Obukhov length L [m]
 */
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_obukhov_length_from_richardson(amrex::Real Ri_b, amrex::Real z0 = amrex::Real(0.1), bool use_golder = true)
{
    if (use_golder) {
        int pg_class = static_cast<int>(std::round(get_stability_class_from_richardson(Ri_b)));
        return compute_obukhov_length_golder(pg_class, z0);
    } else {
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
