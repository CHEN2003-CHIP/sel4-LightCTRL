#ifndef LIGHT_STATUS_SNAPSHOT_H
#define LIGHT_STATUS_SNAPSHOT_H

#include <stddef.h>

#include "light_protocol.h"

typedef struct {
    uint8_t fault_mode;
    uint8_t lifecycle;
    uint8_t recovery_ticks;
    uint8_t active_fault_mask;
    light_vehicle_state_t vehicle_state;
    light_target_output_t target_output;
    uint32_t allow_flags;
    uint8_t last_fault_code;
    uint32_t total_fault_count;
} light_status_snapshot_t;

light_status_snapshot_t light_status_snapshot_capture(const volatile light_shmem_t *shmem);
int light_status_snapshot_format(char *buf, size_t buf_size, light_status_snapshot_t snapshot);

#endif
