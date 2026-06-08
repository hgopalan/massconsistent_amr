#ifndef WIND_INTERPOLATION_H
#define WIND_INTERPOLATION_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <vector>
#include <tuple>
#include <algorithm>
#include <cmath>

namespace WindInterpolation {

using amrex::Real;

static constexpr Real DISTANCE_EPSILON = Real(1.0e-12);  // threshold for exact spatial matches

// IDW interpolation: terrain height at query point (xq, yq)
// Uses k nearest data points with inverse-square-distance weights.
inline Real idw_terrain(Real xq, Real yq,
                        const std::vector<Real>& x,
                        const std::vector<Real>& y,
                        const std::vector<Real>& z,
                        int k = 6)
{
    int n = static_cast<int>(x.size());
    k = std::min(k, n);

    // Squared distances to all data points
    std::vector<std::pair<Real, int>> d2(n);
    for (int i = 0; i < n; ++i) {
        Real dx = x[i] - xq;
        Real dy = y[i] - yq;
        d2[i] = {dx * dx + dy * dy, i};
    }
    // Partial sort: first k elements are the k nearest
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0, zval = 0.0;
    for (int i = 0; i < k; ++i) {
        if (d2[i].first < DISTANCE_EPSILON) return z[d2[i].second]; // exact hit
        Real w = Real(1.0) / d2[i].first;  // inverse-square-distance weight
        wsum += w;
        zval += w * z[d2[i].second];
    }
    return zval / wsum;
}

// IDW interpolation: wind velocity at query point (xq, yq)
// Uses k nearest data points with inverse-square-distance weights.
// Returns (ux, uy) pair.
inline std::pair<Real, Real> idw_velocity(Real xq, Real yq,
                                          const std::vector<Real>& x,
                                          const std::vector<Real>& y,
                                          const std::vector<Real>& ux_data,
                                          const std::vector<Real>& uy_data,
                                          int k = 6)
{
    int n = static_cast<int>(x.size());
    k = std::min(k, n);

    // Squared distances to all data points
    std::vector<std::pair<Real, int>> d2(n);
    for (int i = 0; i < n; ++i) {
        Real dx = x[i] - xq;
        Real dy = y[i] - yq;
        d2[i] = {dx * dx + dy * dy, i};
    }
    // Partial sort: first k elements are the k nearest
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0, ux_val = 0.0, uy_val = 0.0;
    for (int i = 0; i < k; ++i) {
        if (d2[i].first < DISTANCE_EPSILON) {
            return {ux_data[d2[i].second], uy_data[d2[i].second]}; // exact hit
        }
        Real w = Real(1.0) / d2[i].first;  // inverse-square-distance weight
        wsum += w;
        ux_val += w * ux_data[d2[i].second];
        uy_val += w * uy_data[d2[i].second];
    }
    return {ux_val / wsum, uy_val / wsum};
}

// IDW interpolation: surface data (USTAR, Z0, U10, V10) at query point (xq, yq)
// Returns tuple: (ustar, z0, u10, v10)
inline std::tuple<Real, Real, Real, Real> idw_surface_data(
    Real xq, Real yq,
    const std::vector<Real>& x,
    const std::vector<Real>& y,
    const std::vector<Real>& ustar_data,
    const std::vector<Real>& z0_data,
    const std::vector<Real>& u10_data,
    const std::vector<Real>& v10_data,
    int k = 6)
{
    int n = static_cast<int>(x.size());
    k = std::min(k, n);

    std::vector<std::pair<Real, int>> d2(n);
    for (int i = 0; i < n; ++i) {
        Real dx = x[i] - xq;
        Real dy = y[i] - yq;
        d2[i] = {dx * dx + dy * dy, i};
    }
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0, ustar_val = 0.0, z0_val = 0.0, u10_val = 0.0, v10_val = 0.0;
    for (int i = 0; i < k; ++i) {
        if (d2[i].first < DISTANCE_EPSILON) {
            int idx = d2[i].second;
            return {ustar_data[idx], z0_data[idx], u10_data[idx], v10_data[idx]};
        }
        Real w = Real(1.0) / d2[i].first;
        wsum += w;
        ustar_val += w * ustar_data[d2[i].second];
        z0_val += w * z0_data[d2[i].second];
        u10_val += w * u10_data[d2[i].second];
        v10_val += w * v10_data[d2[i].second];
    }
    return {ustar_val / wsum, z0_val / wsum, u10_val / wsum, v10_val / wsum};
}

// IDW interpolation: alpha coefficients at query point (xq, yq)
// Returns pair: (alpha_h, alpha_v)
inline std::pair<Real, Real> idw_alpha_coefficients(
    Real xq, Real yq,
    const std::vector<Real>& x,
    const std::vector<Real>& y,
    const std::vector<Real>& alpha_h_data,
    const std::vector<Real>& alpha_v_data,
    int k = 6)
{
    int n = static_cast<int>(x.size());
    k = std::min(k, n);

    std::vector<std::pair<Real, int>> d2(n);
    for (int i = 0; i < n; ++i) {
        Real dx = x[i] - xq;
        Real dy = y[i] - yq;
        d2[i] = {dx * dx + dy * dy, i};
    }
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0, ah_val = 0.0, av_val = 0.0;
    for (int i = 0; i < k; ++i) {
        if (d2[i].first < DISTANCE_EPSILON) {
            return {alpha_h_data[d2[i].second], alpha_v_data[d2[i].second]}; // exact hit
        }
        Real w = Real(1.0) / d2[i].first;  // inverse-square-distance weight
        wsum += w;
        ah_val += w * alpha_h_data[d2[i].second];
        av_val += w * alpha_v_data[d2[i].second];
    }
    return {ah_val / wsum, av_val / wsum};
}

// GPU-compatible IDW interpolation for alpha coefficients
// Returns pair: (alpha_h, alpha_v)
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
std::pair<Real, Real> idw_alpha_coefficients_gpu(
    Real xq, Real yq,
    const Real* x,
    const Real* y,
    const Real* alpha_h_data,
    const Real* alpha_v_data,
    int n,
    int k = 6)
{
    k = (k < n) ? k : n;
    
    // Simple O(n) approach to find k nearest neighbors
    // Store distances and indices
    Real d2_vals[100];  // Max 100 data points for GPU kernel
    int  idx_vals[100];
    
    if (n > 100) n = 100;  // Safety limit for GPU
    
    for (int i = 0; i < n; ++i) {
        Real dx = x[i] - xq;
        Real dy = y[i] - yq;
        d2_vals[i] = dx * dx + dy * dy;
        idx_vals[i] = i;
    }
    
    // Partial selection sort to find k nearest
    for (int i = 0; i < k && i < n; ++i) {
        int min_idx = i;
        for (int j = i + 1; j < n; ++j) {
            if (d2_vals[j] < d2_vals[min_idx]) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            Real tmp_d = d2_vals[i];
            d2_vals[i] = d2_vals[min_idx];
            d2_vals[min_idx] = tmp_d;
            int tmp_i = idx_vals[i];
            idx_vals[i] = idx_vals[min_idx];
            idx_vals[min_idx] = tmp_i;
        }
    }
    
    Real wsum = Real(0.0), ah_val = Real(0.0), av_val = Real(0.0);
    for (int i = 0; i < k; ++i) {
        if (d2_vals[i] < DISTANCE_EPSILON) {
            return {alpha_h_data[idx_vals[i]], alpha_v_data[idx_vals[i]]}; // exact hit
        }
        Real w = Real(1.0) / d2_vals[i];  // inverse-square-distance weight
        wsum += w;
        ah_val += w * alpha_h_data[idx_vals[i]];
        av_val += w * alpha_v_data[idx_vals[i]];
    }
    return {ah_val / wsum, av_val / wsum};
}

// IDW interpolation: 3D wind velocity at query point (xq, yq, zq)
// Uses k nearest data points with 3D inverse-square-distance weights.
// Returns (ux, uy) pair.
inline std::pair<Real, Real> idw_velocity_3d(Real xq, Real yq, Real zq,
                                             const std::vector<Real>& x,
                                             const std::vector<Real>& y,
                                             const std::vector<Real>& z,
                                             const std::vector<Real>& ux_data,
                                             const std::vector<Real>& uy_data,
                                             int k = 6,
                                             Real gamma = 1.0)
{
    int n = static_cast<int>(x.size());
    k = std::min(k, n);

    // Squared distances to all 3D data points
    std::vector<std::pair<Real, int>> d2(n);
    for (int i = 0; i < n; ++i) {
        Real dx = x[i] - xq;
        Real dy = y[i] - yq;
        Real dz = z[i] - zq;
        Real g_dz = gamma * dz;
        d2[i] = {dx * dx + dy * dy + g_dz * g_dz, i};
    }
    // Partial sort: first k elements are the k nearest
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0, ux_val = 0.0, uy_val = 0.0;
    for (int i = 0; i < k; ++i) {
        if (d2[i].first < DISTANCE_EPSILON) {
            return {ux_data[d2[i].second], uy_data[d2[i].second]}; // exact hit
        }
        Real w = Real(1.0) / d2[i].first;  // inverse-square-distance weight
        wsum += w;
        ux_val += w * ux_data[d2[i].second];
        uy_val += w * uy_data[d2[i].second];
    }
    return {ux_val / wsum, uy_val / wsum};
}

// IDW interpolation: 3D wind velocity at query point (xq, yq, zq) including W
// Uses k nearest data points with 3D inverse-square-distance weights.
// Returns (ux, uy, uz) tuple.
inline std::tuple<Real, Real, Real> idw_velocity_3d_full(Real xq, Real yq, Real zq,
                                                         const std::vector<Real>& x,
                                                         const std::vector<Real>& y,
                                                         const std::vector<Real>& z,
                                                         const std::vector<Real>& ux_data,
                                                         const std::vector<Real>& uy_data,
                                                         const std::vector<Real>& uz_data,
                                                         int k = 6,
                                                         Real gamma = 1.0)
{
    int n = static_cast<int>(x.size());
    k = std::min(k, n);

    // Squared distances to all 3D data points
    std::vector<std::pair<Real, int>> d2(n);
    for (int i = 0; i < n; ++i) {
        Real dx = x[i] - xq;
        Real dy = y[i] - yq;
        Real dz = z[i] - zq;
        Real g_dz = gamma * dz;
        d2[i] = {dx * dx + dy * dy + g_dz * g_dz, i};
    }
    // Partial sort: first k elements are the k nearest
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0, ux_val = 0.0, uy_val = 0.0, uz_val = 0.0;
    for (int i = 0; i < k; ++i) {
        if (d2[i].first < DISTANCE_EPSILON) {
            int idx = d2[i].second;
            return {ux_data[idx], uy_data[idx], uz_data[idx]}; // exact hit
        }
        Real w = Real(1.0) / d2[i].first;  // inverse-square-distance weight
        wsum += w;
        ux_val += w * ux_data[d2[i].second];
        uy_val += w * uy_data[d2[i].second];
        uz_val += w * uz_data[d2[i].second];
    }
    return {ux_val / wsum, uy_val / wsum, uz_val / wsum};
}

} // namespace WindInterpolation

#endif // WIND_INTERPOLATION_H
