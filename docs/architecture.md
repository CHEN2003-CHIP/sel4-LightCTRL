# LightDemo Architecture

This document describes the final v2.0 seL4/Microkit architecture.

## Protection Domains

| Domain | Source | Responsibility |
| --- | --- | --- |
| `commandin` | `src/domains/commandin.c` | UART input gateway, transport dispatch, local status query |
| `scheduler` | `src/domains/scheduler.c` | Rule arbitration and target output computation |
| `lightctl` | `src/domains/lightctl.c` | Execution coordination and runtime guard integration |
| `gpio` | `src/domains/gpio.c` | Hardware-facing output behavior under QEMU |
| `fault_mg` | `src/domains/faultmg.c` | Fault lifecycle owner and fault mode publisher |
| `vehicle_state` | `src/domains/vehicle_state.c` | Vehicle-state update consumer |

Microkit system description: `systems/light.system`.

## Engineering Layers

| Layer | Domains | Boundary rule |
| --- | --- | --- |
| Input access | `commandin`, `vehicle_state` | Normalize external input; do not own final light policy or global fault state |
| Policy decision | `scheduler`, `fault_mg` | Own target-output arbitration and fault lifecycle |
| Execution/hardware | `lightctl`, `gpio` | Convert target output into guarded GPIO actions |

## Message Flow

```text
UART -> commandin -> scheduler -> lightctl -> gpio
UART -> commandin -> vehicle_state -> scheduler -> lightctl -> gpio
fault reports/test injection -> faultmg -> scheduler -> lightctl
faultmg -> gpio
status query -> commandin -> shared memory snapshot -> UART
```

## Shared Memory Layout

The final shared state uses `LIGHT_SHARED_STATE_LAYOUT_V4`.

V4 adds:

- elapsed-time recovery fields: `fault_recovery_elapsed_ms`, `fault_recovery_window_ms`
- v2.0 vehicle-state fields: `gear`, `ambient_light`, `hazard`, `drive_mode`

The contract layer validates:

- layout version
- transport version/type/length
- fault mode/lifecycle/recovery bounds
- active fault mask
- known Microkit channel ids

Runtime evidence:

```text
SCHED_INIT module=scheduler status=ready layout=4
STATUS_SNAPSHOT ... layout=4 contract=OK
```

## Vehicle-State Model

| Field | Meaning | Example command |
| --- | --- | --- |
| `speed_kph` | Vehicle speed | `speed=80` |
| `ignition_on` | Ignition state | `ignition=1` |
| `brake_pedal` | Brake pedal state | `brake=1` |
| `gear` | Park/reverse/neutral/drive | `gear=reverse` |
| `ambient_light` | Day/dusk/night | `ambient=night` |
| `hazard` | Hazard warning request | `hazard=1` |
| `drive_mode` | City/highway/parking/emergency | `mode=emergency` |

## Channel Map

Channel IDs remain centralized in `include/light_channels.h` and synchronized with `systems/light.system`. Existing Microkit communication semantics are preserved across the v2.0 source reorganization.

## Source Layout

```text
src/domains/   Microkit PD entrypoints
src/core/      host-testable logic and contracts
src/support/   printf/util support code
include/       public interfaces
systems/       Microkit system descriptions
tests/         host-side tests
scripts/       QEMU and evidence scripts
reports/       defense-facing summaries
```

## Accepted Evidence

Final architecture evidence is archived under `test-results/final-v2.0/` and summarized by `reports/final_v2_showcase.md`.
