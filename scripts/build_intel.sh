#!/bin/bash

# =============================================================================
# Build Script for Potter with Intel Compiler + oneMKL Optimization
# =============================================================================
# This script automates the build process using Intel compilers (icpx/icpc)
# and Intel oneMKL library for optimal performance on XEON processors.
#
# Usage:
#   ./build_intel.sh [options]
#
# Options:
#   clean    - Clean build directory before building
#   release  - Build in Release mode (default)
#   debug    - Build in Debug mode
#   -j N     - Use N parallel jobs (default: 40)
#
# Example:
#   ./build_intel.sh clean release -j 40
# =============================================================================

set -e  # Exit on error

# Default settings
BUILD_TYPE="Release"
CLEAN_BUILD=false
PARALLEL_JOBS=40

# Parse command line arguments
for arg in "$@"; do
    case $arg in
        clean)
            CLEAN_BUILD=true
            ;;
        release|Release)
            BUILD_TYPE="Release"
            ;;
        debug|Debug)
            BUILD_TYPE="Debug"
            ;;
        -j)
            shift
            PARALLEL_JOBS="$1"
            ;;
        -j*)
            PARALLEL_JOBS="${arg#-j}"
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Usage: $0 [clean] [release|debug] [-j N]"
            exit 1
            ;;
    esac
done

echo "============================================================================="
echo "Potter FPGA Router - Intel Compiler Build"
echo "============================================================================="

# =============================================================================
# 1. Check and load Intel oneAPI environment
# =============================================================================
echo ""
echo "[1/5] Checking Intel oneAPI environment..."

# Check if Intel compiler is available
if command -v icpx &> /dev/null; then
    echo "✓ Intel icpx compiler found: $(which icpx)"
elif command -v icpc &> /dev/null; then
    echo "✓ Intel icpc compiler found: $(which icpc)"
else
    echo "✗ Intel compiler not found in PATH"
    echo ""
    echo "Attempting to load oneAPI environment..."

    # Common oneAPI installation paths
    ONEAPI_PATHS=(
        "/opt/intel/oneapi/setvars.sh"
        "$HOME/intel/oneapi/setvars.sh"
        "/usr/local/intel/oneapi/setvars.sh"
    )

    ONEAPI_FOUND=false
    for path in "${ONEAPI_PATHS[@]}"; do
        if [ -f "$path" ]; then
            echo "Found oneAPI at: $path"
            echo "Loading environment..."
            source "$path" --force
            ONEAPI_FOUND=true
            break
        fi
    done

    if [ "$ONEAPI_FOUND" = false ]; then
        echo ""
        echo "ERROR: Intel oneAPI not found!"
        echo "Please install Intel oneAPI Base Toolkit or manually run:"
        echo "  source /path/to/oneapi/setvars.sh"
        exit 1
    fi
fi

# Verify compiler is now available
if command -v icpx &> /dev/null; then
    COMPILER="icpx"
elif command -v icpc &> /dev/null; then
    COMPILER="icpc"
else
    echo "ERROR: Intel compiler still not available after loading oneAPI"
    exit 1
fi

echo "✓ Using compiler: $COMPILER"

# =============================================================================
# 2. Display compiler and MKL information
# =============================================================================
echo ""
echo "[2/5] Compiler and Library Information:"
echo "----------------------------------------"
$COMPILER --version | head -3
echo ""

if [ -n "$MKLROOT" ]; then
    echo "✓ MKLROOT: $MKLROOT"
else
    echo "✗ WARNING: MKLROOT not set (oneMKL may not be found)"
fi

# =============================================================================
# 3. Clean build directory (if requested)
# =============================================================================
echo ""
echo "[3/5] Build preparation..."
if [ "$CLEAN_BUILD" = true ]; then
    echo "Cleaning build directory..."
    rm -rf build
    echo "✓ Clean complete"
else
    echo "Using existing build directory (use 'clean' to rebuild from scratch)"
fi

# =============================================================================
# 4. CMake configuration
# =============================================================================
echo ""
echo "[4/5] Configuring project with CMake..."
echo "Build type: $BUILD_TYPE"
echo "Parallel jobs: $PARALLEL_JOBS"

cmake -B build \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_CXX_COMPILER=$COMPILER \
    .

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: CMake configuration failed"
    exit 1
fi

echo "✓ Configuration complete"

# =============================================================================
# 5. Build the project
# =============================================================================
echo ""
echo "[5/5] Building project..."
cmake --build build --parallel $PARALLEL_JOBS

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: Build failed"
    exit 1
fi

# =============================================================================
# Build summary
# =============================================================================
echo ""
echo "============================================================================="
echo "✓ Build successful!"
echo "============================================================================="
echo "Executable: $(pwd)/build/route"
echo "Build type: $BUILD_TYPE"
echo "Compiler: $COMPILER"
echo ""
echo "To run the router:"
echo "  ./build/route -i input.phys -o output.phys -d xcvu3p.device -t 32"
echo ""
echo "For help:"
echo "  ./build/route --help"
echo "============================================================================="
