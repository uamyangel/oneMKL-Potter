#!/bin/bash
################################################################################
# Build and Test Script for oneMKL-optimized Potter
################################################################################
# Description: Build Potter with oneMKL optimization and run performance tests
# Author: oneMKL Optimization Team
# Usage: ./build-and-test.sh [benchmark_name]
################################################################################

set -e  # Exit on error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
ONEAPI_DIR="/opt/intel/oneapi"
BENCHMARK="${1:-boom_med_pb}"
BENCHMARK_FILE="${PROJECT_ROOT}/benchmarks/${BENCHMARK}_unrouted.phys"
OUTPUT_FILE="${PROJECT_ROOT}/benchmarks/${BENCHMARK}_routed_onemkl.phys"
DEVICE_FILE="${PROJECT_ROOT}/xcvu3p.device"
NUM_THREADS="${NUM_THREADS:-$nproc}"
RESULT_FILE="${PROJECT_ROOT}/with-onemkl-results.md"
ORIGINAL_RESULT_FILE="${PROJECT_ROOT}/without-onemkl-results.md"

################################################################################
# Helper functions
################################################################################

print_header() {
    echo
    echo -e "${BLUE}════════════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}════════════════════════════════════════════════════════════════════════════════${NC}"
    echo
}

print_subheader() {
    echo
    echo -e "${CYAN}────────────────────────────────────────────────────────────────────────────────${NC}"
    echo -e "${CYAN}  $1${NC}"
    echo -e "${CYAN}────────────────────────────────────────────────────────────────────────────────${NC}"
    echo
}

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

################################################################################
# Environment setup
################################################################################

setup_oneapi_env() {
    print_subheader "Setting up oneAPI Environment"

    if [ ! -f "${ONEAPI_DIR}/setvars.sh" ]; then
        print_error "oneAPI not found at ${ONEAPI_DIR}"
        print_error "Please install Intel oneAPI Base Toolkit first:"
        print_error "  sudo ./scripts/install-oneapi.sh"
        exit 1
    fi

    print_info "Loading oneAPI environment from ${ONEAPI_DIR}"
    source "${ONEAPI_DIR}/setvars.sh" > /dev/null 2>&1

    if [ -z "$MKLROOT" ]; then
        print_error "Failed to load oneAPI environment (MKLROOT not set)"
        exit 1
    fi

    print_success "oneAPI environment loaded"
    print_info "MKLROOT: ${MKLROOT}"

    # Verify MKL libraries
    if [ -d "${MKLROOT}/lib/intel64" ]; then
        MKL_LIB_DIR="${MKLROOT}/lib/intel64"
    elif [ -d "${MKLROOT}/lib" ]; then
        MKL_LIB_DIR="${MKLROOT}/lib"
    else
        print_error "MKL library directory not found"
        exit 1
    fi

    print_info "MKL library directory: ${MKL_LIB_DIR}"

    # Check key libraries
    local libs=("libmkl_core" "libmkl_intel_lp64" "libmkl_gnu_thread")
    for lib in "${libs[@]}"; do
        if ls "${MKL_LIB_DIR}/${lib}".* > /dev/null 2>&1; then
            print_info "✓ ${lib} found"
        else
            print_warning "✗ ${lib} not found"
        fi
    done
}

################################################################################
# Build Potter with oneMKL
################################################################################

build_potter() {
    print_subheader "Building Potter with oneMKL Optimization"

    cd "${PROJECT_ROOT}"

    # Clean old build
    if [ -d "${BUILD_DIR}" ]; then
        print_info "Cleaning old build directory..."
        rm -rf "${BUILD_DIR}"
    fi

    # Configure CMake
    print_info "Configuring CMake (Release mode)..."
    cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release .

    # Check if oneMKL is enabled
    if grep -q "oneMKL optimization ENABLED" "${BUILD_DIR}/CMakeCache.txt" 2>/dev/null || \
       cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release . 2>&1 | grep -q "oneMKL optimization ENABLED"; then
        print_success "oneMKL optimization is ENABLED"
    else
        print_error "oneMKL optimization is NOT enabled"
        print_error "Check if MKLROOT is set correctly: ${MKLROOT}"
        exit 1
    fi

    # Build
    print_info "Building Potter (using ${NUM_THREADS} parallel jobs)..."
    cmake --build "${BUILD_DIR}" --parallel "${NUM_THREADS}"

    if [ ! -f "${BUILD_DIR}/route" ]; then
        print_error "Build failed: executable not found at ${BUILD_DIR}/route"
        exit 1
    fi

    print_success "Build completed successfully"
    print_info "Executable: ${BUILD_DIR}/route"
    ls -lh "${BUILD_DIR}/route"
}

