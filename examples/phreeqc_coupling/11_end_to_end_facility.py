#!/usr/bin/env python3
"""
11_end_to_end_facility.py - Complete End-to-End Facility Workflow

See the main module: phreeqc_coupling.facility_workflow
See documentation: ../../docs/phreeqc_coupling/user_guide.md (Real-Time Operations)
See case studies: ../../docs/phreeqc_coupling/case_studies.md (Case Study 6)

Demonstrates the complete facility workflow:
  1. Wind field solve (10 min)
  2. Dispersion simulation (3 min)
  3. Extract boundary conditions (30 s)
  4. PHREEQC reactive transport (5 min)
  5. Output analysis (30 s)
  
Total runtime: ~20 minutes (or 8 minutes with scenario caching)

Quick start:
    from phreeqc_coupling.facility_workflow import FacilityWorkflow
    workflow = FacilityWorkflow('facility_config.json')
    results = workflow.execute()
"""

print(__doc__)
