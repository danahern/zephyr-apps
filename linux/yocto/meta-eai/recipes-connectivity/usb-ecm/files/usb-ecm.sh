#!/bin/sh
# USB composite gadget — ADB + CDC-ECM network
# Provides: adb shell/push/pull, Ethernet-over-USB for internet access

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
    # MAC: locally-administered, unique per board
    DEV_MAC="02:00:00:e7:00:01"
    HOST_MAC="02:00:00:e7:00:02"
else
    SERIAL="eai-stm32mp1-001"
    PRODUCT="STM32MP1 Dev Board"
    DEV_MAC="02:00:00:a7:00:01"
    HOST_MAC="02:00:00:a7:00:02"
fi

# Load modules (no-op if built-in)
modprobe libcomposite 2>/dev/null
modprobe usb_f_fs 2>/dev/null
modprobe usb_f_ecm 2>/dev/null

# Mount configfs
mount -t configfs none /sys/kernel/config 2>/dev/null

# Create gadget
mkdir -p $GADGET
echo 0x18d1 > $GADGET/idVendor   # Google
echo 0x4e42 > $GADGET/idProduct  # Composite ADB+ECM
echo 0x0100 > $GADGET/bcdDevice
echo 0x0200 > $GADGET/bcdUSB

# Strings
mkdir -p $GADGET/strings/0x409
echo "$SERIAL" > $GADGET/strings/0x409/serialnumber
echo "EAI" > $GADGET/strings/0x409/manufacturer
echo "$PRODUCT" > $GADGET/strings/0x409/product

# --- Function: FunctionFS for ADB ---
mkdir -p $GADGET/functions/ffs.adb

# --- Function: CDC-ECM network ---
mkdir -p $GADGET/functions/ecm.usb0
echo "$DEV_MAC" > $GADGET/functions/ecm.usb0/dev_addr
echo "$HOST_MAC" > $GADGET/functions/ecm.usb0/host_addr

# Configuration — composite: ADB + ECM
mkdir -p $GADGET/configs/c.1/strings/0x409
echo "ADB + CDC-ECM" > $GADGET/configs/c.1/strings/0x409/configuration
echo 250 > $GADGET/configs/c.1/MaxPower
ln -sf $GADGET/functions/ffs.adb $GADGET/configs/c.1/
ln -sf $GADGET/functions/ecm.usb0 $GADGET/configs/c.1/

# Mount FunctionFS for ADB
mkdir -p /dev/usb-ffs/adb
mount -t functionfs adb /dev/usb-ffs/adb 2>/dev/null

# Start adbd — MUST open FFS endpoints BEFORE UDC bind
if [ -x /usr/bin/adbd ]; then
    /usr/bin/adbd &
    sleep 2
    echo "usb-gadget: adbd started"
fi

# Bind UDC — activates both ADB and ECM
echo "$UDC_NAME" > $GADGET/UDC
echo "usb-gadget: ADB + CDC-ECM gadget active on $UDC_NAME ($PRODUCT)"

# Configure ECM network interface
sleep 1
if ifconfig usb0 192.168.55.2 netmask 255.255.255.0 up 2>/dev/null; then
    # Default route via Mac (Internet Sharing gateway)
    route add default gw 192.168.55.1 usb0 2>/dev/null
    echo "usb-gadget: ECM network up — 192.168.55.2/24, gw 192.168.55.1"
else
    echo "usb-gadget: WARNING — usb0 interface not available, ECM may not be supported"
fi
