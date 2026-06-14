#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "light_fault_mode.h"
#include "light_status_snapshot.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static void test_snapshot_capture_reads_shared_state_consistently(void) {
    light_shmem_t shmem;
    light_status_snapshot_t snapshot;

    memset(&shmem, 0, sizeof(shmem));
    shmem.layout_version = LIGHT_SHARED_STATE_LAYOUT_CURRENT;
    shmem.fault_mode = LIGHT_FAULT_MODE_DEGRADED;
    shmem.fault_lifecycle = LIGHT_FAULT_LIFECYCLE_RECOVERING;
    shmem.fault_recovery_ticks = 1U;
    shmem.fault_recovery_elapsed_ms = 1000U;
    shmem.fault_recovery_window_ms = light_fault_recovery_window_ms();
    shmem.active_fault_mask = 0U;
    shmem.vehicle_state.speed_kph = 88U;
    shmem.vehicle_state.ignition_on = 1U;
    shmem.vehicle_state.brake_pedal = 0U;
    shmem.vehicle_state.gear = LIGHT_VEHICLE_GEAR_DRIVE;
    shmem.vehicle_state.ambient_light = LIGHT_VEHICLE_AMBIENT_NIGHT;
    shmem.vehicle_state.hazard = 1U;
    shmem.vehicle_state.drive_mode = LIGHT_VEHICLE_DRIVE_MODE_EMERGENCY;
    shmem.target_output.low_beam_on = 1U;
    shmem.target_output.marker_on = 1U;
    shmem.allow_flags = 0x28U;
    shmem.last_fault_code = LIGHT_ERR_MODE_CONFLICT;
    shmem.total_fault_count = 3U;

    snapshot = light_status_snapshot_capture(&shmem);

    expect_true(snapshot.fault_mode == LIGHT_FAULT_MODE_DEGRADED,
                "snapshot should preserve fault mode");
    expect_true(snapshot.contract_status == 0U,
                "snapshot should preserve contract compatibility status");
    expect_true(snapshot.lifecycle == LIGHT_FAULT_LIFECYCLE_RECOVERING,
                "snapshot should preserve lifecycle");
    expect_true(snapshot.recovery_ticks == 1U,
                "snapshot should preserve recovery ticks");
    expect_true(snapshot.recovery_elapsed_ms == 1000U,
                "snapshot should preserve recovery elapsed time");
    expect_true(snapshot.vehicle_state.speed_kph == 88U,
                "snapshot should preserve speed");
    expect_true(snapshot.target_output.low_beam_on == 1U,
                "snapshot should preserve target output");
    expect_true(snapshot.total_fault_count == 3U,
                "snapshot should preserve fault count");
}

static void test_snapshot_format_emits_unified_status_line(void) {
    light_status_snapshot_t snapshot;
    char buf[512];
    int len;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.fault_mode = LIGHT_FAULT_MODE_SAFE_MODE;
    snapshot.lifecycle = LIGHT_FAULT_LIFECYCLE_RECOVERING;
    snapshot.recovery_ticks = 1U;
    snapshot.recovery_elapsed_ms = 1000U;
    snapshot.recovery_window_ms = light_fault_recovery_window_ms();
    snapshot.vehicle_state.speed_kph = 5U;
    snapshot.vehicle_state.ignition_on = 1U;
    snapshot.vehicle_state.brake_pedal = 1U;
    snapshot.vehicle_state.gear = LIGHT_VEHICLE_GEAR_DRIVE;
    snapshot.vehicle_state.ambient_light = LIGHT_VEHICLE_AMBIENT_NIGHT;
    snapshot.vehicle_state.hazard = 1U;
    snapshot.vehicle_state.drive_mode = LIGHT_VEHICLE_DRIVE_MODE_EMERGENCY;
    snapshot.target_output.low_beam_on = 1U;
    snapshot.target_output.marker_on = 1U;
    snapshot.target_output.brake_on = 1U;
    snapshot.allow_flags = 0x29U;
    snapshot.last_fault_code = LIGHT_ERR_HW_STATE_ERR;
    snapshot.total_fault_count = 2U;

    len = light_status_snapshot_format(buf, sizeof(buf), snapshot);

    expect_true(len > 0, "snapshot formatter should produce output");
    expect_true(strstr(buf, "STATUS_SNAPSHOT fault=SAFE_MODE") != NULL,
                "snapshot formatter should include fault mode");
    expect_true(strstr(buf, "lifecycle=RECOVERING") != NULL,
                "snapshot formatter should include lifecycle");
    expect_true(strstr(buf, "recovery_ticks=1/2") != NULL,
                "snapshot formatter should include recovery progress");
    expect_true(strstr(buf, "recovery_elapsed_ms=1000/2000") != NULL,
                "snapshot formatter should include recovery elapsed-time progress");
    expect_true(strstr(buf, "speed=5") != NULL,
                "snapshot formatter should include vehicle speed");
    expect_true(strstr(buf, "gear=3 ambient=2 hazard=1 drive_mode=3") != NULL,
                "snapshot formatter should include v2.0 vehicle model fields");
    expect_true(strstr(buf, "target[low=1 high=0 left=0 right=0 marker=1 brake=1]") != NULL,
                "snapshot formatter should include target output");
    expect_true(strstr(buf, "contract=OK") != NULL,
                "snapshot formatter should include contract status");
    expect_true(strstr(buf, "last_fault_name=HW_STATE_ERR") != NULL,
                "snapshot formatter should include readable fault code name");
}

int main(void) {
    test_snapshot_capture_reads_shared_state_consistently();
    test_snapshot_format_emits_unified_status_line();

    printf("light_status_snapshot tests passed\n");
    return 0;
}
