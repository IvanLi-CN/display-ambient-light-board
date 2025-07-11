#include "state_machine.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include <string.h>

static const char *TAG = "STATE_MACHINE";

// Global state machine context
static state_machine_context_t g_state_context = {0};

// Event queue for state machine
static QueueHandle_t g_event_queue = NULL;

// Callbacks
static state_transition_cb_t g_transition_callback = NULL;
static state_timeout_cb_t g_timeout_callback = NULL;

// Task handle
static TaskHandle_t g_state_task_handle = NULL;

/**
 * State transition table
 * [current_state][event] = next_state (or current_state if no transition)
 */
static const system_state_t state_transition_table[12][16] = {
    // STATE_SYSTEM_INIT
    {STATE_WIFI_CONNECTING, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT, STATE_SYSTEM_INIT},
    // STATE_WIFI_CONNECTING
    {STATE_WIFI_CONNECTING, STATE_WIFI_CONNECTING, STATE_DHCP_REQUESTING, STATE_WIFI_ERROR, STATE_WIFI_ERROR, STATE_WIFI_CONNECTING, STATE_WIFI_CONNECTING, STATE_WIFI_CONNECTING, STATE_WIFI_CONNECTING, STATE_WIFI_CONNECTING, STATE_WIFI_CONNECTING, STATE_WIFI_CONNECTING, STATE_WIFI_CONNECTING, STATE_WIFI_CONNECTING, STATE_WIFI_CONNECTING, STATE_WIFI_CONNECTING},
    // STATE_DHCP_REQUESTING
    {STATE_DHCP_REQUESTING, STATE_DHCP_REQUESTING, STATE_DHCP_REQUESTING, STATE_WIFI_ERROR, STATE_DHCP_REQUESTING, STATE_NETWORK_READY, STATE_DHCP_ERROR, STATE_DHCP_REQUESTING, STATE_DHCP_REQUESTING, STATE_DHCP_REQUESTING, STATE_DHCP_REQUESTING, STATE_DHCP_REQUESTING, STATE_DHCP_REQUESTING, STATE_DHCP_REQUESTING, STATE_DHCP_REQUESTING, STATE_DHCP_REQUESTING},
    // STATE_NETWORK_READY
    {STATE_NETWORK_READY, STATE_NETWORK_READY, STATE_NETWORK_READY, STATE_WIFI_ERROR, STATE_NETWORK_READY, STATE_NETWORK_READY, STATE_NETWORK_READY, STATE_UDP_STARTING, STATE_UDP_STARTING, STATE_NETWORK_READY, STATE_NETWORK_READY, STATE_NETWORK_READY, STATE_NETWORK_READY, STATE_NETWORK_READY, STATE_NETWORK_READY, STATE_NETWORK_READY},
    // STATE_UDP_STARTING
    {STATE_UDP_STARTING, STATE_UDP_STARTING, STATE_UDP_STARTING, STATE_WIFI_ERROR, STATE_UDP_STARTING, STATE_UDP_STARTING, STATE_UDP_STARTING, STATE_UDP_STARTING, STATE_UDP_STARTING, STATE_UDP_LISTENING, STATE_UDP_ERROR, STATE_UDP_STARTING, STATE_UDP_STARTING, STATE_UDP_STARTING, STATE_UDP_STARTING, STATE_UDP_STARTING},
    // STATE_UDP_LISTENING
    {STATE_UDP_LISTENING, STATE_UDP_LISTENING, STATE_UDP_LISTENING, STATE_WIFI_ERROR, STATE_UDP_LISTENING, STATE_UDP_LISTENING, STATE_UDP_LISTENING, STATE_UDP_LISTENING, STATE_UDP_LISTENING, STATE_UDP_LISTENING, STATE_UDP_ERROR, STATE_OPERATIONAL, STATE_OPERATIONAL, STATE_UDP_TIMEOUT, STATE_UDP_LISTENING, STATE_UDP_LISTENING},
    // STATE_OPERATIONAL
    {STATE_OPERATIONAL, STATE_OPERATIONAL, STATE_OPERATIONAL, STATE_WIFI_ERROR, STATE_OPERATIONAL, STATE_OPERATIONAL, STATE_OPERATIONAL, STATE_OPERATIONAL, STATE_OPERATIONAL, STATE_OPERATIONAL, STATE_UDP_ERROR, STATE_OPERATIONAL, STATE_OPERATIONAL, STATE_UDP_TIMEOUT, STATE_OPERATIONAL, STATE_OPERATIONAL},
    // STATE_UDP_TIMEOUT
    {STATE_UDP_TIMEOUT, STATE_UDP_TIMEOUT, STATE_UDP_TIMEOUT, STATE_WIFI_ERROR, STATE_UDP_TIMEOUT, STATE_UDP_TIMEOUT, STATE_UDP_TIMEOUT, STATE_UDP_TIMEOUT, STATE_UDP_TIMEOUT, STATE_UDP_TIMEOUT, STATE_UDP_ERROR, STATE_UDP_LISTENING, STATE_OPERATIONAL, STATE_UDP_TIMEOUT, STATE_RECONNECTING, STATE_RECONNECTING},
    // STATE_WIFI_ERROR
    {STATE_WIFI_ERROR, STATE_WIFI_CONNECTING, STATE_DHCP_REQUESTING, STATE_WIFI_ERROR, STATE_WIFI_ERROR, STATE_WIFI_ERROR, STATE_WIFI_ERROR, STATE_WIFI_ERROR, STATE_WIFI_ERROR, STATE_WIFI_ERROR, STATE_WIFI_ERROR, STATE_WIFI_ERROR, STATE_WIFI_ERROR, STATE_WIFI_ERROR, STATE_RECONNECTING, STATE_RECONNECTING},
    // STATE_DHCP_ERROR
    {STATE_DHCP_ERROR, STATE_DHCP_ERROR, STATE_DHCP_ERROR, STATE_WIFI_ERROR, STATE_DHCP_ERROR, STATE_DHCP_ERROR, STATE_DHCP_ERROR, STATE_DHCP_ERROR, STATE_DHCP_ERROR, STATE_DHCP_ERROR, STATE_DHCP_ERROR, STATE_DHCP_ERROR, STATE_DHCP_ERROR, STATE_DHCP_ERROR, STATE_RECONNECTING, STATE_RECONNECTING},
    // STATE_UDP_ERROR
    {STATE_UDP_ERROR, STATE_UDP_ERROR, STATE_UDP_ERROR, STATE_WIFI_ERROR, STATE_UDP_ERROR, STATE_UDP_ERROR, STATE_UDP_ERROR, STATE_UDP_ERROR, STATE_UDP_ERROR, STATE_UDP_ERROR, STATE_UDP_ERROR, STATE_UDP_ERROR, STATE_UDP_ERROR, STATE_UDP_ERROR, STATE_RECONNECTING, STATE_RECONNECTING},
    // STATE_RECONNECTING
    {STATE_RECONNECTING, STATE_WIFI_CONNECTING, STATE_RECONNECTING, STATE_RECONNECTING, STATE_RECONNECTING, STATE_RECONNECTING, STATE_RECONNECTING, STATE_RECONNECTING, STATE_RECONNECTING, STATE_RECONNECTING, STATE_RECONNECTING, STATE_RECONNECTING, STATE_RECONNECTING, STATE_RECONNECTING, STATE_RECONNECTING, STATE_RECONNECTING}
};

