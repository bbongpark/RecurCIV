# Controller Macro Studio

Open **USB Controller → Macro Studio** to build or record ordinary controller macros. Open **Macro Library** to load a saved macro, play it, edit it, rename it, duplicate it, or confirm deletion.

## Editing

- Navigate controls with the D-pad/left stick and activate with A, or touch them. B goes back.
- **Previous step / Next step** select the action (gold outline); touching an action card also selects it.
- **Insert action / Insert delay** add after the selected step. Delay starts with a neutral report. The action screen supports simultaneous buttons, eight-way D-pad, all four stick axes, and a hold duration. **Apply action** accepts the draft; **Cancel action / B** leaves the sequence unchanged.
- Use **Remove action**, **Move up/down**, and the existing one-level **Undo / Redo** history.
- **Repeats** accepts 1–1,000,000. **Speed** cycles 25/50/100/150/200%. Numeric and name entry use the Switch software keyboard.
- **Save** writes through the existing safe-save API. Saving to the loaded name updates that file; choosing another name creates a separate file and never silently overwrites a collision. Cancelled/invalid dialogs retain edits.
- Leaving an unsaved editor or a nonempty stopped recording opens a discard confirmation, initially focused on **Keep editing**. Saved files are unchanged by discard. Delete likewise initially focuses Cancel.

## Local recording

1. Choose **Local Record**, then **Start recording**.
2. Release all buttons, sticks and touch to arm. The Start press is excluded.
3. Perform controller inputs. The screen shows elapsed time and completed states.
4. **Touch Stop** to finish, then **Review** to open the captured sequence in the editor. Save only after reviewing it.

All supported button combinations, D-pad directions and both sticks are recorded as full eight-byte states using the existing controller mapping/deadzone. All physical buttons, including B, Plus and Minus, belong to the recording; none acts as a hidden stop chord. Stopping therefore requires touchscreen access. Inputs are **not forwarded** to USB.

Recording stops at 1,024 states, ten minutes, or controller disconnect; retained states remain reviewable. Holds and neutral intervals use sampled state-change timestamps. Transitions within the same millisecond collapse instead of producing invalid zero-length steps. Analog movement/jitter can consume the state limit quickly. Sampling and rendering latency bound capture precision; this is not a hardware-timestamped input trace.

## Live recording

1. Choose **Live Record**. This immediately acquires a persistent controller
   session, **without recording**. A compatible USB host must be connected and
   ready, with no other exclusive controller owner.
2. Wait for the existing exclusive USB controller session to finish neutral
   synchronization. Release buttons, D-pad, **both** sticks and touchscreen.
   Physical-input forwarding begins after this entry safety gate passes.
3. Press physical **Capture** briefly, or touch **Start recording**. The existing
   recorder captures mapped physical snapshots/durations while passthrough runs.
   Recording starts with the next physical sample, including any held gameplay
   buttons/sticks; it does not require another neutral release.
   All supported controller buttons, including B, Plus and Minus, remain macro
   content. There is no Plus+Minus exit shortcut in live recording.
4. Press **Capture** again, or touch **Stop recording**. Capture is finalized;
   **passthrough continues**, before, during and after recording. Recording limits
   stop capture, not passthrough. Gameplay buttons on a Stop frame may still be
   forwarded normally; Capture itself can never enter reports or macro steps.
5. Starting another recording offers **Discard + record / Keep capture** if
   unsaved captured states exist. Only an explicit touchscreen decision can
   discard them; further Capture notifications cannot confirm the prompt.
6. Touch **Exit live**. This stops any active recording, requests neutral USB
   output, and retains ownership until cleanup completes. Normal Capture handling
   is restored on exit, including when an interruption forces cleanup.
7. After cleanup completes, choose **Review**, edit as needed, and **Save**.
   Load and replay it through the existing Macro Library/playback workflow.

The entire live session, including idle passthrough and cleanup, blocks navigation,
editing, saving, playback, USB diagnostics/reset and software-keyboard dialogs.
Physical gameplay buttons never navigate, confirm replacement, or exit live.
Touchscreen Exit live remains available during synchronization/release-to-arm.
During failed cleanup, **touch Retry cleanup**; it retries neutral output only.
You need touchscreen access throughout this workflow, including recovery.

USB loss, physical controller disconnect or transport failure stop capture and
enter cleanup without discarding retained states. Reconnection never restarts
recording or passthrough automatically. Clock regression stops capture without
erasing it; normal limits leave passthrough active until Exit live.
Cleanup waits for a ready host; a failed neutral attempt stays locked until an
explicit retry succeeds. Forced system/application exit remains best effort,
not proof of neutral delivery.

### Capture support and fallback

Capture is **not** a `HidNpadButton` and is never read through `PadState`.
The app uses the supported libnx
`appletSetRequiresCaptureButtonShortPressedMessage(true)` /
`AppletHookType_OnCaptureButtonShortPressed` API (Horizon 3.0.0+), with its own
hook cookie. A lightweight callback queues one coalesced notification; only the
normal application loop changes recording state. Other applet hooks are untouched.

