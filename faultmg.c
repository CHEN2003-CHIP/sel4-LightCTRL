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
#define FAULTMG_GPIO 7
#define FAULTMG_TEST_INPUT 12

uintptr_t test_input_buffer;
uintptr_t fault_mode_shared_vaddr;

uint32_t total_error_count = 0;
static light_fault_state_t g_fault_state;

static void publish_fault_mode(fault_mode_t mode) {
    light_fault_mode_transport_store((volatile uint8_t *)fault_mode_shared_vaddr, mode);
    microkit_notify(FAULTMG_LIGHTCTL);
    microkit_notify(FAULTMG_GPIO);
}

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

static const char *fault_event_source_name(microkit_channel channel) {
    switch (channel) {
        case FAULTMG_LIGHTCTL:
            return "lightctl";
        case FAULTMG_TEST_INPUT:
            return "test";
        default:
            return "unknown";
    }
}

static void handle_fault_event(microkit_channel source_channel, uint8_t error_code) {
    fault_decision_t decision;
    light_fault_event_t event;

    total_error_count++;
    decision = light_fault_mode_record_error(&g_fault_state, error_code);
    event = light_fault_event_create(error_code, g_fault_state.mode);

    LOG_INFO("FAULTMG_EVENT source=%s code=0x%02x total=%u",
             fault_event_source_name(source_channel),
             event.error_code,
             total_error_count);
    LOG_INFO("FAULTMG_MODE_TRANSITION prev=%s next=%s changed=%d code=0x%02x total=%u",
             light_fault_mode_name(decision.previous_mode),
             light_fault_mode_name(decision.current_mode),
             decision.mode_changed ? 1 : 0,
             event.error_code,
             total_error_count);
    print_error_details(error_code);
    publish_fault_mode(event.current_mode);
}

void init(void) {
    light_fault_state_reset(&g_fault_state);
    publish_fault_mode(g_fault_state.mode);
    LOG_INFO("FAULT_INIT module=faultmg status=ready");
    LOG_INFO("FAULT_MGMT: initialized\n");
}

void notified(microkit_channel channel) {
    if (channel == FAULTMG_LIGHTCTL || channel == FAULTMG_TEST_INPUT) {
        uint8_t error_code = 0;

        if (channel == FAULTMG_TEST_INPUT) {
            error_code = *(volatile uint8_t *)test_input_buffer;
        } else {
            error_code = (uint8_t) microkit_mr_get(0);
        }

        handle_fault_event(channel, error_code);
    } else {
        LOG_ERROR("FAULTMG: unknown channel (channel: %d)\n", channel);
    }
}
