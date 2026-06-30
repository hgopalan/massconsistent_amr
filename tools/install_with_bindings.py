#!/usr/bin/env python3
"""
Cross-platform installation script for massconsistent_amr with Python bindings.

This script automates the installation of massconsistent_amr with Python bindings,
including Python environment detection and setup for Windows, Linux, and macOS.

Author: massconsistent_amr development team
Date: June 2026
"""

import os
import sys
import subprocess
import platform
import shutil
import argparse
from pathlib import Path
import json

# Color output for better readability
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

    @staticmethod
    def disable():
        Colors.HEADER = ''
        Colors.OKBLUE = ''
        Colors.OKCYAN = ''
        Colors.OKGREEN = ''
        Colors.WARNING = ''
        Colors.FAIL = ''
        Colors.ENDC = ''
        Colors.BOLD = ''
        Colors.UNDERLINE = ''


def print_header(text):
    """Print a formatted header."""
    print(f"\n{Colors.HEADER}{Colors.BOLD}{'='*60}")
    print(text)
    print(f"{'='*60}{Colors.ENDC}\n")


def print_info(text):
    """Print informational message."""
    print(f"{Colors.OKCYAN}[INFO]{Colors.ENDC} {text}")


def print_success(text):
    """Print success message."""
    print(f"{Colors.OKGREEN}[SUCCESS]{Colors.ENDC} {text}")


def print_warning(text):
    """Print warning message."""
    print(f"{Colors.WARNING}[WARNING]{Colors.ENDC} {text}")


def print_error(text):
    """Print error message."""
    print(f"{Colors.FAIL}[ERROR]{Colors.ENDC} {text}")


def run_command(cmd, description="", check=True):
    """
    Run a shell command and handle errors.

    Args:
        cmd: Command to run (list or string)
        description: Description of the command (for logging)
        check: If True, raise exception on command failure

    Returns:
        CompletedProcess object
    """
    if isinstance(cmd, str):
        cmd = cmd.split()

    if description:
        print_info(description)
        print(f"  Running: {' '.join(cmd)}")

    try:
        result = subprocess.run(cmd, check=False, capture_output=False, text=True)
        if check and result.returncode != 0:
            print_error(f"Command failed with return code {result.returncode}")
            sys.exit(1)
        return result
    except FileNotFoundError as e:
        print_error(f"Command not found: {cmd[0]}")
        raise e


def detect_os():
    """Detect the operating system."""
    system = platform.system()
    if system == 'Darwin':
        return 'macos'
    elif system == 'Windows':
        return 'windows'
    elif system == 'Linux':
        return 'linux'
    else:
        return 'unknown'


def find_python():
    """
    Find the Python executable and verify version.

    Returns:
        Path to Python executable or None if not found
    """
    print_header("Detecting Python Installation")

    # Try various Python executables
    python_candidates = ['python3', 'python', 'python3.11', 'python3.10', 'python3.9']

    if detect_os() == 'windows':
        # On Windows, also check for python.exe and pythonw.exe
        python_candidates.extend(['python.exe', 'python3.exe'])

    for python_cmd in python_candidates:
        try:
            result = subprocess.run(
                [python_cmd, '--version'],
                capture_output=True,
                text=True,
                timeout=5
            )
            if result.returncode == 0:
                version_str = result.stdout.strip()
                print_success(f"Found Python: {python_cmd} ({version_str})")

                # Verify Python version is 3.8 or higher
                version_parts = version_str.split()[-1].split('.')
                try:
                    major = int(version_parts[0])
                    minor = int(version_parts[1])
                    if major >= 3 and minor >= 8:
                        # Get full path
                        full_path = shutil.which(python_cmd)
                        print_info(f"Python path: {full_path}")
                        return full_path
                    else:
                        print_warning(f"Python {version_str} is too old (need 3.8+)")
                except (ValueError, IndexError):
                    print_warning(f"Could not parse Python version: {version_str}")
                    continue
        except (subprocess.TimeoutExpired, FileNotFoundError, Exception):
            continue

    return None


def check_cmake():
    """
    Check if CMake is available.

    Returns:
        Path to cmake executable or None
    """
    print_header("Checking CMake Installation")

    try:
        result = subprocess.run(
            ['cmake', '--version'],
            capture_output=True,
            text=True,
            timeout=5
        )
        if result.returncode == 0:
            version_line = result.stdout.split('\n')[0]
            print_success(f"CMake found: {version_line}")
            return shutil.which('cmake')
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    print_error("CMake not found. Please install CMake (version 3.20+)")
    return None


