/**
 * @file fault_mgmt.c
 * @brief Fault management component
 */

#include <stdint.h>
#include <stdbool.h>
#include <microkit.h>
#include <stddef.h>

#include "printf.h"
#include "light_contract.h"
#include "light_fault_mode.h"
#include "logger.h"
#include "light_protocol.h"
#include "light_transport.h"

#define DEMO_ANSI_RESET  "\x1b[0m"
#define DEMO_ANSI_GREEN  "\x1b[1;32m"
#define DEMO_ANSI_YELLOW "\x1b[1;33m"
#define DEMO_ANSI_RED    "\x1b[1;31m"

uintptr_t test_input_buffer;
uintptr_t fault_mode_shared_vaddr;
uintptr_t shared_memory_base_vaddr;

uint32_t total_error_count = 0;
static light_fault_state_t g_fault_state;
static uint32_t g_fault_history_sequence = 0U;
static light_shmem_t *g_shmem = NULL;

static void publish_fault_state(void) {
    if (g_shmem != NULL) {
        g_shmem->fault_mode = (uint8_t)g_fault_state.mode;
        g_shmem->fault_lifecycle = (uint8_t)g_fault_state.lifecycle;
        g_shmem->fault_recovery_ticks = g_fault_state.recovery_ticks;
        g_shmem->fault_recovery_elapsed_ms = g_fault_state.recovery_elapsed_ms;
        g_shmem->fault_recovery_window_ms = light_fault_recovery_window_ms();
        g_shmem->active_fault_mask = g_fault_state.active_fault_mask;
        g_shmem->last_fault_code = g_fault_state.last_fault_code;
        g_shmem->total_fault_count = total_error_count;
    }

    light_fault_mode_transport_store((volatile uint8_t *)fault_mode_shared_vaddr, g_fault_state.mode);
    microkit_notify(LIGHT_CH_FAULTMG_TO_GPIO);
    microkit_notify(LIGHT_CH_FAULTMG_TO_SCHEDULER);
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
        case LIGHT_CH_FAULTMG_FROM_LIGHTCTL:
            return "lightctl";
        case LIGHT_CH_FAULTMG_FROM_COMMANDIN:
            return "commandin";
        default:
            return "unknown";
    }
}

static void log_fault_decision(const char *tag, fault_decision_t decision, uint8_t error_code) {
    LOG_INFO("%s prev=%s next=%s changed=%d lifecycle_prev=%s lifecycle_next=%s lifecycle_changed=%d code=0x%02x total=%u active=0x%02x recovery_ticks=%u/%u recovery_elapsed_ms=%u recovery_window_ms=%u",
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
             (unsigned int)light_fault_recovery_window_ticks(),
             (unsigned int)g_fault_state.recovery_elapsed_ms,
             (unsigned int)light_fault_recovery_window_ms());
}

static void log_fault_history(light_fault_history_event_type_t event_type, uint8_t error_code) {
    g_fault_history_sequence++;
    LOG_INFO("FAULTMG_HISTORY seq=%u event=%s code=0x%02x code_name=%s mode=%s lifecycle=%s active=0x%02x total=%u",
             (unsigned int)g_fault_history_sequence,
             light_fault_history_event_type_name(event_type),
             (unsigned int)error_code,
             light_fault_code_name(error_code),
             light_fault_mode_name(g_fault_state.mode),
             light_fault_lifecycle_name(g_fault_state.lifecycle),
             (unsigned int)g_fault_state.active_fault_mask,
             total_error_count);
}

