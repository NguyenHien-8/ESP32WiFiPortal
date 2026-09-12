# 24/7 validation record

Validation date: 2026-09-12 (Asia/Saigon)

## Automated results

| Area | Environment | Result |
| --- | --- | --- |
| Host functional suite | MSVC 19.44, C++11, `/W4 /permissive-`, Debug + Release | 8/8 passed in each configuration |
| Release assertion integrity | Always-on host test checks under `NDEBUG` | 8/8 passed; side-effecting checks were executed |
| Repeated host suite | 8 executables x 100 Release runs | 800/800 passed |
| Portal lifecycle stress inside repeated suite | Fake DNS/WebServer/SoftAP | 50,000 start/stop cycles passed |
| Rejected portal-IP update stress | Host fake runtime | 50,000 attempts passed; previous value preserved |
| Memory safety | MSVC AddressSanitizer | 8/8 suites passed; leak detection is not supported by this Windows ASan runtime |
| ESP32 compile matrix | PlatformIO Core 6.1.19, Espressif32 7.0.1, Arduino-ESP32 2.0.17, `esp32dev` Release | 8/8 sketches passed; `AdvancedSTA` also passed with `-Wall -Wextra -Wpedantic` |
| UBSan | Host environment | Not available in the installed MSVC toolchain |

The ESP32 matrix covers all six examples plus
`extras/hardware/PortalCustomIPSmoke` and
`extras/hardware/Portal24x7Soak`. The final soak sketch build uses 846,089 bytes
(64.6%) of flash and 45,756 bytes (14.0%) of global RAM on the generic ESP32 target.

## Host coverage

- CRC32 standard vector, deterministic record serialization, exact maximum
  SSID/password sizes, and open-network password handling.
- Bad magic, version, encoded size, lengths, CRC, truncated record, and short
  NVS read/write rejection.
- Exact read-back before RAM cache commit, unchanged-value no-op writes, and no
  reconnect attempt with an unusable record.
- Transactional update interruption while writing the backup, while writing
  the primary, and after primary commit but before backup cleanup. Reboot always
  selects a complete old/new record and repairs a partial primary from backup.
- Legacy migration, interruption during blob write, interruption after verified
  write but before legacy cleanup, and retry from the complete legacy pair.
- Explicit erase of `cred_blob`, `cred_backup`, `ssid`, and `pass`.
- Open network, 8-63-byte passphrase, exact 64-digit hexadecimal raw PSK, and
  invalid 64-byte non-hex/65-byte password boundaries.
- 10-second, 15-second, and zero-normalized connection timeouts; retry burst,
  capped cooldown, successful recovery, and `millis()` wrap-around.
- Advanced/manual/properties Portal navigation, exact whitespace-preserving SSID
  handling, runtime property JSON, browser markup/behavior invariants,
  server-side boundaries, double-submit guard, password-storage check, and
  fixed-shape progress animation.
- Asynchronous scan start/poll/completion, JSON escaping, duplicate filtering,
  immediate failure, timeout across `millis()` wrap, and active-scan cleanup.
- RAM-only core Wi-Fi storage, hostname-before-mode ordering, checked
  mode/storage/event-policy failures, SoftAP-stop fallback, single-owner
  enforcement for the global `WiFi` object, and restoration of core reconnect.
- Coalesced disconnect events retain an authentication failure even when the
  latest reported reason is non-terminal.
- POST-only reset acknowledgement, deferred non-blocking restart, repeated
  request idempotence, credential preservation, and `millis()` wrap-around.
- Existing custom Portal IP validation, DNS/HTTP lifecycle, cleanup, restart,
  and multi-translation-unit header behavior.

## Pending 72-Hour Hardware Test

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

The sketch reports `credential_crc=backup-valid` when an interrupted primary
update still has a recoverable verified backup. It emits
`SOAK_72H_DURATION_REACHED` when duration alone is met. This is not an automatic
PASS; the complete scenario log still needs review. No 72-hour hardware run was
performed as part of the 2026-09-12 source audit.
