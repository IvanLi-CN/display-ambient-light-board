#include "led_driver.h"
#include "config.h"
#include "esp_log.h"
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "LED_DRIVER";

// Global variables
static uint8_t* g_led_buffer = NULL;
static uint16_t g_led_count = MAX_LED_COUNT;
static size_t g_buffer_size = 0;
static gpio_num_t g_data_pin = LED_DATA_PIN;
static bool g_initialized = false;
static bool g_transmitting = false;
static SemaphoreHandle_t g_transmission_semaphore = NULL;

// Breathing effect
static led_breathing_t g_breathing = {0};
static TimerHandle_t g_breathing_timer = NULL;
static bool g_mixed_mode = false;  // Mixed mode: breathing + LED data

// Statistics
static struct {
    uint32_t transmissions;
    uint32_t bytes_transmitted;
    uint32_t last_transmission_time;
} g_stats = {0};

// RMT items for SK6812 timing
static rmt_item32_t g_bit_1 = {
    .level0 = 1,
    .duration0 = SK6812_T1H_TICKS,
    .level1 = 0,
    .duration1 = SK6812_T1L_TICKS
};

static rmt_item32_t g_bit_0 = {
    .level0 = 1,
    .duration0 = SK6812_T0H_TICKS,
    .level1 = 0,
    .duration1 = SK6812_T0L_TICKS
};

static rmt_item32_t g_reset = {
    .level0 = 0,
    .duration0 = SK6812_RESET_TICKS,
    .level1 = 0,
    .duration1 = 0
};

/**
 * RMT transmission complete callback
 */
