# Release Baseline v1.0

This document freezes the current accepted LightDemo engineering baseline for
review, submission, and future comparison.

## Baseline Identity

| Item | Value |
| --- | --- |
| Release name | LightDemo Engineering Baseline v1.0 |
| Accepted run id | `report-v6` |
| Evidence directory | `test-results/report-v6/` |
| Host OS | Ubuntu 22.04 VM |
| Microkit SDK | 2.0.1 |
| Board | `qemu_virt_aarch64` |
| Config | `debug` |
| Shared memory layout | `layout=3` |

The v1.0 baseline is not a new lighting feature release. It is the first
engineering-grade baseline that combines architecture boundaries, runtime
contracts, fault lifecycle diagnostics, and preserved validation evidence.

## Accepted Validation Evidence

The accepted validation commands are:

```bash
make test TEST_RUN_ID=report-v6
make build
make qemu-test TEST_RUN_ID=report-v6
make evidence TEST_RUN_ID=report-v6
```

Evidence files:

| Evidence | File |
| --- | --- |
| Host-side summary | `test-results/report-v6/host-summary.txt` |
| QEMU summary | `test-results/report-v6/qemu-summary.txt` |
| Smoke log | `test-results/report-v6/smoke.make.log` |
| Fault integration log | `test-results/report-v6/test-integration-fault.make.log` |
| Serial E2E log | `test-results/report-v6/test-serial-e2e.make.log` |
| Evidence manifest | `test-results/report-v6/manifest.txt` |

Acceptance result:

- Host-side tests: PASS
- QEMU smoke: PASS
- QEMU fault integration: PASS
- QEMU serial E2E: PASS
- Evidence manifest: generated

Key serial evidence:

```text
STATUS_SNAPSHOT fault=SAFE_MODE lifecycle=ACTIVE ... layout=3 contract=OK
STATUS_SNAPSHOT fault=SAFE_MODE lifecycle=RECOVERING ... layout=3 contract=OK
STATUS_SNAPSHOT fault=DEGRADED lifecycle=RECOVERING ... layout=3 contract=OK
STATUS_SNAPSHOT ... last_fault_name=HW_STATE_ERR ... layout=3 contract=OK
FAULTMG_HISTORY ... code_name=HW_STATE_ERR ... lifecycle=ACTIVE
FAULTMG_HISTORY ... event=CLEAR ... lifecycle=RECOVERING
FAULTMG_HISTORY ... event=RECOVERY_TICK ... lifecycle=RECOVERING
```

The copied `report-v6` evidence contains no accepted `FAIL` or `VMFault`
result for the final baseline.

## Engineering Capabilities Frozen in v1.0

- Three-layer control path: input access, policy decision, execution/hardware.
- Explicit contract checks for shared memory, transport messages, channel ids,
  and fault snapshots.
- Fault Lifecycle v2 with `STABLE`, `ACTIVE`, and `RECOVERING` lifecycle states.
- Fault modes `NORMAL`, `WARN`, `DEGRADED`, and `SAFE_MODE` with centralized
  ownership in `faultmg`.
- Structured evidence logs, including `STATUS_SNAPSHOT`, `FAULTMG_HISTORY`, and
  `*_CONTRACT_REJECT reason=...`.
- Host-side unit tests plus QEMU integration tests.
- Evidence archiving through `test-results/<run-id>/` and `make evidence`.

## Current Boundary

The v1.0 baseline intentionally keeps the project boundary clear:

- Validation is complete on Ubuntu 22.04 VM + QEMU, not on a real board.
- GPIO behavior is verified through QEMU logs and simulated MMIO behavior, not
  through real electrical measurement.
- Recovery uses observation ticks. It is not yet a real-time recovery window.
- The fault taxonomy is project-scale and reviewable, but not a full automotive
  fault catalog.
- The project is an engineering-practice baseline, not a formal functional
  safety certification artifact.
- Shared memory remains `layout=3`; this baseline does not introduce a V4
  compatibility break.
- `test-results/report-v6/` is release evidence. `build/` and
  `build-test-hooks/` are generated or copied build artifacts, not source of
  truth for the design.

## Release Checklist

Before using this baseline for submission or review:

- Confirm README and validation documents reference `report-v6`.
- Confirm `test-results/report-v6/manifest.txt` exists.
- Confirm host and QEMU summaries contain only accepted PASS results.
- Confirm serial logs contain `STATUS_SNAPSHOT`, `FAULTMG_HISTORY`,
  `last_fault_name=HW_STATE_ERR`, and `layout=3 contract=OK`.
- Confirm any generated binaries are treated as artifacts, while source and
  `.md` documents remain the reviewable engineering baseline.

## Next Versions

The baseline is the starting point for the roadmap in
`docs/engineering_roadmap.md`:

- `v1.1`: real-time recovery window.
- `v1.2`: fuller fault taxonomy.
- `v1.3`: CI evidence retention.
- `v2.0`: real-board GPIO validation or richer vehicle-state modeling.