The notification represents a completed **short press**, not down/held/up state.
It diverts short presses from the screenshot overlay during live sessions.
Long-hold/video-capture behavior has not been verified and is not a recording
control. No raw `hidsys` system-button activation is used.

Queued events are cleared at lifecycle/focus/confirmation/touch-control boundaries;
events older than 500 ms are dropped. Entry and touch controls have a 300 ms quiet
gate, and notifications less than 300 ms apart are debounced. Very rapid deliberate
Start/Stop presses are therefore ignored. Events are timed at callback dispatch,
not at the physical press; OS delivery latency still requires hardware testing.

Unsupported applet environments or registration failures show a visible warning
and preserve touchscreen Start/Stop. Capture restoration failures show a warning
and are retried outside the live session; restart the application if unresolved.
There is no getter for a previous Capture-notification setting: RecurCIV only
enables its own request during live sessions and disables that request on exit.

**Recorded time is physical sampling time, not USB completion time.** Capture
continues while a USB report is pending. Passthrough still sends the latest
state when available, without a new queue: a brief locally captured transition
may never be delivered to the USB host. A recording is therefore not a USB
delivery log, and replay's completion-limited timing may differ from the original
live interaction. USB completion itself does not prove the game processed input.

Local-only recording remains available without acquiring live USB ownership.
Both options use the same controller mapping, 8-bit stick precision/deadzone,
snapshot model, 1,024-state/ten-minute capture limits and RCMS v1 storage.

## Playback

- **Play once** runs exactly one iteration without changing saved repeat metadata. **Play repeats** uses the configured finite count. Legacy files with repeat count zero still load unchanged; choose Play once or edit to a finite count before repeated playback.
- Playback reuses the existing exclusive controller interface. Each full logical hold is timed from successful state acceptance into that session, not from observing completion. Completion still gates the next state: there are no competing submissions, skipped transitions or compressed catch-up holds. Durations scale by speed and round upward to milliseconds. Neutral is sent between iterations with the existing unscaled loop-delay metadata; no extra neutral is inserted between ordinary steps. Empty/invalid macros cannot play.
- Progress shows action, iteration and elapsed time (including synchronization and cleanup). The displayed minimum duration excludes USB/scheduling overhead. The main loop keeps polling USB without blocking waits; active recording/playback redraw at 10 Hz to reduce interference. Late ticks/transfers can stretch the logical timeline. Variable delivery latency can make an individual host-observed hold shorter or longer even though the requested logical duration is never shortened. See [timing investigation and measured limits](macro-timing.md).
- **A / B or touch Stop** cancels. Other navigation/writers remain suppressed while playback owns the controller. Exit is enabled only after the neutral cleanup report completes.
- Disconnect/transfer interruption never resumes macro actions. Cleanup waits for reconnection. If cleanup fails, ownership stays locked and **Retry cleanup** retries neutral only, not playback.
- Forced application/system exit can only request best-effort neutral release; successful delivery cannot be guaranteed if the USB service is terminated or disconnected. Do not forcibly exit with held inputs when normal Stop/cleanup is available.

## Compatibility and verification

The model, RCMS v1 codec, safe-save/recovery logic, storage root (`sdmc:/switch/RecurCIV/macros`) and saved user data format are unchanged. Existing descriptions, timestamps, speed, repeat and loop-delay metadata are retained. Normal model limits remain 1,024 steps, one hour per hold, and 24 hours per sequence.

`tests/test_macro_runtime.c` covers recording bounds/timing, combinations/sticks, speed scaling, submission-relative holds with completion/order gates, finite repeats, Play once, cancellation, interruption and cleanup retry. `tests/test_macro_timing.c` measures production USB playback with 50–75 states, fixed/variable transfer latency, delayed ticks, render stalls, rapid analog changes and transport faults. It also checks recording timestamps and skipped live-passthrough transitions. `tests/test_macro_app.c` exercises actual UI/control routing, file dialogs/operations, invalid files, local-only capture, save collisions/discard safety, and the existing asynchronous USB simulator (including stale completions, cleanup timeout/retry and disconnect). Existing model/editor/storage tests continue to cover codec rejection, editing and recovery. `tests/preview.c` generates macro screen previews.

`tests/test_macro_live.c` exercises the coordinator against a deterministic
completion-driven transport, including full neutral/release gating, combinations,
sticks, sampled timing under backpressure, conflicts, interruption, limits and
cleanup/retry. The application integration test also covers the complete live
capture/stop/review/edit/save/load/replay flow against the production USB mock.
`tests/test_capture_control.c` covers registration/failure, bounded coalescing,
debounce/stale/focus/session gates, restoration retry and independent hook
preservation. Integration tests feed actual queued Capture callbacks through
the application controls and verify touchscreen fallback and passthrough
continuity before/during/after recording, including replacement confirmation.
Routing/raster tests cover the separate Local Record and Live Record choices
and synchronization/cleanup/retry controls. The timing correction does not change
the USB transport, controller mapping, Capture controls or saved macro format.

Host simulation and a successful NRO build do not replace on-device testing. Switch touchscreen/software-keyboard behavior, controller capture cadence, and receiving-device timing still require hardware verification.
