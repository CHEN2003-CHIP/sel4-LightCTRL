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

Available staged targets:

- `make part1`
- `make part2`
- `make part3`
- `make part4`
- `make part5`

`part5` is the recommended full build target for the current repository state:

```bash
make part5
```

Or with an explicit SDK path:

```bash
make MICROKIT_SDK=/path/to/microkit-sdk-2.0.1 part5
```

Important current build settings from `Makefile`:

- `BOARD := qemu_virt_aarch64`
- `MICROKIT_CONFIG := debug`
- output image: `build/loader.img`
- report file: `build/report.txt`

## Run

After building, start QEMU with:

```bash
make run
```

This runs the image at `build/loader.img`.

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
