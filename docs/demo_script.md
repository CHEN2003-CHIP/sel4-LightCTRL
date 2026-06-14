# Demo Script

Use this flow for the final LightDemo v2.0 defense.

## 1. Open the Showcase

Open:

```text
reports/final_v2_showcase.md
reports/final_v2_metrics.md
```

Start with the one-screen conclusion:

- LightDemo Engineering Final v2.0
- Host tests: PASS
- QEMU tests: PASS
- Shared layout: layout=4
- Evidence directory: `test-results/final-v2.0/`

Then switch to `reports/final_v2_metrics.md` and show the result charts:

- validation overview: Host 14/14, QEMU 3/3, evidence tokens 7/7
- scenario sweep: 46,080 vehicle/fault combinations in `vehicle-sweep.csv`
- fault escalation + recovery: deterministic SAFE_MODE/DEGRADED behavior and 2000 ms recovery window
- vehicle model growth: v1.0 3 fields to v2.0 7 fields

## 2. Explain Architecture

Show `docs/architecture.md`.

Explain the protection-domain split:

- `commandin`: UART input and dispatch
- `vehicle_state`: vehicle model updates
- `scheduler`: target-output arbitration
- `lightctl`: execution coordination
- `gpio`: hardware-facing output
- `faultmg`: single fault lifecycle owner

Use this chain:

```text
UART -> commandin -> scheduler -> lightctl -> gpio
UART -> commandin -> vehicle_state -> scheduler
faultmg -> scheduler + gpio
```

## 3. Explain v1.1-v2.0 Work

Use the version table in `reports/final_v2_showcase.md`:

| Version | What to say |
| --- | --- |
| v1.1 | Recovery is now visible as elapsed time: `recovery_elapsed_ms` |
| v1.2 | Fault taxonomy exposes source/severity/recovery/output policy |
| v1.3 | Evidence is automatically summarized and archived |
| v2.0 | Vehicle model includes gear, ambient light, hazard, and drive mode |

## 4. Show Validation

Open:

```text
reports/final_v2_metrics.md
test-results/final-v2.0/host-summary.txt
test-results/final-v2.0/qemu-summary.txt
test-results/final-v2.0/manifest.txt
reports/defense_report_final-v2.0.md
```

Point out:

```text
host_status=PASS
qemu_status=PASS
token_contract_OK=FOUND
token_layout_4=FOUND
token_recovery_elapsed_ms=FOUND
```

## 5. Show Runtime Evidence

Open:

```text
test-results/final-v2.0/serial-e2e/qemu.log
```

Search for:

```text
STATUS_SNAPSHOT
FAULTMG_EVENT
FAULTMG_HISTORY
recovery_elapsed_ms
layout=4 contract=OK
```

Useful lines to explain:

```text
STATUS_SNAPSHOT fault=SAFE_MODE lifecycle=ACTIVE ... recovery_elapsed_ms=0/2000 ... layout=4 contract=OK
FAULTMG_EVENT ... severity=SAFE_MODE_AFTER_2 recovery_policy=clear_then_elapsed_window
FAULTMG_RECOVERY_TICK ... recovery_elapsed_ms=1000 recovery_window_ms=2000
```

## 6. If Running Live

In Ubuntu:

```bash
make test-vehicle-sweep TEST_RUN_ID=final-v2.0
make test TEST_RUN_ID=final-v2.0
make qemu-test TEST_RUN_ID=final-v2.0 MICROKIT_SDK=../microkit-sdk-2.0.1
make evidence TEST_RUN_ID=final-v2.0
make defense-report TEST_RUN_ID=final-v2.0
```

If environment setup fails, show the preserved `final-v2.0` evidence and explain that QEMU validation depends on Microkit SDK, AArch64 toolchain, and QEMU availability.

## 7. Manual Serial Demo

Run:

```bash
make run MICROKIT_SDK=../microkit-sdk-2.0.1
```

Type this sequence slowly during the defense:

```text
?
ambient=night
L
H
speed=5
?
#
#
?
C
C
?
hazard=1
mode=emergency
?
```

Point out these readable log prefixes:

```text
DEMO_READY   available commands
DEMO_FLOW    input command and route between protection domains
DEMO_RESULT  scheduler, vehicle_state, and faultmg decisions
DEMO_FAULT   fault escalation, clear, and recovery progress
```

How to read the live terminal:

| What you type | What to look for | Meaning |
| --- | --- | --- |
| `L` | `[INPUT] light command LOW_BEAM_ON` | commandin parsed the key correctly |
| `L` | `>>> RESULT ... lamps LOW=ON ...` | scheduler recomputed the final light output |
| `ambient=night` | `DEMO_RESULT stage=vehicle_state ... ambient:NIGHT` | vehicle_state accepted the state update |
| `#` twice | `DEMO_FAULT event=INJECT ... HW_STATE_ERR` then `>>> RESULT mode=SAFE_MODE` | faultmg escalated the fault and scheduler applied safe output |
| `C` | `DEMO_FAULT event=CLEAR` or `DEMO_FAULT event=RECOVERY_TICK` | active fault cleared or recovery window advanced |
| `?` | colored `Live Status` panel | current fault, vehicle state, contract, and lamp state |

The most defense-friendly line after a light command is:

```text
>>> RESULT mode=NORMAL speed=10 lamps LOW=ON HIGH=off LEFT=off RIGHT=off MARKER=off BRAKE=off
```

The most defense-friendly view after `?` is the `lamps` row in the status panel:

```text
lamps LOW=ON HIGH=off LEFT=off RIGHT=off MARKER=ON BRAKE=off
```

The original machine-checkable tokens still remain available:

```text
CMD_MSG
SCHED_TARGET
FAULTMG_EVENT
STATUS_SNAPSHOT
```

## 8. Close With Boundary

Say clearly:

- QEMU validation is complete.
- Real-board GPIO electrical validation is not claimed as completed.
- A real-board validation template is provided in `docs/real_board_validation_template.md`.
- The project demonstrates engineering structure, contracts, fault governance, automated evidence, and a v2.0 vehicle model.
