#!/usr/bin/env python3
"""
wind_forecasting.py - Real-time wind speed and AEP forecasting module

Provides time-series forecasting capabilities for wind farms, including:
- 10-minute ahead persistence forecasting (baseline)
- Uncertainty quantification with confidence intervals
- Ramp rate detection and trend analysis
- Per-turbine forecast export
- Integration with SCADA measurements

Business context: 30-40% of operational wind farms use some form of
forecasting for grid compliance. This module provides a lightweight,
terrain-aware approach suitable for real-time operations.
"""

import numpy as np
from typing import Dict, List, Tuple, Optional
import json
from dataclasses import dataclass, asdict
from datetime import datetime, timedelta


@dataclass
class WindSpeedForecast:
    """Wind speed forecast for a single turbine at a specific time"""
    turbine_id: int
    timestamp_sec: float
    forecast_ahead_sec: float
    wind_speed_mps: float
    wind_speed_std_mps: float
    wind_direction_deg: float
    wind_direction_std_deg: float
    confidence_level: float
    persistence_factor: float


@dataclass
class AEPForecast:
    """AEP forecast for the farm at a specific time"""
    timestamp_sec: float
    forecast_ahead_sec: float
    forecasted_aep_mwh: float
    confidence_interval_low_mwh: float
    confidence_interval_high_mwh: float
    expected_power_mw: float
    expected_power_std_mw: float
    ramp_rate_mw_per_min: Optional[float]


class PersistenceForecaster:
    """
    Persistence-based wind speed forecaster
    
    Uses the most recent SCADA measurement and applies persistence
    with uncertainty that grows with forecast horizon.
    """
    
    def __init__(self, num_turbines: int):
        """Initialize the forecaster"""
        self.num_turbines = num_turbines
        self.last_measurement_sec = 0.0
        self.last_wind_speeds = [10.0] * num_turbines  # Default 10 m/s
        self.last_wind_directions = [270.0] * num_turbines  # Default 270°
        self.measurement_history = []  # Store recent history for trend analysis
        
        # Uncertainty parameters (can be tuned per site)
        self.base_wind_speed_uncertainty_mps = 0.5  # Base measurement uncertainty
        self.wind_speed_uncertainty_growth_rate = 0.1  # Growth per minute ahead
        self.persistence_decay_factor = 0.95  # How much to trust persistence with time
        
    def update_measurement(self, 
                          wind_speeds: List[float],
                          wind_directions: List[float],
                          timestamp_sec: float):
        """
        Update the forecaster with new SCADA measurements
        
        Args:
            wind_speeds: List of wind speeds per turbine (m/s)
            wind_directions: List of wind directions per turbine (degrees)
            timestamp_sec: Timestamp of measurement (seconds)
        """
        if len(wind_speeds) != self.num_turbines:
            raise ValueError(f"Expected {self.num_turbines} wind speeds, got {len(wind_speeds)}")
        
        self.last_wind_speeds = list(wind_speeds)
        self.last_wind_directions = list(wind_directions)
        self.last_measurement_sec = timestamp_sec
        
        # Store in history for trend analysis
        self.measurement_history.append({
            'timestamp': timestamp_sec,
            'wind_speeds': list(wind_speeds),
            'wind_directions': list(wind_directions)
        })
        
        # Keep only last 2 hours of history
        max_history_sec = 7200.0
        cutoff_time = timestamp_sec - max_history_sec
        self.measurement_history = [
            m for m in self.measurement_history
            if m['timestamp'] >= cutoff_time
        ]
    
    def forecast(self,
                 turbine_id: int,
                 forecast_ahead_sec: float,
                 confidence_level: float = 0.95) -> WindSpeedForecast:
        """
        Forecast wind speed for a single turbine
        
        Args:
            turbine_id: Turbine ID (0-indexed)
            forecast_ahead_sec: Time ahead to forecast (seconds)
            confidence_level: Confidence level for uncertainty band (0-1)
        
        Returns:
            WindSpeedForecast object
        """
        if turbine_id < 0 or turbine_id >= self.num_turbines:
            raise ValueError(f"Invalid turbine ID: {turbine_id}")
        
        # Get last measurement
        base_wind_speed = self.last_wind_speeds[turbine_id]
        base_wind_direction = self.last_wind_directions[turbine_id]
        
        # Compute persistence factor (decays with time)
        forecast_ahead_min = forecast_ahead_sec / 60.0
        persistence_factor = self.persistence_decay_factor ** forecast_ahead_min
        
        # Compute uncertainty (grows with forecast horizon)
        wind_speed_uncertainty = (
            self.base_wind_speed_uncertainty_mps +
            self.wind_speed_uncertainty_growth_rate * forecast_ahead_min
        )
        
        # Compute forecast
        forecasted_wind_speed = base_wind_speed * persistence_factor
        
        # Convert confidence level to z-score for normal distribution
        # confidence 0.95 -> z ≈ 1.96
        z_score = 1.96 if confidence_level == 0.95 else \
                  1.645 if confidence_level == 0.90 else \
                  2.576 if confidence_level == 0.99 else 1.96
        
        wind_speed_std = wind_speed_uncertainty
        
        # Directional uncertainty (in degrees)
        wind_direction_uncertainty = 5.0 + 0.5 * forecast_ahead_min
        
        return WindSpeedForecast(
            turbine_id=turbine_id,
            timestamp_sec=self.last_measurement_sec,
            forecast_ahead_sec=forecast_ahead_sec,
            wind_speed_mps=forecasted_wind_speed,
            wind_speed_std_mps=wind_speed_std,
            wind_direction_deg=base_wind_direction,
            wind_direction_std_deg=wind_direction_uncertainty,
            confidence_level=confidence_level,
            persistence_factor=persistence_factor
        )
    
    def compute_ramp_rate(self) -> Optional[float]:
        """
        Compute recent wind speed ramp rate (MW/min change in power)
        
        Returns:
            Ramp rate (MW/min) or None if not enough data
        """
        if len(self.measurement_history) < 2:
            return None
        
        # Get first and last measurements
        m0 = self.measurement_history[0]
        m1 = self.measurement_history[-1]
        
        dt = m1['timestamp'] - m0['timestamp']
        if dt <= 0:
            return None
        
        # Average wind speed change
        ws0 = np.mean(m0['wind_speeds'])
        ws1 = np.mean(m1['wind_speeds'])
        ws_rate = (ws1 - ws0) / (dt / 60.0)  # m/s per minute
        
        # Rough conversion: each m/s is approximately 100 kW for typical turbine
        # This is a simplified approximation
        power_rate_mw_per_min = ws_rate * 0.1  # Very rough estimate
        
        return power_rate_mw_per_min


