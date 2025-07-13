# ESP32-C3 Ambient Light Board

A WiFi-enabled ambient light controller based on ESP32-C3, designed to drive SK6812 RGBW LED strips. This project implements the same protocol as the original board-rs project but uses C language and ESP-IDF framework.

## Features

- **ESP32-C3 RISC-V MCU** running at 160MHz
- **WiFi 802.11 b/g/n** connectivity (2.4GHz)
- **SK6812 RGBW LED** support up to 500 LEDs
- **UDP protocol** for real-time LED data transmission
- **mDNS service discovery** for automatic device detection
- **State machine** for robust system management
- **Advanced Status System**:
  - All LEDs breathe with configurable ambient color
  - First LED shows system status with different colors (see LED Status Indication section)
- **Configurable Settings**: WiFi credentials, LED count, colors, and more via menuconfig
- **Real-time statistics** and monitoring

## Hardware Requirements

- ESP32-C3 development board
- SK6812 RGBW LED strip
- 5V power supply for LED strip
- Level shifter (optional, for better signal integrity)

## Pin Configuration

- **GPIO4**: LED data output (configurable in `config.h`)
- **3.3V**: ESP32-C3 power
- **5V**: LED strip power (separate supply recommended)
- **GND**: Common ground

## Software Requirements

- ESP-IDF v5.0 or later
- Python 3.7+
- Git

## Installation

### 1. Install ESP-IDF

```bash
# Clone ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Install for ESP32-C3
./install.sh esp32c3

# Set up environment
source export.sh
```

### 2. Clone and Build Project

```bash
# Set target chip
idf.py set-target esp32c3

# Configure project (optional)
idf.py menuconfig

# Build project
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

## Configuration

### WiFi Credentials

WiFi credentials are now configured through the ESP-IDF configuration system for security:

```bash
# Configure WiFi settings
idf.py menuconfig

# Navigate to: Ambient Light Board Configuration
# Set your WiFi SSID and password
```

Or edit `sdkconfig` directly:
```
CONFIG_WIFI_SSID="YourWiFiSSID"
CONFIG_WIFI_PASSWORD="YourWiFiPassword"
CONFIG_WIFI_MAXIMUM_RETRY=5
CONFIG_LED_DATA_PIN=4
CONFIG_MAX_LED_COUNT=500
CONFIG_UDP_PORT=23042
CONFIG_MDNS_HOSTNAME="board-rs"
CONFIG_LED_REFRESH_RATE_FPS=30
CONFIG_ENABLE_BREATHING_EFFECT=y
CONFIG_BREATHING_BASE_RED=20
CONFIG_BREATHING_BASE_GREEN=20
CONFIG_BREATHING_BASE_BLUE=50
CONFIG_BREATHING_BASE_WHITE=0
```

### Available Configuration Options

| Option | Description | Default |
|--------|-------------|---------|
| `CONFIG_WIFI_SSID` | WiFi network name | "myssid" |
| `CONFIG_WIFI_PASSWORD` | WiFi password | "mypassword" |
| `CONFIG_WIFI_MAXIMUM_RETRY` | Max WiFi retry attempts | 5 |
| `CONFIG_LED_DATA_PIN` | GPIO pin for LED data | 4 |
| `CONFIG_MAX_LED_COUNT` | Maximum number of LEDs | 500 |
| `CONFIG_UDP_PORT` | UDP server port | 23042 |
| `CONFIG_MDNS_HOSTNAME` | mDNS hostname | "board-rs" |
| `CONFIG_LED_REFRESH_RATE_FPS` | LED refresh rate | 30 |
| `CONFIG_ENABLE_BREATHING_EFFECT` | Enable breathing effect | Yes |
| `CONFIG_BREATHING_BASE_*` | Base breathing color (RGBW) | (20,20,50,0) |

### LED Configuration

Modify `main/config.h` to adjust LED settings:

```c
#define LED_DATA_PIN            GPIO_NUM_4    // LED data pin
#define MAX_LED_COUNT           500           // Maximum LEDs
#define LED_CHANNELS_PER_LED    4             // RGBW channels
```

## LED Status Indication

The system provides visual feedback through LED breathing effects:

### System States
- **🤍 White breathing**: System initializing
- **🔵 Blue breathing**: Connecting to WiFi
- **🟢 Green breathing**: Network ready (DHCP complete)
- **🟣 Purple breathing**: Operational (ready to receive LED data)
- **🔴 Red breathing**: WiFi or general error
- **🟠 Orange breathing**: UDP or DHCP error

### Behavior
- **All LEDs**: Breathe with configurable base color (default: soft blue)
- **First LED**: Shows system status with specific colors
- **Data Mode**: Breathing stops when receiving LED data packets

## Protocol

The device implements the same UDP protocol as the original board-rs project:

### Packet Types

- **0x01**: Ping packet (connection check)
- **0x02**: LED data packet
- **0x03, 0x04**: Ignored packets

### LED Data Packet Format

```
[0x02] [Offset High] [Offset Low] [LED Data...]
```

- **Offset**: 16-bit big-endian LED offset
- **LED Data**: RGBW data (4 bytes per LED)

### mDNS Service

The device advertises itself as:
- **Service**: `_ambient_light._udp.local.`
- **Hostname**: `board-rs.local.`
- **Port**: 23042

## System States

The device uses a state machine with the following states:

1. **SYSTEM_INIT**: Initial system setup
2. **WIFI_CONNECTING**: Connecting to WiFi
3. **DHCP_REQUESTING**: Obtaining IP address
4. **NETWORK_READY**: Network connection established
5. **UDP_STARTING**: Starting UDP server
6. **UDP_LISTENING**: Listening for UDP packets
7. **OPERATIONAL**: Fully operational, processing LED data
8. **Error States**: Various error conditions with recovery

## Status Indication

- **Blue breathing**: System connecting to WiFi
- **Off**: System operational and ready
- **LED data**: Direct passthrough from UDP packets

## Monitoring

The system provides detailed logging and statistics:

```
I (12345) MAIN: System status: OPERATIONAL, Free heap: 234567 bytes
I (12345) MAIN: UDP stats: 1234 packets (567890 bytes), 1200 LED, 34 ping
I (12345) MAIN: LED stats: 1200 transmissions (2400000 bytes)
```

## Troubleshooting

### WiFi Connection Issues

1. Check WiFi credentials in `main.c`
2. Ensure 2.4GHz network (ESP32-C3 doesn't support 5GHz)
3. Check signal strength and router compatibility

### LED Issues

1. Verify power supply (5V for LEDs, 3.3V for ESP32-C3)
2. Check data pin connection (GPIO4 by default)
3. Ensure proper grounding between ESP32-C3 and LED strip
4. Consider using a level shifter for long LED strips

### UDP Communication Issues

1. Check firewall settings on client device
2. Verify mDNS resolution (`ping board-rs.local`)
3. Test UDP connectivity (`nc -u board-rs.local 23042`)
4. Check network configuration and port forwarding

## Performance

- **Data Latency**: < 10ms typical
- **LED Refresh Rate**: 30fps (33.33ms period)
- **Maximum LEDs**: 500 RGBW LEDs
- **Memory Usage**: ~50KB RAM for 500 LEDs

## License

This project is licensed under the GNU General Public License v3.0 (GPLv3). See the [LICENSE](LICENSE) file for more details.