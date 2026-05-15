# Test Plan

This test plan maps requirements to runnable validation commands. Host-side tests validate pure logic quickly; QEMU tests validate Microkit wiring, notifications, and serial behavior.

## Test Commands

| Command | Scope | Expected result |
| --- | --- | --- |
| `make test` | All host-side unit tests. | Every test binary builds and prints its pass message. |
| `make qemu-test` | Smoke, fault injection, and serial E2E tests. | QEMU boots, expected logs are observed, and scripts exit 0. |
| `make build` | Full Microkit image build. | `build/loader.img` and `build/report.txt` are produced. |
| `make smoke` | Minimal QEMU boot and command propagation. | Core domains initialize and basic light commands propagate. |
| `make test-integration-fault` | QEMU fault mode integration with test hooks. | Fault mode changes immediately affect target output and GPIO logs. |
| `make test-serial-e2e` | Serial transport, status query, clear, and recovery tick flow. | `STATUS_SNAPSHOT` reflects lifecycle and recovery progress. |

## Requirement Matrix

| Requirement | Validation command | Evidence to inspect |
| --- | --- | --- |
| `REQ-LIGHT-001` | `make smoke`, `make test-control`, `make test-execution` | Command logs, scheduler target output, lightctl GPIO actions. |
| `REQ-LIGHT-002` | `make test-control`, `make test-runtime` | High-beam policy and runtime guard assertions. |
| `REQ-LIGHT-003` | `make test-control` | Opposite turn request is cleared by command logic. |
| `REQ-LIGHT-004` | `make test-vehicle`, `make test-control` | Vehicle state update result and recomputed target output. |
| `REQ-LIGHT-005` | `make test-snapshot`, `make test-serial-e2e` | Snapshot formatting and serial `STATUS_SNAPSHOT` logs. |
| `REQ-FAULT-001` | `make test-fault` | Single recognized fault enters `WARN` and `ACTIVE`. |
| `REQ-FAULT-002` | `make test-fault`, `make test-integration-fault` | Three mode conflicts enter `DEGRADED`. |
| `REQ-FAULT-003` | `make test-fault`, `make test-serial-e2e` | Two hardware-state errors enter `SAFE_MODE`. |
| `REQ-FAULT-004` | `make test-fault`, `make test-integration-fault` | `DEGRADED` disables high beam and enforces low beam. |
| `REQ-FAULT-005` | `make test-fault`, `make test-integration-fault` | `SAFE_MODE` conservative output profile appears in logs. |
| `REQ-RECOVERY-001` | `make test-fault`, `make test-serial-e2e` | Clear enters `RECOVERING` without direct return to `NORMAL`. |
| `REQ-RECOVERY-002` | `make test-fault`, `make test-serial-e2e` | Recovery ticks step down one level at a time. |
| `REQ-RECOVERY-003` | `make test-fault` | New fault during recovery resets progress and returns to `ACTIVE`. |
| `REQ-ENG-001` | Static grep for local channel defines | No component-local channel ID definitions remain. |
| `REQ-ENG-002` | `make test` | All host-side tests run from one command. |
| `REQ-ENG-003` | `make qemu-test` | All QEMU validation scripts run from one command. |
| `REQ-ENG-004` | Review `docs/` | Review materials are present and consistent. |

## Recommended Review Sequence

```bash
make help
make test
make build
make qemu-test
```

If `make qemu-test` fails because QEMU or the Microkit SDK is missing, run `make test` first and record the environment gap separately.
