#ifndef MDNS_SERVICE_H
#define MDNS_SERVICE_H

#include "esp_err.h"
#include "esp_netif.h"

/**
 * Initialize mDNS service
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mdns_service_init(void);

/**
 * Start mDNS service with given IP address
 * @param ip_addr IP address to advertise
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mdns_service_start(esp_ip4_addr_t ip_addr);

/**
 * Stop mDNS service
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mdns_service_stop(void);

/**
 * Update mDNS service IP address
 * @param ip_addr New IP address to advertise
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mdns_service_update_ip(esp_ip4_addr_t ip_addr);

/**
 * Check if mDNS service is running
 * @return true if running, false otherwise
 */
bool mdns_service_is_running(void);

/**
 * Deinitialize mDNS service
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mdns_service_deinit(void);

#endif // MDNS_SERVICE_H
