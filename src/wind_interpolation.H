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
static constexpr Real TOPOGRAPHIC_SHIELDING_PENALTY_FACTOR = Real(1.0e-5); // penalty for blocked stations

// IDW interpolation: terrain height at query point (xq, yq)
// Uses k nearest data points with configurable distance weights.
inline Real idw_terrain(Real xq, Real yq,
                        const std::vector<Real>& x,
                        const std::vector<Real>& y,
                        const std::vector<Real>& z,
                        int k = 6,
                        Real idw_exponent = 2.0)
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
        Real w = (idw_exponent == Real(2.0)) ? (Real(1.0) / d2[i].first) : std::pow(d2[i].first, -idw_exponent / Real(2.0));  // configurable distance weight
        wsum += w;
        zval += w * z[d2[i].second];
    }
    return zval / wsum;
}

// IDW interpolation: wind velocity at query point (xq, yq)
// Uses k nearest data points with configurable distance weights.
// Returns (ux, uy) pair.
inline std::pair<Real, Real> idw_velocity(Real xq, Real yq,
                                          const std::vector<Real>& x,
                                          const std::vector<Real>& y,
                                          const std::vector<Real>& ux_data,
                                          const std::vector<Real>& uy_data,
                                          int k = 6,
                                          Real idw_exponent = 2.0)
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
        Real w = (idw_exponent == Real(2.0)) ? (Real(1.0) / d2[i].first) : std::pow(d2[i].first, -idw_exponent / Real(2.0));  // configurable distance weight
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
    int k = 6,
    Real idw_exponent = 2.0)
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
        Real w = (idw_exponent == Real(2.0)) ? (Real(1.0) / d2[i].first) : std::pow(d2[i].first, -idw_exponent / Real(2.0));
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
    int k = 6,
    Real idw_exponent = 2.0)
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
        Real w = (idw_exponent == Real(2.0)) ? (Real(1.0) / d2[i].first) : std::pow(d2[i].first, -idw_exponent / Real(2.0));  // configurable distance weight
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
    int k = 6,
    Real idw_exponent = 2.0)
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
        Real w = (idw_exponent == Real(2.0)) ? (Real(1.0) / d2_vals[i]) : std::pow(d2_vals[i], -idw_exponent / Real(2.0));  // configurable distance weight
        wsum += w;
        ah_val += w * alpha_h_data[idx_vals[i]];
        av_val += w * alpha_v_data[idx_vals[i]];
    }
    return {ah_val / wsum, av_val / wsum};
}

// Check if the line-of-sight between (xq, yq, zq) and (xi, yi, zi) is blocked by terrain
inline bool is_line_of_sight_blocked(
    Real xq, Real yq, Real zq,
    Real xi, Real yi, Real zi,
    const std::vector<Real>& terrain_h,
    Real x_lo, Real y_lo,
    Real dx, Real dy,
    int nx, int ny)
{
    Real dist = std::sqrt((xq - xi) * (xq - xi) + (yq - yi) * (yq - yi));
    if (dist < Real(1.0e-6)) return false; // same location, not blocked

    Real min_spacing = std::min(dx, dy);
    if (min_spacing < Real(1.0e-6)) min_spacing = Real(1.0);
    int num_steps = std::max(5, static_cast<int>(dist / min_spacing));
    if (num_steps > 100) num_steps = 100; // safety limit

    for (int step = 1; step < num_steps; ++step) {
        Real t = static_cast<Real>(step) / static_cast<Real>(num_steps);
        Real x = xq + t * (xi - xq);
        Real y = yq + t * (yi - yq);
        Real z = zq + t * (zi - zq);

        // Map to terrain grid index
        int i = static_cast<int>((x - x_lo) / dx);
        int j = static_cast<int>((y - y_lo) / dy);
        i = std::max(0, std::min(i, nx - 1));
        j = std::max(0, std::min(j, ny - 1));

        Real h_terrain = terrain_h[static_cast<std::size_t>(j) * nx + i];
        if (h_terrain > z) {
            return true; // blocked!
        }
    }
    return false;
}

