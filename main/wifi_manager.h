#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"

/**
 * WiFi connection status
 */
typedef enum {
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_ERROR
} wifi_status_t;

/**
 * WiFi event callback function type
 */
typedef void (*wifi_event_cb_t)(wifi_status_t status, esp_ip4_addr_t ip_addr);

/**
 * Initialize WiFi manager
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_init(void);

/**
 * Connect to WiFi network
 * @param ssid WiFi network SSID
 * @param password WiFi network password
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_connect(const char* ssid, const char* password);

/**
 * Disconnect from WiFi network
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * Check if WiFi is connected
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

/**
 * Get current IP address
 * @return IP address (0.0.0.0 if not connected)
 */
esp_ip4_addr_t wifi_manager_get_ip(void);

/**
 * Get WiFi connection status
 * @return Current WiFi status
 */
wifi_status_t wifi_manager_get_status(void);

/**
 * Register WiFi event callback
 * @param callback Callback function to register
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_register_callback(wifi_event_cb_t callback);

/**
 * Get WiFi signal strength (RSSI)
 * @return RSSI value in dBm
 */
int8_t wifi_manager_get_rssi(void);

/**
 * Start WiFi scan
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_start_scan(void);

/**
 * Get scan results
 * @param ap_info Array to store AP information
 * @param max_aps Maximum number of APs to return
 * @param num_aps Pointer to store actual number of APs found
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_get_scan_results(wifi_ap_record_t* ap_info, uint16_t max_aps, uint16_t* num_aps);

/**
 * Deinitialize WiFi manager
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_deinit(void);

#endif // WIFI_MANAGER_H
