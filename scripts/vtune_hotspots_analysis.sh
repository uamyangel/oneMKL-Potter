#!/bin/bash

# =============================================================================
# Intel vTune Hotspots Analysis Script for Potter Router
# =============================================================================
# This script runs Intel vTune Profiler in Hotspots mode to identify
# performance bottlenecks (functions/lines consuming the most CPU time).
#
# Prerequisites:
#   1. Potter must be built with Profiling mode (run ./build_for_profiling.sh)
#   2. Intel oneAPI environment must be loaded (includes vTune)
#   3. Sufficient disk space for results (~100-500MB)
#
# Usage:
#   ./vtune_hotspots_analysis.sh [benchmark] [threads]
#
# Arguments:
#   benchmark  - Input benchmark file (default: boom_med_pb_unrouted.phys)
#   threads    - Number of threads (default: 32)
#
# Examples:
#   ./vtune_hotspots_analysis.sh
#   ./vtune_hotspots_analysis.sh boom_med_pb_unrouted.phys 64
#   ./vtune_hotspots_analysis.sh logicnets_jscl_unrouted.phys 32
#
# Output:
#   - Results directory: ./vtune_results/
#   - Summary file: ./vtune_analysis_summary.txt
# =============================================================================

set -e  # Exit on error

# Default settings
BENCHMARK="${1:-boom_med_pb_unrouted.phys}"
THREADS="${2:-32}"
RESULT_DIR="vtune_results"
BENCHMARK_DIR="benchmarks"
OUTPUT_FILE="run/vtune_profiling_output.phys"

# Ensure output directory exists
mkdir -p run

echo "============================================================================="
echo "Intel vTune Profiler - Hotspots Analysis"
echo "============================================================================="
echo ""
echo "Configuration:"
echo "  Benchmark: $BENCHMARK"
echo "  Threads: $THREADS"
echo "  Results directory: $RESULT_DIR"
echo ""

# =============================================================================
# 1. Environment checks
# =============================================================================
echo "[1/5] Checking environment..."

# Check if oneAPI is loaded
if [ -z "$ONEAPI_ROOT" ]; then
    echo "⚠️  Warning: ONEAPI_ROOT not set, attempting to load oneAPI..."

    ONEAPI_PATHS=(
        "/xrepo/App/oneAPI/setvars.sh"
        "$HOME/intel/oneapi/setvars.sh"
        "/usr/local/intel/oneapi/setvars.sh"
    )

    ONEAPI_FOUND=false
    for path in "${ONEAPI_PATHS[@]}"; do
        if [ -f "$path" ]; then
            echo "Loading oneAPI from: $path"
            source "$path" --force
            ONEAPI_FOUND=true
            break
        fi
    done

    if [ "$ONEAPI_FOUND" = false ]; then
        echo ""
        echo "ERROR: Intel oneAPI not found!"
        echo "Please run: source /xrepo/App/oneAPI/setvars.sh"
        exit 1
    fi
fi

# Check if vTune is available
if ! command -v vtune &> /dev/null; then
    echo "ERROR: Intel vTune not found in PATH"
    echo "Please ensure Intel oneAPI Base Toolkit is installed and environment is loaded."
    echo "Run: source /xrepo/App/oneAPI/setvars.sh"
    exit 1
fi

echo "✓ Intel vTune found: $(which vtune)"

# Get vTune version
VTUNE_VERSION=$(vtune --version 2>&1 | head -1)
echo "✓ vTune version: $VTUNE_VERSION"

# =============================================================================
# Configure system for vTune profiling
# =============================================================================
echo ""
echo "Configuring system for vTune profiling..."

# Check current perf_event_paranoid setting
CURRENT_PARANOID=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo "unknown")
echo "Current perf_event_paranoid: $CURRENT_PARANOID"

if [ "$CURRENT_PARANOID" != "unknown" ] && [ "$CURRENT_PARANOID" -gt 1 ]; then
    echo "⚠️  perf_event_paranoid is too restrictive ($CURRENT_PARANOID > 1)"
    echo "   Attempting to set it to 1 (requires sudo)..."
    echo "   This enables kernel-level performance monitoring"
    
    if sudo sh -c 'echo 1 > /proc/sys/kernel/perf_event_paranoid' 2>/dev/null; then
        echo "✓ Successfully set perf_event_paranoid to 1"
    else
        echo "⚠️  Failed to set perf_event_paranoid (continuing with current value)"
        echo "   You may need to run: sudo sh -c 'echo 1 > /proc/sys/kernel/perf_event_paranoid'"
    fi
fi

