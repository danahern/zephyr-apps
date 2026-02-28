#!/usr/bin/env python3
"""Send commands to Alif E7 Linux console via UART and capture output."""
import serial
import sys
import time

PORT = '/dev/cu.usbserial-AO009AHE'
BAUD = 115200

def send_cmd(cmd, timeout=3, settle=0.5):
    """Send a command and return the output."""
    ser = serial.Serial(PORT, BAUD, timeout=timeout)
    # Flush input buffer
    ser.reset_input_buffer()
    # Send command
    ser.write(f'{cmd}\r\n'.encode())
    time.sleep(settle)
    # Read response
    output = b''
    end_time = time.time() + timeout
    while time.time() < end_time:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            output += chunk
        else:
            time.sleep(0.1)
    ser.close()
    # Decode and clean up
    text = output.decode('utf-8', errors='replace')
    # Filter out "Linux Hello World" spam
    lines = [l for l in text.split('\n') if 'Linux Hello World' not in l]
    return '\n'.join(lines).strip()

if __name__ == '__main__':
    cmd = ' '.join(sys.argv[1:]) if len(sys.argv) > 1 else 'echo ok'
    print(send_cmd(cmd))
