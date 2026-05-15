# Safety and Fault Model

This document captures the current safety argument for the lighting demo. The implementation is intentionally small, but it already has a clear fault taxonomy, escalation policy, recovery lifecycle, and anti-flap behavior.

## Safety Intent

The lighting controller favors conservative output when the system detects unsafe or inconsistent behavior. The goal is not to model a complete production automotive safety case yet. The goal is to make fault behavior explicit, reviewable, and testable while preserving the Microkit protection-domain boundaries.

The current safety invariant is:

```text
Fault handling is centralized in faultmg; execution domains may report faults,
but they do not directly own global fault mode transitions.
```

## Fault Taxonomy

| Fault code | Name | Typical source | Meaning |
| ---: | --- | --- | --- |
| `0x01` | `LIGHT_ERR_SPEED_LIMIT` | `lightctl` runtime guard | Requested action conflicts with vehicle speed policy, such as high beam below 10 km/h or turn action above 120 km/h. |
| `0x02` | `LIGHT_ERR_MODE_CONFLICT` | `lightctl` runtime guard or test injection | Requested action conflicts with current lighting state, such as disabling low beam while high beam is active or turn request during brake-active state. |
| `0x03` | `LIGHT_ERR_INVALID_CMD` | command/channel validation | Invalid command, invalid transport message, or unexpected Microkit channel. |
| `0x04` | `LIGHT_ERR_HW_STATE_ERR` | runtime guard or test injection | Hardware-facing state is unsafe or inconsistent enough to require conservative output. |

Each fault updates `last_fault_code`, `active_fault_mask`, total counters, and active counters in the fault state owned by `faultmg`.

## Fault Modes

| Mode | Intent | Output policy |
| --- | --- | --- |
| `NORMAL` | No active safety restriction. | Requested target output passes through policy. |
| `WARN` | Fault observed, but output can still follow requested target. | Same output policy as normal, with fault state visible for diagnostics. |
| `DEGRADED` | Repeated mode conflict indicates unstable command/state behavior. | High beam is forced off; minimum illumination is enforced. |
| `SAFE_MODE` | Repeated hardware-state error indicates high-risk state. | Turn signals and high beam are forced off; low beam and position light are forced on; brake output is preserved. |

The output policy is implemented in `light_output_policy.c`. The runtime guard is implemented in `light_runtime_guard.c`.

## Escalation Policy

Fault mode is derived from active fault counters:

| Condition | Result |
| --- | --- |
| No active fault counters | `NORMAL` |
| Any speed-limit, mode-conflict, invalid-command, or hardware-state error | `WARN` |
| Three consecutive `LIGHT_ERR_MODE_CONFLICT` events | `DEGRADED` |
| Two `LIGHT_ERR_HW_STATE_ERR` events | `SAFE_MODE` |

Consecutive mode-conflict count is reset by non-conflict errors. This prevents unrelated faults from accidentally building a false conflict streak.

## Lifecycle States

Fault mode and lifecycle are separate:

| Lifecycle | Meaning |
| --- | --- |
| `STABLE` | No active fault and no recovery in progress. |
| `ACTIVE` | One or more active faults are present. |
| `RECOVERING` | Active faults were cleared, but the system has not yet returned directly to `NORMAL`. |

This separation matters because a system can have no active fault markers while still intentionally staying in `SAFE_MODE` or `DEGRADED` during observation.

## Recovery Policy

`clear` does not immediately restore `NORMAL`.

Current recovery behavior:

1. A fault event enters `ACTIVE` and derives the current fault mode.
2. A valid clear request removes active fault markers.
3. If the mode is above `NORMAL`, lifecycle moves to `RECOVERING`.
4. Each healthy observation tick advances `recovery_ticks`.
5. Once the recovery window is satisfied, the mode steps down exactly one level.
6. The system repeats observation until it returns to `NORMAL`.
7. When mode reaches `NORMAL`, lifecycle returns to `STABLE`.

The current recovery window is `2` observation ticks, defined by `LIGHT_FAULT_RECOVERY_WINDOW_TICKS` in `light_fault_mode.c`.

## Anti-Flap Behavior

If a new fault occurs while the system is recovering:

- lifecycle returns to `ACTIVE`
- `recovery_ticks` resets to zero
- active fault mask is updated
- the system does not immediately reduce severity simply because active counters were previously cleared

This is the anti-flap behavior. It prevents a repeated clear/fault pattern from quickly bouncing the system back to `NORMAL`.

## Observable Evidence

The safety model is visible through:

- `STATUS_SNAPSHOT` query output from `commandin`
- `FAULTMG_MODE_TRANSITION` logs
- `FAULTMG_CLEAR` and `FAULTMG_RECOVERY_TICK` logs
- `LIGHTCTL_TARGET_SUMMARY` logs
- host-side tests in `tests/test_light_fault_mode.c`
- QEMU fault-injection and serial E2E scripts under `scripts/`

Recommended validation:

```bash
make test
make qemu-test
```

Useful focused checks:

```bash
make test-fault
make test-runtime
make test-integration-fault
make test-serial-e2e
```

## Current Limits

- Recovery uses observation ticks, not a real-time clock source.
- Fault taxonomy is intentionally small and should be expanded before claiming production realism.
- Fault severity thresholds are project policy constants, not derived from an automotive standard.
- The current evidence is suitable for engineering practice review, not formal certification.
