#!/bin/bash
# Profiling script for gallery lag investigation
# Usage: ./tools/run_profiling.sh [gallery_path] [duration_seconds]

set -e

SWAYIMG_BIN="${PWD}/build_profile/swayimg"
GALLERY_PATH="${1:-$HOME/test_gallery_large}"
DURATION="${2:-30}"  # Default: 30 seconds of profiling
OUTPUT_DIR="profiling_results"

if [ ! -f "$SWAYIMG_BIN" ]; then
    echo "Error: Profiling build not found at $SWAYIMG_BIN"
    echo "Please run: meson setup build_profile --buildtype debugoptimized && meson compile -C build_profile"
    exit 1
fi

if [ ! -d "$GALLERY_PATH" ]; then
    echo "Error: Gallery directory not found: $GALLERY_PATH"
    exit 1
fi

echo "=== Gallery Lag Profiling ==="
echo "Binary: $SWAYIMG_BIN"
echo "Gallery: $GALLERY_PATH"
echo "Duration: $DURATION seconds"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Move to repo root
cd "$(dirname "$SWAYIMG_BIN")/.."

# Run swayimg and capture output
echo "Starting profiling... (Press arrow keys rapidly in gallery window to simulate navigation)"
echo "The application will close automatically after profiling."
echo ""

# Create a temporary script to automate navigation
NAV_SCRIPT="/tmp/gallery_nav.txt"
cat > "$NAV_SCRIPT" << 'EOF'
# Auto-scroll gallery using arrow keys
set-timeout 1000 'send-key Down Down Down Down Down'
set-timeout 2000 'send-key Down Down Down Down Down'
set-timeout 3000 'send-key Up Up Up Up Up'
set-timeout 4000 'send-key Down Down Down Down Down'
set-timeout 5000 'send-key Page_Down'
set-timeout 6000 'send-key Page_Up'
set-timeout 7000 'send-key End'
set-timeout 8000 'send-key Home'
EOF

# Run swayimg with gallery and collect frame timing
echo "Running: $SWAYIMG_BIN '$GALLERY_PATH'"
echo ""

# Simple version: just run swayimg, user will manually press keys
"$SWAYIMG_BIN" "$GALLERY_PATH" &
PID=$!

echo "Profiling running (PID $PID)..."
echo "In the swayimg window, rapidly press arrow keys to simulate gallery navigation"
echo "This will help generate varied frame timing data"
echo ""

# Wait for specified duration
sleep "$DURATION"

# Kill swayimg
echo ""
echo "Stopping profiling..."
kill $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

# Check for results
if [ -f "frame_profile.csv" ]; then
    echo ""
    echo "✓ Profiling complete!"
    echo ""
    echo "Results saved:"

    # Move results to output directory
    cp frame_profile.csv "$OUTPUT_DIR/frame_profile_$(date +%s).csv"
    echo "  - Frame timings: $OUTPUT_DIR/frame_profile_*.csv"

    # Analyze results
    echo ""
    echo "=== Quick Analysis ==="

    # Calculate statistics from CSV
    if command -v awk &> /dev/null; then
        AVG=$(awk -F',' 'NR>1 {sum+=$2; count++} END {print sum/count}' "frame_profile.csv")
        FPS=$(echo "1000 / $AVG" | bc -l)

        echo "Average frame time: ${AVG%.6f} ms"
        echo "Average FPS: ${FPS%.1f}"
        echo ""
        echo "Frame time budget:"
        echo "  - 60 FPS = 16.67 ms/frame"
        echo "  - 30 FPS = 33.33 ms/frame"
        echo ""
        echo "For detailed analysis, load frame_profile.csv in a spreadsheet"
        echo "or analyze with: awk -F',' 'NR>1 {print $2}' frame_profile.csv"
    fi
else
    echo "⚠ Warning: No profiling data generated"
    echo "  Make sure swayimg launched successfully"
    echo "  Try running manually: $SWAYIMG_BIN '$GALLERY_PATH'"
fi

echo ""
echo "Next steps:"
echo "1. Review frame_profile.csv for frame timing distribution"
echo "2. Check which frames exceed 16.67ms (60 FPS target)"
echo "3. Document findings in specs/9-fix-gallery-lag/research.md"