static void rmt_tx_done_callback(rmt_channel_t channel, void* arg)
{
    g_transmitting = false;
    if (g_transmission_semaphore) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(g_transmission_semaphore, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

/**
 * Convert LED data to RMT items
 */
static size_t led_data_to_rmt_items(const uint8_t* led_data, size_t led_data_len, 
                                   rmt_item32_t* rmt_items, size_t max_items)
{
    size_t item_count = 0;
    
    for (size_t i = 0; i < led_data_len && item_count < max_items - 1; i++) {
        uint8_t byte = led_data[i];
        
        // Convert each bit to RMT item (MSB first)
        for (int bit = 7; bit >= 0 && item_count < max_items - 1; bit--) {
            if (byte & (1 << bit)) {
                rmt_items[item_count] = g_bit_1;
            } else {
                rmt_items[item_count] = g_bit_0;
            }
            item_count++;
        }
    }
    
    // Add reset pulse
    if (item_count < max_items) {
        rmt_items[item_count] = g_reset;
        item_count++;
    }
    
    return item_count;
}

/**
 * Get the actual number of LED channels from color order string
 */
static int get_led_channels_count(void) {
  const char* color_order = CONFIG_LED_COLOR_ORDER_STRING;
  return strlen(color_order);
}

/**
 * Set LED color based on configured channel order
 */
static void set_led_color(uint8_t* buffer, size_t offset, uint8_t r, uint8_t g,
                          uint8_t b, uint8_t w) {
  const char* color_order = CONFIG_LED_COLOR_ORDER_STRING;
  int channels = get_led_channels_count();

  for (int i = 0; i < channels && i < 4; i++) {
    switch (color_order[i]) {
      case 'R':
      case 'r':
        buffer[offset + i] = r;
        break;
      case 'G':
      case 'g':
        buffer[offset + i] = g;
        break;
      case 'B':
      case 'b':
        buffer[offset + i] = b;
        break;
      case 'W':
      case 'w':
        buffer[offset + i] = w;
        break;
      default:
        buffer[offset + i] = 0;
        ESP_LOGW(TAG, "Unknown color channel '%c' in position %d",
                 color_order[i], i);
        break;
    }
  }
}

/**
 * Get status color based on current status
 */
static void get_status_color(led_status_t status, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* w)
{
    switch (status) {
        case LED_STATUS_INIT:
            *r = 0; *g = 0; *b = 0; *w = 255;  // 白色 - 系统初始化
            break;
        case LED_STATUS_WIFI_CONFIG_ERROR:
          *r = 255;
          *g = 0;
          *b = 0;
          *w = 0;  // 红色 - WiFi配置异常
          break;
        case LED_STATUS_WIFI_CONNECTING:
            *r = 0; *g = 0; *b = 255; *w = 0;  // 蓝色 - WiFi连接中
            break;
        case LED_STATUS_WIFI_CONNECTED:
          *r = 0;
          *g = 255;
          *b = 255;
          *w = 0;  // 青色 - WiFi连接成功
          break;
        case LED_STATUS_IP_REQUESTING:
          *r = 255;
          *g = 255;
          *b = 0;
          *w = 0;  // 黄色 - IP获取中
          break;
        case LED_STATUS_IP_SUCCESS:
          *r = 0;
          *g = 255;
          *b = 0;
          *w = 0;  // 绿色 - IP获取成功
          break;
        case LED_STATUS_IP_FAILED:
          *r = 255;
          *g = 128;
          *b = 0;
          *w = 0;  // 橙色 - IP获取失败
          break;
        case LED_STATUS_NETWORK_READY:
            *r = 0; *g = 255; *b = 0; *w = 0;  // 绿色 - 网络就绪
            break;
        case LED_STATUS_OPERATIONAL:
            *r = 128; *g = 0; *b = 128; *w = 0; // 紫色 - 正常运行
            break;
        case LED_STATUS_HOST_ONLINE_NO_DATA:
          *r = 64;
          *g = 0;
          *b = 64;
          *w = 0;  // 淡紫色 - 上位机在线但未发送数据
          break;
        case LED_STATUS_WIFI_ERROR:
            *r = 255; *g = 0; *b = 0; *w = 0;  // 红色 - WiFi错误
            break;
        case LED_STATUS_UDP_ERROR:
            *r = 255; *g = 128; *b = 0; *w = 0; // 橙色 - UDP错误
            break;
        case LED_STATUS_GENERAL_ERROR:
            *r = 255; *g = 0; *b = 0; *w = 0;  // 红色 - 一般错误
            break;
        default:
            *r = 0; *g = 0; *b = 0; *w = 0;    // 关闭
            break;
    }
}

/**
 * Breathing effect timer callback
 */
static void breathing_timer_callback(TimerHandle_t xTimer)
{
    if (!g_breathing.enabled || !g_led_buffer || g_buffer_size < 4) {
        return;
    }

    // Additional safety check: ensure status colors are initialized
    if (g_breathing.status_r == 0 && g_breathing.status_g == 0 &&
        g_breathing.status_b == 0 && g_breathing.status_w == 0) {
        ESP_LOGW(TAG, "Status colors not initialized, skipping breathing update");
        return;
    }

    // Update breathing brightness
    if (g_breathing.direction == 1) {
        // Brightening
        if (g_breathing.brightness < CONFIG_BREATHING_MAX_BRIGHTNESS) {
          g_breathing.brightness += CONFIG_BREATHING_STEP_SIZE;
        } else {
          g_breathing.direction = 0;
        }
    } else {
        // Dimming
        if (g_breathing.brightness > CONFIG_BREATHING_MIN_BRIGHTNESS) {
          g_breathing.brightness -= CONFIG_BREATHING_STEP_SIZE;
        } else {
          g_breathing.direction = 1;
        }
    }

    // Calculate brightness factor (0.0 to 1.0)
    float brightness_factor =
        (float)g_breathing.brightness / (float)CONFIG_BREATHING_MAX_BRIGHTNESS;

    // Apply breathing effect
    // Add bounds checking to prevent buffer overflow
    int actual_channels = get_led_channels_count();
    size_t max_leds = g_buffer_size / actual_channels;

    if (g_mixed_mode) {
      // Mixed mode: Don't update any LEDs, let ambient data control all LEDs
      // The breathing effect is disabled in mixed mode to allow full ambient
      // control
      return;
    } else {
        // Full breathing mode: Update ALL LEDs
        for (size_t led_idx = 0; led_idx < max_leds; led_idx++) {
          size_t i = led_idx * actual_channels;

          // Double check bounds
          if (i + 3 >= g_buffer_size) {
            ESP_LOGW(TAG, "LED buffer bounds exceeded at index %zu", i);
            break;
          }

            if (led_idx == 0) {
                // First LED: Status indicator with breathing
                uint8_t r = (uint8_t)(g_breathing.status_r * brightness_factor);
                uint8_t g = (uint8_t)(g_breathing.status_g * brightness_factor);
                uint8_t b = (uint8_t)(g_breathing.status_b * brightness_factor);
                uint8_t w = (uint8_t)(g_breathing.status_w * brightness_factor);
                set_led_color(g_led_buffer, i, r, g, b, w);
            } else {
                // Other LEDs: Base breathing color
                uint8_t r = (uint8_t)(g_breathing.base_r * brightness_factor);
                uint8_t g = (uint8_t)(g_breathing.base_g * brightness_factor);
                uint8_t b = (uint8_t)(g_breathing.base_b * brightness_factor);
                uint8_t w = (uint8_t)(g_breathing.base_w * brightness_factor);
                set_led_color(g_led_buffer, i, r, g, b, w);
            }
        }
    }

    // Transmit the change
    led_driver_transmit_all();
}

esp_err_t led_driver_init(gpio_num_t data_pin)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "LED driver already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing LED driver on GPIO %d", data_pin);

    // Get actual channel count from color order string
    int actual_channels = get_led_channels_count();
    ESP_LOGI(TAG, "LED color order: %s (%d channels per LED)",
             CONFIG_LED_COLOR_ORDER_STRING, actual_channels);

    g_data_pin = data_pin;
    g_buffer_size = g_led_count * actual_channels;

    // Allocate LED buffer
    g_led_buffer = malloc(g_buffer_size);
    if (!g_led_buffer) {
        ESP_LOGE(TAG, "Failed to allocate LED buffer (%" PRIu32 " bytes)", (uint32_t)g_buffer_size);
        return ESP_ERR_NO_MEM;
    }
    
    // Clear buffer
    memset(g_led_buffer, 0, g_buffer_size);

    // Configure RMT
    rmt_config_t rmt_cfg = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_CHANNEL,
        .gpio_num = data_pin,
        .clk_div = RMT_CLK_DIV,
        .mem_block_num = 1,
        .tx_config = {
            .loop_en = false,
            .carrier_en = false,
            .idle_output_en = true,
            .idle_level = RMT_IDLE_LEVEL_LOW,
        }
    };

    esp_err_t ret = rmt_config(&rmt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure RMT: %s", esp_err_to_name(ret));
        free(g_led_buffer);
        g_led_buffer = NULL;
        return ret;
    }
    
    // Install RMT driver
    ret = rmt_driver_install(RMT_CHANNEL, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install RMT driver: %s", esp_err_to_name(ret));
        free(g_led_buffer);
        g_led_buffer = NULL;
        return ret;
    }
    
    // Register transmission complete callback
    rmt_register_tx_end_callback(rmt_tx_done_callback, NULL);
    
    // Create transmission semaphore
    g_transmission_semaphore = xSemaphoreCreateBinary();
    if (!g_transmission_semaphore) {
        ESP_LOGE(TAG, "Failed to create transmission semaphore");
        rmt_driver_uninstall(RMT_CHANNEL);
        free(g_led_buffer);
        g_led_buffer = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    // Create breathing timer
    g_breathing_timer = xTimerCreate(
        "led_breathing",
        pdMS_TO_TICKS(
            CONFIG_BREATHING_TIMER_PERIOD_MS),  // Use configured period
        pdTRUE,                                 // Auto-reload
        NULL, breathing_timer_callback);

    if (!g_breathing_timer) {
        ESP_LOGE(TAG, "Failed to create breathing timer");
        vSemaphoreDelete(g_transmission_semaphore);
        rmt_driver_uninstall(RMT_CHANNEL);
        free(g_led_buffer);
        g_led_buffer = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    g_initialized = true;
    ESP_LOGI(TAG, "LED driver initialized: %d LEDs, %" PRIu32 " bytes buffer", g_led_count, (uint32_t)g_buffer_size);

    // Immediately clear all physical LEDs to prevent random colors from showing
    ESP_LOGI(TAG, "Clearing all LEDs on initialization");
    led_driver_transmit_all();

    return ESP_OK;
}

esp_err_t led_driver_update_buffer(uint16_t offset, const uint8_t* data, size_t len)
{
    if (!g_initialized || !g_led_buffer) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // offset is already a byte offset according to protocol specification
    size_t byte_offset = offset;

    // Check bounds
    if (byte_offset + len > g_buffer_size) {
      ESP_LOGW(
          TAG,
          "LED data exceeds buffer: byte_offset=%d, len=%d, buffer_size=%d",
          byte_offset, len, g_buffer_size);
      len = g_buffer_size - byte_offset;
    }

    if (len > 0) {
      // In mixed mode, all LEDs including the first one should display ambient
      // data
      memcpy(g_led_buffer + byte_offset, data, len);
      ESP_LOGD(TAG, "Updated LED buffer: byte_offset=%d, len=%" PRIu32,
               byte_offset, (uint32_t)len);
    }

    return ESP_OK;
}

esp_err_t led_driver_transmit_all(void)
{
    if (!g_initialized || !g_led_buffer) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_transmitting) {
        ESP_LOGW(TAG, "Transmission already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Transmitting %d LEDs (%" PRIu32 " bytes)", g_led_count, (uint32_t)g_buffer_size);
    
    // Allocate RMT items (8 bits per byte + reset)
    size_t rmt_item_count = g_buffer_size * 8 + 1;
    rmt_item32_t* rmt_items = malloc(rmt_item_count * sizeof(rmt_item32_t));
    if (!rmt_items) {
        ESP_LOGE(TAG, "Failed to allocate RMT items");
        return ESP_ERR_NO_MEM;
    }
    
    // Convert LED data to RMT items
    size_t actual_items = led_data_to_rmt_items(g_led_buffer, g_buffer_size, 
                                               rmt_items, rmt_item_count);
    
    g_transmitting = true;
    
    // Transmit via RMT
    esp_err_t ret = rmt_write_items(RMT_CHANNEL, rmt_items, actual_items, false);
    
    free(rmt_items);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit RMT items: %s", esp_err_to_name(ret));
        g_transmitting = false;
        return ret;
    }
    
    // Update statistics
    g_stats.transmissions++;
    g_stats.bytes_transmitted += g_buffer_size;
    g_stats.last_transmission_time = xTaskGetTickCount();
    
    return ESP_OK;
}