def check_git():
    """
    Check if Git is available and if submodules are initialized.

    Returns:
        True if git is available and submodules are initialized, False otherwise
    """
    print_header("Checking Git and Submodules")

    try:
        result = subprocess.run(
            ['git', '--version'],
            capture_output=True,
            text=True,
            timeout=5
        )
        if result.returncode == 0:
            print_success(f"Git found: {result.stdout.strip()}")

            # Check if AMReX submodule is initialized
            amrex_path = Path('external/amrex/CMakeLists.txt')
            if amrex_path.exists():
                print_success("AMReX submodule is initialized")
                return True
            else:
                print_warning("AMReX submodule not initialized. Initializing...")
                run_command(
                    ['git', 'submodule', 'update', '--init', '--recursive'],
                    description="Initializing git submodules"
                )
                return True
    except FileNotFoundError:
        print_warning("Git not found, assuming submodules are already initialized")
        return True


def check_python_packages(python_path):
    """
    Check if required Python packages are available.

    Args:
        python_path: Path to Python executable

    Returns:
        Dictionary with package availability status
    """
    print_header("Checking Python Packages")

    packages = {
        'numpy': 'NumPy (required for Python bindings)',
        'pybind11': 'pybind11 (required for Python bindings)',
        'cmake': 'CMake Python package (optional)',
    }

    results = {}
    for package, description in packages.items():
        try:
            subprocess.run(
                [python_path, '-c', f'import {package}'],
                capture_output=True,
                timeout=5,
                check=True
            )
            print_success(f"{description}: Available")
            results[package] = True
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            results[package] = False
            if package in ['numpy']:
                print_error(f"{description}: NOT FOUND (required)")
            else:
                print_warning(f"{description}: NOT FOUND")

    return results


def setup_python_environment(python_path, venv_path=None):
    """
    Set up Python environment variables for compilation.

    Args:
        python_path: Path to Python executable
        venv_path: Optional path to virtual environment

    Returns:
        Dictionary with environment variables to set
    """
    print_header("Setting Up Python Environment")

    env_vars = {}

    try:
        # Get Python info
        result = subprocess.run(
            [python_path, '-c',
             'import sys, sysconfig; '
             'print(sys.executable); '
             'print(sysconfig.get_path("include")); '
             'print(sysconfig.get_path("purelib"))'],
            capture_output=True,
            text=True,
            timeout=5,
            check=True
        )

        lines = result.stdout.strip().split('\n')
        python_exec = lines[0]
        python_include = lines[1] if len(lines) > 1 else None
        python_lib = lines[2] if len(lines) > 2 else None

        env_vars['PYTHON_EXECUTABLE'] = python_exec
        print_info(f"Python executable: {python_exec}")

        if python_include:
            env_vars['PYTHON_INCLUDE'] = python_include
            print_info(f"Python include path: {python_include}")

        if python_lib:
            env_vars['PYTHON_LIB'] = python_lib
            print_info(f"Python lib path: {python_lib}")

    except subprocess.CalledProcessError as e:
        print_warning(f"Could not determine Python paths: {e}")

    return env_vars


def create_build_directory(build_dir='build'):
    """
    Create build directory.

    Args:
        build_dir: Path to build directory

    Returns:
        Path object for build directory
    """
    print_header("Preparing Build Directory")

    build_path = Path(build_dir)
    if build_path.exists():
        print_warning(f"Build directory '{build_dir}' already exists")
        response = input("Do you want to reconfigure? (y/n): ").strip().lower()
        if response == 'y':
            print_info(f"Reconfiguring existing build directory")
        else:
            print_info("Using existing build directory")
    else:
        build_path.mkdir(parents=True)
        print_success(f"Created build directory: {build_dir}")

    return build_path


def configure_with_cmake(cmake_path, python_path, python_include, python_lib,
                         build_dir='build', gpu_backend='NONE', enable_mpi=False):
    """
    Configure the project with CMake.

    Args:
        cmake_path: Path to cmake executable
        python_path: Path to Python executable
        python_include: Python include directory
        python_lib: Python library directory
        build_dir: Build directory path
        gpu_backend: GPU backend (NONE, CUDA, HIP, SYCL)
        enable_mpi: Whether to enable MPI support

    Returns:
        True if configuration successful, False otherwise
    """
    print_header(f"Configuring with CMake (GPU Backend: {gpu_backend})")

    os.chdir(build_dir)

    # Build CMake command
    cmake_cmd = [
        cmake_path,
        '..',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON',
        f'-DMASSCONSISTENT_GPU_BACKEND={gpu_backend}',
    ]

    # Add Python configuration
    if python_path:
        cmake_cmd.append(f'-DPython3_EXECUTABLE={python_path}')

    if python_include:
        cmake_cmd.append(f'-DPython3_INCLUDE_DIR={python_include}')

    if python_lib:
        cmake_cmd.append(f'-DPython3_LIBRARY={python_lib}')

    # Add MPI if requested
    if enable_mpi:
        cmake_cmd.append('-DMASSCONSISTENT_ENABLE_MPI=ON')
        print_info("MPI support enabled")

    # Run CMake configuration
    print_info(f"CMake configuration command:")
    for part in cmake_cmd:
        print(f"  {part}")

    try:
        result = subprocess.run(cmake_cmd, check=True)
        print_success("CMake configuration completed successfully")
        return True
    except subprocess.CalledProcessError as e:
        print_error(f"CMake configuration failed: {e}")
        return False
    finally:
        os.chdir('..')