/**
 * Get timeout value for a given state
 */
static uint32_t get_state_timeout_ms(system_state_t state)
{
    switch (state) {
        case STATE_WIFI_CONNECTING:
            return STATE_TIMEOUT_WIFI_MS;
        case STATE_DHCP_REQUESTING:
            return STATE_TIMEOUT_DHCP_MS;
        case STATE_UDP_STARTING:
            return STATE_TIMEOUT_UDP_MS;
        case STATE_OPERATIONAL:
            return STATE_TIMEOUT_PING_MS;
        default:
            return 0; // No timeout
    }
}

/**
 * Timer callback for state timeouts
 */
static void state_timeout_timer_callback(TimerHandle_t xTimer)
{
    system_event_t timeout_event = EVENT_ERROR_RECOVERY;
    
    ESP_LOGW(TAG, "State timeout in state: %s", 
             state_machine_state_to_string(g_state_context.current_state));
    
    // Send timeout event to state machine
    if (g_event_queue) {
        xQueueSend(g_event_queue, &timeout_event, 0);
    }
    
    // Call timeout callback if registered
    if (g_timeout_callback) {
        g_timeout_callback(g_state_context.current_state);
    }
}

/**
 * Perform state transition
 */
static esp_err_t perform_state_transition(system_state_t new_state)
{
    system_state_t old_state = g_state_context.current_state;
    
    if (old_state == new_state) {
        return ESP_OK; // No transition needed
    }
    
    ESP_LOGI(TAG, "State transition: %s -> %s", 
             state_machine_state_to_string(old_state),
             state_machine_state_to_string(new_state));
    
    // Update state context
    g_state_context.previous_state = old_state;
    g_state_context.current_state = new_state;
    g_state_context.state_enter_time = xTaskGetTickCount();
    
    // Update operational flag
    g_state_context.operational = (new_state == STATE_OPERATIONAL);
    
    // Stop current timeout timer
    if (g_state_context.timeout_timer) {
        xTimerStop(g_state_context.timeout_timer, 0);
    }
    
    // Start new timeout timer if needed
    uint32_t timeout_ms = get_state_timeout_ms(new_state);
    if (timeout_ms > 0) {
        if (g_state_context.timeout_timer) {
            xTimerChangePeriod(g_state_context.timeout_timer, 
                             pdMS_TO_TICKS(timeout_ms), 0);
            xTimerStart(g_state_context.timeout_timer, 0);
        }
    }
    
    // Call transition callback if registered
    if (g_transition_callback) {
        g_transition_callback(old_state, new_state);
    }
    
    return ESP_OK;
}

