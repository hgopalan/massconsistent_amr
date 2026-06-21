// ============================================================================
// scada_interface.H
// SCADA (Supervisory Control and Data Acquisition) data ingestion interface
//
// Provides real-time wind farm telemetry ingestion from SCADA systems, including:
// - Wind speed and direction measurements at hub height
// - Turbine power output, pitch, and yaw angles
// - Ambient conditions (temperature, pressure)
// - Data validation and quality checking
// - Time-series data buffering
//
// Business context: 20% of operational farms have SCADA integration (growing
// rapidly as wind farms become more digital). Actual measured data improves
// model accuracy significantly over pure NWP/physics-based estimates.
// ============================================================================

#ifndef SCADA_INTERFACE_H
#define SCADA_INTERFACE_H

#include <AMReX_REAL.H>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace ScadaInterface {

using amrex::Real;

// ============================================================================
// SCADA Data Quality Flags
// ============================================================================
enum class DataQuality : int {
    GOOD = 0,                      // Data is valid
    WARNING = 1,                   // Data is valid but suspect
    ERROR = 2,                     // Data is invalid, should not be used
    MISSING = 3                    // Data not available
};

// ============================================================================
// Per-Turbine SCADA Measurement
//
// Represents a single instantaneous SCADA measurement from one turbine
// ============================================================================
struct TurbineScadaMeasurement {
    int turbine_id = -1;                   // Turbine ID
    Real timestamp_sec = 0.0;              // Timestamp (seconds since simulation start)
    
    // Measurements
    Real power_output_mw = 0.0;            // Measured power output (MW)
    Real wind_speed_mps = 0.0;             // Wind speed at hub (m/s)
    Real wind_direction_deg = 0.0;         // Wind direction at hub (degrees)
    Real nacelle_yaw_deg = 0.0;            // Actual nacelle yaw position (degrees)
    Real pitch_angle_deg = 0.0;            // Actual blade pitch angle (degrees)
    Real rotor_speed_rpm = 0.0;            // Rotor rotation speed (RPM)
    Real generator_speed_rpm = 0.0;        // Generator speed (RPM)
    
    // Environmental
    Real ambient_temp_c = 20.0;            // Ambient temperature (°C)
    Real air_pressure_kpa = 101.3;         // Air pressure (kPa)
    Real relative_humidity_pct = 50.0;     // Relative humidity (%)
    
    // Quality indicators
    DataQuality quality = DataQuality::GOOD;
    std::string quality_notes = "";        // Additional quality information
    
    // Validation flags
    bool power_valid = true;
    bool wind_speed_valid = true;
    bool wind_direction_valid = true;
    bool yaw_valid = true;
    bool pitch_valid = true;
};

// ============================================================================
// Farm-Level SCADA State
//
// Represents the complete state of a wind farm at a point in time,
// collected from SCADA systems
// ============================================================================
struct FarmScadaState {
    Real timestamp_sec = 0.0;                              // Timestamp (seconds since simulation start)
    std::vector<TurbineScadaMeasurement> turbine_data;     // Per-turbine measurements
    
    // Farm-level aggregates
    Real total_power_output_mw = 0.0;                      // Sum of all turbine power (MW)
    Real farm_wind_speed_avg_mps = 0.0;                   // Average wind speed across all turbines
    Real farm_wind_direction_avg_deg = 0.0;               // Average wind direction
    
    // Quality metrics
    Real data_completeness_pct = 100.0;                   // Percentage of valid measurements
    int num_turbines_reporting = 0;                       // Count of turbines with valid data
    
    // Source information
    std::string source_id = "";                           // ID of SCADA system (e.g., "SCADA_1")
    bool is_valid = true;                                 // Overall state validity
};

// ============================================================================
// SCADA Measurement Validator
//
// Checks SCADA measurements for validity and applies simple QC flags
// ============================================================================
class ScadaValidator {
public:
    // Acceptable ranges (can be customized per site)
    Real min_wind_speed_mps = 0.0;
    Real max_wind_speed_mps = 50.0;
    Real min_power_output_mw = 0.0;
    Real max_power_output_mw = 100.0;
    Real min_ambient_temp_c = -40.0;
    Real max_ambient_temp_c = 60.0;
    Real min_pressure_kpa = 80.0;
    Real max_pressure_kpa = 110.0;
    
    // Rate-of-change limits (detect sensor failures)
    Real max_wind_speed_change_mps = 5.0;        // Max change per second
    Real max_power_change_mw = 5.0;              // Max change per second
    
    // Last measurements for change detection
    std::vector<Real> last_wind_speed;
    std::vector<Real> last_power_output;
    std::vector<Real> last_timestamp;
    
