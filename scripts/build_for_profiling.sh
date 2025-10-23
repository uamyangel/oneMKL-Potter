#!/bin/bash

# =============================================================================
# Build Script for Potter with Profiling Mode (vTune Analysis)
# =============================================================================
# This script builds Potter with NO optimizations and full debug symbols
# to make performance bottlenecks clearly visible in Intel vTune Profiler.
#
# Usage:
#   ./build_for_profiling.sh [options]
#
# Options:
#   clean         - Clean build directory before building (recommended)
#   --target-only - Only build 'route' target (skip capnproto tests)
#   -j N          - Use N parallel jobs (default: 40)
#
# Build Configuration:
#   - Compiler: Intel icpx (required for vTune integration)
#   - Build Type: Profiling (CMAKE_BUILD_TYPE=Profiling)
#   - Optimization: -O0 (no optimization)
#   - Debug Info: -g -fno-inline -fno-omit-frame-pointer
#   - oneMKL: Enabled (but also built without optimization)
#
# Example:
#   ./build_for_profiling.sh clean -j 40
#   ./build_for_profiling.sh clean --target-only
# =============================================================================

set -e  # Exit on error

# Default settings
CLEAN_BUILD=true  # Always recommend clean build for profiling
PARALLEL_JOBS=40
TARGET_ONLY=true  # Default to target-only to avoid third-party test issues

# Parse command line arguments
for arg in "$@"; do
    case $arg in
        clean)
            CLEAN_BUILD=true
            ;;
        --target-only)
            TARGET_ONLY=true
            ;;
        --all)
            TARGET_ONLY=false
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
            echo "Usage: $0 [clean] [--target-only|--all] [-j N]"
            exit 1
            ;;
    esac
done

echo "============================================================================="
echo "Potter FPGA Router - Profiling Build (for vTune Analysis)"
echo "============================================================================="
echo ""
echo "⚠️  WARNING: This build uses -O0 (no optimization) for profiling purposes."
echo "    Do NOT use this build for performance benchmarks or production."
echo "    Use this build ONLY for vTune performance analysis."
echo ""

# =============================================================================
# 1. Check and load Intel oneAPI environment
# =============================================================================
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
        "/xrepo/App/oneAPI/setvars.sh"
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

# Check if vTune is available (not required for build, but good to verify)
if command -v vtune &> /dev/null; then
    echo "✓ Intel vTune found: $(which vtune)"
else
    echo "⚠️  Warning: Intel vTune not found in PATH"
    echo "   vTune is required for profiling analysis (not for building)"
fi

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
# 3. Clean build directory (recommended for profiling)
# =============================================================================
echo ""
echo "[3/5] Build preparation..."
if [ "$CLEAN_BUILD" = true ]; then
    echo "Cleaning build directory..."
    rm -rf build
    echo "✓ Clean complete"
else
    echo "Using existing build directory"
fi

# =============================================================================
# 4. CMake configuration (Profiling mode)
# =============================================================================
echo ""
echo "[4/5] Configuring project with CMake (Profiling mode)..."
echo "Build type: Profiling (no optimization, full debug info)"
echo "Parallel jobs: $PARALLEL_JOBS"
if [ "$TARGET_ONLY" = true ]; then
    echo "Build target: route only (skipping third-party tests)"
else
    echo "Build target: all"
fi

cmake -B build \
    -DCMAKE_BUILD_TYPE=Profiling \
    -DCMAKE_CXX_COMPILER=$COMPILER \
    .

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: CMake configuration failed"
    exit 1
fi

echo "✓ Configuration complete"

# Display the actual compiler flags being used
echo ""
echo "Compiler flags for profiling:"
cd build
cmake -LA -N . | grep CMAKE_CXX_FLAGS_PROFILING
cd ..

# =============================================================================
# 5. Build the project
# =============================================================================
echo ""
echo "[5/5] Building project (this may take longer due to -O0)..."

if [ "$TARGET_ONLY" = true ]; then
    # Build only the route target
    cmake --build build --target route --parallel $PARALLEL_JOBS
    BUILD_RESULT=$?
else
    # Try to build all targets
    cmake --build build --parallel $PARALLEL_JOBS
    BUILD_RESULT=$?

    # If build failed but route was built successfully, inform user
    if [ $BUILD_RESULT -ne 0 ]; then
        if [ -f "build/route" ]; then
            echo ""
            echo "============================================================================="
            echo "⚠️  Warning: Full build failed, but 'route' executable was built successfully"
            echo "============================================================================="
            echo ""
            echo "This typically means third-party tests failed, but the main program is OK."
            echo "The 'route' executable is ready for vTune analysis."
            BUILD_RESULT=0  # Override exit code since route is available
        else
            echo ""
            echo "ERROR: Build failed and route executable was not created"
            exit 1
        fi
    fi
fi

if [ $BUILD_RESULT -ne 0 ]; then
    echo ""
    echo "ERROR: Build failed"
    exit 1
fi

# =============================================================================
# Build summary
# =============================================================================
echo ""
echo "============================================================================="
echo "✓ Profiling build successful!"
echo "============================================================================="
echo "Executable: $(pwd)/build/route"
echo "Build type: Profiling (-O0, full debug symbols)"
echo "Compiler: $COMPILER"
echo ""

# Check executable exists and is executable
if [ -x "build/route" ]; then
    # Get file size
    FILESIZE=$(ls -lh build/route | awk '{print $5}')
    echo "Executable size: $FILESIZE (larger than Release due to debug info)"
    echo ""

    # Check if executable has debug symbols
    if file build/route | grep -q "not stripped"; then
        echo "✓ Debug symbols present (good for vTune)"
    else
        echo "⚠️  Warning: Debug symbols may be missing"
    fi
fi

echo ""
echo "Next steps:"
echo "  1. Run vTune analysis:"
echo "     ./scripts/vtune_hotspots_analysis.sh"
echo ""
echo "  2. Or manually run vTune:"
echo "     vtune -collect hotspots -result-dir vtune_results -- \\"
echo "         ./build/route -i benchmarks/boom_med_pb_unrouted.phys \\"
echo "                       -o benchmarks/boom_med_pb_routed.phys -t 32"
echo ""
echo "  3. Analyze results:"
echo "     ./scripts/analyze_vtune_top3.sh"
echo ""
echo "============================================================================="
echo ""
echo "⚠️  REMINDER: This build is for PROFILING ONLY (not for benchmarks)"
echo "    It will run much slower than the optimized Release build."
echo "============================================================================="
