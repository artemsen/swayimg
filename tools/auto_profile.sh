#!/bin/bash
# Automated frame profiling for swayimg gallery lag investigation
# Requires: Wayland session
# Usage: ./tools/auto_profile.sh [gallery_path]

set -e

SWAYIMG_BIN="${PWD}/build_profile/swayimg"
GALLERY_PATH="${1:-$HOME/test_gallery_large}"
OUTPUT_DIR="profiling_results"

if [ ! -f "$SWAYIMG_BIN" ]; then
    echo "❌ Error: Profiling build not found at $SWAYIMG_BIN"
    echo "Build with: meson compile -C build_profile"
    exit 1
fi

if [ ! -d "$GALLERY_PATH" ]; then
    echo "❌ Error: Gallery not found at $GALLERY_PATH"
    exit 1
fi

echo "════════════════════════════════════════════════════"
echo "  SWAYIMG GALLERY LAG PROFILING"
echo "════════════════════════════════════════════════════"
echo ""
echo "📊 Profiling Configuration:"
echo "  Binary: $SWAYIMG_BIN"
echo "  Gallery: $GALLERY_PATH"
echo "  Images: $(ls $GALLERY_PATH | wc -l)"
echo "  Duration: ~30 seconds (auto-navigation)"
echo ""
echo "Starting profiling..."
echo ""

mkdir -p "$OUTPUT_DIR"

# Run with profiling enabled
cd "$(dirname "$SWAYIMG_BIN")/.."
export SWAYIMG_PROFILE=1
"$SWAYIMG_BIN" "$GALLERY_PATH" 2>&1 | grep -E "Frame Profiling|Frames captured|Average|FPS|Min|Max|Saved|%|GOOD|FAIR|POOR|BAD" || true

# Wait a moment for file to be written
sleep 0.5

# Process results
if [ -f "frame_profile.csv" ]; then
    TIMESTAMP=$(date +%s)
    RESULTS_FILE="$OUTPUT_DIR/frame_profile_${TIMESTAMP}.csv"

    echo ""
    echo "✓ Profiling complete!"
    echo ""

    # Move to results directory
    mv frame_profile.csv "$RESULTS_FILE"
    echo "📁 Results saved to: $RESULTS_FILE"

    # Analyze with Python for detailed statistics
    if command -v python3 &> /dev/null; then
        python3 << 'EOF' "$RESULTS_FILE"
import csv
import sys
from statistics import mean, stdev, median

csv_file = sys.argv[1]
frame_times = []

with open(csv_file, 'r') as f:
    reader = csv.reader(f)
    next(reader)  # Skip header
    for row in reader:
        if row:
            frame_times.append(float(row[1]))

if frame_times:
    avg = mean(frame_times)
    med = median(frame_times)
    min_t = min(frame_times)
    max_t = max(frame_times)
    std_dev = stdev(frame_times) if len(frame_times) > 1 else 0
    fps = 1000 / avg if avg > 0 else 0

    # Count frames by category
    fps60 = sum(1 for t in frame_times if t < 16.67)
    fps30 = sum(1 for t in frame_times if t < 33.33)
    fps15 = sum(1 for t in frame_times if t < 66.67)

    print("\n" + "="*50)
    print("DETAILED FRAME ANALYSIS")
    print("="*50)
    print(f"\nTotal frames: {len(frame_times)}")
    print(f"Average frame time: {avg:.2f} ms ({fps:.1f} FPS)")
    print(f"Median frame time: {med:.2f} ms")
    print(f"Std deviation: {std_dev:.2f} ms")
    print(f"Min/Max: {min_t:.2f} ms / {max_t:.2f} ms")
    print(f"\nFrame rate distribution:")
    print(f"  60 FPS target (<16.67ms): {fps60} frames ({100*fps60/len(frame_times):.1f}%)")
    print(f"  30 FPS target (<33.33ms): {fps30} frames ({100*fps30/len(frame_times):.1f}%)")
    print(f"  15 FPS minimum (<66.67ms): {fps15} frames ({100*fps15/len(frame_times):.1f}%)")
    print("\n" + "="*50)
EOF
    fi

    echo ""
    echo "📈 Next steps:"
    echo "  1. Review frame timing data in: $RESULTS_FILE"
    echo "  2. Update specs/9-fix-gallery-lag/research.md with findings"
    echo "  3. Identify root cause from profiling data"
    echo ""
    echo "💡 Profiling metrics:"
    echo "  - Frame times recorded in CSV format"
    echo "  - Analyze which operations exceed 16.67ms budget (60 FPS)"
    echo "  - Check if issue is consistent or intermittent"

else
    echo "⚠ Warning: No profiling data generated"
    echo "Make sure swayimg launched successfully in Wayland"
fi

echo ""
