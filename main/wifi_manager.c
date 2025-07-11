#include "wifi_manager.h"
#include "config.h"
#include "state_machine.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "WIFI_MANAGER";

// WiFi event bits
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_FAIL_BIT         BIT1

// Global variables
static EventGroupHandle_t g_wifi_event_group;
static esp_netif_t *g_sta_netif = NULL;
static wifi_status_t g_wifi_status = WIFI_STATUS_DISCONNECTED;
static esp_ip4_addr_t g_ip_addr = {0};
static wifi_event_cb_t g_event_callback = NULL;
static int g_retry_count = 0;

/**
 * WiFi event handler
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi station started");
        esp_wifi_connect();
        g_wifi_status = WIFI_STATUS_CONNECTING;
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected");
        g_wifi_status = WIFI_STATUS_DISCONNECTED;
        g_ip_addr.addr = 0;
        
        if (g_retry_count < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            g_retry_count++;
            ESP_LOGI(TAG, "Retry to connect to the AP (attempt %d/%d)", 
                     g_retry_count, WIFI_MAXIMUM_RETRY);
        } else {
            ESP_LOGE(TAG, "Failed to connect to WiFi after %d attempts", WIFI_MAXIMUM_RETRY);
            g_wifi_status = WIFI_STATUS_ERROR;
            xEventGroupSetBits(g_wifi_event_group, WIFI_FAIL_BIT);
            state_machine_handle_event(EVENT_WIFI_FAILED);
        }
        
        if (g_event_callback) {
            g_event_callback(g_wifi_status, g_ip_addr);
        }
        
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        g_ip_addr = event->ip_info.ip;
        g_wifi_status = WIFI_STATUS_CONNECTED;
        g_retry_count = 0;
        
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&g_ip_addr));
        
        xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
        state_machine_handle_event(EVENT_WIFI_CONNECTED);
        state_machine_handle_event(EVENT_DHCP_SUCCESS);
        
        if (g_event_callback) {
            g_event_callback(g_wifi_status, g_ip_addr);
        }
    }
}

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi manager");
    
    // Create event group
    g_wifi_event_group = xEventGroupCreate();
    if (!g_wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create WiFi event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default WiFi STA interface
    g_sta_netif = esp_netif_create_default_wifi_sta();
    if (!g_sta_netif) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA interface");
        return ESP_FAIL;
    }
    
    // Initialize WiFi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                              &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                              &wifi_event_handler, NULL));
    
    // Set WiFi mode to station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const char* ssid, const char* password)
{
    if (!ssid) {
        ESP_LOGE(TAG, "SSID cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid);
    
    // Configure WiFi
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    
    // Set WiFi configuration
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    g_retry_count = 0;
    g_wifi_status = WIFI_STATUS_CONNECTING;
    
    state_machine_handle_event(EVENT_WIFI_CONNECT_START);
    
    return ESP_OK;
}

esp_err_t wifi_manager_disconnect(void)
{
    ESP_LOGI(TAG, "Disconnecting from WiFi");
    
    esp_err_t ret = esp_wifi_disconnect();
    if (ret == ESP_OK) {
        g_wifi_status = WIFI_STATUS_DISCONNECTED;
        g_ip_addr.addr = 0;
    }
    
    return ret;
}

bool wifi_manager_is_connected(void)
{
    return (g_wifi_status == WIFI_STATUS_CONNECTED);
}

esp_ip4_addr_t wifi_manager_get_ip(void)
{
    return g_ip_addr;
}

wifi_status_t wifi_manager_get_status(void)
{
    return g_wifi_status;
}

esp_err_t wifi_manager_register_callback(wifi_event_cb_t callback)
{
    g_event_callback = callback;
    return ESP_OK;
}

int8_t wifi_manager_get_rssi(void)
{
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (ret == ESP_OK) {
        return ap_info.rssi;
    }
    
    return -100; // Return very weak signal if error
}

esp_err_t wifi_manager_start_scan(void)
{
    ESP_LOGI(TAG, "Starting WiFi scan");
    return esp_wifi_scan_start(NULL, false);
}

esp_err_t wifi_manager_get_scan_results(wifi_ap_record_t* ap_info, uint16_t max_aps, uint16_t* num_aps)
{
    if (!ap_info || !num_aps) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return esp_wifi_scan_get_ap_records(&max_aps, ap_info);
}

esp_err_t wifi_manager_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing WiFi manager");
    
    // Stop WiFi
    esp_wifi_stop();
    
    // Unregister event handlers
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);
    
    // Deinitialize WiFi
    esp_wifi_deinit();
    
    // Destroy event group
    if (g_wifi_event_group) {
        vEventGroupDelete(g_wifi_event_group);
        g_wifi_event_group = NULL;
    }
    
    g_wifi_status = WIFI_STATUS_DISCONNECTED;
    g_ip_addr.addr = 0;
    
    ESP_LOGI(TAG, "WiFi manager deinitialized");
    return ESP_OK;
}