/**
 * State machine task
 */
static void state_machine_task(void *pvParameters)
{
    system_event_t event;
    
    ESP_LOGI(TAG, "State machine task started");
    
    while (1) {
        // Wait for events
        if (xQueueReceive(g_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            ESP_LOGD(TAG, "Received event: %s in state: %s",
                     state_machine_event_to_string(event),
                     state_machine_state_to_string(g_state_context.current_state));
            
            // Look up next state from transition table
            system_state_t next_state = state_transition_table[g_state_context.current_state][event];
            
            // Perform transition if needed
            if (next_state != g_state_context.current_state) {
                perform_state_transition(next_state);
            }
        }
    }
}

esp_err_t state_machine_init(void)
{
    ESP_LOGI(TAG, "Initializing state machine");
    
    // Initialize context
    memset(&g_state_context, 0, sizeof(g_state_context));
    g_state_context.current_state = STATE_SYSTEM_INIT;
    g_state_context.previous_state = STATE_SYSTEM_INIT;
    g_state_context.operational = false;
    
    // Create event queue
    g_event_queue = xQueueCreate(10, sizeof(system_event_t));
    if (!g_event_queue) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }
    
    // Create timeout timer
    g_state_context.timeout_timer = xTimerCreate("state_timeout",
                                                pdMS_TO_TICKS(1000),
                                                pdFALSE,
                                                NULL,
                                                state_timeout_timer_callback);
    if (!g_state_context.timeout_timer) {
        ESP_LOGE(TAG, "Failed to create timeout timer");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "State machine initialized");
    return ESP_OK;
}

