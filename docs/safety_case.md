# Safety and Fault Model

This document captures the final v2.0 safety argument for LightDemo.

## Safety Intent

The controller favors conservative output when it detects unsafe or inconsistent behavior. The project does not claim formal automotive certification; it demonstrates bounded, reviewable, and testable fault handling in a seL4/Microkit system.

Core invariant:

```text
faultmg is the only owner of global fault mode and lifecycle transitions.
```

## Fault Taxonomy

| Fault code | Name | Source | Severity rule | Recovery policy | Output policy |
| ---: | --- | --- | --- | --- | --- |
| `0x01` | `SPEED_LIMIT` | `lightctl.runtime_guard` | `WARN` | clear then elapsed-time window | preserve requested output |
| `0x02` | `MODE_CONFLICT` | runtime guard or test injection | `DEGRADED_AFTER_3_CONSECUTIVE` | clear then elapsed-time window | disable high beam, enforce minimum illumination |
| `0x03` | `INVALID_CMD` | transport or channel contract | `WARN` | clear then elapsed-time window | preserve requested output |
| `0x04` | `HW_STATE_ERR` | hardware state or test injection | `SAFE_MODE_AFTER_2` | clear then elapsed-time window | conservative low-beam and position profile |

Runtime evidence:

```text
FAULTMG_EVENT ... name=HW_STATE_ERR severity=SAFE_MODE_AFTER_2 recovery_policy=clear_then_elapsed_window
FAULTMG_EVENT ... name=MODE_CONFLICT severity=DEGRADED_AFTER_3_CONSECUTIVE
```

## Fault Modes

| Mode | Intent | Output behavior |
| --- | --- | --- |
| `NORMAL` | No active restriction | Requested target output passes through policy |
| `WARN` | Fault observed | Output remains visible in diagnostics |
| `DEGRADED` | Repeated mode conflict | High beam off, minimum illumination enforced |
| `SAFE_MODE` | Repeated hardware-state error | Low beam and position on; high beam and turn outputs off; brake preserved |

## Lifecycle

| Lifecycle | Meaning |
| --- | --- |
| `STABLE` | No active fault and no recovery in progress |
| `ACTIVE` | One or more active fault markers exist |
| `RECOVERING` | Active faults were cleared but the system is still stepping down severity |

## Elapsed-Time Recovery

v2.0 replaces a purely observation-count recovery story with an elapsed-time recovery window. The accepted configured window is visible in logs:

```text
recovery_elapsed_ms=1000 recovery_window_ms=2000
STATUS_SNAPSHOT ... recovery_elapsed_ms=0/2000 ... layout=4 contract=OK
```

Recovery rules:

1. Fault events enter `ACTIVE`.
2. Clear removes active fault markers.
3. If mode is above `NORMAL`, lifecycle becomes `RECOVERING`.
4. Recovery progress is measured against an elapsed-time window.
5. A satisfied window steps down exactly one fault mode level.
6. New faults during recovery interrupt recovery and return lifecycle to `ACTIVE`.
7. Returning to `NORMAL` returns lifecycle to `STABLE`.

## Observable Evidence

- `STATUS_SNAPSHOT ... layout=4 contract=OK`
- `FAULTMG_EVENT ... severity=... recovery_policy=... output_policy=...`
- `FAULTMG_HISTORY ... event=ERROR/CLEAR/RECOVERY_TICK`
- `SCHED_TARGET ... gear=... ambient=... hazard=... drive_mode=...`
- `make test-fault`
- `make test-serial-e2e`
- `make test-integration-fault`

## Current Limits

- Validation is QEMU-based.
- Real-board GPIO electrical behavior is not claimed as completed.
- Fault thresholds are project policy constants, not certification-derived safety limits.
