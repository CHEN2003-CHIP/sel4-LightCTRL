# Engineering Roadmap

This roadmap starts from **LightDemo Engineering Baseline v1.0**, accepted by
the `report-v6` validation evidence. The goal is to evolve the project as a
mature embedded engineering baseline, not to add unrelated lighting demo
features.

## Current Baseline

The current release baseline is:

| Item | Value |
| --- | --- |
| Baseline | LightDemo Engineering Baseline v1.0 |
| Accepted run id | `report-v6` |
| Evidence directory | `test-results/report-v6/` |
| Platform | Ubuntu 22.04 VM + QEMU |
| Board | `qemu_virt_aarch64` |
| Config | `debug` |
| Shared memory layout | `layout=3` |

Accepted commands:

```bash
make test TEST_RUN_ID=report-v6
make build
make qemu-test TEST_RUN_ID=report-v6
make evidence TEST_RUN_ID=report-v6
```

Accepted evidence:

- Host-side tests: PASS
- QEMU smoke: PASS
- QEMU fault integration: PASS
- QEMU serial E2E: PASS
- Evidence manifest: `test-results/report-v6/manifest.txt`

## Current Boundary

The v1.0 baseline is intentionally bounded:

- QEMU validation is complete; real-board GPIO validation is not included.
- Recovery still uses observation ticks instead of real elapsed time.
- Fault taxonomy is sufficient for the project safety story, but not a complete
  vehicle-grade fault catalog.
- The project demonstrates engineering practice and reviewability; it does not
  claim ISO 26262 or other automotive safety certification.
- Shared memory stays at `layout=3`; future layout changes must be treated as
  compatibility-affecting releases.
- Generated `build/` and `build-test-hooks/` contents are artifacts, while
  source files, documentation, scripts, and `test-results/report-v6/` are the
  reviewable baseline evidence.

## Version Roadmap

| Version | Main Line | Deliverables | Validation Evidence |
| --- | --- | --- | --- |
| `v1.1` | Real-time recovery window | Replace observation-tick recovery with a time-window model backed by an explicit tick/time source. | Host tests for time-window transitions; QEMU logs proving recovery waits for the configured window. |
| `v1.2` | Fuller fault taxonomy | Define a broader fault catalog with severity, source, recovery policy, and test mapping. | Contract tests for fault metadata; fault-policy tests; updated traceability matrix. |
| `v1.3` | CI evidence retention | Automatically archive summaries, manifests, QEMU logs, and failure hints in CI. | CI artifacts containing `manifest.txt`, host summary, QEMU summary, and serial logs. |
| `v2.0` | Hardware or system-model expansion | Validate GPIO behavior on a real board, or introduce a richer vehicle-state model if hardware is unavailable. | Real-board GPIO report, or expanded vehicle-state integration tests with QEMU evidence. |

## v1.1: Real-Time Recovery Window

Intent:

- Move recovery from manual observation ticks to explicit elapsed-time windows.
- Keep `faultmg` as the lifecycle owner.
- Preserve `STABLE`, `ACTIVE`, and `RECOVERING` semantics.

Implementation direction:

- Introduce a small time-source abstraction suitable for host tests and QEMU.
- Define recovery windows in time units.
- Keep serial evidence readable through `STATUS_SNAPSHOT` and
  `FAULTMG_HISTORY`.

Acceptance:

- Recovery does not step down before the configured time window.
- A new fault during recovery resets the time window and returns lifecycle to
  `ACTIVE`.
- QEMU evidence can show the configured recovery window and current progress.

## v1.2: Fuller Fault Taxonomy

Intent:

- Turn the current project-scale fault codes into a more systematic taxonomy.
- Make each fault explainable by source, severity, recovery behavior, and test
  coverage.

Implementation direction:

- Add a fault taxonomy table in code or documentation with stable names.
- Map each fault to severity escalation and recovery policy.
- Extend traceability so each fault class has implementation and test evidence.

Acceptance:

- Each supported fault has a name, source, severity, recovery policy, and test.
- Unknown or unsupported faults have a documented handling rule.
- Documentation and tests agree on the taxonomy.

## v1.3: CI Evidence Retention

Intent:

- Make evidence collection automatic instead of dependent on manual VM copying.
- Preserve the current `test-results/<run-id>/` model.

Implementation direction:

- Add CI steps for host tests and optional QEMU tests when SDK access is
  available.
- Upload `manifest.txt`, summaries, make logs, and serial logs as CI artifacts.
- Keep failure summaries easy to inspect.

Acceptance:

- Every CI validation run keeps a run id.
- PASS and FAIL outcomes are visible from summaries.
- QEMU skips are explicit when SDK or QEMU is unavailable.

## v2.0: Real Board or Richer Vehicle Model

Intent:

- Move beyond QEMU-only evidence.
- Choose the expansion based on available resources.

Option A:

- Validate GPIO behavior on a real board.
- Add a board-level test report covering observed output behavior.

Option B:

- Expand vehicle-state modeling if real hardware is unavailable.
- Add more realistic state transitions and policy tests.

Acceptance:

- Hardware path: GPIO behavior has real-board evidence.
- Model path: vehicle-state behavior has expanded host and QEMU evidence.
- The project remains contract-driven and test-evidence-driven.

## Release Discipline

For every future version:

- Keep a named run id under `test-results/<run-id>/`.
- Update `docs/release_baseline.md` or add a new release note.
- Preserve the current boundary and explicitly state what changed.
- Run `make evidence TEST_RUN_ID=<run-id>` after validation.
- Do not change shared memory layout without documenting compatibility impact.
