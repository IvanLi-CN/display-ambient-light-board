#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "esp_err.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

/**
 * System states as defined in the plan
 */
typedef enum {
    STATE_SYSTEM_INIT,
    STATE_WIFI_CONNECTING,
    STATE_DHCP_REQUESTING,
    STATE_NETWORK_READY,
    STATE_UDP_STARTING,
    STATE_UDP_LISTENING,
    STATE_OPERATIONAL,
    STATE_UDP_TIMEOUT,
    STATE_WIFI_ERROR,
    STATE_DHCP_ERROR,
    STATE_UDP_ERROR,
    STATE_RECONNECTING
} system_state_t;

/**
 * System events that trigger state transitions
 */
typedef enum {
    EVENT_SYSTEM_INIT_COMPLETE,
    EVENT_WIFI_CONNECT_START,
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECTED,
    EVENT_WIFI_FAILED,
    EVENT_DHCP_SUCCESS,
    EVENT_DHCP_FAILED,
    EVENT_NETWORK_READY,
    EVENT_UDP_START,
    EVENT_UDP_STARTED,
    EVENT_UDP_FAILED,
    EVENT_UDP_LISTENING,
    EVENT_PING_RECEIVED,
    EVENT_PING_TIMEOUT,
    EVENT_ERROR_RECOVERY,
    EVENT_RECONNECT_START
} system_event_t;

/**
 * State machine context structure
 */
typedef struct {
    system_state_t current_state;
    system_state_t previous_state;
    TimerHandle_t timeout_timer;
    uint32_t state_enter_time;
    uint32_t error_count;
    bool operational;
} state_machine_context_t;

/**
 * State transition callback function type
 */
typedef esp_err_t (*state_transition_cb_t)(system_state_t from_state, system_state_t to_state);

/**
 * State timeout callback function type
 */
typedef esp_err_t (*state_timeout_cb_t)(system_state_t state);

/**
 * Initialize the state machine
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t state_machine_init(void);

/**
 * Handle a system event and potentially trigger state transition
 * @param event The system event to handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t state_machine_handle_event(system_event_t event);

/**
 * Get the current system state
 * @return Current system state
 */
system_state_t state_machine_get_current_state(void);

/**
 * Get the previous system state
 * @return Previous system state
 */
system_state_t state_machine_get_previous_state(void);

/**
 * Check if the system is operational
 * @return true if operational, false otherwise
 */
bool state_machine_is_operational(void);

/**
 * Force a state transition (for testing/debugging)
 * @param new_state The state to transition to
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t state_machine_force_state(system_state_t new_state);

/**
 * Register state transition callback
 * @param callback The callback function to register
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t state_machine_register_transition_callback(state_transition_cb_t callback);

/**
 * Register state timeout callback
 * @param callback The callback function to register
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t state_machine_register_timeout_callback(state_timeout_cb_t callback);

/**
 * Get string representation of state
 * @param state The state to convert
 * @return String representation of the state
 */
const char* state_machine_state_to_string(system_state_t state);

/**
 * Get string representation of event
 * @param event The event to convert
 * @return String representation of the event
 */
const char* state_machine_event_to_string(system_event_t event);

/**
 * Start the state machine task
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t state_machine_start(void);

/**
 * Stop the state machine task
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t state_machine_stop(void);

#endif // STATE_MACHINE_H
