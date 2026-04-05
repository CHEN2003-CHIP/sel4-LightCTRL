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
} light_fault_state_t;

typedef struct {
    fault_mode_t previous_mode;
    fault_mode_t current_mode;
    bool mode_changed;
} fault_decision_t;

typedef struct {
    uint8_t error_code;
    fault_mode_t current_mode;
} light_fault_event_t;

light_fault_state_t light_fault_state_init(void);
void light_fault_state_reset(light_fault_state_t *state);
fault_decision_t light_fault_mode_record_error(light_fault_state_t *state, uint8_t error_code);
light_fault_event_t light_fault_event_create(uint8_t error_code, fault_mode_t current_mode);
uint8_t light_fault_mode_transport_encode(fault_mode_t mode);
fault_mode_t light_fault_mode_transport_decode(uint8_t raw_mode);
void light_fault_mode_transport_store(volatile uint8_t *slot, fault_mode_t mode);
fault_mode_t light_fault_mode_transport_load(volatile const uint8_t *slot);
const char *light_fault_mode_name(fault_mode_t mode);

#endif
