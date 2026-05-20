#include "light_contract.h"

#define LIGHT_FAULT_ACTIVE_MASK_ALL 0x0fU

static light_contract_check_t contract_result(light_contract_status_t status,
                                              uint32_t expected,
                                              uint32_t actual) {
    light_contract_check_t result;

    result.status = status;
    result.expected = expected;
    result.actual = actual;

    return result;
}

static bool fault_mode_is_valid(uint8_t mode) {
    return mode <= (uint8_t)LIGHT_FAULT_MODE_SAFE_MODE;
}

static bool fault_lifecycle_is_valid(uint8_t lifecycle) {
    return lifecycle <= (uint8_t)LIGHT_FAULT_LIFECYCLE_RECOVERING;
}

static uint8_t expected_transport_len(light_transport_msg_type_t type) {
    switch (type) {
        case LIGHT_TRANSPORT_MSG_LIGHT_CMD:
            return (uint8_t)sizeof(((light_transport_message_t *)0)->payload.light_cmd);
        case LIGHT_TRANSPORT_MSG_VEHICLE_STATE_UPDATE:
            return (uint8_t)sizeof(((light_transport_message_t *)0)->payload.vehicle_state_update);
        case LIGHT_TRANSPORT_MSG_FAULT_INJECT:
            return (uint8_t)sizeof(((light_transport_message_t *)0)->payload.fault_error_code);
        case LIGHT_TRANSPORT_MSG_QUERY:
            return (uint8_t)sizeof(((light_transport_message_t *)0)->payload.query_id);
        case LIGHT_TRANSPORT_MSG_FAULT_CLEAR:
            return (uint8_t)sizeof(((light_transport_message_t *)0)->payload.fault_clear_scope);
        case LIGHT_TRANSPORT_MSG_INVALID:
        default:
            return 0U;
    }
}

light_contract_check_t light_contract_check_shared_state(const volatile light_shmem_t *shmem) {
    if (shmem == 0) {
        return contract_result(LIGHT_CONTRACT_NULL, 0U, 0U);
    }

    if (shmem->layout_version != LIGHT_SHARED_STATE_LAYOUT_V3) {
        return contract_result(LIGHT_CONTRACT_LAYOUT_MISMATCH,
                               LIGHT_SHARED_STATE_LAYOUT_V3,
                               shmem->layout_version);
    }

    return light_contract_check_fault_snapshot(shmem->fault_mode,
                                               shmem->fault_lifecycle,
                                               shmem->fault_recovery_ticks,
                                               shmem->active_fault_mask);
}

light_contract_check_t light_contract_check_transport_message(light_transport_message_t message,
                                                              light_transport_msg_type_t expected_type) {
    uint8_t expected_len;

    if (message.version != LIGHT_TRANSPORT_VERSION) {
        return contract_result(LIGHT_CONTRACT_TRANSPORT_VERSION,
                               LIGHT_TRANSPORT_VERSION,
                               message.version);
    }

    if (message.type != (uint8_t)expected_type) {
        return contract_result(LIGHT_CONTRACT_TRANSPORT_TYPE,
                               (uint8_t)expected_type,
                               message.type);
    }

    expected_len = expected_transport_len(expected_type);
    if (expected_len == 0U || message.len != expected_len) {
        return contract_result(LIGHT_CONTRACT_TRANSPORT_LENGTH,
                               expected_len,
                               message.len);
    }

    return contract_result(LIGHT_CONTRACT_OK, 0U, 0U);
}

light_contract_check_t light_contract_check_fault_snapshot(uint8_t mode,
                                                           uint8_t lifecycle,
                                                           uint8_t recovery_ticks,
                                                           uint8_t active_fault_mask) {
    if (!fault_mode_is_valid(mode)) {
        return contract_result(LIGHT_CONTRACT_FAULT_MODE,
                               (uint8_t)LIGHT_FAULT_MODE_SAFE_MODE,
                               mode);
    }

    if (!fault_lifecycle_is_valid(lifecycle)) {
        return contract_result(LIGHT_CONTRACT_FAULT_LIFECYCLE,
                               (uint8_t)LIGHT_FAULT_LIFECYCLE_RECOVERING,
                               lifecycle);
    }

    if (recovery_ticks > light_fault_recovery_window_ticks()) {
        return contract_result(LIGHT_CONTRACT_FAULT_RECOVERY,
                               light_fault_recovery_window_ticks(),
                               recovery_ticks);
    }

    if ((active_fault_mask & (uint8_t)~LIGHT_FAULT_ACTIVE_MASK_ALL) != 0U) {
        return contract_result(LIGHT_CONTRACT_FAULT_MASK,
                               LIGHT_FAULT_ACTIVE_MASK_ALL,
                               active_fault_mask);
    }

    return contract_result(LIGHT_CONTRACT_OK, 0U, 0U);
}

