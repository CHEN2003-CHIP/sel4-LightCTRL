# Engineering Upgrade Baseline

This note describes the next LightDemo baseline as an engineering-grade
embedded project rather than a tutorial demo. The scope is intentionally
conservative: no new lighting feature is added. The value is in explicit
contracts, repeatable validation, and safety-oriented fault ownership.

## Engineering Positioning

LightDemo is treated as a small seL4/Microkit automotive lighting controller:

```text
UART input -> policy arbitration -> execution coordination -> GPIO output
                         |
                         v
              centralized fault management
```

The system is reviewable because each protection domain has one primary
responsibility:

| Layer | Domains | Engineering responsibility |
| --- | --- | --- |
| Input access | `commandin`, `vehicle_state` | Parse external input and normalize it into transport messages or vehicle-state updates. |
| Policy decision | `scheduler`, `fault_mg` | Own target-output arbitration and global fault lifecycle. |
| Execution and hardware | `lightctl`, `gpio` | Convert target output into checked GPIO actions and hardware-facing state. |

This split preserves the existing Microkit communication behavior while making
the control chain easier to defend in an engineering review.

## Interface Contracts

The contract layer in `light_contract.c` makes compatibility checks explicit:

| Contract | Guarded item | Evidence |
| --- | --- | --- |
| Shared-state layout | `LIGHT_SHARED_STATE_LAYOUT_V3` in `light_shmem_t` | `light_contract_check_shared_state`, `make test-contract` |
| Transport wire shape | version, type, payload length | `light_contract_check_transport_message`, `make test-contract` |
| Fault snapshot bounds | fault mode, lifecycle, recovery window, active mask | `light_contract_check_fault_snapshot`, `make test-contract` |
| Channel table | known Microkit endpoint IDs | `light_contract_channel_is_known`, `make test-contract` |

Runtime users log contract evidence with stable tags such as
`SCHED_CONTRACT`, `LIGHTCTL_CONTRACT`, and `FAULTMG_CONTRACT`. This turns
compatibility assumptions into inspectable QEMU log evidence.
Contract rejects use the same shape across components:
`CMD_CONTRACT_REJECT`, `SCHED_CONTRACT_REJECT`,
`VEHICLE_STATE_CONTRACT_REJECT`, `FAULTMG_CONTRACT_REJECT`, and
`LIGHTCTL_CONTRACT_REJECT`.

## Safety and Diagnostics

The fault-management design is promoted from demo behavior to a small safety
subsystem:

- `fault_mg` is the only owner of global fault mode and lifecycle transitions.
- Fault modes are explicit: `NORMAL`, `WARN`, `DEGRADED`, `SAFE_MODE`.
- Recovery is observation-based and rate-limited by a recovery window.
- New faults during recovery reset recovery progress, preventing fast clear and
  re-fault oscillation.
- `STATUS_SNAPSHOT` includes layout, contract status, and `last_fault_name`,
  so the serial diagnostic line can be used as regression evidence.
- `FAULTMG_HISTORY` records recent fault events, clear events, and recovery
  ticks as stable QEMU log evidence.

The important review point is not that this is production-certified automotive
safety software. The point is that the safety argument is visible, bounded, and
testable.

## Validation Model

The standard validation environment is Ubuntu 22.04 with the Microkit SDK,
QEMU, and an AArch64 cross compiler installed.

Recommended sequence:

```bash
make test
make build
make qemu-test
```

Aggregate runs store review evidence under `test-results/<run-id>/`. Use a
stable run id before copying files back from the Ubuntu VM:

```bash
make test TEST_RUN_ID=vm-20260520
make qemu-test TEST_RUN_ID=vm-20260520
make evidence TEST_RUN_ID=vm-20260520
```

`make evidence` writes `manifest.txt` in the run directory. The manifest lists
the run id, board, Microkit config, summary files, and expected evidence tokens
for snapshot, fault-history, and contract-reject checks.

Focused checks:

```bash
make test-contract
make test-fault
make test-runtime
make test-transport
make test-snapshot
make smoke
make test-integration-fault
make test-serial-e2e
```

Host-side tests validate pure policy, protocol, contract, and fault logic
quickly. QEMU tests validate protection-domain wiring, notifications, and
serial-observable behavior.

## Latest Evidence

The latest preserved validation run is `test-results/report-v6/`.

| Command group | Result |
| --- | --- |
| `make test TEST_RUN_ID=report-v6` | PASS |
| `make build` | PASS |
| `make qemu-test TEST_RUN_ID=report-v6` | PASS |
| `make evidence TEST_RUN_ID=report-v6` | PASS |

Important evidence:

- `host-summary.txt` shows all host-side tests passed.
- `qemu-summary.txt` shows `smoke`, `test-integration-fault`, and
  `test-serial-e2e` all passed.
- `serial-e2e/qemu.log` contains `STATUS_SNAPSHOT ... last_fault_name=HW_STATE_ERR ... layout=3 contract=OK`.
- `serial-e2e/qemu.log` contains `FAULTMG_HISTORY ... lifecycle=ACTIVE` and
  `FAULTMG_HISTORY ... lifecycle=RECOVERING`.

The report-ready v2 evidence is now preserved in `report-v6`.

See `docs/validation_report.md` for the review-ready validation record.

## Presentation Highlights

Use these points for the project report:

- The project moved from staged tutorial targets to one engineering baseline:
  `make build`, `make test`, and `make qemu-test`.
- Microkit communication contracts are centralized and tested rather than
  duplicated across components.
- Fault handling has a single owner, explicit severity levels, conservative
  output policy, and anti-flap recovery.
- Serial and QEMU logs are structured enough to serve as repeatable regression
  evidence.
- The requirements, architecture, safety case, and test plan now form a
  traceable chain from design intent to validation command.
