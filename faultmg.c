/**
 * @file fault_mgmt.c
 * @brief Fault management component
 */

#include <stdint.h>
#include <stdbool.h>
#include <microkit.h>
#include <stddef.h>
#include "printf.h"
#include "light_fault_mode.h"
#include "logger.h"
#include "light_protocol.h"

#define FAULTMG_LIGHTCTL 5

uint32_t total_error_count = 0;
static light_fault_state_t g_fault_state;

static void print_error_details(uint8_t err_code) {
    switch (err_code) {
        case LIGHT_ERR_SPEED_LIMIT:
            LOG_WARN("FAULT_MGMT: warning - speed limit denied operation");
            break;
        case LIGHT_ERR_MODE_CONFLICT:
            LOG_WARN("FAULT_MGMT: warning - light mode conflict");
            break;
        case LIGHT_ERR_INVALID_CMD:
            LOG_WARN("FAULT_MGMT: warning - invalid command or channel");
            break;
        case LIGHT_ERR_HW_STATE_ERR:
            LOG_WARN("FAULT_MGMT: warning - hardware state mismatch");
            break;
        default:
            LOG_WARN("FAULT_MGMT: warning - unknown error code 0x%x", err_code);
            break;
    }
}

void init(void) {
    light_fault_state_reset(&g_fault_state);
    LOG_INFO("FAULT_INIT module=faultmg status=ready");
    LOG_INFO("FAULT_MGMT: initialized\n");
}

void notified(microkit_channel channel) {
    if (channel == FAULTMG_LIGHTCTL) {
        uint8_t error_code = (uint8_t) microkit_mr_get(0);
        fault_decision_t decision;

        total_error_count++;
        decision = light_fault_mode_record_error(&g_fault_state, error_code);

        LOG_INFO("FAULT_EVENT code=0x%02x total=%u",
                 error_code,
                 total_error_count);
        LOG_INFO("FAULT_MODE current=%s consecutive_mode_conflicts=%u hw_state=%u",
                 light_fault_mode_name(g_fault_state.mode),
                 g_fault_state.counters.consecutive_mode_conflicts,
                 g_fault_state.counters.hw_state_errors);
        if (decision.mode_changed) {
            LOG_WARN("FAULT_MODE transition %s -> %s",
                     light_fault_mode_name(decision.previous_mode),
                     light_fault_mode_name(decision.current_mode));
        }
        microkit_mr_set(0, g_fault_state.mode);
        microkit_notify(FAULTMG_LIGHTCTL);
        LOG_INFO("FAULT_MGMT: fault notification received (total: %d)", total_error_count);

        print_error_details(error_code);
    } else {
        LOG_ERROR("FAULTMG: unknown channel (channel: %d)\n", channel);
    }
}
