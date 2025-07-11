#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_flash.h"

#include "config.h"
#include "state_machine.h"
#include "wifi_manager.h"
#include "mdns_service.h"
#include "udp_server.h"
#include "led_driver.h"

static const char *TAG = "MAIN";

// LED data timeout detection
static TimerHandle_t g_led_timeout_timer = NULL;
static bool g_led_data_active = false;
#define LED_DATA_TIMEOUT_MS 5000  // 5 seconds timeout

/**
 * LED data timeout callback
 */
static void led_timeout_callback(TimerHandle_t xTimer)
{
    if (g_led_data_active) {
        ESP_LOGI(TAG, "LED data timeout - resuming breathing effect");
        g_led_data_active = false;

        // Resume full breathing effect
        led_driver_set_mixed_mode(false);

        // Ensure breathing effect is enabled
        if (!led_driver_is_breathing_enabled()) {
            led_driver_set_breathing_effect(true);
        }
    }
}

/**
 * Initialize NVS (Non-Volatile Storage)
 */
static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * Initialize GPIO pins
 */
static esp_err_t init_gpio(void)
{
    // Configure LED data pin as output
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_DATA_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret == ESP_OK) {
        // Set initial state to low
        gpio_set_level(LED_DATA_PIN, 0);
        ESP_LOGI(TAG, "GPIO initialized - LED data pin: %d", LED_DATA_PIN);
    } else {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * Print system information
 */
static void print_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "ESP32-C3 Ambient Light Board Starting...");
    ESP_LOGI(TAG, "Chip: %s, cores: %d, revision: %d",
             CONFIG_IDF_TARGET, chip_info.cores, chip_info.revision);
    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "Flash: %" PRIu32 "MB %s",
             flash_size / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  - LED Data Pin: GPIO%d", LED_DATA_PIN);
    ESP_LOGI(TAG, "  - Max LEDs: %d", MAX_LED_COUNT);
    ESP_LOGI(TAG, "  - UDP Port: %d", UDP_PORT);
    ESP_LOGI(TAG, "  - mDNS Service: %s.%s.local", MDNS_SERVICE_NAME, MDNS_PROTOCOL);
}

/**
 * WiFi event callback
 */
static void wifi_event_callback(wifi_status_t status, esp_ip4_addr_t ip_addr)
{
    switch (status) {
        case WIFI_STATUS_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected, starting mDNS service");
            mdns_service_start(ip_addr);
            state_machine_handle_event(EVENT_NETWORK_READY);
            state_machine_handle_event(EVENT_UDP_START);
            break;

        case WIFI_STATUS_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi disconnected, stopping services");
            mdns_service_stop();
            udp_server_stop();
            state_machine_handle_event(EVENT_WIFI_DISCONNECTED);
            break;

        case WIFI_STATUS_ERROR:
            ESP_LOGE(TAG, "WiFi error");
            state_machine_handle_event(EVENT_WIFI_FAILED);
            break;

        default:
            break;
    }
}

/**
 * LED data callback from UDP server
 */
static void led_data_callback(uint16_t offset, const uint8_t* data, size_t len)
{
    ESP_LOGD(TAG, "Received LED data: offset=%d, len=%d", offset, len);

    // Switch to mixed mode if not already active
    if (!g_led_data_active) {
        ESP_LOGI(TAG, "LED data received - switching to mixed mode");
        g_led_data_active = true;

        // Enable mixed mode (breathing status LED + LED data)
        led_driver_set_mixed_mode(true);

        // Ensure breathing effect is enabled for status LED
        if (!led_driver_is_breathing_enabled()) {
            led_driver_set_breathing_effect(true);
        }
    }

    // Reset timeout timer
    if (g_led_timeout_timer) {
        xTimerReset(g_led_timeout_timer, 0);
    }

    // Update LED buffer
    esp_err_t ret = led_driver_update_buffer(offset, data, len);
    if (ret == ESP_OK) {
        // Transmit updated data
        led_driver_transmit_all();
    } else {
        ESP_LOGW(TAG, "Failed to update LED buffer: %s", esp_err_to_name(ret));
    }
}

