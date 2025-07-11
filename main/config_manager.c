#include "config.h"
#include "esp_log.h"
#include "esp_crc.h"
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

static const char *TAG = "CONFIG_MANAGER";

// Global configuration instance
firmware_config_t g_firmware_config = {0};
bool g_config_loaded = false;

// External symbols for firmware configuration section
extern const uint8_t* firmware_config_section_start;
extern const uint8_t* firmware_config_section_end;

esp_err_t config_init(void)
{
    ESP_LOGI(TAG, "Initializing configuration manager");
    
    // Try to load configuration from firmware
    esp_err_t ret = config_load_from_firmware();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load firmware configuration, using defaults");
        config_set_defaults(&g_firmware_config);
        g_config_loaded = false;
    } else {
        ESP_LOGI(TAG, "Firmware configuration loaded successfully");
        g_config_loaded = true;
    }
    
    // Log configuration
    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  WiFi SSID: %s", g_firmware_config.wifi_ssid);
    ESP_LOGI(TAG, "  WiFi Password: %s", g_firmware_config.wifi_password[0] ? "***" : "(empty)");
    ESP_LOGI(TAG, "  UDP Port: %d", g_firmware_config.udp_port);
    ESP_LOGI(TAG, "  mDNS Hostname: %s", g_firmware_config.mdns_hostname);
    ESP_LOGI(TAG, "  LED Pin: %d", g_firmware_config.led_pin);
    ESP_LOGI(TAG, "  Max LEDs: %d", g_firmware_config.max_leds);
    ESP_LOGI(TAG, "  LED Order: %s", g_firmware_config.led_order);
    
    return ESP_OK;
}

