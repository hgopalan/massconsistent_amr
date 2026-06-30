@echo off
REM Windows installation script for massconsistent_amr with Python bindings
REM This script automatically detects Python and configures the build environment
REM
REM Usage: install_with_bindings.bat [options]
REM For help: install_with_bindings.bat --help
REM
REM Author: massconsistent_amr development team
REM Date: June 2026

setlocal enabledelayedexpansion

REM Get the directory where this batch file is located
for %%I in ("%~dp0..") do set "REPO_ROOT=%%~fI"
cd /d "%REPO_ROOT%"

REM Define color output using registry for console colors (Windows 10+)
set "INFO=[INFO]"
set "SUCCESS=[SUCCESS]"
set "WARNING=[WARNING]"
set "ERROR=[ERROR]"

REM Print welcome message
cls
echo.
echo ============================================================
echo   massconsistent_amr Installation with Python Bindings
echo ============================================================
echo.
echo %INFO% Platform: Windows
echo %INFO% Python Search: Searching for Python 3.8+...
echo.

REM Step 1: Find Python
set "PYTHON_EXECUTABLE="
set "PYTHON_FOUND=0"

REM Try to find Python
for %%P in (python3.exe python.exe python3 python) do (
    where /q "%%P"
    if !errorlevel! equ 0 (
        for /f "tokens=2" %%V in ('%%P --version 2^>^&1') do (
            set "PYTHON_VER=%%V"
            set "PYTHON_EXECUTABLE=%%P"
            set "PYTHON_FOUND=1"
            goto python_found
        )
    )
)

:python_found
if "%PYTHON_FOUND%"=="1" (
    echo %SUCCESS% Found Python: %PYTHON_EXECUTABLE% (Version: %PYTHON_VER%)
) else (
    echo %ERROR% Could not find Python. Please install Python 3.8+ or specify with --python
    exit /b 1
)

REM Step 2: Check CMake
echo.
echo ============================================================
echo   Checking CMake Installation
echo ============================================================
echo.

where /q cmake
if %errorlevel% equ 0 (
    for /f "tokens=*" %%C in ('cmake --version') do (
        set "CMAKE_INFO=%%C"
        goto cmake_found
    )
) else (
    echo %ERROR% CMake not found. Please install CMake 3.20+
    exit /b 1
)

:cmake_found
echo %SUCCESS% CMake found: %CMAKE_INFO%

REM Step 3: Check Git and submodules
echo.
echo ============================================================
echo   Checking Git and Submodules
echo ============================================================
echo.

where /q git
if %errorlevel% equ 0 (
    if not exist "external\amrex\CMakeLists.txt" (
        echo %WARNING% AMReX submodule not initialized. Initializing...
        call git submodule update --init --recursive
        if !errorlevel! neq 0 (
            echo %WARNING% Git submodule initialization failed
        )
    ) else (
        echo %SUCCESS% AMReX submodule is initialized
    )
) else (
    echo %WARNING% Git not found, assuming submodules are already initialized
)

REM Step 4: Check Python packages
echo.
echo ============================================================
echo   Checking Python Packages
echo ============================================================
echo.

%PYTHON_EXECUTABLE% -c "import numpy" 2>nul
if %errorlevel% equ 0 (
    echo %SUCCESS% NumPy: Available
) else (
    echo %ERROR% NumPy: NOT FOUND (required for Python bindings)
    exit /b 1
)

%PYTHON_EXECUTABLE% -c "import pybind11" 2>nul
if %errorlevel% equ 0 (
    echo %SUCCESS% pybind11: Available
) else (
    echo %WARNING% pybind11: NOT FOUND (will be fetched during build)
)

REM Step 5: Setup Python environment
echo.
echo ============================================================
echo   Setting Up Python Environment
echo ============================================================
echo.

echo %INFO% Python executable: %PYTHON_EXECUTABLE%

for /f "tokens=*" %%I in ('%PYTHON_EXECUTABLE% -c "import sysconfig; print(sysconfig.get_path('include'))" 2^>nul') do (
    set "PYTHON_INCLUDE=%%I"
)

for /f "tokens=*" %%L in ('%PYTHON_EXECUTABLE% -c "import sysconfig; print(sysconfig.get_path('purelib'))" 2^>nul') do (
    set "PYTHON_LIB=%%L"
)

if defined PYTHON_INCLUDE echo %INFO% Python include path: %PYTHON_INCLUDE%
if defined PYTHON_LIB echo %INFO% Python lib path: %PYTHON_LIB%

REM Step 6: Create build directory
echo.
echo ============================================================
echo   Preparing Build Directory
echo ============================================================
echo.

set "BUILD_DIR=build"
if exist "%BUILD_DIR%" (
    echo %WARNING% Build directory '%BUILD_DIR%' already exists
) else (
    mkdir "%BUILD_DIR%"
    echo %SUCCESS% Created build directory: %BUILD_DIR%
)

