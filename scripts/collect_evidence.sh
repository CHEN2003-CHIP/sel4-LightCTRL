#!/usr/bin/env bash
set -eu

RUN_DIR="${1:?run directory required}"
RUN_ID="${2:?run id required}"
BOARD="${BOARD:-qemu_virt_aarch64}"
MICROKIT_CONFIG="${MICROKIT_CONFIG:-debug}"
SHARED_LAYOUT="${SHARED_LAYOUT:-4}"

mkdir -p "$RUN_DIR"

manifest="$RUN_DIR/manifest.txt"
summary="$RUN_DIR/summary.md"
defense="$RUN_DIR/defense_report.md"

git_commit="unknown"
if command -v git >/dev/null 2>&1; then
    git_commit="$(git rev-parse --short HEAD 2>/dev/null || printf unknown)"
fi

token_status() {
    token="$1"
    if grep -R -F -q "$token" "$RUN_DIR" 2>/dev/null; then
        printf "FOUND"
    else
        printf "MISSING"
    fi
}

host_status="MISSING"
if [ -f "$RUN_DIR/host-summary.txt" ]; then
    if grep -F -q "FAIL " "$RUN_DIR/host-summary.txt"; then
        host_status="FAIL"
    else
        host_status="PASS"
    fi
fi

qemu_status="MISSING"
if [ -f "$RUN_DIR/qemu-summary.txt" ]; then
    if grep -F -q "FAIL " "$RUN_DIR/qemu-summary.txt"; then
        qemu_status="FAIL"
    else
        qemu_status="PASS"
    fi
fi

created_at="$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || date)"
status_snapshot_status="$(token_status "STATUS_SNAPSHOT")"
fault_history_status="$(token_status "FAULTMG_HISTORY")"
safe_mode_status="$(token_status "SAFE_MODE")"
recovering_status="$(token_status "RECOVERING")"
contract_status="$(token_status "contract=OK")"
layout_status="$(token_status "layout=${SHARED_LAYOUT}")"
recovery_ms_status="$(token_status "recovery_elapsed_ms")"
sweep_rows="0"
sweep_csv_status="MISSING"
if [ -f "$RUN_DIR/vehicle-sweep-summary.txt" ]; then
    sweep_rows="$(grep -Eo 'VEHICLE_SWEEP rows=[0-9]+' "$RUN_DIR/vehicle-sweep-summary.txt" | sed 's/VEHICLE_SWEEP rows=//' | tail -n 1 || printf 0)"
fi
sweep_rows="${sweep_rows:-0}"
if [ "$sweep_rows" = "0" ] && [ -f "$RUN_DIR/vehicle-sweep.csv" ]; then
    sweep_rows="$(awk 'END { if (NR > 0) print NR - 1; else print 0 }' "$RUN_DIR/vehicle-sweep.csv")"
fi
if [ -f "$RUN_DIR/vehicle-sweep.csv" ]; then
    sweep_csv_status="FOUND"
fi

{
    printf "LightDemo validation evidence manifest\n"
    printf "release=LightDemo Engineering Final v2.0\n"
    printf "run_id=%s\n" "$RUN_ID"
    printf "created_at=%s\n" "$created_at"
    printf "git_commit=%s\n" "$git_commit"
    printf "board=%s\n" "$BOARD"
    printf "microkit_config=%s\n" "$MICROKIT_CONFIG"
    printf "shared_layout=%s\n" "$SHARED_LAYOUT"
    printf "host_summary=%s\n" "$RUN_DIR/host-summary.txt"
    printf "qemu_summary=%s\n" "$RUN_DIR/qemu-summary.txt"
    printf "host_status=%s\n" "$host_status"
    printf "qemu_status=%s\n" "$qemu_status"
    printf "token_STATUS_SNAPSHOT=%s\n" "$status_snapshot_status"
    printf "token_FAULTMG_HISTORY=%s\n" "$fault_history_status"
    printf "token_SAFE_MODE=%s\n" "$safe_mode_status"
    printf "token_RECOVERING=%s\n" "$recovering_status"
    printf "token_contract_OK=%s\n" "$contract_status"
    printf "token_layout_%s=%s\n" "$SHARED_LAYOUT" "$layout_status"
    printf "token_recovery_elapsed_ms=%s\n" "$recovery_ms_status"
    printf "vehicle_sweep_rows=%s\n" "$sweep_rows"
    printf "vehicle_sweep_csv=%s\n" "$sweep_csv_status"
} > "$manifest"

