#ifndef MATH_CONSTANTS_H
#define MATH_CONSTANTS_H

#include <AMReX_REAL.H>

namespace MathConstants {
    // Mathematical constant π (pi) with high precision
    // Defined as a constexpr literal for cross-platform compatibility
    // Works on all platforms (Windows, Linux, macOS) and GPU backends
    constexpr amrex::Real pi = amrex::Real(3.14159265358979323846);
}

#endif // MATH_CONSTANTS_H
