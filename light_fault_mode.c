#include "light_fault_mode.h"

#include "light_protocol.h"

static fault_mode_t derive_mode(const fault_counters_t *counters) {
    if (counters->hw_state_errors >= 2U) {
        return LIGHT_FAULT_MODE_SAFE_MODE;
    }

    if (counters->consecutive_mode_conflicts >= 3U) {
        return LIGHT_FAULT_MODE_DEGRADED;
    }

    if (counters->speed_limit_errors > 0U
        || counters->mode_conflict_errors > 0U
        || counters->invalid_cmd_errors > 0U) {
        return LIGHT_FAULT_MODE_WARN;
    }

    return LIGHT_FAULT_MODE_NORMAL;
}

light_fault_state_t light_fault_state_init(void) {
    light_fault_state_t state;

    light_fault_state_reset(&state);

    return state;
}

void light_fault_state_reset(light_fault_state_t *state) {
    state->mode = LIGHT_FAULT_MODE_NORMAL;
    state->counters.total_errors = 0;
    state->counters.speed_limit_errors = 0;
    state->counters.mode_conflict_errors = 0;
    state->counters.invalid_cmd_errors = 0;
    state->counters.hw_state_errors = 0;
    state->counters.consecutive_mode_conflicts = 0;
}

fault_decision_t light_fault_mode_record_error(light_fault_state_t *state, uint8_t error_code) {
    fault_decision_t decision;

    decision.previous_mode = state->mode;
    state->counters.total_errors++;

    switch (error_code) {
        case LIGHT_ERR_SPEED_LIMIT:
            state->counters.speed_limit_errors++;
            state->counters.consecutive_mode_conflicts = 0;
            break;
        case LIGHT_ERR_MODE_CONFLICT:
            state->counters.mode_conflict_errors++;
            state->counters.consecutive_mode_conflicts++;
            break;
        case LIGHT_ERR_INVALID_CMD:
            state->counters.invalid_cmd_errors++;
            state->counters.consecutive_mode_conflicts = 0;
            break;
        case LIGHT_ERR_HW_STATE_ERR:
            state->counters.hw_state_errors++;
            state->counters.consecutive_mode_conflicts = 0;
            break;
        default:
            state->counters.consecutive_mode_conflicts = 0;
            break;
    }

    state->mode = derive_mode(&state->counters);
    decision.current_mode = state->mode;
    decision.mode_changed = decision.previous_mode != decision.current_mode;

    return decision;
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
