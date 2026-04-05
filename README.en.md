# LightDemo

LightDemo is a seL4 Microkit demo project for an automotive light-control pipeline. The repository models the full path from UART command input to scheduling, light control, GPIO operations, and fault logging on the default `qemu_virt_aarch64` target.

## Overview

The current mainline flow is:

```text
commandin -> scheduler -> lightctl -> gpio
lightctl  -> faultmg
faultmg   -> lightctl
```

`scheduler` and `lightctl` also share one memory region for the current allow-state.

## Component Roles

- `commandin`: receives UART input, parses keyboard commands, and writes normalized command bytes to the input buffer.
- `scheduler`: updates allow-flags in shared memory and applies the current rule checks before notifying `lightctl`.
- `lightctl`: converts allowed states into concrete GPIO actions and reports faults when checks fail.
- `gpio`: maps GPIO/timer regions and performs the actual pin-level operations.
- `faultmg`: receives fault notifications, counts them, and prints fault logs.

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
- `make test-fault-transport`
- `make help`

Recommended full build:

```bash
make build
```

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
make test-policy
make test-runtime
make test-fault
make test-fault-transport
```

Full QEMU validation:

```bash
make smoke
make test-integration-fault
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

That path proves `fault event -> faultmg transition -> lightctl immediate sync -> gpio output switch` without waiting for another normal scheduler update.

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

## Notes

- `build/` is a build-output directory, not source code.
- `vmm/` exists in the repository, but it is not part of the default `part1` to `part5` build path.
- The current fault-management implementation is limited to error counting and logging; it does not implement a full recovery flow.