esp_err_t led_driver_set_all(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!g_initialized || !g_led_buffer) {
        return ESP_ERR_INVALID_STATE;
    }

    // Set all LEDs to the specified color using configured channel order
    int actual_channels = get_led_channels_count();
    for (size_t i = 0; i < g_buffer_size; i += actual_channels) {
      set_led_color(g_led_buffer, i, r, g, b, w);
    }

    ESP_LOGI(TAG, "Set all LEDs to RGBW(%d,%d,%d,%d)", r, g, b, w);
    return ESP_OK;
}

esp_err_t led_driver_clear_all(void)
{
    if (!g_initialized || !g_led_buffer) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(g_led_buffer, 0, g_buffer_size);
    ESP_LOGI(TAG, "Cleared all LEDs");
    return ESP_OK;
}

esp_err_t led_driver_set_breathing_effect(bool enable)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_breathing.enabled == enable) {
        return ESP_OK;  // No change needed
    }

    g_breathing.enabled = enable;

    if (enable) {
        // Initialize breathing parameters
        g_breathing.brightness = 0;       // Start from 0 on power-up
        g_breathing.direction = 1;        // Start brightening
        g_breathing.step_delay_ms =
            CONFIG_BREATHING_TIMER_PERIOD_MS;  // Use configured timer period

        // Set default status (will be updated by state machine)
        g_breathing.status = LED_STATUS_INIT;
        get_status_color(g_breathing.status, &g_breathing.status_r, &g_breathing.status_g,
                        &g_breathing.status_b, &g_breathing.status_w);

        // Set default base breathing color from compile-time parsed hex
        // configuration
        g_breathing.base_r = BREATHING_BASE_R;
        g_breathing.base_g = BREATHING_BASE_G;
        g_breathing.base_b = BREATHING_BASE_B;
        g_breathing.base_w = BREATHING_BASE_W;

        ESP_LOGI(TAG, "Base breathing color from hex '%s': RGBW(%d,%d,%d,%d)",
                 CONFIG_BREATHING_BASE_COLOR_HEX, g_breathing.base_r,
                 g_breathing.base_g, g_breathing.base_b, g_breathing.base_w);

        // Clear all LEDs first and transmit to ensure clean state
        if (g_led_buffer && g_buffer_size > 0) {
            memset(g_led_buffer, 0, g_buffer_size);
            led_driver_transmit_all();  // Immediately clear physical LEDs
            vTaskDelay(pdMS_TO_TICKS(100));  // Give more time for transmission to complete
        }

        // Start breathing timer with a small delay to ensure all initialization is complete
        if (g_breathing_timer) {
            ESP_LOGI(TAG, "Starting breathing timer with initial delay");
            xTimerStart(g_breathing_timer, pdMS_TO_TICKS(200));  // 200ms delay before starting
        }
        ESP_LOGI(TAG, "Breathing effect enabled (all LEDs with status indicator)");
    } else {
        if (g_breathing_timer) {
            xTimerStop(g_breathing_timer, 0);
        }

        // Clear all LEDs when disabled
        if (g_led_buffer && g_buffer_size > 0) {
            memset(g_led_buffer, 0, g_buffer_size);
            led_driver_transmit_all();
        }

        ESP_LOGI(TAG, "Breathing effect disabled");
    }

    return ESP_OK;
}

