# Demo Script

Use this flow for project inspection or a short engineering-practice defense. It is written as a checklist rather than an executable script.

## 1. Architecture Walkthrough

Show `docs/architecture.md` and the README architecture diagram.

Explain the protection-domain split:

- `commandin`: UART input gateway.
- `scheduler`: rule arbitration and target output.
- `lightctl`: execution planning and runtime guard.
- `gpio`: hardware-facing output.
- `fault_mg`: single owner of fault lifecycle.
- `vehicle_state`: vehicle state updates.

Point out that Microkit channel IDs are centralized in `include/light_channels.h` and mirrored by `light.system`.

## 2. Build

Run:

```bash
make build
```

Expected evidence:

- `build/loader.img`
- `build/report.txt`
- no compiler warnings because the project uses `-Werror`

## 3. Host-Side Test Suite

Run:

```bash
make test
```

Explain that this validates policy, protocol, command codec, transport parsing, snapshot formatting, control logic, vehicle state, execution planning, runtime guard, and fault lifecycle logic without booting QEMU.

## 4. QEMU Smoke Test

Run:

```bash
make smoke
```

Expected evidence:

- all core domains print init logs
- serial commands are accepted
- scheduler and lightctl logs show command propagation

## 5. Fault Injection Demo

Run:

```bash
make test-integration-fault
```

Explain the important path:

```text
fault injection -> faultmg transition -> scheduler re-arbitration -> lightctl sync -> gpio output
```

Expected evidence:

- repeated mode conflicts enter `DEGRADED`
- repeated hardware-state errors enter `SAFE_MODE`
- GPIO output changes without waiting for another normal light command

## 6. Status Snapshot and Recovery

Run:

```bash
make test-serial-e2e
```

Explain the serial keys:

- `#`: inject hardware-state error
- `?`: print `STATUS_SNAPSHOT`
- `C`: clear active faults or advance recovery observation

Expected evidence:

- `SAFE_MODE` appears after repeated hardware-state errors
- `clear` enters `RECOVERING`, not `NORMAL`
- recovery ticks step down one level at a time

## 7. One-Command QEMU Validation

Run:

```bash
make qemu-test
```

Use this as the final demonstration command when the environment has QEMU and the Microkit SDK configured.

## 8. If the Environment Fails

If Ubuntu cannot find a dependency, record the exact missing command:

```bash
command -v make
command -v qemu-system-aarch64
command -v aarch64-linux-gnu-gcc
```

Then show `make test` if host-side compilation is still available, and explain that QEMU validation depends on the SDK, cross toolchain, and QEMU installation.