esp_err_t state_machine_handle_event(system_event_t event)
{
    if (!g_event_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t result = xQueueSend(g_event_queue, &event, pdMS_TO_TICKS(100));
    return (result == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

system_state_t state_machine_get_current_state(void)
{
    return g_state_context.current_state;
}

system_state_t state_machine_get_previous_state(void)
{
    return g_state_context.previous_state;
}

bool state_machine_is_operational(void)
{
    return g_state_context.operational;
}

esp_err_t state_machine_force_state(system_state_t new_state)
{
    return perform_state_transition(new_state);
}

esp_err_t state_machine_register_transition_callback(state_transition_cb_t callback)
{
    g_transition_callback = callback;
    return ESP_OK;
}

esp_err_t state_machine_register_timeout_callback(state_timeout_cb_t callback)
{
    g_timeout_callback = callback;
    return ESP_OK;
}

const char* state_machine_state_to_string(system_state_t state)
{
    switch (state) {
        case STATE_SYSTEM_INIT: return "SYSTEM_INIT";
        case STATE_WIFI_CONNECTING: return "WIFI_CONNECTING";
        case STATE_DHCP_REQUESTING: return "DHCP_REQUESTING";
        case STATE_NETWORK_READY: return "NETWORK_READY";
        case STATE_UDP_STARTING: return "UDP_STARTING";
        case STATE_UDP_LISTENING: return "UDP_LISTENING";
        case STATE_OPERATIONAL: return "OPERATIONAL";
        case STATE_UDP_TIMEOUT: return "UDP_TIMEOUT";
        case STATE_WIFI_ERROR: return "WIFI_ERROR";
        case STATE_DHCP_ERROR: return "DHCP_ERROR";
        case STATE_UDP_ERROR: return "UDP_ERROR";
        case STATE_RECONNECTING: return "RECONNECTING";
        default: return "UNKNOWN";
    }
}

const char* state_machine_event_to_string(system_event_t event)
{
    switch (event) {
        case EVENT_SYSTEM_INIT_COMPLETE: return "SYSTEM_INIT_COMPLETE";
        case EVENT_WIFI_CONNECT_START: return "WIFI_CONNECT_START";
        case EVENT_WIFI_CONNECTED: return "WIFI_CONNECTED";
        case EVENT_WIFI_DISCONNECTED: return "WIFI_DISCONNECTED";
        case EVENT_WIFI_FAILED: return "WIFI_FAILED";
        case EVENT_DHCP_SUCCESS: return "DHCP_SUCCESS";
        case EVENT_DHCP_FAILED: return "DHCP_FAILED";
        case EVENT_NETWORK_READY: return "NETWORK_READY";
        case EVENT_UDP_START: return "UDP_START";
        case EVENT_UDP_STARTED: return "UDP_STARTED";
        case EVENT_UDP_FAILED: return "UDP_FAILED";
        case EVENT_UDP_LISTENING: return "UDP_LISTENING";
        case EVENT_PING_RECEIVED: return "PING_RECEIVED";
        case EVENT_PING_TIMEOUT: return "PING_TIMEOUT";
        case EVENT_ERROR_RECOVERY: return "ERROR_RECOVERY";
        case EVENT_RECONNECT_START: return "RECONNECT_START";
        default: return "UNKNOWN";
    }
}

esp_err_t state_machine_start(void)
{
    if (g_state_task_handle) {
        ESP_LOGW(TAG, "State machine task already running");
        return ESP_OK;
    }

    BaseType_t result = xTaskCreate(state_machine_task,
                                   "state_machine",
                                   4096,
                                   NULL,
                                   5,
                                   &g_state_task_handle);

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create state machine task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "State machine task started");
    return ESP_OK;
}

esp_err_t state_machine_stop(void)
{
    if (g_state_task_handle) {
        vTaskDelete(g_state_task_handle);
        g_state_task_handle = NULL;
        ESP_LOGI(TAG, "State machine task stopped");
    }

    return ESP_OK;
}
