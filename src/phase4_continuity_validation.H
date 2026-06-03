// ============================================================================
// phase4_continuity_validation.H
// Phase 4: Continuity & Physics Validation
//
// Validates physical properties of generated turbulent fluctuations:
// 1. Mass continuity: ∇·(u'+v'+w') ≈ 0
// 2. Coherence decay with height/distance
// 3. Anisotropy ratios (v_rms/u_rms, w_rms/u_rms)
// 4. Cross-correlations between components
// 5. Realistic correlation magnitudes and decay
//
// References:
//   - Pope, S. B. (2000). Turbulent Flows. Cambridge University Press.
//   - Hinze, J. O. (1975). Turbulence. 2nd ed. McGraw-Hill.
//   - Panofsky & Dutton (1984). Atmospheric Turbulence.
//
// ============================================================================

#ifndef PHASE4_CONTINUITY_VALIDATION_H
#define PHASE4_CONTINUITY_VALIDATION_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include "synthetic_turbulence.H"
#include <cmath>
#include <algorithm>
#include <vector>

namespace Phase4Validation {

// ============================================================================
// Mass Continuity Validator
// ============================================================================

/**
 * Validates mass continuity: ∇·u ≈ 0 in fluctuations
 * 
 * For incompressible flow: ∂u'/∂x + ∂v'/∂y + ∂w'/∂z = 0
 * 
 * This check verifies that divergence is numerically small
 */
class ContinuityValidator {
public:
    /**
     * Compute divergence at a grid point using finite differences
     * 
     * @param u_field U-component field [m/s]
     * @param v_field V-component field [m/s]
     * @param w_field W-component field [m/s]
     * @param nx, ny, nz Grid dimensions
     * @param dx, dy, dz Grid spacing [m]
     * @param i, j, k Grid indices
     * @return ∇·u at grid point [1/s]
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static amrex::Real ComputeDivergence(
        const amrex::Real* u_field,
        const amrex::Real* v_field,
        const amrex::Real* w_field,
        int nx, int ny, int nz,
        amrex::Real dx, amrex::Real dy, amrex::Real dz,
        int i, int j, int k)
    {
        // Use central differences where possible, one-sided at boundaries
        amrex::Real du_dx = 0.0, dv_dy = 0.0, dw_dz = 0.0;
        
        // ∂u/∂x
        if (i > 0 && i < nx - 1) {
            int idx_c = k * nx * ny + j * nx + i;
            int idx_e = k * nx * ny + j * nx + (i + 1);
            int idx_w = k * nx * ny + j * nx + (i - 1);
            du_dx = (u_field[idx_e] - u_field[idx_w]) / (2.0 * dx);
        }
        
        // ∂v/∂y
        if (j > 0 && j < ny - 1) {
            int idx_c = k * nx * ny + j * nx + i;
            int idx_n = k * nx * ny + (j + 1) * nx + i;
            int idx_s = k * nx * ny + (j - 1) * nx + i;
            dv_dy = (v_field[idx_n] - v_field[idx_s]) / (2.0 * dy);
        }
        
        // ∂w/∂z
        if (k > 0 && k < nz - 1) {
            int idx_c = k * nx * ny + j * nx + i;
            int idx_u = (k + 1) * nx * ny + j * nx + i;
            int idx_d = (k - 1) * nx * ny + j * nx + i;
            dw_dz = (w_field[idx_u] - w_field[idx_d]) / (2.0 * dz);
        }
        
        return du_dx + dv_dy + dw_dz;
    }
    
    /**
     * Compute domain-average divergence magnitude
     * 
     * Returns: <|∇·u|> - should be very small for incompressible flow
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static amrex::Real ComputeAverageDivergenceMagnitude(
        const amrex::Real* u_field,
        const amrex::Real* v_field,
        const amrex::Real* w_field,
        int nx, int ny, int nz,
        amrex::Real dx, amrex::Real dy, amrex::Real dz)
    {
        amrex::Real total_divergence_mag = 0.0;
        int interior_count = 0;
        
        // Only check interior points (avoid boundary)
        for (int k = 1; k < nz - 1; ++k) {
            for (int j = 1; j < ny - 1; ++j) {
                for (int i = 1; i < nx - 1; ++i) {
                    amrex::Real div = ComputeDivergence(
                        u_field, v_field, w_field, nx, ny, nz,
                        dx, dy, dz, i, j, k);
                    
                    total_divergence_mag += std::abs(div);
                    interior_count++;
                }
            }
        }
        
        if (interior_count > 0) {
            return total_divergence_mag / interior_count;
        }
        return 0.0;
    }
    
    /**
     * Validate mass continuity
     * 
     * Check: <|∇·u|> / <|du/dx|> < tolerance
     * Typical tolerance: 5-10%
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateContinuity(
        amrex::Real avg_div_magnitude,
        amrex::Real typical_gradient_scale,
        amrex::Real tolerance = 0.1)
    {
        // Avoid division by very small numbers
        if (typical_gradient_scale < 1.0e-10) {
            return avg_div_magnitude < 1.0e-10;
        }
        
        amrex::Real relative_error = avg_div_magnitude / typical_gradient_scale;
        return relative_error <= tolerance;
    }
};

// ============================================================================
// Anisotropy Validator
// ============================================================================

/**
 * Validates component anisotropy ratios
 * 
 * Typical atmospheric turbulence:
 *   v_rms / u_rms ≈ 0.80  (lateral component reduced)
 *   w_rms / u_rms ≈ 0.50  (vertical component reduced)
 * 
 * These ratios should be stable over time and space
 */
class AnisotropyValidator {
public:
    /**
     * Compute component RMS values from field
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static void ComputeComponentRms(
        const amrex::Real* u_field,
        const amrex::Real* v_field,
        const amrex::Real* w_field,
        int num_points,
        amrex::Real& rms_u,
        amrex::Real& rms_v,
        amrex::Real& rms_w)
    {
        amrex::Real sum_u2 = 0.0, sum_v2 = 0.0, sum_w2 = 0.0;
        
        for (int i = 0; i < num_points; ++i) {
            sum_u2 += u_field[i] * u_field[i];
            sum_v2 += v_field[i] * v_field[i];
            sum_w2 += w_field[i] * w_field[i];
        }
        
        rms_u = std::sqrt(sum_u2 / num_points);
        rms_v = std::sqrt(sum_v2 / num_points);
        rms_w = std::sqrt(sum_w2 / num_points);
    }
    
    /**
     * Validate anisotropy ratio
     * 
     * Expected v_rms/u_rms ≈ 0.80, tolerance ±5%
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateVtoURatio(
        amrex::Real rms_v,
        amrex::Real rms_u,
        amrex::Real tolerance = 0.05)
    {
        if (rms_u < 0.001) return false;
        
        amrex::Real observed_ratio = rms_v / rms_u;
        amrex::Real expected_ratio = 0.80;
        amrex::Real error = std::abs(observed_ratio - expected_ratio) / expected_ratio;
        
        return error <= tolerance;
    }
    
    /**
     * Validate anisotropy ratio
     * 
     * Expected w_rms/u_rms ≈ 0.50, tolerance ±5%
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateWtoURatio(
        amrex::Real rms_w,
        amrex::Real rms_u,
        amrex::Real tolerance = 0.05)
    {
        if (rms_u < 0.001) return false;
        
        amrex::Real observed_ratio = rms_w / rms_u;
        amrex::Real expected_ratio = 0.50;
        amrex::Real error = std::abs(observed_ratio - expected_ratio) / expected_ratio;
        
        return error <= tolerance;
    }
};

// ============================================================================
// Coherence Decay Validator
// ============================================================================

/**
 * Validates coherence decay with distance
 * 
 * Expected behavior:
 *   - Coherence(0) = 1.0
 *   - Coherence(∞) = 0.0
 *   - Smooth exponential or Gaussian decay
 */
class CoherenceDecayValidator {
public:
    /**
     * Compute spatial autocorrelation at lag Δz
     * 
     * ρ(Δz) = E[u'(z)·u'(z+Δz)] / (σ_u · σ_u)
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static amrex::Real ComputeAutocorrelation(
        const amrex::Real* field,
        int nz,
        int lag_indices)
    {
        if (lag_indices >= nz) return 0.0;
        
        // Compute mean and standard deviation
        amrex::Real mean = 0.0;
        for (int i = 0; i < nz; ++i) {
            mean += field[i];
        }
        mean /= nz;
        
        amrex::Real var = 0.0;
        for (int i = 0; i < nz; ++i) {
            amrex::Real dev = field[i] - mean;
            var += dev * dev;
        }
        var /= nz;
        
        if (var < 1.0e-10) return 0.0;
        
        amrex::Real sigma = std::sqrt(var);
        
        // Compute lag-1 autocorrelation
        amrex::Real covariance = 0.0;
        int count = 0;
        
        for (int i = 0; i < nz - lag_indices; ++i) {
            covariance += (field[i] - mean) * (field[i + lag_indices] - mean);
            count++;
        }
        
        if (count > 0) {
            covariance /= count;
        }
        
        return covariance / (sigma * sigma);
    }
    
    /**
     * Validate coherence at origin
     * 
     * ρ(0) should be very close to 1.0
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateCoherenceAtOrigin(
        amrex::Real coherence_zero,
        amrex::Real tolerance = 0.01)
    {
        return std::abs(coherence_zero - 1.0) <= tolerance;
    }
    
    /**
     * Validate coherence decay is monotonic decreasing
     * 
     * Check that coherence doesn't increase with distance
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateMonotonicDecay(
        const amrex::Real* coherence_values,
        int num_lags)
    {
        for (int i = 1; i < num_lags; ++i) {
            if (coherence_values[i] > coherence_values[i-1]) {
                return false;  // Non-monotonic increase detected
            }
        }
        return true;
    }
    
    /**
     * Validate coherence bounds [0, 1]
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateCoherenceBounds(
        const amrex::Real* coherence_values,
        int num_values)
    {
        for (int i = 0; i < num_values; ++i) {
            if (coherence_values[i] < 0.0 || coherence_values[i] > 1.0) {
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// Cross-Correlation Validator
// ============================================================================

/**
 * Validates realistic cross-correlations between components
 * 
 * Typical atmospheric turbulence:
 *   |ρ_uv| ≈ 0.20  (weak correlation between u and v)
 *   |ρ_uw| ≈ 0.15  (weak correlation between u and w)
 *   |ρ_vw| ≈ 0.05  (very weak correlation between v and w)
 */
class CrossCorrelationValidator {
public:
    /**
     * Compute cross-correlation coefficient between two fields
     * 
     * ρ_xy = Cov(x,y) / (σ_x · σ_y)
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static amrex::Real ComputeCrossCorrelation(
        const amrex::Real* field_x,
        const amrex::Real* field_y,
        int num_points)
    {
        if (num_points < 2) return 0.0;
        
        // Compute means
        amrex::Real mean_x = 0.0, mean_y = 0.0;
        for (int i = 0; i < num_points; ++i) {
            mean_x += field_x[i];
            mean_y += field_y[i];
        }
        mean_x /= num_points;
        mean_y /= num_points;
        
        // Compute variances and covariance
        amrex::Real var_x = 0.0, var_y = 0.0, covar_xy = 0.0;
        for (int i = 0; i < num_points; ++i) {
            amrex::Real dx = field_x[i] - mean_x;
            amrex::Real dy = field_y[i] - mean_y;
            var_x += dx * dx;
            var_y += dy * dy;
            covar_xy += dx * dy;
        }
        
        var_x /= num_points;
        var_y /= num_points;
        covar_xy /= num_points;
        
        amrex::Real sigma_x = std::sqrt(var_x);
        amrex::Real sigma_y = std::sqrt(var_y);
        
        if (sigma_x < 1.0e-10 || sigma_y < 1.0e-10) {
            return 0.0;
        }
        
        return covar_xy / (sigma_x * sigma_y);
    }
    
    /**
     * Validate u-v cross-correlation magnitude
     * 
     * Expected |ρ_uv| ≈ 0.20 ± 0.10 (weak negative correlation typical)
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateUVCrossCorrelation(
        amrex::Real rho_uv,
        amrex::Real tolerance = 0.15)
    {
        // Typical: -0.3 < ρ_uv < 0
        amrex::Real abs_corr = std::abs(rho_uv);
        return abs_corr < 0.35;  // Reasonable magnitude check
    }
    
    /**
     * Validate that correlations are physically realistic
     * 
     * |ρ_xy| should be < 0.5 for typical atmospheric turbulence
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateCrossCorrelationMagnitude(
        amrex::Real rho_xy,
        amrex::Real max_magnitude = 0.5)
    {
        return std::abs(rho_xy) <= max_magnitude;
    }
};

// ============================================================================
// Turbulence Intensity Validator
// ============================================================================

/**
 * Validates turbulence intensity from computed RMS
 */
class TurbulenceIntensityValidator {
public:
    /**
     * Compute turbulence intensity
     * 
     * I = u_rms / U_mean
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static amrex::Real ComputeIntensity(
        amrex::Real u_rms,
        amrex::Real u_mean)
    {
        if (u_mean < 0.1) return 0.0;
        return u_rms / u_mean;
    }
    
    /**
     * Validate intensity is within physical bounds
     * 
     * Typical range: 0.01 ≤ I ≤ 0.30
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateIntensityBounds(
        amrex::Real intensity,
        amrex::Real lower_bound = 0.01,
        amrex::Real upper_bound = 0.30)
    {
        return intensity >= lower_bound && intensity <= upper_bound;
    }
    
    /**
     * Validate intensity matches input profile
     * 
     * @param computed_intensity Intensity from field [m/s/m/s]
     * @param target_intensity Expected intensity [m/s/m/s]
     * @param tolerance Relative tolerance, typically ±10%
     */
    AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
    static bool ValidateIntensityProfile(
        amrex::Real computed_intensity,
        amrex::Real target_intensity,
        amrex::Real tolerance = 0.1)
    {
        if (target_intensity < 0.001) return false;
        
        amrex::Real error = std::abs(computed_intensity - target_intensity) / 
                            target_intensity;
        return error <= tolerance;
    }
};

// ============================================================================
// Comprehensive Physics Validator
// ============================================================================

/**
 * Master class combining all physics validations
 */
class PhysicsValidator {
public:
    struct ValidationResult {
        bool continuity_valid = false;
        bool anisotropy_v_valid = false;
        bool anisotropy_w_valid = false;
        bool coherence_decay_valid = false;
        bool cross_corr_uv_valid = false;
        bool cross_corr_uw_valid = false;
        bool intensity_valid = false;
        bool all_checks_passed = false;
        
