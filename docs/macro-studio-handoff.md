# Macro Studio: inspection and implementation handoff

**Historical handoff:** the implementation-status statements below predate the
current editor, recorder and playback integration. See [Macro Studio](macro-studio.md)
and the production source/tests for current local and live recording behavior.

**Recovery update (2026-11-05):** see [UI recovery](ui-recovery.md) for the current
UI/routing state, baseline failure evidence, tests and hardware checklist. The
"Next" and proposed-integration sections below are historical plans, not authority
to implement builder/recording/playback during UI recovery. Controller routing
and exclusive-transport tests already exist. Hardware verification is pending.

Status: Macro Studio is **not complete**. A library and details screen
can browse and delete existing saved macros; creation, editing, rename, duplicate, recording,
and playback are not implemented. USB descriptors, keyboard scheduling, and
payload code have not been changed.

## Continuation status

`include/usb_gamepad.h` and `source/usb_keyboard.c` expose an exclusive session.
Entry waits for a neutral completion, one requested state may be outstanding,
and exit waits for a neutral completion. Existing state writers and host echoes
are blocked during ownership; generation fencing drops host OUT requests crossing
an ownership boundary. Errors, timeouts, USB readiness loss, and transport exit
interrupt ownership and request neutral. After reconnection, callers must request
stop and wait for its neutral completion before using passthrough. Completion is
USB-level only, not proof that the target processed the report. This is an opt-in
transport primitive, not a completed application-level arbiter or playback engine.
Integrators must ensure UI, diagnostics, application exit, reset, and passthrough
all use one policy and cannot clear ownership early. Test timing under backpressure
and on hardware before relying on this path.

`include/macro_model.h` and `source/macro_model.c` provide bounded full-report
steps and insert, replace, remove, move, duplicate, and validation operations.
They limit the sequence to 1024 steps and one day, individual steps to one hour,
and reject unsupported button bits, hat values, and vendor data. Metadata,
recording, playback, settings, and the builder screen are still missing.
The storage foundation and version 1 file format are documented in
`docs/macro-storage.md`; the read-only library calls the list and load APIs. Storage failure
injection tests cover save boundaries; actual SD-card/power-loss testing remains
incomplete. `include/macro_editor.h` and `source/macro_editor.c` add a standalone
single-level undo/redo editor model with saved-document dirty tracking. It is not
connected to a screen or to save operations; callers must mark saved only after a
successful write.

Home opens a local three-option Controller menu. Library is accessed from that
menu, not from live passthrough. The former ZL+ZR+L3+R3 shortcut remains removed.
Standard Controller uses the exclusive gamepad session: entry waits for neutral
synchronization and button release, and an ordinary Back/Diagnostics transition
waits for a completed neutral report. If USB is interrupted during exit the
screen waits for reconnection and neutral completion; completion does not prove
the target acted on the report. Forced application shutdown is still best effort.
The library and details screens do not send their navigation input as controller
state. B returns to the local Controller menu.
Home has three cards: CIV Exploit, USB Keyboard, and USB Controller. Standard
Controller offers a touchscreen Exit; the legacy two-second Plus+Minus exit
continues to forward those buttons during the hold. The new Home and footer
styling and previews have been inspected on the host, not on Switch hardware.
`controller_route.c` contains the screen transitions and fresh-button routing used
by the main loop, and `controller_mode.c` owns Standard Controller decisions.
Host tests exercise those production functions with simulated transport states,
including menu isolation, entry gating, exit, disconnect/reconnect and reentry.
The mocked usb:ds regression also exercises production routing with the real
transport code for entry, held activation, passthrough and neutral-confirmed exit.
The Horizon main loop, touchscreen sampling, actual usb:ds and target behavior are
not simulated together; hardware testing is still needed.
The list is bounded to 8192 entries and reports an error above that limit. Details
show readable buttons, D-pad and stick directions, durations and non-neutral axis
values. Deletion requires explicit confirmation. Names and steps are not editable.

Next: add create/edit, rename, duplicate, and builder
screens; wire storage and dirty-exit prompts, and test touch/controller navigation
and save failure behavior. Verify SD recovery; build a
completion-aware playback state machine using this API; implement recorder and
integrate lifecycle and remaining controller/touch UI; then run
the full suite, previews, final build and hardware tests. Do not advertise the
current NRO as containing a working Macro Studio.

## Existing controller path

`source/main.c` reads `PadState` once per loop. On `SCREEN_CONTROLLER`, it
converts physical input using `controller_monitor_report()` and submits state
through the exclusive session. It arms passthrough after buttons are released and
USB synchronization completes. Touch Back, Diagnostics, and the two-second
Plus+Minus shortcut initiate neutral shutdown; the shortcut itself is forwarded
until exit. The diagnostic A test is another state writer outside that session.
Forced application exit still does not wait for a gamepad completion.

