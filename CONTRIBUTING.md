# Contributing

## Validation levels

This repository supports two validation layers:

- Host validation: pure host-side unit tests that do not require Microkit SDK or QEMU.
- QEMU validation: full-system checks that require Microkit SDK 2.0.1, an AArch64 cross toolchain, and `qemu-system-aarch64`.

## Recommended local flow

Run host validation first:

```bash
make test-policy
make test-runtime
make test-fault
make test-fault-transport
```

When the local environment also provides Microkit SDK and QEMU, run the integration layer:

```bash
make smoke
make test-integration-fault
```

## CI behavior

- CI always runs host validation.
- CI runs QEMU validation only when a Microkit SDK download URL is configured in `MICROKIT_SDK_URL`.
- When that URL is absent, CI prints an explicit skip reason instead of silently omitting QEMU validation.

## Fault mode transport tests

`test-fault-transport` only proves the shared-byte transport rules:

- mode encoding
- mode decoding
- repeated updates
- invalid wire-value fallback

It does not prove asynchronous delivery ordering or Microkit notification behavior. Those remain the responsibility of `make smoke` and `make test-integration-fault`.
