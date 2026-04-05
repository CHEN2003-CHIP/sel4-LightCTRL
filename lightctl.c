/**
 * @file lightctl.c
 * @brief 杞︾伅鎺у埗绯荤粺-鏍稿績鎺у埗缁勪欢
 */

#include <stdint.h>
#include <stdbool.h>
#include <microkit.h>
#include "printf.h"
#include "wordle.h"
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include "logger.h"
#include "light_policy.h"
#include "light_runtime_guard.h"
#include "light_protocol.h"

// 閫氶亾ID锛氳皟搴﹀櫒鈫掔伅鍏夋帶鍒讹紙鍏佽鎵ц閫氱煡锛?
#define CH_SCHEDULER_ALLOW 10
// 閫氶亾ID锛氱伅鍏夋帶鍒垛啋閿欒澶勭悊妯″潡
#define CH_ERROR_REPORT 6

uintptr_t cmd_buffer;
uintptr_t input_buffer;
uintptr_t shared_memory_base_vaddr;

static light_shmem_t *g_shmem = NULL;

static uint8_t g_last_turn_state = 0;
static uint8_t g_last_beam_state = 0;
static uint8_t g_last_brake_state = 0;
static uint8_t g_last_position_state = 1;

static const char *gpio_action_name(microkit_channel ch) {
    switch (ch) {
        case LIGHT_CH_GPIO_TURN_LEFT_ON:
            return "turn_left_on";
        case LIGHT_CH_GPIO_TURN_LEFT_OFF:
            return "turn_left_off";
        case LIGHT_CH_GPIO_TURN_RIGHT_ON:
            return "turn_right_on";
        case LIGHT_CH_GPIO_TURN_RIGHT_OFF:
            return "turn_right_off";
        case LIGHT_CH_GPIO_BRAKE_ON:
            return "brake_on";
        case LIGHT_CH_GPIO_BRAKE_OFF:
            return "brake_off";
        case LIGHT_CH_GPIO_LOW_BEAM_ON:
            return "low_beam_on";
        case LIGHT_CH_GPIO_LOW_BEAM_OFF:
            return "low_beam_off";
        case LIGHT_CH_GPIO_HIGH_BEAM_ON:
            return "high_beam_on";
        case LIGHT_CH_GPIO_HIGH_BEAM_OFF:
            return "high_beam_off";
        case LIGHT_CH_GPIO_POSITION_ON:
            return "position_on";
        case LIGHT_CH_GPIO_POSITION_OFF:
            return "position_off";
        default:
            return "unknown";
    }
}

static light_runtime_guard_context_t runtime_guard_context(void) {
    light_runtime_guard_context_t context;

    context.vehicle_speed = g_shmem->vehicle_speed;
    context.last_turn_state = g_last_turn_state;
    context.last_beam_state = g_last_beam_state;
    context.last_brake_state = g_last_brake_state;
    context.last_position_state = g_last_position_state;

    return context;
}

static void log_guard_rejection(microkit_channel ch, uint8_t error_code) {
    if (error_code == LIGHT_ERR_SPEED_LIMIT) {
        if (ch == LIGHT_CH_GPIO_HIGH_BEAM_ON) {
            LOG_INFO("LightCtrl: Speed too low (<10km/h), high beam denied\n");
        } else {
            LOG_INFO("LightCtrl: Speed limit exceeded (>120km/h), turn light denied\n");
        }
        return;
    }

    if (error_code == LIGHT_ERR_MODE_CONFLICT) {
        if (ch == LIGHT_CH_GPIO_LOW_BEAM_OFF) {
            LOG_INFO("LightCtrl: Mode conflict, can't turn off low beam when high beam is on\n");
        } else {
            LOG_INFO("LightCtrl: Mode conflict, brake light active, turn light denied\n");
        }
    }
}

static bool guard_allows_action(microkit_channel ch) {
    light_runtime_guard_result_t result = light_runtime_guard_check_action(ch, runtime_guard_context());

    if (!result.allowed) {
        log_guard_rejection(ch, result.error_code);
        if (result.report_fault) {
            microkit_mr_set(0, result.error_code);
            microkit_notify(CH_ERROR_REPORT);
        }
    }

    return result.allowed;
}

static void trigger_gpio_operation(microkit_channel ch) {
    microkit_notify(ch);
    LOG_INFO("LIGHTCTL_GPIO action=%s channel=%d", gpio_action_name(ch), ch);
    LOG_INFO("LightCtrl: Trigger GPIO on channel %d\n", ch);
}

