#!/usr/bin/env bash
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE_FILE="${IMAGE_FILE:-build/loader.img}"
if [ -n "${TEST_RESULTS_RUN_DIR:-}" ]; then
    LOG_DIR="$TEST_RESULTS_RUN_DIR/fault-injection"
else
    LOG_DIR="$ROOT_DIR/${TEST_RESULTS_DIR:-test-results}/$(date +%Y%m%d-%H%M%S)/fault-injection"
fi
LOG_FILE="$LOG_DIR/qemu.log"
UART_FIFO="$LOG_DIR/uart.fifo"
QEMU_PID=""

stop_qemu() {
    if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
    fi
}

cleanup() {
    stop_qemu
    rm -f "$UART_FIFO"
}

wait_for_log() {
    pattern="$1"
    timeout_seconds="${2:-20}"
    elapsed=0

    while [ "$elapsed" -lt "$timeout_seconds" ]; do
        if grep -Fq "$pattern" "$LOG_FILE" 2>/dev/null; then
            return 0
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done

    echo "Fault integration timeout waiting for log: $pattern" >&2
    echo "--- qemu.log tail ---" >&2
    tail -n 120 "$LOG_FILE" >&2 || true
    return 1
}

send_key() {
    key="$1"
    printf '%s' "$key" >&3
}

line_after() {
    pattern="$1"
    after_line="$2"
    awk -v pattern="$pattern" -v after_line="$after_line" '
        index($0, pattern) > 0 && NR > after_line { print NR; exit }
    ' "$LOG_FILE"
}

last_line() {
    pattern="$1"
    awk -v pattern="$pattern" '
        index($0, pattern) > 0 { line = NR }
        END { if (line > 0) { print line } }
    ' "$LOG_FILE"
}

assert_no_pattern_between() {
    pattern="$1"
    start_line="$2"
    end_line="$3"

    if awk -v pattern="$pattern" -v start_line="$start_line" -v end_line="$end_line" '
        NR > start_line && NR < end_line && index($0, pattern) > 0 { found = 1 }
        END { exit(found ? 0 : 1) }
    ' "$LOG_FILE"; then
        echo "Unexpected log between lines $start_line and $end_line: $pattern" >&2
        exit 1
    fi
}

start_qemu() {
    cd "$ROOT_DIR"
    mkdir -p "$LOG_DIR"
    rm -f "$UART_FIFO"
    mkfifo "$UART_FIFO"

    qemu-system-aarch64 -machine virt,virtualization=on \
        -cpu cortex-a53 \
        -rtc base=localtime \
        -serial mon:stdio \
        -device loader,file="$IMAGE_FILE",addr=0x70000000,cpu-num=0 \
        -m size=2G \
        -nographic \
        -netdev user,id=mynet0 \
        -device virtio-net-device,netdev=mynet0,mac=52:55:00:d1:55:01 \
        <"$UART_FIFO" >"$LOG_FILE" 2>&1 &
    QEMU_PID="$!"
    exec 3>"$UART_FIFO"

    wait_for_log "CMD_INIT module=commandin status=ready"
    wait_for_log "SCHED_INIT module=scheduler status=ready"
    wait_for_log "LIGHTCTL_INIT module=lightctl status=ready"
    wait_for_log "GPIO_INIT module=gpio status=ready"
    wait_for_log "FAULT_INIT module=faultmg status=ready"
}

restart_qemu() {
    exec 3>&-
    stop_qemu
    cp "$LOG_FILE" "$LOG_DIR/qemu-degraded.log"
    rm -f "$UART_FIFO"
    : >"$LOG_FILE"
    QEMU_PID=""
    start_qemu
}

run_degraded_scenario() {
    local inject_line output_line

    send_key "!"
    wait_for_log "CMD_MSG type=fault_inject code=0x02"
    send_key "!"
    wait_for_log "FAULTMG_MODE_TRANSITION prev=WARN next=WARN changed=0"
    send_key "!"
    wait_for_log "FAULTMG_MODE_TRANSITION prev=WARN next=DEGRADED changed=1"
    wait_for_log "SCHED_TARGET mode=DEGRADED"
    wait_for_log "LIGHTCTL_TARGET_SUMMARY mode=DEGRADED target=[brake=0,left=0,right=0,low=1,high=0,marker=1]"
    wait_for_log "GPIO_OUTPUT_SUMMARY brake=0 left=0 right=0 low=1 high=0 position=1"

    inject_line=$(last_line "CMD_MSG type=fault_inject code=0x02")
    output_line=$(line_after "GPIO_OUTPUT_SUMMARY brake=0 left=0 right=0 low=1 high=0 position=1" "$inject_line")
    assert_no_pattern_between "CMD_MSG type=light_cmd" "$inject_line" "$output_line"
}

run_safe_mode_scenario() {
    local inject_line output_line

    send_key "p"
    wait_for_log "CMD_MSG type=light_cmd cmd=0x40"
    wait_for_log "SCHED_CMD_ACCEPT cmd=0x40"
    wait_for_log "GPIO_OUTPUT_SUMMARY brake=0 left=0 right=0 low=0 high=0 position=0"

    send_key "#"
    wait_for_log "CMD_MSG type=fault_inject code=0x04"
    send_key "#"
    wait_for_log "FAULTMG_MODE_TRANSITION prev=WARN next=SAFE_MODE changed=1"
    wait_for_log "SCHED_TARGET mode=SAFE_MODE"
    wait_for_log "LIGHTCTL_TARGET_SUMMARY mode=SAFE_MODE target=[brake=0,left=0,right=0,low=1,high=0,marker=1]"
    wait_for_log "GPIO_OUTPUT_SUMMARY brake=0 left=0 right=0 low=1 high=0 position=1"

    inject_line=$(last_line "CMD_MSG type=fault_inject code=0x04")
    output_line=$(line_after "GPIO_OUTPUT_SUMMARY brake=0 left=0 right=0 low=1 high=0 position=1" "$inject_line")
    assert_no_pattern_between "CMD_MSG type=light_cmd" "$inject_line" "$output_line"
}

trap cleanup EXIT
start_qemu
run_degraded_scenario
restart_qemu
run_safe_mode_scenario
trap - EXIT
cleanup
cp "$LOG_FILE" "$LOG_DIR/qemu-safe-mode.log"
echo "Fault integration test passed. Log file: $LOG_FILE"