// IDW interpolation: 3D wind velocity at query point (xq, yq, zq)
// Uses k nearest data points with 3D configurable distance weights.
// Returns (ux, uy) pair.
inline std::pair<Real, Real> idw_velocity_3d(Real xq, Real yq, Real zq,
                                             const std::vector<Real>& x,
                                             const std::vector<Real>& y,
                                             const std::vector<Real>& z,
                                             const std::vector<Real>& ux_data,
                                             const std::vector<Real>& uy_data,
                                             int k = 6,
                                             Real gamma = 1.0,
                                             bool enable_shielding = false,
                                             const std::vector<Real>& terrain_h = {},
                                             Real x_lo = 0.0, Real y_lo = 0.0,
                                             Real dx = 1.0, Real dy = 1.0,
                                             int nx = 0, int ny = 0,
                                             Real rmax = -1.0,
                                             Real idw_exponent = 2.0)
{
    int n = static_cast<int>(x.size());

    // Squared distances to all 3D data points
    std::vector<std::pair<Real, int>> d2;
    d2.reserve(n);
    for (int i = 0; i < n; ++i) {
        Real dx_val = x[i] - xq;
        Real dy_val = y[i] - yq;
        if (rmax > Real(0.0)) {
            Real d2D_sq = dx_val * dx_val + dy_val * dy_val;
            if (d2D_sq > rmax * rmax) {
                continue; // Skip station if it's beyond rmax
            }
        }
        Real dz_val = z[i] - zq;
        Real g_dz = gamma * dz_val;
        d2.push_back({dx_val * dx_val + dy_val * dy_val + g_dz * g_dz, i});
    }

    int n_valid = static_cast<int>(d2.size());
    k = std::min(k, n_valid);

    if (k == 0) {
        return {Real(0.0), Real(0.0)};
    }

    // Partial sort: first k elements are the k nearest
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0, ux_val = 0.0, uy_val = 0.0;
    bool any_unshielded = false;
    std::vector<Real> weights(k, 0.0);

    for (int i = 0; i < k; ++i) {
        int idx = d2[i].second;
        if (d2[i].first < DISTANCE_EPSILON) {
            return {ux_data[idx], uy_data[idx]}; // exact hit
        }
        Real w = (idw_exponent == Real(2.0)) ? (Real(1.0) / d2[i].first) : std::pow(d2[i].first, -idw_exponent / Real(2.0));  // configurable distance weight
        bool blocked = false;
        if (enable_shielding && !terrain_h.empty()) {
            blocked = is_line_of_sight_blocked(xq, yq, zq, x[idx], y[idx], z[idx], terrain_h, x_lo, y_lo, dx, dy, nx, ny);
        }
        if (blocked) {
            weights[i] = w * TOPOGRAPHIC_SHIELDING_PENALTY_FACTOR; // heavily penalized
        } else {
            weights[i] = w;
            any_unshielded = true;
        }
    }

    // If all are shielded, fallback to unpenalized weights to avoid division by zero or complete signal loss
    if (enable_shielding && !any_unshielded) {
        for (int i = 0; i < k; ++i) {
            weights[i] = (idw_exponent == Real(2.0)) ? (Real(1.0) / d2[i].first) : std::pow(d2[i].first, -idw_exponent / Real(2.0));
        }
    }

    for (int i = 0; i < k; ++i) {
        int idx = d2[i].second;
        wsum += weights[i];
        ux_val += weights[i] * ux_data[idx];
        uy_val += weights[i] * uy_data[idx];
    }
    return {ux_val / wsum, uy_val / wsum};
}

