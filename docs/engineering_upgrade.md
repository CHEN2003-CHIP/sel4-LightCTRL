# Engineering Upgrade Summary

This document summarizes the final engineering upgrade delivered in **LightDemo Engineering Final v2.0**.

## Engineering Positioning

LightDemo is now presented as a small seL4/Microkit automotive lighting controller with:

- separated source layout
- explicit runtime contracts
- centralized fault lifecycle ownership
- elapsed-time recovery evidence
- fault taxonomy metadata
- v2.0 vehicle-state model
- archived validation and defense reports

## Final Source Structure

| Area | Path | Purpose |
| --- | --- | --- |
| Microkit domains | `src/domains/` | Protection-domain entrypoints |
| Core logic | `src/core/` | Host-testable protocol, policy, contract, fault, vehicle logic |
| Support code | `src/support/` | Shared support implementation |
| Interfaces | `include/` | Public headers |
| System description | `systems/light.system` | Microkit configuration |
| Tests | `tests/` | Host-side unit and scenario tests |
| Evidence scripts | `scripts/` | QEMU tests and evidence generation |
| Reports | `reports/` | Defense-facing summaries |

## Interface Contracts

The contract layer validates:

| Contract | Evidence |
| --- | --- |
| Shared layout V4 | `LIGHT_SHARED_STATE_LAYOUT_V4`, `STATUS_SNAPSHOT ... layout=4 contract=OK` |
| Transport message shape | `make test-contract` |
| Fault snapshot bounds | `make test-contract` |
| Channel ID table | `make test-contract` |

## Fault Management Upgrade

Fault handling is centralized in `faultmg`. v2.0 adds:

- elapsed-time recovery window evidence
- fault taxonomy fields
- structured `FAULTMG_EVENT`
- preserved `FAULTMG_HISTORY`

Accepted evidence:

```text
FAULTMG_EVENT ... severity=SAFE_MODE_AFTER_2 recovery_policy=clear_then_elapsed_window
FAULTMG_RECOVERY_TICK ... recovery_elapsed_ms=1000 recovery_window_ms=2000
STATUS_SNAPSHOT ... layout=4 contract=OK
```

## Vehicle-State Model Upgrade

v2.0 expands vehicle state from basic speed/ignition/brake into:

```text
speed, ignition, brake_pedal, gear, ambient_light, hazard, drive_mode
```

This allows reviewable scenarios such as night driving, emergency hazard behavior, parking behavior, reverse high-beam blocking, and fault overlay in safe mode.

## Validation Model

Final accepted commands:

```bash
make test TEST_RUN_ID=final-v2.0
make build MICROKIT_SDK=../microkit-sdk-2.0.1
make qemu-test TEST_RUN_ID=final-v2.0 MICROKIT_SDK=../microkit-sdk-2.0.1
make evidence TEST_RUN_ID=final-v2.0
make defense-report TEST_RUN_ID=final-v2.0
```

Final accepted results:

| Group | Result |
| --- | --- |
| Host tests | PASS |
| QEMU smoke | PASS |
| QEMU fault integration | PASS |
| QEMU serial E2E | PASS |
| Evidence manifest | PASS |

## Presentation Highlights

Use `reports/final_v2_showcase.md` as the first visual entry. Then show:

- `test-results/final-v2.0/manifest.txt`
- `test-results/final-v2.0/qemu-summary.txt`
- `test-results/final-v2.0/serial-e2e/qemu.log`
- `reports/defense_report_final-v2.0.md`

## Boundary

Real-board GPIO electrical validation remains future work. The project includes `docs/real_board_validation_template.md` to make that boundary and next step explicit.
