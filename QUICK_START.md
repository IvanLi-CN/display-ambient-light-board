# 🚀 Quick Start Guide

## Overview

This project now supports user-friendly firmware configuration! You no longer need to modify code or recompile. Simply use our web configuration tool to customize your ambient light board.

## 📥 Getting Files

### Method 1: Download from Releases (Recommended)

1. Visit the [Releases page](../../releases)
2. Download the latest version of the following files:
   - `display-ambient-light-board.bin` - Firmware template
   - `firmware-config-tool.html` - Configuration tool

### Method 2: Download from Development Build

1. Visit the [Actions page](../../actions)
2. Select the latest successful build
3. Download files from Artifacts

## ⚙️ Configure Firmware

### 1. Open Configuration Tool

Open the `firmware-config-tool.html` file in your browser

### 2. Upload Firmware

- Drag and drop `display-ambient-light-board.bin` to the upload area
- Or click "Select Firmware File" button to choose the file

### 3. Configure Parameters

**WiFi Settings** (Required):
- WiFi SSID: Your WiFi network name
- WiFi Password: Your WiFi password

**Network Settings** (Optional):
- UDP Port: Default 23042
- mDNS Hostname: Default board-rs (accessible at board-rs.local)

**LED Settings** (Optional):
- LED Pin: Default GPIO 4
- Max LED Count: Default 500
- LED Color Order: Default RGBW

**Breathing Effect** (Optional):
- Enable/Disable breathing effect
- Base color settings (RGBW)

### 4. Download Configured Firmware

1. Click "Update Configuration" button
2. Click "Download Configured Firmware" button
3. Save the `display-ambient-light-board-configured.bin` file

## 🔥 Flash Firmware

### Using esptool.py

```bash
# Install esptool (if not already installed)
pip install esptool

# Flash firmware
esptool.py --chip esp32c3 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 display-ambient-light-board-configured.bin
```

### Port Information

- **Linux**: `/dev/ttyUSB0` or `/dev/ttyACM0`
- **macOS**: `/dev/cu.usbserial-*` or `/dev/cu.usbmodem*`
- **Windows**: `COM3`, `COM4`, etc.

## 🌐 Using the Device

### 1. First Boot

After startup, the device will:
1. Connect to your configured WiFi network
2. Start UDP server (port 23042)
3. Enable mDNS service (board-rs.local)
4. Begin breathing effect (if enabled)

### 2. Send Ambient Light Data

Use UDP protocol to send RGB data to the device:

```python
import socket

# Connect to device
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Send RGB data (3 bytes per LED: R, G, B)
rgb_data = bytes([255, 0, 0] * 10)  # 10 red LEDs
sock.sendto(rgb_data, ('board-rs.local', 23042))
```

### 3. Network Discovery

The device supports mDNS and can be accessed via hostname:
- Default: `board-rs.local`
- Custom: `your-hostname.local`

## 🔧 Troubleshooting

### Configuration Tool Cannot Recognize Firmware

**Problem**: "Configuration area not found" message after uploading firmware

**Solution**:
1. Ensure you're using the latest version of firmware
2. Check if the file was downloaded completely
3. Try re-downloading the firmware file

### Device Cannot Connect to WiFi

**Problem**: Device cannot connect to network after startup

**Solution**:
1. Check if WiFi SSID and password are correct
2. Confirm WiFi network is 2.4GHz (ESP32-C3 doesn't support 5GHz)
3. Check if WiFi network name contains special characters
4. Reconfigure firmware and flash again

### Cannot Discover Device

**Problem**: Cannot find device via mDNS

**Solution**:
1. Confirm device is connected to the same network
2. Try using IP address instead of hostname
3. Check firewall settings
4. Use network scanning tools to find device IP

### LEDs Not Working or Wrong Colors

**Problem**: LED display abnormal

**Solution**:
1. Check if LED pin configuration is correct
2. Confirm LED color order setting (RGB/GRB/RGBW etc.)
3. Check LED count setting
4. Verify hardware connections

## 📋 Configuration Parameters

| Parameter | Default Value | Description | Range |
|-----------|---------------|-------------|-------|
| WiFi SSID | TEMPLATE_SSID | WiFi network name | 1-63 characters |
| WiFi Password | TEMPLATE_PASS | WiFi password | 0-63 characters |
| UDP Port | 23042 | UDP server port | 1024-65535 |
| mDNS Hostname | board-rs | Device network name | 1-31 characters |
| LED Pin | 4 | GPIO pin number | 0-21 |
| Max LED Count | 500 | Supported LED count | 1-1000 |
| LED Color Order | RGBW | Color channel order | RGB/GRB/RGBW etc. |

## 🎯 Next Steps

1. **Test Connection**: Confirm device can connect to WiFi properly
2. **Send Data**: Try sending RGB data to the device
3. **Adjust Parameters**: Reconfigure firmware as needed
4. **Integrate Application**: Integrate device into your ambient light system

## 💡 Tips

- Configuration tool runs entirely in browser, no data is uploaded
- You can save multiple configured firmware versions
- Recommend backing up original firmware file before configuration
- If configuration is wrong, you can reconfigure and flash again

## 🆘 Getting Help

If you encounter problems:
1. Check the [Troubleshooting](#troubleshooting) section
2. Review known issues in [Issues](../../issues)
3. Create a new Issue describing your problem
