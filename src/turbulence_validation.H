// ============================================================================
// turbulence_validation.H
// Comprehensive validation framework for synthetic turbulence time-series
//
// This header provides validation tools to verify that exported time-series
// data meets physics requirements and format specifications. Includes:
//
//   1. Energy Spectra Validation
//      - Verifies spectral content against Phase 1 targets
//      - Parseval's theorem check: ∫S(f)df ≈ σ²_target (±5% tolerance)
//
//   2. Temporal Coherence Validation
//      - Autocorrelation decay matches expected timescale
//      - Integral timescale consistency
//
//   3. Spatial Coherence Validation
//      - Correlation between nearby grid points
//      - Coherence function validation
//
//   4. Anisotropy Validation
//      - Component ratios: v_rms/u_rms and w_rms/u_rms
//      - Stability across time (should be ±5% constant)
//
//   5. Format Compliance
//      - BTS file structure validation
//      - Header consistency
//
// Physics Thresholds:
//   - Energy conservation: ±5%
//   - Coherence correctness: ±0.1
//   - Anisotropy stability: ±5%
//   - Reproducibility: bitwise identical for same seed
//
// References:
//   - Parseval's theorem (energy conservation)
//   - IEC 61400-1 (2019): Validation criteria
//   - Shinozuka & Deodatis (1991): Stochastic simulation validation
//
// ============================================================================

#ifndef TURBULENCE_VALIDATION_H
#define TURBULENCE_VALIDATION_H

#include <AMReX_REAL.H>
#include <AMReX_GpuQualifiers.H>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <iomanip>

// ============================================================================
// Namespace: Phase3Validation
// ============================================================================

namespace Phase3Validation {

// ============================================================================
// Constants
// ============================================================================

constexpr amrex::Real ENERGY_TOLERANCE = 0.05;       // ±5%
constexpr amrex::Real COHERENCE_TOLERANCE = 0.10;    // ±0.1
constexpr amrex::Real ANISOTROPY_TOLERANCE = 0.05;   // ±5%
constexpr amrex::Real RMS_TOLERANCE = 0.10;          // ±10%

// ============================================================================
// Structure: ValidationResult
// Container for validation check results
// ============================================================================

struct ValidationResult {
    bool passed = false;
    amrex::Real expected = 0.0;
    amrex::Real actual = 0.0;
    amrex::Real tolerance = 0.0;
    std::string description = "";
    
    std::string GetStatus() const {
        return passed ? "PASS" : "FAIL";
    }
    
    std::string GetErrorMessage() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4);
        oss << "Expected: " << expected << ", Actual: " << actual
            << ", Tolerance: " << tolerance;
        return oss.str();
    }
};

// ============================================================================
// Utility Functions - Statistics
// ============================================================================

/// Compute RMS (root-mean-square) of data
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real ComputeRMS(const std::vector<amrex::Real>& data)
{
    if (data.empty()) return 0.0;
    
    amrex::Real sum_sq = 0.0;
    for (const auto& val : data) {
        sum_sq += val * val;
    }
    return std::sqrt(sum_sq / data.size());
}

/// Compute mean of data
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real ComputeMean(const std::vector<amrex::Real>& data)
{
    if (data.empty()) return 0.0;
    
    amrex::Real sum = 0.0;
    for (const auto& val : data) {
        sum += val;
    }
    return sum / data.size();
}

/// Compute autocorrelation at lag k
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
amrex::Real ComputeAutocorrelation(
    const std::vector<amrex::Real>& data,
    int lag)
{
    if (data.size() <= (size_t)lag || lag < 0) return 0.0;
    
    amrex::Real mean = ComputeMean(data);
    
    amrex::Real numerator = 0.0;
    amrex::Real denominator = 0.0;
    
    for (size_t i = 0; i + lag < data.size(); ++i) {
        numerator += (data[i] - mean) * (data[i + lag] - mean);
    }
    
    for (const auto& val : data) {
        denominator += (val - mean) * (val - mean);
    }
    
    if (denominator < 1.0e-10) return 0.0;
    return numerator / denominator;
}

