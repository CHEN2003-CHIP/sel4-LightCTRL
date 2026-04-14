#include <microkit.h>

#include "logger.h"
#include "light_protocol.h"

#define CH_SCHEDULER_VEHICLE_UPDATE 16

uintptr_t shared_memory_base_vaddr;

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
    LOG_INFO("VEHICLE_STATE_IGNORE channel=%d", ch);
}
