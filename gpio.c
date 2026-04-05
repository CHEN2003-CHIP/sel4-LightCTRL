/**
 * @file gpio_ctrl.c
 * @brief GPIO control component
 */

#include <stdint.h>
#include <microkit.h>
#include "printf.h"
#include <stdatomic.h>
#include "logger.h"
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <sel4/sel4.h>
#include "light_protocol.h"

uintptr_t gpio_base_vaddr;
uintptr_t cmd_buffer;
uintptr_t timer_base_vaddr;

#define GPIO_CHANNEL 1
#define FAULT_NOTIFY_CHANNEL 8

#define REG_PTR(base, offset) ((volatile uint32_t *)((base) + (offset)))

#define TIMER_LOAD_OFFSET 0x00
#define TIMER_CTRL_OFFSET 0x08

static bool left_turn_active = false;
static bool right_turn_active = false;
static uint8_t left_turn_state = 0;
static uint8_t right_turn_state = 0;

#define PIN_LOW_BEAM   0
#define PIN_HIGH_BEAM  1
#define PIN_TURN_LEFT  2
#define PIN_TURN_RIGHT 3
#define PIN_BRAKE      4
#define PIN_POSITION   5

#define GPIO_DIR_OFFSET 0x00
#define GPIO_OUT_OFFSET 0x04

static void timer_init() {
    *REG_PTR(timer_base_vaddr, TIMER_LOAD_OFFSET) = 5000000;
    *REG_PTR(timer_base_vaddr, TIMER_CTRL_OFFSET) = (1 << 0) | (1 << 1) | (1 << 7);
}

