# Macro Studio storage foundation

This module is not connected to the UI, recorder, or playback. The caller creates
the default hierarchy with `macro_store_prepare_default()`, whose library directory
is `sdmc:/switch/RecurCIV/macros/`. Store operations accept a root argument to
allow host testing. Callers must serialize access: concurrent writers to the
same library are not supported. Do not pass an untrusted root path.

Names have 1–63 ASCII letters, digits, spaces, `_` or `-`; leading and trailing
spaces are forbidden. File keys are lowercased ASCII names followed by `.rcm`;
names differing only in case collide. Separators, dots and traversal are rejected.
Descriptions are at most 255 ASCII characters, with printable characters, tab and
LF allowed. The directory listing returns sorted lowercase keys; load a file to
obtain its display name and metadata. A list can include corrupt files, so callers
must validate each entry before displaying metadata.

## Binary format version 1

All integers are unsigned little-endian. File size is exactly `364 + 12 * count`
bytes, at most 12,652 bytes. Fixed strings are NUL-terminated and zero-padded.

| Offset | Size | Contents |
| --- | ---: | --- |
| 0 | 4 | ASCII `RCMS` |
| 4 | 4 | Version, 1 |
| 8 | 4 | Step count, 0–1024 |
| 12 | 4 | Speed percentage: 25, 50, 100, 150, or 200 |
| 16 | 4 | Repeat count, 0 for continuous; maximum 1,000,000 |
| 20 | 4 | Delay between loops in ms, 0–3,600,000 |
| 24 | 8 | Created Unix seconds, 0 if unavailable |
| 32 | 8 | Modified Unix seconds, 0 if unavailable |
| 40 | 64 | Name |
| 104 | 256 | Description |
| 360 | 12 × count | Step records |
| end − 4 | 4 | CRC-32/ISO-HDLC of all preceding bytes |

Each record contains the eight HID state bytes (16-bit button mask, hat, LX,
LY, RX, RY, vendor) followed by a 32-bit duration in milliseconds. The model
validates button bits 0–11, hat 0–8, vendor zero, a nonzero duration up to one
hour, and total duration up to one day. Analog bytes cover the full 0–255 range.
CRC detects accidental damage, not malicious changes or authenticity. Unknown
versions, noncanonical strings, truncation, extra data and invalid steps fail
validation; failed decode leaves the caller's document unchanged. The file
validator accepts an explicit external path for import validation but does not
copy or execute it.

## Save and recovery

New and replacement saves create an exclusive `.tmp`, write, flush and fsync it,
close it, reopen and validate it, then compare its canonical encoding with the
source. Replacement first checks that the old `.rcm` is valid, renames it to
`.bak`, and renames `.tmp` to `.rcm`. It removes `.bak` only after the new file
is in place. An existing `.tmp` or `.bak` blocks new saves and deletion.

`macro_store_recover` restores a valid backup if the destination is missing, or
promotes a valid temporary file if neither destination nor backup exists. If a
valid destination and staged temporary file coexist, it leaves the temporary file
in place and reports `MACRO_STORE_RECOVERY_REQUIRED`: copy the files elsewhere,
validate them, decide which data to keep, then move the unwanted temporary file
out of the library. Invalid destination or backup files are not automatically
overwritten. If a valid destination and backup coexist without a temporary file,
recovery removes the backup after validating both. Rename uses a new save followed
by deletion of the old name; if deletion fails, both copies can remain and the
operation returns an error.

This is a backup/recovery protocol, not a guarantee of atomic power-loss behavior:
filesystem metadata and directory durability depend on Horizon and the SD card.
The code uses `fsync`, but target-card behavior and recovery under real power loss
have not been hardware verified. Keep independent backups of valuable macros.
The current implementation has no portable directory-fsync operation for libnx
or the Windows test harness; it does not claim directory synchronization.
`MACRO_STORE_FULL` reports ENOSPC when observed during temporary creation or
writing, flushing, fsync or either replacement rename. A `.tmp` may remain after
such a failure and must be resolved before retrying; retain it for inspection.
Other interrupted-write failures return `MACRO_STORE_RECOVERY_REQUIRED`.

Host tests compiled with `MACRO_STORAGE_TESTING` inject failures at temporary
creation, partial write, flush, fsync, read-back, backup rename, replacement rename
backup cleanup, and a failed replacement followed by failed rollback. These hooks
are absent from normal builds. They simulate API
failures, not actual card removal or power loss.