/**
 * State machine transition callback
 */
static esp_err_t state_transition_callback(system_state_t from_state, system_state_t to_state)
{
    ESP_LOGI(TAG, "State transition: %s -> %s",
             state_machine_state_to_string(from_state),
             state_machine_state_to_string(to_state));

    switch (to_state) {
        case STATE_SYSTEM_INIT:
            // System initialization - white status LED
            #if CONFIG_ENABLE_BREATHING_EFFECT
            ESP_LOGI(TAG, "Setting up LED breathing effect for system initialization");
            // Set colors first before enabling breathing effect
            led_driver_set_breathing_color(BREATHING_BASE_R, BREATHING_BASE_G,
                                           BREATHING_BASE_B, BREATHING_BASE_W);
            led_driver_set_status(LED_STATUS_INIT);
            // Small delay to ensure settings are applied
            vTaskDelay(pdMS_TO_TICKS(50));
            led_driver_set_breathing_effect(true);
            #endif
            break;

        case STATE_WIFI_CONNECTING:
            // WiFi connecting - blue status LED
            #if CONFIG_ENABLE_BREATHING_EFFECT
            led_driver_set_breathing_effect(true);
            led_driver_set_status(LED_STATUS_WIFI_CONNECTING);
            // Keep using configured base breathing color
            #endif
            break;

        case STATE_DHCP_REQUESTING:
            // DHCP requesting - green status LED
            #if CONFIG_ENABLE_BREATHING_EFFECT
            led_driver_set_status(LED_STATUS_NETWORK_READY);
            // Keep using configured base breathing color
            #endif
            break;

        case STATE_NETWORK_READY:
            // Network ready - green status LED
            #if CONFIG_ENABLE_BREATHING_EFFECT
            led_driver_set_status(LED_STATUS_NETWORK_READY);
            // Keep using configured base breathing color
            #endif
            break;

        case STATE_UDP_STARTING:
            // Start UDP server when entering UDP_STARTING state
            ESP_LOGI(TAG, "=== ENTERING STATE_UDP_STARTING ===");
            #if CONFIG_ENABLE_BREATHING_EFFECT
            led_driver_set_status(LED_STATUS_NETWORK_READY);  // Keep green while starting UDP
            #endif
            ESP_LOGI(TAG, "Calling udp_server_start()...");
            esp_err_t udp_result = udp_server_start();
            if (udp_result == ESP_OK) {
                ESP_LOGI(TAG, "UDP server started successfully");
            } else {
                ESP_LOGE(TAG, "Failed to start UDP server: %s", esp_err_to_name(udp_result));
                state_machine_handle_event(EVENT_UDP_FAILED);
            }
            ESP_LOGI(TAG, "=== STATE_UDP_STARTING processing complete ===");
            break;

        case STATE_OPERATIONAL:
            // Operational - purple status LED, keep breathing for ambient effect
            #if CONFIG_ENABLE_BREATHING_EFFECT
            led_driver_set_status(LED_STATUS_OPERATIONAL);
            // Keep using configured base breathing color
            #endif
            ESP_LOGI(TAG, "System is now operational! All LEDs breathing with status indicator.");
            break;

        case STATE_UDP_LISTENING:
            // UDP server is already started, just log
            ESP_LOGI(TAG, "UDP server is listening for packets");
            break;

        case STATE_WIFI_ERROR:
            // WiFi error - red status LED
            #if CONFIG_ENABLE_BREATHING_EFFECT
            led_driver_set_status(LED_STATUS_WIFI_ERROR);
            // Keep using configured base breathing color
            #endif
            break;

        case STATE_DHCP_ERROR:
        case STATE_UDP_ERROR:
            // UDP/DHCP error - orange status LED
            #if CONFIG_ENABLE_BREATHING_EFFECT
            led_driver_set_status(LED_STATUS_UDP_ERROR);
            // Keep using configured base breathing color
            #endif
            break;

        default:
            break;
    }

    return ESP_OK;
}

