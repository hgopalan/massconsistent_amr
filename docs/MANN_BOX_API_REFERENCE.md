# MANN BOX API REFERENCE

**Version**: 1.0  
**Namespace**: `Phase7Diagnostics`, `Phase7Export`  
**Header Files**: `mann_box_validation_diagnostics.H`, `mann_box_export_utilities.H`  
**C++ Standard**: C++17

---

## TABLE OF CONTENTS

1. [Spectral Power Density](#spectral-power-density)
2. [Turbulence Statistics](#turbulence-statistics)
3. [Coherence Analysis](#coherence-analysis)
4. [Energy Balance Validation](#energy-balance-validation)
5. [CSV Export](#csv-export)
6. [NetCDF Export](#netcdf-export)
7. [BTS Export](#bts-export)
8. [Validation Report](#validation-report)

---

## SPECTRAL POWER DENSITY

### Namespace
```cpp
namespace Phase7Diagnostics {
class SpectralPowerDensity { ... };
}
```

### Methods

#### ComputePSD()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static void ComputePSD(
    const amrex::Real* u_timeseries,
    amrex::Real dt,
    std::vector<amrex::Real>& frequencies,
    std::vector<amrex::Real>& psd,
    int n_samples);
```

**Purpose**: Compute one-sided power spectral density from time series

**Parameters**:
- `u_timeseries` — Input velocity time series [m/s], size `n_samples`
- `dt` — Time step [s]
- `frequencies` — Output frequency array [Hz] (resized)
- `psd` — Output PSD array [m²/s²/Hz] (resized)
- `n_samples` — Number of time samples

**Returns**: None (modifies output vectors in-place)

**Physical Theory**:
```
PSD computed from frequency domain:
  S_u(f) = |U(f)|² / (n_samples × df)

Energy conservation (Parseval's theorem):
  ∫₀^∞ S_u(f)df = σ_u²
```

**Example**:
```cpp
std::vector<amrex::Real> u_series(1000);
// ... fill with velocity data ...

std::vector<amrex::Real> frequencies, psd;
SpectralPowerDensity::ComputePSD(
    u_series.data(),
    0.01,           // 10 ms time step
    frequencies,
    psd,
    u_series.size());

// frequencies: [0.001, 0.002, ..., 5.0] Hz
// psd: corresponding power spectral densities [m²/s²/Hz]
```

---

#### ExtractPeakFrequency()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static amrex::Real ExtractPeakFrequency(
    const amrex::Real* psd,
    const amrex::Real* frequencies,
    int n_frequencies);
```

**Purpose**: Find the frequency at which PSD is maximum

**Parameters**:
- `psd` — Power spectral density array [m²/s²/Hz]
- `frequencies` — Frequency bin array [Hz]
- `n_frequencies` — Number of frequency bins

**Returns**: Peak frequency [Hz]

**Physical Interpretation**:
- Represents the most energetic frequency component
- Typically: f_peak = U_mean / L_u
- For L_u = 300 m, U = 10 m/s: f_peak ≈ 0.033 Hz

**Example**:
```cpp
amrex::Real f_peak = SpectralPowerDensity::ExtractPeakFrequency(
    psd.data(), frequencies.data(), psd.size());
printf("Peak frequency: %.4f Hz\n", f_peak);
```

---

#### ComputeIntegralLengthScale()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static amrex::Real ComputeIntegralLengthScale(
    const amrex::Real* psd,
    const amrex::Real* frequencies,
    amrex::Real U,
    int n_frequencies);
```

**Purpose**: Compute integral length scale from PSD

**Parameters**:
- `psd` — Power spectral density [m²/s²/Hz]
- `frequencies` — Frequency array [Hz]
- `U` — Mean wind speed [m/s]
- `n_frequencies` — Number of bins

**Returns**: Integral length scale [m]

**Theory**:
```
L_u = (U/π) × ∫₀^∞ [S_u(f) / U²] df

In discrete form:
L_u = (U/π) × Σᵢ [S_u(fᵢ) / U²] × Δfᵢ
```

**Typical Values**:
- Grassland: 100–200 m
- Forest: 300–500 m
- Urban: 400–800 m

**Example**:
```cpp
amrex::Real L_u = SpectralPowerDensity::ComputeIntegralLengthScale(
    psd.data(), frequencies.data(), 10.0, psd.size());
printf("Integral length scale: %.1f m\n", L_u);
```

---

## TURBULENCE STATISTICS

### Namespace
```cpp
namespace Phase7Diagnostics {
class TurbulenceStatistics { ... };
}
```

### Methods

#### ComputeTurbulenceIntensity()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static amrex::Real ComputeTurbulenceIntensity(
    const amrex::Real* u_timeseries,
    amrex::Real U_mean,
    int n_samples);
```

**Purpose**: Compute turbulence intensity (TI)

**Formula**: `TI = σ_u / U_mean`

**Parameters**:
- `u_timeseries` — Velocity time series [m/s]
- `U_mean` — Mean wind speed [m/s]
- `n_samples` — Number of samples

**Returns**: Turbulence intensity (0–1 range, typically 0.08–0.20)

**Physical Ranges**:
- Smooth water: 0.05–0.08
- Grassland: 0.10–0.15
- Forest: 0.15–0.25
- Urban: 0.20–0.35

**Example**:
```cpp
amrex::Real TI = TurbulenceStatistics::ComputeTurbulenceIntensity(
    u_series.data(), 10.0, u_series.size());
printf("Turbulence intensity: %.2f%% \n", TI * 100.0);
```

---

#### ComputeRMS()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static amrex::Real ComputeRMS(
    const amrex::Real* u_timeseries,
    int n_samples);
```

**Purpose**: Compute root-mean-square (RMS) of velocity fluctuations

**Formula**: `σ_u = √(mean((u - mean(u))²))`

**Returns**: RMS velocity [m/s]

**Example**:
```cpp
amrex::Real u_rms = TurbulenceStatistics::ComputeRMS(
    u_series.data(), u_series.size());
printf("u_RMS: %.4f m/s\n", u_rms);
```

---

#### ComputeKurtosis()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static amrex::Real ComputeKurtosis(
    const amrex::Real* u_timeseries,
    int n_samples);
```

**Purpose**: Compute kurtosis (4th statistical moment)

**Formula**: `K = m₄ / σ⁴`

**Interpretation**:
- K = 3: Normal distribution (Gaussian)
- K > 3: Heavy-tailed (outliers present)
- K < 3: Light-tailed (uniform-like)

**Returns**: Kurtosis value (typically 2.5–4.5)

**Example**:
```cpp
amrex::Real kurt = TurbulenceStatistics::ComputeKurtosis(
    u_series.data(), u_series.size());
```

---

#### ComputeSkewness()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static amrex::Real ComputeSkewness(
    const amrex::Real* u_timeseries,
    int n_samples);
```

**Purpose**: Compute skewness (3rd statistical moment)

**Formula**: `S = m₃ / σ³`

**Interpretation**:
- S = 0: Symmetric distribution
- S < 0: Left-skewed
- S > 0: Right-skewed

**Returns**: Skewness coefficient (typically -0.5 to +0.5)

---

## COHERENCE ANALYSIS

### Namespace
```cpp
namespace Phase7Diagnostics {
class CoherenceAnalysis { ... };
}
```

### Methods

#### ComputeSpatialCoherence()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static amrex::Real ComputeSpatialCoherence(
    amrex::Real separation,
    amrex::Real frequency,
    amrex::Real decay_rate = 5.0);
```

**Purpose**: Compute spatial coherence as function of separation and frequency

**Formula**: `Coh(r,f) = exp(-decay_rate × f × r / U)`

**Parameters**:
- `separation` — Distance between points [m]
- `frequency` — Frequency [Hz]
- `decay_rate` — Decay coefficient [m⁻¹], default 5.0

**Returns**: Coherence value [0, 1]

**Physical Meaning**:
- Coherence = 1: Perfect correlation
- Coherence = 0: No correlation
- Decays with distance and frequency

**Example**:
```cpp
amrex::Real coh = CoherenceAnalysis::ComputeSpatialCoherence(
    10.0,   // separation [m]
    0.1,    // frequency [Hz]
    5.0);   // decay rate [m⁻¹]
printf("Coherence at 10m, 0.1Hz: %.4f\n", coh);
```

---

#### ComputeAutocorrelation()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static amrex::Real ComputeAutocorrelation(
    const amrex::Real* timeseries,
    int lag,
    int n_samples);
```

**Purpose**: Compute temporal autocorrelation

**Formula**: `ρ(τ) = <u(t)·u(t+τ)> / <u²>`

**Parameters**:
- `timeseries` — Time series data
- `lag` — Time lag in samples
- `n_samples` — Total samples

**Returns**: Autocorrelation [-1, 1]

**Physical Properties**:
- ρ(0) = 1 (always)
- ρ(τ) decreases monotonically
- Related to integral time scale

**Example**:
```cpp
std::vector<amrex::Real> autocorr(100);
for (int lag = 0; lag < 100; ++lag) {
    autocorr[lag] = CoherenceAnalysis::ComputeAutocorrelation(
        u_series.data(), lag, u_series.size());
}
```

---

#### ComputeIntegralTimeScale()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static amrex::Real ComputeIntegralTimeScale(
    const amrex::Real* autocorr,
    amrex::Real dt,
    int n_lags);
```

**Purpose**: Compute integral time scale from autocorrelation

**Formula**: `T_L = ∫₀^∞ ρ(τ)dτ`

**Returns**: Integral time scale [s]

**Physical Relation**:
```
T_L = L_u / U_mean
e.g., L_u = 300m, U = 10m/s → T_L = 30s
```

---

## ENERGY BALANCE VALIDATION

### Namespace
```cpp
namespace Phase7Diagnostics {
class EnergyBalanceValidator { ... };
}
```

### Methods

#### ComputeTKE()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static amrex::Real ComputeTKE(
    amrex::Real u_rms,
    amrex::Real v_rms,
    amrex::Real w_rms);
```

**Purpose**: Compute turbulent kinetic energy

**Formula**: `TKE = 0.5 × (σ_u² + σ_v² + σ_w²)`

**Returns**: TKE [m²/s²]

**Example**:
```cpp
amrex::Real tke = EnergyBalanceValidator::ComputeTKE(
    1.2, 0.96, 0.72);  // [m/s]
printf("TKE: %.4f m²/s²\n", tke);  // ≈ 1.0 m²/s²
```

---

#### ComputeDissipationRate()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static amrex::Real ComputeDissipationRate(
    amrex::Real tke,
    amrex::Real dissipation_scale);
```

**Purpose**: Estimate dissipation rate

**Formula**: `ε = C_μ × (k^1.5) / L_ε`  (C_μ = 0.09)

**Returns**: Dissipation rate [m²/s³]

---

#### ValidateAnisotropy()

```cpp
AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
static void ValidateAnisotropy(
    amrex::Real u_rms,
    amrex::Real v_rms,
    amrex::Real w_rms,
    amrex::Real& ratio_v_u,
    amrex::Real& ratio_w_u);
```

**Purpose**: Check anisotropy ratios are physical

**Output**:
- `ratio_v_u` = v_rms / u_rms (typically 0.6–0.9)
- `ratio_w_u` = w_rms / u_rms (typically 0.3–0.7)

---

## CSV EXPORT

### Namespace
```cpp
namespace Phase7Export {
class CSVExporter { ... };
}
```

### Methods

#### ExportSpectralPSD()

```cpp
static bool ExportSpectralPSD(
    const std::string& filename,
    const std::vector<amrex::Real>& frequencies,
    const std::vector<amrex::Real>& psd_u,
    const std::vector<amrex::Real>& psd_v,
    const std::vector<amrex::Real>& psd_w);
```

**Purpose**: Export spectral PSD to CSV

**Output Format**:
```csv
Frequency_Hz,PSD_u_m2s2Hz,PSD_v_m2s2Hz,PSD_w_m2s2Hz,Coherence_uv,Coherence_uw,Coherence_vw
0.001000,1.234560,0.987648,0.617280,0.999999,0.999998,0.999997
```

---

#### ExportStatisticsSummary()

```cpp
static bool ExportStatisticsSummary(
    const std::string& filename,
    amrex::Real u_rms,
    amrex::Real v_rms,
    amrex::Real w_rms,
    amrex::Real u_mean,
    amrex::Real TI,
    amrex::Real integral_length_u,
    amrex::Real integral_time_scale,
    amrex::Real peak_frequency,
    amrex::Real skewness,
    amrex::Real kurtosis);
```

**Purpose**: Export key statistics to CSV

**Output Format**:
```csv
Parameter,Value,Unit
u_RMS,1.200000,m/s
Turbulence_Intensity,0.120000,fraction
```

---

## BTS EXPORT

### Namespace
```cpp
namespace Phase7Export {
class BTSExporter { ... };
}
```

### Methods

#### ExportBTS()

```cpp
static bool ExportBTS(
    const std::string& filename,
    const std::vector<amrex::Real>& u_timeseries,
    const std::vector<amrex::Real>& v_timeseries,
    const std::vector<amrex::Real>& w_timeseries,
    amrex::Real dt,
    amrex::Real z);
```

**Purpose**: Export turbulence to OpenFAST/TurbSim BTS format

**Format**:
- Binary format, little-endian
- Header: [uint32 n_samples, float dt, float z]
- Data: Interleaved floats (u, v, w, u, v, w, ...)

**Compatible with**: OpenFAST, TurbSim, FAST.Farm

---

#### ExportBTSMetadata()

```cpp
static bool ExportBTSMetadata(
    const std::string& filename,
    amrex::Real u_rms,
    amrex::Real v_rms,
    amrex::Real w_rms,
    amrex::Real dt,
    int n_samples,
    const std::string& spectrum_type = "VonKarman");
```

**Purpose**: Create .meta file with simulation parameters

---

## VALIDATION REPORT

### Namespace
```cpp
namespace Phase7Export {
class ValidationReportGenerator { ... };
}
```

### Methods

#### GenerateValidationReport()

```cpp
static bool GenerateValidationReport(
    const std::string& filename,
    const std::string& title,
    const std::vector<std::pair<std::string, std::string>>& metadata);
```

**Purpose**: Generate human-readable validation report

**Example**:
```cpp
std::vector<std::pair<std::string, std::string>> metadata = {
    {"Simulation Date", "2026-06-04"},
    {"Domain Size", "500×500×300 m"},
    {"Grid Resolution", "50×50×30 cells"},
    {"Mann Box Phase", "7"}
};

ValidationReportGenerator::GenerateValidationReport(
    "validation_report.txt",
    "MANN BOX VALIDATION REPORT",
    metadata);
```

---

## ERROR HANDLING

All functions return `bool` or use AMREX error handling:

```cpp
// Example with error checking
std::vector<amrex::Real> frequencies, psd;
try {
    SpectralPowerDensity::ComputePSD(
        u_series.data(),
        0.01,
        frequencies,
        psd,
        u_series.size());
} catch (const std::exception& e) {
    amrex::Print() << "Error computing PSD: " << e.what() << "\n";
}
```

---

## GPU COMPATIBILITY

All functions in `Phase7Diagnostics` are GPU-compatible (marked `AMREX_GPU_HOST_DEVICE`):

```cpp
// GPU kernel example
auto const& ma = mfi.tilebox();
ParallelFor(ma, [=] AMREX_GPU_DEVICE(int i, int j, int k) {
    auto rms = TurbulenceStatistics::ComputeRMS(...);
    // Use RMS value on GPU
});
```

---

## PERFORMANCE NOTES

| Operation | CPU Time | GPU Time | Complexity |
|-----------|----------|----------|------------|
| ComputeRMS() | 1 μs | - | O(n) |
| ComputeTI() | 2 μs | - | O(n) |
| ComputeAutocorr() | 100 μs | - | O(n²) |
| ExportSpectralPSD() | 5 ms | - | O(n) |
| ExportBTS() | 10 ms | - | O(n) |

---

**API Reference Version**: 1.0  
**Last Updated**: June 4, 2026