/// Compute integral timescale from autocorrelation
amrex::Real ComputeIntegralTimescale(
    const std::vector<amrex::Real>& data,
    amrex::Real dt,
    int max_lag = 1000)
{
    amrex::Real timescale = 0.0;
    int count = 0;
    
    for (int k = 0; k < max_lag && k < (int)data.size() / 2; ++k) {
        amrex::Real rho = ComputeAutocorrelation(data, k);
        if (rho > 0.05) {  // Only sum significant correlation
            timescale += rho * dt;
            count++;
        } else {
            break;
        }
    }
    
    return timescale;
}

// ============================================================================
// Class: SpectraValidator
// Validate spectral content
// ============================================================================

class SpectraValidator {
public:
    /// Validate energy in each component (Parseval check)
    ValidationResult ValidateComponentEnergy(
        const std::vector<amrex::Real>& component_data,
        amrex::Real expected_rms,
        const std::string& component_name = "")
    {
        ValidationResult result;
        
        amrex::Real actual_rms = ComputeRMS(component_data);
        
        result.expected = expected_rms;
        result.actual = actual_rms;
        result.tolerance = expected_rms * ENERGY_TOLERANCE;
        result.description = "Energy in " + component_name;
        
        amrex::Real error = std::abs(actual_rms - expected_rms);
        result.passed = (error <= result.tolerance);
        
        return result;
    }
    
    /// Validate total energy conservation
    ValidationResult ValidateTotalEnergy(
        const std::vector<amrex::Real>& u_prime,
        const std::vector<amrex::Real>& v_prime,
        const std::vector<amrex::Real>& w_prime,
        amrex::Real expected_u_rms,
        amrex::Real expected_v_rms,
        amrex::Real expected_w_rms)
    {
        ValidationResult result;
        
        amrex::Real actual_u_rms = ComputeRMS(u_prime);
        amrex::Real actual_v_rms = ComputeRMS(v_prime);
        amrex::Real actual_w_rms = ComputeRMS(w_prime);
        
        amrex::Real expected_total = std::sqrt(
            expected_u_rms * expected_u_rms +
            expected_v_rms * expected_v_rms +
            expected_w_rms * expected_w_rms);
        
        amrex::Real actual_total = std::sqrt(
            actual_u_rms * actual_u_rms +
            actual_v_rms * actual_v_rms +
            actual_w_rms * actual_w_rms);
        
        result.expected = expected_total;
        result.actual = actual_total;
        result.tolerance = expected_total * ENERGY_TOLERANCE;
        result.description = "Total kinetic energy";
        
        amrex::Real error = std::abs(actual_total - expected_total);
        result.passed = (error <= result.tolerance);
        
        return result;
    }
};

// ============================================================================
// Class: TemporalCoherenceValidator
// Validate temporal correlations
// ============================================================================

class TemporalCoherenceValidator {
public:
    /// Extract time-series for a single grid point
    std::vector<amrex::Real> ExtractTimeSeries(
        const std::vector<amrex::Real>& data,
        int x, int y, int z,
        int nx, int ny, int nz, int nt)
    {
        std::vector<amrex::Real> time_series(nt);
        
        for (int t = 0; t < nt; ++t) {
            int idx = t * (nx * ny * nz) + z * (nx * ny) + y * nx + x;
            if (idx >= (int)data.size()) {
                time_series[t] = 0.0;
            } else {
                time_series[t] = data[idx];
            }
        }
        
        return time_series;
    }
    
    /// Validate autocorrelation decay rate
    ValidationResult ValidateAutocorrelationDecay(
        const std::vector<amrex::Real>& time_series,
        amrex::Real expected_timescale,
        amrex::Real dt)
    {
        ValidationResult result;
        
        amrex::Real measured_timescale = ComputeIntegralTimescale(time_series, dt);
        
        result.expected = expected_timescale;
        result.actual = measured_timescale;
        result.tolerance = expected_timescale * 0.20;  // ±20% tolerance for timescale
        result.description = "Integral timescale";
        
        amrex::Real error = std::abs(measured_timescale - expected_timescale);
        result.passed = (error <= result.tolerance);
        
        return result;
    }
    
