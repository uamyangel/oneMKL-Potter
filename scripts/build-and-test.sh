#!/bin/bash
################################################################################
# Build and Test Script for oneMKL-optimized Potter
################################################################################
# Description: Build Potter with oneMKL and run benchmark test
# Usage: ./build-and-test.sh [benchmark_name]
################################################################################

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
ONEAPI_DIR="/opt/intel/oneapi"
BENCHMARK="${1:-boom_med_pb}"
BENCHMARK_FILE="${PROJECT_ROOT}/benchmarks/${BENCHMARK}_unrouted.phys"
OUTPUT_FILE="${PROJECT_ROOT}/benchmarks/${BENCHMARK}_routed_onemkl.phys"
DEVICE_FILE="${PROJECT_ROOT}/xcvu3p.device"
NUM_THREADS="${NUM_THREADS:-32}"
RESULT_FILE="${PROJECT_ROOT}/with-onemkl-results.md"

################################################################################
# Step 1: Load oneAPI environment
################################################################################

echo "=== Step 1: Load oneAPI Environment ==="
echo ""

if [ ! -f "${ONEAPI_DIR}/setvars.sh" ]; then
    echo "ERROR: oneAPI not found at ${ONEAPI_DIR}"
    echo "Please install it first: sudo ./scripts/install-oneapi.sh"
    exit 1
fi

echo "Loading oneAPI environment..."
source "${ONEAPI_DIR}/setvars.sh" > /dev/null 2>&1

if [ -z "$MKLROOT" ]; then
    echo "ERROR: Failed to load oneAPI environment (MKLROOT not set)"
    exit 1
fi

echo "✓ oneAPI environment loaded"
echo "✓ MKLROOT: ${MKLROOT}"
echo ""

################################################################################
# Step 2: Build Potter with oneMKL
################################################################################

echo "=== Step 2: Build Potter with oneMKL ==="
echo ""

cd "${PROJECT_ROOT}"

# Clean old build
if [ -d "${BUILD_DIR}" ]; then
    echo "Cleaning old build..."
    rm -rf "${BUILD_DIR}"
fi

# Configure and build
echo "Configuring CMake (Release mode)..."
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release . 2>&1 | grep -E "(oneMKL|Found MKLROOT|Building)" || true

echo ""
echo "Building Potter (using ${NUM_THREADS} threads)..."
cmake --build "${BUILD_DIR}" --parallel "${NUM_THREADS}"

# Verify
if [ ! -f "${BUILD_DIR}/route" ]; then
    echo "ERROR: Build failed"
    exit 1
fi

echo "✓ Build completed: ${BUILD_DIR}/route"
ls -lh "${BUILD_DIR}/route"
echo ""

################################################################################
# Step 3: Run benchmark test
################################################################################

echo "=== Step 3: Run Benchmark Test ==="
echo ""

# Check benchmark file
if [ ! -f "${BENCHMARK_FILE}" ]; then
    echo "ERROR: Benchmark file not found: ${BENCHMARK_FILE}"
    echo "Available benchmarks:"
    ls -1 "${PROJECT_ROOT}/benchmarks/"*_unrouted.phys 2>/dev/null | xargs -n1 basename || true
    exit 1
fi

echo "Benchmark: ${BENCHMARK}"
echo "Input:     ${BENCHMARK_FILE}"
echo "Output:    ${OUTPUT_FILE}"
echo "Device:    ${DEVICE_FILE}"
echo "Threads:   ${NUM_THREADS}"
echo ""
echo "Starting routing process..."
echo "=================================================================================="

# Run Potter and save output
"${BUILD_DIR}/route" \
    -i "${BENCHMARK_FILE}" \
    -o "${OUTPUT_FILE}" \
    -d "${DEVICE_FILE}" \
    -t "${NUM_THREADS}" \
    2>&1 | tee /tmp/potter_onemkl_output.log

echo "=================================================================================="
echo ""

# Save results
if [ -f /tmp/potter_onemkl_output.log ]; then
    echo "Saving results to ${RESULT_FILE}..."
    cat > "${RESULT_FILE}" <<EOF
xaccel@xaccel:/xrepo/App/oneMKL-Potter$ ./build/route -i benchmarks/${BENCHMARK}_unrouted.phys -o benchmarks/${BENCHMARK}_routed.phys
EOF
    cat /tmp/potter_onemkl_output.log >> "${RESULT_FILE}"
    echo "✓ Results saved to ${RESULT_FILE}"
fi

echo ""
echo "=== Build and Test Complete ==="
echo ""
echo "Next steps:"
echo "  1. Review test results above"
echo "  2. Compare with original Potter results manually"
echo "  3. Check detailed log: ${RESULT_FILE}"
echo "  4. See ONEMKL-OPTIMIZATION.md for more information"
