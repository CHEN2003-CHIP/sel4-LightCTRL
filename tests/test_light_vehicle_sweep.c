#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "light_control_logic.h"
#include "light_vehicle_state.h"

static void fail(const char *message) {
    fprintf(stderr, "test failed: %s\n", message);
    exit(1);
}

static light_operator_request_t request_profile(unsigned int id) {
    light_operator_request_t request = light_operator_request_init();

    switch (id) {
        case 1U:
            request.low_beam_req = 1U;
            break;
        case 2U:
            request.low_beam_req = 1U;
            request.high_beam_req = 1U;
            break;
        case 3U:
            request.left_turn_req = 1U;
            break;
        case 4U:
            request.brake_req = 1U;
            break;
        default:
            break;
    }

    return request;
}

static const char *profile_name(unsigned int id) {
    switch (id) {
        case 0U:
            return "idle";
        case 1U:
            return "low";
        case 2U:
            return "high";
        case 3U:
            return "left";
        case 4U:
            return "brake";
        default:
            return "unknown";
    }
}

int main(int argc, char **argv) {
    static const unsigned int speeds[] = {0U, 5U, 10U, 30U, 80U, 130U};
    FILE *csv = NULL;
    unsigned long rows = 0UL;
    unsigned long low_rows = 0UL;
    unsigned long high_rows = 0UL;
    unsigned long marker_rows = 0UL;
    unsigned long hazard_rows = 0UL;
    unsigned long brake_rows = 0UL;
    unsigned long safe_mode_rows = 0UL;
    unsigned long degraded_rows = 0UL;

    if (argc > 1 && argv[1] != NULL) {
        csv = fopen(argv[1], "w");
        if (csv == NULL) {
            fail("could not open sweep csv output");
        }
    }

    if (csv != NULL) {
        fprintf(csv,
                "profile,speed,ignition,brake_pedal,gear,ambient,hazard,drive_mode,fault_mode,low,high,left,right,marker,brake\n");
    }

    for (unsigned int profile = 0U; profile < 5U; profile++) {
        for (size_t speed_i = 0U; speed_i < sizeof(speeds) / sizeof(speeds[0]); speed_i++) {
            for (unsigned int ignition = 0U; ignition <= 1U; ignition++) {
                for (unsigned int brake_pedal = 0U; brake_pedal <= 1U; brake_pedal++) {
                    for (unsigned int gear = 0U; gear <= LIGHT_VEHICLE_GEAR_MAX; gear++) {
                        for (unsigned int ambient = 0U; ambient <= LIGHT_VEHICLE_AMBIENT_MAX; ambient++) {
                            for (unsigned int hazard = 0U; hazard <= 1U; hazard++) {
                                for (unsigned int drive_mode = 0U;
                                     drive_mode <= LIGHT_VEHICLE_DRIVE_MODE_MAX;
                                     drive_mode++) {
                                    for (unsigned int fault_mode = LIGHT_FAULT_MODE_NORMAL;
                                         fault_mode <= LIGHT_FAULT_MODE_SAFE_MODE;
                                         fault_mode++) {
                                        light_vehicle_state_t vehicle = light_vehicle_state_default();
                                        light_operator_request_t request = request_profile(profile);
                                        light_target_output_t target;

                                        vehicle.speed_kph = (uint16_t)speeds[speed_i];
                                        vehicle.ignition_on = (uint8_t)ignition;
                                        vehicle.brake_pedal = (uint8_t)brake_pedal;
                                        vehicle.gear = (uint8_t)gear;
                                        vehicle.ambient_light = (uint8_t)ambient;
                                        vehicle.hazard = (uint8_t)hazard;
                                        vehicle.drive_mode = (uint8_t)drive_mode;

                                        target = light_control_compute_target_output(request,
                                                                                    vehicle,
                                                                                    (fault_mode_t)fault_mode);

                                        rows++;
                                        low_rows += target.low_beam_on != 0U ? 1UL : 0UL;
                                        high_rows += target.high_beam_on != 0U ? 1UL : 0UL;
                                        marker_rows += target.marker_on != 0U ? 1UL : 0UL;
                                        hazard_rows +=
                                            target.left_turn_on != 0U && target.right_turn_on != 0U ? 1UL : 0UL;
                                        brake_rows += target.brake_on != 0U ? 1UL : 0UL;
                                        safe_mode_rows += fault_mode == LIGHT_FAULT_MODE_SAFE_MODE ? 1UL : 0UL;
                                        degraded_rows += fault_mode == LIGHT_FAULT_MODE_DEGRADED ? 1UL : 0UL;

                                        if (csv != NULL) {
                                            fprintf(csv,
                                                    "%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                                                    profile_name(profile),
                                                    speeds[speed_i],
                                                    ignition,
                                                    brake_pedal,
                                                    gear,
                                                    ambient,
                                                    hazard,
                                                    drive_mode,
                                                    fault_mode,
                                                    target.low_beam_on,
                                                    target.high_beam_on,
                                                    target.left_turn_on,
                                                    target.right_turn_on,
                                                    target.marker_on,
                                                    target.brake_on);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (csv != NULL) {
        fclose(csv);
    }

    if (rows != 46080UL) {
        fail("unexpected sweep row count");
    }
    if (safe_mode_rows == 0UL || degraded_rows == 0UL || low_rows == 0UL || marker_rows == 0UL) {
        fail("sweep did not exercise expected output/fault states");
    }

    printf("VEHICLE_SWEEP rows=%lu profiles=5 speeds=6 ignition=2 brake_pedal=2 gear=4 ambient=3 hazard=2 drive_mode=4 fault_mode=4\n",
           rows);
    printf("VEHICLE_SWEEP_OUTPUT low=%lu high=%lu hazard_pair=%lu marker=%lu brake=%lu degraded_rows=%lu safe_mode_rows=%lu\n",
           low_rows,
           high_rows,
           hazard_rows,
           marker_rows,
           brake_rows,
           degraded_rows,
           safe_mode_rows);
    if (argc > 1 && argv[1] != NULL) {
        printf("VEHICLE_SWEEP_CSV path=%s\n", argv[1]);
    }
    printf("light_vehicle_sweep tests passed\n");

    return 0;
}