# Check and configure kptr_restrict for better kernel symbol resolution
CURRENT_KPTR=$(cat /proc/sys/kernel/kptr_restrict 2>/dev/null || echo "unknown")
echo "Current kptr_restrict: $CURRENT_KPTR"

if [ "$CURRENT_KPTR" != "0" ] && [ "$CURRENT_KPTR" != "unknown" ]; then
    echo "⚠️  kptr_restrict restricts kernel symbol access"
    echo "   Attempting to set it to 0 for better profiling results..."
    
    if sudo sh -c 'echo 0 > /proc/sys/kernel/kptr_restrict' 2>/dev/null; then
        echo "✓ Successfully set kptr_restrict to 0"
    else
        echo "⚠️  Failed to set kptr_restrict (kernel symbols may not be fully resolved)"
    fi
fi

# Set stack size limit (required by vTune)
echo ""
echo "Configuring stack size limit..."
CURRENT_STACK=$(ulimit -s)
echo "Current stack size: $CURRENT_STACK KB"

# vTune requires finite stack size in driverless mode (not unlimited)
# Set to 8192 KB (8 MB) which is sufficient for most applications
if [ "$CURRENT_STACK" = "unlimited" ]; then
    echo "Setting stack size to 8192 KB (8 MB) for vTune compatibility..."
    ulimit -s 8192
    echo "✓ Stack size set to 8192 KB"
else
    echo "✓ Stack size is finite ($CURRENT_STACK KB), suitable for vTune"
fi

# =============================================================================
# 2. Check executable
# =============================================================================
echo ""
echo "[2/6] Checking executable..."

if [ ! -f "build/route" ]; then
    echo "ERROR: build/route not found"
    echo "Please build Potter with profiling mode first:"
    echo "  ./scripts/build_for_profiling.sh"
    exit 1
fi

echo "✓ Executable found: build/route"

# Check if executable has debug symbols
if file build/route | grep -q "not stripped"; then
    echo "✓ Debug symbols present (good for source-level analysis)"
else
    echo "⚠️  Warning: Debug symbols may be missing"
    echo "   Consider rebuilding with: ./scripts/build_for_profiling.sh clean"
fi

# Check if build is optimized (warn if it is)
if strings build/route | grep -q "O3"; then
    echo "⚠️  Warning: Executable may be optimized (-O3)"
    echo "   For best profiling results, use -O0 (Profiling build mode)"
fi

# =============================================================================
# 3. Check benchmark file
# =============================================================================
echo ""
echo "[3/6] Checking benchmark file..."

