#include <microkit.h>

#include "logger.h"
#include "light_contract.h"
#include "light_protocol.h"
#include "light_transport.h"
#include "light_vehicle_state.h"

#define DEMO_ANSI_RESET "\x1b[0m"
#define DEMO_ANSI_GREEN "\x1b[1;32m"
#define DEMO_ANSI_RED   "\x1b[1;31m"

uintptr_t shared_memory_base_vaddr;
uintptr_t input_buffer;

static light_shmem_t *g_shmem = NULL;

static const char *vehicle_field_name(uint8_t field) {
    switch ((light_vehicle_field_t)field) {
        case LIGHT_VEHICLE_FIELD_SPEED_KPH:
            return "speed";
        case LIGHT_VEHICLE_FIELD_IGNITION_ON:
            return "ignition";
        case LIGHT_VEHICLE_FIELD_BRAKE_PEDAL:
            return "brake";
        case LIGHT_VEHICLE_FIELD_GEAR:
            return "gear";
        case LIGHT_VEHICLE_FIELD_AMBIENT_LIGHT:
            return "ambient";
        case LIGHT_VEHICLE_FIELD_HAZARD:
            return "hazard";
        case LIGHT_VEHICLE_FIELD_DRIVE_MODE:
            return "mode";
        default:
            return "unknown";
    }
}

void init(void) {
    light_vehicle_state_t vehicle_state = light_vehicle_state_default();

    g_shmem = (light_shmem_t *)shared_memory_base_vaddr;
    g_shmem->vehicle_state = vehicle_state;

    LOG_INFO("VEHICLE_STATE_INIT speed=%u brake=%u ignition=%u gear=%u ambient=%u hazard=%u drive_mode=%u",
             (unsigned int)vehicle_state.speed_kph,
             (unsigned int)vehicle_state.brake_pedal,
             (unsigned int)vehicle_state.ignition_on,
             (unsigned int)vehicle_state.gear,
             (unsigned int)vehicle_state.ambient_light,
             (unsigned int)vehicle_state.hazard,
             (unsigned int)vehicle_state.drive_mode);

    microkit_notify(LIGHT_CH_VEHICLE_STATE_TO_SCHEDULER);
}

void notified(microkit_channel ch) {
    if (ch == LIGHT_CH_VEHICLE_STATE_FROM_COMMANDIN) {
        light_transport_message_t message = *(light_transport_message_t *)input_buffer;
        light_vehicle_state_request_t request;
        light_vehicle_state_update_result_t result;

        light_contract_check_t contract =
            light_contract_check_transport_message(message, LIGHT_TRANSPORT_MSG_VEHICLE_STATE_UPDATE);

        if (contract.status != LIGHT_CONTRACT_OK) {
            LOG_INFO("VEHICLE_STATE_CONTRACT_REJECT reason=%s expected=%u actual=%u type=%u len=%u version=%u",
                     light_contract_status_name(contract.status),
                     (unsigned int)contract.expected,
                     (unsigned int)contract.actual,
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
            LOG_INFO("%sDEMO_RESULT%s stage=vehicle_state status=REJECT field=%s value=%u reason=%d",
                     DEMO_ANSI_RED,
                     DEMO_ANSI_RESET,
                     vehicle_field_name(request.field),
                     (unsigned int)request.value,
                     (int)result.reason);
            return;
        }

        g_shmem->vehicle_state = result.next_state;
        LOG_INFO("VEHICLE_STATE_UPDATE field=%u value=%u changed=%d speed=%u brake=%u ignition=%u gear=%u ambient=%u hazard=%u drive_mode=%u",
                 (unsigned int)request.field,
                 (unsigned int)request.value,
                 result.changed ? 1 : 0,
                 (unsigned int)result.next_state.speed_kph,
                 (unsigned int)result.next_state.brake_pedal,
                 (unsigned int)result.next_state.ignition_on,
                 (unsigned int)result.next_state.gear,
                 (unsigned int)result.next_state.ambient_light,
                 (unsigned int)result.next_state.hazard,
                 (unsigned int)result.next_state.drive_mode);
        LOG_INFO("%sDEMO_RESULT%s stage=vehicle_state status=ACCEPT field=%s value=%u changed=%d state=speed:%u,ignition:%u,brake:%u,gear:%s,ambient:%s,hazard:%u,mode:%s",
                 DEMO_ANSI_GREEN,
                 DEMO_ANSI_RESET,
                 vehicle_field_name(request.field),
                 (unsigned int)request.value,
                 result.changed ? 1 : 0,
                 (unsigned int)result.next_state.speed_kph,
                 (unsigned int)result.next_state.ignition_on,
                 (unsigned int)result.next_state.brake_pedal,
                 light_vehicle_gear_name(result.next_state.gear),
                 light_vehicle_ambient_light_name(result.next_state.ambient_light),
                 (unsigned int)result.next_state.hazard,
                 light_vehicle_drive_mode_name(result.next_state.drive_mode));
        if (result.changed) {
            microkit_notify(LIGHT_CH_VEHICLE_STATE_TO_SCHEDULER);
        }
        return;
    }

    LOG_INFO("VEHICLE_STATE_IGNORE channel=%d", ch);
}
