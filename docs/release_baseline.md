# Release Baseline v2.0

This document freezes the accepted final LightDemo engineering baseline for review, defense, and future comparison.

## Baseline Identity

| Item | Value |
| --- | --- |
| Release name | LightDemo Engineering Final v2.0 |
| Accepted run id | `final-v2.0` |
| Evidence directory | `test-results/final-v2.0/` |
| Host OS | Ubuntu 22.04 VM |
| Microkit SDK | 2.0.1 |
| Board | `qemu_virt_aarch64` |
| Config | `debug` |
| Shared memory layout | `layout=4` |

v2.0 is the final course-defense baseline. It extends the previous v1.0 engineering baseline with elapsed-time recovery, fault taxonomy metadata, archived evidence reports, a richer vehicle-state model, and a clearer repository structure.

## Accepted Validation Commands

```bash
make test TEST_RUN_ID=final-v2.0
make build MICROKIT_SDK=../microkit-sdk-2.0.1
make qemu-test TEST_RUN_ID=final-v2.0 MICROKIT_SDK=../microkit-sdk-2.0.1
make evidence TEST_RUN_ID=final-v2.0
make defense-report TEST_RUN_ID=final-v2.0
```

## Accepted Evidence

| Evidence | File |
| --- | --- |
| Host summary | `test-results/final-v2.0/host-summary.txt` |
| QEMU summary | `test-results/final-v2.0/qemu-summary.txt` |
| Smoke log | `test-results/final-v2.0/smoke.make.log` |
| Fault integration log | `test-results/final-v2.0/test-integration-fault.make.log` |
| Serial E2E log | `test-results/final-v2.0/test-serial-e2e.make.log` |
| Manifest | `test-results/final-v2.0/manifest.txt` |
| Evidence summary | `test-results/final-v2.0/summary.md` |
| Defense report | `reports/defense_report_final-v2.0.md` |

Acceptance result:

- Host-side tests: PASS
- QEMU smoke: PASS
- QEMU fault integration: PASS
- QEMU serial E2E: PASS
- Evidence manifest: generated
- Defense report: generated

## Key Runtime Evidence

```text
STATUS_SNAPSHOT fault=SAFE_MODE lifecycle=ACTIVE ... recovery_elapsed_ms=0/2000 ... layout=4 contract=OK
STATUS_SNAPSHOT fault=SAFE_MODE lifecycle=RECOVERING ... recovery_elapsed_ms=0/2000 ... layout=4 contract=OK
STATUS_SNAPSHOT fault=DEGRADED lifecycle=RECOVERING ... recovery_elapsed_ms=0/2000 ... layout=4 contract=OK
FAULTMG_EVENT ... severity=SAFE_MODE_AFTER_2 recovery_policy=clear_then_elapsed_window
FAULTMG_HISTORY ... event=RECOVERY_TICK ... lifecycle=RECOVERING
SCHED_TARGET ... gear=3 ambient=0 hazard=0 drive_mode=0
```

## Capabilities Frozen in v2.0

- Structured repository layout under `src/domains`, `src/core`, `src/support`, and `systems`.
- Shared memory layout V4 with elapsed-time recovery fields and richer vehicle-state fields.
- Fault lifecycle owner remains centralized in `faultmg`.
- Fault taxonomy metadata exposes fault source, severity, recovery policy, and output policy.
- Vehicle-state model covers speed, ignition, brake pedal, gear, ambient light, hazard, and drive mode.
- Evidence generation creates `manifest.txt`, `summary.md`, and `defense_report.md`.
- Host tests and QEMU tests are preserved as the accepted regression gate.

## Current Boundary

- Validation is complete on Ubuntu 22.04 VM + QEMU, not on a real board.
- GPIO behavior is verified through QEMU logs and simulated MMIO behavior, not electrical measurement.
- The project is an engineering-practice and course-defense baseline, not a formal functional-safety certification artifact.
- Real-board GPIO validation is documented as a follow-up template in `docs/real_board_validation_template.md`.

## Release Checklist

- `test-results/final-v2.0/manifest.txt` reports `host_status=PASS` and `qemu_status=PASS`.
- Manifest token checks report `FOUND` for `STATUS_SNAPSHOT`, `FAULTMG_HISTORY`, `SAFE_MODE`, `RECOVERING`, `contract=OK`, `layout=4`, and `recovery_elapsed_ms`.
- `reports/defense_report_final-v2.0.md` exists and states Host/QEMU PASS.
- README and docs reference `final-v2.0` and `layout=4`.
- Generated `build/` and `build-test-hooks/` artifacts are treated as generated outputs, not design source.
