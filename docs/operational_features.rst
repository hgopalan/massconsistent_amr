Wind Farm Operational Features
==============================

Overview
--------

The massconsistent_amr solver includes comprehensive operational capabilities for real-time
wind farm management and control. These features address the practical requirements of
wind farm operations, integrating with SCADA systems, grid requirements, and
performance monitoring workflows.

Operational capabilities include:

1. **Power Curtailment and Operations Management** - Real-time power limiting and ramp rate control
2. **SCADA Data Integration** - Real-time telemetry ingestion with validation
3. **Wind Speed and AEP Forecasting** - 10-minute ahead forecasts with uncertainty
4. **Wake Loss Diagnostics** - Per-turbine diagnostics for troubleshooting
5. **Enhanced AEP Accounting** - Availability factors and seasonal breakdowns


Power Curtailment and Operations Management
-------------------------------------------

Purpose
~~~~~~~

Real-time farm-level and per-turbine power limiting is essential for:

- Grid stability compliance (mandated by transmission operators)
- Power management and flow control during high wind periods
- Load balancing across interconnected wind farms
- Emergency response to grid requests

The curtailment module implements:

- **Farm-level power limiting** - Reduce total farm output to a specified maximum
- **Per-turbine derating** - Apply individual power reductions (e.g., 20% derating)
- **Ramp rate control** - Gradual power transitions (e.g., 10 MW/min) to prevent grid impact
- **State tracking and diagnostics** - Monitor curtailment events and energy losses

Implementation
~~~~~~~~~~~~~~

The power curtailment logic is provided in ``wind_farm_operations.H``:

.. code-block:: cpp

    #include "wind_farm_operations.H"
    
    WindFarmOps::CurtailmentParams curtailment;
    curtailment.enabled = true;
    curtailment.power_limit_mw = 50.0;  // Limit farm to 50 MW
    curtailment.max_ramp_down_rate = 10.0;  // 10 MW/minute ramp rate
    
    // After computing turbine power outputs:
    Real available_power = sum(turbine_power_outputs);
    WindFarmOps::update_curtailment_state(dt_minutes, curtailment, available_power);
    
    // Get actual power after curtailment:
    Real actual_farm_power = curtailment.current_power_mw;

Python Interface
~~~~~~~~~~~~~~~~

The Python API provides convenient access to curtailment control:

.. code-block:: python

    from wind_solver import WindSolver
    
    wind = WindSolver("inputs.i")
    wind.solve()
    
    # Apply power curtailment
    wind.set_power_limit(power_limit_mw=50.0)
    wind.set_turbine_derating(turbine_ids=[0, 2, 5], derating_pct=20)
    
    # Get current state
    current_power = wind.get_current_power_output()
    curtailment_factor = wind.get_curtailment_factor()


SCADA Data Integration
----------------------

Purpose
~~~~~~~

SCADA (Supervisory Control and Data Acquisition) systems provide real-time measurements
of wind farm conditions. Integration enables:

- Validation of simulation results against actual conditions
- Data assimilation for improved wind field estimates
- Real-time farm health monitoring
- Rapid anomaly detection

The SCADA interface ingests measurements including:

- Wind speed and direction at hub height
- Turbine power output, yaw, and pitch angles
- Ambient temperature, pressure, and humidity
- Rotor and generator speeds

Implementation
~~~~~~~~~~~~~~

The SCADA interface is provided in ``scada_interface.H``:

.. code-block:: cpp

    #include "scada_interface.H"
    
    ScadaInterface::FarmScadaState scada_state;
    scada_state.timestamp_sec = current_time;
    
    // Populate turbine-level measurements
    for (int i = 0; i < num_turbines; ++i) {
        ScadaInterface::TurbineScadaMeasurement meas;
        meas.turbine_id = i;
        meas.power_output_mw = turbine_power[i];
        meas.wind_speed_mps = hub_wind_speed[i];
        meas.wind_direction_deg = wind_direction[i];
        scada_state.turbine_data.push_back(meas);
    }
    
    // Validate and buffer measurements
    ScadaInterface::ScadaValidator validator;
    validator.validate_farm_state(scada_state, num_turbines);
    
    buffer.push_measurement(scada_state);  // Time-series buffer

Python Interface
~~~~~~~~~~~~~~~~