static void handle_fault_event(microkit_channel source_channel, uint8_t error_code) {
    fault_decision_t decision;
    light_fault_event_t event;
    const light_fault_taxonomy_entry_t *taxonomy;

    total_error_count++;
    decision = light_fault_mode_record_error(&g_fault_state, error_code);
    event = light_fault_event_create(error_code, g_fault_state.mode);
    taxonomy = light_fault_taxonomy_lookup(error_code);

    LOG_INFO("FAULTMG_EVENT source=%s code=0x%02x name=%s severity=%s recovery_policy=%s output_policy=%s total=%u",
             fault_event_source_name(source_channel),
             event.error_code,
             taxonomy != NULL ? taxonomy->name : "UNKNOWN",
             taxonomy != NULL ? taxonomy->severity : "UNKNOWN",
             taxonomy != NULL ? taxonomy->recovery_policy : "UNKNOWN",
             taxonomy != NULL ? taxonomy->output_policy : "UNKNOWN",
             total_error_count);
    LOG_INFO("%sDEMO_FAULT%s event=INJECT source=%s code=%s severity=%s policy=%s total=%u",
             DEMO_ANSI_RED,
             DEMO_ANSI_RESET,
             fault_event_source_name(source_channel),
             taxonomy != NULL ? taxonomy->name : "UNKNOWN",
             taxonomy != NULL ? taxonomy->severity : "UNKNOWN",
             taxonomy != NULL ? taxonomy->recovery_policy : "UNKNOWN",
             total_error_count);
    log_fault_decision("FAULTMG_MODE_TRANSITION", decision, event.error_code);
    LOG_INFO("DEMO_RESULT stage=faultmg prev=%s next=%s lifecycle=%s active=0x%02x recovery=%u/%u_ms",
             light_fault_mode_name(decision.previous_mode),
             light_fault_mode_name(decision.current_mode),
             light_fault_lifecycle_name(decision.current_lifecycle),
             (unsigned int)g_fault_state.active_fault_mask,
             (unsigned int)g_fault_state.recovery_elapsed_ms,
             (unsigned int)light_fault_recovery_window_ms());
    log_fault_history(LIGHT_FAULT_HISTORY_EVENT_ERROR, event.error_code);
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
        LOG_INFO("%sDEMO_FAULT%s event=CLEAR prev=%s next=%s lifecycle=%s recovery=%u/%u_ms",
                 DEMO_ANSI_GREEN,
                 DEMO_ANSI_RESET,
                 light_fault_mode_name(decision.previous_mode),
                 light_fault_mode_name(decision.current_mode),
                 light_fault_lifecycle_name(decision.current_lifecycle),
                 (unsigned int)g_fault_state.recovery_elapsed_ms,
                 (unsigned int)light_fault_recovery_window_ms());
        log_fault_history(LIGHT_FAULT_HISTORY_EVENT_CLEAR, 0U);
    } else if (g_fault_state.lifecycle == LIGHT_FAULT_LIFECYCLE_RECOVERING) {
        decision = light_fault_mode_observe_recovery(&g_fault_state);
        log_fault_decision("FAULTMG_RECOVERY_TICK", decision, 0U);
        LOG_INFO("%sDEMO_FAULT%s event=RECOVERY_TICK prev=%s next=%s lifecycle=%s recovery=%u/%u_ms",
                 DEMO_ANSI_YELLOW,
                 DEMO_ANSI_RESET,
                 light_fault_mode_name(decision.previous_mode),
                 light_fault_mode_name(decision.current_mode),
                 light_fault_lifecycle_name(decision.current_lifecycle),
                 (unsigned int)g_fault_state.recovery_elapsed_ms,
                 (unsigned int)light_fault_recovery_window_ms());
        log_fault_history(LIGHT_FAULT_HISTORY_EVENT_RECOVERY_TICK, 0U);
    } else {
        decision = light_fault_mode_clear_active(&g_fault_state);
        log_fault_decision("FAULTMG_CLEAR", decision, 0U);
        LOG_INFO("%sDEMO_FAULT%s event=CLEAR prev=%s next=%s lifecycle=%s recovery=%u/%u_ms",
                 DEMO_ANSI_GREEN,
                 DEMO_ANSI_RESET,
                 light_fault_mode_name(decision.previous_mode),
                 light_fault_mode_name(decision.current_mode),
                 light_fault_lifecycle_name(decision.current_lifecycle),
                 (unsigned int)g_fault_state.recovery_elapsed_ms,
                 (unsigned int)light_fault_recovery_window_ms());
        log_fault_history(LIGHT_FAULT_HISTORY_EVENT_CLEAR, 0U);
    }

    publish_fault_state();
}

