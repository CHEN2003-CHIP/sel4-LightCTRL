#!/usr/bin/env bash
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/lightctl-smoke.XXXXXX")"
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
    rm -rf "$LOG_DIR"
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

    echo "Smoke test timeout waiting for log: $pattern" >&2
    echo "--- qemu.log tail ---" >&2
    tail -n 80 "$LOG_FILE" >&2 || true
    return 1
}

send_key() {
    key="$1"
    printf '%s' "$key" >&3
}

cd "$ROOT_DIR"
mkfifo "$UART_FIFO"

qemu-system-aarch64 -machine virt,virtualization=on \
    -cpu cortex-a53 \
    -rtc base=localtime \
    -serial mon:stdio \
    -device loader,file=build/loader.img,addr=0x70000000,cpu-num=0 \
    -m size=2G \
    -nographic \
    -netdev user,id=mynet0 \
    -device virtio-net-device,netdev=mynet0,mac=52:55:00:d1:55:01 \
    <"$UART_FIFO" >"$LOG_FILE" 2>&1 &
QEMU_PID="$!"
trap cleanup EXIT

exec 3>"$UART_FIFO"

wait_for_log "CMD_INIT module=commandin status=ready"
wait_for_log "SCHED_INIT module=scheduler status=ready"
wait_for_log "LIGHTCTL_INIT module=lightctl status=ready"
wait_for_log "GPIO_INIT module=gpio status=ready"
wait_for_log "FAULT_INIT module=faultmg status=ready"

send_key "L"
wait_for_log "CMD_RX char=L opcode=0x01"
wait_for_log "SCHED_APPLY cmd=0x01"
wait_for_log "LIGHTCTL_GPIO action=low_beam_on"
wait_for_log "GPIO_APPLY action=low_beam_on"

send_key "H"
wait_for_log "CMD_RX char=H opcode=0x11"
wait_for_log "SCHED_APPLY cmd=0x11"
wait_for_log "LIGHTCTL_GPIO action=high_beam_on"
wait_for_log "GPIO_APPLY action=high_beam_on"

send_key "B"
wait_for_log "CMD_RX char=B opcode=0x51"
wait_for_log "SCHED_APPLY cmd=0x51"
wait_for_log "LIGHTCTL_GPIO action=brake_on"
wait_for_log "GPIO_APPLY action=brake_on"

trap - EXIT
stop_qemu
echo "Smoke test passed. Log file: $LOG_FILE"
