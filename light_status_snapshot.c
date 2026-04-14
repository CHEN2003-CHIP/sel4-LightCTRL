#include "light_status_snapshot.h"

#include <stdio.h>

#include "light_fault_mode.h"

light_status_snapshot_t light_status_snapshot_capture(const volatile light_shmem_t *shmem) {
    light_status_snapshot_t snapshot;

    snapshot.fault_mode = shmem->fault_mode;
    snapshot.vehicle_state = (light_vehicle_state_t)shmem->vehicle_state;
    snapshot.target_output = (light_target_output_t)shmem->target_output;
    snapshot.allow_flags = shmem->allow_flags;
    snapshot.last_fault_code = shmem->last_fault_code;
    snapshot.total_fault_count = shmem->total_fault_count;

    return snapshot;
}

int light_status_snapshot_format(char *buf, size_t buf_size, light_status_snapshot_t snapshot) {
    return snprintf(buf,
                    buf_size,
                    "STATUS_SNAPSHOT fault=%s speed=%u ignition=%u brake_pedal=%u target[low=%u high=%u left=%u right=%u marker=%u brake=%u] allow=0x%02x last_fault=0x%02x total_faults=%u",
                    light_fault_mode_name((fault_mode_t)snapshot.fault_mode),
                    (unsigned int)snapshot.vehicle_state.speed_kph,
                    (unsigned int)snapshot.vehicle_state.ignition_on,
                    (unsigned int)snapshot.vehicle_state.brake_pedal,
                    (unsigned int)snapshot.target_output.low_beam_on,
                    (unsigned int)snapshot.target_output.high_beam_on,
                    (unsigned int)snapshot.target_output.left_turn_on,
                    (unsigned int)snapshot.target_output.right_turn_on,
                    (unsigned int)snapshot.target_output.marker_on,
                    (unsigned int)snapshot.target_output.brake_on,
                    (unsigned int)snapshot.allow_flags,
                    (unsigned int)snapshot.last_fault_code,
                    (unsigned int)snapshot.total_fault_count);
}
