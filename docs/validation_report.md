# Validation Report

This document records the latest validation evidence for the engineering-grade
LightDemo baseline. The accepted release name is **LightDemo Engineering
Baseline v1.0**.

## Environment

| Item | Value |
| --- | --- |
| Host OS | Ubuntu 22.04 VM |
| Microkit SDK | 2.0.1 |
| Board | `qemu_virt_aarch64` |
| Config | `debug` |
| Evidence directory | `test-results/report-v6/` |

`report-v6` is the accepted VM validation baseline for Fault Lifecycle v2 and
the evidence-manifest flow. The release boundary and roadmap are recorded in
`docs/release_baseline.md` and `docs/engineering_roadmap.md`.

## Commands

```bash
make test TEST_RUN_ID=report-v6
make build
make qemu-test TEST_RUN_ID=report-v6
make evidence TEST_RUN_ID=report-v6
```

## Summary

| Validation group | Result | Evidence |
| --- | --- | --- |
| Host-side unit tests | PASS | `test-results/report-v6/host-summary.txt` |
| QEMU smoke test | PASS | `test-results/report-v6/smoke.make.log` |
| QEMU fault integration | PASS | `test-results/report-v6/test-integration-fault.make.log` |
| QEMU serial E2E | PASS | `test-results/report-v6/test-serial-e2e.make.log` |
| Evidence manifest | PASS | `test-results/report-v6/manifest.txt` |

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

These tests cover policy logic, protocol compatibility, interface contracts,
transport parsing, snapshot formatting, vehicle-state updates, execution
planning, runtime guards, and fault lifecycle behavior.

## QEMU Test Results

All QEMU tests passed:

- `smoke`
- `test-integration-fault`
- `test-serial-e2e`

The QEMU evidence confirms that the Microkit protection domains boot, normal
commands propagate through the full control chain, fault injection triggers
fault-mode re-arbitration, and serial status/recovery behavior is observable.

## Key Runtime Evidence

The serial E2E log contains the expected status snapshots:

```text
STATUS_SNAPSHOT fault=SAFE_MODE lifecycle=ACTIVE ... layout=3 contract=OK
STATUS_SNAPSHOT fault=SAFE_MODE lifecycle=RECOVERING ... layout=3 contract=OK
STATUS_SNAPSHOT fault=DEGRADED lifecycle=RECOVERING ... layout=3 contract=OK
```

The serial E2E log also preserves Fault Lifecycle v2 evidence:

```text
STATUS_SNAPSHOT ... last_fault_name=HW_STATE_ERR ... layout=3 contract=OK
FAULTMG_HISTORY ... code_name=HW_STATE_ERR ... lifecycle=ACTIVE
FAULTMG_HISTORY ... event=CLEAR ... lifecycle=RECOVERING
FAULTMG_HISTORY ... event=RECOVERY_TICK ... lifecycle=RECOVERING
```

This proves that:

- `SAFE_MODE` is reached after repeated hardware-state errors.
- Clear does not immediately return the system to `NORMAL`.
- Recovery steps down through the observation window.
- Runtime contract status is visible in serial evidence.
- Fault lifecycle v2 adds recent-history evidence without changing
  shared-memory layout version.

## Regression Closed

During validation, the serial query path previously exposed a `commandin`
VMFault after `?`. The implementation was corrected by keeping the Microkit
runtime query path low-risk: `commandin` now emits `STATUS_SNAPSHOT` by reading
shared memory fields directly and appending a lightweight contract result.

`report-v6` confirms the regression is closed because `test-serial-e2e` passes
and the expected `STATUS_SNAPSHOT ... contract=OK` lines are present.

## Remaining Risk

- Validation is QEMU-based, not real-board GPIO validation.
- Recovery uses observation ticks rather than a real-time source.
- Fault severity thresholds are project policy constants, not certification
  artifacts.
- Current evidence is appropriate for engineering-practice review, not formal
  automotive certification.
