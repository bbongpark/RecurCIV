# CIV USB integration (Phase 2)

**Phase 3 update:** [CIV UI integration](recurciv-ui.md) now connects the explicit
START PAYLOAD action and local cancellation/recovery controls. The no-UI/no-caller
statements below describe the Phase 2 checkpoint; transport behavior is unchanged.

`civ_usb_init(civ_can_start)` initializes a main-thread singleton. It does not
start automation. No production caller of `civ_usb_start()` exists in this phase;
the CIV screen remains unchanged. Phase 3 must add an explicit, reviewed action.

## Ownership and scheduling

`civ_usb_start()` accepts an explicit request for the approved embedded payload.
The application's eligibility callback rejects active senders (Compose/files or
Caps Lock), Live Keyboard, Standard Controller, diagnostics, the gamepad test,
pending composition, input lock, and application exit. The USB lease additionally
rejects a pending keyboard URB or any Standard Controller exclusive session.
It fences the previous continuous controller stream before the runner starts.
A failed acquisition fence retains ownership and allows only cleanup recovery.

While owned, the main loop still calls `keyboard_poll`, `gamepad_poll`, then
`civ_usb_tick`, and renders normally. It does not run competing senders, controller
routing, diagnostics/reset actions, modal applets, or idle keyboard reports.
Local input is consumed; held input must be released before normal actions resume.
Transport-level guards also reject ordinary keyboard/gamepad sends, cancellation,
initialization/exit, controller-exclusive calls, and host output echo updates.

## Transactions and cleanup

`usb_civ_submit` copies the complete keyboard/controller state into the existing
separate aligned DMA buffers. The adapter maps a CivRunner ticket to one actual
IN URB per interface. Only ID-matched, successful eight-byte completions set its
completion bits. During the lease the controller is one-shot, not automatically
reposted; HID state remains held until the next state pair. The scheduler waits
for both completions, then the unchanged Phase 1 minimum interval.

Partial submissions are ambiguous failures, never replayed. Short/failed reports,
report-read failures, and pending transfers older than 2,500 ms stop progression.
Disconnects, resets, suspend, and USB state-read failures are sticky interruptions,
even if USB recovers before the next runner tick. Reconnect permits cleanup only.

Cancellation fences both IN endpoints using Cancel and report draining before
buffer reuse. No event wait, sleep, or catch-up loop is added. These calls use the
existing synchronous usb:ds IPC API; real hardware IPC latency remains unmeasured.
Failed Cancel/drain retains the buffer and lease. A matching completion may prove
the fence, or `civ_usb_retry_cleanup()` may explicitly retry fencing/neutral cleanup.
Failed cleanup submission/completion is blocked until that explicit retry.

Ownership is released only after the runner's terminal neutral transaction has
completed on both interfaces. New runs require a separate explicit start; tickets
are preserved across runs. `civ_usb_progress()` exposes acquisition, runner status,
fence blockage, and forced-shutdown uncertainty. During acquisition the runner has
not started yet; its progress is represented as cleanup/synchronization.

On OS-forced application exit, `civ_usb_shutdown()` records unresolved neutral
delivery and closes usb:ds as the final buffer-lifetime fence. It never claims
neutral completion. The service is then terminal until process restart.

## Validation and Phase 3

Host tests link the real adapter, runner, and USB implementation against simulated
usb:ds completions/failures. They cover complete report copies, partial completions,
timeouts, stale URBs, all-stage cancellation, reset/reconnect, failed fencing,
cleanup retry, safe re-entry, forced exit, and the complete approved payload.
The actual posted report stream is compared to the independent Phase 1 oracle.
Main-loop ownership/activation checks are structural, not a simulated libnx GUI.

Phase 3 must add explicit start/cancel, progress and blocked-cleanup controls;
retain polling/ownership while exiting or recovering; and explain that USB
completion is not in-game acceptance or receiver readiness. Hardware verification
of USB readiness, one-shot controller delivery, timing, cancellation, and
disconnect/cleanup remains required before trusting a full automation run.
No descriptors, payload bytes, other keyboard timing, or controller-mode semantics
are changed by this integration.