esp_err_t led_driver_set_status(led_status_t status)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    g_breathing.status = status;
    get_status_color(status, &g_breathing.status_r, &g_breathing.status_g,
                    &g_breathing.status_b, &g_breathing.status_w);

    const char* status_names[] = {"INIT",
                                  "WIFI_CONFIG_ERROR",
                                  "WIFI_CONNECTING",
                                  "WIFI_CONNECTED",
                                  "IP_REQUESTING",
                                  "IP_SUCCESS",
                                  "IP_FAILED",
                                  "NETWORK_READY",
                                  "OPERATIONAL",
                                  "HOST_ONLINE_NO_DATA",
                                  "WIFI_ERROR",
                                  "UDP_ERROR",
                                  "GENERAL_ERROR"};

    if (status < sizeof(status_names) / sizeof(status_names[0])) {
        ESP_LOGI(TAG, "Status LED set to: %s (R:%d G:%d B:%d W:%d)",
                 status_names[status], g_breathing.status_r, g_breathing.status_g,
                 g_breathing.status_b, g_breathing.status_w);
    }

    return ESP_OK;
}

esp_err_t led_driver_set_breathing_color(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    g_breathing.base_r = r;
    g_breathing.base_g = g;
    g_breathing.base_b = b;
    g_breathing.base_w = w;

    ESP_LOGI(TAG, "Base breathing color set to RGBW(%d,%d,%d,%d)", r, g, b, w);

    return ESP_OK;
}

