# Data Center Heat Island Scenarios

This directory contains examples and validation scenarios for thermal plume development and buoyant dispersion above single or multiple data center facilities.

## Cases & Scripts

* **`example_datacenter_heat_island.py`**:
  A comprehensive example demonstrating how to set up, load solver output, and analyze the thermal plume characteristics of a standard data center. Compares the numerical solution with the classic Briggs analytical plume rise equations.
* **`datacenter_validation.py`**:
  Performs quantitative validation of plume height and temperature decay profiles against reference meteorological measurements.
* **`datacenter_visualization.py`**:
  An utility script that generates high-resolution horizontal and vertical slices of thermal plumes, displaying buoyant temperature distributions.
* **`example_multi_datacenter.py`**:
  Demonstrates thermal interactions, plume mergers, and downwind cumulative heat island effects from multiple neighboring facilities.
