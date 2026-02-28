#!/usr/bin/env python3
"""
Full USB PHY reset sequence for Alif E7 DWC3, matching bare-metal TinyUSB init.

Linux's DWC3 driver only does DCTL.CSFTRST (controller soft reset).
The Alif USB PHY requires a full PHY reset sequence:
  1. GCTL.CORESOFTRESET
  2. GUSB2PHYCFG.PHYSOFTRST
  3. Wait 50ms
  4. Clear PHYSOFTRST
  5. Wait 50ms
  6. Clear CORESOFTRESET

Approach: unbind DWC3 ensemble driver, do PHY reset, rebind driver.
"""
import serial
import time
import sys

PORT = '/dev/cu.usbserial-AO009AHE'
BAUD = 115200

# DWC3 register addresses (base 0x4820C000, global regs at +0x100)
GSBUSCFG0    = 0x4820C100
GCTL         = 0x4820C110
GSNPSID      = 0x4820C120
GUSB2PHYCFG  = 0x4820C200
DCTL         = 0x4820C700
DSTS         = 0x4820C704

def send_cmd(ser, cmd, timeout=2, settle=0.3):
    """Send a command and return the output."""
    ser.reset_input_buffer()
    ser.write(f'{cmd}\r\n'.encode())
    time.sleep(settle)
    output = b''
    end_time = time.time() + timeout
    while time.time() < end_time:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            output += chunk
        else:
            time.sleep(0.05)
    text = output.decode('utf-8', errors='replace')
    lines = [l for l in text.split('\n') if 'Linux Hello World' not in l]
    return '\n'.join(lines).strip()

def devmem_read(ser, addr):
    """Read a 32-bit register via devmem."""
    result = send_cmd(ser, f'devmem 0x{addr:08X}')
    for line in result.split('\n'):
        line = line.strip()
        if line.startswith('0x'):
            return int(line, 16)
    return None

def devmem_write(ser, addr, val):
    """Write a 32-bit register via devmem."""
    send_cmd(ser, f'devmem 0x{addr:08X} 32 0x{val:08X}')

def main():
    ser = serial.Serial(PORT, BAUD, timeout=2)
    time.sleep(0.5)

    print("=== Alif E7 USB PHY Reset v2 ===\n")

    # Step 0: Read initial state
    print("--- Initial state ---")
    for name, addr in [("GSNPSID", GSNPSID), ("GCTL", GCTL),
                        ("GUSB2PHYCFG", GUSB2PHYCFG), ("GSBUSCFG0", GSBUSCFG0),
                        ("DCTL", DCTL), ("DSTS", DSTS)]:
        val = devmem_read(ser, addr)
        print(f"  {name:15s} = 0x{val:08X}" if val is not None else f"  {name:15s} = ERROR")

    result = send_cmd(ser, "cat /sys/class/udc/*/state 2>/dev/null")
    for line in result.split('\n'):
        if 'attached' in line or 'configured' in line or 'default' in line:
            print(f"  UDC state       = {line.strip()}")
            break

    # Step 1: Unbind gadget from UDC (if bound)
    print("\n--- Step 1: Unbind gadget ---")
    send_cmd(ser, "echo '' > /sys/kernel/config/usb_gadget/g1/UDC 2>/dev/null")
    print("  Gadget unbound from UDC")
    time.sleep(0.5)

    # Step 2: PHY Reset sequence (matching bare-metal usbd_phy_reset)
    # This is the key missing step — Linux DWC3 driver never does this
    print("\n--- Step 2: PHY Reset (GCTL.CORESOFTRESET + GUSB2PHYCFG.PHYSOFTRST) ---")

    # 2a: Put core in reset
    gctl = devmem_read(ser, GCTL)
    gctl_rst = gctl | (1 << 11)  # CORESOFTRESET
    devmem_write(ser, GCTL, gctl_rst)
    print(f"  Set GCTL.CORESOFTRESET: 0x{gctl:08X} -> 0x{gctl_rst:08X}")

    # 2b: Assert PHY soft reset
    gusb2 = devmem_read(ser, GUSB2PHYCFG)
    gusb2_rst = gusb2 | (1 << 31)  # PHYSOFTRST
    devmem_write(ser, GUSB2PHYCFG, gusb2_rst)
    print(f"  Set GUSB2PHYCFG.PHYSOFTRST: 0x{gusb2:08X} -> 0x{gusb2_rst:08X}")

    # 2c: Wait 100ms (bare-metal uses 50ms)
    print("  Waiting 100ms...")
    time.sleep(0.1)

    # 2d: Clear PHY soft reset
    gusb2 = devmem_read(ser, GUSB2PHYCFG)
    gusb2_clr = gusb2 & ~(1 << 31)
    devmem_write(ser, GUSB2PHYCFG, gusb2_clr)
    print(f"  Clear PHYSOFTRST: 0x{gusb2:08X} -> 0x{gusb2_clr:08X}")

    # 2e: Wait 100ms
    print("  Waiting 100ms...")
    time.sleep(0.1)

    # 2f: Clear core reset
    gctl = devmem_read(ser, GCTL)
    gctl_clr = gctl & ~(1 << 11)
    devmem_write(ser, GCTL, gctl_clr)
    print(f"  Clear CORESOFTRESET: 0x{gctl:08X} -> 0x{gctl_clr:08X}")
    time.sleep(0.1)

    # Step 3: Verify PHY config after reset
    print("\n--- Step 3: Post-reset state ---")
    for name, addr in [("GCTL", GCTL), ("GUSB2PHYCFG", GUSB2PHYCFG),
                        ("GSBUSCFG0", GSBUSCFG0), ("DCTL", DCTL), ("DSTS", DSTS)]:
        val = devmem_read(ser, addr)
        print(f"  {name:15s} = 0x{val:08X}" if val is not None else f"  {name:15s} = ERROR")

    # Step 4: Rebind gadget (this triggers Linux DWC3 gadget_start → Run/Stop)
    print("\n--- Step 4: Rebind gadget ---")
    result = send_cmd(ser, "echo '48200000.usb-dual' > /sys/kernel/config/usb_gadget/g1/UDC 2>&1",
                      timeout=3)
    print(f"  Bind output: {result}")
    time.sleep(2)

    # Step 5: Final state
    print("\n--- Step 5: Final state ---")
    for name, addr in [("GCTL", GCTL), ("GUSB2PHYCFG", GUSB2PHYCFG),
                        ("DCTL", DCTL), ("DSTS", DSTS)]:
        val = devmem_read(ser, addr)
        print(f"  {name:15s} = 0x{val:08X}" if val is not None else f"  {name:15s} = ERROR")

    result = send_cmd(ser, "cat /sys/class/udc/*/state 2>/dev/null")
    for line in result.split('\n'):
        if 'attached' in line or 'configured' in line or 'default' in line:
            print(f"  UDC state       = {line.strip()}")
            break

    dsts = devmem_read(ser, DSTS)
    if dsts is not None:
        link_state = (dsts >> 18) & 0xF
        speed = dsts & 0x7
        speed_names = {0: "HS", 1: "FS", 2: "LS", 4: "SS"}
        print(f"  DSTS link       = {link_state}")
        print(f"  DSTS speed      = {speed_names.get(speed, f'Unknown({speed})')}")

    print("\n=== Check Mac: ioreg -p IOUSB -l ===")
    ser.close()

if __name__ == '__main__':
    main()
