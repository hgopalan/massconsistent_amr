// ============================================================================
// wind_farm_operations.H
// Power curtailment, derating, and operational control for wind farms
//
// Provides real-time wind farm operations capabilities including:
// - Farm-level power curtailment (reduce output to specified limit)
// - Per-turbine derating (reduce individual turbine output by percentage)
// - Ramp rate control (gradual power transitions)
// - Curtailment state tracking
// - Power availability metrics
//
// Business context: 60-80% of operational wind farms use power curtailment
// daily for grid compliance. Grid operators mandate reductions in real-time.
// ============================================================================

#ifndef WIND_FARM_OPERATIONS_H
#define WIND_FARM_OPERATIONS_H

#include <AMReX_REAL.H>
#include <vector>
#include <algorithm>
#include <cmath>

namespace WindFarmOps {

using amrex::Real;

// ============================================================================
// Curtailment mode enumeration
// ============================================================================
enum class CurtailmentMode {
    UNCURTAILED = 0,           // No curtailment active
    POWER_LIMIT = 1,           // Farm-wide power limit enforced
    RAMP_DOWN = 2,             // Ramping down to target power
    RAMP_UP = 3,               // Ramping up from target power
    SELECTIVE_DERATING = 4     // Individual turbine derating
};

// ============================================================================
// Curtailment Parameters Structure
//
// Tracks curtailment state and constraints for a wind farm
// ============================================================================
struct CurtailmentParams {
    // Farm-level curtailment settings
    bool enabled = false;                       // Enable curtailment module
    CurtailmentMode mode = CurtailmentMode::UNCURTAILED;
    
    // Power limit (MW) - the farm must not exceed this
    Real power_limit_mw = 1e6;                 // Default: 1000 MW (essentially unlimited)
    Real current_power_mw = 0.0;               // Current farm power output (read-only)
    Real available_power_mw = 0.0;             // Power available before curtailment (read-only)
    
    // Ramp rate control (MW/minute)
    Real max_ramp_up_rate = 10.0;              // Maximum increase: 10 MW/min
    Real max_ramp_down_rate = 10.0;            // Maximum decrease: 10 MW/min
    Real ramp_setpoint_mw = 0.0;               // Target power during ramping
    Real time_last_ramp_update = 0.0;          // Timestamp of last ramp rate check
    
    // Derating (percentage reduction per turbine)
    std::vector<Real> turbine_derating_pct;    // Derating for each turbine (0-100%)
    bool use_selective_derating = false;       // If true, apply per-turbine derating
    
    // Minimum and maximum power constraints
    Real min_power_limit_mw = 0.0;             // Minimum allowed limit (for safety)
    Real max_power_limit_mw = 1e6;             // Maximum allowed limit
    
    // State tracking
    int num_curtailed_turbines = 0;            // Count of currently curtailed turbines
    Real curtailment_factor = 1.0;             // Global power multiplier (0.0 to 1.0)
    
