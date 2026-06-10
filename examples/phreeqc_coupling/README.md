# PHREEQC Coupling Examples

11 standalone example scripts demonstrating each capability of the PHREEQC reactive transport coupling framework.

## Quick Start

All examples can be run independently with synthetic data:

```bash
cd examples/phreeqc_coupling

# Example 1: Wind velocity boundary conditions
python3 01_wind_field_bc.py

# Example 2: Temperature profiles
python3 02_temperature_profile_bc.py

# Example 3: Precipitation infiltration
python3 03_precipitation_recharge.py

# ... and so on
```

## Example Scripts

### Foundation Capabilities (1–5)

| File | Capability | Output | Runtime |
|------|-----------|--------|---------|
| **01_wind_field_bc.py** | Wind velocity as boundary condition | ASCII boundary conditions | <1 s |
| **02_temperature_profile_bc.py** | Temperature profile extraction | Temperature-dependent rate factors | <1 s |
| **03_precipitation_recharge.py** | Precipitation infiltration | Infiltration velocity & dust suppression | <1 s |
| **04_kv_dispersivity.py** | Vertical diffusivity & dispersivity | K_v profile and dispersivity | <1 s |
| **05_stability_classification.py** | Atmospheric stability (PGT A–F) | Stability class and rate modifiers | <1 s |

### Advanced Geochemical Capabilities (6–7)

| File | Capability | Output | Runtime |
|------|-----------|--------|---------|
| **06_valley_amd_hotspots.py** | AMD hotspot detection | GeoJSON hotspot map, risk classification | ~0.2 s |
| **07_sulfide_oxidation.py** | Sulfide oxidation kinetics | Oxidation rates, acid generation, pH change | ~0.2 s |

### Optimization & Caching (8–10)

| File | Capability | Output | Runtime |
|------|-----------|--------|---------|
| **08_spatial_temperature_cache.py** | Scenario library caching | Fast scenario lookup (<30 ms) | <0.1 s |
| **09_dust_suppression.py** | Dust suppression lookup | Dust settling factors | <0.1 s |
| **10_leaching_efficiency_sherwood.py** | Leaching efficiency (Sherwood) | Leaching enhancement factors | <0.1 s |

### Integration (11)

| File | Capability | Output | Runtime |
|------|-----------|--------|---------|
| **11_end_to_end_facility.py** | End-to-end facility workflow | Complete reactive transport simulation | ~20 min |

---

## Physics References

Each example implements peer-reviewed physics:

| Physics Module | References |
|---|---|
| Wind profiles & boundary layer | Businger et al. (1971); Stull (2011) |
| Temperature-dependent kinetics | Nicholson et al. (1990); Arrhenius equation |
| Atmospheric stability | Turner (1994); Paulson & Simpson (1981) |
| AMD hotspot detection | Sherwood (1954); Businger et al. (1971) |
| Sulfide oxidation | Nicholson et al. (1990); Businger et al. (1971) |
| Leaching efficiency | Ranz & Marshall (1952); Sherwood (1954) |

---

## Expected Outputs

Each example creates an output directory with results:

```
01_wind_field_bc_output/
├── wind_bc.txt (ASCII boundary conditions)

02_temperature_output/
├── temperature_bc.txt (Temperature profile)

03_precipitation_output/
├── infiltration_bc.txt (Infiltration BC)

... (similar for other examples)
```

---

## Dependencies

Examples use massconsistent_amr and PHREEQC coupling modules:

```bash
# Install massconsistent_amr with Python bindings
cmake -B build -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON
cmake --build build

# Install optional dependencies
pip install netcdf4 h5py numpy scipy pandas

# Run examples
python3 01_wind_field_bc.py
```

---

## Validation & Performance

All examples include:
- ✅ Synthetic data fallback (run without full wind solver)
- ✅ Physical bounds checking
- ✅ Performance timing measurements
- ✅ Validation against literature values
- ✅ Realistic operational performance expectations

---

## Further Reading

- **User Guide:** `../../docs/phreeqc_coupling/user_guide.md`
- **API Reference:** `../../docs/phreeqc_coupling/api_reference.md`
- **Case Studies:** `../../docs/phreeqc_coupling/case_studies.md`
- **Deployment Guide:** `../../docs/phreeqc_coupling/deployment_guide.md`
- **Troubleshooting:** `../../docs/phreeqc_coupling/troubleshooting.md`

---

**Last Updated:** 2026-06-10  
**massconsistent_amr PHREEQC Coupling v1.0.0**
