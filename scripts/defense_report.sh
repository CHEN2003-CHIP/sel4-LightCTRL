#!/usr/bin/env bash
set -eu

RUN_DIR="${1:?run directory required}"
RUN_ID="${2:?run id required}"
REPORTS_DIR="${REPORTS_DIR:-reports}"

mkdir -p "$REPORTS_DIR"

summary="$RUN_DIR/summary.md"
defense="$RUN_DIR/defense_report.md"
manifest="$RUN_DIR/manifest.txt"
report_copy="$REPORTS_DIR/defense_report_${RUN_ID}.md"

if [ ! -f "$defense" ] || [ ! -f "$summary" ] || [ ! -f "$manifest" ]; then
    BOARD="${BOARD:-qemu_virt_aarch64}" MICROKIT_CONFIG="${MICROKIT_CONFIG:-debug}" SHARED_LAYOUT="${SHARED_LAYOUT:-4}" \
        bash scripts/collect_evidence.sh "$RUN_DIR" "$RUN_ID" >/dev/null
fi

cp "$defense" "$report_copy"

host_status="$(grep -E '^host_status=' "$manifest" 2>/dev/null | sed 's/^host_status=//' || printf MISSING)"
qemu_status="$(grep -E '^qemu_status=' "$manifest" 2>/dev/null | sed 's/^qemu_status=//' || printf MISSING)"
layout="$(grep -E '^shared_layout=' "$manifest" 2>/dev/null | sed 's/^shared_layout=//' || printf 4)"
sweep_rows="$(grep -E '^vehicle_sweep_rows=' "$manifest" 2>/dev/null | sed 's/^vehicle_sweep_rows=//' || printf 0)"

printf '%s\n' ""
printf '%s\n' "LightDemo Engineering Final v2.0"
printf '%s\n' "================================"
printf "Run ID: %s\n" "$RUN_ID"
printf "Evidence: %s\n" "$RUN_DIR"
printf "Shared layout: %s\n" "$layout"
printf '%s\n' ""
printf '%s\n' "Validation"
printf '%s\n' "----------"
printf "Host tests: %s\n" "$host_status"
printf "QEMU tests: %s\n" "$qemu_status"
printf "Vehicle/fault sweep rows: %s\n" "$sweep_rows"
printf '%s\n' ""
printf '%s\n' "Engineering story"
printf '%s\n' "-----------------"
printf "v1.1 elapsed-time recovery: %s\n" "$(grep -E '^token_recovery_elapsed_ms=' "$manifest" | sed 's/^token_recovery_elapsed_ms=//')"
printf "v1.2 fault taxonomy: %s\n" "$(grep -E '^token_FAULTMG_HISTORY=' "$manifest" | sed 's/^token_FAULTMG_HISTORY=//')"
printf '%s\n' "v1.3 archived evidence: manifest + summary + defense_report"
printf '%s\n' "v2.0 vehicle model: speed, ignition, brake, gear, ambient, hazard, drive mode"
printf "Extended scenario sweep: %s rows\n" "$sweep_rows"
printf '%s\n' "Live demo output: colored [INPUT], >>> RESULT, DEMO_FAULT, and Live Status panel"
printf '%s\n' ""
printf '%s\n' "Key evidence tokens"
printf '%s\n' "-------------------"
grep -E '^token_' "$manifest" | sed 's/^token_/- /; s/=/ = /'
printf '%s\n' ""
printf "Markdown report: %s\n" "$report_copy"
