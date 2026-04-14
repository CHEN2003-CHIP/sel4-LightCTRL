#include "light_execution_plan.h"

static void plan_add(light_execution_plan_t *plan, uint32_t action) {
    if (plan->action_count < LIGHT_EXECUTION_PLAN_MAX_ACTIONS) {
        plan->actions[plan->action_count++] = action;
    }
}

light_execution_state_t light_execution_state_init(void) {
    light_execution_state_t state;

    state.turn_state = 0;
    state.beam_state = 0;
    state.brake_state = 0;
    state.position_state = 0;

    return state;
}

light_execution_plan_t light_execution_plan_build(light_execution_state_t current_state,
                                                  light_target_output_t target_output) {
    light_execution_plan_t plan;

    plan.action_count = 0;
    plan.next_state = current_state;

    if (target_output.brake_on != 0U) {
        if (current_state.brake_state != 1U) {
            plan_add(&plan, LIGHT_CH_GPIO_BRAKE_ON);
            plan.next_state.brake_state = 1;
        }
    } else if (current_state.brake_state != 0U) {
        plan_add(&plan, LIGHT_CH_GPIO_BRAKE_OFF);
        plan.next_state.brake_state = 0;
    }

    if (target_output.left_turn_on != 0U) {
        if (current_state.turn_state == 2U) {
            plan_add(&plan, LIGHT_CH_GPIO_TURN_RIGHT_OFF);
            current_state.turn_state = 0;
        }
        if (current_state.turn_state != 1U) {
            plan_add(&plan, LIGHT_CH_GPIO_TURN_LEFT_ON);
            plan.next_state.turn_state = 1;
        }
    } else if (target_output.right_turn_on != 0U) {
        if (current_state.turn_state == 1U) {
            plan_add(&plan, LIGHT_CH_GPIO_TURN_LEFT_OFF);
            current_state.turn_state = 0;
        }
        if (current_state.turn_state != 2U) {
            plan_add(&plan, LIGHT_CH_GPIO_TURN_RIGHT_ON);
            plan.next_state.turn_state = 2;
        }
    } else if (current_state.turn_state == 1U) {
        plan_add(&plan, LIGHT_CH_GPIO_TURN_LEFT_OFF);
        plan.next_state.turn_state = 0;
    } else if (current_state.turn_state == 2U) {
        plan_add(&plan, LIGHT_CH_GPIO_TURN_RIGHT_OFF);
        plan.next_state.turn_state = 0;
    }

    if (target_output.low_beam_on != 0U) {
        if (current_state.beam_state == 2U) {
            plan_add(&plan, LIGHT_CH_GPIO_HIGH_BEAM_OFF);
            current_state.beam_state = 0;
        }
        if (current_state.beam_state != 1U) {
            plan_add(&plan, LIGHT_CH_GPIO_LOW_BEAM_ON);
            plan.next_state.beam_state = 1;
        }
    } else if (target_output.high_beam_on != 0U) {
        if (current_state.beam_state == 1U) {
            plan_add(&plan, LIGHT_CH_GPIO_LOW_BEAM_OFF);
            current_state.beam_state = 0;
        }
        if (current_state.beam_state != 2U) {
            plan_add(&plan, LIGHT_CH_GPIO_HIGH_BEAM_ON);
            plan.next_state.beam_state = 2;
        }
    } else if (current_state.beam_state == 1U) {
        plan_add(&plan, LIGHT_CH_GPIO_LOW_BEAM_OFF);
        plan.next_state.beam_state = 0;
    } else if (current_state.beam_state == 2U) {
        plan_add(&plan, LIGHT_CH_GPIO_HIGH_BEAM_OFF);
        plan.next_state.beam_state = 0;
    }

    if (target_output.marker_on != 0U) {
        if (current_state.position_state != 1U) {
            plan_add(&plan, LIGHT_CH_GPIO_POSITION_ON);
            plan.next_state.position_state = 1;
        }
    } else if (current_state.position_state != 0U) {
        plan_add(&plan, LIGHT_CH_GPIO_POSITION_OFF);
        plan.next_state.position_state = 0;
    }

    return plan;
}
