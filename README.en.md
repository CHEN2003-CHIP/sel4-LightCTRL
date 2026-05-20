# LightDemo

LightDemo is a seL4 Microkit demo project for an automotive light-control pipeline. The repository models the full path from UART command input to scheduling, light control, GPIO operations, and fault logging on the default `qemu_virt_aarch64` target.

The current baseline is intended to be reviewed as an engineering-grade
embedded project, not only as a tutorial demo. See
`docs/release_baseline.md` for the frozen v1.0 release baseline,
`docs/engineering_upgrade.md` for the contract model and presentation
highlights, `docs/validation_report.md` for accepted validation evidence, and
`docs/engineering_roadmap.md` for the v1.1 to v2.0 roadmap.

## Overview

The current mainline flow is:

```text
commandin -> scheduler -> lightctl -> gpio
lightctl  -> faultmg
faultmg   -> gpio
faultmg   -> scheduler -> lightctl
commandin -> vehicle_state -> scheduler
```

The shared state carries operator requests, vehicle state, fault mode, and scheduler target output.

## Component Roles

- `commandin`: receives UART input, parses keyboard commands, and writes normalized command bytes to the input buffer.
- `scheduler`: updates allow-flags in shared memory and applies the current rule checks before notifying `lightctl`.
- `lightctl`: converts allowed states into concrete GPIO actions and reports faults when checks fail.
- `gpio`: maps GPIO/timer regions and performs the actual pin-level operations.
- `faultmg`: receives fault notifications, owns the fault lifecycle, and publishes the current fault mode.

## Build Requirements

The current `Makefile` expects:

- Microkit SDK 2.0.1
- An AArch64 cross toolchain, auto-detected in this order:
  - `aarch64-linux-gnu-gcc`
  - `aarch64-unknown-linux-gnu-gcc`
  - `aarch64-none-elf-gcc`
- `qemu-system-aarch64`

Default SDK path:

```text
../microkit-sdk-2.0.1
```

If your SDK is elsewhere, override it on the command line.

## Build

The `Makefile` now exposes project-level entry points while keeping the tutorial-stage targets as legacy compatibility aliases.

Recommended targets:

- `make build`
- `make run`
- `make clean`
- `make debug`
- `make release`
- `make smoke`
- `make test`
- `make test-contract`
- `make evidence`
- `make qemu-test`
- `make test-transport`
- `make test-snapshot`
- `make test-fault-transport`
- `make help`

Recommended full build:

```bash
make build
```

Aggregate test logs are written to `test-results/<run-id>/`. For a report run,
use a stable id before copying files back from the Ubuntu VM:

```bash
make test TEST_RUN_ID=report-v6
make build
make qemu-test TEST_RUN_ID=report-v6
make evidence TEST_RUN_ID=report-v6
```

`make evidence` writes `test-results/<run-id>/manifest.txt`, which records the
run id, target board, Microkit config, summary files, and expected evidence
tokens such as `STATUS_SNAPSHOT ... layout=3 contract=OK` and
`FAULTMG_HISTORY ... lifecycle=ACTIVE/RECOVERING`.

The latest accepted baseline is **LightDemo Engineering Baseline v1.0** with
run id `report-v6`:

| Validation item | Result | Evidence |
| --- | --- | --- |
| Host-side tests | PASS | `test-results/report-v6/host-summary.txt` |
| QEMU smoke | PASS | `test-results/report-v6/smoke.make.log` |
| QEMU fault integration | PASS | `test-results/report-v6/test-integration-fault.make.log` |
| QEMU serial E2E | PASS | `test-results/report-v6/test-serial-e2e.make.log` |
| Evidence manifest | PASS | `test-results/report-v6/manifest.txt` |

Or with an explicit SDK path:

```bash
make build MICROKIT_SDK=/path/to/microkit-sdk-2.0.1
```

Legacy staged targets are still available:

- `make part1`
- `make part2`
- `make part3`
- `make part4`
- `make part5`
- `make legacy`

Legacy behavior notes:

- the current `light.system` describes the full system, so `part1` to `part4` are now compatibility aliases that reuse the full build and emit the old `demo_part_*.img` filenames
- `part5` remains equivalent to `make build`
- `make run` still uses `build/loader.img`

Important current build settings from `Makefile`:

- `BOARD := qemu_virt_aarch64`
- default `MICROKIT_CONFIG := debug`
- output image: `build/loader.img`
- legacy stage images: `build/demo_part_one.img` to `build/demo_part_five.img`
- report file: `build/report.txt`

## Run

After building, start QEMU with:

```bash
make run
```

This runs the image at `build/loader.img`.

## Validation

Host-side validation:

```bash
make test
```

Individual host-side validation:

```bash
make test-policy
make test-runtime
make test-fault
make test-fault-transport
```

Full QEMU validation:

```bash
make qemu-test
```

Individual QEMU validation:

```bash
make smoke
make test-integration-fault
make test-serial-e2e
```

The repository includes a minimal automated smoke test:

```bash
make smoke
```

It builds the full image, boots QEMU, waits for the five core module init logs, sends `L`, `H`, and `B`, and checks the expected input/scheduler/execution log chain.

The repository also includes a QEMU fault-injection integration test:

```bash
make test-integration-fault
```