// IDW interpolation: 3D wind velocity at query point (xq, yq, zq) including W
// Uses k nearest data points with 3D configurable distance weights.
// Returns (ux, uy, uz) tuple.
inline std::tuple<Real, Real, Real> idw_velocity_3d_full(Real xq, Real yq, Real zq,
                                                         const std::vector<Real>& x,
                                                         const std::vector<Real>& y,
                                                         const std::vector<Real>& z,
                                                         const std::vector<Real>& ux_data,
                                                         const std::vector<Real>& uy_data,
                                                         const std::vector<Real>& uz_data,
                                                         int k = 6,
                                                         Real gamma = 1.0,
                                                         bool enable_shielding = false,
                                                         const std::vector<Real>& terrain_h = {},
                                                         Real x_lo = 0.0, Real y_lo = 0.0,
                                                         Real dx = 1.0, Real dy = 1.0,
                                                         int nx = 0, int ny = 0,
                                                         Real rmax = -1.0,
                                                         Real idw_exponent = 2.0)
{
    int n = static_cast<int>(x.size());

    // Squared distances to all 3D data points
    std::vector<std::pair<Real, int>> d2;
    d2.reserve(n);
    for (int i = 0; i < n; ++i) {
        Real dx_val = x[i] - xq;
        Real dy_val = y[i] - yq;
        if (rmax > Real(0.0)) {
            Real d2D_sq = dx_val * dx_val + dy_val * dy_val;
            if (d2D_sq > rmax * rmax) {
                continue; // Skip station if it's beyond rmax
            }
        }
        Real dz_val = z[i] - zq;
        Real g_dz = gamma * dz_val;
        d2.push_back({dx_val * dx_val + dy_val * dy_val + g_dz * g_dz, i});
    }

    int n_valid = static_cast<int>(d2.size());
    k = std::min(k, n_valid);

    if (k == 0) {
        return {Real(0.0), Real(0.0), Real(0.0)};
    }

    // Partial sort: first k elements are the k nearest
    std::partial_sort(d2.begin(), d2.begin() + k, d2.end());

    Real wsum = 0.0, ux_val = 0.0, uy_val = 0.0, uz_val = 0.0;
    bool any_unshielded = false;
    std::vector<Real> weights(k, 0.0);

    for (int i = 0; i < k; ++i) {
        int idx = d2[i].second;
        if (d2[i].first < DISTANCE_EPSILON) {
            return {ux_data[idx], uy_data[idx], uz_data[idx]}; // exact hit
        }
        Real w = (idw_exponent == Real(2.0)) ? (Real(1.0) / d2[i].first) : std::pow(d2[i].first, -idw_exponent / Real(2.0));  // configurable distance weight
        bool blocked = false;
        if (enable_shielding && !terrain_h.empty()) {
            blocked = is_line_of_sight_blocked(xq, yq, zq, x[idx], y[idx], z[idx], terrain_h, x_lo, y_lo, dx, dy, nx, ny);
        }
        if (blocked) {
            weights[i] = w * TOPOGRAPHIC_SHIELDING_PENALTY_FACTOR; // heavily penalized
        } else {
            weights[i] = w;
            any_unshielded = true;
        }
    }

    // If all are shielded, fallback to unpenalized weights to avoid division by zero or complete signal loss
    if (enable_shielding && !any_unshielded) {
        for (int i = 0; i < k; ++i) {
            weights[i] = (idw_exponent == Real(2.0)) ? (Real(1.0) / d2[i].first) : std::pow(d2[i].first, -idw_exponent / Real(2.0));
        }
    }

    for (int i = 0; i < k; ++i) {
        int idx = d2[i].second;
        wsum += weights[i];
        ux_val += weights[i] * ux_data[idx];
        uy_val += weights[i] * uy_data[idx];
        uz_val += weights[i] * uz_data[idx];
    }
    return {ux_val / wsum, uy_val / wsum, uz_val / wsum};
}