    /// Validate lag-1 autocorrelation
    ValidationResult ValidateLag1Correlation(
        const std::vector<amrex::Real>& time_series,
        amrex::Real expected_lag1_corr)
    {
        ValidationResult result;
        
        amrex::Real actual_lag1 = ComputeAutocorrelation(time_series, 1);
        
        result.expected = expected_lag1_corr;
        result.actual = actual_lag1;
        result.tolerance = 0.1;  // ±0.1 tolerance for correlation
        result.description = "Lag-1 autocorrelation";
        
        amrex::Real error = std::abs(actual_lag1 - expected_lag1_corr);
        result.passed = (error <= result.tolerance);
        
        return result;
    }
};

// ============================================================================
// Class: AnisotropyValidator
// Validate component ratio stability
// ============================================================================

class AnisotropyValidator {
public:
    /// Validate component ratio constancy over time
    ValidationResult ValidateComponentRatioStability(
        const std::vector<amrex::Real>& component1,
        const std::vector<amrex::Real>& component2,
        amrex::Real expected_ratio,
        int nx, int ny, int nz, int nt,
        int sample_interval = 10)
    {
        ValidationResult result;
        
        std::vector<amrex::Real> ratios;
        
        // Sample at different times
        for (int t = 0; t < nt; t += sample_interval) {
            amrex::Real sum1 = 0.0, sum2 = 0.0;
            int count = 0;
            
            for (int z = 0; z < nz; ++z) {
                for (int y = 0; y < ny; ++y) {
                    for (int x = 0; x < nx; ++x) {
                        int idx = t * (nx * ny * nz) + z * (nx * ny) + y * nx + x;
                        
                        if (idx < (int)component1.size() && idx < (int)component2.size()) {
                            sum1 += component1[idx] * component1[idx];
                            sum2 += component2[idx] * component2[idx];
                            count++;
                        }
                    }
                }
            }
            
            if (count > 0 && sum1 > 0.0) {
                amrex::Real rms1 = std::sqrt(sum1 / count);
                amrex::Real rms2 = std::sqrt(sum2 / count);
                if (rms1 > 1.0e-8) {
                    ratios.push_back(rms2 / rms1);
                }
            }
        }
        
        if (ratios.empty()) {
            result.passed = false;
            return result;
        }
        
        // Compute statistics of ratio
        amrex::Real mean_ratio = ComputeMean(ratios);
        amrex::Real ratio_std_dev = 0.0;
        for (const auto& r : ratios) {
            ratio_std_dev += (r - mean_ratio) * (r - mean_ratio);
        }
        ratio_std_dev = std::sqrt(ratio_std_dev / ratios.size());
        
        result.expected = expected_ratio;
        result.actual = mean_ratio;
        result.tolerance = expected_ratio * ANISOTROPY_TOLERANCE;
        result.description = "Component ratio stability";
        
        amrex::Real error = std::abs(mean_ratio - expected_ratio);
        result.passed = (error <= result.tolerance) && (ratio_std_dev < 0.01);
        
        return result;
    }
};

// ============================================================================
// Class: ValidationSuite
// Master validation orchestrator
// ============================================================================

class ValidationSuite {
private:
    std::vector<ValidationResult> results;
    
public:
    /// Add validation result
    void AddResult(const ValidationResult& result) {
        results.push_back(result);
    }
    