def build_project(build_dir='build', parallel_jobs=None):
    """
    Build the project using CMake.

    Args:
        build_dir: Build directory path
        parallel_jobs: Number of parallel jobs (None for auto)

    Returns:
        True if build successful, False otherwise
    """
    print_header("Building massconsistent_amr with Python Bindings")

    build_path = Path(build_dir)
    if not build_path.exists():
        print_error(f"Build directory '{build_dir}' does not exist")
        return False

    # Build command
    build_cmd = ['cmake', '--build', build_dir, '--config', 'Release']

    if parallel_jobs:
        build_cmd.extend(['--parallel', str(parallel_jobs)])
    else:
        # Use number of available CPU cores
        try:
            import multiprocessing
            num_cores = multiprocessing.cpu_count()
            build_cmd.extend(['--parallel', str(num_cores)])
            print_info(f"Using {num_cores} parallel jobs")
        except Exception:
            build_cmd.append('--parallel')

    try:
        result = subprocess.run(build_cmd, check=True)
        print_success("Build completed successfully")
        return True
    except subprocess.CalledProcessError as e:
        print_error(f"Build failed: {e}")
        return False


def run_tests(build_dir='build'):
    """
    Run regression tests.

    Args:
        build_dir: Build directory path

    Returns:
        True if all tests pass, False otherwise
    """
    print_header("Running Regression Tests")

    os.chdir(build_dir)

    try:
        result = subprocess.run(
            ['ctest', '-j', '4', '--output-on-failure'],
            check=True
        )
        print_success("All tests passed")
        return True
    except subprocess.CalledProcessError as e:
        print_warning(f"Some tests failed: {e}")
        return False
    except FileNotFoundError:
        print_warning("ctest not found, skipping tests")
        return True
    finally:
        os.chdir('..')


def setup_pythonpath(build_dir='build'):
    """
    Generate PYTHONPATH setup instructions.

    Args:
        build_dir: Build directory path

    Returns:
        Dictionary with environment setup info
    """
    print_header("Python Bindings Configuration")

    build_path = Path(build_dir).resolve()
    python_module_path = build_path / 'python'

    print_success("Python bindings have been built successfully!")
    print_info(f"Module location: {python_module_path}")

    # Generate setup instructions based on OS
    os_type = detect_os()
    setup_info = {
        'module_path': str(python_module_path),
        'os': os_type,
    }

    if os_type == 'windows':
        # Windows CMD
        cmd_setup = f"set PYTHONPATH={python_module_path};%PYTHONPATH%"
        setup_info['cmd_setup'] = cmd_setup
        print_info("For Windows CMD:")
        print(f"  {cmd_setup}")

        # Windows PowerShell
        ps_setup = f"$env:PYTHONPATH = '{python_module_path};' + $env:PYTHONPATH"
        setup_info['powershell_setup'] = ps_setup
        print_info("For Windows PowerShell:")
        print(f"  {ps_setup}")
    else:
        # Linux/macOS
        bash_setup = f"export PYTHONPATH={python_module_path}:$PYTHONPATH"
        setup_info['bash_setup'] = bash_setup
        print_info("For Linux/macOS (bash/zsh):")
        print(f"  {bash_setup}")

    # Test Python bindings
    print_header("Testing Python Bindings")
    try:
        test_cmd = [
            sys.executable, '-c',
            f'import sys; sys.path.insert(0, "{python_module_path}"); import pyWindSolver; print("pyWindSolver module loaded successfully")'
        ]
        result = subprocess.run(test_cmd, capture_output=True, text=True, timeout=10)
        if result.returncode == 0:
            print_success("Python bindings test passed!")
            print(f"  {result.stdout.strip()}")
        else:
            print_warning("Python bindings test failed")
            if result.stderr:
                print(f"  Error: {result.stderr}")
    except Exception as e:
        print_warning(f"Could not test Python bindings: {e}")

    return setup_info


def save_configuration(config_dict, config_file='install_config.json'):
    """
    Save installation configuration to file for reference.

    Args:
        config_dict: Configuration dictionary
        config_file: Output file path
    """
    with open(config_file, 'w') as f:
        json.dump(config_dict, f, indent=2)
    print_info(f"Configuration saved to {config_file}")


