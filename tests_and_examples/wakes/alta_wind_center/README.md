# Alta Wind Energy Center Operational Wake Case

This case contains one fixed Alta Wind Energy Center setup built from exact operational turbine entries in the public USWTDB v3.1 dataset.

## Operational scope

The committed `turbines.csv` includes **485 turbines** across these Alta phases:

- Alta I (100)
- Alta II (41)
- Alta III (50)
- Alta IV (37)
- Alta V (56)
- Alta VI (partial, 46)
- Alta VIII (partial, 58)
- Alta X (48)
- Alta XI (49)

The turbine coordinates were filtered from the public USWTDB v3.1 snapshot mirrored at:

- `https://raw.githubusercontent.com/chrisbaugh-user/USWTDB/5d391833cc2869de044b96c8bbc0819895cd47e6/uswtdb_v3_1_20200717.csv`

The committed setup keeps only one operational configuration:

- `turbines.csv` — exact Alta turbine coordinates converted to UTM Zone 11N with per-turbine hub height and rotor diameter
- `terrain.csv` — terrain surface covering the full Alta operational footprint
- `inputs.i` — wake solver inputs with aspect ratio 8 (`dx = dy = 120 m`, `dz = 15 m`) and strengthened MLMG smoothing

## Generated outputs

Running the case produces:

- `alta_turbine_layout.png` — all turbines in the committed setup
- `alta_wake_80m.png` — hub-height wind speed field
- `turbine_power_output.csv` — per-turbine inflow speed and power
- `alta_power_output.png` — power plot for all turbines

## Running the case

```bash
export PYTHONPATH=/home/runner/work/massconsistent_amr/massconsistent_amr/build/python:$PYTHONPATH
cd /home/runner/work/massconsistent_amr/massconsistent_amr/tests_and_examples/wakes/alta_wind_center
python3 test_alta_wind_center.py
python3 plot_power.py
```
