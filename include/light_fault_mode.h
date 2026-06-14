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

#define LIGHT_FAULT_HISTORY_CAPACITY 4U

typedef enum {
    LIGHT_FAULT_HISTORY_EVENT_ERROR = 1,
    LIGHT_FAULT_HISTORY_EVENT_CLEAR = 2,
    LIGHT_FAULT_HISTORY_EVENT_RECOVERY_TICK = 3,
} light_fault_history_event_type_t;

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
    uint32_t recovery_window_start_ms;
    uint32_t recovery_elapsed_ms;
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

typedef struct {
    uint32_t sequence;
    light_fault_history_event_type_t event_type;
    uint8_t error_code;
    fault_mode_t mode;
    light_fault_lifecycle_t lifecycle;
    uint8_t active_fault_mask;
    uint32_t total_errors;
} light_fault_history_record_t;

typedef struct {
    light_fault_history_record_t records[LIGHT_FAULT_HISTORY_CAPACITY];
    uint8_t next_index;
    uint8_t count;
    uint32_t next_sequence;
} light_fault_history_t;

typedef struct {
    uint8_t code;
    const char *name;
    const char *source;
    const char *severity;
    const char *recovery_policy;
    const char *output_policy;
} light_fault_taxonomy_entry_t;

light_fault_state_t light_fault_state_init(void);
void light_fault_state_reset(light_fault_state_t *state);
fault_decision_t light_fault_mode_record_error(light_fault_state_t *state, uint8_t error_code);
fault_decision_t light_fault_mode_clear_active(light_fault_state_t *state);
fault_decision_t light_fault_mode_observe_recovery_at(light_fault_state_t *state, uint32_t now_ms);
fault_decision_t light_fault_mode_observe_recovery(light_fault_state_t *state);
light_fault_event_t light_fault_event_create(uint8_t error_code, fault_mode_t current_mode);
void light_fault_history_reset(light_fault_history_t *history);
void light_fault_history_record(light_fault_history_t *history,
                                light_fault_history_record_t *record,
                                light_fault_history_event_type_t event_type,
                                uint8_t error_code,
                                const light_fault_state_t *state,
                                uint32_t total_errors);
bool light_fault_history_latest(const light_fault_history_t *history,
                                light_fault_history_record_t *record);
uint8_t light_fault_mode_transport_encode(fault_mode_t mode);
fault_mode_t light_fault_mode_transport_decode(uint8_t raw_mode);
void light_fault_mode_transport_store(volatile uint8_t *slot, fault_mode_t mode);
fault_mode_t light_fault_mode_transport_load(volatile const uint8_t *slot);
const char *light_fault_mode_name(fault_mode_t mode);
const char *light_fault_lifecycle_name(light_fault_lifecycle_t lifecycle);
const char *light_fault_history_event_type_name(light_fault_history_event_type_t event_type);
const char *light_fault_code_name(uint8_t error_code);
const light_fault_taxonomy_entry_t *light_fault_taxonomy_lookup(uint8_t error_code);
uint8_t light_fault_recovery_window_ticks(void);
uint32_t light_fault_recovery_window_ms(void);

#endif
