#!/usr/bin/env python3
import sys
sys.path.insert(0, "/tmp/workspace/hgopalan/massconsistent_amr/build/python")

from wind_solver import WindSolver

print("Test 1: Creating first WindSolver instance...")
wind1 = WindSolver()
wind1.initialize("inputs.i")
print(f"✓ Wind solver 1 initialized: {wind1.nx}x{wind1.ny}x{wind1.nz}")
wind1.finalize()
print("✓ Wind solver 1 finalized")

print("\nTest 2: Creating second WindSolver instance...")
wind2 = WindSolver()
wind2.initialize("inputs.i")
print(f"✓ Wind solver 2 initialized: {wind2.nx}x{wind2.ny}x{wind2.nz}")
wind2.finalize()
print("✓ Wind solver 2 finalized")

print("\nTest 3: Creating third WindSolver instance...")
wind3 = WindSolver()
wind3.initialize("inputs.i")
print(f"✓ Wind solver 3 initialized: {wind3.nx}x{wind3.ny}x{wind3.nz}")
wind3.finalize()
print("✓ Wind solver 3 finalized")

print("\n✓✓✓ All tests passed! Multiple instances work correctly!")
