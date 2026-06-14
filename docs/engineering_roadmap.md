# Engineering Roadmap

This roadmap records the completed path from the earlier engineering baseline to **LightDemo Engineering Final v2.0**.

## Completed Version Path

| Version | Main line | Final status | Evidence |
| --- | --- | --- | --- |
| v1.0 | Engineering baseline | Completed earlier as `report-v6` | Historical evidence in `test-results/report-v6/` |
| v1.1 | Real-time recovery window | Completed | `recovery_elapsed_ms`, `recovery_window_ms=2000` |
| v1.2 | Fuller fault taxonomy | Completed | `FAULTMG_EVENT ... severity=... recovery_policy=...` |
| v1.3 | Evidence retention | Completed | `manifest.txt`, `summary.md`, `defense_report.md` |
| v2.0 | Vehicle-state model and final defense baseline | Completed | `test-results/final-v2.0/` |

## Accepted Final Baseline

| Item | Value |
| --- | --- |
| Release | LightDemo Engineering Final v2.0 |
| Accepted run id | `final-v2.0` |
| Evidence directory | `test-results/final-v2.0/` |
| Board | `qemu_virt_aarch64` |
| Config | `debug` |
| Shared layout | `layout=4` |
| Host tests | PASS |
| QEMU tests | PASS |

## v1.1 Outcome

The recovery model now exposes elapsed-time progress in runtime evidence:

```text
recovery_elapsed_ms=1000 recovery_window_ms=2000
STATUS_SNAPSHOT ... recovery_elapsed_ms=0/2000 ... layout=4 contract=OK
```

The compatibility `C` path remains useful for serial demonstrations, but the engineering evidence now names the elapsed-time window explicitly.

## v1.2 Outcome

Fault metadata is visible in runtime logs:

```text
FAULTMG_EVENT ... name=HW_STATE_ERR severity=SAFE_MODE_AFTER_2 recovery_policy=clear_then_elapsed_window output_policy=conservative_low_beam_position_only
FAULTMG_EVENT ... name=MODE_CONFLICT severity=DEGRADED_AFTER_3_CONSECUTIVE recovery_policy=clear_then_elapsed_window
```

## v1.3 Outcome

Evidence generation now preserves:

- `manifest.txt`
- `summary.md`
- `defense_report.md`
- QEMU logs
- host summaries
- PASS/FAIL status and key evidence token checks

## v2.0 Outcome

The vehicle-state model now covers:

- speed
- ignition
- brake pedal
- gear
- ambient light
- hazard
- drive mode

The accepted host scenario test is `test-vehicle-scenarios`.

## Remaining Future Work

The next practical step is real-board GPIO validation using `docs/real_board_validation_template.md`. Until that is filled with measured board evidence, the project should be described as QEMU-validated rather than real-board electrically validated.
