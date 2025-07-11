#include "mdns_service.h"
#include "config.h"
#include "esp_log.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "MDNS_SERVICE";

// Global variables
static bool g_mdns_initialized = false;
static bool g_mdns_running = false;
static TimerHandle_t g_announce_timer = NULL;
static esp_ip4_addr_t g_current_ip = {0};

/**
 * Timer callback for periodic mDNS announcements
 */
static void mdns_announce_timer_callback(TimerHandle_t xTimer)
{
    if (g_mdns_running) {
        ESP_LOGD(TAG, "Sending periodic mDNS announcement");
        // The mDNS library handles periodic announcements automatically
        ESP_LOGD(TAG, "mDNS periodic announcement (automatic)");
    }
}

esp_err_t mdns_service_init(void)
{
    if (g_mdns_initialized) {
        ESP_LOGW(TAG, "mDNS service already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing mDNS service");
    
    // Initialize mDNS
    ESP_LOGI(TAG, "Initializing mDNS service...");
    esp_err_t ret = mdns_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mDNS: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "mDNS initialized successfully");
    
    // Set hostname
    ret = mdns_hostname_set(MDNS_HOSTNAME);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set mDNS hostname: %s", esp_err_to_name(ret));
        mdns_free();
        return ret;
    }
    
    // Set default instance name
    ret = mdns_instance_name_set("ESP32-C3 Ambient Light Board");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set mDNS instance name: %s", esp_err_to_name(ret));
        mdns_free();
        return ret;
    }
    
    // Create announcement timer
    g_announce_timer = xTimerCreate("mdns_announce",
                                   pdMS_TO_TICKS(MDNS_ANNOUNCE_INTERVAL),
                                   pdTRUE,  // Auto-reload
                                   NULL,
                                   mdns_announce_timer_callback);
    
    if (!g_announce_timer) {
        ESP_LOGE(TAG, "Failed to create mDNS announcement timer");
        mdns_free();
        return ESP_ERR_NO_MEM;
    }
    
    g_mdns_initialized = true;
    ESP_LOGI(TAG, "mDNS service initialized with hostname: %s.local", MDNS_HOSTNAME);
    
    return ESP_OK;
}

esp_err_t mdns_service_start(esp_ip4_addr_t ip_addr)
{
    if (!g_mdns_initialized) {
        ESP_LOGE(TAG, "mDNS service not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_mdns_running) {
        ESP_LOGW(TAG, "mDNS service already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting mDNS service with IP: " IPSTR, IP2STR(&ip_addr));
    
    g_current_ip = ip_addr;
    
    // Add UDP service
    ESP_LOGI(TAG, "Adding mDNS service: %s%s.local on port %d", MDNS_SERVICE_NAME, MDNS_PROTOCOL, UDP_PORT);
    esp_err_t ret = mdns_service_add(NULL, MDNS_SERVICE_NAME, MDNS_PROTOCOL, UDP_PORT, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add mDNS service: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "mDNS service added successfully");
    
    // Add service instance
    ret = mdns_service_instance_name_set(MDNS_SERVICE_NAME, MDNS_PROTOCOL, "ESP32-C3 Ambient Light");
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set service instance name: %s", esp_err_to_name(ret));
        // Continue anyway, this is not critical
    }
    
    // Add TXT records with device information
    mdns_txt_item_t txt_records[] = {
        {"version", "1.0"},
        {"device", "esp32c3"},
        {"type", "ambient_light"},
        {"max_leds", "500"},
        {"protocol", "udp"}
    };
    
    ret = mdns_service_txt_set(MDNS_SERVICE_NAME, MDNS_PROTOCOL, txt_records, 
                              sizeof(txt_records) / sizeof(txt_records[0]));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set TXT records: %s", esp_err_to_name(ret));
        // Continue anyway, this is not critical
    }
    
    // Start periodic announcements
    if (g_announce_timer) {
        xTimerStart(g_announce_timer, 0);
    }
    
    g_mdns_running = true;
    
    ESP_LOGI(TAG, "mDNS service started: %s.%s.local:%d", 
             MDNS_SERVICE_NAME, MDNS_PROTOCOL, UDP_PORT);
    
    return ESP_OK;
}

esp_err_t mdns_service_stop(void)
{
    if (!g_mdns_running) {
        ESP_LOGW(TAG, "mDNS service not running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping mDNS service");
    
    // Stop announcement timer
    if (g_announce_timer) {
        xTimerStop(g_announce_timer, 0);
    }
    
    // Remove service
    esp_err_t ret = mdns_service_remove(MDNS_SERVICE_NAME, MDNS_PROTOCOL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to remove mDNS service: %s", esp_err_to_name(ret));
    }
    
    g_mdns_running = false;
    g_current_ip.addr = 0;
    
    ESP_LOGI(TAG, "mDNS service stopped");
    return ESP_OK;
}

esp_err_t mdns_service_update_ip(esp_ip4_addr_t ip_addr)
{
    if (!g_mdns_running) {
        ESP_LOGW(TAG, "mDNS service not running, cannot update IP");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_current_ip.addr == ip_addr.addr) {
        ESP_LOGD(TAG, "IP address unchanged, no update needed");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Updating mDNS IP address: " IPSTR " -> " IPSTR, 
             IP2STR(&g_current_ip), IP2STR(&ip_addr));
    
    g_current_ip = ip_addr;
    
    // The mDNS library automatically updates IP addresses when the network interface changes
    ESP_LOGI(TAG, "mDNS IP address updated automatically");
    
    return ESP_OK;
}

bool mdns_service_is_running(void)
{
    return g_mdns_running;
}

esp_err_t mdns_service_deinit(void)
{
    if (!g_mdns_initialized) {
        ESP_LOGW(TAG, "mDNS service not initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing mDNS service");
    
    // Stop service if running
    if (g_mdns_running) {
        mdns_service_stop();
    }
    
    // Delete timer
    if (g_announce_timer) {
        xTimerDelete(g_announce_timer, 0);
        g_announce_timer = NULL;
    }
    
    // Free mDNS
    mdns_free();
    
    g_mdns_initialized = false;
    g_mdns_running = false;
    g_current_ip.addr = 0;
    
    ESP_LOGI(TAG, "mDNS service deinitialized");
    return ESP_OK;
}
