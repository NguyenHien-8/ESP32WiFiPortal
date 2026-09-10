# 24/7 validation record

Validation date: 2026-09-10 (Asia/Saigon)

## Automated results

| Area | Environment | Result |
| --- | --- | --- |
| Host functional suite | MSVC 19.44, C++14, `/W4` | 6/6 passed |
| Repeated host suite | 6 executables x 100 runs | 600/600 passed |
| Portal lifecycle stress inside repeated suite | Fake DNS/WebServer/SoftAP | 50,000 start/stop cycles passed |
| Rejected portal-IP update stress | Host fake runtime | 50,000 attempts passed; previous value preserved |
| Memory safety | MSVC AddressSanitizer | 6/6 suites passed |
| Arduino compile matrix | Arduino CLI 1.5.1, `esp32:esp32@3.3.11`, FQBN `esp32:esp32:esp32`, `--warnings all` | 8/8 targets, 3/3 runs each passed |
| UBSan | Host environment | Not available in the installed compiler toolchain |

The Arduino matrix covers all six examples plus
`extras/hardware/PortalCustomIPSmoke` and
`extras/hardware/Portal24x7Soak`. The final soak sketch build uses 980,156 bytes
(74%) of flash and 47,352 bytes (14%) of global RAM on the generic ESP32 target.

## Host coverage

- CRC32 standard vector, deterministic record serialization, exact maximum
  SSID/password sizes, and open-network password handling.
- Bad magic, version, encoded size, lengths, CRC, truncated record, and short
  NVS read/write rejection.
- One-key blob write, exact read-back before RAM cache commit, and no reconnect
  attempt with a corrupt record.
- Legacy migration, interruption during blob write, interruption after verified
  write but before legacy cleanup, and retry from the complete legacy pair.
- Explicit erase of `cred_blob`, `ssid`, and `pass`.
- 10-second, 15-second, and zero-normalized connection timeouts; retry burst,
  capped cooldown, successful recovery, and `millis()` wrap-around.
- Advanced manual Portal route, exact whitespace-preserving SSID handling,
  browser markup/behavior invariants, server-side boundaries, double-submit
  guard, password-storage check, and fixed-shape progress animation.
- Existing custom Portal IP validation, DNS/HTTP lifecycle, cleanup, restart,
  and multi-translation-unit header behavior.

## 72-Hour Hardware Test

Use `extras/hardware/Portal24x7Soak/Portal24x7Soak.ino` on the target board. It
prints one telemetry record per minute with 64-bit uptime, Wi-Fi status,
disconnect/connect/reconnect counts, accumulated downtime, Portal starts,
current/minimum heap, largest free block, reset reason, and credential CRC state. Serial commands
are `p` (open Portal), `s` (stop Portal), `e` (erase credentials), `t`
(immediate telemetry), and `r` (restart).

Before declaring a hardware result, retain complete serial logs for all of these
scenarios:

1. Initial provisioning and successful connection.
2. Wrong password submission followed by recovery with correct credentials.
3. Multiple router-off/router-on cycles across retry burst and cooldown.
4. Repeated Portal open/close cycles while monitoring minimum heap and largest
   block.
5. Hard reset during **Connect and save**, followed by verification that boot
   selects a complete old/new record or rejects corruption—never mixed fields.
6. At least 72 continuous idle hours after the fault scenarios, with no
   unexpected reset, reconnect storm, monotonic heap loss, or invalid CRC.

The sketch emits `SOAK_72H_DURATION_REACHED` when duration alone is met. This is
not an automatic PASS; the complete scenario log still needs review.
