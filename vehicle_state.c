#include <microkit.h>

#include "logger.h"
#include "light_protocol.h"
#include "light_transport.h"
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
        light_transport_message_t message = *(light_transport_message_t *)input_buffer;
        light_vehicle_state_request_t request;
        light_vehicle_state_update_result_t result;

        if (message.version != LIGHT_TRANSPORT_VERSION
            || message.type != LIGHT_TRANSPORT_MSG_VEHICLE_STATE_UPDATE
            || message.len != sizeof(message.payload.vehicle_state_update)) {
            LOG_INFO("VEHICLE_STATE_MSG_REJECT type=%u len=%u version=%u",
                     (unsigned int)message.type,
                     (unsigned int)message.len,
                     (unsigned int)message.version);
            return;
        }

        request = message.payload.vehicle_state_update;
        result = light_vehicle_state_apply_request((light_vehicle_state_t)g_shmem->vehicle_state,
                                                   request);

        if (!result.accepted) {
            LOG_INFO("VEHICLE_STATE_REJECT field=%u value=%u reason=%d",
                     (unsigned int)request.field,
                     (unsigned int)request.value,
                     (int)result.reason);
            return;
        }

        g_shmem->vehicle_state = result.next_state;
        LOG_INFO("VEHICLE_STATE_UPDATE field=%u value=%u changed=%d speed=%u brake=%u ignition=%u",
                 (unsigned int)request.field,
                 (unsigned int)request.value,
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
