# Test Plan

This test plan defines the accepted validation flow for **LightDemo Engineering Final v2.0**.

## Standard Commands

| Command | Scope | Expected result |
| --- | --- | --- |
| `make test TEST_RUN_ID=final-v2.0` | All host-side logic and scenario tests | All targets PASS |
| `make test-vehicle-sweep TEST_RUN_ID=final-v2.0` | Extended vehicle/fault scenario sweep | 46,080 CSV rows generated |
| `make build MICROKIT_SDK=../microkit-sdk-2.0.1` | Full Microkit image build | `build/loader.img` is produced |
| `make qemu-test TEST_RUN_ID=final-v2.0 MICROKIT_SDK=../microkit-sdk-2.0.1` | Smoke, fault integration, serial E2E | All QEMU checks PASS |
| `make evidence TEST_RUN_ID=final-v2.0` | Evidence manifest and summary | `manifest.txt` and `summary.md` generated |
| `make defense-report TEST_RUN_ID=final-v2.0` | Defense report | `reports/defense_report_final-v2.0.md` generated |

## Host-Side Tests

| Target | Coverage |
| --- | --- |
| `test-policy` | Legacy policy compatibility |
| `test-protocol` | Shared protocol helpers and default state |
| `test-contract` | Layout V4, transport, fault snapshot, channel contract |
| `test-command` | Light commands and vehicle-state command parsing |
| `test-transport` | UART transport parser and route selection |
| `test-snapshot` | `STATUS_SNAPSHOT` formatting with layout 4 |
| `test-control` | Scheduler target-output policy |
| `test-vehicle` | Vehicle-state field validation |
| `test-vehicle-scenarios` | v2.0 night, emergency, parking, and fault-overlay scenarios |
| `test-vehicle-sweep` | 46,080-row vehicle/fault combination sweep with CSV evidence |
| `test-execution` | Lightctl execution diff |
| `test-runtime` | Runtime guard behavior |
| `test-fault` | Fault lifecycle, taxonomy, elapsed-time recovery |
| `test-fault-transport` | Fault mode transport encoding |

## QEMU Tests

| Target | Coverage |
| --- | --- |
| `smoke` | Protection-domain boot and normal command propagation |
| `test-integration-fault` | Fault injection, mode escalation, scheduler re-arbitration, GPIO output |
| `test-serial-e2e` | Serial fault injection, status query, clear, elapsed-time recovery evidence |

## Requirement Matrix

| Requirement | Validation |
| --- | --- |
| Valid light command reaches GPIO | `make smoke`, `test-control`, `test-execution` |
| Runtime contracts remain explicit | `test-contract`, QEMU `contract=OK` token |
| Fault mode escalation works | `test-fault`, `test-integration-fault`, `test-serial-e2e` |
| Elapsed-time recovery works | `test-fault`, QEMU `recovery_elapsed_ms` token |
| Fault taxonomy is visible | `test-fault`, QEMU `FAULTMG_EVENT ... severity=...` |
| Vehicle-state model affects policy | `test-vehicle`, `test-vehicle-scenarios`, `test-vehicle-sweep`, QEMU `SCHED_TARGET ... gear=...` |
| Evidence is archived | `make evidence`, `make defense-report` |

## Accepted Run

The accepted final run is:

```text
run_id=final-v2.0
host_status=PASS
qemu_status=PASS
shared_layout=4
```

See `docs/validation_report.md` and `test-results/final-v2.0/manifest.txt` for final evidence.