def main():
    """Main installation function."""
    # Parse arguments
    parser = argparse.ArgumentParser(
        description='Install massconsistent_amr with Python bindings',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Standard installation (CPU only)
  python tools/install_with_bindings.py

  # With CUDA support
  python tools/install_with_bindings.py --gpu-backend CUDA

  # With MPI and CUDA
  python tools/install_with_bindings.py --gpu-backend CUDA --enable-mpi

  # Specify Python explicitly
  python tools/install_with_bindings.py --python /usr/bin/python3.10

  # Skip tests
  python tools/install_with_bindings.py --skip-tests
        '''
    )

    parser.add_argument(
        '--python',
        help='Path to Python executable (auto-detected if not specified)'
    )
    parser.add_argument(
        '--build-dir',
        default='build',
        help='Build directory path (default: build)'
    )
    parser.add_argument(
        '--gpu-backend',
        choices=['NONE', 'CUDA', 'HIP', 'SYCL'],
        default='NONE',
        help='GPU backend to use (default: NONE)'
    )
    parser.add_argument(
        '--enable-mpi',
        action='store_true',
        help='Enable MPI support'
    )
    parser.add_argument(
        '--skip-tests',
        action='store_true',
        help='Skip running regression tests'
    )
    parser.add_argument(
        '--jobs',
        type=int,
        help='Number of parallel build jobs (default: auto)'
    )
    parser.add_argument(
        '--no-color',
        action='store_true',
        help='Disable colored output'
    )

    args = parser.parse_args()

    if args.no_color:
        Colors.disable()

    # Print welcome message
    print_header("massconsistent_amr Installation with Python Bindings")
    print_info(f"Platform: {platform.platform()}")
    print_info(f"Python: {sys.version}")

    # Step 1: Detect Python
    if args.python:
        python_path = args.python
        if not Path(python_path).exists():
            print_error(f"Specified Python path does not exist: {python_path}")
            sys.exit(1)
    else:
        python_path = find_python()
        if not python_path:
            print_error("Could not find Python. Please install Python 3.8+ or specify with --python")
            sys.exit(1)

    # Step 2: Check CMake
    cmake_path = check_cmake()
    if not cmake_path:
        print_error("CMake is required. Please install CMake 3.20+")
        sys.exit(1)

    # Step 3: Check Git and submodules
    if not check_git():
        print_warning("Git submodule initialization failed, continuing anyway...")

    # Step 4: Check Python packages
    packages = check_python_packages(python_path)
    if not packages.get('numpy'):
        print_error("NumPy is required for Python bindings. Please install it.")
        sys.exit(1)

    # Step 5: Setup Python environment
    env_vars = setup_python_environment(python_path)

    # Step 6: Create build directory
    build_path = create_build_directory(args.build_dir)

    # Step 7: Configure with CMake
    success = configure_with_cmake(
        cmake_path,
        python_path,
        env_vars.get('PYTHON_INCLUDE'),
        env_vars.get('PYTHON_LIB'),
        build_dir=args.build_dir,
        gpu_backend=args.gpu_backend,
        enable_mpi=args.enable_mpi
    )

    if not success:
        print_error("CMake configuration failed")
        sys.exit(1)

    # Step 8: Build project
    if not build_project(build_dir=args.build_dir, parallel_jobs=args.jobs):
        print_error("Build failed")
        sys.exit(1)

    # Step 9: Run tests (optional)
    if not args.skip_tests:
        run_tests(build_dir=args.build_dir)
    else:
        print_info("Skipping test execution")

    # Step 10: Setup PYTHONPATH
    setup_info = setup_pythonpath(build_dir=args.build_dir)

    # Save configuration
    config = {
        'python_path': python_path,
        'cmake_path': cmake_path,
        'build_dir': str(build_path),
        'gpu_backend': args.gpu_backend,
        'enable_mpi': args.enable_mpi,
        'platform': detect_os(),
        'setup_info': setup_info,
    }
    save_configuration(config)

    # Final summary
    print_header("Installation Summary")
    print_success("Installation completed successfully!")
    print_info(f"Build directory: {build_path}")
    print_info(f"Python bindings: {setup_info['module_path']}")
    print_info("Next steps:")
    print_info("1. Add to PYTHONPATH:")
    if setup_info['os'] == 'windows':
        if 'cmd_setup' in setup_info:
            print(f"   {setup_info['cmd_setup']}")
    else:
        if 'bash_setup' in setup_info:
            print(f"   {setup_info['bash_setup']}")
    print_info("2. Verify installation:")
    print(f"   python -c 'import sys; sys.path.insert(0, \"{setup_info['module_path']}\"); import pyWindSolver'")
    print_info("3. See install_config.json for full configuration details")


if __name__ == '__main__':
    main()
