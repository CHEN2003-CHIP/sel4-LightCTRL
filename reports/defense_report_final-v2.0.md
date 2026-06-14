# LightDemo Engineering Final v2.0 Defense Report

## One-line Claim

LightDemo is now a seL4/Microkit automotive lighting controller with explicit contracts, elapsed-time fault recovery, a v2.0 vehicle-state model, and archived validation evidence.

## Validation Result

- Host tests: `PASS`
- QEMU tests: `PASS`
- Shared memory layout: `layout=4`
- Evidence run: `final-v2.0`

## Demonstration Highlights

- v1.1: elapsed-time recovery evidence token: `FOUND`
- v1.2: fault taxonomy visible through `FAULTMG_EVENT` and `FAULTMG_HISTORY` logs.
- v1.3: manifest, summary, QEMU logs, and failure hints are archived under `test-results/final-v2.0`.
- v2.0: vehicle model covers speed, ignition, brake, gear, ambient light, hazard, and drive mode.

- extended data: vehicle/fault scenario sweep rows: `46080`.

- live demo: colored `[INPUT]`, `>>> RESULT`, `DEMO_FAULT`, and `Live Status` output make manual serial behavior readable.

## Remaining Boundary

Real-board GPIO electrical validation is documented as a follow-up path; this final baseline is validated on Ubuntu/QEMU.