void init(void) {
    g_shmem = (light_shmem_t *)shared_memory_base_vaddr;
    light_fault_state_reset(&g_fault_state);
    g_fault_history_sequence = 0U;
    total_error_count = 0;
    publish_fault_state();
    {
        light_contract_check_t contract =
            light_contract_check_fault_snapshot((uint8_t)g_fault_state.mode,
                                                (uint8_t)g_fault_state.lifecycle,
                                                g_fault_state.recovery_ticks,
                                                g_fault_state.recovery_elapsed_ms,
                                                g_fault_state.active_fault_mask);
        LOG_INFO("FAULTMG_CONTRACT fault_snapshot=%s expected=%u actual=%u",
                 light_contract_status_name(contract.status),
                 (unsigned int)contract.expected,
                 (unsigned int)contract.actual);
    }
    LOG_INFO("FAULT_INIT module=faultmg status=ready");
    LOG_INFO("FAULT_MGMT: initialized\n");
}

void notified(microkit_channel channel) {
    if (channel == LIGHT_CH_FAULTMG_FROM_LIGHTCTL) {
        uint8_t error_code = (uint8_t)microkit_mr_get(0);

        handle_fault_event(channel, error_code);
        return;
    }

    if (channel == LIGHT_CH_FAULTMG_FROM_COMMANDIN) {
        light_transport_message_t message =
            *(volatile light_transport_message_t *)test_input_buffer;

        switch ((light_transport_msg_type_t)message.type) {
            case LIGHT_TRANSPORT_MSG_FAULT_INJECT:
            {
                light_contract_check_t contract =
                    light_contract_check_transport_message(message, LIGHT_TRANSPORT_MSG_FAULT_INJECT);
                if (contract.status != LIGHT_CONTRACT_OK) {
                    LOG_ERROR("FAULTMG_CONTRACT_REJECT reason=%s expected=%u actual=%u type=%u len=%u version=%u",
                              light_contract_status_name(contract.status),
                              (unsigned int)contract.expected,
                              (unsigned int)contract.actual,
                              (unsigned int)message.type,
                              (unsigned int)message.len,
                              (unsigned int)message.version);
                    return;
                }
                handle_fault_event(channel, message.payload.fault_error_code);
                return;
            }
            case LIGHT_TRANSPORT_MSG_FAULT_CLEAR:
            {
                light_contract_check_t contract =
                    light_contract_check_transport_message(message, LIGHT_TRANSPORT_MSG_FAULT_CLEAR);
                if (contract.status != LIGHT_CONTRACT_OK) {
                    LOG_ERROR("FAULTMG_CONTRACT_REJECT reason=%s expected=%u actual=%u type=%u len=%u version=%u",
                              light_contract_status_name(contract.status),
                              (unsigned int)contract.expected,
                              (unsigned int)contract.actual,
                              (unsigned int)message.type,
                              (unsigned int)message.len,
                              (unsigned int)message.version);
                    return;
                }
                handle_fault_clear(message.payload.fault_clear_scope);
                return;
            }
            default:
                LOG_ERROR("FAULTMG_CONTRACT_REJECT reason=TRANSPORT_TYPE expected=%u actual=%u type=%u len=%u version=%u",
                          (unsigned int)LIGHT_TRANSPORT_MSG_FAULT_INJECT,
                          (unsigned int)message.type,
                          (unsigned int)message.type,
                          (unsigned int)message.len,
                          (unsigned int)message.version);
                return;
        }
    }

    LOG_ERROR("FAULTMG_CONTRACT_REJECT reason=UNKNOWN_CHANNEL expected=%u actual=%u type=0 len=0 version=0",
              (unsigned int)LIGHT_CH_FAULTMG_FROM_COMMANDIN,
              (unsigned int)channel);
}