class CubicSpline1D {
private:
    std::vector<Real> x;
    std::vector<Real> y;
    std::vector<Real> m; // second derivatives
public:
    CubicSpline1D() = default;
    CubicSpline1D(const std::vector<Real>& x_in, const std::vector<Real>& y_in) {
        x = x_in;
        y = y_in;
        int n = static_cast<int>(x.size());
        m.resize(n, 0.0);
        if (n < 3) {
            return; // Fallback to linear if too few points
        }

        std::vector<Real> h(n - 1);
        for (int i = 0; i < n - 1; ++i) {
            h[i] = x[i+1] - x[i];
        }

        // Setup tridiagonal system for second derivatives
        std::vector<Real> a(n), b(n), c(n), d(n);
        for (int i = 1; i < n - 1; ++i) {
            a[i] = h[i-1] / 6.0;
            b[i] = (h[i-1] + h[i]) / 3.0;
            c[i] = h[i] / 6.0;
            d[i] = (y[i+1] - y[i])/h[i] - (y[i] - y[i-1])/h[i-1];
        }

        // Thomas algorithm for tridiagonal solver
        std::vector<Real> c_prime(n, 0.0);
        std::vector<Real> d_prime(n, 0.0);

        c_prime[1] = c[1] / b[1];
        d_prime[1] = d[1] / b[1];

        for (int i = 2; i < n - 1; ++i) {
            Real denom = b[i] - a[i] * c_prime[i-1];
            c_prime[i] = c[i] / denom;
            d_prime[i] = (d[i] - a[i] * d_prime[i-1]) / denom;
        }

        m[n-2] = d_prime[n-2];
        for (int i = n - 3; i >= 1; --i) {
            m[i] = d_prime[i] - c_prime[i] * m[i+1];
        }
        m[0] = 0.0;
        m[n-1] = 0.0;
    }

    Real evaluate(Real xq) const {
        int n = static_cast<int>(x.size());
        if (n == 0) return 0.0;
        if (n == 1) return y[0];
        if (xq <= x[0]) return y[0];
        if (xq >= x[n-1]) return y[n-1];

        // Binary search to find interval
        auto it = std::lower_bound(x.begin(), x.end(), xq);
        int idx = static_cast<int>(std::distance(x.begin(), it));
        if (idx == 0) idx = 1;
        if (idx >= n) idx = n - 1;

        int i = idx;
        Real h = x[i] - x[i-1];
        if (h < 1e-9) return y[i];

        if (n < 3) {
            Real t = (xq - x[i-1]) / h;
            return y[i-1] + t * (y[i] - y[i-1]);
        }

        Real A = (x[i] - xq) / h;
        Real B = (xq - x[i-1]) / h;

        return A * y[i-1] + B * y[i] + (h*h/6.0) * ((A*A*A - A) * m[i-1] + (B*B*B - B) * m[i]);
    }
};

inline Real log_linear_interpolate(Real xq, const std::vector<Real>& x, const std::vector<Real>& y) {
    int n = static_cast<int>(x.size());
    if (n == 0) return 0.0;
    if (n == 1) return y[0];
    if (xq <= x[0]) return y[0];
    if (xq >= x[n-1]) return y[n-1];

    auto it = std::lower_bound(x.begin(), x.end(), xq);
    int idx = static_cast<int>(std::distance(x.begin(), it));
    if (idx == 0) idx = 1;
    if (idx >= n) idx = n - 1;

    Real x0 = x[idx-1];
    Real x1 = x[idx];
    Real y0 = y[idx-1];
    Real y1 = y[idx];

    // Fallback to linear interpolation to prevent taking the logarithm of non-positive height values
    if (x0 <= 0.0 || xq <= 0.0) {
        Real t = (xq - x0) / (x1 - x0);
        return y0 + t * (y1 - y0);
    }

    Real t = (std::log(xq) - std::log(x0)) / (std::log(x1) - std::log(x0));
    return y0 + t * (y1 - y0);
}

} // namespace WindInterpolation

#endif // WIND_INTERPOLATION_H
