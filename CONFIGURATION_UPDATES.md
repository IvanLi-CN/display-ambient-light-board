# LED Strip Configuration Updates

This document describes important updates to the ambient light board LED strip configuration system.

## Update Overview

### 1. LED Strip Color Channel Configuration (Smart String Input)

Added intelligent LED color channel configuration in `main/Kconfig.projbuild`:

#### LED Color Channel Order String
Users only need to input the color channel string, and the system automatically calculates the channel count without manual configuration.

**Common Configurations**:
- `"RGB"`: Red-Green-Blue (auto-detected as 3 channels, suitable for some WS2812B)
- `"GRB"`: Green-Red-Blue (auto-detected as 3 channels, standard configuration for most WS2812B)
- `"RGBW"`: Red-Green-Blue-White (auto-detected as 4 channels, suitable for some SK6812)
- `"GRBW"`: Green-Red-Blue-White (auto-detected as 4 channels, standard configuration for most SK6812, default)

**Smart Features**:
- System automatically calculates LED channel count based on string length
- Compile-time validation of configuration correctness
- Runtime dynamic parsing of color channel mapping
- No need for manual channel count setting, avoiding configuration errors

**How to Determine Your LED Strip Order**:
1. Check LED strip datasheet or manufacturer specifications
2. Test method: Set RGB(255,0,0) pure red, observe actual displayed color
3. If red is displayed, your strip likely uses RGB order
4. If green is displayed, your strip likely uses GRB order
5. For RGBW strips, test the white channel separately

**Important Note**: For mixed LED strips, RGB order is recommended for better compatibility.

### 2. Breathing Effect Configuration Enhancement (Compile-time Hex Parsing)

#### New Configuration Items
- `BREATHING_BASE_COLOR_HEX`: Hexadecimal color string (e.g., "143250")
- `BREATHING_MIN_BRIGHTNESS`: Breathing effect minimum brightness (default 30)
- `BREATHING_MAX_BRIGHTNESS`: Breathing effect maximum brightness (default 180)
- `BREATHING_STEP_SIZE`: Breathing effect brightness change step size (default 2)
- `BREATHING_TIMER_PERIOD_MS`: Breathing effect timer period (default 33ms, ~30Hz)

#### Compile-time Color Parsing
- **Performance Optimization**: Hexadecimal colors are parsed into RGBW components at compile time with no runtime overhead
- **Automatic Validation**: Compile-time checking of color string length matching LED channel configuration
- **Smart Mapping**: Automatic mapping to correct RGBW components based on color channel order

#### Breathing Effect Behavior Improvements
- Starts from brightness 0 and increases on power-up
- Uses configured minimum/maximum brightness range
- 30Hz refresh rate provides smoother breathing effect

### 3. Extended System Status Definitions

Added more detailed system states and corresponding LED indicator colors:

| Status | Description | LED Color |
|--------|-------------|-----------|
| `LED_STATUS_INIT` | System initialization | White |
| `LED_STATUS_WIFI_CONFIG_ERROR` | WiFi configuration error | Red |
| `LED_STATUS_WIFI_CONNECTING` | WiFi connecting | Blue |
| `LED_STATUS_WIFI_CONNECTED` | WiFi connected | Cyan |
| `LED_STATUS_IP_REQUESTING` | IP requesting | Yellow |
| `LED_STATUS_IP_SUCCESS` | IP obtained successfully | Green |
| `LED_STATUS_IP_FAILED` | IP acquisition failed | Orange |
| `LED_STATUS_NETWORK_READY` | Network ready | Green |
| `LED_STATUS_OPERATIONAL` | Normal operation | Purple |
| `LED_STATUS_HOST_ONLINE_NO_DATA` | Host online but no data sent | Light purple |
| `LED_STATUS_WIFI_ERROR` | WiFi error | Red |
| `LED_STATUS_UDP_ERROR` | UDP error | Orange |
| `LED_STATUS_GENERAL_ERROR` | General error | Red |

### 4. Code Architecture Improvements

#### New Functions
- `set_led_color()`: Set LED color based on configured color channel order
- Support for dynamic color channel mapping, compatible with different types of LED strips

#### Configuration File Updates
- `main/config.h`: Removed hardcoded LED channel count, using sdkconfig configuration
- `sdkconfig`: Updated default configuration to support new features

## Usage Instructions

### Configure LED Strip Color Channels

1. Run `idf.py menuconfig`
2. Navigate to "Ambient Light Board Configuration"
3. Find "LED Color Channel Order" configuration item
4. Enter your LED strip color channel order string (system will automatically calculate channel count)

### Common Configuration Examples

#### WS2812B Strip (GRB Order)
```
CONFIG_LED_COLOR_ORDER_STRING="GRB"
CONFIG_BREATHING_BASE_COLOR_HEX="143250"  # Compile-time parsed as RGB(20,50,80)
# Channel count automatically calculated as 3
```

#### SK6812 Strip (GRBW Order)
```
CONFIG_LED_COLOR_ORDER_STRING="GRBW"
CONFIG_BREATHING_BASE_COLOR_HEX="14325000"  # Compile-time parsed as GRBW(20,50,80,0)
# Channel count automatically calculated as 4
```

#### Custom Configuration Example
```
CONFIG_LED_COLOR_ORDER_STRING="WRGB"
CONFIG_BREATHING_BASE_COLOR_HEX="00143250"  # Compile-time parsed as WRGB(0,20,50,80)
# Channel count automatically calculated as 4
```

#### Testing and Verification
You can verify the configuration is correct by:
1. Set pure red RGB(255,0,0), observe if red is displayed
2. Set pure green RGB(0,255,0), observe if green is displayed
3. Set pure blue RGB(0,0,255), observe if blue is displayed
4. For RGBW strips, test the white channel

### Automation Features
- **Smart Channel Calculation**: System automatically determines LED channel count based on string length
- **Compile-time Validation**: Configuration errors are detected at compile time
- **Runtime Parsing**: Dynamic parsing of color channel mapping, supporting any reasonable combination
- **Error Messages**: Detailed log information to help debug configuration issues

### Breathing Effect Adjustment

You can adjust the breathing effect through the following configuration items:
- Adjust `BREATHING_MIN_BRIGHTNESS` and `BREATHING_MAX_BRIGHTNESS` to change brightness range
- Adjust `BREATHING_STEP_SIZE` to change breathing speed
- Adjust `BREATHING_TIMER_PERIOD_MS` to change refresh frequency

## Compatibility Notes

- All changes are backward compatible with existing configurations
- Default configuration remains SK6812 GRBW mode
- For mixed LED strips, RGB configuration is recommended for best compatibility

## Build and Deploy

After updating configuration, rebuild and flash the firmware:

```bash
idf.py build
idf.py flash
```

Build successful, all new features have been integrated into the system.
