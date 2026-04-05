/**
 * @file fault_mgmt.c
 * @brief Fault management component
 */

#include <stdint.h>
#include <stdbool.h>
#include <microkit.h>
#include <stddef.h>
#include "printf.h"
#include "logger.h"
#include "light_protocol.h"

#define FAULTMG_LIGHTCTL 5

uint32_t total_error_count = 0;

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
    LOG_INFO("FAULT_MGMT: initialized\n");
}

void notified(microkit_channel channel) {
    if (channel == FAULTMG_LIGHTCTL) {
        uint8_t error_code = (uint8_t) microkit_mr_get(0);

        total_error_count++;

        LOG_INFO("FAULT_MGMT: fault notification received (total: %d)", total_error_count);

        print_error_details(error_code);
    } else {
        LOG_ERROR("FAULTMG: unknown channel (channel: %d)\n", channel);
    }
}
