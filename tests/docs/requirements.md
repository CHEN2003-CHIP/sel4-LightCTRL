# Requirements

This document records the minimum engineering requirements for the current LightDemo scope. The goal is traceability: each requirement should map to implementation and validation evidence.

## Functional Requirements

| ID | Requirement | Primary implementation evidence |
| --- | --- | --- |
| `REQ-LIGHT-001` | A valid low-beam command shall be accepted from UART transport and propagated to GPIO output. | `commandin`, `scheduler`, `lightctl`, `gpio` |
| `REQ-LIGHT-002` | High beam shall require a valid low-beam request and shall be blocked when vehicle speed is below 10 km/h. | `light_control_logic.c`, `light_runtime_guard.c` |
| `REQ-LIGHT-003` | Left and right turn requests shall be mutually exclusive at the operator request level. | `light_control_logic.c` |
| `REQ-LIGHT-004` | Vehicle state updates shall affect target output arbitration without bypassing scheduler. | `vehicle_state.c`, `scheduler.c` |
| `REQ-LIGHT-005` | Status query shall expose current fault mode, lifecycle, recovery progress, vehicle state, target output, and fault counters. | `commandin.c`, `light_status_snapshot.c` |

## Fault and Safety Requirements

| ID | Requirement | Primary implementation evidence |
| --- | --- | --- |
| `REQ-FAULT-001` | Any recognized fault shall move the lifecycle into `ACTIVE` and at least `WARN` mode. | `light_fault_mode.c` |
| `REQ-FAULT-002` | Three consecutive `LIGHT_ERR_MODE_CONFLICT` events shall enter `DEGRADED`. | `light_fault_mode.c` |
| `REQ-FAULT-003` | Two `LIGHT_ERR_HW_STATE_ERR` events shall enter `SAFE_MODE`. | `light_fault_mode.c` |
| `REQ-FAULT-004` | `DEGRADED` mode shall disable high beam and enforce minimum forward illumination. | `light_output_policy.c` |
| `REQ-FAULT-005` | `SAFE_MODE` shall force a conservative profile: low beam and position on, high beam and turn outputs off, brake preserved. | `light_output_policy.c` |
| `REQ-RECOVERY-001` | A clear request shall not immediately return a non-normal fault mode to `NORMAL`. | `light_fault_mode.c` |
| `REQ-RECOVERY-002` | Recovery shall step down one fault mode level per satisfied observation window. | `light_fault_mode.c` |
| `REQ-RECOVERY-003` | A new fault during recovery shall interrupt recovery and reset recovery progress. | `light_fault_mode.c` |

## Engineering Requirements

| ID | Requirement | Primary implementation evidence |
| --- | --- | --- |
| `REQ-ENG-001` | Microkit channel endpoint IDs shall be centralized in a shared header and kept synchronized with `light.system`. | `include/light_channels.h`, `light.system` |
| `REQ-ENG-002` | Host-side logic tests shall be runnable through one command. | `make test` |
| `REQ-ENG-003` | QEMU smoke, fault injection, and serial E2E checks shall be runnable through one command. | `make qemu-test` |
| `REQ-ENG-004` | Architecture, safety, requirements, test plan, and demo flow shall be documented for engineering review. | `docs/` |
| `REQ-ENG-005` | Shared-memory, transport, fault snapshot, and channel contracts shall be validated explicitly. | `light_contract.c`, `make test-contract` |
| `REQ-ENG-006` | Runtime logs shall expose contract and snapshot evidence suitable for QEMU regression checks. | `SCHED_CONTRACT`, `LIGHTCTL_CONTRACT`, `FAULTMG_CONTRACT`, `STATUS_SNAPSHOT` |

## Out of Current Scope

- Real automotive certification claims.
- Real-time clock based recovery windows.
- Hardware-board-specific GPIO validation beyond `qemu_virt_aarch64`.
- New user-facing lighting features beyond the current command set.
