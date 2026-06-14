# Requirements

This document records the final v2.0 requirements and their validation evidence.

## Functional Requirements

| ID | Requirement | Evidence |
| --- | --- | --- |
| `REQ-LIGHT-001` | Valid UART light commands shall propagate through `commandin -> scheduler -> lightctl -> gpio`. | `make smoke`, `test-results/final-v2.0/smoke/qemu.log` |
| `REQ-LIGHT-002` | High beam shall be constrained by low-beam request, braking state, gear, drive mode, and speed. | `make test-control`, `make test-vehicle-scenarios` |
| `REQ-LIGHT-003` | Turn requests shall remain mutually exclusive unless hazard/emergency state requests both outputs. | `make test-control`, `make test-vehicle-scenarios` |
| `REQ-LIGHT-004` | Vehicle-state updates shall affect target-output arbitration only through scheduler. | `make test-vehicle`, `make test-control` |
| `REQ-LIGHT-005` | Status query shall expose fault state, recovery progress, vehicle model, target output, layout, and contract status. | `make test-snapshot`, `make test-serial-e2e` |

## v2.0 Vehicle-State Requirements

| ID | Requirement | Evidence |
| --- | --- | --- |
| `REQ-VEH-001` | Vehicle state shall include speed, ignition, brake pedal, gear, ambient light, hazard, and drive mode. | `include/light_protocol.h`, `make test-vehicle` |
| `REQ-VEH-002` | Night/dusk ambient state shall enforce visible forward/position lighting. | `make test-vehicle-scenarios` |
| `REQ-VEH-003` | Emergency or hazard mode shall drive both turn outputs unless clamped by fault safe mode. | `make test-vehicle-scenarios` |
| `REQ-VEH-004` | Park/reverse contexts shall block high beam and preserve conservative lighting. | `make test-vehicle-scenarios` |

## Fault and Safety Requirements

| ID | Requirement | Evidence |
| --- | --- | --- |
| `REQ-FAULT-001` | Any recognized fault shall enter lifecycle `ACTIVE` and at least `WARN`. | `make test-fault` |
| `REQ-FAULT-002` | Three consecutive `LIGHT_ERR_MODE_CONFLICT` events shall enter `DEGRADED`. | `make test-fault`, `make test-integration-fault` |
| `REQ-FAULT-003` | Two `LIGHT_ERR_HW_STATE_ERR` events shall enter `SAFE_MODE`. | `make test-fault`, `make test-serial-e2e` |
| `REQ-FAULT-004` | Fault taxonomy shall expose source, severity, recovery policy, and output policy. | `make test-fault`, `FAULTMG_EVENT` logs |
| `REQ-RECOVERY-001` | Clear shall not immediately return a non-normal fault mode to `NORMAL`. | `make test-fault`, `make test-serial-e2e` |
| `REQ-RECOVERY-002` | Recovery shall use an elapsed-time window and step down one level at a time. | `make test-fault`, `recovery_elapsed_ms` logs |
| `REQ-RECOVERY-003` | New faults during recovery shall interrupt recovery and reset progress. | `make test-fault` |

## Engineering Requirements

| ID | Requirement | Evidence |
| --- | --- | --- |
| `REQ-ENG-001` | Source structure shall separate Microkit domains, host-testable core logic, support code, systems, docs, reports, and evidence. | `src/`, `systems/`, `reports/` |
| `REQ-ENG-002` | Shared memory layout shall be explicitly versioned and validated. | `LIGHT_SHARED_STATE_LAYOUT_V4`, `make test-contract` |
| `REQ-ENG-003` | Host-side tests shall run through one command. | `make test` |
| `REQ-ENG-004` | QEMU smoke/fault/serial checks shall run through one command. | `make qemu-test` |
| `REQ-ENG-005` | Evidence shall be archived as manifest, summary, logs, and defense report. | `make evidence`, `make defense-report` |

## Out of Scope

- Formal automotive certification claims.
- Real-board GPIO electrical validation as a completed result.
- Production vehicle integration.
