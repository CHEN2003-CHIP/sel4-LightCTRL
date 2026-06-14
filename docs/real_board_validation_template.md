# Real Board GPIO Validation Template

This template records the follow-up path for real-board GPIO validation. The final v2.0 baseline is validated on Ubuntu/QEMU; this document prevents the boundary from being vague.

## Board Information

| Item | Value |
| --- | --- |
| Board | TBD |
| Microkit SDK | 2.0.1 or target-specific SDK |
| GPIO pins | TBD |
| Measurement method | LED, logic analyzer, oscilloscope, or multimeter |
| Operator | TBD |
| Date | TBD |

## Wiring Table

| Light output | GPIO/channel | Expected observation |
| --- | --- | --- |
| Low beam | TBD | ON/OFF follows low-beam command or safe-mode policy |
| High beam | TBD | Blocked in low speed, reverse/park, degraded, and safe mode |
| Left turn | TBD | ON by left request or hazard unless clamped by safe mode |
| Right turn | TBD | ON by right request or hazard unless clamped by safe mode |
| Position light | TBD | ON in marker/night/parking/safe mode |
| Brake | TBD | Preserved by brake command/pedal state |

## Test Cases

| Case | Input | Expected result | Evidence |
| --- | --- | --- | --- |
| Normal low beam | `L` | Low beam ON, position ON | Photo/log |
| High beam allowed | `L`, `H`, speed above threshold | High beam ON | Photo/log |
| Low-speed high beam block | `speed=5`, `L`, `H` | High beam OFF, low beam ON | Photo/log |
| Night mode | `ambient=night` | Low beam and position ON | Photo/log |
| Hazard mode | `hazard=1` | Left and right turn outputs ON | Photo/log |
| Safe mode | `#`, `#` | Conservative output profile | Photo/log |

## Pass Criteria

- GPIO observations match QEMU-visible target output.
- No unexpected reset or fault occurs during command sequence.
- Logs preserve `STATUS_SNAPSHOT ... layout=4 contract=OK`.

## Boundary Statement

Until this template is filled with measured board evidence, LightDemo v2.0 should be described as QEMU-validated, not real-board electrically validated.
