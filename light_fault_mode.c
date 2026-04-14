#include "light_fault_mode.h"

#include "light_protocol.h"

#define LIGHT_FAULT_MASK_SPEED_LIMIT   (1U << 0)
#define LIGHT_FAULT_MASK_MODE_CONFLICT (1U << 1)
#define LIGHT_FAULT_MASK_INVALID_CMD   (1U << 2)
#define LIGHT_FAULT_MASK_HW_STATE      (1U << 3)
#define LIGHT_FAULT_RECOVERY_WINDOW_TICKS 2U

static fault_mode_t derive_mode(const fault_counters_t *counters) {
    if (counters->hw_state_errors >= 2U) {
        return LIGHT_FAULT_MODE_SAFE_MODE;
    }

    if (counters->consecutive_mode_conflicts >= 3U) {
        return LIGHT_FAULT_MODE_DEGRADED;
    }

    if (counters->speed_limit_errors > 0U
        || counters->mode_conflict_errors > 0U
        || counters->invalid_cmd_errors > 0U
        || counters->hw_state_errors > 0U) {
        return LIGHT_FAULT_MODE_WARN;
    }

    return LIGHT_FAULT_MODE_NORMAL;
}

static fault_mode_t fault_mode_step_down(fault_mode_t mode) {
    switch (mode) {
        case LIGHT_FAULT_MODE_SAFE_MODE:
            return LIGHT_FAULT_MODE_DEGRADED;
        case LIGHT_FAULT_MODE_DEGRADED:
            return LIGHT_FAULT_MODE_WARN;
        case LIGHT_FAULT_MODE_WARN:
            return LIGHT_FAULT_MODE_NORMAL;
        case LIGHT_FAULT_MODE_NORMAL:
        default:
            return LIGHT_FAULT_MODE_NORMAL;
    }
}

static uint8_t fault_mask_for_error(uint8_t error_code) {
    switch (error_code) {
        case LIGHT_ERR_SPEED_LIMIT:
            return LIGHT_FAULT_MASK_SPEED_LIMIT;
        case LIGHT_ERR_MODE_CONFLICT:
            return LIGHT_FAULT_MASK_MODE_CONFLICT;
        case LIGHT_ERR_INVALID_CMD:
            return LIGHT_FAULT_MASK_INVALID_CMD;
        case LIGHT_ERR_HW_STATE_ERR:
            return LIGHT_FAULT_MASK_HW_STATE;
        default:
            return 0U;
    }
}

static void fault_counters_reset(fault_counters_t *counters) {
    counters->total_errors = 0U;
    counters->speed_limit_errors = 0U;
    counters->mode_conflict_errors = 0U;
    counters->invalid_cmd_errors = 0U;
    counters->hw_state_errors = 0U;
    counters->consecutive_mode_conflicts = 0U;
}

static void apply_error_to_counters(fault_counters_t *counters, uint8_t error_code) {
    counters->total_errors++;

    switch (error_code) {
        case LIGHT_ERR_SPEED_LIMIT:
            counters->speed_limit_errors++;
            counters->consecutive_mode_conflicts = 0U;
            break;
        case LIGHT_ERR_MODE_CONFLICT:
            counters->mode_conflict_errors++;
            counters->consecutive_mode_conflicts++;
            break;
        case LIGHT_ERR_INVALID_CMD:
            counters->invalid_cmd_errors++;
            counters->consecutive_mode_conflicts = 0U;
            break;
        case LIGHT_ERR_HW_STATE_ERR:
            counters->hw_state_errors++;
            counters->consecutive_mode_conflicts = 0U;
            break;
        default:
            counters->consecutive_mode_conflicts = 0U;
            break;
    }
}

static fault_decision_t make_decision(const light_fault_state_t *state,
                                      fault_mode_t previous_mode,
                                      light_fault_lifecycle_t previous_lifecycle) {
    fault_decision_t decision;

    decision.previous_mode = previous_mode;
    decision.current_mode = state->mode;
    decision.previous_lifecycle = previous_lifecycle;
    decision.current_lifecycle = state->lifecycle;
    decision.mode_changed = previous_mode != state->mode;
    decision.lifecycle_changed = previous_lifecycle != state->lifecycle;

    return decision;
}

light_fault_state_t light_fault_state_init(void) {
    light_fault_state_t state;

    light_fault_state_reset(&state);

    return state;
}

void light_fault_state_reset(light_fault_state_t *state) {
    state->mode = LIGHT_FAULT_MODE_NORMAL;
    fault_counters_reset(&state->counters);
    fault_counters_reset(&state->active_counters);
    state->lifecycle = LIGHT_FAULT_LIFECYCLE_STABLE;
    state->active_fault_mask = 0U;
    state->recovery_ticks = 0U;
    state->last_fault_code = 0U;
}

