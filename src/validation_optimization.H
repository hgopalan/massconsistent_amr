#ifndef VALIDATION_OPTIMIZATION_H
#define VALIDATION_OPTIMIZATION_H

/**
 * @file validation_optimization.H
 * @brief Validation and Optimization Layer (Layer 3) for Advanced Solver Features
 * 
 * This layer provides:
 * - Performance profiling on CPU/GPU
 * - Physical correctness validation against reference data
 * - Parameter sensitivity analysis
 * - Production-ready hardening and error handling
 */

#include <AMReX.H>
#include <AMReX_MultiFab.H>
#include <AMReX_ParmParse.H>
#include <chrono>
#include <vector>
#include <cmath>
#include <algorithm>

/**
 * @namespace ValidationOptimization
 * @brief Layer 3 validation and optimization utilities
 */
namespace ValidationOptimization {

    /**
     * @struct PerformanceMetrics
     * @brief Aggregated performance metrics for validation and optimization
     */
    struct PerformanceMetrics {
        double wall_time_ms;           // Total wall time in milliseconds
        double cpu_time_ms;            // CPU time in milliseconds
        double peak_memory_mb;         // Peak memory usage in MB
        int    total_iterations;       // Total number of iterations
        double convergence_rate;       // Convergence rate metric
        double computational_throughput; // Operations per second
        
        PerformanceMetrics() 
            : wall_time_ms(0.0), cpu_time_ms(0.0), peak_memory_mb(0.0),
              total_iterations(0), convergence_rate(0.0), computational_throughput(0.0) {}
    };

    /**
     * @struct ValidationResult
     * @brief Results of physical correctness validation
     */
    struct ValidationResult {
        bool    is_valid;              // Overall validation status
        double  max_error;             // Maximum error vs reference
        double  mean_error;            // Mean absolute error vs reference
        double  rms_error;             // RMS error vs reference
        int     failed_cells;          // Number of cells exceeding tolerance
        double  error_tolerance;       // Validation tolerance
        std::string error_message;     // Error description if validation failed
        
        ValidationResult()
            : is_valid(true), max_error(0.0), mean_error(0.0), rms_error(0.0),
              failed_cells(0), error_tolerance(0.0), error_message("") {}
    };

    /**
     * @struct ParameterSensitivity
     * @brief Parameter sensitivity analysis results
     */
    struct ParameterSensitivity {
        std::string parameter_name;    // Name of the parameter
        double      nominal_value;     // Nominal parameter value
        double      min_value;         // Minimum tested value
        double      max_value;         // Maximum tested value
        double      sensitivity_index; // Sensitivity index (0-1)
        bool        is_sensitive;      // True if parameter significantly affects output
        double      output_variation;  // Output variation % per parameter % change
        
        ParameterSensitivity()
            : parameter_name(""), nominal_value(0.0), min_value(0.0), max_value(0.0),
              sensitivity_index(0.0), is_sensitive(false), output_variation(0.0) {}
    };

    /**
     * @class PerformanceProfiler
     * @brief CPU/GPU performance profiling utility
     */
    class PerformanceProfiler {
    public:
        /**
         * Start timing a code section
         */
        AMREX_FORCE_INLINE
        void StartTimer() {
            start_time_ = std::chrono::high_resolution_clock::now();
        }

        /**
         * Stop timing and return elapsed time in milliseconds
         */
        AMREX_FORCE_INLINE
        double StopTimer() {
            auto end_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end_time - start_time_;
            return elapsed.count();
        }

        /**
         * Get memory usage estimate for MultiFab
         */
        AMREX_FORCE_INLINE
        static double GetMultiFabMemoryMB(const amrex::MultiFab& mf) {
            return static_cast<double>(mf.bytesAllocated()) / (1024.0 * 1024.0);
        }

        /**
         * Record performance metric
         */
        void RecordMetric(const PerformanceMetrics& metric) {
            metrics_.push_back(metric);
        }