bool light_contract_channel_is_known(uint32_t channel) {
    switch (channel) {
        case LIGHT_CH_COMMANDIN_UART_IRQ:
        case LIGHT_CH_GPIO_TO_LIGHTCTL:
        case LIGHT_CH_LIGHTCTL_FROM_GPIO:
        case LIGHT_CH_COMMANDIN_TO_SCHEDULER:
        case LIGHT_CH_SCHEDULER_FROM_COMMANDIN:
        case LIGHT_CH_FAULTMG_FROM_LIGHTCTL:
        case LIGHT_CH_LIGHTCTL_TO_FAULTMG:
        case LIGHT_CH_FAULTMG_TO_GPIO:
        case LIGHT_CH_GPIO_FROM_FAULTMG:
        case LIGHT_CH_SCHEDULER_TO_LIGHTCTL:
        case LIGHT_CH_LIGHTCTL_FROM_SCHEDULER:
        case LIGHT_CH_COMMANDIN_TO_FAULTMG:
        case LIGHT_CH_FAULTMG_FROM_COMMANDIN:
        case LIGHT_CH_SCHEDULER_FROM_FAULTMG:
        case LIGHT_CH_FAULTMG_TO_SCHEDULER:
        case LIGHT_CH_SCHEDULER_FROM_VEHICLE_STATE:
        case LIGHT_CH_VEHICLE_STATE_TO_SCHEDULER:
        case LIGHT_CH_COMMANDIN_TO_VEHICLE_STATE:
        case LIGHT_CH_VEHICLE_STATE_FROM_COMMANDIN:
        case LIGHT_CH_GPIO_TURN_LEFT_ON:
        case LIGHT_CH_GPIO_TURN_LEFT_OFF:
        case LIGHT_CH_GPIO_TURN_RIGHT_ON:
        case LIGHT_CH_GPIO_TURN_RIGHT_OFF:
        case LIGHT_CH_GPIO_BRAKE_ON:
        case LIGHT_CH_GPIO_BRAKE_OFF:
        case LIGHT_CH_GPIO_LOW_BEAM_ON:
        case LIGHT_CH_GPIO_LOW_BEAM_OFF:
        case LIGHT_CH_GPIO_HIGH_BEAM_ON:
        case LIGHT_CH_GPIO_HIGH_BEAM_OFF:
        case LIGHT_CH_GPIO_POSITION_ON:
        case LIGHT_CH_GPIO_POSITION_OFF:
            return true;
        default:
            return false;
    }
}

const char *light_contract_status_name(light_contract_status_t status) {
    switch (status) {
        case LIGHT_CONTRACT_OK:
            return "OK";
        case LIGHT_CONTRACT_NULL:
            return "NULL";
        case LIGHT_CONTRACT_LAYOUT_MISMATCH:
            return "LAYOUT_MISMATCH";
        case LIGHT_CONTRACT_TRANSPORT_VERSION:
            return "TRANSPORT_VERSION";
        case LIGHT_CONTRACT_TRANSPORT_TYPE:
            return "TRANSPORT_TYPE";
        case LIGHT_CONTRACT_TRANSPORT_LENGTH:
            return "TRANSPORT_LENGTH";
        case LIGHT_CONTRACT_FAULT_MODE:
            return "FAULT_MODE";
        case LIGHT_CONTRACT_FAULT_LIFECYCLE:
            return "FAULT_LIFECYCLE";
        case LIGHT_CONTRACT_FAULT_RECOVERY:
            return "FAULT_RECOVERY";
        case LIGHT_CONTRACT_FAULT_MASK:
            return "FAULT_MASK";
        default:
            return "UNKNOWN";
    }
}
