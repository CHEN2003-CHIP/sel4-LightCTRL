#ifndef LIGHT_FAULT_MODE_H
#define LIGHT_FAULT_MODE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LIGHT_FAULT_MODE_NORMAL = 0,
    LIGHT_FAULT_MODE_WARN = 1,
    LIGHT_FAULT_MODE_DEGRADED = 2,
    LIGHT_FAULT_MODE_SAFE_MODE = 3,
} fault_mode_t;

typedef enum {
    LIGHT_FAULT_LIFECYCLE_STABLE = 0,
    LIGHT_FAULT_LIFECYCLE_ACTIVE = 1,
    LIGHT_FAULT_LIFECYCLE_RECOVERING = 2,
} light_fault_lifecycle_t;

typedef struct {
    uint32_t total_errors;
    uint32_t speed_limit_errors;
    uint32_t mode_conflict_errors;
    uint32_t invalid_cmd_errors;
    uint32_t hw_state_errors;
    uint32_t consecutive_mode_conflicts;
} fault_counters_t;

typedef struct {
    fault_mode_t mode;
    fault_counters_t counters;
    fault_counters_t active_counters;
    light_fault_lifecycle_t lifecycle;
    uint8_t active_fault_mask;
    uint8_t recovery_ticks;
    uint8_t last_fault_code;
} light_fault_state_t;

typedef struct {
    fault_mode_t previous_mode;
    fault_mode_t current_mode;
    light_fault_lifecycle_t previous_lifecycle;
    light_fault_lifecycle_t current_lifecycle;
    bool mode_changed;
    bool lifecycle_changed;
} fault_decision_t;

typedef struct {
    uint8_t error_code;
    fault_mode_t current_mode;
} light_fault_event_t;

light_fault_state_t light_fault_state_init(void);
void light_fault_state_reset(light_fault_state_t *state);
fault_decision_t light_fault_mode_record_error(light_fault_state_t *state, uint8_t error_code);
fault_decision_t light_fault_mode_clear_active(light_fault_state_t *state);
fault_decision_t light_fault_mode_observe_recovery(light_fault_state_t *state);
light_fault_event_t light_fault_event_create(uint8_t error_code, fault_mode_t current_mode);
uint8_t light_fault_mode_transport_encode(fault_mode_t mode);
fault_mode_t light_fault_mode_transport_decode(uint8_t raw_mode);
void light_fault_mode_transport_store(volatile uint8_t *slot, fault_mode_t mode);
fault_mode_t light_fault_mode_transport_load(volatile const uint8_t *slot);
const char *light_fault_mode_name(fault_mode_t mode);
const char *light_fault_lifecycle_name(light_fault_lifecycle_t lifecycle);
uint8_t light_fault_recovery_window_ticks(void);

#endif
