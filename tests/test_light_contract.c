#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "light_contract.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static light_shmem_t valid_shared_state(void) {
    light_shmem_t shmem;

    memset(&shmem, 0, sizeof(shmem));
    shmem.layout_version = LIGHT_SHARED_STATE_LAYOUT_V3;
    shmem.fault_mode = LIGHT_FAULT_MODE_NORMAL;
    shmem.fault_lifecycle = LIGHT_FAULT_LIFECYCLE_STABLE;
    shmem.fault_recovery_ticks = 0U;
    shmem.active_fault_mask = 0U;

    return shmem;
}

static void test_shared_state_contract_accepts_current_layout(void) {
    light_shmem_t shmem = valid_shared_state();
    light_contract_check_t check = light_contract_check_shared_state(&shmem);

    expect_true(check.status == LIGHT_CONTRACT_OK,
                "current shared memory layout should satisfy the contract");
}

static void test_shared_state_contract_rejects_old_layout(void) {
    light_shmem_t shmem = valid_shared_state();
    light_contract_check_t check;

    shmem.layout_version = LIGHT_SHARED_STATE_LAYOUT_V3 - 1U;
    check = light_contract_check_shared_state(&shmem);

    expect_true(check.status == LIGHT_CONTRACT_LAYOUT_MISMATCH,
                "old shared memory layout should be rejected");
    expect_true(check.expected == LIGHT_SHARED_STATE_LAYOUT_V3,
                "layout rejection should report expected version");
}

static void test_transport_contract_checks_version_type_and_len(void) {
    light_transport_message_t message;
    light_contract_check_t check;

    memset(&message, 0, sizeof(message));
    message.version = LIGHT_TRANSPORT_VERSION;
    message.type = LIGHT_TRANSPORT_MSG_LIGHT_CMD;
    message.len = sizeof(message.payload.light_cmd);
    message.payload.light_cmd = LIGHT_CMD_LOW_BEAM_ON;

    check = light_contract_check_transport_message(message, LIGHT_TRANSPORT_MSG_LIGHT_CMD);
    expect_true(check.status == LIGHT_CONTRACT_OK,
                "valid transport message should satisfy contract");

    message.version = LIGHT_TRANSPORT_VERSION + 1U;
    check = light_contract_check_transport_message(message, LIGHT_TRANSPORT_MSG_LIGHT_CMD);
    expect_true(check.status == LIGHT_CONTRACT_TRANSPORT_VERSION,
                "transport version mismatch should be explicit");

    message.version = LIGHT_TRANSPORT_VERSION;
    message.type = LIGHT_TRANSPORT_MSG_FAULT_CLEAR;
    message.len = sizeof(message.payload.fault_clear_scope);
    check = light_contract_check_transport_message(message, LIGHT_TRANSPORT_MSG_LIGHT_CMD);
    expect_true(check.status == LIGHT_CONTRACT_TRANSPORT_TYPE,
                "transport type mismatch should be explicit");

    message.type = LIGHT_TRANSPORT_MSG_LIGHT_CMD;
    message.len = 0U;
    check = light_contract_check_transport_message(message, LIGHT_TRANSPORT_MSG_LIGHT_CMD);
    expect_true(check.status == LIGHT_CONTRACT_TRANSPORT_LENGTH,
                "transport length mismatch should be explicit");
}

static void test_fault_snapshot_contract_bounds_recovery_and_mask(void) {
    light_contract_check_t check;

    check = light_contract_check_fault_snapshot(LIGHT_FAULT_MODE_SAFE_MODE,
                                                LIGHT_FAULT_LIFECYCLE_RECOVERING,
                                                light_fault_recovery_window_ticks(),
                                                0x0fU);
    expect_true(check.status == LIGHT_CONTRACT_OK,
                "boundary recovery state should satisfy contract");

    check = light_contract_check_fault_snapshot(LIGHT_FAULT_MODE_SAFE_MODE,
                                                LIGHT_FAULT_LIFECYCLE_RECOVERING,
                                                light_fault_recovery_window_ticks() + 1U,
                                                0x0fU);
    expect_true(check.status == LIGHT_CONTRACT_FAULT_RECOVERY,
                "recovery ticks beyond window should be rejected");

    check = light_contract_check_fault_snapshot(LIGHT_FAULT_MODE_SAFE_MODE,
                                                LIGHT_FAULT_LIFECYCLE_RECOVERING,
                                                0U,
                                                0x80U);
    expect_true(check.status == LIGHT_CONTRACT_FAULT_MASK,
                "unknown active fault bits should be rejected");
}

static void test_channel_contract_knows_system_channels(void) {
    expect_true(light_contract_channel_is_known(LIGHT_CH_COMMANDIN_TO_SCHEDULER),
                "commandin to scheduler channel should be known");
    expect_true(light_contract_channel_is_known(LIGHT_CH_GPIO_POSITION_OFF),
                "gpio action channel should be known");
    expect_true(!light_contract_channel_is_known(99U),
                "unknown channel should not be accepted by contract table");
}

int main(void) {
    test_shared_state_contract_accepts_current_layout();
    test_shared_state_contract_rejects_old_layout();
    test_transport_contract_checks_version_type_and_len();
    test_fault_snapshot_contract_bounds_recovery_and_mask();
    test_channel_contract_knows_system_channels();

    printf("light_contract tests passed\n");
    return 0;
}