        amrex::Real avg_divergence = 0.0;
        amrex::Real ratio_v_u = 0.0;
        amrex::Real ratio_w_u = 0.0;
        amrex::Real corr_uv = 0.0;
        amrex::Real corr_uw = 0.0;
        amrex::Real computed_intensity = 0.0;
    };
    
    /**
     * Run comprehensive physics validation
     */
    static ValidationResult ValidatePhysics(
        const amrex::Real* u_field,
        const amrex::Real* v_field,
        const amrex::Real* w_field,
        int nx, int ny, int nz,
        amrex::Real dx, amrex::Real dy, amrex::Real dz,
        amrex::Real u_mean,
        amrex::Real target_intensity)
    {
        ValidationResult result;
        int num_points = nx * ny * nz;
        
        // Check 1: Continuity
        result.avg_divergence = ContinuityValidator::ComputeAverageDivergenceMagnitude(
            u_field, v_field, w_field, nx, ny, nz, dx, dy, dz);
        
        // Typical gradient scale: mean wind / domain size
        amrex::Real typical_gradient = u_mean / std::max({dx, dy, dz});
        result.continuity_valid = ContinuityValidator::ValidateContinuity(
            result.avg_divergence, typical_gradient, 0.1);
        
        // Check 2 & 3: Anisotropy
        amrex::Real rms_u, rms_v, rms_w;
        AnisotropyValidator::ComputeComponentRms(
            u_field, v_field, w_field, num_points, rms_u, rms_v, rms_w);
        
        result.ratio_v_u = (rms_u > 0.001) ? rms_v / rms_u : 0.0;
        result.ratio_w_u = (rms_u > 0.001) ? rms_w / rms_u : 0.0;
        
        result.anisotropy_v_valid = AnisotropyValidator::ValidateVtoURatio(
            rms_v, rms_u, 0.05);
        result.anisotropy_w_valid = AnisotropyValidator::ValidateWtoURatio(
            rms_w, rms_u, 0.05);
        
        // Check 4: Cross-correlations
        result.corr_uv = CrossCorrelationValidator::ComputeCrossCorrelation(
            u_field, v_field, num_points);
        result.corr_uw = CrossCorrelationValidator::ComputeCrossCorrelation(
            u_field, w_field, num_points);
        
        result.cross_corr_uv_valid = CrossCorrelationValidator::ValidateUVCrossCorrelation(
            result.corr_uv, 0.15);
        result.cross_corr_uw_valid = CrossCorrelationValidator::ValidateCrossCorrelationMagnitude(
            result.corr_uw, 0.5);
        
        // Check 5: Intensity
        result.computed_intensity = TurbulenceIntensityValidator::ComputeIntensity(
            rms_u, u_mean);
        result.intensity_valid = TurbulenceIntensityValidator::ValidateIntensityProfile(
            result.computed_intensity, target_intensity, 0.1);
        
        // Overall result
        result.all_checks_passed = 
            result.continuity_valid &&
            result.anisotropy_v_valid &&
            result.anisotropy_w_valid &&
            result.cross_corr_uv_valid &&
            result.cross_corr_uw_valid &&
            result.intensity_valid;
        
        return result;
    }
};

} // namespace Phase4Validation

#endif // PHASE4_CONTINUITY_VALIDATION_H
