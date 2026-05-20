# Test Plan

This test plan maps requirements to runnable validation commands. Host-side tests validate pure logic quickly; QEMU tests validate Microkit wiring, notifications, and serial behavior.

## Test Commands

All aggregate test runs write logs under `test-results/<run-id>/` by default.
Set `TEST_RUN_ID` to choose a stable directory name, for example
`make qemu-test TEST_RUN_ID=vm-20260520`.

| Command | Scope | Expected result |
| --- | --- | --- |
| `make test` | All host-side unit tests. | Every test binary builds and prints its pass message. |
| `make test-contract` | Interface and compatibility contracts. | Shared-state layout, transport wire shape, fault snapshot bounds, and known channel table pass. |
| `make qemu-test` | Smoke, fault injection, and serial E2E tests. | QEMU boots, expected logs are observed, and scripts exit 0. |
| `make evidence` | Validation evidence manifest. | `test-results/<run-id>/manifest.txt` records run metadata and expected evidence tokens. |
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
| `REQ-LIGHT-005` | `make test-snapshot`, `make test-serial-e2e` | Snapshot formatting and serial `STATUS_SNAPSHOT` logs, including `last_fault_name`. |
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
| `REQ-ENG-005` | `make test-contract`, `make test-snapshot` | Contract checks and status snapshot contract field. |
| `REQ-ENG-006` | `make smoke`, `make test-serial-e2e` | Contract logs, `STATUS_SNAPSHOT contract=OK`, and `FAULTMG_HISTORY` evidence. |

## Recommended Review Sequence

```bash
make help
make test
make test-contract
make build
make qemu-test
make evidence
```

After a run, preserve these files for review:

- `test-results/<run-id>/host-summary.txt`
- `test-results/<run-id>/qemu-summary.txt`
- `test-results/<run-id>/*.log`
- `test-results/<run-id>/*/qemu.log`
- `test-results/<run-id>/manifest.txt`

Latest accepted evidence:

- Run id: `report-v6`
- Host summary: `test-results/report-v6/host-summary.txt`
- QEMU summary: `test-results/report-v6/qemu-summary.txt`
- Result: all host-side tests and all QEMU tests passed.
- Key serial evidence: `STATUS_SNAPSHOT ... layout=3 contract=OK`.
- Fault Lifecycle v2 evidence: `FAULTMG_HISTORY ... lifecycle=ACTIVE`,
  `FAULTMG_HISTORY ... lifecycle=RECOVERING`, and `STATUS_SNAPSHOT ...
  last_fault_name=HW_STATE_ERR`.

If `make qemu-test` fails because QEMU or the Microkit SDK is missing, run `make test` first and record the environment gap separately.