.. code-block:: python

    # Ingest SCADA measurements
    farm_state = {
        'timestamp': 2024_06_21_14_32_45,
        'turbine_power': [3.5, 3.2, 3.6, ...],      # MW per turbine
        'wind_speeds': [10.1, 10.3, 9.8, ...],      # m/s at hub
        'wind_directions': [270, 271, 270, ...],    # degrees
        'nacelle_yaw': [270, 271, 270, ...],        # actual yaw angle
        'pitch_angles': [0, 0, 2, ...],             # pitch position
        'ambient_temp': 15.2,                       # °C
        'air_pressure': 101.3                       # kPa
    }
    
    wind.update_scada_state(farm_state)
    wind.validate_scada_data()
    
    # Access time-series buffer for analysis
    recent_measurements = wind.get_scada_history(time_window_sec=300)


Wind Speed and AEP Forecasting
------------------------------

Purpose
~~~~~~~

Grid operators require 10-30 minute ahead forecasts of wind farm output to:

- Plan dispatch and reserve scheduling
- Minimize curtailment penalties
- Maintain grid stability
- Coordinate with other renewable sources

The forecasting module provides:

- **Persistence-based predictions** - Baseline using recent measurements
- **Uncertainty quantification** - Confidence intervals that grow with forecast horizon
- **Multi-horizon support** - Generate forecasts at multiple time steps
- **Per-turbine predictions** - Individual turbine power forecasts

Implementation
~~~~~~~~~~~~~~

The forecasting module is in ``wind_forecasting.py``:

.. code-block:: python

    from wind_forecasting import AEPForecastingEngine
    
    # Initialize forecaster
    forecaster = AEPForecastingEngine(wind_solver, num_turbines=50)
    
    # Update with latest SCADA measurement
    wind_speeds = [10.5, 10.3, 10.7, ...]  # m/s per turbine
    wind_directions = [270, 271, 270, ...]
    forecaster.update_scada_measurement(wind_speeds, wind_directions, timestamp_sec)
    
    # Forecast 10 minutes ahead
    forecast = forecaster.forecast_aep(forecast_ahead_sec=600, confidence_level=0.95)
    
    print(f"Expected AEP: {forecast.forecasted_aep_mwh:.2f} MWh")
    print(f"Confidence interval: [{forecast.confidence_interval_low_mwh:.2f}, "
          f"{forecast.confidence_interval_high_mwh:.2f}] MWh")
    print(f"Expected power: {forecast.expected_power_mw:.1f} ± {forecast.expected_power_std_mw:.1f} MW")
    
    # Generate forecasts at multiple horizons
    horizons = [300, 600, 900, 1200]  # 5, 10, 15, 20 minutes
    forecasts = forecaster.forecast_multiple_horizons(horizons)
    
    # Export formatted table
    print(forecaster.export_forecast_table(horizons))

Uncertainty Quantification
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The forecaster estimates wind speed uncertainty that increases with forecast horizon:

- **Base uncertainty:** ~0.5 m/s (SCADA measurement and variability)
- **Growth rate:** ~0.1 m/s per minute forecast ahead
- **Example:** 10-min forecast uncertainty ~ 1.5 m/s

Power uncertainty is derived from wind speed uncertainty using power curve sensitivity.
Confidence intervals assume normal distribution (95% confidence: ±1.96 × std).


Wake Loss Diagnostics
---------------------

Purpose
~~~~~~~

Real-time wake loss analysis enables:

- Quick diagnosis of underperforming turbines
- Identification of wake effects contributing to losses
- Farm health monitoring
- Verification of wake model predictions

The diagnostics module provides:

- **Per-turbine wake loss estimation** - What fraction of power is lost to wakes?
- **Upwind turbine identification** - Which turbines are causing wakes?
- **Loss attribution** - Breakdown of losses by contributing turbine
- **Anomaly detection** - Identification of unexpected underperformance

Implementation
~~~~~~~~~~~~~~

The wake loss diagnostics module is in ``wake_loss_diagnostics.H``:

.. code-block:: cpp

    #include "wake_loss_diagnostics.H"
    
    WakeDiagnostics::WakeLossCalculator calculator(turbines);
    
    // Get upwind turbines for reference turbine 5
    auto upwind = calculator.get_upwind_turbines(
        reference_turbine_id=5,
        wind_direction_deg=270.0,
        wind_speed_mps=10.5);
    
    // Upwind list includes:
    // - Turbine ID, distances, wind speeds
    // - Wake deficit contribution (m/s and fraction)
    // - Estimated power loss
    
    // Compute full diagnostics
    auto diag = calculator.compute_turbine_diagnostics(
        turbine_id=5,
        wind_direction_deg=270.0,
        measured_power_mw=2.5);
    
    // diag includes:
    // - inflow wind speed
    // - expected free-wind power
    // - total wake losses
    // - list of upwind turbines with individual losses
    // - anomaly flag if performance is unexpectedly low

Python Interface
~~~~~~~~~~~~~~~~

