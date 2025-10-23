#!/bin/bash

# =============================================================================
# Extract TOP3 Performance Bottlenecks from vTune Results
# =============================================================================
# This script analyzes vTune Hotspots results and extracts the top 3
# performance bottlenecks (both function-level and source-line-level).
#
# Prerequisites:
#   - vTune analysis must be completed (run ./vtune_hotspots_analysis.sh first)
#   - Results directory (vtune_results/) must exist
#
# Usage:
#   ./analyze_vtune_top3.sh [result_dir]
#
# Arguments:
#   result_dir  - vTune results directory (default: vtune_results)
#
# Output:
#   - VTUNE-HOTSPOTS-ANALYSIS.md: Detailed Markdown report with TOP3 bottlenecks
#   - vtune_top_functions.txt: Function-level hotspots (raw data)
#   - vtune_top_lines.txt: Source-line-level hotspots (raw data)
#
# Example:
#   ./analyze_vtune_top3.sh
#   ./analyze_vtune_top3.sh vtune_results
# =============================================================================

set -e  # Exit on error

# Default settings
RESULT_DIR="${1:-vtune_results}"
OUTPUT_MD="VTUNE-HOTSPOTS-ANALYSIS.md"
FUNCTIONS_TXT="vtune_top_functions.txt"
LINES_TXT="vtune_top_lines.txt"
CALLSTACKS_TXT="vtune_callstacks.txt"

echo "============================================================================="
echo "vTune Results Analysis - TOP3 Performance Bottlenecks"
echo "============================================================================="
echo ""
echo "Results directory: $RESULT_DIR"
echo "Output report: $OUTPUT_MD"
echo ""

# =============================================================================
# 1. Check prerequisites
# =============================================================================
echo "[1/4] Checking prerequisites..."

# Check if vTune is available
if ! command -v vtune &> /dev/null; then
    echo "ERROR: Intel vTune not found in PATH"
    echo "Please load oneAPI environment: source /xrepo/App/oneAPI/setvars.sh"
    exit 1
fi

echo "✓ vTune command found"

# Check if results directory exists
if [ ! -d "$RESULT_DIR" ]; then
    echo "ERROR: Results directory not found: $RESULT_DIR"
    echo ""
    echo "Please run vTune analysis first:"
    echo "  ./scripts/vtune_hotspots_analysis.sh"
    exit 1
fi

echo "✓ Results directory found: $RESULT_DIR"