bool led_driver_is_breathing_enabled(void)
{
    return g_breathing.enabled;
}

esp_err_t led_driver_set_mixed_mode(bool enable)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    g_mixed_mode = enable;
    ESP_LOGI(TAG, "Mixed mode %s", enable ? "enabled" : "disabled");

    return ESP_OK;
}

esp_err_t led_driver_set_led_count(uint16_t count)
{
    if (count > MAX_LED_COUNT) {
        ESP_LOGE(TAG, "LED count %d exceeds maximum %d", count, MAX_LED_COUNT);
        return ESP_ERR_INVALID_ARG;
    }

    g_led_count = count;
    int actual_channels = get_led_channels_count();
    size_t new_buffer_size = count * actual_channels;

    if (g_initialized && new_buffer_size != g_buffer_size) {
        // Reallocate buffer if size changed
        uint8_t* new_buffer = realloc(g_led_buffer, new_buffer_size);
        if (!new_buffer) {
            ESP_LOGE(TAG, "Failed to reallocate LED buffer");
            return ESP_ERR_NO_MEM;
        }

        g_led_buffer = new_buffer;

        // Clear new area if buffer grew
        if (new_buffer_size > g_buffer_size) {
            memset(g_led_buffer + g_buffer_size, 0, new_buffer_size - g_buffer_size);
        }

        g_buffer_size = new_buffer_size;
        ESP_LOGI(TAG, "LED count changed to %d (%" PRIu32 " bytes)", count, (uint32_t)g_buffer_size);
    }

    return ESP_OK;
}

