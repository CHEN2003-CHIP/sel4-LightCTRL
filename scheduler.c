#include <stdbool.h>
#include <microkit.h>

#include "include/logger.h"
#include "light_control_logic.h"
#include "light_fault_mode.h"
#include "light_protocol.h"
#include "light_transport.h"

#define CH_UART_CMD 4
#define CH_LIGHT_CONTROL_SYNC 9
#define CH_FAULT_MODE_UPDATE 13
#define CH_VEHICLE_STATE_UPDATE 15

uintptr_t shared_memory_base_vaddr;
uintptr_t input_buffer;

static light_shmem_t *g_shmem = NULL;

static void log_command_result(light_control_command_result_t result) {
    if (!result.accepted) {
        LOG_INFO("SCHED_CMD_REJECT cmd=0x%02x reason=%d",
                 (unsigned int)result.cmd,
                 (int)result.reason);
        return;
    }

    LOG_INFO("SCHED_CMD_ACCEPT cmd=0x%02x low=%u high=%u left=%u right=%u marker=%u brake=%u",
             (unsigned int)result.cmd,
             (unsigned int)result.next_request.low_beam_req,
             (unsigned int)result.next_request.high_beam_req,
             (unsigned int)result.next_request.left_turn_req,
             (unsigned int)result.next_request.right_turn_req,
             (unsigned int)result.next_request.marker_req,
             (unsigned int)result.next_request.brake_req);
}

static void recompute_target_output(void) {
    light_target_output_t target_output =
        light_control_compute_target_output((light_operator_request_t)g_shmem->operator_request,
                                            (light_vehicle_state_t)g_shmem->vehicle_state,
                                            (fault_mode_t)g_shmem->fault_mode);

    g_shmem->target_output = target_output;
    g_shmem->allow_flags = light_target_output_to_allow_flags(target_output);
    g_shmem->vehicle_speed = g_shmem->vehicle_state.speed_kph;

    LOG_INFO("SCHED_TARGET mode=%s speed=%u ignition=%u brake_pedal=%u req[low=%u high=%u left=%u right=%u marker=%u brake=%u] target[low=%u high=%u left=%u right=%u marker=%u brake=%u]",
             light_fault_mode_name((fault_mode_t)g_shmem->fault_mode),
             (unsigned int)g_shmem->vehicle_state.speed_kph,
             (unsigned int)g_shmem->vehicle_state.ignition_on,
             (unsigned int)g_shmem->vehicle_state.brake_pedal,
             (unsigned int)g_shmem->operator_request.low_beam_req,
             (unsigned int)g_shmem->operator_request.high_beam_req,
             (unsigned int)g_shmem->operator_request.left_turn_req,
             (unsigned int)g_shmem->operator_request.right_turn_req,
             (unsigned int)g_shmem->operator_request.marker_req,
             (unsigned int)g_shmem->operator_request.brake_req,
             (unsigned int)target_output.low_beam_on,
             (unsigned int)target_output.high_beam_on,
             (unsigned int)target_output.left_turn_on,
             (unsigned int)target_output.right_turn_on,
             (unsigned int)target_output.marker_on,
             (unsigned int)target_output.brake_on);
}

void init(void) {
    g_shmem = (light_shmem_t *)shared_memory_base_vaddr;

    g_shmem->layout_version = LIGHT_SHARED_STATE_LAYOUT_V2;
    g_shmem->uart_cmd = LIGHT_UART_CMD_INVALID;
    g_shmem->last_fault_code = 0U;
    g_shmem->total_fault_count = 0U;
    g_shmem->operator_request = light_operator_request_init();
    if (g_shmem->vehicle_state.ignition_on == 0U && g_shmem->vehicle_state.speed_kph == 0U
        && g_shmem->vehicle_state.brake_pedal == 0U) {
        g_shmem->vehicle_state = light_vehicle_state_default();
    }
    g_shmem->fault_mode = (uint8_t)LIGHT_FAULT_MODE_NORMAL;
    g_shmem->target_output = light_target_output_init();
    recompute_target_output();

    LOG_INFO("SCHED_INIT module=scheduler status=ready layout=%u",
             (unsigned int)g_shmem->layout_version);
}

void notified(microkit_channel ch) {
    bool need_notify_light_control = false;

    if (ch == CH_UART_CMD) {
        light_transport_message_t message = *(light_transport_message_t *)input_buffer;
        light_control_command_result_t result;

        if (message.version != LIGHT_TRANSPORT_VERSION
            || message.type != LIGHT_TRANSPORT_MSG_LIGHT_CMD
            || message.len != sizeof(message.payload.light_cmd)) {
            LOG_INFO("SCHED_MSG_REJECT type=%u len=%u version=%u",
                     (unsigned int)message.type,
                     (unsigned int)message.len,
                     (unsigned int)message.version);
            return;
        }

        result = light_control_apply_operator_command((light_operator_request_t)g_shmem->operator_request,
                                                      message.payload.light_cmd);

        log_command_result(result);
        if (result.accepted) {
            g_shmem->operator_request = result.next_request;
            g_shmem->uart_cmd = message.payload.light_cmd;
            recompute_target_output();
            need_notify_light_control = result.notify;
        }
    } else if (ch == CH_FAULT_MODE_UPDATE || ch == CH_VEHICLE_STATE_UPDATE) {
        recompute_target_output();
        need_notify_light_control = true;
    } else {
        LOG_INFO("Scheduler: Unknown channel received\n");
    }

    if (need_notify_light_control) {
        microkit_notify(CH_LIGHT_CONTROL_SYNC);
    }
}
