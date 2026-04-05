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

static bool check_speed_limit(microkit_channel ch) {
    if ((ch == LIGHT_CH_GPIO_TURN_LEFT_ON || ch == LIGHT_CH_GPIO_TURN_RIGHT_ON)
        && g_shmem->vehicle_speed > 120) {
        LOG_INFO("LightCtrl: Speed limit exceeded (>120km/h), turn light denied\n");
        microkit_mr_set(0, LIGHT_ERR_SPEED_LIMIT);
        microkit_notify(CH_ERROR_REPORT);
        return false;
    }
    if (ch == LIGHT_CH_GPIO_HIGH_BEAM_ON && g_shmem->vehicle_speed < 10) {
        LOG_INFO("LightCtrl: Speed too low (<10km/h), high beam denied\n");
        microkit_mr_set(0, LIGHT_ERR_SPEED_LIMIT);
        microkit_notify(CH_ERROR_REPORT);
        return false;
    }
    return true;
}

static bool check_mode_conflict(microkit_channel ch) {
    if (ch == LIGHT_CH_GPIO_LOW_BEAM_OFF && g_last_beam_state == 2) {
        LOG_INFO("LightCtrl: Mode conflict, can't turn off low beam when high beam is on\n");
        microkit_mr_set(0, LIGHT_ERR_MODE_CONFLICT);
        microkit_notify(CH_ERROR_REPORT);
        return false;
    }
    if ((ch == LIGHT_CH_GPIO_TURN_LEFT_ON || ch == LIGHT_CH_GPIO_TURN_RIGHT_ON)
        && g_last_brake_state == 1) {
        LOG_INFO("LightCtrl: Mode conflict, brake light active, turn light denied\n");
        microkit_mr_set(0, LIGHT_ERR_MODE_CONFLICT);
        microkit_notify(CH_ERROR_REPORT);
        return false;
    }
    return true;
}

static void trigger_gpio_operation(microkit_channel ch) {
    microkit_notify(ch);
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

    LOG_INFO("Light control module initialized\n");
}

void notified(microkit_channel ch) {
    if (ch != CH_SCHEDULER_ALLOW) {
        LOG_INFO("LightCtrl: Unknown channel, ignore\n");
        microkit_mr_set(0, LIGHT_ERR_INVALID_CMD);
        microkit_notify(CH_ERROR_REPORT);
        return;
    }

    LOG_INFO("--- LightCtrl State Check ---");
    LOG_INFO("Shmem: brake=%d, left=%d, right=%d, low=%d, high=%d, pos=%d",
             LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_BRAKE),
             LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_TURN_LEFT),
             LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_TURN_RIGHT),
             LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_LOW_BEAM),
             LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_HIGH_BEAM),
             LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_POSITION));
    LOG_INFO("Internal: last_brake=%d, last_turn=%d, last_beam=%d",
             g_last_brake_state,
             g_last_turn_state,
             g_last_beam_state);

    LOG_INFO("GET SCHEDULER SIGANL");

    if (LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_BRAKE)) {
        if (g_last_brake_state != 1) {
            if (check_mode_conflict(LIGHT_CH_GPIO_BRAKE_ON)) {
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

    if (LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_TURN_LEFT)) {
        if (g_last_turn_state != 1) {
            if (check_speed_limit(LIGHT_CH_GPIO_TURN_LEFT_ON)
                && check_mode_conflict(LIGHT_CH_GPIO_TURN_LEFT_ON)) {
                trigger_gpio_operation(LIGHT_CH_GPIO_TURN_LEFT_ON);
                g_last_turn_state = 1;
            }
        }
    } else if (LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_TURN_RIGHT)) {
        if (g_last_turn_state != 2) {
            if (check_speed_limit(LIGHT_CH_GPIO_TURN_RIGHT_ON)
                && check_mode_conflict(LIGHT_CH_GPIO_TURN_RIGHT_ON)) {
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

    if (LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_LOW_BEAM)) {
        if (g_last_beam_state != 1) {
            if (check_mode_conflict(LIGHT_CH_GPIO_LOW_BEAM_ON)) {
                trigger_gpio_operation(LIGHT_CH_GPIO_LOW_BEAM_ON);
                g_last_beam_state = 1;
            }
        }
    } else if (LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_HIGH_BEAM)) {
        if (g_last_beam_state != 2) {
            if (check_speed_limit(LIGHT_CH_GPIO_HIGH_BEAM_ON)
                && check_mode_conflict(LIGHT_CH_GPIO_HIGH_BEAM_ON)) {
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

    if (LIGHT_FLAG_IS_SET(g_shmem->allow_flags, LIGHT_ALLOW_POSITION)) {
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
}