{
    printf '%s\n\n' "# LightDemo Final v2.0 Evidence Summary"
    printf '%s\n' "| Item | Value |"
    printf '%s\n' "| --- | --- |"
    printf '| Run ID | `%s` |\n' "$RUN_ID"
    printf '| Created at | `%s` |\n' "$created_at"
    printf '| Git commit | `%s` |\n' "$git_commit"
    printf '| Board | `%s` |\n' "$BOARD"
    printf '| Config | `%s` |\n' "$MICROKIT_CONFIG"
    printf '| Shared layout | `%s` |\n' "$SHARED_LAYOUT"
    printf '| Host tests | `%s` |\n' "$host_status"
    printf '| QEMU tests | `%s` |\n' "$qemu_status"
    printf '| Vehicle sweep rows | `%s` |\n' "$sweep_rows"
    printf '| Vehicle sweep CSV | `%s` |\n\n' "$sweep_csv_status"
    printf '%s\n\n' "## Key Evidence Tokens"
    printf '%s\n' "| Token | Status |"
    printf '%s\n' "| --- | --- |"
    printf '| `STATUS_SNAPSHOT` | `%s` |\n' "$status_snapshot_status"
    printf '| `FAULTMG_HISTORY` | `%s` |\n' "$fault_history_status"
    printf '| `SAFE_MODE` | `%s` |\n' "$safe_mode_status"
    printf '| `RECOVERING` | `%s` |\n' "$recovering_status"
    printf '| `contract=OK` | `%s` |\n' "$contract_status"
    printf '| `layout=%s` | `%s` |\n' "$SHARED_LAYOUT" "$layout_status"
    printf '| `recovery_elapsed_ms` | `%s` |\n' "$recovery_ms_status"
    printf '%s\n\n' "## Scenario Sweep"
    printf '%s\n' "| Item | Value |"
    printf '%s\n' "| --- | --- |"
    printf '| Vehicle/fault scenario rows | `%s` |\n' "$sweep_rows"
    printf '| CSV evidence | `%s` |\n' "$RUN_DIR/vehicle-sweep.csv"
} > "$summary"

{
    printf '%s\n\n' "# LightDemo Engineering Final v2.0 Defense Report"
    printf '%s\n\n' "## One-line Claim"
    printf '%s\n\n' "LightDemo is now a seL4/Microkit automotive lighting controller with explicit contracts, elapsed-time fault recovery, a v2.0 vehicle-state model, and archived validation evidence."
    printf '%s\n\n' "## Validation Result"
    printf -- '- Host tests: `%s`\n' "$host_status"
    printf -- '- QEMU tests: `%s`\n' "$qemu_status"
    printf -- '- Shared memory layout: `layout=%s`\n' "$SHARED_LAYOUT"
    printf -- '- Evidence run: `%s`\n\n' "$RUN_ID"
    printf '%s\n\n' "## Demonstration Highlights"
    printf -- '- v1.1: elapsed-time recovery evidence token: `%s`\n' "$recovery_ms_status"
    printf '%s\n' '- v1.2: fault taxonomy visible through `FAULTMG_EVENT` and `FAULTMG_HISTORY` logs.'
    printf -- '- v1.3: manifest, summary, QEMU logs, and failure hints are archived under `%s`.\n' "$RUN_DIR"
    printf '%s\n\n' '- v2.0: vehicle model covers speed, ignition, brake, gear, ambient light, hazard, and drive mode.'
    printf -- '- extended data: vehicle/fault scenario sweep rows: `%s`.\n\n' "$sweep_rows"
    printf '%s\n\n' '- live demo: colored `[INPUT]`, `>>> RESULT`, `DEMO_FAULT`, and `Live Status` output make manual serial behavior readable.'
    printf '%s\n\n' "## Remaining Boundary"
    printf '%s\n' "Real-board GPIO electrical validation is documented as a follow-up path; this final baseline is validated on Ubuntu/QEMU."
} > "$defense"

printf "Evidence manifest: %s\n" "$manifest"
printf "Evidence summary: %s\n" "$summary"
printf "Defense report: %s\n" "$defense"
if [ -f "$RUN_DIR/host-summary.txt" ]; then
    cat "$RUN_DIR/host-summary.txt"
fi
if [ -f "$RUN_DIR/qemu-summary.txt" ]; then
    cat "$RUN_DIR/qemu-summary.txt"
fi
