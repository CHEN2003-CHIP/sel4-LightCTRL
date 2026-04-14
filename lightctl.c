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
#include "light_fault_mode.h"
#include "light_execution_plan.h"
#include "light_runtime_guard.h"
#include "light_protocol.h"

// 閫氶亾ID锛氳皟搴﹀櫒鈫掔伅鍏夋帶鍒讹紙鍏佽鎵ц閫氱煡锛?
#define CH_SCHEDULER_ALLOW 10
// 閫氶亾ID锛氱伅鍏夋帶鍒垛啋閿欒澶勭悊妯″潡
#define CH_FAULT_LINK 6

uintptr_t cmd_buffer;
uintptr_t input_buffer;
uintptr_t shared_memory_base_vaddr;

static light_shmem_t *g_shmem = NULL;

static light_execution_state_t g_execution_state;
static fault_mode_t g_fault_mode_cache = LIGHT_FAULT_MODE_NORMAL;

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
    context.last_turn_state = g_execution_state.turn_state;
    context.last_beam_state = g_execution_state.beam_state;
    context.last_brake_state = g_execution_state.brake_state;
    context.last_position_state = g_execution_state.position_state;
    context.fault_mode = g_fault_mode_cache;

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
        return;
    }

    if (error_code == LIGHT_ERR_HW_STATE_ERR) {
        LOG_INFO("LightCtrl: Fault mode restriction active, high-risk action denied\n");
    }
}

static bool guard_allows_action(microkit_channel ch) {
    light_runtime_guard_result_t result = light_runtime_guard_check_action(ch, runtime_guard_context());

    if (!result.allowed) {
        log_guard_rejection(ch, result.error_code);
        if (result.report_fault) {
            microkit_mr_set(0, result.error_code);
            microkit_notify(CH_FAULT_LINK);
        }
    }

    return result.allowed;
}

static void log_target_summary(light_target_output_t target_output) {
    LOG_INFO("LIGHTCTL_TARGET_SUMMARY mode=%s target=[brake=%u,left=%u,right=%u,low=%u,high=%u,marker=%u]",
             light_fault_mode_name(g_fault_mode_cache),
             (unsigned int)target_output.brake_on,
             (unsigned int)target_output.left_turn_on,
             (unsigned int)target_output.right_turn_on,
             (unsigned int)target_output.low_beam_on,
             (unsigned int)target_output.high_beam_on,
             (unsigned int)target_output.marker_on);
}

static void trigger_gpio_operation(microkit_channel ch) {
    microkit_notify(ch);
    LOG_INFO("LIGHTCTL_GPIO action=%s channel=%d", gpio_action_name(ch), ch);
    LOG_INFO("LightCtrl: Trigger GPIO on channel %d\n", ch);
}

static void apply_execution_state_transition(microkit_channel action) {
    switch (action) {
        case LIGHT_CH_GPIO_TURN_LEFT_ON:
            g_execution_state.turn_state = 1;
            break;
        case LIGHT_CH_GPIO_TURN_LEFT_OFF:
            if (g_execution_state.turn_state == 1U) {
                g_execution_state.turn_state = 0;
            }
            break;
        case LIGHT_CH_GPIO_TURN_RIGHT_ON:
            g_execution_state.turn_state = 2;
            break;
        case LIGHT_CH_GPIO_TURN_RIGHT_OFF:
            if (g_execution_state.turn_state == 2U) {
                g_execution_state.turn_state = 0;
            }
            break;
        case LIGHT_CH_GPIO_BRAKE_ON:
            g_execution_state.brake_state = 1;
            break;
        case LIGHT_CH_GPIO_BRAKE_OFF:
            g_execution_state.brake_state = 0;
            break;
        case LIGHT_CH_GPIO_LOW_BEAM_ON:
            g_execution_state.beam_state = 1;
            break;
        case LIGHT_CH_GPIO_LOW_BEAM_OFF:
            if (g_execution_state.beam_state == 1U) {
                g_execution_state.beam_state = 0;
            }
            break;
        case LIGHT_CH_GPIO_HIGH_BEAM_ON:
            g_execution_state.beam_state = 2;
            break;
        case LIGHT_CH_GPIO_HIGH_BEAM_OFF:
            if (g_execution_state.beam_state == 2U) {
                g_execution_state.beam_state = 0;
            }
            break;
        case LIGHT_CH_GPIO_POSITION_ON:
            g_execution_state.position_state = 1;
            break;
        case LIGHT_CH_GPIO_POSITION_OFF:
            g_execution_state.position_state = 0;
            break;
        default:
            break;
    }
}

static void sync_outputs(void) {
    size_t i;
    light_target_output_t target = (light_target_output_t)g_shmem->target_output;
    light_execution_plan_t plan;

    g_fault_mode_cache = (fault_mode_t)g_shmem->fault_mode;
    plan = light_execution_plan_build(g_execution_state, target);

    LOG_INFO("--- LightCtrl State Check ---");
    LOG_INFO("LIGHTCTL_SYNC allow_flags=0x%02x brake=%u left=%u right=%u low=%u high=%u marker=%u actions=%u",
             (unsigned int)g_shmem->allow_flags,
             (unsigned int)target.brake_on,
             (unsigned int)target.left_turn_on,
             (unsigned int)target.right_turn_on,
             (unsigned int)target.low_beam_on,
             (unsigned int)target.high_beam_on,
             (unsigned int)target.marker_on,
             (unsigned int)plan.action_count);
    log_target_summary(target);

    for (i = 0; i < plan.action_count; i++) {
        microkit_channel action = (microkit_channel)plan.actions[i];

        if (guard_allows_action(action)) {
            trigger_gpio_operation(action);
            apply_execution_state_transition(action);
        }
    }

    LOG_INFO("LIGHTCTL_STATE brake=%d turn=%d beam=%d position=%d",
             g_execution_state.brake_state,
             g_execution_state.turn_state,
             g_execution_state.beam_state,
             g_execution_state.position_state);
}

void init(void) {
    g_shmem = (light_shmem_t *)shared_memory_base_vaddr;
    if (g_shmem == NULL) {
        microkit_dbg_puts("LightCtrl ERROR: Shared memory not mapped!\n");
        while (1);
    }

    g_execution_state = light_execution_state_init();
    g_fault_mode_cache = LIGHT_FAULT_MODE_NORMAL;

    LOG_INFO("LIGHTCTL_INIT module=lightctl status=ready");
    LOG_INFO("Light control module initialized\n");
}

void notified(microkit_channel ch) {
    if (ch != CH_SCHEDULER_ALLOW) {
        LOG_INFO("LightCtrl: Unknown channel, ignore\n");
        microkit_mr_set(0, LIGHT_ERR_INVALID_CMD);
        microkit_notify(CH_FAULT_LINK);
        return;
    }

    sync_outputs();
}
