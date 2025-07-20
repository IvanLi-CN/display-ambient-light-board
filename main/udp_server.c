#include "udp_server.h"
#include "config.h"
#include "state_machine.h"
#include "led_driver.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <errno.h>
#include <inttypes.h>

static const char *TAG = "UDP_SERVER";

// Global variables
static int g_socket_fd = -1;
static bool g_server_running = false;
static uint16_t g_server_port = 0;
static TaskHandle_t g_server_task_handle = NULL;

// Callbacks
static udp_packet_cb_t g_packet_callback = NULL;
static led_data_cb_t g_led_callback = NULL;

// Statistics
static struct {
    uint32_t packets_received;
    uint32_t bytes_received;
    uint32_t led_packets;
    uint32_t ping_packets;
    uint32_t invalid_packets;
    TickType_t last_led_data_time;  // Last time LED data was received
} g_stats = {0};

/**
 * UDP server task
 */
static void udp_server_task(void *pvParameters)
{
    uint8_t rx_buffer[MAX_PACKET_SIZE];
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    
    ESP_LOGI(TAG, "UDP server task started on port %d", g_server_port);
    
    while (g_server_running) {
        // Receive data
        int len = recvfrom(g_socket_fd, rx_buffer, sizeof(rx_buffer) - 1, 0,
                          (struct sockaddr *)&source_addr, &socklen);
        
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout, continue
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            } else {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
        } else if (len == 0) {
            ESP_LOGW(TAG, "Received empty packet");
            continue;
        }
        
        // Update statistics
        g_stats.packets_received++;
        g_stats.bytes_received += len;
        
        ESP_LOGD(TAG, "Received %d bytes from %d.%d.%d.%d:%d",
                 len,
                 (int)((source_addr.sin_addr.s_addr >> 0) & 0xFF),
                 (int)((source_addr.sin_addr.s_addr >> 8) & 0xFF),
                 (int)((source_addr.sin_addr.s_addr >> 16) & 0xFF),
                 (int)((source_addr.sin_addr.s_addr >> 24) & 0xFF),
                 ntohs(source_addr.sin_port));
        
        // Process packet
        if (len >= 1) {
            uint8_t packet_type = rx_buffer[0];
            
            switch (packet_type) {
                case PACKET_TYPE_PING:
                    ESP_LOGD(TAG, "Received ping packet");
                    g_stats.ping_packets++;
                    state_machine_handle_event(EVENT_PING_RECEIVED);

                    // Send ping response
                    uint8_t ping_response = PACKET_TYPE_PING;
                    int sent = sendto(g_socket_fd, &ping_response, 1, 0,
                                    (struct sockaddr *)&source_addr, sizeof(source_addr));
                    if (sent < 0) {
                        ESP_LOGW(TAG, "Failed to send ping response: errno %d", errno);
                    } else {
                        ESP_LOGD(TAG, "Sent ping response to %d.%d.%d.%d:%d",
                                (int)((source_addr.sin_addr.s_addr >> 0) & 0xFF),
                                (int)((source_addr.sin_addr.s_addr >> 8) & 0xFF),
                                (int)((source_addr.sin_addr.s_addr >> 16) & 0xFF),
                                (int)((source_addr.sin_addr.s_addr >> 24) & 0xFF),
                                ntohs(source_addr.sin_port));
                    }

                    if (g_packet_callback) {
                        g_packet_callback(UDP_PACKET_PING, rx_buffer, len);
                    }
                    break;
                    
                case PACKET_TYPE_LED_DATA:
                    if (len >= LED_DATA_HEADER_SIZE) {
                        // Parse LED data packet
                        uint16_t offset;
                        uint8_t* led_data;
                        size_t led_len;

                        if (udp_server_parse_led_packet(rx_buffer, len, &offset, &led_data, &led_len)) {
                            ESP_LOGD(TAG, "Received LED data: offset=%d, len=%" PRIu32, offset, (uint32_t)led_len);
                            g_stats.led_packets++;

                            // Update last LED data received time for timeout detection
                            g_stats.last_led_data_time = xTaskGetTickCount();

                            if (g_led_callback) {
                                g_led_callback(offset, led_data, led_len);
                            }

                            if (g_packet_callback) {
                                g_packet_callback(UDP_PACKET_LED_DATA, rx_buffer, len);
                            }
                        } else {
                            ESP_LOGW(TAG, "Invalid LED data packet");
                            g_stats.invalid_packets++;
                        }
                    } else {
                        ESP_LOGW(TAG, "LED data packet too short: %d bytes", len);
                        g_stats.invalid_packets++;
                    }
                    break;
                    
                case PACKET_TYPE_IGNORE_1:
                case PACKET_TYPE_IGNORE_2:
                    ESP_LOGD(TAG, "Ignoring packet type 0x%02X", packet_type);
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown packet type: 0x%02X", packet_type);
                    g_stats.invalid_packets++;
                    break;
            }
        }
    }
    
    ESP_LOGI(TAG, "UDP server task ended");
    g_server_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t udp_server_init(uint16_t port)
{
    if (g_socket_fd >= 0) {
        ESP_LOGW(TAG, "UDP server already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing UDP server on port %d", port);
    
    g_server_port = port;
    
    // Create socket
    g_socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (g_socket_fd < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = UDP_RECEIVE_TIMEOUT_MS * 1000;
    setsockopt(g_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Bind socket
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    
    int err = bind(g_socket_fd, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(g_socket_fd);
        g_socket_fd = -1;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "UDP server initialized on port %d", port);
    return ESP_OK;
}

esp_err_t udp_server_start(void)
{
    ESP_LOGI(TAG, "udp_server_start() called");

    if (g_socket_fd < 0) {
        ESP_LOGE(TAG, "UDP server not initialized - socket_fd: %d", g_socket_fd);
        return ESP_ERR_INVALID_STATE;
    }

    if (g_server_running) {
        ESP_LOGW(TAG, "UDP server already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting UDP server on socket %d", g_socket_fd);

    g_server_running = true;

    // Create server task with larger stack size to accommodate rx_buffer
    ESP_LOGI(TAG, "Creating UDP server task...");
    BaseType_t result = xTaskCreate(udp_server_task,
                                   "udp_server",
                                   8192,  // Increased from 4096 to 8192 bytes
                                   NULL,
                                   5,
                                   &g_server_task_handle);

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UDP server task - result: %d", result);
        g_server_running = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UDP server task created successfully");

    // Send UDP_LISTENING event to state machine
    ESP_LOGI(TAG, "Sending EVENT_UDP_LISTENING to state machine");
    esp_err_t event_result = state_machine_handle_event(EVENT_UDP_LISTENING);
    if (event_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send EVENT_UDP_LISTENING: %s", esp_err_to_name(event_result));
    } else {
        ESP_LOGI(TAG, "EVENT_UDP_LISTENING sent successfully");
    }

    ESP_LOGI(TAG, "UDP server started successfully");
    return ESP_OK;
}

esp_err_t udp_server_stop(void)
{
    if (!g_server_running) {
        ESP_LOGW(TAG, "UDP server not running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping UDP server");
    
    g_server_running = false;
    
    // Wait for task to finish
    if (g_server_task_handle) {
        // Give task time to finish gracefully
        vTaskDelay(pdMS_TO_TICKS(100));
        
        if (g_server_task_handle) {
            vTaskDelete(g_server_task_handle);
            g_server_task_handle = NULL;
        }
    }
    
    ESP_LOGI(TAG, "UDP server stopped");
    return ESP_OK;
}

bool udp_server_is_running(void)
{
    return g_server_running;
}

esp_err_t udp_server_receive_packet(uint8_t* buffer, size_t buffer_size, 
                                   size_t* received_len, uint32_t timeout_ms)
{
    if (g_socket_fd < 0 || !buffer || !received_len) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(g_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    int len = recvfrom(g_socket_fd, buffer, buffer_size, 0,
                      (struct sockaddr *)&source_addr, &socklen);
    
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ESP_ERR_TIMEOUT;
        } else {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            return ESP_FAIL;
        }
    }
    
    *received_len = len;
    return ESP_OK;
}

bool udp_server_parse_led_packet(const uint8_t* data, size_t len,
                                uint16_t* offset, uint8_t** led_data, size_t* led_len)
{
    if (!data || len < LED_DATA_HEADER_SIZE || !offset || !led_data || !led_len) {
        return false;
    }

    // Check packet type
    if (data[0] != PACKET_TYPE_LED_DATA) {
        return false;
    }

    // Parse offset (big-endian)
    *offset = (data[1] << 8) | data[2];

    // Set LED data pointer and length
    *led_data = (uint8_t*)(data + LED_DATA_HEADER_SIZE);
    *led_len = len - LED_DATA_HEADER_SIZE;

    // Get actual LED channels from configuration
    int led_channels = strlen(CONFIG_LED_COLOR_ORDER_STRING);

    // Note: We do NOT validate LED data length to be multiple of channels
    // This allows for UDP packet fragmentation and partial updates
    // The desktop application is responsible for sending correct data

    // Validate offset and length don't exceed buffer size
    // offset is byte offset, led_len is data length in bytes
    uint16_t max_buffer_size = MAX_LED_COUNT * led_channels;
    if (*offset + *led_len > max_buffer_size) {
      ESP_LOGW(TAG,
               "LED data exceeds buffer: byte_offset=%d, data_len=%" PRIu32
               ", max_buffer=%d",
               *offset, (uint32_t)*led_len, max_buffer_size);
      return false;
    }

    return true;
}

esp_err_t udp_server_register_packet_callback(udp_packet_cb_t callback)
{
    g_packet_callback = callback;
    return ESP_OK;
}

esp_err_t udp_server_register_led_callback(led_data_cb_t callback)
{
    g_led_callback = callback;
    return ESP_OK;
}

esp_err_t udp_server_get_stats(uint32_t* packets_received, uint32_t* bytes_received,
                              uint32_t* led_packets, uint32_t* ping_packets)
{
    if (packets_received) *packets_received = g_stats.packets_received;
    if (bytes_received) *bytes_received = g_stats.bytes_received;
    if (led_packets) *led_packets = g_stats.led_packets;
    if (ping_packets) *ping_packets = g_stats.ping_packets;

    return ESP_OK;
}

esp_err_t udp_server_reset_stats(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
    ESP_LOGI(TAG, "UDP server statistics reset");
    return ESP_OK;
}

esp_err_t udp_server_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing UDP server");

    // Stop server if running
    if (g_server_running) {
        udp_server_stop();
    }

    // Close socket
    if (g_socket_fd >= 0) {
        close(g_socket_fd);
        g_socket_fd = -1;
    }

    // Reset variables
    g_server_port = 0;
    g_packet_callback = NULL;
    g_led_callback = NULL;
    memset(&g_stats, 0, sizeof(g_stats));

    ESP_LOGI(TAG, "UDP server deinitialized");
    return ESP_OK;
}