That path proves `fault event -> faultmg transition -> scheduler re-arbitration -> lightctl sync -> gpio output switch` without waiting for another normal light command.

## Debug / Release Notes

- `make debug` and `make release` switch `MICROKIT_CONFIG` between the Microkit SDK `debug` and `release` board directories.
- In this repository, `make release` also uses `-O2 -DNDEBUG -g0` so it is closer to a production-style build.
- With Microkit SDK 2.0.1 on `qemu_virt_aarch64`, both `debug` and `release` directories are present, so `make release` is supported.

## CI

- CI always runs host validation.
- CI runs QEMU validation only when `MICROKIT_SDK_URL` is configured.
- If the SDK URL is not configured, CI explicitly logs that QEMU validation was skipped and why.

## UART Commands

The current command mapping is:

| Function | On | Off | Opcode |
| --- | --- | --- | --- |
| Low beam | `L` | `l` | `0x01` / `0x00` |
| High beam | `H` | `h` | `0x11` / `0x10` |
| Left turn | `Z` | `z` | `0x21` / `0x20` |
| Right turn | `Y` | `y` | `0x31` / `0x30` |
| Position light | `P` | `p` | `0x41` / `0x40` |
| Brake light | `B` | `b` | `0x51` / `0x50` |

Fault-management helpers:

- `!`: inject `LIGHT_ERR_MODE_CONFLICT`
- `#`: inject `LIGHT_ERR_HW_STATE_ERR`
- `C`: clear active faults and, while already recovering, advance one recovery observation tick
- `?`: print a unified `STATUS_SNAPSHOT` including fault mode, lifecycle phase, recovery progress, and fault statistics
- Current `STATUS_SNAPSHOT` lines also include `last_fault_name=...` for a readable fault-code token.

## Fault Lifecycle

The README lifecycle description is synchronized with the current
implementation. `fault_mg` is the only owner of global fault mode and lifecycle.

| Lifecycle | Meaning |
| --- | --- |
| `STABLE` | No active fault and no recovery in progress. |
| `ACTIVE` | One or more active fault markers are present. |
| `RECOVERING` | Active faults were cleared, but the system is still stepping down severity through observation ticks. |

Current recovery behavior:

1. A fault event enters `ACTIVE`.
2. Clear removes active fault markers.
3. Non-normal modes enter `RECOVERING`.
4. Healthy observation ticks advance `recovery_ticks`.
5. Each satisfied window steps down one fault mode level.
6. A new fault during recovery interrupts recovery and returns to `ACTIVE`.
7. Returning to `NORMAL` moves lifecycle back to `STABLE`.

Current escalation behavior:

| Condition | Result |
| --- | --- |
| No active fault | `NORMAL` |
| Any recognized fault | `WARN` |
| Three consecutive `LIGHT_ERR_MODE_CONFLICT` events | `DEGRADED` |
| Two `LIGHT_ERR_HW_STATE_ERR` events | `SAFE_MODE` |

Fault Lifecycle v2 diagnostics add stable evidence logs without changing the
shared-memory layout. The current layout remains `layout=3`.

```text
FAULTMG_HISTORY seq=... event=ERROR code=0x04 code_name=HW_STATE_ERR mode=SAFE_MODE lifecycle=ACTIVE ...
FAULTMG_HISTORY seq=... event=CLEAR code=0x00 code_name=NONE mode=SAFE_MODE lifecycle=RECOVERING ...
FAULTMG_HISTORY seq=... event=RECOVERY_TICK code=0x00 code_name=NONE mode=DEGRADED lifecycle=RECOVERING ...
```

Contract rejects use a common runtime evidence shape:

```text
CMD_CONTRACT_REJECT reason=...
SCHED_CONTRACT_REJECT reason=...
VEHICLE_STATE_CONTRACT_REJECT reason=...
FAULTMG_CONTRACT_REJECT reason=...
LIGHTCTL_CONTRACT_REJECT reason=...
```

## Notes

- `build/` is a build-output directory, not source code.
- `vmm/` exists in the repository, but it is not part of the default `part1` to `part5` build path.
- The current lifecycle v2 keeps the same fault semantics and adds diagnostics: recent event history, readable fault names, and contract reject evidence. It still does not implement real time-based recovery or a full fault taxonomy.
- Engineering review docs live under `docs/`: architecture, safety case, requirements, test plan, and demo script.

## Current Boundary and Roadmap

Current boundary:

- Validation is accepted on Ubuntu 22.04 VM + QEMU, not on a real board.
- GPIO behavior is observed through QEMU logs and simulated MMIO behavior.
- Recovery uses observation ticks, not a real-time recovery window.
- Fault taxonomy is project-scale and is not a full automotive fault catalog.
- The project is an engineering-practice baseline, not a formal safety
  certification artifact.
- Shared memory remains `layout=3`.
- `test-results/report-v6/` is release evidence; `build/` and
  `build-test-hooks/` are generated or copied artifacts.

Roadmap:

| Version | Main line |
| --- | --- |
| `v1.1` | Replace observation ticks with a real-time recovery window. |
| `v1.2` | Expand fault taxonomy with source, severity, recovery policy, and tests. |
| `v1.3` | Archive manifests, summaries, QEMU logs, and failure hints in CI. |
| `v2.0` | Validate real-board GPIO behavior or expand the vehicle-state model. |
