#ifndef CONFIG_H
#define CONFIG_H

#include "esp_system.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

// Hardware Configuration - use sdkconfig values
#define LED_DATA_PIN            (gpio_num_t)CONFIG_LED_DATA_PIN
#define MAX_LED_COUNT           CONFIG_MAX_LED_COUNT
#define LED_CHANNELS_PER_LED    4  // RGBW

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
