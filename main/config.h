#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_system.h"
#include "sdkconfig.h"

// Firmware configuration magic numbers and constants
#define FIRMWARE_CONFIG_MAGIC 0x12345678
#define FIRMWARE_CONFIG_VERSION 1
#define FIRMWARE_CONFIG_SIZE 256
#define FIRMWARE_CONFIG_MARKER "FWCFG_START"
#define FIRMWARE_CONFIG_MARKER_END "FWCFG_END"

// Configuration structure (exactly 256 bytes)
typedef struct {
  uint32_t magic;                      // Magic number for validation
  uint32_t version;                    // Configuration version
  char wifi_ssid[64];                  // WiFi SSID
  char wifi_password[64];              // WiFi password
  uint16_t udp_port;                   // UDP server port
  char mdns_hostname[32];              // mDNS hostname
  uint8_t led_pin;                     // LED data GPIO pin
  uint16_t max_leds;                   // Maximum LED count
  char led_order[8];                   // LED color order (RGB, RGBW, etc.)
  uint8_t led_refresh_rate;            // LED refresh rate in FPS
  uint8_t breathing_enabled;           // Breathing effect enabled
  uint8_t breathing_base_r;            // Breathing base color R
  uint8_t breathing_base_g;            // Breathing base color G
  uint8_t breathing_base_b;            // Breathing base color B
  uint8_t breathing_base_w;            // Breathing base color W
  uint8_t breathing_min_brightness;    // Breathing minimum brightness
  uint8_t breathing_max_brightness;    // Breathing maximum brightness
  uint8_t breathing_step_size;         // Breathing step size
  uint16_t breathing_timer_period_ms;  // Breathing timer period
  uint8_t reserved[48];                // Reserved for future use
  uint32_t checksum;                   // CRC32 checksum
} __attribute__((packed)) firmware_config_t;

// Global configuration instance
extern firmware_config_t g_firmware_config;
extern bool g_config_loaded;

// Configuration management functions
esp_err_t config_init(void);
esp_err_t config_load_from_firmware(void);
bool config_is_valid(const firmware_config_t* config);
uint32_t config_calculate_checksum(const firmware_config_t* config);
void config_set_defaults(firmware_config_t* config);
const char* config_get_wifi_ssid(void);
const char* config_get_wifi_password(void);
uint16_t config_get_udp_port(void);
const char* config_get_mdns_hostname(void);
uint8_t config_get_led_pin(void);
uint16_t config_get_max_leds(void);
const char* config_get_led_order(void);

// Hardware Configuration - use sdkconfig values
#define LED_DATA_PIN            (gpio_num_t)CONFIG_LED_DATA_PIN
#define MAX_LED_COUNT           CONFIG_MAX_LED_COUNT
// LED_CHANNELS_PER_LED is now defined from sdkconfig

// Network Configuration - use sdkconfig values
#define UDP_PORT                CONFIG_UDP_PORT
#define MDNS_SERVICE_NAME       "_ambient_light"
#define MDNS_PROTOCOL           "_udp"
#define MDNS_HOSTNAME           CONFIG_MDNS_HOSTNAME
#define MDNS_ANNOUNCE_INTERVAL  30000  // 30 seconds in ms

// Protocol Configuration
#define PACKET_TYPE_PING        0x01
#define PACKET_TYPE_LED_DATA    0x02
#define PACKET_TYPE_IGNORE_1    0x03
#define PACKET_TYPE_IGNORE_2    0x04
#define MAX_PACKET_SIZE         4096
#define LED_DATA_HEADER_SIZE    3  // Type + Offset (2 bytes)

// Performance Configuration - use sdkconfig values
#define LED_REFRESH_RATE_FPS    CONFIG_LED_REFRESH_RATE_FPS
#define LED_REFRESH_PERIOD_MS   (1000 / LED_REFRESH_RATE_FPS)
#define MAX_DATA_LATENCY_MS     10
#define UDP_RECEIVE_TIMEOUT_MS  100

// RMT Configuration for SK6812
#define RMT_CHANNEL             RMT_CHANNEL_0
#define RMT_CLK_DIV             8  // 80MHz / 8 = 10MHz
#define RMT_TICK_DURATION_NS    100  // 1 tick = 100ns at 10MHz

// SK6812 Timing (in RMT ticks at 10MHz)
#define SK6812_T1H_TICKS        6   // 1-bit high time: 600ns
#define SK6812_T1L_TICKS        6   // 1-bit low time: 600ns
#define SK6812_T0H_TICKS        3   // 0-bit high time: 300ns
#define SK6812_T0L_TICKS        9   // 0-bit low time: 900ns
#define SK6812_RESET_TICKS      800 // Reset pulse: 80us

// LED Configuration - channels calculated dynamically from color order string
// Helper macro to get string length at compile time
#define STRLEN_CONST(s) (sizeof(s) - 1)
#define LED_CHANNELS_PER_LED STRLEN_CONST(CONFIG_LED_COLOR_ORDER_STRING)

