#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * System status colors for first LED
 */
typedef enum {
  LED_STATUS_INIT,                 // 系统初始化 - 白色
  LED_STATUS_WIFI_CONFIG_ERROR,    // WiFi配置异常 - 快闪红色
  LED_STATUS_WIFI_CONNECTING,      // WiFi连接中 - 蓝色
  LED_STATUS_WIFI_CONNECTED,       // WiFi连接成功 - 青色
  LED_STATUS_IP_REQUESTING,        // IP获取中 - 黄色
  LED_STATUS_IP_SUCCESS,           // IP获取成功 - 绿色
  LED_STATUS_IP_FAILED,            // IP获取失败 - 橙色
  LED_STATUS_NETWORK_READY,        // 网络就绪 - 绿色
  LED_STATUS_OPERATIONAL,          // 正常运行 - 紫色
  LED_STATUS_HOST_ONLINE_NO_DATA,  // 上位机在线但未发送数据 - 淡紫色
  LED_STATUS_WIFI_ERROR,           // WiFi错误 - 红色
  LED_STATUS_UDP_ERROR,            // UDP错误 - 橙色
  LED_STATUS_GENERAL_ERROR         // 一般错误 - 快闪红色
} led_status_t;

/**
 * LED breathing effect parameters
 */
typedef struct {
    bool enabled;
    uint8_t brightness;
    uint8_t direction;  // 0 = dimming, 1 = brightening
    uint32_t step_delay_ms;
    led_status_t status;  // 当前状态
    uint8_t status_r, status_g, status_b, status_w;  // 状态LED颜色
    uint8_t base_r, base_g, base_b, base_w;          // 基础呼吸颜色
} led_breathing_t;

/**
 * Initialize LED driver
 * @param data_pin GPIO pin for LED data
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_init(gpio_num_t data_pin);

/**
 * Update LED buffer with new data
 * @param offset Byte offset in LED buffer (not LED units)
 * @param data LED data in configured color order format
 * @param len Length of data in bytes
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_update_buffer(uint16_t offset, const uint8_t* data, size_t len);

/**
 * Transmit all LED data to the strip
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_transmit_all(void);

/**
 * Set all LEDs to a specific color
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param w White component (0-255)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_set_all(uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/**
 * Clear all LEDs (set to black)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_clear_all(void);

/**
 * Set breathing effect
 * @param enable Enable or disable breathing effect
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_set_breathing_effect(bool enable);

/**
 * Set system status for first LED
 * @param status System status to display
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_set_status(led_status_t status);

/**
 * Set base breathing color for all LEDs
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param w White component (0-255)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_set_breathing_color(uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/**
 * Get breathing effect status
 * @return true if breathing effect is enabled, false otherwise
 */
bool led_driver_is_breathing_enabled(void);

/**
 * Set mixed mode (breathing effect with LED data override)
 * @param enable Enable or disable mixed mode
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_set_mixed_mode(bool enable);

/**
 * Set LED count
 * @param count Number of LEDs in the strip
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_set_led_count(uint16_t count);

/**
 * Get LED count
 * @return Number of LEDs in the strip
 */
uint16_t led_driver_get_led_count(void);

/**
 * Get LED buffer pointer (for direct access)
 * @return Pointer to LED buffer
 */
uint8_t* led_driver_get_buffer(void);

/**
 * Get LED buffer size in bytes
 * @return Buffer size in bytes
 */
size_t led_driver_get_buffer_size(void);

/**
 * Check if transmission is in progress
 * @return true if transmitting, false otherwise
 */
bool led_driver_is_transmitting(void);

/**
 * Wait for transmission to complete
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout
 */
esp_err_t led_driver_wait_transmission_complete(uint32_t timeout_ms);

/**
 * Get transmission statistics
 * @param transmissions Pointer to store total transmissions
 * @param bytes_transmitted Pointer to store total bytes transmitted
 * @param last_transmission_time Pointer to store last transmission time
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_get_stats(uint32_t* transmissions, uint32_t* bytes_transmitted,
                              uint32_t* last_transmission_time);

/**
 * Reset transmission statistics
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_reset_stats(void);

/**
 * Deinitialize LED driver
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t led_driver_deinit(void);

#endif // LED_DRIVER_H
