#!/bin/bash
# Create a test gallery with 2000+ images for performance profiling

set -e

GALLERY_DIR="${1:-$HOME/test_gallery_large}"
NUM_IMAGES="${2:-2000}"

echo "Creating test gallery: $GALLERY_DIR with $NUM_IMAGES images..."

# Create directory
mkdir -p "$GALLERY_DIR"

# Find an existing image to use as template
TEMPLATE_IMAGE="/home/neg/src/swayimg/.github/gallery.png"

if [ ! -f "$TEMPLATE_IMAGE" ]; then
    echo "Error: Template image not found at $TEMPLATE_IMAGE"
    exit 1
fi

# Create symlinks to the same image with different names
# This is fast and doesn't use extra disk space
echo "Creating $NUM_IMAGES symlinks to test image..."

for i in $(seq 1 $NUM_IMAGES); do
    # Format: image_0001.png, image_0002.png, etc.
    FILENAME=$(printf "image_%04d.png" $i)
    ln -sf "$TEMPLATE_IMAGE" "$GALLERY_DIR/$FILENAME" 2>/dev/null || {
        # Fallback: copy if symlink fails
        cp "$TEMPLATE_IMAGE" "$GALLERY_DIR/$FILENAME"
    }

    # Show progress every 500 images
    if [ $((i % 500)) -eq 0 ]; then
        echo "  Created $i/$NUM_IMAGES images..."
    fi
done

echo "✓ Test gallery created successfully!"
echo "  Location: $GALLERY_DIR"
echo "  Images: $NUM_IMAGES"
echo ""
echo "To use with swayimg:"
echo "  ./build_profile/swayimg $GALLERY_DIR"
