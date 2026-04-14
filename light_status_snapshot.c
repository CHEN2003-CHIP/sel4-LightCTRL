#include "light_status_snapshot.h"

#include <stdio.h>

#include "light_fault_mode.h"

light_status_snapshot_t light_status_snapshot_capture(const volatile light_shmem_t *shmem) {
    light_status_snapshot_t snapshot;

    snapshot.fault_mode = shmem->fault_mode;
    snapshot.lifecycle = shmem->fault_lifecycle;
    snapshot.recovery_ticks = shmem->fault_recovery_ticks;
    snapshot.active_fault_mask = shmem->active_fault_mask;
    snapshot.vehicle_state.speed_kph = shmem->vehicle_state.speed_kph;
    snapshot.vehicle_state.ignition_on = shmem->vehicle_state.ignition_on;
    snapshot.vehicle_state.brake_pedal = shmem->vehicle_state.brake_pedal;
    snapshot.target_output.low_beam_on = shmem->target_output.low_beam_on;
    snapshot.target_output.high_beam_on = shmem->target_output.high_beam_on;
    snapshot.target_output.left_turn_on = shmem->target_output.left_turn_on;
    snapshot.target_output.right_turn_on = shmem->target_output.right_turn_on;
    snapshot.target_output.marker_on = shmem->target_output.marker_on;
    snapshot.target_output.brake_on = shmem->target_output.brake_on;
    snapshot.allow_flags = shmem->allow_flags;
    snapshot.last_fault_code = shmem->last_fault_code;
    snapshot.total_fault_count = shmem->total_fault_count;

    return snapshot;
}

int light_status_snapshot_format(char *buf, size_t buf_size, light_status_snapshot_t snapshot) {
    return snprintf(buf,
                    buf_size,
                    "STATUS_SNAPSHOT fault=%s lifecycle=%s recovery_ticks=%u/%u active_faults=0x%02x speed=%u ignition=%u brake_pedal=%u target[low=%u high=%u left=%u right=%u marker=%u brake=%u] allow=0x%02x last_fault=0x%02x total_faults=%u",
                    light_fault_mode_name((fault_mode_t)snapshot.fault_mode),
                    light_fault_lifecycle_name((light_fault_lifecycle_t)snapshot.lifecycle),
                    (unsigned int)snapshot.recovery_ticks,
                    (unsigned int)light_fault_recovery_window_ticks(),
                    (unsigned int)snapshot.active_fault_mask,
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