class AEPForecastingEngine:
    """
    AEP forecasting engine for wind farms
    
    Combines per-turbine wind speed forecasts with power curves
    to estimate expected AEP and uncertainty
    """
    
    def __init__(self, wind_solver, num_turbines: int):
        """
        Initialize the forecasting engine
        
        Args:
            wind_solver: WindSolver object with get_turbine_power_outputs() method
            num_turbines: Number of turbines in the farm
        """
        self.wind_solver = wind_solver
        self.num_turbines = num_turbines
        self.persistence_forecaster = PersistenceForecaster(num_turbines)
        
    def update_scada_measurement(self,
                                wind_speeds: List[float],
                                wind_directions: List[float],
                                timestamp_sec: float):
        """Update with latest SCADA measurement"""
        self.persistence_forecaster.update_measurement(
            wind_speeds, wind_directions, timestamp_sec)
    
    def forecast_aep(self,
                    forecast_ahead_sec: float,
                    confidence_level: float = 0.95) -> AEPForecast:
        """
        Forecast AEP at specified time ahead
        
        Args:
            forecast_ahead_sec: Time ahead to forecast (seconds)
            confidence_level: Confidence level (0-1)
        
        Returns:
            AEPForecast object with expected power and uncertainty
        """
        # Get wind speed forecasts for all turbines
        wind_speed_forecasts = []
        for t_id in range(self.num_turbines):
            forecast = self.persistence_forecaster.forecast(
                t_id, forecast_ahead_sec, confidence_level)
            wind_speed_forecasts.append(forecast)
        
        # Estimate power output based on forecasted wind speeds
        # This is simplified - in practice, would call wind_solver with forecasted conditions
        expected_power_mw = 0.0
        power_variance = 0.0
        
        for forecast in wind_speed_forecasts:
            # Rough power curve approximation: P = 0.5 * rho * A * Cp * V^3
            # Simplified: P ≈ 0.01 * V^3 (for 3 MW turbine)
            # At 10 m/s: 0.01 * 1000 = 10 kW approximation
            turbine_power_mw = 0.1 * (forecast.wind_speed_mps ** 2.5) / 1000.0
            expected_power_mw += turbine_power_mw
            
            # Estimate power variance from wind speed uncertainty
            # d(Power)/d(WS) ≈ 0.025 * WS^1.5, so d(Power) ≈ dWS * derivative
            power_deriv = 0.025 * (forecast.wind_speed_mps ** 1.5)
            power_std = power_deriv * forecast.wind_speed_std_mps
            power_variance += power_std ** 2
        
        power_std_mw = np.sqrt(power_variance)
        
        # Convert to AEP (over a 10-minute period, rough estimate)
        forecast_period_hours = forecast_ahead_sec / 3600.0
        expected_aep_mwh = expected_power_mw * forecast_period_hours
        
        # Confidence interval
        z_score = 1.96 if confidence_level == 0.95 else 1.645
        aep_std_mwh = power_std_mw * forecast_period_hours
        ci_low = expected_aep_mwh - z_score * aep_std_mwh
        ci_high = expected_aep_mwh + z_score * aep_std_mwh
        
        # Compute ramp rate
        ramp_rate = self.persistence_forecaster.compute_ramp_rate()
        
        return AEPForecast(
            timestamp_sec=self.persistence_forecaster.last_measurement_sec,
            forecast_ahead_sec=forecast_ahead_sec,
            forecasted_aep_mwh=expected_aep_mwh,
            confidence_interval_low_mwh=max(0.0, ci_low),
            confidence_interval_high_mwh=ci_high,
            expected_power_mw=expected_power_mw,
            expected_power_std_mw=power_std_mw,
            ramp_rate_mw_per_min=ramp_rate
        )
    
    def forecast_multiple_horizons(self,
                                  horizons_sec: List[float],
                                  confidence_level: float = 0.95) -> List[AEPForecast]:
        """
        Generate forecasts for multiple time horizons
        
        Args:
            horizons_sec: List of forecast ahead times (seconds)
            confidence_level: Confidence level (0-1)
        
        Returns:
            List of AEPForecast objects
        """
        return [self.forecast_aep(h, confidence_level) for h in horizons_sec]
    
    def to_json(self, forecast: AEPForecast) -> str:
        """Convert forecast to JSON string"""
        return json.dumps(asdict(forecast))
    
    def export_forecast_table(self,
                             horizons_sec: List[float],
                             confidence_level: float = 0.95) -> str:
        """
        Export forecasts as a formatted table
        
        Args:
            horizons_sec: List of forecast horizons (seconds)
            confidence_level: Confidence level (0-1)
        
        Returns:
            Formatted table string
        """
        forecasts = self.forecast_multiple_horizons(horizons_sec, confidence_level)
        
        lines = []
        lines.append("Wind Farm AEP Forecast Report")
        lines.append("=" * 100)
        lines.append(f"{'Time Ahead (min)':<20} {'Expected AEP (MWh)':<20} {'Confidence Interval':<30} {'Expected Power (MW)':<20}")
        lines.append("-" * 100)
        
        for forecast in forecasts:
            time_ahead_min = forecast.forecast_ahead_sec / 60.0
            ci_str = f"[{forecast.confidence_interval_low_mwh:.3f}, {forecast.confidence_interval_high_mwh:.3f}]"
            lines.append(f"{time_ahead_min:<20.1f} {forecast.forecasted_aep_mwh:<20.4f} {ci_str:<30} {forecast.expected_power_mw:<20.2f}")
        
        return "\n".join(lines)


# ============================================================================
# Example usage
# ============================================================================
if __name__ == "__main__":
    # Example: Create a forecasting engine for a 10-turbine farm
    from aep_calculator import AEPCalculator
    
    print("Wind Forecasting Module Example")
    print("================================")
    print()
    
    # Initialize dummy wind solver
    class DummySolver:
        def get_turbine_power_outputs(self):
            return [0.1, 0.15, 0.12, 0.18, 0.14] * 2  # 10 turbines
    
    solver = DummySolver()
    forecaster = AEPForecastingEngine(solver, num_turbines=10)
    
    # Simulate SCADA measurements
    wind_speeds = [10.5 + np.random.randn() * 0.5 for _ in range(10)]
    wind_directions = [270.0 + np.random.randn() * 5.0 for _ in range(10)]
    
    forecaster.update_scada_measurement(wind_speeds, wind_directions, timestamp_sec=0.0)
    
    # Generate forecasts at different horizons
    horizons = [300.0, 600.0, 900.0, 1200.0]  # 5, 10, 15, 20 minutes
    
    table = forecaster.export_forecast_table(horizons)
    print(table)