    // Diagnostics
    int curtailment_events_count = 0;          // Total curtailment events since simulation start
    Real time_curtailed_sec = 0.0;             // Total time curtailed
    Real total_energy_curtailed_mwh = 0.0;     // Cumulative energy lost to curtailment
};

// ============================================================================
// Apply curtailment to turbine power output
//
// Modifies power_output and effective_ct based on curtailment state
// Returns the effective power output (MW) after curtailment
// ============================================================================
inline Real apply_curtailment(
    Real base_power_mw,
    Real derating_pct,
    Real global_curtailment_factor)
{
    // Step 1: Apply per-turbine derating (e.g., 20% derating -> 0.8 multiplier)
    Real derating_factor = (100.0 - std::min(100.0, std::max(0.0, derating_pct))) / 100.0;
    
    // Step 2: Apply global curtailment factor
    Real curtailed_power = base_power_mw * derating_factor * global_curtailment_factor;
    
    return curtailed_power;
}

// ============================================================================
// Compute ramp-limited power transition
//
// Given current power, target power, and ramp rates, compute the power
// that can be reached within the given time step while respecting ramp limits
//
// dt_minutes: Time step in minutes
// current_power: Current farm power output (MW)
// target_power: Desired farm power output (MW)
// max_ramp_up: Maximum ramp up rate (MW/minute)
// max_ramp_down: Maximum ramp down rate (MW/minute)
//
// Returns: Power output after applying ramp rate constraints
// ============================================================================
inline Real compute_ramp_limited_power(
    Real current_power,
    Real target_power,
    Real max_ramp_up,
    Real max_ramp_down,
    Real dt_minutes)
{
    Real power_delta = target_power - current_power;
    
    if (power_delta > 0.0) {
        // Ramping up
        Real max_increase = max_ramp_up * dt_minutes;
        return current_power + std::min(power_delta, max_increase);
    } else if (power_delta < 0.0) {
        // Ramping down
        Real max_decrease = max_ramp_down * dt_minutes;
        return current_power - std::min(-power_delta, max_decrease);
    } else {
        // No change needed
        return current_power;
    }
}

// ============================================================================
// Compute global curtailment factor needed to meet power limit
//
// Given available power and power limit, compute the factor to apply
// to all turbines' power output to meet the constraint.
//
// available_power_mw: Total power available from all turbines (MW)
// power_limit_mw: Maximum allowed power output (MW)
//
// Returns: Curtailment factor (0.0 to 1.0) to apply to all turbines
// ============================================================================
inline Real compute_curtailment_factor(
    Real available_power_mw,
    Real power_limit_mw)
{
    if (available_power_mw <= 0.0) return 0.0;
    if (power_limit_mw >= available_power_mw) return 1.0;
    
    return power_limit_mw / available_power_mw;
}

// ============================================================================
// Update curtailment state based on time evolution
//
// Applies time-dependent updates to curtailment parameters, including:
// - Ramp rate constraints
// - Energy tracking
// - Event counting
//
// dt_minutes: Time step in minutes
// params: Curtailment parameters (modified in-place)
// available_power_mw: Available power from wind turbines (MW)
// ============================================================================
inline void update_curtailment_state(
    Real dt_minutes,
    CurtailmentParams& params,
    Real available_power_mw)
{
    // If curtailment is not enabled, do nothing
    if (!params.enabled) {
        params.curtailment_factor = 1.0;
        params.current_power_mw = available_power_mw;
        return;
    }
    
    // Store available power before curtailment
    params.available_power_mw = available_power_mw;
    
    // Check if curtailment is needed
    if (available_power_mw > params.power_limit_mw) {
        params.mode = CurtailmentMode::POWER_LIMIT;
        params.curtailment_factor = compute_curtailment_factor(
            available_power_mw, params.power_limit_mw);
    } else {
        params.mode = CurtailmentMode::UNCURTAILED;
        params.curtailment_factor = 1.0;
    }
    
    // Apply ramp rate constraints if in ramping mode
    if (params.mode == CurtailmentMode::RAMP_DOWN ||
        params.mode == CurtailmentMode::RAMP_UP) {
        Real new_power = compute_ramp_limited_power(
            params.current_power_mw,
            params.ramp_setpoint_mw,
            params.max_ramp_up_rate,
            params.max_ramp_down_rate,
            dt_minutes);
        
        params.current_power_mw = new_power;
        params.curtailment_factor = (available_power_mw > 0.0) ? 
            new_power / available_power_mw : 0.0;
    } else {
        params.current_power_mw = available_power_mw * params.curtailment_factor;
    }
    
    // Track energy curtailed
    Real energy_curtailed_mwh = (available_power_mw - params.current_power_mw) * 
                                 (dt_minutes / 60.0);
    params.total_energy_curtailed_mwh += energy_curtailed_mwh;
    
    // Track time curtailed
    if (params.curtailment_factor < 1.0 - 1e-6) {
        params.time_curtailed_sec += dt_minutes * 60.0;
        if (params.curtailment_events_count == 0 || params.mode != CurtailmentMode::UNCURTAILED) {
            if (params.mode != CurtailmentMode::UNCURTAILED) {
                params.curtailment_events_count++;
            }
        }
    }
}

// ============================================================================
// Get curtailment status string (for diagnostics)
// ============================================================================
inline std::string get_curtailment_status_string(const CurtailmentParams& params)
{
    std::string mode_str;
    switch (params.mode) {
        case CurtailmentMode::UNCURTAILED:
            mode_str = "UNCURTAILED";
            break;
        case CurtailmentMode::POWER_LIMIT:
            mode_str = "POWER_LIMIT";
            break;
        case CurtailmentMode::RAMP_DOWN:
            mode_str = "RAMP_DOWN";
            break;
        case CurtailmentMode::RAMP_UP:
            mode_str = "RAMP_UP";
            break;
        case CurtailmentMode::SELECTIVE_DERATING:
            mode_str = "SELECTIVE_DERATING";
            break;
        default:
            mode_str = "UNKNOWN";
    }
    
    std::string status = "Curtailment Mode: " + mode_str + 
                         " | Power Limit: " + std::to_string(static_cast<int>(params.power_limit_mw)) + 
                         " MW | Current: " + std::to_string(static_cast<int>(params.current_power_mw)) + 
                         " MW | Factor: " + std::to_string(params.curtailment_factor);
    
    return status;
}

} // namespace WindFarmOps

#endif // WIND_FARM_OPERATIONS_H