################################################################################
# Run benchmark test
################################################################################

run_test() {
    print_subheader "Running Benchmark Test"

    # Check benchmark file exists
    if [ ! -f "${BENCHMARK_FILE}" ]; then
        print_error "Benchmark file not found: ${BENCHMARK_FILE}"
        print_info "Available benchmarks:"
        ls -1 "${PROJECT_ROOT}/benchmarks/"*_unrouted.phys 2>/dev/null | xargs -n1 basename || echo "  (none found)"
        exit 1
    fi

    print_info "Benchmark: ${BENCHMARK}"
    print_info "Input: ${BENCHMARK_FILE}"
    print_info "Output: ${OUTPUT_FILE}"
    print_info "Device: ${DEVICE_FILE}"
    print_info "Threads: ${NUM_THREADS}"

    # Run Potter
    print_info "Starting routing process..."
    echo

    local start_time=$(date +%s)

    "${BUILD_DIR}/route" \
        -i "${BENCHMARK_FILE}" \
        -o "${OUTPUT_FILE}" \
        -d "${DEVICE_FILE}" \
        -t "${NUM_THREADS}" \
        2>&1 | tee /tmp/potter_onemkl_output.log

    local end_time=$(date +%s)
    local elapsed=$((end_time - start_time))

    echo
    print_success "Routing completed in ${elapsed} seconds"

    # Save results to markdown
    save_results
}

################################################################################
# Save results to markdown
################################################################################

save_results() {
    print_subheader "Saving Results"

    if [ ! -f /tmp/potter_onemkl_output.log ]; then
        print_error "Output log not found"
        return
    fi

    print_info "Saving results to ${RESULT_FILE}"

    cat > "${RESULT_FILE}" <<'EOF'
xaccel@xaccel:/xrepo/App/oneMKL-Potter$ ./build/route -i benchmarks/boom_med_pb_unrouted.phys -o benchmarks/boom_med_pb_routed.phys
EOF

    cat /tmp/potter_onemkl_output.log >> "${RESULT_FILE}"

    print_success "Results saved to ${RESULT_FILE}"
}

################################################################################
# Compare results with original Potter
################################################################################

