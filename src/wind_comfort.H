#ifndef WIND_COMFORT_H
#define WIND_COMFORT_H

#include <AMReX.H>
#include <AMReX_REAL.H>

// ============================================================================
// Pedestrian Wind Comfort Assessment Module
//
// Implements the Lopes comfort criterion (Lopes et al., 2006) for assessing
// pedestrian wind comfort in urban environments around buildings.
//
// References:
//   Lopes, A.S., Miranda, A.I., and Borrego, C., 2006. Pedestrian Wind
//   Comfort in Urban Areas: Assessment on Fernanda Mascarenhas Avenue in
//   Aveiro, Portugal. Journal of Wind Engineering and Industrial Aerodynamics
//   94(12): 904-918.
// ============================================================================

// Comfort classification categories based on discomfort frequency
enum class ComfortCategory : int {
    COMFORTABLE = 0,           // < 1.5% of time (< 54 hours/year)
    SLIGHTLY_UNCOMFORTABLE = 1, // 1.5% - 5% of time (54-180 hours/year)
    UNPLEASANT = 2,            // 5% - 10% of time (180-360 hours/year)
    DANGEROUS = 3               // > 10% of time (> 360 hours/year)
};

// ============================================================================
// Compute Lopes comfort classification
//
// Classifies pedestrian wind comfort based on discomfort frequency criterion:
//   Discomfort Frequency τ = time when U(1.5m) > U_crit / total_time
//
// where:
//   - U(1.5m) is wind speed at head height (1.5 m AGL)
//   - U_crit is discomfort threshold velocity (~5 m/s for general pedestrians)
//   - Total time is annual (8760 hours)
//
// Classification thresholds (Lopes et al., 2006):
//   τ < 1.5%:   Comfortable (unpleasant speeds rare)
//   1.5% ≤ τ < 5%: Slightly Uncomfortable (occasional discomfort)
//   5% ≤ τ < 10%: Unpleasant (frequent discomfort)
//   τ ≥ 10%:   Dangerous (strong wind nuisance)
//
// Inputs:
//   discomfort_frequency - Fraction of time (0-1) when U > U_crit
//
// Returns:
//   ComfortCategory enum indicating comfort classification
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
ComfortCategory compute_lopes_comfort_category(const amrex::Real discomfort_frequency)
{
    // Convert fraction to percentage (0-100)
    const amrex::Real freq_percent = discomfort_frequency * amrex::Real(100.0);
    
    if (freq_percent < amrex::Real(1.5)) {
        return ComfortCategory::COMFORTABLE;
    } else if (freq_percent < amrex::Real(5.0)) {
        return ComfortCategory::SLIGHTLY_UNCOMFORTABLE;
    } else if (freq_percent < amrex::Real(10.0)) {
        return ComfortCategory::UNPLEASANT;
    } else {
        return ComfortCategory::DANGEROUS;
    }
}

// ============================================================================
// Get numeric score for comfort category
//
// Converts ComfortCategory enum to numeric value for gridded output.
// Allows quantitative analysis and visualization of comfort distribution.
//
// Outputs:
//   0 = Comfortable
//   1 = Slightly Uncomfortable
//   2 = Unpleasant
//   3 = Dangerous
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
int comfort_category_to_score(const ComfortCategory category)
{
    return static_cast<int>(category);
}

// ============================================================================
// Get comfort description string
//
// Returns human-readable description of comfort category.
// Note: GPU kernels cannot directly use std::string; this is provided for
// CPU-side post-processing and output generation.
// ============================================================================
inline const char* comfort_category_name(const ComfortCategory category)
{
    switch (category) {
        case ComfortCategory::COMFORTABLE:
            return "Comfortable";
        case ComfortCategory::SLIGHTLY_UNCOMFORTABLE:
            return "Slightly Uncomfortable";
        case ComfortCategory::UNPLEASANT:
            return "Unpleasant";
        case ComfortCategory::DANGEROUS:
            return "Dangerous";
        default:
            return "Unknown";
    }
}

// ============================================================================
// Compute estimated discomfort frequency from wind speed distribution
//
// Estimates annual discomfort frequency from local wind speed statistics
// assuming logarithmic speed distribution with annual wind direction weighting.
//
// Simplified model (for diagnostic output):
//   τ ≈ (1/N) × Σ[1 if U_i > U_crit else 0] for all measurements
//
// In full implementation, this would be computed from long-term statistics
// (e.g., 1-year or multi-year wind speed time series).
//
// Inputs:
//   wind_speed   - Local wind speed at assessment height [m/s]
//   u_crit       - Critical discomfort velocity threshold [m/s], typically 5 m/s
//   freq_ref     - Reference discomfort frequency for scaling (diagnostic)
//
// Returns:
//   Estimated discomfort frequency (0-1, representing fraction of year)
//
// Note: This is a simplified approximation. Full Lopes criterion requires
// historical wind speed time series and directional bin statistics.
// ============================================================================
AMREX_GPU_HOST_DEVICE AMREX_INLINE
amrex::Real estimate_discomfort_frequency(
    const amrex::Real wind_speed,
    const amrex::Real u_crit,
    const amrex::Real freq_ref)
{
    // Simple exceedance model: frequency scales with speed ratio above threshold
    if (wind_speed <= u_crit) {
        return amrex::Real(0.0);
    }
    
    // Frequency grows with speed excess above threshold
    // This is a diagnostic estimate; full model requires time series
    const amrex::Real speed_ratio = wind_speed / u_crit;
    const amrex::Real freq_est = freq_ref * (speed_ratio - amrex::Real(1.0));
    
    // Clamp to [0, 1] range
    return amrex::max(amrex::Real(0.0), amrex::min(freq_est, amrex::Real(1.0)));
}

#endif // WIND_COMFORT_H