uint16_t led_driver_get_led_count(void)
{
    return g_led_count;
}

uint8_t* led_driver_get_buffer(void)
{
    return g_led_buffer;
}

size_t led_driver_get_buffer_size(void)
{
    return g_buffer_size;
}

bool led_driver_is_transmitting(void)
{
    return g_transmitting;
}

esp_err_t led_driver_wait_transmission_complete(uint32_t timeout_ms)
{
    if (!g_transmitting) {
        return ESP_OK;
    }

    if (!g_transmission_semaphore) {
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t result = xSemaphoreTake(g_transmission_semaphore, pdMS_TO_TICKS(timeout_ms));
    return (result == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t led_driver_get_stats(uint32_t* transmissions, uint32_t* bytes_transmitted,
                              uint32_t* last_transmission_time)
{
    if (transmissions) *transmissions = g_stats.transmissions;
    if (bytes_transmitted) *bytes_transmitted = g_stats.bytes_transmitted;
    if (last_transmission_time) *last_transmission_time = g_stats.last_transmission_time;

    return ESP_OK;
}

esp_err_t led_driver_reset_stats(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
    ESP_LOGI(TAG, "LED driver statistics reset");
    return ESP_OK;
}

esp_err_t led_driver_deinit(void)
{
    if (!g_initialized) {
        ESP_LOGW(TAG, "LED driver not initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing LED driver");

    // Stop breathing effect
    led_driver_set_breathing_effect(false);

    // Delete timer
    if (g_breathing_timer) {
        xTimerDelete(g_breathing_timer, 0);
        g_breathing_timer = NULL;
    }

    // Delete semaphore
    if (g_transmission_semaphore) {
        vSemaphoreDelete(g_transmission_semaphore);
        g_transmission_semaphore = NULL;
    }

    // Uninstall RMT driver
    rmt_driver_uninstall(RMT_CHANNEL);

    // Free buffer
    if (g_led_buffer) {
        free(g_led_buffer);
        g_led_buffer = NULL;
    }

    g_initialized = false;
    g_transmitting = false;
    g_buffer_size = 0;
    memset(&g_breathing, 0, sizeof(g_breathing));
    memset(&g_stats, 0, sizeof(g_stats));

    ESP_LOGI(TAG, "LED driver deinitialized");
    return ESP_OK;
}