fault_decision_t light_fault_mode_record_error(light_fault_state_t *state, uint8_t error_code) {
    fault_mode_t previous_mode = state->mode;
    light_fault_lifecycle_t previous_lifecycle = state->lifecycle;
    fault_mode_t derived_mode;

    state->last_fault_code = error_code;
    apply_error_to_counters(&state->counters, error_code);
    apply_error_to_counters(&state->active_counters, error_code);
    state->active_fault_mask |= fault_mask_for_error(error_code);
    state->recovery_ticks = 0U;
    state->lifecycle = LIGHT_FAULT_LIFECYCLE_ACTIVE;

    derived_mode = derive_mode(&state->active_counters);
    if (previous_lifecycle == LIGHT_FAULT_LIFECYCLE_RECOVERING
        && previous_mode > derived_mode) {
        state->mode = previous_mode;
    } else {
        state->mode = derived_mode;
    }

    return make_decision(state, previous_mode, previous_lifecycle);
}

fault_decision_t light_fault_mode_clear_active(light_fault_state_t *state) {
    fault_mode_t previous_mode = state->mode;
    light_fault_lifecycle_t previous_lifecycle = state->lifecycle;

    if (state->active_fault_mask == 0U) {
        return make_decision(state, previous_mode, previous_lifecycle);
    }

    fault_counters_reset(&state->active_counters);
    state->active_fault_mask = 0U;
    state->recovery_ticks = 0U;

    if (state->mode == LIGHT_FAULT_MODE_NORMAL) {
        state->lifecycle = LIGHT_FAULT_LIFECYCLE_STABLE;
    } else {
        state->lifecycle = LIGHT_FAULT_LIFECYCLE_RECOVERING;
    }

    return make_decision(state, previous_mode, previous_lifecycle);
}

fault_decision_t light_fault_mode_observe_recovery(light_fault_state_t *state) {
    fault_mode_t previous_mode = state->mode;
    light_fault_lifecycle_t previous_lifecycle = state->lifecycle;

    if (state->lifecycle != LIGHT_FAULT_LIFECYCLE_RECOVERING
        || state->active_fault_mask != 0U) {
        return make_decision(state, previous_mode, previous_lifecycle);
    }

    state->recovery_ticks++;
    if (state->recovery_ticks < LIGHT_FAULT_RECOVERY_WINDOW_TICKS) {
        return make_decision(state, previous_mode, previous_lifecycle);
    }

    state->recovery_ticks = 0U;
    state->mode = fault_mode_step_down(state->mode);
    if (state->mode == LIGHT_FAULT_MODE_NORMAL) {
        state->lifecycle = LIGHT_FAULT_LIFECYCLE_STABLE;
    }

    return make_decision(state, previous_mode, previous_lifecycle);
}

light_fault_event_t light_fault_event_create(uint8_t error_code, fault_mode_t current_mode) {
    light_fault_event_t event;

    event.error_code = error_code;
    event.current_mode = current_mode;

    return event;
}

uint8_t light_fault_mode_transport_encode(fault_mode_t mode) {
    switch (mode) {
        case LIGHT_FAULT_MODE_NORMAL:
        case LIGHT_FAULT_MODE_WARN:
        case LIGHT_FAULT_MODE_DEGRADED:
        case LIGHT_FAULT_MODE_SAFE_MODE:
            return (uint8_t)mode;
        default:
            return (uint8_t)LIGHT_FAULT_MODE_NORMAL;
    }
}

fault_mode_t light_fault_mode_transport_decode(uint8_t raw_mode) {
    switch (raw_mode) {
        case LIGHT_FAULT_MODE_NORMAL:
        case LIGHT_FAULT_MODE_WARN:
        case LIGHT_FAULT_MODE_DEGRADED:
        case LIGHT_FAULT_MODE_SAFE_MODE:
            return (fault_mode_t)raw_mode;
        default:
            return LIGHT_FAULT_MODE_NORMAL;
    }
}

void light_fault_mode_transport_store(volatile uint8_t *slot, fault_mode_t mode) {
    *slot = light_fault_mode_transport_encode(mode);
}

fault_mode_t light_fault_mode_transport_load(volatile const uint8_t *slot) {
    return light_fault_mode_transport_decode(*slot);
}

const char *light_fault_mode_name(fault_mode_t mode) {
    switch (mode) {
        case LIGHT_FAULT_MODE_NORMAL:
            return "NORMAL";
        case LIGHT_FAULT_MODE_WARN:
            return "WARN";
        case LIGHT_FAULT_MODE_DEGRADED:
            return "DEGRADED";
        case LIGHT_FAULT_MODE_SAFE_MODE:
            return "SAFE_MODE";
        default:
            return "UNKNOWN";
    }
}

const char *light_fault_lifecycle_name(light_fault_lifecycle_t lifecycle) {
    switch (lifecycle) {
        case LIGHT_FAULT_LIFECYCLE_STABLE:
            return "STABLE";
        case LIGHT_FAULT_LIFECYCLE_ACTIVE:
            return "ACTIVE";
        case LIGHT_FAULT_LIFECYCLE_RECOVERING:
            return "RECOVERING";
        default:
            return "UNKNOWN";
    }
}

uint8_t light_fault_recovery_window_ticks(void) {
    return LIGHT_FAULT_RECOVERY_WINDOW_TICKS;
}
