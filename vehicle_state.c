#include <microkit.h>

#include "logger.h"
#include "light_protocol.h"
#include "light_vehicle_state.h"

#define CH_SCHEDULER_VEHICLE_UPDATE 16
#define CH_COMMAND_INPUT 18

uintptr_t shared_memory_base_vaddr;
uintptr_t input_buffer;

static light_shmem_t *g_shmem = NULL;

void init(void) {
    light_vehicle_state_t vehicle_state = light_vehicle_state_default();

    g_shmem = (light_shmem_t *)shared_memory_base_vaddr;
    g_shmem->vehicle_state = vehicle_state;

    LOG_INFO("VEHICLE_STATE_INIT speed=%u brake=%u ignition=%u",
             (unsigned int)vehicle_state.speed_kph,
             (unsigned int)vehicle_state.brake_pedal,
             (unsigned int)vehicle_state.ignition_on);

    microkit_notify(CH_SCHEDULER_VEHICLE_UPDATE);
}

void notified(microkit_channel ch) {
    if (ch == CH_COMMAND_INPUT) {
        uint8_t cmd = *(uint8_t *)input_buffer;
        light_vehicle_state_update_result_t result =
            light_vehicle_state_apply_command((light_vehicle_state_t)g_shmem->vehicle_state, cmd);

        if (!result.accepted) {
            LOG_INFO("VEHICLE_STATE_REJECT cmd=0x%02x reason=%d",
                     (unsigned int)cmd,
                     (int)result.reason);
            return;
        }

        g_shmem->vehicle_state = result.next_state;
        LOG_INFO("VEHICLE_STATE_UPDATE cmd=0x%02x changed=%d speed=%u brake=%u ignition=%u",
                 (unsigned int)cmd,
                 result.changed ? 1 : 0,
                 (unsigned int)result.next_state.speed_kph,
                 (unsigned int)result.next_state.brake_pedal,
                 (unsigned int)result.next_state.ignition_on);
        if (result.changed) {
            microkit_notify(CH_SCHEDULER_VEHICLE_UPDATE);
        }
        return;
    }

    LOG_INFO("VEHICLE_STATE_IGNORE channel=%d", ch);
}
