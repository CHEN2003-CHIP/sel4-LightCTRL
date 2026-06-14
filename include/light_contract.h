#ifndef LIGHT_CONTRACT_H
#define LIGHT_CONTRACT_H

#include <stdbool.h>
#include <stdint.h>

#include "light_fault_mode.h"
#include "light_protocol.h"
#include "light_transport.h"

typedef enum {
    LIGHT_CONTRACT_OK = 0,
    LIGHT_CONTRACT_NULL = 1,
    LIGHT_CONTRACT_LAYOUT_MISMATCH = 2,
    LIGHT_CONTRACT_TRANSPORT_VERSION = 3,
    LIGHT_CONTRACT_TRANSPORT_TYPE = 4,
    LIGHT_CONTRACT_TRANSPORT_LENGTH = 5,
    LIGHT_CONTRACT_FAULT_MODE = 6,
    LIGHT_CONTRACT_FAULT_LIFECYCLE = 7,
    LIGHT_CONTRACT_FAULT_RECOVERY = 8,
    LIGHT_CONTRACT_FAULT_MASK = 9,
} light_contract_status_t;

typedef struct {
    light_contract_status_t status;
    uint32_t expected;
    uint32_t actual;
} light_contract_check_t;

light_contract_check_t light_contract_check_shared_state(const volatile light_shmem_t *shmem);
light_contract_check_t light_contract_check_transport_message(light_transport_message_t message,
                                                              light_transport_msg_type_t expected_type);
light_contract_check_t light_contract_check_fault_snapshot(uint8_t mode,
                                                           uint8_t lifecycle,
                                                           uint8_t recovery_ticks,
                                                           uint32_t recovery_elapsed_ms,
                                                           uint8_t active_fault_mask);
bool light_contract_channel_is_known(uint32_t channel);
const char *light_contract_status_name(light_contract_status_t status);

#endif
