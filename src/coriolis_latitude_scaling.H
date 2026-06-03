#ifndef CORIOLIS_LATITUDE_SCALING_H
#define CORIOLIS_LATITUDE_SCALING_H

#include <AMReX_REAL.H>
#include <cmath>

namespace {
    // Coriolis parameter from latitude
    // f = 2 * Omega * sin(latitude)
    // where Omega = 7.27e-5 rad/s (Earth's angular velocity)
    
    static constexpr amrex::Real EARTH_ANGULAR_VELOCITY = 7.27e-5;  // rad/s
    static constexpr amrex::Real DEG_TO_RAD = 3.14159265358979323846 / 180.0;
}

// Compute Coriolis parameter from latitude
// Physical basis: f = 2Ω sin(φ) where Ω = 7.27e-5 rad/s, φ = latitude
// For northern hemisphere: f > 0; for southern: f < 0
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_coriolis_parameter(amrex::Real latitude_deg)
{
    // latitude_deg: positive for N, negative for S
    // Returns: f [rad/s]
    // Typical values:
    //   At equator (0°): f = 0
    //   At 45° N: f ≈ 1.03e-4 rad/s
    //   At 60° N: f ≈ 1.26e-4 rad/s
    //   At pole (90°): f ≈ 1.45e-4 rad/s
    
    amrex::Real latitude_rad = latitude_deg * DEG_TO_RAD;
    return 2.0 * EARTH_ANGULAR_VELOCITY * std::sin(latitude_rad);
}

// Coriolis parameter at specific latitudes (pre-computed reference values)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real get_coriolis_parameter_reference(amrex::Real latitude_deg)
{
    // Lookup for common latitudes
    // More efficient than computing sin() every time for standard cases
    
    amrex::Real lat_abs = std::abs(latitude_deg);
    
    if (lat_abs < 10.0) {
        // Near equator: f ≈ 0 (compute)
        return compute_coriolis_parameter(latitude_deg);
    } else if (lat_abs < 20.0) {
        // Tropics: f ≈ 2.6e-5 to 5.2e-5
        return compute_coriolis_parameter(latitude_deg);
    } else if (lat_abs < 30.0) {
        // Mid-tropics: f ≈ 7.6e-5
        return compute_coriolis_parameter(latitude_deg);
    } else if (lat_abs < 45.0) {
        // Mid-latitudes: f ≈ 7.6e-5 to 1.03e-4
        return compute_coriolis_parameter(latitude_deg);
    } else if (lat_abs < 60.0) {
        // High mid-latitudes: f ≈ 1.03e-4 to 1.26e-4
        return compute_coriolis_parameter(latitude_deg);
    } else {
        // Polar regions: f ≈ 1.26e-4 to 1.45e-4
        return compute_coriolis_parameter(latitude_deg);
    }
}

// Rossby number (measure of Coriolis importance)
// Ro = U / (f × L)
// Ro << 1: Coriolis important; Ro >> 1: advection dominates
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_rossby_number(
    amrex::Real velocity,           // Wind speed [m/s]
    amrex::Real coriolis_f,         // Coriolis parameter [rad/s]
    amrex::Real length_scale = 1000.0)  // Horizontal length scale [m]
{
    // Prevent division by zero
    if (std::abs(coriolis_f) < 1e-8) {
        return 1e6;  // Infinite Rossby number at equator
    }
    return velocity / (std::abs(coriolis_f) * length_scale);
}

// Inertial oscillation period (time to complete one Coriolis rotation)
// T = 2π / f
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real compute_inertial_period(amrex::Real coriolis_f)
{
    // Returns period in seconds
    // Typical values:
    //   At 45° N: T ≈ 14 hours
    //   At 60° N: T ≈ 12 hours
    //   At 90° (pole): T ≈ 12 hours (actually 24 hours for sidereal day)
    
    if (std::abs(coriolis_f) < 1e-8) {
        return 1e10;  // Very large period at equator
    }
    return 2.0 * 3.14159265358979323846 / std::abs(coriolis_f);
}

#endif
