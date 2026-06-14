# Validation Report

This document records the accepted validation evidence for **LightDemo Engineering Final v2.0**.

## Environment

| Item | Value |
| --- | --- |
| Host OS | Ubuntu 22.04 VM |
| Microkit SDK | 2.0.1 |
| Board | `qemu_virt_aarch64` |
| Config | `debug` |
| Evidence directory | `test-results/final-v2.0/` |
| Shared layout | `layout=4` |

## Commands

```bash
make test-vehicle-sweep TEST_RUN_ID=final-v2.0
make test TEST_RUN_ID=final-v2.0
make build MICROKIT_SDK=../microkit-sdk-2.0.1
make qemu-test TEST_RUN_ID=final-v2.0 MICROKIT_SDK=../microkit-sdk-2.0.1
make evidence TEST_RUN_ID=final-v2.0
make defense-report TEST_RUN_ID=final-v2.0
```

## Summary

| Validation group | Result | Evidence |
| --- | --- | --- |
| Host-side unit and scenario tests | PASS | `test-results/final-v2.0/host-summary.txt` |
| Extended vehicle/fault sweep | CSV archived | `test-results/final-v2.0/vehicle-sweep.csv` |
| QEMU smoke test | PASS | `test-results/final-v2.0/smoke.make.log` |
| QEMU fault integration | PASS | `test-results/final-v2.0/test-integration-fault.make.log` |
| QEMU serial E2E | PASS | `test-results/final-v2.0/test-serial-e2e.make.log` |
| Evidence manifest | PASS | `test-results/final-v2.0/manifest.txt` |
| Defense report | PASS | `reports/defense_report_final-v2.0.md` |

## Host Test Results

All host-side tests passed:

- `test-policy`
- `test-protocol`
- `test-contract`
- `test-command`
- `test-transport`
- `test-snapshot`
- `test-control`
- `test-vehicle`
- `test-execution`
- `test-runtime`
- `test-fault`
- `test-fault-transport`
- `test-vehicle-scenarios`
- `test-vehicle-sweep`

These tests cover policy logic, protocol compatibility, interface contracts, transport parsing, status snapshot formatting, vehicle-state updates, execution planning, runtime guards, fault lifecycle behavior, fault taxonomy metadata, elapsed-time recovery, and v2.0 vehicle scenarios.

The extended sweep target adds 46,080 deterministic rows across request profiles, speed points, ignition, brake pedal, gear, ambient light, hazard, drive mode, and fault mode.

Evidence note: the copied CSV contains the complete 46,080-row dataset. During review, a stale `test-vehicle-sweep.log` showed the old expected count; the source test has been corrected to expect 46,080 rows and the Makefile now uses `bash -o pipefail` so future pipeline failures cannot be hidden by `tee`.

## QEMU Test Results

All QEMU tests passed:

- `smoke`
- `test-integration-fault`
- `test-serial-e2e`

The QEMU evidence confirms that Microkit protection domains boot, normal commands propagate through the full control chain, fault injection triggers fault-mode re-arbitration, and serial status/recovery behavior remains observable.

## Key Runtime Evidence

Serial E2E evidence:

```text
STATUS_SNAPSHOT fault=SAFE_MODE lifecycle=ACTIVE recovery_ticks=0/2 recovery_elapsed_ms=0/2000 ... layout=4 contract=OK
STATUS_SNAPSHOT fault=SAFE_MODE lifecycle=RECOVERING recovery_ticks=0/2 recovery_elapsed_ms=0/2000 ... layout=4 contract=OK
STATUS_SNAPSHOT fault=DEGRADED lifecycle=RECOVERING recovery_ticks=0/2 recovery_elapsed_ms=0/2000 ... layout=4 contract=OK
```

Fault taxonomy evidence:

```text
FAULTMG_EVENT source=commandin code=0x04 name=HW_STATE_ERR severity=SAFE_MODE_AFTER_2 recovery_policy=clear_then_elapsed_window output_policy=conservative_low_beam_position_only
FAULTMG_EVENT source=commandin code=0x02 name=MODE_CONFLICT severity=DEGRADED_AFTER_3_CONSECUTIVE recovery_policy=clear_then_elapsed_window output_policy=disable_high_beam_minimum_illumination
```

Vehicle model evidence:

```text
SCHED_TARGET mode=NORMAL speed=10 ignition=1 brake_pedal=0 gear=3 ambient=0 hazard=0 drive_mode=0
VEHICLE_STATE_INIT speed=10 brake=0 ignition=1 gear=3 ambient=0 hazard=0 drive_mode=0
```

## Regression Closed

The previous `commandin` VMFault regression on status query remains closed. In `final-v2.0`, `test-serial-e2e` passes and serial logs contain `STATUS_SNAPSHOT ... layout=4 contract=OK`.

## Remaining Risk

- Validation is QEMU-based, not real-board GPIO electrical validation.
- Fault severity thresholds are project policy constants, not certification artifacts.
- Current evidence is appropriate for engineering-practice review and course defense, not formal automotive certification.
