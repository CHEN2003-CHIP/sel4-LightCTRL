#include <stdint.h>
#include <stdbool.h>
#include <microkit.h>
#include <stddef.h>
#include "include/logger.h"
#include "light_policy.h"
#include "light_protocol.h"

#define CH_UART_CMD 4
#define CH_LIGHT_CONTROL_ALLOW 9

uintptr_t shared_memory_base_vaddr;
uintptr_t input_buffer;

static light_shmem_t *g_shmem = NULL;

void init(void) {
    light_policy_state_t initial_state = light_policy_init_state();

    g_shmem = (light_shmem_t *)shared_memory_base_vaddr;
    g_shmem->uart_cmd = LIGHT_UART_CMD_INVALID;
    g_shmem->allow_flags = initial_state.allow_flags;
    g_shmem->turn_switch_pos = 0;
    g_shmem->beam_switch_pos = 0;
    g_shmem->vehicle_speed = initial_state.vehicle_speed;

    LOG_INFO("SCHED_INIT module=scheduler status=ready allow_flags=0x%02x speed=%u",
             (unsigned int)g_shmem->allow_flags,
             (unsigned int)g_shmem->vehicle_speed);

    LOG_INFO("g_shmem->allow_brake:%d \t g_shmem->allow_position: %d ",
             LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_BRAKE),
             LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_POSITION));

    LOG_INFO("Light scheduler initialized: position light allowed by default\n");
}

static void log_policy_result(light_policy_result_t result) {
    switch (result.cmd) {
        case LIGHT_CMD_LOW_BEAM_ON:
            LOG_INFO("Scheduler: UART cmd - Low beam ON allowed\n");
            break;
        case LIGHT_CMD_LOW_BEAM_OFF:
            LOG_INFO("Scheduler: UART cmd - Low beam OFF, high beam locked\n");
            break;
        case LIGHT_CMD_HIGH_BEAM_ON:
            if (result.accepted) {
                LOG_INFO("Scheduler: UART cmd - High beam ON allowed\n");
            } else if (result.reason == LIGHT_POLICY_REJECT_BRAKE_ACTIVE) {
                LOG_INFO("Scheduler: UART cmd - High beam ON denied (brake active)\n");
            } else if (result.reason == LIGHT_POLICY_REJECT_LOW_BEAM_REQUIRED) {
                LOG_INFO("Scheduler: UART cmd - High beam ON denied (low beam off)\n");
            }
            break;
        case LIGHT_CMD_HIGH_BEAM_OFF:
            LOG_INFO("Scheduler: UART cmd - High beam OFF\n");
            break;
        case LIGHT_CMD_LEFT_TURN_ON:
            if (result.accepted) {
                LOG_INFO("Scheduler: UART cmd - Left turn ON allowed\n");
            } else if (result.reason == LIGHT_POLICY_REJECT_RIGHT_TURN_ACTIVE) {
                LOG_INFO("Scheduler: UART cmd - Left turn ON denied (right turn active)\n");
            } else if (result.reason == LIGHT_POLICY_REJECT_BRAKE_ACTIVE) {
                LOG_INFO("Scheduler: UART cmd - Left turn ON denied (brake active)\n");
            }
            break;
        case LIGHT_CMD_LEFT_TURN_OFF:
            LOG_INFO("Scheduler: UART cmd - Left turn OFF\n");
            break;
        case LIGHT_CMD_RIGHT_TURN_ON:
            if (result.accepted) {
                LOG_INFO("Scheduler: UART cmd - Right turn ON allowed\n");
            } else if (result.reason == LIGHT_POLICY_REJECT_LEFT_TURN_ACTIVE) {
                LOG_INFO("Scheduler: UART cmd - Right turn ON denied (left turn active)\n");
            } else if (result.reason == LIGHT_POLICY_REJECT_BRAKE_ACTIVE) {
                LOG_INFO("Scheduler: UART cmd - Right turn ON denied (brake active)\n");
            }
            break;
        case LIGHT_CMD_RIGHT_TURN_OFF:
            LOG_INFO("Scheduler: UART cmd - Right turn OFF\n");
            break;
        case LIGHT_CMD_POSITION_ON:
            LOG_INFO("Scheduler: UART cmd - Position light ON allowed\n");
            break;
        case LIGHT_CMD_POSITION_OFF:
            LOG_INFO("Scheduler: UART cmd - Position light OFF\n");
            break;
        case LIGHT_CMD_BRAKE_ON:
            LOG_INFO("Scheduler: UART cmd - Brake ON allowed\n");
            break;
        case LIGHT_CMD_BRAKE_OFF:
            LOG_INFO("Scheduler: UART cmd - Brake OFF\n");
            break;
        default:
            LOG_INFO("Scheduler: UART cmd - Invalid command received\n");
            break;
    }
}

void notified(microkit_channel ch) {
    bool need_notify_light_control = false;
    uint32_t prev_allow_flags = g_shmem->allow_flags;
    uint8_t cmd = LIGHT_UART_CMD_INVALID;

    if (ch == CH_UART_CMD) {
        light_policy_state_t policy_state;
        light_policy_result_t result;

        cmd = *(uint8_t *)input_buffer;
        policy_state.allow_flags = g_shmem->allow_flags;
        policy_state.vehicle_speed = g_shmem->vehicle_speed;
        result = light_policy_apply_command(policy_state, cmd);
        g_shmem->allow_flags = result.next_allow_flags;
        need_notify_light_control = result.notify;
        log_policy_result(result);
        g_shmem->uart_cmd = LIGHT_UART_CMD_INVALID;
    } else {
        LOG_INFO("Scheduler: Unknown channel received\n");
    }

    LOG_INFO("SCHED_APPLY cmd=0x%02x allow_flags=0x%02x->0x%02x notify=%d",
             (unsigned int)cmd,
             (unsigned int)prev_allow_flags,
             (unsigned int)g_shmem->allow_flags,
             need_notify_light_control ? 1 : 0);

    if (need_notify_light_control) {
        microkit_notify(CH_LIGHT_CONTROL_ALLOW);
    }
}
