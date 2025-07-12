# Firmware Configuration Guide

## Overview

This project now supports user-friendly firmware configuration. You no longer need to set WiFi passwords at compile time, but can use our provided web configuration tool to personalize the firmware.

## 🎯 Problems Solved

- ✅ **User-friendly**: No compilation needed, configure firmware directly
- ✅ **Security**: WiFi passwords are not exposed in code repository
- ✅ **Mass Production**: One firmware template works for all users
- ✅ **Flexible Configuration**: Support for personalized settings of all parameters

## 🚀 Usage

### 1. Get Firmware and Configuration Tool

Download from [Releases](../../releases) page:
- `display-ambient-light-board.bin` - Firmware template file
- `firmware-config-tool.html` - Configuration tool

### 2. Configure Firmware

1. **Open Configuration Tool**
   - Open `firmware-config-tool.html` in your browser
   - Supports all modern browsers (Chrome, Firefox, Safari, Edge)

2. **Upload Firmware**
   - Drag and drop or select `display-ambient-light-board.bin` file
   - Tool will automatically parse current configuration

3. **Modify Configuration**
   - **WiFi Settings**: Enter your WiFi SSID and password
   - **Network Settings**: UDP port, mDNS hostname
   - **LED Settings**: Pin, count, color order
   - **Breathing Effect**: Enable/disable, base color

4. **Download Configured Firmware**
   - Click "Update Configuration" button
   - Click "Download Configured Firmware"
   - Get personalized `display-ambient-light-board-configured.bin`

### 3. Flash Firmware

Use esptool.py to flash the configured firmware:

```bash
esptool.py --chip esp32c3 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 display-ambient-light-board-configured.bin
```

## 📋 Configurable Parameters

| Parameter | Description | Default Value | Range/Format |
|-----------|-------------|---------------|--------------|
| **WiFi SSID** | WiFi network name | TEMPLATE_SSID | 1-63 characters |
| **WiFi Password** | WiFi password | TEMPLATE_PASS | 0-63 characters |
| **UDP Port** | UDP server port | 23042 | 1024-65535 |
| **mDNS Hostname** | Device network name | board-rs | 1-31 characters |
| **LED Pin** | GPIO pin number | 4 | 0-21 |
| **Max LED Count** | Supported LED count | 500 | 1-1000 |
| **LED Color Order** | Color channel order | RGBW | RGB/GRB/RGBW etc. |
| **Breathing Effect** | Enable breathing effect | Enabled | Enabled/Disabled |
| **Breathing Base Color** | RGBW base color | (20,20,50,0) | 0-255 |

## 🔧 Technical Principles

### Firmware Configuration Area

The firmware contains a 256-byte configuration area:
- **Location**: Fixed position in firmware, identified by `FWCFG_START` marker
- **Format**: Binary structure containing magic number, version, configuration data, and checksum
- **Verification**: CRC32 checksum ensures data integrity

### Configuration Tool Working Principle

1. **Parse Firmware**: Find configuration marker, read current configuration
2. **Validate Data**: Check magic number, version, and checksum
3. **Modify Configuration**: User inputs new configuration parameters
4. **Write to Firmware**: Write new configuration to firmware's configuration area
5. **Calculate Checksum**: Recalculate and write checksum

### Runtime Reading

When device starts up:
1. Find configuration area in firmware
2. Validate configuration validity
3. If configuration is valid, use firmware configuration
4. If configuration is invalid, use compile-time default configuration

## 🛠️ Developer Information

### Build Firmware with Configuration

The project now automatically builds firmware containing configuration templates:

```bash
# Normal build
idf.py build

# Firmware will contain default configuration template
# Users can modify these configurations using the configuration tool
```

### Configuration Data Structure

```c
typedef struct {
    uint32_t magic;                 // 0x12345678
    uint32_t version;               // Configuration version
    char wifi_ssid[64];             // WiFi SSID
    char wifi_password[64];         // WiFi password
    uint16_t udp_port;              // UDP port
    char mdns_hostname[32];         // mDNS hostname
    uint8_t led_pin;                // LED pin
    uint16_t max_leds;              // Maximum LED count
    char led_order[8];              // LED color order
    // ... other configuration parameters
    uint8_t reserved[48];           // Reserved space
    uint32_t checksum;              // CRC32 checksum
} firmware_config_t;
```

## 🔍 Troubleshooting

### Configuration Tool Cannot Recognize Firmware

- **Cause**: Firmware may not contain configuration area
- **Solution**: Ensure you're using the latest version of firmware

### Device Cannot Connect to WiFi

- **Check**: Confirm WiFi SSID and password are correct
- **Reconfigure**: Use configuration tool to reset WiFi parameters

### Configuration Lost

- **Cause**: Firmware configuration area may be corrupted
- **Solution**: Reconfigure firmware using configuration tool and flash again

## 📝 Changelog

### v1.0.0
- ✅ Implemented firmware configuration area
- ✅ Created web configuration tool
- ✅ Support for runtime configuration reading
- ✅ Updated CI/CD build process

## 🤝 Contributing

If you find issues or have improvement suggestions, welcome to:
1. Submit Issues
2. Create Pull Requests
3. Participate in discussions

## 📄 License

This project is licensed under GPLv3. See [LICENSE](LICENSE) file for details.
