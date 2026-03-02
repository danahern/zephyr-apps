#!/usr/bin/env bash
#
# stage-ospi.sh — Copy Yocto build artifacts to images/ with flash-config names
#
# Copies xipImage and rootfs from the yocto-build container, renaming them
# to match what linux-boot-e7-ospi-jlink.json expects:
#   xipImage                          → images/xipImage-ospi
#   alif-tiny-image-devkit-e8.rootfs.cramfs-xip → images/rootfs-ospi.bin
#
# DTB (appkit-e7-ospi.dtb) is NOT copied — it's maintained manually with
# fdtput patches and lives directly in images/.
#
# Usage:
#   ./stage-ospi.sh              # copy kernel + rootfs from yocto-build container
#   ./stage-ospi.sh --dry-run    # show what would be copied without doing it

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGES_DIR="$SCRIPT_DIR/images"
CONTAINER="yocto-build"
DEPLOY_DIR="/home/builder/yocto/build-alif-e7/tmp/deploy/images/devkit-e8"

DRY_RUN=false
if [[ "${1:-}" == "--dry-run" ]]; then
    DRY_RUN=true
fi

# Verify container is running
if ! docker inspect "$CONTAINER" --format '{{.State.Status}}' 2>/dev/null | grep -q running; then
    echo "Error: container '$CONTAINER' is not running"
    exit 1
fi

mkdir -p "$IMAGES_DIR"

# Source → destination pairs (parallel arrays for bash 3 compat)
SRCS=(
    "$DEPLOY_DIR/xipImage"
    "$DEPLOY_DIR/alif-tiny-image-devkit-e8.rootfs.cramfs-xip"
)
DSTS=(
    "$IMAGES_DIR/xipImage-ospi"
    "$IMAGES_DIR/rootfs-ospi.bin"
)

for i in 0 1; do
    src="${SRCS[$i]}"
    dst="${DSTS[$i]}"
    name=$(basename "$dst")

    # Resolve symlink to get real file (Yocto uses timestamped names)
    real=$(docker exec "$CONTAINER" readlink -f "$src" 2>/dev/null) || {
        echo "Warning: $src not found in container, skipping"
        continue
    }

    if $DRY_RUN; then
        size=$(docker exec "$CONTAINER" stat -c %s "$real" 2>/dev/null || echo "?")
        echo "[dry-run] $real -> $name ($size bytes)"
    else
        echo "Copying $name..."
        docker cp "$CONTAINER:$real" "$dst"
        ls -lh "$dst"
    fi
done

if ! $DRY_RUN; then
    echo ""
    echo "Staged files:"
    ls -lh "$IMAGES_DIR"/xipImage-ospi "$IMAGES_DIR"/rootfs-ospi.bin "$IMAGES_DIR"/appkit-e7-ospi.dtb 2>/dev/null
    echo ""
    echo "Ready for: alif-flash.jlink_flash(config=\"linux-boot-e7-ospi-jlink.json\", verify=true)"
fi