void init(void) {
    g_shmem = (light_shmem_t *)shared_memory_base_vaddr;
    if (g_shmem == NULL) {
        microkit_dbg_puts("LightCtrl ERROR: Shared memory not mapped!\n");
        while (1);
    }

    g_last_turn_state = 0;
    g_last_beam_state = 0;
    g_last_brake_state = 0;
    g_last_position_state = 1;

    LOG_INFO("LIGHTCTL_INIT module=lightctl status=ready");
    LOG_INFO("Light control module initialized\n");
}

void notified(microkit_channel ch) {
    light_target_state_t target;

    if (ch != CH_SCHEDULER_ALLOW) {
        LOG_INFO("LightCtrl: Unknown channel, ignore\n");
        microkit_mr_set(0, LIGHT_ERR_INVALID_CMD);
        microkit_notify(CH_ERROR_REPORT);
        return;
    }

    target = light_policy_target_from_flags(g_shmem->allow_flags);

    LOG_INFO("--- LightCtrl State Check ---");
    LOG_INFO("LIGHTCTL_SYNC allow_flags=0x%02x brake=%d left=%d right=%d low=%d high=%d pos=%d",
             (unsigned int)g_shmem->allow_flags,
             target.brake,
             target.turn_left,
             target.turn_right,
             target.low_beam,
             target.high_beam,
             target.position);
    LOG_INFO("Shmem: brake=%d, left=%d, right=%d, low=%d, high=%d, pos=%d",
             target.brake,
             target.turn_left,
             target.turn_right,
             target.low_beam,
             target.high_beam,
             target.position);
    LOG_INFO("Internal: last_brake=%d, last_turn=%d, last_beam=%d",
             g_last_brake_state,
             g_last_turn_state,
             g_last_beam_state);

    LOG_INFO("GET SCHEDULER SIGANL");

    if (target.brake) {
        if (g_last_brake_state != 1) {
            if (guard_allows_action(LIGHT_CH_GPIO_BRAKE_ON)) {
                trigger_gpio_operation(LIGHT_CH_GPIO_BRAKE_ON);
                g_last_brake_state = 1;
            }
        }
    } else {
        if (g_last_brake_state != 0) {
            trigger_gpio_operation(LIGHT_CH_GPIO_BRAKE_OFF);
            g_last_brake_state = 0;
        }
    }

    if (target.turn_left) {
        if (g_last_turn_state != 1) {
            if (guard_allows_action(LIGHT_CH_GPIO_TURN_LEFT_ON)) {
                trigger_gpio_operation(LIGHT_CH_GPIO_TURN_LEFT_ON);
                g_last_turn_state = 1;
            }
        }
    } else if (target.turn_right) {
        if (g_last_turn_state != 2) {
            if (guard_allows_action(LIGHT_CH_GPIO_TURN_RIGHT_ON)) {
                trigger_gpio_operation(LIGHT_CH_GPIO_TURN_RIGHT_ON);
                g_last_turn_state = 2;
            }
        }
    } else {
        if (g_last_turn_state != 0) {
            microkit_channel off_ch = (g_last_turn_state == 1)
                ? LIGHT_CH_GPIO_TURN_LEFT_OFF
                : LIGHT_CH_GPIO_TURN_RIGHT_OFF;
            trigger_gpio_operation(off_ch);
            g_last_turn_state = 0;
        }
    }

    if (target.low_beam) {
        if (g_last_beam_state != 1) {
            if (guard_allows_action(LIGHT_CH_GPIO_LOW_BEAM_ON)) {
                trigger_gpio_operation(LIGHT_CH_GPIO_LOW_BEAM_ON);
                g_last_beam_state = 1;
            }
        }
    } else if (target.high_beam) {
        if (g_last_beam_state != 2) {
            if (guard_allows_action(LIGHT_CH_GPIO_HIGH_BEAM_ON)) {
                trigger_gpio_operation(LIGHT_CH_GPIO_HIGH_BEAM_ON);
                g_last_beam_state = 2;
            }
        }
    } else {
        if (g_last_beam_state != 0) {
            if (g_last_beam_state == 2) {
                trigger_gpio_operation(LIGHT_CH_GPIO_HIGH_BEAM_OFF);
            }
            trigger_gpio_operation(LIGHT_CH_GPIO_LOW_BEAM_OFF);
            g_last_beam_state = 0;
        }
    }

    if (target.position) {
        if (g_last_position_state != 1) {
            trigger_gpio_operation(LIGHT_CH_GPIO_POSITION_ON);
            g_last_position_state = 1;
        }
    } else {
        if (g_last_position_state != 0) {
            trigger_gpio_operation(LIGHT_CH_GPIO_POSITION_OFF);
            g_last_position_state = 0;
        }
    }

    LOG_INFO("LIGHTCTL_STATE brake=%d turn=%d beam=%d position=%d",
             g_last_brake_state,
             g_last_turn_state,
             g_last_beam_state,
             g_last_position_state);
}