.. code-block:: python

    # Identify upwind turbines
    upwind_turbines = farm.get_upwind_turbines(
        reference_turbine_id=5,
        wind_speed_ms=10.5,
        wind_direction_deg=270)
    
    # Compute wake losses for that turbine
    wake_losses_pct = farm.calculate_wake_losses(
        reference_turbine=5,
        upwind_list=upwind_turbines)
    
    print(f"Turbine 5 wake losses: {wake_losses_pct:.1f}%")
    
    # Get full diagnostics
    diagnostics = farm.get_turbine_diagnostics(turbine_id=5)
    print(f"Measured: {diagnostics.measured_power_mw:.2f} MW")
    print(f"Expected (no wakes): {diagnostics.freewind_power_mw:.2f} MW")
    print(f"Expected (with wakes): {diagnostics.freewind_power_mw * (1 - diagnostics.total_wake_loss_fraction):.2f} MW")
    
    # Farm summary
    summary = farm.get_farm_wake_loss_summary()
    print(f"Average wake loss: {summary.average_wake_loss_fraction*100:.1f}%")
    print(f"Total farm power loss: {summary.total_farm_wake_power_loss_mw:.1f} MW")


Enhanced AEP Accounting
-----------------------

Purpose
~~~~~~~

Operational wind farms require detailed AEP accounting for:

- Performance verification and diagnostics
- Energy production assessment
- Seasonal energy breakdown
- Performance monitoring assessments

The enhancement module provides:

- **Availability factors** - Mechanical, electrical, environmental, blade degradation, control
- **Seasonal breakdown** - Monthly energy estimates with seasonal variation
- **Per-turbine micro-siting** - Identify best-performing locations
- **Performance comparison** - Compare measured vs. baseline AEP

Implementation
~~~~~~~~~~~~~~

The AEP enhancement module is in ``aep_enhancements.py``:

.. code-block:: python

    from aep_enhancements import AEPEnhancementEngine, AvailabilityFactors
    
    engine = AEPEnhancementEngine()
    
    # Set site-specific availability factors
    factors = AvailabilityFactors(
        mechanical_availability=0.97,      # 97% uptime
        electrical_availability=0.99,      # 99% electrical availability
        environmental_loss=0.98,           # 2% soiling/icing loss
        blade_degradation=0.99,            # 1% aerodynamic degradation
        control_system_loss=0.99,          # 1% control optimization loss
        curtailment_loss=0.98              # 2% grid curtailment
    )
    engine.set_availability_factors(factors)
    
    # Apply factors to base AEP
    base_aep_gwh = 150.0
    breakdown = engine.apply_availability_adjustment(base_aep_gwh)
    
    print(f"Base AEP (ideal): {breakdown['base_aep_gwh']:.2f} GWh/year")
    print(f"Final AEP (with losses): {breakdown['final_aep_gwh']:.2f} GWh/year")
    print(f"Total losses: {breakdown['total_loss_gwh']:.2f} GWh/year")
    
    # Seasonal breakdown
    seasonal = engine.compute_seasonal_breakdown(breakdown['final_aep_gwh'])
    print(engine.export_seasonal_report())
    
    # Compare measured vs. baseline AEP
    measured_aep = 142.0  # GWh measured
    baseline_aep = 143.0  # GWh baseline
    validation = engine.validate_contractual_guarantee(measured_aep, baseline_aep, tolerance_pct=5)
    print(f"Status: {validation['status']}")
    print(f"Explanation: {validation['explanation']}")

Availability Factors
~~~~~~~~~~~~~~~~~~~~

.. table:: Default Availability Factors

    ========================= ========= ===============================
    Factor                    Default   Description
    ========================= ========= ===============================
    Mechanical Availability   97%       Scheduled/unscheduled downtime
    Electrical Availability   99%       Power electronics, cables, etc.
    Environmental Loss        98%       Soiling, icing, extreme weather
    Blade Degradation         99%       Aerodynamic performance decline
    Control System Loss       99%       Pitch/yaw optimization losses
    Curtailment Loss          98-100%   Grid-requested output reduction
    ========================= ========= ===============================

Users can adjust these factors based on site-specific data, equipment age, and
operational history.


Integration with External Tools
--------------------------------

The operational features integrate seamlessly with:

- **FLORIS** - Wind farm optimization framework
- **PyWake** - Advanced wake modeling
- **SCADA Systems** - Real-time telemetry (via JSON/CSV interfaces)
- **Grid Operations** - Ramp rate and curtailment compliance


References
----------

- Wind farm curtailment: Essential Grid Integration (2020). NREL Technical Report.
- SCADA data assimilation: Wind energy forecasting (2019). Journal of Renewable Energy.
- Wind resource assessment: Best practices (2017). IWEA Guidelines.
