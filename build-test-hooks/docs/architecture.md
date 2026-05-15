# LightDemo Architecture

This document describes the current seL4/Microkit architecture for the automotive lighting control demo. It is intended to support engineering review: component boundaries, channel contracts, shared memory ownership, and behavior that must remain unchanged during cleanup.

## System Shape

The system is split into small Microkit protection domains:

| Protection domain | Responsibility |
| --- | --- |
| `commandin` | UART input gateway. It parses serial input into `light_transport_message_t`, dispatches light commands, vehicle state updates, fault injection/clear requests, and local status queries. |
| `scheduler` | Rule and policy arbitration. It consumes operator requests, vehicle state, and current fault mode, then computes `target_output` and `allow_flags`. |
| `lightctl` | Execution coordination. It diffs current execution state against `target_output`, runs runtime guard checks, notifies GPIO channels, and reports rejected high-risk actions to `faultmg`. |
| `gpio` | Pin-level output and timer/MMIO interaction. It observes the current fault mode and performs final hardware-facing light output actions. |
| `fault_mg` | Fault lifecycle owner. It records fault events, owns fault mode transitions, publishes lifecycle state, and notifies GPIO and scheduler about fault mode updates. |
| `vehicle_state` | Vehicle state update consumer. It writes speed, ignition, and brake pedal state into the shared state used by scheduler. |

## Message Flow

Normal lighting command path:

```text
UART -> commandin -> scheduler -> lightctl -> gpio
```

Vehicle state update path:

```text
UART -> commandin -> vehicle_state -> scheduler -> lightctl -> gpio
```

Fault path:

```text
lightctl -> fault_mg -> scheduler -> lightctl
lightctl -> fault_mg -> gpio
commandin -> fault_mg -> scheduler -> lightctl
commandin -> fault_mg -> gpio
```

`fault_mg` does not directly notify `lightctl` during normal mode publication. It notifies `scheduler` and `gpio`; `scheduler` recomputes `target_output` and then synchronizes `lightctl`.

Status query path:

```text
UART -> commandin -> shared memory snapshot -> UART
```

## Channel Map

The channel IDs are declared in `light.system` and centralized for C code in `include/light_channels.h`.

| Channel end A | ID | Channel end B | ID | Purpose |
| --- | ---: | --- | ---: | --- |
| `gpio` | 1 | `lightctl` | 2 | Reserved legacy GPIO/lightctl link. |
| `commandin` | 3 | `scheduler` | 4 | Light command notification. |
| `fault_mg` | 5 | `lightctl` | 6 | Fault report path from lightctl to faultmg. |
| `fault_mg` | 7 | `gpio` | 8 | Fault mode update notification to GPIO. |
| `scheduler` | 9 | `lightctl` | 10 | Scheduler target-output synchronization. |
| `commandin` | 11 | `fault_mg` | 12 | Fault injection and fault clear transport path. |
| `scheduler` | 13 | `fault_mg` | 14 | Fault mode update notification to scheduler. |
| `scheduler` | 15 | `vehicle_state` | 16 | Vehicle state update notification to scheduler. |
| `commandin` | 17 | `vehicle_state` | 18 | Vehicle state transport path. |
| `lightctl` | 20-31 | `gpio` | 20-31 | Concrete GPIO actions such as low beam on/off, turn signals, brake, high beam, and position light. |

GPIO action IDs:

| ID | Action |
| ---: | --- |
| 20 | Turn left on |
| 21 | Turn left off |
| 22 | Turn right on |
| 23 | Turn right off |
| 24 | Brake on |
| 25 | Brake off |
| 26 | Low beam on |
| 27 | Low beam off |
| 28 | High beam on |
| 29 | High beam off |
| 30 | Position light on |
| 31 | Position light off |

## Shared Memory Layout

The primary shared state is `light_shmem_t` in `include/light_protocol.h`. Its layout is versioned by `LIGHT_SHARED_STATE_LAYOUT_V3` so that future changes can be detected explicitly.

Current shared fields include:

| Field group | Producer | Consumers | Purpose |
| --- | --- | --- | --- |
| `layout_version` | `scheduler` | all readers | Shared memory compatibility guard. |
| `uart_cmd` | `scheduler` | diagnostics | Last accepted light command. |
| `operator_request` | `scheduler` | `scheduler`, snapshot | Latched operator lighting request. |
| `vehicle_state` | `vehicle_state` | `scheduler`, snapshot | Speed, ignition, brake pedal. |
| `fault_mode`, lifecycle fields | `fault_mg` | `scheduler`, `lightctl`, `commandin`, diagnostics | Current fault state and recovery progress. |
| `target_output` | `scheduler` | `lightctl`, snapshot | Desired effective light output after policy arbitration. |
| `allow_flags` | `scheduler` | `lightctl`, diagnostics | Bitset representation of target output. |

The separate `fault_mode_shared` memory region is a compact mode slot used by `fault_mg` to publish mode updates to domains that need a low-overhead fault-mode observation path.

## Fault Mode Ownership

`fault_mg` is the only owner of fault mode because fault mode is global safety state. Allowing several domains to mutate it would create race-prone and hard-to-review behavior, especially when a clear request and a new fault event happen close together.

The ownership rule is:

- Other domains may report fault events.
- Other domains may request clear/recovery observation through transport.
- Only `fault_mg` mutates `fault_mode`, `fault_lifecycle`, `active_fault_mask`, `recovery_ticks`, and fault counters.
- `fault_mg` publishes the resulting state, notifies `gpio` and `scheduler`, and relies on scheduler synchronization for downstream `lightctl` updates.

This keeps escalation, recovery, and anti-flap behavior explainable and testable in `light_fault_mode.c`.

## Behavior Preserved During Cleanup

Documentation and source-comment cleanup must not change the Microkit communication semantics:

- Protection domain names remain the same.
- Channel IDs remain the same.
- Shared memory region names, virtual addresses, and `setvar_vaddr` bindings remain the same.
- The `light_transport_message_t` wire shape remains the same.
- `light_shmem_t` layout version remains explicit and is only changed with a coordinated compatibility update.
- Fault mode transitions remain owned by `fault_mg`.
- GPIO action channels remain one notification per concrete action.

When refactoring code, keep these contracts stable unless the task explicitly asks for a communication refactor.
