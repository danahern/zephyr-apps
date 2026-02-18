#!/bin/sh
# Flash Yocto WIC image to STM32MP1 SD card over SSH (no card swap needed)
#
# Usage: ./flash-over-ssh.sh [image.wic.bz2] [board_ip]
#
# Streams the decompressed WIC image over SSH to dd on the board.
# WARNING: Overwrites the SD card while the board is running from it.
# The kernel and userspace are cached in RAM, so this works in practice,
# but if the connection drops mid-flash, you'll need a physical SD card swap.

set -e

IMAGE="${1:-yocto-build/core-image-minimal-stm32mp1.wic.bz2}"
BOARD="${2:-192.168.7.2}"
DEVICE="/dev/mmcblk1"

if [ ! -f "$IMAGE" ]; then
    echo "Error: Image not found: $IMAGE"
    exit 1
fi

echo "Flashing $IMAGE to $BOARD:$DEVICE"
echo "Image size: $(ls -lh "$IMAGE" | awk '{print $5}')"
echo ""
echo "WARNING: This overwrites the running SD card. If interrupted, do a physical card swap."
echo "Press Ctrl-C within 3 seconds to cancel..."
sleep 3

# Pre-flight: verify SSH works and find the SD card device
echo "Verifying connection..."
ssh -o ConnectTimeout=5 root@"$BOARD" "
    echo 'Connected to:' \$(hostname)
    echo 'SD card:' \$(ls $DEVICE 2>/dev/null || echo 'NOT FOUND')
    # Sync filesystems before we start overwriting
    sync
    # Drop caches to free RAM
    echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || true
"

echo ""
echo "Flashing... (this takes ~2 minutes)"
START=$(date +%s)

bzcat "$IMAGE" | ssh root@"$BOARD" "dd of=$DEVICE bs=4M 2>&1"

END=$(date +%s)
ELAPSED=$((END - START))
echo ""
echo "Flash complete in ${ELAPSED}s"
echo "Rebooting board..."

ssh -o ConnectTimeout=5 root@"$BOARD" "reboot" 2>/dev/null || true

echo "Board is rebooting. Wait ~40 seconds for USB networking to come up."
echo "Then: ssh root@$BOARD"