static inline uint64_t read_cntpct(void) {
    uint64_t val;
    asm volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

static inline uint64_t read_cntfrq(void) {
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

uint64_t get_system_ticks(void) {
    return read_cntpct();
}

uint64_t get_timer_freq(void) {
    return read_cntfrq();
}

uint64_t ticks_to_ms(uint64_t ticks) {
    uint64_t freq = get_timer_freq();
    return (ticks * 1000ULL) / freq;
}

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

static uint8_t gpio_action_pin(microkit_channel ch) {
    switch (ch) {
        case LIGHT_CH_GPIO_TURN_LEFT_ON:
        case LIGHT_CH_GPIO_TURN_LEFT_OFF:
            return PIN_TURN_LEFT;
        case LIGHT_CH_GPIO_TURN_RIGHT_ON:
        case LIGHT_CH_GPIO_TURN_RIGHT_OFF:
            return PIN_TURN_RIGHT;
        case LIGHT_CH_GPIO_BRAKE_ON:
        case LIGHT_CH_GPIO_BRAKE_OFF:
            return PIN_BRAKE;
        case LIGHT_CH_GPIO_LOW_BEAM_ON:
        case LIGHT_CH_GPIO_LOW_BEAM_OFF:
            return PIN_LOW_BEAM;
        case LIGHT_CH_GPIO_HIGH_BEAM_ON:
        case LIGHT_CH_GPIO_HIGH_BEAM_OFF:
            return PIN_HIGH_BEAM;
        case LIGHT_CH_GPIO_POSITION_ON:
        case LIGHT_CH_GPIO_POSITION_OFF:
            return PIN_POSITION;
        default:
            return 0xff;
    }
}

void init(void) {
    volatile uint32_t *gpio_dir = REG_PTR(gpio_base_vaddr, GPIO_DIR_OFFSET);
    *gpio_dir |= (1 << PIN_LOW_BEAM) |
                 (1 << PIN_HIGH_BEAM) |
                 (1 << PIN_TURN_LEFT) |
                 (1 << PIN_TURN_RIGHT) |
                 (1 << PIN_BRAKE) |
                 (1 << PIN_POSITION);

    LOG_INFO("GPIO_INIT module=gpio status=ready low=%d high=%d left=%d right=%d brake=%d position=%d",
             PIN_LOW_BEAM, PIN_HIGH_BEAM, PIN_TURN_LEFT, PIN_TURN_RIGHT, PIN_BRAKE, PIN_POSITION);
    LOG_INFO("GPIO Ctrl: Initialized. All pins set to output.");
    LOG_INFO("GPIO pins configured: low=%d high=%d left=%d right=%d",
             PIN_LOW_BEAM, PIN_HIGH_BEAM, PIN_TURN_LEFT, PIN_TURN_RIGHT);
    LOG_INFO("GPIO: starting\n");

    timer_init();
    LOG_INFO("GPIO PD initialized, timer started");
}

static void gpio_set_pin(uint8_t pin, bool level) {
    volatile uint32_t *gpio_out = REG_PTR(gpio_base_vaddr, GPIO_OUT_OFFSET);

    if (level) {
        *gpio_out |= (1 << pin);
    } else {
        *gpio_out &= ~(1 << pin);
    }
}

void notified(microkit_channel ch) {
    bool level = false;

    switch (ch) {
        case LIGHT_CH_GPIO_TURN_LEFT_ON:
            gpio_set_pin(PIN_TURN_LEFT, true);
            level = true;
            LOG_INFO("GPIO: Left Turn ON");
            break;
        case LIGHT_CH_GPIO_TURN_LEFT_OFF:
            gpio_set_pin(PIN_TURN_LEFT, false);
            LOG_INFO("GPIO: Left Turn OFF");
            break;
        case LIGHT_CH_GPIO_TURN_RIGHT_ON:
            gpio_set_pin(PIN_TURN_RIGHT, true);
            level = true;
            LOG_INFO("GPIO: Right Turn ON");
            break;
        case LIGHT_CH_GPIO_TURN_RIGHT_OFF:
            gpio_set_pin(PIN_TURN_RIGHT, false);
            LOG_INFO("GPIO: Right Turn OFF");
            break;

        case LIGHT_CH_GPIO_BRAKE_ON:
            gpio_set_pin(PIN_BRAKE, true);
            level = true;
            LOG_INFO("GPIO: Brake ON");
            break;
        case LIGHT_CH_GPIO_BRAKE_OFF:
            gpio_set_pin(PIN_BRAKE, false);
            LOG_INFO("GPIO: Brake OFF");
            break;

        case LIGHT_CH_GPIO_LOW_BEAM_ON:
            gpio_set_pin(PIN_LOW_BEAM, true);
            level = true;
            LOG_INFO("GPIO: Low Beam ON");
            break;
        case LIGHT_CH_GPIO_LOW_BEAM_OFF:
            gpio_set_pin(PIN_LOW_BEAM, false);
            LOG_INFO("GPIO: Low Beam OFF");
            break;

        case LIGHT_CH_GPIO_HIGH_BEAM_ON:
            gpio_set_pin(PIN_HIGH_BEAM, true);
            level = true;
            LOG_INFO("GPIO: High Beam ON");
            break;
        case LIGHT_CH_GPIO_HIGH_BEAM_OFF:
            gpio_set_pin(PIN_HIGH_BEAM, false);
            LOG_INFO("GPIO: High Beam OFF");
            break;

        case LIGHT_CH_GPIO_POSITION_ON:
            gpio_set_pin(PIN_POSITION, true);
            level = true;
            LOG_INFO("GPIO: Position Light ON");
            break;
        case LIGHT_CH_GPIO_POSITION_OFF:
            gpio_set_pin(PIN_POSITION, false);
            LOG_INFO("GPIO: Position Light OFF");
            break;

        default:
            LOG_ERROR("GPIO: Unknown channel %d", ch);
            break;
    }

    if (ch >= LIGHT_CH_GPIO_TURN_LEFT_ON && ch <= LIGHT_CH_GPIO_POSITION_OFF) {
        LOG_INFO("GPIO_APPLY action=%s pin=%d level=%d",
                 gpio_action_name(ch),
                 gpio_action_pin(ch),
                 level ? 1 : 0);
    }
}
