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
    shmem.fault_mode = LIGHT_FAULT_MODE_DEGRADED;
    shmem.fault_lifecycle = LIGHT_FAULT_LIFECYCLE_RECOVERING;
    shmem.fault_recovery_ticks = 1U;
    shmem.active_fault_mask = 0U;
    shmem.vehicle_state.speed_kph = 88U;
    shmem.vehicle_state.ignition_on = 1U;
    shmem.vehicle_state.brake_pedal = 0U;
    shmem.target_output.low_beam_on = 1U;
    shmem.target_output.marker_on = 1U;
    shmem.allow_flags = 0x28U;
    shmem.last_fault_code = LIGHT_ERR_MODE_CONFLICT;
    shmem.total_fault_count = 3U;

    snapshot = light_status_snapshot_capture(&shmem);

    expect_true(snapshot.fault_mode == LIGHT_FAULT_MODE_DEGRADED,
                "snapshot should preserve fault mode");
    expect_true(snapshot.lifecycle == LIGHT_FAULT_LIFECYCLE_RECOVERING,
                "snapshot should preserve lifecycle");
    expect_true(snapshot.recovery_ticks == 1U,
                "snapshot should preserve recovery ticks");
    expect_true(snapshot.vehicle_state.speed_kph == 88U,
                "snapshot should preserve speed");
    expect_true(snapshot.target_output.low_beam_on == 1U,
                "snapshot should preserve target output");
    expect_true(snapshot.total_fault_count == 3U,
                "snapshot should preserve fault count");
}

static void test_snapshot_format_emits_unified_status_line(void) {
    light_status_snapshot_t snapshot;
    char buf[256];
    int len;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.fault_mode = LIGHT_FAULT_MODE_SAFE_MODE;
    snapshot.lifecycle = LIGHT_FAULT_LIFECYCLE_RECOVERING;
    snapshot.recovery_ticks = 1U;
    snapshot.vehicle_state.speed_kph = 5U;
    snapshot.vehicle_state.ignition_on = 1U;
    snapshot.vehicle_state.brake_pedal = 1U;
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
    expect_true(strstr(buf, "speed=5") != NULL,
                "snapshot formatter should include vehicle speed");
    expect_true(strstr(buf, "target[low=1 high=0 left=0 right=0 marker=1 brake=1]") != NULL,
                "snapshot formatter should include target output");
}

int main(void) {
    test_snapshot_capture_reads_shared_state_consistently();
    test_snapshot_format_emits_unified_status_line();

    printf("light_status_snapshot tests passed\n");
    return 0;
}