# Verify it's a valid vTune result (check for common result files/directories)
VALID_RESULT=false
if [ -d "$RESULT_DIR/data.0" ] || [ -f "$RESULT_DIR/result.db" ] || [ -f "$RESULT_DIR/result.sqlite" ] || ls "$RESULT_DIR"/*.amplxe &>/dev/null; then
    VALID_RESULT=true
fi

# Additional check: try to run a simple vTune command
if vtune -report summary -result-dir "$RESULT_DIR" &>/dev/null; then
    VALID_RESULT=true
fi

if [ "$VALID_RESULT" = false ]; then
    echo "ERROR: Directory does not contain valid vTune results"
    echo "Please check the results directory or re-run the analysis."
    echo ""
    echo "Directory contents:"
    ls -la "$RESULT_DIR" | head -20
    exit 1
fi

echo "✓ Valid vTune results detected"

# =============================================================================
# 2. Export function-level hotspots
# =============================================================================
echo ""
echo "[2/4] Extracting function-level hotspots..."

vtune -report hotspots \
      -result-dir "$RESULT_DIR" \
      -format text \
      -report-output "$FUNCTIONS_TXT" \
      -column="CPU Time:Self,Function,Module,Source File" \
      -csv-delimiter comma

if [ $? -eq 0 ] && [ -f "$FUNCTIONS_TXT" ]; then
    echo "✓ Function-level hotspots exported to: $FUNCTIONS_TXT"
    FUNCTIONS_LINES=$(wc -l < "$FUNCTIONS_TXT")
    echo "  Total entries: $FUNCTIONS_LINES"
else
    echo "⚠️  Warning: Failed to export function-level hotspots"
fi

# =============================================================================
# 3. Export source-line-level hotspots
# =============================================================================
echo ""
echo "[3/4] Extracting source-line-level hotspots..."

vtune -report hotspots \
      -result-dir "$RESULT_DIR" \
      -group-by source-line \
      -format text \
      -report-output "$LINES_TXT" \
      -column="CPU Time:Self,Source File,Source Line" \
      -csv-delimiter comma

if [ $? -eq 0 ] && [ -f "$LINES_TXT" ]; then
    echo "✓ Source-line-level hotspots exported to: $LINES_TXT"
    LINES_COUNT=$(wc -l < "$LINES_TXT")
    echo "  Total entries: $LINES_COUNT"
else
    echo "⚠️  Warning: Failed to export source-line-level hotspots"
fi

# =============================================================================
# 4. Generate Markdown report
# =============================================================================
echo ""
echo "[4/4] Generating Markdown report..."

# Start writing the Markdown report
cat > "$OUTPUT_MD" << 'EOF_HEADER'
# Intel vTune Hotspots Analysis - Performance Bottlenecks

## Executive Summary

This report identifies the **TOP 3 performance bottlenecks** in the Potter FPGA router based on Intel vTune Profiler analysis.

**Analysis Configuration:**
- **Tool**: Intel vTune Profiler (Hotspots Analysis)
- **Build Type**: Profiling (-O0, no optimization, full debug symbols)
- **Sampling Mode**: Hardware event-based sampling
- **Metric**: CPU Time (Self) - Time spent in the function itself (excluding callees)

**Why -O0 (no optimization)?**
- Makes bottlenecks more visible and easier to identify
- Prevents compiler optimizations from hiding performance issues
- Allows accurate source-line-level attribution
- Real-world performance gains will be higher when optimizations are re-enabled

---

## Analysis Metadata

EOF_HEADER

# Add metadata
echo "- **Analysis Date**: $(date)" >> "$OUTPUT_MD"
echo "- **Results Directory**: \`$RESULT_DIR\`" >> "$OUTPUT_MD"
echo "- **Potter Executable**: \`build/route\` (Profiling build)" >> "$OUTPUT_MD"
echo "" >> "$OUTPUT_MD"

# Get summary info from vTune
if vtune -report summary -result-dir "$RESULT_DIR" > /tmp/vtune_summary.txt 2>&1; then
    # Extract application command line
    APP_CMD=$(grep -A1 "Application Command Line" /tmp/vtune_summary.txt | tail -1 || echo "N/A")
    echo "- **Command Line**: \`$APP_CMD\`" >> "$OUTPUT_MD"

    # Extract elapsed time
    ELAPSED=$(grep "Elapsed Time" /tmp/vtune_summary.txt | awk '{print $3, $4}' || echo "N/A")
    echo "- **Total Elapsed Time**: $ELAPSED" >> "$OUTPUT_MD"

    # Extract CPU time
    CPU_TIME=$(grep "CPU Time" /tmp/vtune_summary.txt | head -1 | awk '{print $3, $4}' || echo "N/A")
    echo "- **Total CPU Time**: $CPU_TIME" >> "$OUTPUT_MD"

    rm -f /tmp/vtune_summary.txt
fi

echo "" >> "$OUTPUT_MD"
echo "---" >> "$OUTPUT_MD"
echo "" >> "$OUTPUT_MD"

# =============================================================================
# Extract TOP3 Functions
# =============================================================================
echo "## TOP 3 Performance Bottlenecks (Function-Level)" >> "$OUTPUT_MD"
echo "" >> "$OUTPUT_MD"

if [ -f "$FUNCTIONS_TXT" ]; then
    # Parse the CSV-style output
    # Skip header lines and extract top 3 functions
    echo "### #1 Hotspot Function" >> "$OUTPUT_MD"
    echo "" >> "$OUTPUT_MD"

    # Extract function data (skip header, get line 1-3)
    awk -F',' 'NR>1 && NR<=4 {
        if (NR == 2) rank = "#1";
        else if (NR == 3) rank = "#2";
        else if (NR == 4) rank = "#3";

        gsub(/^[ \t]+|[ \t]+$/, "", $1);  # Trim whitespace
        gsub(/^[ \t]+|[ \t]+$/, "", $2);
        gsub(/^[ \t]+|[ \t]+$/, "", $3);
        gsub(/^[ \t]+|[ \t]+$/, "", $4);

        if (NR > 2) print "### " rank " Hotspot Function\n";
        print "**CPU Time (Self)**: " $1 "";
        print "**Function**: `" $2 "`";
        print "**Module**: `" $3 "`";
        print "**Source File**: `" $4 "`";
        print "";
        print "---";
        print "";
    }' "$FUNCTIONS_TXT" >> "$OUTPUT_MD"

    # If no data was extracted, add a note
    if ! grep -q "CPU Time (Self)" "$OUTPUT_MD"; then
        echo "⚠️ **Note**: Unable to parse function-level data. Check \`$FUNCTIONS_TXT\` manually." >> "$OUTPUT_MD"
        echo "" >> "$OUTPUT_MD"
    fi
else
    echo "⚠️ **Error**: Function-level data not available." >> "$OUTPUT_MD"
    echo "" >> "$OUTPUT_MD"
fi

echo "" >> "$OUTPUT_MD"

# =============================================================================
# Extract TOP3 Source Lines
# =============================================================================
echo "## TOP 3 Performance Bottlenecks (Source-Line-Level)" >> "$OUTPUT_MD"
echo "" >> "$OUTPUT_MD"
echo "These are the specific source code lines consuming the most CPU time." >> "$OUTPUT_MD"
echo "" >> "$OUTPUT_MD"

if [ -f "$LINES_TXT" ]; then
    echo "### Top Hotspot Lines" >> "$OUTPUT_MD"
    echo "" >> "$OUTPUT_MD"

    # Parse line-level data
    awk -F',' 'NR>1 && NR<=11 {
        if (NR == 2) rank = 1;
        else rank = NR - 1;

        gsub(/^[ \t]+|[ \t]+$/, "", $1);  # Trim
        gsub(/^[ \t]+|[ \t]+$/, "", $2);
        gsub(/^[ \t]+|[ \t]+$/, "", $3);

        print rank ". **" $1 "** - `" $2 ":" $3 "`";
    }' "$LINES_TXT" >> "$OUTPUT_MD"

    if ! grep -q "\\*\\*" "$OUTPUT_MD" | tail -5 | head -1; then
        echo "⚠️ **Note**: Unable to parse source-line data. Check \`$LINES_TXT\` manually." >> "$OUTPUT_MD"
    fi
else
    echo "⚠️ **Error**: Source-line data not available." >> "$OUTPUT_MD"
fi

echo "" >> "$OUTPUT_MD"
echo "---" >> "$OUTPUT_MD"
echo "" >> "$OUTPUT_MD"

# =============================================================================
# Add optimization recommendations
# =============================================================================
cat >> "$OUTPUT_MD" << 'EOF_RECOMMENDATIONS'
## Optimization Recommendations

Based on the TOP 3 bottlenecks identified above, consider the following optimization strategies:

### Strategy 1: Replace with oneAPI Libraries

| Bottleneck Type | Recommended Library | Example Functions |
|----------------|---------------------|-------------------|
| **Mathematical functions** (exp, log, sqrt, sin, cos) | **oneMKL VML** | `vdExp`, `vdSqrt`, `vdLog`, `vdSin` |
| **Linear algebra** (matrix multiply, solve) | **oneMKL BLAS/LAPACK** | `cblas_dgemm`, `dgesv` |
| **Sorting/searching** | **oneDPL** (Data Parallel C++ Library) | Parallel `std::sort`, `std::find` |
| **Image/signal processing** | **Intel IPP** | Convolution, FFT, filtering |
| **Memory-intensive loops** | **Intel TBB** | `parallel_for`, `parallel_reduce` |
| **String operations** | **Intel IPP** | String search, comparison |

### Strategy 2: Vectorization

If the bottleneck is a tight loop:
1. Use `#pragma omp simd` for auto-vectorization
2. Ensure data alignment (`alignas(64)`)
3. Use Intel intrinsics for manual vectorization (AVX-512)

### Strategy 3: Parallelization

If the bottleneck is embarrassingly parallel:
1. Use Intel TBB `parallel_for` for loop parallelism
2. Use OpenMP `#pragma omp parallel for` for simple cases
3. Consider task-based parallelism with TBB task groups

---

## Next Steps

### 1. Analyze Each Bottleneck

For each of the TOP 3 hotspots:
- Read the source code at the identified location
- Understand the algorithm and data structures
- Identify the bottleneck type (compute, memory, I/O)

### 2. Select Appropriate Library

Based on bottleneck type:
- **oneMKL**: For math-heavy code
- **Intel IPP**: For signal/image processing
- **Intel TBB**: For parallelizable loops
- **oneDPL**: For standard algorithm replacements

### 3. Implement Optimization

- Replace the hotspot code with library calls
- Compile with Intel icpx compiler
- Ensure proper linking (`-lmkl_intel_lp64 -lmkl_intel_thread -lmkl_core`)

### 4. Rebuild with Optimization

After applying library replacements:
```bash
# Rebuild with full optimizations
./scripts/build_intel.sh clean release -j 40

# Re-run benchmark
./build/route -i benchmarks/boom_med_pb_unrouted.phys \
              -o benchmarks/boom_med_pb_routed.phys -t 32
```

### 5. Measure Performance Gain

Compare before/after:
- Total route time
- Iteration count
- Memory usage

Target improvement: **20-50%** for well-optimized hotspots

---

## Additional Analysis (Optional)

### View Interactive GUI

```bash
vtune-gui vtune_results
```

### Export Detailed Reports

```bash
# Function call stacks
vtune -report callstacks -result-dir vtune_results -format text -report-output callstacks.txt

# Threading analysis
vtune -report summary -result-dir vtune_results -format text -report-output threading.txt

# Module/library breakdown
vtune -report hotspots -result-dir vtune_results -group-by module -format text -report-output by_module.txt
```

---

## Raw Data Files

For detailed analysis, refer to:
- **Function-level hotspots**: `vtune_top_functions.txt`
- **Source-line hotspots**: `vtune_top_lines.txt`
- **Full vTune results**: `vtune_results/` (open with `vtune-gui`)

---

## References

- [Intel oneMKL Developer Reference](https://www.intel.com/content/www/us/en/docs/onemkl/developer-reference-dpcpp/2024-2/overview.html)
- [Intel IPP Documentation](https://www.intel.com/content/www/us/en/docs/ipp/developer-reference/current/overview.html)
- [Intel TBB Documentation](https://www.intel.com/content/www/us/en/docs/onetbb/developer-guide-api-reference/current/overview.html)
- [Intel VTune Profiler User Guide](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/current/overview.html)

EOF_RECOMMENDATIONS

echo ""
echo "============================================================================="
echo "✓ Analysis complete!"
echo "============================================================================="
echo ""
echo "Generated files:"
echo "  1. $OUTPUT_MD (Main report with TOP3 bottlenecks)"
echo "  2. $FUNCTIONS_TXT (Raw function-level data)"
echo "  3. $LINES_TXT (Raw source-line data)"
echo ""
echo "Next steps:"
echo "  1. Read the analysis report:"
echo "     cat $OUTPUT_MD"
echo ""
echo "  2. Open in a Markdown viewer:"
echo "     mdless $OUTPUT_MD  # or your preferred viewer"
echo ""
echo "  3. For interactive analysis:"
echo "     vtune-gui $RESULT_DIR"
echo ""
echo "============================================================================="
