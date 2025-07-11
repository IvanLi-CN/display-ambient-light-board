#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include "esp_err.h"
#include "config.h"
#include <stdint.h>
#include <stddef.h>

/**
 * UDP packet types as defined in the protocol
 */
typedef enum {
    UDP_PACKET_PING = PACKET_TYPE_PING,         // 0x01
    UDP_PACKET_LED_DATA = PACKET_TYPE_LED_DATA, // 0x02
    UDP_PACKET_IGNORE_1 = PACKET_TYPE_IGNORE_1, // 0x03
    UDP_PACKET_IGNORE_2 = PACKET_TYPE_IGNORE_2  // 0x04
} udp_packet_type_t;

/**
 * LED data packet structure
 */
typedef struct {
    uint8_t type;           // Packet type (0x02)
    uint16_t offset;        // LED offset (big-endian)
    uint8_t* led_data;      // Pointer to LED data
    size_t led_data_len;    // Length of LED data
} led_data_packet_t;

/**
 * UDP packet callback function type
 */
typedef void (*udp_packet_cb_t)(udp_packet_type_t type, const uint8_t* data, size_t len);

/**
 * LED data callback function type
 */
typedef void (*led_data_cb_t)(uint16_t offset, const uint8_t* data, size_t len);

/**
 * Initialize UDP server
 * @param port UDP port to bind to
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t udp_server_init(uint16_t port);

/**
 * Start UDP server
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t udp_server_start(void);

/**
 * Stop UDP server
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t udp_server_stop(void);

/**
 * Check if UDP server is running
 * @return true if running, false otherwise
 */
bool udp_server_is_running(void);

/**
 * Receive a UDP packet (blocking with timeout)
 * @param buffer Buffer to store received data
 * @param buffer_size Size of the buffer
 * @param received_len Pointer to store actual received length
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout, other error codes on failure
 */
esp_err_t udp_server_receive_packet(uint8_t* buffer, size_t buffer_size, 
                                   size_t* received_len, uint32_t timeout_ms);

/**
 * Parse LED data packet
 * @param data Raw packet data
 * @param len Length of packet data
 * @param offset Pointer to store LED offset
 * @param led_data Pointer to store LED data pointer
 * @param led_len Pointer to store LED data length
 * @return true if packet is valid LED data packet, false otherwise
 */
bool udp_server_parse_led_packet(const uint8_t* data, size_t len, 
                                uint16_t* offset, uint8_t** led_data, size_t* led_len);

/**
 * Register packet callback
 * @param callback Callback function to register
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t udp_server_register_packet_callback(udp_packet_cb_t callback);

/**
 * Register LED data callback
 * @param callback Callback function to register
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t udp_server_register_led_callback(led_data_cb_t callback);

/**
 * Get server statistics
 * @param packets_received Pointer to store total packets received
 * @param bytes_received Pointer to store total bytes received
 * @param led_packets Pointer to store LED packets received
 * @param ping_packets Pointer to store ping packets received
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t udp_server_get_stats(uint32_t* packets_received, uint32_t* bytes_received,
                              uint32_t* led_packets, uint32_t* ping_packets);

/**
 * Reset server statistics
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t udp_server_reset_stats(void);

/**
 * Deinitialize UDP server
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t udp_server_deinit(void);

#endif // UDP_SERVER_H