/**
 * Initialize all system modules
 */
static esp_err_t init_system_modules(void)
{
    esp_err_t ret;

    // Initialize state machine
    ret = state_machine_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize state machine: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register state machine callbacks
    state_machine_register_transition_callback(state_transition_callback);

    // Manually trigger initial state setup since transition callback
    // is only called on state changes, not initial state
    ESP_LOGI(TAG, "Setting up initial state: SYSTEM_INIT");
    state_transition_callback(STATE_SYSTEM_INIT, STATE_SYSTEM_INIT);

    // Initialize WiFi manager
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register WiFi callback
    wifi_manager_register_callback(wifi_event_callback);

    // Initialize mDNS service
    ret = mdns_service_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mDNS service: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize UDP server
    ret = udp_server_init(UDP_PORT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize UDP server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register UDP callbacks
    udp_server_register_led_callback(led_data_callback);

    // Initialize LED driver
    ret = led_driver_init(LED_DATA_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED driver: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create LED data timeout timer
    g_led_timeout_timer = xTimerCreate("led_timeout",
                                      pdMS_TO_TICKS(LED_DATA_TIMEOUT_MS),
                                      pdFALSE,  // One-shot timer
                                      NULL,
                                      led_timeout_callback);
    if (!g_led_timeout_timer) {
        ESP_LOGE(TAG, "Failed to create LED timeout timer");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "All system modules initialized successfully");
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-C3 Ambient Light Board");

    // Initialize NVS
    ESP_ERROR_CHECK(init_nvs());
    ESP_LOGI(TAG, "NVS initialized");

    // Initialize GPIO
    ESP_ERROR_CHECK(init_gpio());

    // Print system information
    print_system_info();

    // Initialize all system modules
    ESP_ERROR_CHECK(init_system_modules());

    // Start state machine
    ESP_ERROR_CHECK(state_machine_start());

    // Give a small delay to ensure all modules are fully ready
    vTaskDelay(pdMS_TO_TICKS(100));

    // Signal system initialization complete
    state_machine_handle_event(EVENT_SYSTEM_INIT_COMPLETE);

    // Connect to WiFi using configured credentials
    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", CONFIG_WIFI_SSID);
    wifi_manager_connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);

    ESP_LOGI(TAG, "System initialization complete");

    // TEMPORARY: Force start mDNS service for debugging
    ESP_LOGI(TAG, "=== FORCE STARTING MDNS SERVICE FOR DEBUGGING ===");
    esp_ip4_addr_t debug_ip;
    debug_ip.addr = esp_ip4addr_aton("192.168.31.182");  // Use current IP
    esp_err_t mdns_debug_result = mdns_service_start(debug_ip);
    ESP_LOGI(TAG, "Force mDNS start result: %s", esp_err_to_name(mdns_debug_result));

    // Main monitoring loop
    while (1) {
        // Print system status every 30 seconds
        ESP_LOGI(TAG, "System status: %s, Free heap: %" PRIu32 " bytes",
                 state_machine_state_to_string(state_machine_get_current_state()),
                 esp_get_free_heap_size());

        // Print statistics
        uint32_t packets, bytes, led_packets, ping_packets;
        if (udp_server_get_stats(&packets, &bytes, &led_packets, &ping_packets) == ESP_OK) {
            ESP_LOGI(TAG, "UDP stats: %" PRIu32 " packets (%" PRIu32 " bytes), %" PRIu32 " LED, %" PRIu32 " ping",
                     packets, bytes, led_packets, ping_packets);
        }

        uint32_t transmissions, led_bytes, last_tx;
        if (led_driver_get_stats(&transmissions, &led_bytes, &last_tx) == ESP_OK) {
            ESP_LOGI(TAG, "LED stats: %" PRIu32 " transmissions (%" PRIu32 " bytes)", transmissions, led_bytes);
        }

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}