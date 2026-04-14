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
#include "light_transport.h"

#define FAULTMG_LIGHTCTL 5
#define FAULTMG_GPIO 7
#define FAULTMG_SCHEDULER 14
#define FAULTMG_TEST_INPUT 12

uintptr_t test_input_buffer;
uintptr_t fault_mode_shared_vaddr;
uintptr_t shared_memory_base_vaddr;

uint32_t total_error_count = 0;
static light_fault_state_t g_fault_state;
static light_shmem_t *g_shmem = NULL;

static void publish_fault_state(void) {
    if (g_shmem != NULL) {
        g_shmem->fault_mode = (uint8_t)g_fault_state.mode;
        g_shmem->fault_lifecycle = (uint8_t)g_fault_state.lifecycle;
        g_shmem->fault_recovery_ticks = g_fault_state.recovery_ticks;
        g_shmem->active_fault_mask = g_fault_state.active_fault_mask;
        g_shmem->last_fault_code = g_fault_state.last_fault_code;
        g_shmem->total_fault_count = total_error_count;
    }

    light_fault_mode_transport_store((volatile uint8_t *)fault_mode_shared_vaddr, g_fault_state.mode);
    microkit_notify(FAULTMG_GPIO);
    microkit_notify(FAULTMG_SCHEDULER);
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
            return "commandin";
        default:
            return "unknown";
    }
}

static void log_fault_decision(const char *tag, fault_decision_t decision, uint8_t error_code) {
    LOG_INFO("%s prev=%s next=%s changed=%d lifecycle_prev=%s lifecycle_next=%s lifecycle_changed=%d code=0x%02x total=%u active=0x%02x recovery=%u/%u",
             tag,
             light_fault_mode_name(decision.previous_mode),
             light_fault_mode_name(decision.current_mode),
             decision.mode_changed ? 1 : 0,
             light_fault_lifecycle_name(decision.previous_lifecycle),
             light_fault_lifecycle_name(decision.current_lifecycle),
             decision.lifecycle_changed ? 1 : 0,
             (unsigned int)error_code,
             total_error_count,
             (unsigned int)g_fault_state.active_fault_mask,
             (unsigned int)g_fault_state.recovery_ticks,
             (unsigned int)light_fault_recovery_window_ticks());
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
    log_fault_decision("FAULTMG_MODE_TRANSITION", decision, event.error_code);
    print_error_details(error_code);
    publish_fault_state();
}

static void handle_fault_clear(uint8_t scope) {
    fault_decision_t decision;

    if (scope != LIGHT_TRANSPORT_FAULT_CLEAR_ALL) {
        LOG_WARN("FAULTMG_CLEAR invalid_scope=%u mode=%s lifecycle=%s",
                 (unsigned int)scope,
                 light_fault_mode_name(g_fault_state.mode),
                 light_fault_lifecycle_name(g_fault_state.lifecycle));
        return;
    }

    if (g_fault_state.active_fault_mask != 0U) {
        decision = light_fault_mode_clear_active(&g_fault_state);
        log_fault_decision("FAULTMG_CLEAR", decision, 0U);
    } else if (g_fault_state.lifecycle == LIGHT_FAULT_LIFECYCLE_RECOVERING) {
        decision = light_fault_mode_observe_recovery(&g_fault_state);
        log_fault_decision("FAULTMG_RECOVERY_TICK", decision, 0U);
    } else {
        decision = light_fault_mode_clear_active(&g_fault_state);
        log_fault_decision("FAULTMG_CLEAR", decision, 0U);
    }

    publish_fault_state();
}

void init(void) {
    g_shmem = (light_shmem_t *)shared_memory_base_vaddr;
    light_fault_state_reset(&g_fault_state);
    total_error_count = 0;
    publish_fault_state();
    LOG_INFO("FAULT_INIT module=faultmg status=ready");
    LOG_INFO("FAULT_MGMT: initialized\n");
}

void notified(microkit_channel channel) {
    if (channel == FAULTMG_LIGHTCTL) {
        uint8_t error_code = (uint8_t)microkit_mr_get(0);

        handle_fault_event(channel, error_code);
        return;
    }

    if (channel == FAULTMG_TEST_INPUT) {
        light_transport_message_t message =
            *(volatile light_transport_message_t *)test_input_buffer;

        if (message.version != LIGHT_TRANSPORT_VERSION) {
            LOG_ERROR("FAULTMG: invalid transport version=%u",
                      (unsigned int)message.version);
            return;
        }

        switch ((light_transport_msg_type_t)message.type) {
            case LIGHT_TRANSPORT_MSG_FAULT_INJECT:
                if (message.len != sizeof(message.payload.fault_error_code)) {
                    LOG_ERROR("FAULTMG: invalid fault inject len=%u",
                              (unsigned int)message.len);
                    return;
                }
                handle_fault_event(channel, message.payload.fault_error_code);
                return;
            case LIGHT_TRANSPORT_MSG_FAULT_CLEAR:
                if (message.len != sizeof(message.payload.fault_clear_scope)) {
                    LOG_ERROR("FAULTMG: invalid fault clear len=%u",
                              (unsigned int)message.len);
                    return;
                }
                handle_fault_clear(message.payload.fault_clear_scope);
                return;
            default:
                LOG_ERROR("FAULTMG: unsupported transport message type=%u len=%u",
                          (unsigned int)message.type,
                          (unsigned int)message.len);
                return;
        }
    }

    LOG_ERROR("FAULTMG: unknown channel (channel: %d)\n", channel);
}