`SwitchGamepadReport` is exactly eight bytes: a 16-bit button mask, one hat byte,
four unsigned eight-bit stick axes, and one vendor byte. The implementation maps
Y, B, A, X, L, R, ZL, ZR, Minus, Plus, L3 and R3 to button bits 0 through 11.
The hat is 0 through 7 clockwise from up, or 8 for neutral. Axes center at 128;
the monitor inverts Y and applies its own small deadzone. The other descriptor
button bits do not imply supported physical controls. Motion, analog trigger
pressure, Home, Capture, rumble and NFC are not forwarded by the monitor.

`gamepad_poll()` continuously submits the latest desired report on the existing
gamepad IN endpoint. A state changed while a previous report is in flight may be
superseded before it is ever submitted. Current status exposes pending state but
not which report completed or whether it failed. Both control SET_REPORT and
interrupt OUT also overwrite the desired input report as PicoSwitch-compatible
echoes. `gamepad_send()` is another submission path. Consequently, merely calling
`gamepad_set_state()` from a macro timer would not provide exclusive ownership or
reliable transition delivery. Neutral is currently best effort, not confirmed.

The keyboard has its own interface and scheduling. Preserve it, its 80/80 ms
Live Keyboard timing, and all existing descriptors and USB identities. The main
loop already uses the monotonic system tick via `now_ms()`; USB is pumped every
iteration, and rendering and sleep are separately paced.

## Proposed integration, not yet implemented

1. Complete inspection of the UI/touch routing, USB tests and filesystem behavior
   on the target before changing code. Preserve the existing controller monitor
   and expose Studio from it without stealing passthrough inputs.
2. Add a bounded portable model with validated full controller-state steps and
   durations. Full snapshots encode combinations and neutral unambiguously; editor
   operations can manipulate those snapshots. Define and test exactly how press,
   release, wait and interpolated stick movement map to snapshots. Bound duration,
   total duration, event count, name and description lengths, and undo memory.
3. Add versioned storage and settings under `sdmc:/switch/RecurCIV/`. Strictly
   validate lengths, values, versions and names before allocating or loading.
   Implement temporary-write, flush/close, read-back validation, backup and
   recovery with explicit errors; verify actual libnx/FAT rename and durability
   behavior rather than assuming POSIX replacement or crash atomicity. Define
   import/export paths and collision handling without directory traversal.
4. Implement a nonblocking recorder using monotonic timestamps, bounded sampling
   and capacity. A separate touchscreen-only recording control avoids reserving
   recorded buttons; require release-to-arm so navigation does not enter the
   recording. Pause must remove paused time. Define how held states at pause and
   resume become timeline events and how buffer exhaustion stops safely.
5. Introduce one authoritative controller output arbiter for passthrough,
   recording, playback, pause and neutral transition, including the diagnostics
   writer and both host OUT echo paths. Keep physical input available to UI and
   emergency controls without forwarding it during exclusive playback. Extend
   transport bookkeeping to identify successful completions and failures of the
   actual submitted report, and serialize meaningful transitions; do not post
   competing transfers or alter endpoints/descriptors. Explicitly define timing
   behavior under USB backpressure so events are not silently skipped. A USB
   completion still cannot prove that the target acted on an input.
6. Stop or interrupt playback on USB readiness loss, clear pending macro state,
   and require an explicit restart after reconnection. Neutralize and re-arm on
   ownership changes, including app exit and diagnostics; never claim a neutral
   was delivered if it could not be confirmed. Ensure held physical inputs do not
   leak into passthrough at handoff.
7. Add Studio home, editor, recorder/review, library/details, playback and settings
   screens, using navy cards, white text, cyan accents and gold outline selection.
   Capture screen-specific controls before global shortcuts, including Plus. Guard
   all unsaved changes and ensure the Live Keyboard remains unchanged.
8. Add deterministic tests with simulated clock, physical input and transport,
   including failure, backpressure, disconnect and recovery. Run all existing
   tests and previews, build with `py .\build.py`, and report actual NRO hash and
   size. Hardware verification requires a separate Switch 1 to Switch 2 test.

## Current project test and filesystem notes

`tests/run-tests.cmd` compiles the existing C host tests and preview using MSVC;
Python tests and the optional file test must also be considered separately. The
existing settings in `source/main.c` are simple direct text writes under
`sdmc:/switch/plume-usb/`; they are not a model for crash-safe macro storage.
The existing browser is `.txt`-specific and should not be reused for macro files
without explicit validation and a clearly defined import/export boundary.