compare_results() {
    print_header "Performance Comparison Report"

    if [ ! -f "${ORIGINAL_RESULT_FILE}" ]; then
        print_warning "Original Potter results not found at ${ORIGINAL_RESULT_FILE}"
        print_warning "Skipping comparison"
        return
    fi

    if [ ! -f "${RESULT_FILE}" ]; then
        print_warning "oneMKL results not found at ${RESULT_FILE}"
        print_warning "Skipping comparison"
        return
    fi

    print_info "Comparing results..."
    echo

    # Extract key metrics
    local original_total=$(grep "Total route time:" "${ORIGINAL_RESULT_FILE}" | awk '{print $4}' | head -1)
    local onemkl_total=$(grep "Total route time:" "${RESULT_FILE}" | awk '{print $4}' | head -1)

    local original_indirect=$(grep "Route indirect time:" "${ORIGINAL_RESULT_FILE}" | awk '{print $4}' | head -1)
    local onemkl_indirect=$(grep "Route indirect time:" "${RESULT_FILE}" | awk '{print $4}' | head -1)

    local original_write=$(grep "Write time:" "${ORIGINAL_RESULT_FILE}" | awk '{print $3}' | head -1)
    local onemkl_write=$(grep "Write time:" "${RESULT_FILE}" | awk '{print $3}' | head -1)

    local original_mem=$(grep "memory_peak:" "${ORIGINAL_RESULT_FILE}" | awk '{print $3}' | head -1)
    local onemkl_mem=$(grep "memory_peak:" "${RESULT_FILE}" | awk '{print $3}' | head -1)

    # Calculate improvements
    local total_improvement=""
    local indirect_improvement=""
    local mem_diff=""

    if [ -n "$original_total" ] && [ -n "$onemkl_total" ]; then
        total_improvement=$(awk "BEGIN {printf \"%.2f%%\", ($original_total - $onemkl_total) / $original_total * 100}")
    fi

    if [ -n "$original_indirect" ] && [ -n "$onemkl_indirect" ]; then
        indirect_improvement=$(awk "BEGIN {printf \"%.2f%%\", ($original_indirect - $onemkl_indirect) / $original_indirect * 100}")
    fi

    if [ -n "$original_mem" ] && [ -n "$onemkl_mem" ]; then
        mem_diff=$(awk "BEGIN {printf \"%.2f%%\", ($onemkl_mem - $original_mem) / $original_mem * 100}")
    fi

    # Print comparison table
    echo -e "${CYAN}Test Case:${NC} ${BENCHMARK}_unrouted.phys"
    echo -e "${CYAN}Device:${NC} xcvu3p.device"
    echo -e "${CYAN}Threads:${NC} ${NUM_THREADS}"
    echo

    printf "${CYAN}┌────────────────────────────────┬─────────────┬──────────────┬─────────────┐${NC}\n"
    printf "${CYAN}│${NC} %-30s ${CYAN}│${NC} %-11s ${CYAN}│${NC} %-12s ${CYAN}│${NC} %-11s ${CYAN}│${NC}\n" "Metric" "Original" "oneMKL" "Improvement"
    printf "${CYAN}├────────────────────────────────┼─────────────┼──────────────┼─────────────┤${NC}\n"

    if [ -n "$original_total" ] && [ -n "$onemkl_total" ]; then
        printf "${CYAN}│${NC} %-30s ${CYAN}│${NC} %10ss ${CYAN}│${NC} %11ss ${CYAN}│${NC} ${GREEN}%10s${NC} ${CYAN}│${NC}\n" \
            "Total Route Time" "$original_total" "$onemkl_total" "$total_improvement"
    fi

    if [ -n "$original_indirect" ] && [ -n "$onemkl_indirect" ]; then
        printf "${CYAN}│${NC} %-30s ${CYAN}│${NC} %10ss ${CYAN}│${NC} %11ss ${CYAN}│${NC} ${GREEN}%10s${NC} ${CYAN}│${NC}\n" \
            "Indirect Route Time" "$original_indirect" "$onemkl_indirect" "$indirect_improvement"
    fi

    if [ -n "$original_write" ] && [ -n "$onemkl_write" ]; then
        local write_change=$(awk "BEGIN {printf \"%.2f%%\", ($onemkl_write - $original_write) / $original_write * 100}")
        printf "${CYAN}│${NC} %-30s ${CYAN}│${NC} %10ss ${CYAN}│${NC} %11ss ${CYAN}│${NC} ${YELLOW}%10s${NC} ${CYAN}│${NC}\n" \
            "Write Time" "$original_write" "$onemkl_write" "$write_change"
    fi

    if [ -n "$original_mem" ] && [ -n "$onemkl_mem" ]; then
        local original_mem_gb=$(awk "BEGIN {printf \"%.2f\", $original_mem / 1024 / 1024}")
        local onemkl_mem_gb=$(awk "BEGIN {printf \"%.2f\", $onemkl_mem / 1024 / 1024}")
        printf "${CYAN}│${NC} %-30s ${CYAN}│${NC} %9s GB ${CYAN}│${NC} %10s GB ${CYAN}│${NC} ${YELLOW}%10s${NC} ${CYAN}│${NC}\n" \
            "Memory Peak" "$original_mem_gb" "$onemkl_mem_gb" "$mem_diff"
    fi

    printf "${CYAN}└────────────────────────────────┴─────────────┴──────────────┴─────────────┘${NC}\n"
    echo

    # Print summary
    if [ -n "$total_improvement" ]; then
        local improvement_value=$(echo "$total_improvement" | sed 's/%//')
        if awk "BEGIN {exit !($improvement_value > 0)}"; then
            echo -e "${GREEN}✅ Core routing performance improved by ${total_improvement}${NC}"
        elif awk "BEGIN {exit !($improvement_value < -1)}"; then
            echo -e "${RED}❌ Performance regressed by ${total_improvement}${NC}"
        else
            echo -e "${YELLOW}⚠️  Performance change is negligible (${total_improvement})${NC}"
        fi
    fi

    echo
    print_info "Detailed results:"
    print_info "  - oneMKL version: ${RESULT_FILE}"
    print_info "  - Original version: ${ORIGINAL_RESULT_FILE}"
}

################################################################################
# Main execution
################################################################################

main() {
    print_header "Build and Test oneMKL-optimized Potter"

    print_info "Project root: ${PROJECT_ROOT}"
    print_info "Build directory: ${BUILD_DIR}"
    echo

    # Step 1: Setup environment
    setup_oneapi_env

    # Step 2: Build Potter
    build_potter

    # Step 3: Run test
    run_test

    # Step 4: Compare results
    compare_results

    # Final message
    print_header "Build and Test Complete"

    print_success "All tasks completed successfully!"
    echo
    print_info "Next steps:"
    print_info "  1. Review performance comparison above"
    print_info "  2. Check detailed logs:"
    print_info "     - ${RESULT_FILE}"
    print_info "     - ${ORIGINAL_RESULT_FILE}"
    print_info "  3. Read optimization guide: ONEMKL-OPTIMIZATION.md"
    echo
}

# Run main function
main