BENCHMARK_PATH="$BENCHMARK_DIR/$BENCHMARK"
if [ ! -f "$BENCHMARK_PATH" ]; then
    echo "ERROR: Benchmark file not found: $BENCHMARK_PATH"
    echo "Available benchmarks:"
    ls -1 "$BENCHMARK_DIR"/*.phys 2>/dev/null || echo "  (none found)"
    exit 1
fi

BENCHMARK_SIZE=$(ls -lh "$BENCHMARK_PATH" | awk '{print $5}')
echo "✓ Benchmark file found: $BENCHMARK_PATH ($BENCHMARK_SIZE)"

# =============================================================================
# 4. Clean old results
# =============================================================================
echo ""
echo "[4/6] Preparing results directory..."

if [ -d "$RESULT_DIR" ]; then
    echo "⚠️  Existing results directory found, removing..."
    rm -rf "$RESULT_DIR"
fi

echo "✓ Results directory ready: $RESULT_DIR"

# =============================================================================
# 5. Detect virtualization
# =============================================================================
echo ""
echo "[5/6] Detecting environment type..."

# Detect if running in virtualized environment
IS_VIRTUALIZED=false
if systemd-detect-virt &> /dev/null; then
    VIRT_TYPE=$(systemd-detect-virt)
    if [ "$VIRT_TYPE" != "none" ]; then
        IS_VIRTUALIZED=true
        echo "⚠️  Virtualized environment detected: $VIRT_TYPE"
        echo "   Hardware performance counters may not be available"
        echo "   Will use software sampling mode (user-mode timing)"
    else
        echo "✓ Physical machine detected (hardware sampling available)"
    fi
else
    echo "✓ Assuming physical machine (systemd-detect-virt not available)"
fi

# =============================================================================
# 6. Run vTune Hotspots analysis
# =============================================================================
echo ""
echo "[6/6] Running vTune Hotspots analysis..."
echo ""
echo "⏱️  This will take 5-15 minutes (program will run slower during profiling)"
echo "   Progress updates will appear below..."
echo ""
echo "============================================================================="

# Build the command
ROUTE_CMD="./build/route -i $BENCHMARK_PATH -o $OUTPUT_FILE -t $THREADS"

# Determine sampling mode based on environment detection
SAMPLING_MODE="hw"
if [ "$IS_VIRTUALIZED" = true ]; then
    SAMPLING_MODE="sw"
fi

echo "Sampling mode: $SAMPLING_MODE"
echo "Command: vtune -collect hotspots -knob sampling-mode=$SAMPLING_MODE -result-dir $RESULT_DIR -- $ROUTE_CMD"
echo "============================================================================="
echo ""

# Run vTune with appropriate sampling mode
# Options explained:
#   -collect hotspots: Run hotspot analysis to find time-consuming functions
#   -knob sampling-mode=hw/sw: Hardware (accurate) or software (VMware-compatible)
#   -knob enable-stack-collection=true: Collect call stacks for better context
#   -result-dir: Directory to store results
vtune -collect hotspots \
      -knob sampling-mode=$SAMPLING_MODE \
      -knob enable-stack-collection=true \
      -result-dir "$RESULT_DIR" \
      -- $ROUTE_CMD

VTUNE_EXIT_CODE=$?

# If hardware sampling failed, retry with software sampling
if [ $VTUNE_EXIT_CODE -ne 0 ] && [ "$SAMPLING_MODE" = "hw" ]; then
    echo ""
    echo "⚠️  Hardware sampling failed, retrying with software sampling..."
    echo "   (This is common in virtualized environments like VMware/VirtualBox)"
    echo ""
    
    # Clean failed results
    rm -rf "$RESULT_DIR"
    
    echo "Command: vtune -collect hotspots -knob sampling-mode=sw -result-dir $RESULT_DIR -- $ROUTE_CMD"
    echo "============================================================================="
    echo ""
    
    vtune -collect hotspots \
          -knob sampling-mode=sw \
          -knob enable-stack-collection=true \
          -result-dir "$RESULT_DIR" \
          -- $ROUTE_CMD
    
    VTUNE_EXIT_CODE=$?
    
    if [ $VTUNE_EXIT_CODE -eq 0 ]; then
        echo ""
        echo "✓ Software sampling succeeded!"
        echo "  Note: Results may be less accurate than hardware sampling"
    fi
fi

echo ""
echo "============================================================================="

if [ $VTUNE_EXIT_CODE -ne 0 ]; then
    echo "ERROR: vTune analysis failed (exit code: $VTUNE_EXIT_CODE)"
    echo ""
    echo "Common issues:"
    echo "  - Insufficient permissions (try: sudo sysctl -w kernel.perf_event_paranoid=0)"
    echo "  - Out of disk space"
    echo "  - Application crashed during profiling"
    exit 1
fi

echo "✓ vTune analysis completed successfully!"
echo "============================================================================="

# =============================================================================
# Generate quick summary
# =============================================================================
echo ""
echo "Generating summary report..."

# Get total execution time from vTune
SUMMARY_FILE="vtune_analysis_summary.txt"

cat > "$SUMMARY_FILE" << EOF
================================================================================
Intel vTune Hotspots Analysis Summary
================================================================================
Date: $(date)
Benchmark: $BENCHMARK
Threads: $THREADS
Results Directory: $RESULT_DIR
================================================================================

EOF

# Extract basic statistics from vTune
vtune -report summary -result-dir "$RESULT_DIR" -format text >> "$SUMMARY_FILE" 2>&1 || true

echo "✓ Summary saved to: $SUMMARY_FILE"

# =============================================================================
# Final instructions
# =============================================================================
echo ""
echo "============================================================================="
echo "✓ vTune analysis complete!"
echo "============================================================================="
echo ""
echo "Results location: $RESULT_DIR/"
echo "Quick summary: $SUMMARY_FILE"
echo ""
echo "Next steps:"
echo ""
echo "  1. Extract TOP3 bottlenecks (recommended):"
echo "     ./scripts/analyze_vtune_top3.sh"
echo ""
echo "  2. View detailed report in terminal:"
echo "     vtune -report hotspots -result-dir $RESULT_DIR"
echo ""
echo "  3. Open GUI for interactive analysis:"
echo "     vtune-gui $RESULT_DIR"
echo ""
echo "  4. Export specific reports:"
echo "     vtune -report hotspots -result-dir $RESULT_DIR -format text -report-output hotspots.txt"
echo "     vtune -report hotspots -result-dir $RESULT_DIR -group-by source-line -format text -report-output hotspots_by_line.txt"
echo ""
echo "============================================================================="
echo ""
echo "Disk space used:"
du -sh "$RESULT_DIR"
echo ""
echo "============================================================================="