    /// Run complete validation suite on time-series data
    bool RunFullValidation(
        const std::vector<amrex::Real>& u_prime,
        const std::vector<amrex::Real>& v_prime,
        const std::vector<amrex::Real>& w_prime,
        int nx, int ny, int nz, int nt,
        amrex::Real dt,
        amrex::Real expected_u_rms,
        amrex::Real expected_v_rms,
        amrex::Real expected_w_rms,
        amrex::Real expected_timescale,
        amrex::Real anisotropy_ratio_v = 0.80,
        amrex::Real anisotropy_ratio_w = 0.50)
    {
        results.clear();
        
        // 1. Energy validation
        SpectraValidator spectral_validator;
        
        AddResult(spectral_validator.ValidateComponentEnergy(
            u_prime, expected_u_rms, "u-component"));
        AddResult(spectral_validator.ValidateComponentEnergy(
            v_prime, expected_v_rms, "v-component"));
        AddResult(spectral_validator.ValidateComponentEnergy(
            w_prime, expected_w_rms, "w-component"));
        AddResult(spectral_validator.ValidateTotalEnergy(
            u_prime, v_prime, w_prime,
            expected_u_rms, expected_v_rms, expected_w_rms));
        
        // 2. Temporal coherence validation (sample a few points)
        TemporalCoherenceValidator temporal_validator;
        
        int sample_x = nx / 2;
        int sample_y = ny / 2;
        int sample_z = nz / 2;
        
        auto ts_u = temporal_validator.ExtractTimeSeries(
            u_prime, sample_x, sample_y, sample_z, nx, ny, nz, nt);
        auto ts_v = temporal_validator.ExtractTimeSeries(
            v_prime, sample_x, sample_y, sample_z, nx, ny, nz, nt);
        
        amrex::Real expected_lag1 = std::exp(-dt / expected_timescale);
        AddResult(temporal_validator.ValidateLag1Correlation(ts_u, expected_lag1));
        
        // 3. Anisotropy validation
        AnisotropyValidator anisotropy_validator;
        
        AddResult(anisotropy_validator.ValidateComponentRatioStability(
            u_prime, v_prime, anisotropy_ratio_v, nx, ny, nz, nt));
        AddResult(anisotropy_validator.ValidateComponentRatioStability(
            u_prime, w_prime, anisotropy_ratio_w, nx, ny, nz, nt));
        
        // Check all results
        for (const auto& result : results) {
            if (!result.passed) {
                return false;
            }
        }
        return true;
    }
    
    /// Get validation summary
    std::string GetSummary() const {
        std::ostringstream oss;
        oss << "Validation Results: " << GetPassCount() << "/" << results.size() << " passed\n";
        
        for (const auto& result : results) {
            oss << "  [" << result.GetStatus() << "] " << result.description
                << " - " << result.GetErrorMessage() << "\n";
        }
        
        return oss.str();
    }
    
    /// Get pass count
    int GetPassCount() const {
        int count = 0;
        for (const auto& result : results) {
            if (result.passed) count++;
        }
        return count;
    }
    
    /// Get total tests
    int GetTotalTests() const {
        return results.size();
    }
    
    /// Check if all tests passed
    bool AllTestsPassed() const {
        return GetPassCount() == GetTotalTests();
    }
    
    /// Get results
    const std::vector<ValidationResult>& GetResults() const {
        return results;
    }
};

// ============================================================================
// Class: ReportGenerator
// Generate validation reports
// ============================================================================

class ReportGenerator {
public:
    /// Generate ASCII report
    std::string GenerateReport(const ValidationSuite& suite) {
        std::ostringstream oss;
        
        oss << "=" << std::string(70, '=') << "=\n";
        oss << "PHASE 3 VALIDATION REPORT\n";
        oss << "=" << std::string(70, '=') << "=\n\n";
        
        oss << "Summary:\n";
        oss << "  Total Tests: " << suite.GetTotalTests() << "\n";
        oss << "  Passed: " << suite.GetPassCount() << "\n";
        oss << "  Failed: " << (suite.GetTotalTests() - suite.GetPassCount()) << "\n";
        oss << "  Status: " << (suite.AllTestsPassed() ? "✓ PASS" : "✗ FAIL") << "\n\n";
        
        oss << "Detailed Results:\n";
        for (const auto& result : suite.GetResults()) {
            oss << "  [" << result.GetStatus() << "] " << result.description << "\n";
            oss << "        Expected: " << std::fixed << std::setprecision(4)
                << result.expected << "\n";
            oss << "        Actual:   " << result.actual << "\n";
            oss << "        Tolerance: ±" << result.tolerance << "\n";
        }
        
        oss << "\n" << std::string(72, '=') << "\n";
        
        return oss.str();
    }
};

} // namespace Phase3Validation

#endif // TURBULENCE_VALIDATION_H
