#include <stdint.h>
#include <stdbool.h>
#include <microkit.h>
#include <stddef.h>
#include "include/logger.h"
#include "light_protocol.h"

#define CH_UART_CMD 4
#define CH_LIGHT_CONTROL_ALLOW 9

uintptr_t shared_memory_base_vaddr;
uintptr_t input_buffer;

static light_shmem_t *g_shmem = NULL;

void init(void) {
    g_shmem = (light_shmem_t *)shared_memory_base_vaddr;
    g_shmem->uart_cmd = LIGHT_UART_CMD_INVALID;

    g_shmem->allow_flags = 0;
    g_shmem->allow_flags |= LIGHT_ALLOW_POSITION;
    g_shmem->turn_switch_pos = 0;
    g_shmem->beam_switch_pos = 0;
    g_shmem->vehicle_speed = 10;

    LOG_INFO("g_shmem->allow_brake:%d \t g_shmem->allow_position: %d ",
             LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_BRAKE),
             LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_POSITION));

    LOG_INFO("Light scheduler initialized: position light allowed by default\n");
}

static bool process_uart_command(uint8_t cmd) {
    bool need_notify = false;

    switch (cmd) {
        case LIGHT_CMD_LOW_BEAM_ON:
            g_shmem->allow_flags |= LIGHT_ALLOW_LOW_BEAM;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Low beam ON allowed\n");
            break;

        case LIGHT_CMD_LOW_BEAM_OFF:
            g_shmem->allow_flags &= ~LIGHT_ALLOW_LOW_BEAM;
            g_shmem->allow_flags &= ~LIGHT_ALLOW_HIGH_BEAM;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Low beam OFF, high beam locked\n");
            break;

        case LIGHT_CMD_HIGH_BEAM_ON:
            if (LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_LOW_BEAM)) {
                if (!LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_BRAKE)) {
                    g_shmem->allow_flags |= LIGHT_ALLOW_HIGH_BEAM;
                    need_notify = true;
                    LOG_INFO("Scheduler: UART cmd - High beam ON allowed\n");
                } else {
                    LOG_INFO("Scheduler: UART cmd - High beam ON denied (brake active)\n");
                }
            } else {
                LOG_INFO("Scheduler: UART cmd - High beam ON denied (low beam off)\n");
            }
            break;

        case LIGHT_CMD_HIGH_BEAM_OFF:
            g_shmem->allow_flags &= ~LIGHT_ALLOW_HIGH_BEAM;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - High beam OFF\n");
            break;

        case LIGHT_CMD_LEFT_TURN_ON:
            if (!LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_BRAKE)) {
                if (!LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_TURN_RIGHT)) {
                    LOG_INFO("Pre-check: allow_turn_left=%d",
                             LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_TURN_LEFT));
                    g_shmem->allow_flags |= LIGHT_ALLOW_TURN_LEFT;
                    LOG_INFO("Post-check: allow_turn_left=%d",
                             LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_TURN_LEFT));
                    need_notify = true;
                    LOG_INFO("Scheduler: UART cmd - Left turn ON allowed\n");
                } else {
                    LOG_INFO("Scheduler: UART cmd - Left turn ON denied (right turn active)\n");
                }
            } else {
                LOG_INFO("Scheduler: UART cmd - Left turn ON denied (brake active)\n");
            }
            break;

        case LIGHT_CMD_LEFT_TURN_OFF:
            g_shmem->allow_flags &= ~LIGHT_ALLOW_TURN_LEFT;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Left turn OFF\n");
            break;

        case LIGHT_CMD_RIGHT_TURN_ON:
            if (!LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_BRAKE)) {
                if (!LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_TURN_LEFT)) {
                    g_shmem->allow_flags |= LIGHT_ALLOW_TURN_RIGHT;
                    need_notify = true;
                    LOG_INFO("Scheduler: UART cmd - Right turn ON allowed\n");
                } else {
                    LOG_INFO("Scheduler: UART cmd - Right turn ON denied (left turn active)\n");
                }
            } else {
                LOG_INFO("Scheduler: UART cmd - Right turn ON denied (brake active)\n");
            }
            break;

        case LIGHT_CMD_RIGHT_TURN_OFF:
            g_shmem->allow_flags &= ~LIGHT_ALLOW_TURN_RIGHT;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Right turn OFF\n");
            break;

        case LIGHT_CMD_POSITION_ON:
            g_shmem->allow_flags |= LIGHT_ALLOW_POSITION;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Position light ON allowed\n");
            break;

        case LIGHT_CMD_POSITION_OFF:
            g_shmem->allow_flags &= ~LIGHT_ALLOW_POSITION;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Position light OFF\n");
            break;

        case LIGHT_CMD_BRAKE_ON:
            g_shmem->allow_flags |= LIGHT_ALLOW_BRAKE;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Brake ON allowed\n");
            break;

        case LIGHT_CMD_BRAKE_OFF:
            g_shmem->allow_flags &= ~LIGHT_ALLOW_BRAKE;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Brake OFF\n");
            break;

        default:
            LOG_INFO("Scheduler: UART cmd - Invalid command received\n");
            break;
    }

    return need_notify;
}

void notified(microkit_channel ch) {
    bool need_notify_light_control = false;

    if (ch == CH_UART_CMD) {
        uint8_t cmd = *(uint8_t *)input_buffer;
        need_notify_light_control = process_uart_command(cmd);
        g_shmem->uart_cmd = LIGHT_UART_CMD_INVALID;
    } else {
        LOG_INFO("Scheduler: Unknown channel received\n");
    }

    if (need_notify_light_control) {
        microkit_notify(CH_LIGHT_CONTROL_ALLOW);
    }
}