    // ========================================================================
    // Validate a single turbine measurement
    // ========================================================================
    DataQuality validate_turbine_measurement(
        const TurbineScadaMeasurement& meas,
        int num_turbines)
    {
        // Initialize data if needed
        if (last_wind_speed.size() < static_cast<size_t>(num_turbines)) {
            last_wind_speed.resize(num_turbines, -999.0);
            last_power_output.resize(num_turbines, -999.0);
            last_timestamp.resize(num_turbines, 0.0);
        }
        
        DataQuality quality = DataQuality::GOOD;
        
        // Check wind speed range
        if (meas.wind_speed_mps < min_wind_speed_mps || 
            meas.wind_speed_mps > max_wind_speed_mps) {
            quality = DataQuality::ERROR;
        }
        
        // Check power output range
        if (meas.power_output_mw < min_power_output_mw || 
            meas.power_output_mw > max_power_output_mw) {
            quality = DataQuality::ERROR;
        }
        
        // Check temperature range
        if (meas.ambient_temp_c < min_ambient_temp_c || 
            meas.ambient_temp_c > max_ambient_temp_c) {
            quality = DataQuality::WARNING;
        }
        
        // Check pressure range
        if (meas.air_pressure_kpa < min_pressure_kpa || 
            meas.air_pressure_kpa > max_pressure_kpa) {
            quality = DataQuality::WARNING;
        }
        
        // Check rate of change (detect spikes)
        int tid = meas.turbine_id;
        if (tid >= 0 && tid < static_cast<int>(last_wind_speed.size())) {
            if (last_wind_speed[tid] >= 0.0) {
                Real dt = std::max(0.001, meas.timestamp_sec - last_timestamp[tid]);
                Real ws_rate = std::abs(meas.wind_speed_mps - last_wind_speed[tid]) / dt;
                Real power_rate = std::abs(meas.power_output_mw - last_power_output[tid]) / dt;
                
                if (ws_rate > max_wind_speed_change_mps) {
                    quality = DataQuality::WARNING;
                }
                if (power_rate > max_power_change_mw) {
                    quality = DataQuality::WARNING;
                }
            }
            
            // Update last measurements
            last_wind_speed[tid] = meas.wind_speed_mps;
            last_power_output[tid] = meas.power_output_mw;
            last_timestamp[tid] = meas.timestamp_sec;
        }
        
        return quality;
    }
    
    // ========================================================================
    // Validate entire farm state
    // ========================================================================
    bool validate_farm_state(FarmScadaState& state, int num_turbines)
    {
        int valid_count = 0;
        Real total_power = 0.0;
        
        for (auto& meas : state.turbine_data) {
            DataQuality quality = validate_turbine_measurement(meas, num_turbines);
            meas.quality = quality;
            
            if (quality == DataQuality::GOOD) {
                valid_count++;
                total_power += meas.power_output_mw;
            }
        }
        
        state.num_turbines_reporting = valid_count;
        state.total_power_output_mw = total_power;
        state.data_completeness_pct = (num_turbines > 0) ? 
            (100.0 * valid_count) / num_turbines : 100.0;
        state.is_valid = (valid_count > 0);
        
        return state.is_valid;
    }
};

// ============================================================================
// SCADA Time-Series Buffer
//
// Stores recent SCADA measurements for time-series analysis,
// interpolation, and forecasting
// ============================================================================
struct ScadaTimeSeriesBuffer {
    int max_buffer_size = 360;                      // Keep ~10 minutes at 1-min intervals
    std::vector<FarmScadaState> buffer;             // Time-ordered buffer of farm states
    
    // ========================================================================
    // Add a new measurement to the buffer
    // ========================================================================
    void push_measurement(const FarmScadaState& state)
    {
        buffer.push_back(state);
        
        // Remove oldest measurement if buffer exceeds size limit
        while (buffer.size() > static_cast<size_t>(max_buffer_size)) {
            buffer.erase(buffer.begin());
        }
    }
    
    // ========================================================================
    // Get most recent measurement
    // ========================================================================
    const FarmScadaState& get_latest() const
    {
        static FarmScadaState empty;
        return buffer.empty() ? empty : buffer.back();
    }
    
    // ========================================================================
    // Get measurement at or before specified timestamp
    // ========================================================================
    const FarmScadaState* get_at_time(Real timestamp_sec) const
    {
        // Binary search for timestamp
        for (int i = buffer.size() - 1; i >= 0; --i) {
            if (buffer[i].timestamp_sec <= timestamp_sec) {
                return &buffer[i];
            }
        }
        return nullptr;
    }
    
    // ========================================================================
    // Interpolate farm state at specified timestamp (linear interpolation)
    // ========================================================================
    FarmScadaState interpolate_at_time(Real timestamp_sec) const
    {
        FarmScadaState result;
        result.timestamp_sec = timestamp_sec;
        
        if (buffer.empty()) return result;
        if (buffer.size() == 1) return buffer[0];
        
        // Find surrounding measurements
        int idx_after = -1;
        for (size_t i = 0; i < buffer.size(); ++i) {
            if (buffer[i].timestamp_sec >= timestamp_sec) {
                idx_after = i;
                break;
            }
        }
        
        if (idx_after < 0) {
            // Timestamp is after all measurements
            return buffer.back();
        } else if (idx_after == 0) {
            // Timestamp is before all measurements
            return buffer[0];
        } else {
            // Interpolate between idx_after-1 and idx_after
            const FarmScadaState& s0 = buffer[idx_after - 1];
            const FarmScadaState& s1 = buffer[idx_after];
            
            Real dt = std::max(0.001, s1.timestamp_sec - s0.timestamp_sec);
            Real t = (timestamp_sec - s0.timestamp_sec) / dt;
            t = std::max(0.0, std::min(1.0, t));
            
            result.total_power_output_mw = s0.total_power_output_mw + 
                                           t * (s1.total_power_output_mw - s0.total_power_output_mw);
            result.farm_wind_speed_avg_mps = s0.farm_wind_speed_avg_mps + 
                                             t * (s1.farm_wind_speed_avg_mps - s0.farm_wind_speed_avg_mps);
            result.farm_wind_direction_avg_deg = s0.farm_wind_direction_avg_deg + 
                                                 t * (s1.farm_wind_direction_avg_deg - s0.farm_wind_direction_avg_deg);
            
            return result;
        }
    }
    
    // ========================================================================
    // Clear the buffer
    // ========================================================================
    void clear()
    {
        buffer.clear();
    }
    
    // ========================================================================
    // Get buffer size
    // ========================================================================
    size_t size() const
    {
        return buffer.size();
    }
};

} // namespace ScadaInterface

#endif // SCADA_INTERFACE_H