        /**
         * Get average performance metrics
         */
        PerformanceMetrics GetAverageMetrics() const {
            PerformanceMetrics avg;
            if (metrics_.empty()) return avg;
            
            for (const auto& m : metrics_) {
                avg.wall_time_ms += m.wall_time_ms;
                avg.cpu_time_ms += m.cpu_time_ms;
                avg.peak_memory_mb = std::max(avg.peak_memory_mb, m.peak_memory_mb);
                avg.total_iterations += m.total_iterations;
                avg.convergence_rate += m.convergence_rate;
                avg.computational_throughput += m.computational_throughput;
            }
            
            double n = static_cast<double>(metrics_.size());
            avg.wall_time_ms /= n;
            avg.cpu_time_ms /= n;
            avg.total_iterations /= static_cast<int>(n);
            avg.convergence_rate /= n;
            avg.computational_throughput /= n;
            
            return avg;
        }

    private:
        std::chrono::high_resolution_clock::time_point start_time_;
        std::vector<PerformanceMetrics> metrics_;
    };

    /**
     * @class PhysicalValidator
     * @brief Physical correctness validation
     */
    class PhysicalValidator {
    public:
        /**
         * Validate wind field against reference data
         */
        static ValidationResult ValidateWindField(
            const amrex::MultiFab& computed_field,
            const amrex::MultiFab& reference_field,
            double tolerance = 0.01) // Default 1% tolerance
        {
            ValidationResult result;
            result.error_tolerance = tolerance;
            
            // TODO: Implement validation logic
            // This would compare computed_field against reference_field
            // element-wise and compute error metrics
            
            return result;
        }

        /**
         * Check physical bounds (e.g., wind speed non-negative, temperature physical range)
         */
        static bool CheckPhysicalBounds(
            const amrex::MultiFab& field,
            double min_value,
            double max_value,
            int component = 0) 
        {
            // TODO: Implement bounds checking
            return true;
        }

        /**
         * Validate mass conservation (divergence close to zero)
         */
        static double GetMaxDivergence(
            const amrex::MultiFab& velocity_x,
            const amrex::MultiFab& velocity_y,
            const amrex::MultiFab& velocity_z)
        {
            // TODO: Implement divergence calculation
            return 0.0;
        }
    };

    /**
     * @class SensitivityAnalyzer
     * @brief Parameter sensitivity analysis
     */
    class SensitivityAnalyzer {
    public:
        /**
         * Analyze sensitivity of output to parameter variations
         */
        static ParameterSensitivity AnalyzeParameterSensitivity(
            const std::string& param_name,
            double nominal_value,
            double variation_percent = 10.0) // ±10% variation
        {
            ParameterSensitivity result;
            result.parameter_name = param_name;
            result.nominal_value = nominal_value;
            result.min_value = nominal_value * (1.0 - variation_percent / 100.0);
            result.max_value = nominal_value * (1.0 + variation_percent / 100.0);
            
            // TODO: Implement sensitivity analysis
            // Run simulations with varied parameter and compute output variation
            
            return result;
        }

        /**
         * Generate sensitivity report
         */
        static void GenerateSensitivityReport(
            const std::vector<ParameterSensitivity>& sensitivities,
            const std::string& output_file)
        {
            // TODO: Implement report generation
        }
    };

    /**
     * @class OptimizationHelper
     * @brief Optimization suggestions based on profiling
     */
    class OptimizationHelper {
    public:
        /**
         * Suggest GPU optimizations based on profiling results
         */
        static std::vector<std::string> SuggestGPUOptimizations(
            const PerformanceMetrics& metrics)
        {
            std::vector<std::string> suggestions;
            
            // TODO: Implement optimization suggestions
            // e.g., if memory bandwidth limited, suggest block size adjustments
            
            return suggestions;
        }

        /**
         * Check if feature should be enabled based on performance impact
         */
        static bool IsFeatureWorthEnabling(
            const PerformanceMetrics& with_feature,
            const PerformanceMetrics& without_feature,
            double overhead_threshold = 0.05) // 5% overhead threshold
        {
            if (without_feature.wall_time_ms == 0.0) return false;
            
            double overhead = (with_feature.wall_time_ms - without_feature.wall_time_ms) 
                            / without_feature.wall_time_ms;
            
            return overhead <= overhead_threshold;
        }
    };

} // namespace ValidationOptimization

#endif // VALIDATION_OPTIMIZATION_H
