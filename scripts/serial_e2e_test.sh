#!/usr/bin/env bash
set -eu

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE_FILE="${IMAGE_FILE:-build/loader.img}"
LOG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/lightctl-serial-e2e.XXXXXX")"
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

    echo "Serial E2E timeout waiting for log: $pattern" >&2
    echo "--- qemu.log tail ---" >&2
    tail -n 120 "$LOG_FILE" >&2 || true
    return 1
}

send_text() {
    text="$1"
    printf '%s' "$text" >&3
}

trap cleanup EXIT
cd "$ROOT_DIR"
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

send_text "L"
wait_for_log "CMD_MSG type=light_cmd cmd=0x01"
wait_for_log "SCHED_TARGET mode=NORMAL speed=10 ignition=1 brake_pedal=0 req[low=1 high=0 left=0 right=0 marker=1 brake=0] target[low=1 high=0 left=0 right=0 marker=1 brake=0]"

send_text "H"
wait_for_log "CMD_MSG type=light_cmd cmd=0x11"

send_text "speed=5\r"
wait_for_log "CMD_MSG type=vehicle_state field=1 value=5"
wait_for_log "VEHICLE_STATE_UPDATE field=1 value=5 changed=1 speed=5 brake=0 ignition=1"
wait_for_log "SCHED_TARGET mode=NORMAL speed=5 ignition=1 brake_pedal=0 req[low=1 high=1 left=0 right=0 marker=1 brake=0] target[low=1 high=0 left=0 right=0 marker=1 brake=0]"

send_text "#"
wait_for_log "CMD_MSG type=fault_inject code=0x04"
send_text "#"
wait_for_log "FAULTMG_MODE_TRANSITION prev=WARN next=SAFE_MODE changed=1 code=0x04 total=2"
wait_for_log "SCHED_TARGET mode=SAFE_MODE speed=5 ignition=1 brake_pedal=0 req[low=1 high=1 left=0 right=0 marker=1 brake=0] target[low=1 high=0 left=0 right=0 marker=1 brake=0]"

send_text "?"
wait_for_log "CMD_MSG type=query_status"
wait_for_log "STATUS_SNAPSHOT fault=SAFE_MODE speed=5 ignition=1 brake_pedal=0 target[low=1 high=0 left=0 right=0 marker=1 brake=0]"

trap - EXIT
cleanup
echo "Serial E2E test passed. Log file: $LOG_FILE"