// Compile-time hex color parsing macros
#define HEX_CHAR_TO_INT(c)                     \
  ((c) >= '0' && (c) <= '9'   ? (c) - '0'      \
   : (c) >= 'A' && (c) <= 'F' ? (c) - 'A' + 10 \
   : (c) >= 'a' && (c) <= 'f' ? (c) - 'a' + 10 \
                              : 0)

#define HEX_BYTE_TO_INT(h, l) ((HEX_CHAR_TO_INT(h) << 4) | HEX_CHAR_TO_INT(l))

// Extract color components from hex string based on color order
// These macros work at compile time for optimal performance
#define GET_COLOR_COMPONENT(hex_str, color_order, channel, pos)                \
  (((color_order)[pos] == (channel) || (color_order)[pos] == ((channel) + 32)) \
       ? HEX_BYTE_TO_INT((hex_str)[pos * 2], (hex_str)[pos * 2 + 1])           \
       : 0)

// Compile-time color extraction for different channel positions
#define EXTRACT_R_COMPONENT(hex_str, color_order)      \
  (GET_COLOR_COMPONENT(hex_str, color_order, 'R', 0) + \
   GET_COLOR_COMPONENT(hex_str, color_order, 'R', 1) + \
   GET_COLOR_COMPONENT(hex_str, color_order, 'R', 2) + \
   GET_COLOR_COMPONENT(hex_str, color_order, 'R', 3))

#define EXTRACT_G_COMPONENT(hex_str, color_order)      \
  (GET_COLOR_COMPONENT(hex_str, color_order, 'G', 0) + \
   GET_COLOR_COMPONENT(hex_str, color_order, 'G', 1) + \
   GET_COLOR_COMPONENT(hex_str, color_order, 'G', 2) + \
   GET_COLOR_COMPONENT(hex_str, color_order, 'G', 3))

#define EXTRACT_B_COMPONENT(hex_str, color_order)      \
  (GET_COLOR_COMPONENT(hex_str, color_order, 'B', 0) + \
   GET_COLOR_COMPONENT(hex_str, color_order, 'B', 1) + \
   GET_COLOR_COMPONENT(hex_str, color_order, 'B', 2) + \
   GET_COLOR_COMPONENT(hex_str, color_order, 'B', 3))

#define EXTRACT_W_COMPONENT(hex_str, color_order)      \
  (GET_COLOR_COMPONENT(hex_str, color_order, 'W', 0) + \
   GET_COLOR_COMPONENT(hex_str, color_order, 'W', 1) + \
   GET_COLOR_COMPONENT(hex_str, color_order, 'W', 2) + \
   GET_COLOR_COMPONENT(hex_str, color_order, 'W', 3))

// Final compile-time color component definitions
#define BREATHING_BASE_R                               \
  EXTRACT_R_COMPONENT(CONFIG_BREATHING_BASE_COLOR_HEX, \
                      CONFIG_LED_COLOR_ORDER_STRING)
#define BREATHING_BASE_G                               \
  EXTRACT_G_COMPONENT(CONFIG_BREATHING_BASE_COLOR_HEX, \
                      CONFIG_LED_COLOR_ORDER_STRING)
#define BREATHING_BASE_B                               \
  EXTRACT_B_COMPONENT(CONFIG_BREATHING_BASE_COLOR_HEX, \
                      CONFIG_LED_COLOR_ORDER_STRING)
#define BREATHING_BASE_W                               \
  EXTRACT_W_COMPONENT(CONFIG_BREATHING_BASE_COLOR_HEX, \
                      CONFIG_LED_COLOR_ORDER_STRING)

// Memory Configuration
#define LED_BUFFER_SIZE         (MAX_LED_COUNT * LED_CHANNELS_PER_LED)
#define UDP_BUFFER_SIZE         MAX_PACKET_SIZE
#define RMT_BUFFER_SIZE         (LED_BUFFER_SIZE * 8 * 2)  // 8 bits per byte, 2 items per bit

// WiFi Configuration - use sdkconfig values
#define WIFI_MAXIMUM_RETRY      CONFIG_WIFI_MAXIMUM_RETRY
#define WIFI_RETRY_DELAY_MS     5000
#define DHCP_TIMEOUT_MS         30000

// State Machine Timeouts
#define STATE_TIMEOUT_WIFI_MS       30000
#define STATE_TIMEOUT_DHCP_MS       30000
#define STATE_TIMEOUT_UDP_MS        5000
#define STATE_TIMEOUT_PING_MS       60000  // Timeout if no ping received

// Debug Configuration - use sdkconfig values
#define DEBUG_ENABLE_WIFI       CONFIG_DEBUG_ENABLE_WIFI
#define DEBUG_ENABLE_UDP        CONFIG_DEBUG_ENABLE_UDP
#define DEBUG_ENABLE_LED        CONFIG_DEBUG_ENABLE_LED
#define DEBUG_ENABLE_STATE      CONFIG_DEBUG_ENABLE_STATE

// Error Codes
#define ERR_WIFI_CONNECT        -1
#define ERR_DHCP_TIMEOUT        -2
#define ERR_UDP_BIND            -3
#define ERR_LED_INIT            -4
#define ERR_MDNS_INIT           -5

#endif // CONFIG_H