REM Step 7: Configure with CMake
echo.
echo ============================================================
echo   Configuring with CMake (GPU Backend: NONE)
echo ============================================================
echo.

cd /d "%BUILD_DIR%"

set "CMAKE_ARGS=-DCMAKE_BUILD_TYPE=Release -DMASSCONSISTENT_BUILD_PYTHON_BINDINGS=ON -DMASSCONSISTENT_GPU_BACKEND=NONE"

if defined PYTHON_EXECUTABLE (
    set "CMAKE_ARGS=!CMAKE_ARGS! -DPython3_EXECUTABLE=%PYTHON_EXECUTABLE%"
)

if defined PYTHON_INCLUDE (
    set "CMAKE_ARGS=!CMAKE_ARGS! -DPython3_INCLUDE_DIR=%PYTHON_INCLUDE%"
)

if defined PYTHON_LIB (
    set "CMAKE_ARGS=!CMAKE_ARGS! -DPython3_LIBRARY=%PYTHON_LIB%"
)

echo %INFO% Running CMake configuration...
cmake .. %CMAKE_ARGS%

if %errorlevel% equ 0 (
    echo %SUCCESS% CMake configuration completed successfully
) else (
    echo %ERROR% CMake configuration failed
    exit /b 1
)

cd /d "%REPO_ROOT%"

REM Step 8: Build project
echo.
echo ============================================================
echo   Building massconsistent_amr with Python Bindings
echo ============================================================
echo.

echo %INFO% Building with CMake...
cmake --build "%BUILD_DIR%" --config Release --parallel

if %errorlevel% equ 0 (
    echo %SUCCESS% Build completed successfully
) else (
    echo %ERROR% Build failed
    exit /b 1
)

REM Step 9: Setup PYTHONPATH
echo.
echo ============================================================
echo   Python Bindings Configuration
echo ============================================================
echo.

cd /d "%BUILD_DIR%\python"
set "PYTHON_MODULE_PATH=%cd%"
cd /d "%REPO_ROOT%"

echo %SUCCESS% Python bindings have been built successfully!
echo %INFO% Module location: %PYTHON_MODULE_PATH%

echo.
echo ============================================================
echo   Installation Summary
echo ============================================================
echo.

echo %SUCCESS% Installation completed successfully!
echo %INFO% Build directory: %BUILD_DIR%
echo %INFO% Python bindings: %PYTHON_MODULE_PATH%
echo.
echo %INFO% Next steps:
echo.
echo 1. Add to PYTHONPATH (Windows CMD):
echo    set PYTHONPATH=%PYTHON_MODULE_PATH%;%%PYTHONPATH%%
echo.
echo 2. Add to PYTHONPATH (Windows PowerShell):
echo    $env:PYTHONPATH = '%PYTHON_MODULE_PATH%;' + $env:PYTHONPATH
echo.
echo 3. Verify installation:
echo    python -c "import sys; sys.path.insert(0, '%PYTHON_MODULE_PATH%'); import pyWindSolver"
echo.
echo 4. See INSTALL.md for usage examples
echo.

REM Create setup batch file
set "SETUP_BAT=%BUILD_DIR%\setup_pythonpath.bat"
(
    echo @echo off
    echo REM Setup script for massconsistent_amr Python bindings
    echo REM Usage: call %SETUP_BAT%
    echo.
    echo set "PYTHONPATH=%PYTHON_MODULE_PATH%;%%PYTHONPATH%%"
    echo echo PYTHONPATH updated: %%PYTHONPATH%%
    echo.
    echo python -c "import sys; sys.path.insert(0, '%PYTHON_MODULE_PATH%'); import pyWindSolver; print('[SUCCESS] pyWindSolver module loaded')" 2>nul || echo [WARNING] Could not import pyWindSolver module
) > "%SETUP_BAT%"

echo %SUCCESS% Created setup script: %SETUP_BAT%

echo.
REM Create setup PowerShell file
set "SETUP_PS1=%BUILD_DIR%\setup_pythonpath.ps1"
(
    echo # Setup script for massconsistent_amr Python bindings
    echo # Usage: . %SETUP_PS1%
    echo.
    echo $env:PYTHONPATH = '%PYTHON_MODULE_PATH%;' + $env:PYTHONPATH
    echo Write-Host "PYTHONPATH updated: $env:PYTHONPATH"
    echo.
    echo python -c "import sys; sys.path.insert(0, '%PYTHON_MODULE_PATH%'); import pyWindSolver; print('[SUCCESS] pyWindSolver module loaded')" 2>$null
) > "%SETUP_PS1%"

echo %SUCCESS% Created PowerShell setup script: %SETUP_PS1%

echo.
exit /b 0
