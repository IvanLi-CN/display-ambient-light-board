#include "config.h"
#include <string.h>

// Firmware configuration data
// This will be placed in the firmware and can be modified by external tools
// We use a simple approach without custom linker sections

const char firmware_config_data[] =
    FIRMWARE_CONFIG_MARKER
    // Default configuration template (will be replaced by tools)
    "\x78\x56\x34\x12"  // magic (little endian)
    "\x01\x00\x00\x00"  // version
    "TEMPLATE_SSID\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"  // wifi_ssid[64]
    "TEMPLATE_PASS\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"  // wifi_password[64]
    "\x02\x5A"          // udp_port (23042, little endian)
    "board-rs\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"  // mdns_hostname[32]
    "\x04"              // led_pin
    "\xF4\x01"          // max_leds (500, little endian)
    "RGBW\0\0\0\0"      // led_order[8]
    "\x1E"              // led_refresh_rate (30)
    "\x01"              // breathing_enabled
    "\x14"              // breathing_base_r (20)
    "\x14"              // breathing_base_g (20)
    "\x32"              // breathing_base_b (50)
    "\x00"              // breathing_base_w (0)
    "\x1E"              // breathing_min_brightness (30)
    "\xB4"              // breathing_max_brightness (180)
    "\x02"              // breathing_step_size (2)
    "\x21\x00"          // breathing_timer_period_ms (33, little endian)
    // reserved[48] - all zeros
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\x00\x00\x00\x00"  // checksum (will be calculated by tool)
    FIRMWARE_CONFIG_MARKER_END;

// Symbols for accessing the configuration section
const uint8_t* firmware_config_section_start = (const uint8_t*)firmware_config_data;
const uint8_t* firmware_config_section_end = (const uint8_t*)firmware_config_data + sizeof(firmware_config_data);
