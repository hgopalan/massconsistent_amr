// ============================================================================
// terrain_following_coords.H
// Terrain-following (streamline) coordinate transformation for mass-consistent
// wind solver
//
// Physical Motivation:
//   Mass-consistent solvers work better when coordinates align with flow:
//     - Reduces artificial terrain-induced divergence
//     - Improves numerical stability on steep slopes
//     - More accurate representation of boundary layers
//
// Coordinate Transformation (Mason & King 1985):
//   Transform from Cartesian (x, y, z) to terrain-following (x, y, s)
//   
//   s = z - z_terrain * decay(z)
//   
//   where decay(z) provides smooth transition from terrain-following (surface)
//   to flat (aloft). Typical decay function:
//   
//   decay(z) = exp(-z_agl / H)
//   
//   where H is the decay height scale (typically domain_height / 3).
//
// Jacobian Terms:
//   The divergence operator in terrain-following coordinates requires metric
//   terms from the coordinate transformation. If s = z - z_terrain * f(z),
//   where f(z) = decay function, then:
//   
//   ∂s/∂x = -∂z_terrain/∂x * f(z)
//   ∂s/∂y = -∂z_terrain/∂y * f(z)
//   ∂s/∂z = 1 - z_terrain * f'(z)
//
//   The divergence in terrain-following coordinates becomes:
//   
//   ∇·u = ∂u/∂x + ∂v/∂y + (1/J) * ∂(J*w)/∂s
//   
//   where J = ∂z/∂s is the Jacobian. The modified vertical derivative accounts
//   for the coordinate stretching.
//
// Reference:
//   Mason, P. J., & King, J. C. (1985). Measurements and predictions of flow
//   over a succession of large hills. Boundary-Layer Meteorology, 32(4), 339-359.
//
// ============================================================================

#ifndef TERRAIN_FOLLOWING_COORDS_H
#define TERRAIN_FOLLOWING_COORDS_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>

