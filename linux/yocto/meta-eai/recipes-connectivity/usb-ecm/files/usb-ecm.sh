#!/bin/sh
# USB ADB gadget — board-agnostic
# Provides adb shell, adb push/pull, adb forward

GADGET=/sys/kernel/config/usb_gadget/g1

# Find UDC
UDC_NAME=$(ls /sys/class/udc/ 2>/dev/null | head -n 1)
if [ -z "$UDC_NAME" ]; then
    echo "usb-gadget: No UDC found"
    exit 1
fi

# Detect board from device tree
if grep -q "Alif" /proc/device-tree/model 2>/dev/null; then
    SERIAL="eai-alif-e7-001"
    PRODUCT="Alif E7 Dev Board"
else
    SERIAL="eai-stm32mp1-001"
    PRODUCT="STM32MP1 Dev Board"
fi

# Load modules
modprobe libcomposite 2>/dev/null
modprobe usb_f_fs 2>/dev/null

# Mount configfs
mount -t configfs none /sys/kernel/config 2>/dev/null

# Create gadget
mkdir -p $GADGET
echo 0x18d1 > $GADGET/idVendor   # Google
echo 0x4e11 > $GADGET/idProduct  # ADB device
echo 0x0100 > $GADGET/bcdDevice
echo 0x0200 > $GADGET/bcdUSB

# Strings
mkdir -p $GADGET/strings/0x409
echo "$SERIAL" > $GADGET/strings/0x409/serialnumber
echo "EAI" > $GADGET/strings/0x409/manufacturer
echo "$PRODUCT" > $GADGET/strings/0x409/product

# FunctionFS for ADB
mkdir -p $GADGET/functions/ffs.adb

# Configuration
mkdir -p $GADGET/configs/c.1/strings/0x409
echo "ADB" > $GADGET/configs/c.1/strings/0x409/configuration
echo 250 > $GADGET/configs/c.1/MaxPower
ln -sf $GADGET/functions/ffs.adb $GADGET/configs/c.1/

# Mount FunctionFS
mkdir -p /dev/usb-ffs/adb
mount -t functionfs adb /dev/usb-ffs/adb 2>/dev/null

# Start adbd — MUST open FFS endpoints BEFORE UDC bind
if [ -x /usr/bin/adbd ]; then
    /usr/bin/adbd &
    sleep 2
    echo "usb-gadget: adbd started"
fi

# Bind UDC
echo "$UDC_NAME" > $GADGET/UDC
echo "usb-gadget: ADB gadget active on $UDC_NAME ($PRODUCT)"
