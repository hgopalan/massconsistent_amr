#!/bin/bash

# MOST Surface Layer Test Script
cd /home/runner/work/massconsistent_amr/massconsistent_amr/build

# Run the wind solver with MOST surface layer enabled
echo "Testing MOST surface layer boundary conditions..."
timeout 120 ./wind_solver /home/runner/work/massconsistent_amr/massconsistent_amr/regtest/physics/most_surface_layer/input.txt 2>&1 | grep -E "MOST|applying|surface layer"

# Check if successful
if [ $? -eq 0 ]; then
    echo "SUCCESS: MOST surface layer test completed"
    exit 0
else
    echo "FAILED: MOST surface layer test failed"
    exit 1
fi