namespace TerrainFollowingCoords {

using namespace amrex;

// ============================================================================
// Decay function: smooth transition from terrain-following to flat coordinates
// ============================================================================

// Exponential decay function: f(z) = exp(-z_agl / H)
// This provides smooth transition from terrain-following at surface (f=1)
// to flat coordinates aloft (f→0)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real decay_function(Real z_agl, Real decay_height) noexcept
{
    if (z_agl <= Real(0.0)) {
        return Real(1.0);
    }
    return std::exp(-z_agl / decay_height);
}

// Derivative of decay function: f'(z) = -exp(-z_agl / H) / H
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real decay_function_derivative(Real z_agl, Real decay_height) noexcept
{
    if (z_agl <= Real(0.0)) {
        return Real(0.0);
    }
    return -std::exp(-z_agl / decay_height) / decay_height;
}

// ============================================================================
// Coordinate transformation: z → s
// ============================================================================

// Transform from Cartesian z to terrain-following s coordinate
// s = z - z_terrain * f(z_agl)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real cartesian_to_terrain_following(Real z, Real z_terrain, Real z_agl, Real decay_height) noexcept
{
    const Real f = decay_function(z_agl, decay_height);
    return z - z_terrain * f;
}

// Transform from terrain-following s to Cartesian z coordinate
// This requires iterative solution since s = z - z_terrain * f(z - z_terrain)
// For simplicity, we use approximate inverse: z ≈ s + z_terrain * f(s - z_terrain)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real terrain_following_to_cartesian(Real s, Real z_terrain, Real decay_height) noexcept
{
    // First-order approximation
    const Real z_agl_approx = s - z_terrain;
    const Real f = decay_function(z_agl_approx, decay_height);
    return s + z_terrain * f;
}

// ============================================================================
// Jacobian and metric coefficients
// ============================================================================

// Jacobian of transformation: J = ∂z/∂s
// From s = z - z_terrain * f(z_agl), we get:
// ∂s/∂z = 1 - z_terrain * f'(z_agl)
// Therefore, J = ∂z/∂s = 1 / [1 - z_terrain * f'(z_agl)]
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real jacobian(Real z_terrain, Real z_agl, Real decay_height) noexcept
{
    if (z_agl <= Real(0.0)) {
        return Real(1.0);
    }
    const Real df_dz = decay_function_derivative(z_agl, decay_height);
    const Real dsdz = Real(1.0) - z_terrain * df_dz;
    
    // Avoid division by zero; if dsdz is too small, return 1.0
    if (std::abs(dsdz) < Real(1.0e-12)) {
        return Real(1.0);
    }
    return Real(1.0) / dsdz;
}

// Metric coefficient for horizontal derivatives in terrain-following coords
// ∂s/∂x = -∂z_terrain/∂x * f(z_agl)
// We need terrain slope ∂z_terrain/∂x for this
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real metric_dsdx(Real dz_terrain_dx, Real z_agl, Real decay_height) noexcept
{
    if (z_agl <= Real(0.0)) {
        return Real(0.0);
    }
    const Real f = decay_function(z_agl, decay_height);
    return -dz_terrain_dx * f;
}

// Metric coefficient for horizontal derivatives in terrain-following coords
// ∂s/∂y = -∂z_terrain/∂y * f(z_agl)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real metric_dsdy(Real dz_terrain_dy, Real z_agl, Real decay_height) noexcept
{
    if (z_agl <= Real(0.0)) {
        return Real(0.0);
    }
    const Real f = decay_function(z_agl, decay_height);
    return -dz_terrain_dy * f;
}

// ============================================================================
// Terrain slope computation
// ============================================================================

// Compute terrain slope ∂z_terrain/∂x using centered finite difference
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real compute_terrain_slope_x(int i, int j, const Real* terrain, int nx, Real dx,
                              int ilo, int ihi) noexcept
{
    if (ihi <= ilo) {
        return Real(0.0);
    }
    
    const Real inv2dx = Real(0.5) / dx;
    const Real inv1dx = Real(1.0) / dx;
    
    if (i == ilo) {
        return (terrain[j * nx + (i + 1)] - terrain[j * nx + i]) * inv1dx;
    } else if (i == ihi) {
        return (terrain[j * nx + i] - terrain[j * nx + (i - 1)]) * inv1dx;
    } else {
        return (terrain[j * nx + (i + 1)] - terrain[j * nx + (i - 1)]) * inv2dx;
    }
}

// Compute terrain slope ∂z_terrain/∂y using centered finite difference
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real compute_terrain_slope_y(int i, int j, const Real* terrain, int nx, Real dy,
                              int jlo, int jhi) noexcept
{
    if (jhi <= jlo) {
        return Real(0.0);
    }
    
    const Real inv2dy = Real(0.5) / dy;
    const Real inv1dy = Real(1.0) / dy;
    
    if (j == jlo) {
        return (terrain[(j + 1) * nx + i] - terrain[j * nx + i]) * inv1dy;
    } else if (j == jhi) {
        return (terrain[j * nx + i] - terrain[(j - 1) * nx + i]) * inv1dy;
    } else {
        return (terrain[(j + 1) * nx + i] - terrain[(j - 1) * nx + i]) * inv2dy;
    }
}

// ============================================================================
// Divergence operator in terrain-following coordinates
// ============================================================================

// Compute divergence in terrain-following coordinates:
// ∇·u = ∂u/∂x + ∂s/∂x * ∂u/∂s + ∂v/∂y + ∂s/∂y * ∂v/∂s + (1/J) * ∂(J*w)/∂s
//
// In practice, the s-coordinate is implicit (we work in z but apply corrections):
// ∇·u = ∂u/∂x + ∂v/∂y + (1/J) * ∂(J*w)/∂z
//     - (∂s/∂x * ∂u/∂z + ∂s/∂y * ∂v/∂z)
//
// This function returns the additional metric correction terms that need to be
// added to the standard Cartesian divergence
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
Real divergence_metric_correction(
    Real dudz, Real dvdz, [[maybe_unused]] Real w, Real dwdz,
    Real dz_terrain_dx, Real dz_terrain_dy,
    Real z_terrain, Real z_agl, Real decay_height) noexcept
{
    if (z_agl <= Real(0.0)) {
        return Real(0.0);
    }
    
    // Compute metric coefficients
    const Real dsdx = metric_dsdx(dz_terrain_dx, z_agl, decay_height);
    const Real dsdy = metric_dsdy(dz_terrain_dy, z_agl, decay_height);
    const Real J = jacobian(z_terrain, z_agl, decay_height);
    
    // Metric correction to vertical divergence: (1/J - 1) * ∂w/∂z + w * ∂(1/J)/∂z
    // Simplified as: (J - 1) * dwdz / J + metric correction for horizontal derivatives
    const Real vertical_correction = (J - Real(1.0)) * dwdz;
    
    // Horizontal metric corrections: -∂s/∂x * ∂u/∂z - ∂s/∂y * ∂v/∂z
    const Real horizontal_correction = -(dsdx * dudz + dsdy * dvdz);
    
    return vertical_correction + horizontal_correction;
}

} // namespace TerrainFollowingCoords

#endif // TERRAIN_FOLLOWING_COORDS_H