esp_err_t config_load_from_firmware(void)
{
    ESP_LOGI(TAG, "Loading configuration from firmware");
    
    // Calculate expected section size
    size_t section_size = firmware_config_section_end - firmware_config_section_start;
    ESP_LOGI(TAG, "Firmware config section size: %zu bytes", section_size);
    
    if (section_size < sizeof(firmware_config_t)) {
        ESP_LOGE(TAG, "Firmware config section too small: %zu < %zu", 
                 section_size, sizeof(firmware_config_t));
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Find configuration marker
    const char* marker_pos = NULL;
    for (size_t i = 0; i <= section_size - strlen(FIRMWARE_CONFIG_MARKER); i++) {
        if (memcmp(firmware_config_section_start + i, FIRMWARE_CONFIG_MARKER, 
                   strlen(FIRMWARE_CONFIG_MARKER)) == 0) {
            marker_pos = (const char*)(firmware_config_section_start + i);
            break;
        }
    }
    
    if (!marker_pos) {
        ESP_LOGW(TAG, "Configuration marker not found in firmware");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Configuration starts after the marker
    const firmware_config_t* fw_config = (const firmware_config_t*)(marker_pos + strlen(FIRMWARE_CONFIG_MARKER));
    
    // Validate configuration
    if (!config_is_valid(fw_config)) {
        ESP_LOGE(TAG, "Invalid firmware configuration");
        return ESP_ERR_INVALID_CRC;
    }
    
    // Copy configuration
    memcpy(&g_firmware_config, fw_config, sizeof(firmware_config_t));
    
    ESP_LOGI(TAG, "Firmware configuration loaded and validated");
    return ESP_OK;
}

bool config_is_valid(const firmware_config_t* config)
{
    if (!config) {
        return false;
    }
    
    // Check magic number
    if (config->magic != FIRMWARE_CONFIG_MAGIC) {
        ESP_LOGE(TAG, "Invalid magic number: 0x%08" PRIx32 " (expected 0x%08x)", 
                 config->magic, FIRMWARE_CONFIG_MAGIC);
        return false;
    }
    
    // Check version
    if (config->version != FIRMWARE_CONFIG_VERSION) {
        ESP_LOGE(TAG, "Unsupported config version: %" PRIu32 " (expected %d)", 
                 config->version, FIRMWARE_CONFIG_VERSION);
        return false;
    }
    
    // Calculate and verify checksum
    uint32_t calculated_checksum = config_calculate_checksum(config);
    if (config->checksum != calculated_checksum) {
        ESP_LOGE(TAG, "Checksum mismatch: 0x%08" PRIx32 " != 0x%08" PRIx32, 
                 config->checksum, calculated_checksum);
        return false;
    }
    
    return true;
}

uint32_t config_calculate_checksum(const firmware_config_t* config)
{
    if (!config) {
        return 0;
    }
    
    // Calculate CRC32 of everything except the checksum field
    size_t data_size = sizeof(firmware_config_t) - sizeof(config->checksum);
    return esp_crc32_le(0, (const uint8_t*)config, data_size);
}

void config_set_defaults(firmware_config_t* config)
{
    if (!config) {
        return;
    }
    
    memset(config, 0, sizeof(firmware_config_t));
    
    config->magic = FIRMWARE_CONFIG_MAGIC;
    config->version = FIRMWARE_CONFIG_VERSION;
    
    // WiFi defaults
    strncpy(config->wifi_ssid, CONFIG_WIFI_SSID, sizeof(config->wifi_ssid) - 1);
    strncpy(config->wifi_password, CONFIG_WIFI_PASSWORD, sizeof(config->wifi_password) - 1);
    
    // Network defaults
    config->udp_port = CONFIG_UDP_PORT;
    strncpy(config->mdns_hostname, CONFIG_MDNS_HOSTNAME, sizeof(config->mdns_hostname) - 1);
    
    // LED defaults
    config->led_pin = CONFIG_LED_DATA_PIN;
    config->max_leds = CONFIG_MAX_LED_COUNT;
    strncpy(config->led_order, CONFIG_LED_COLOR_ORDER_STRING, sizeof(config->led_order) - 1);
    config->led_refresh_rate = CONFIG_LED_REFRESH_RATE_FPS;

    // Breathing effect defaults
    config->breathing_enabled = CONFIG_ENABLE_BREATHING_EFFECT ? 1 : 0;
    // Parse hex color from CONFIG_BREATHING_BASE_COLOR_HEX
    const char* hex_color = CONFIG_BREATHING_BASE_COLOR_HEX;
    if (strlen(hex_color) >= 6) {
        // Parse RGB from hex string (RRGGBB format)
        char r_hex[3] = {hex_color[0], hex_color[1], '\0'};
        char g_hex[3] = {hex_color[2], hex_color[3], '\0'};
        char b_hex[3] = {hex_color[4], hex_color[5], '\0'};
        config->breathing_base_r = (uint8_t)strtol(r_hex, NULL, 16);
        config->breathing_base_g = (uint8_t)strtol(g_hex, NULL, 16);
        config->breathing_base_b = (uint8_t)strtol(b_hex, NULL, 16);

        // Parse W if available (RRGGBBWW format)
        if (strlen(hex_color) >= 8) {
            char w_hex[3] = {hex_color[6], hex_color[7], '\0'};
            config->breathing_base_w = (uint8_t)strtol(w_hex, NULL, 16);
        } else {
            config->breathing_base_w = 0;
        }
    } else {
        // Default colors if hex parsing fails
        config->breathing_base_r = 20;
        config->breathing_base_g = 50;
        config->breathing_base_b = 80;
        config->breathing_base_w = 0;
    }
    config->breathing_min_brightness = CONFIG_BREATHING_MIN_BRIGHTNESS;
    config->breathing_max_brightness = CONFIG_BREATHING_MAX_BRIGHTNESS;
    config->breathing_step_size = CONFIG_BREATHING_STEP_SIZE;
    config->breathing_timer_period_ms = CONFIG_BREATHING_TIMER_PERIOD_MS;
    
    // Calculate checksum
    config->checksum = config_calculate_checksum(config);
    
    ESP_LOGI(TAG, "Default configuration set");
}

// Getter functions
const char* config_get_wifi_ssid(void)
{
    return g_firmware_config.wifi_ssid;
}

const char* config_get_wifi_password(void)
{
    return g_firmware_config.wifi_password;
}

uint16_t config_get_udp_port(void)
{
    return g_firmware_config.udp_port;
}

const char* config_get_mdns_hostname(void)
{
    return g_firmware_config.mdns_hostname;
}

uint8_t config_get_led_pin(void)
{
    return g_firmware_config.led_pin;
}

uint16_t config_get_max_leds(void)
{
    return g_firmware_config.max_leds;
}

const char* config_get_led_order(void)
{
    return g_firmware_config.led_order;
}
