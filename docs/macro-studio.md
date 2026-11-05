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

1. Choose **Record Inputs**, then **Start recording**.
2. Release all buttons, sticks and touch to arm. The Start press is excluded.
3. Perform controller inputs. The screen shows elapsed time and completed states.
4. **Touch Stop** to finish, then **Review** to open the captured sequence in the editor. Save only after reviewing it.

All supported button combinations, D-pad directions and both sticks are recorded as full eight-byte states using the existing controller mapping/deadzone. All physical buttons, including B, Plus and Minus, belong to the recording; none acts as a hidden stop chord. Stopping therefore requires touchscreen access. Inputs are **not forwarded** to USB.

Recording stops at 1,024 states, ten minutes, or controller disconnect; retained states remain reviewable. Holds and neutral intervals use sampled state-change timestamps. Transitions within the same millisecond collapse instead of producing invalid zero-length steps. Analog movement/jitter can consume the state limit quickly. Sampling and rendering latency bound capture precision; this is not a hardware-timestamped input trace.

## Playback

- **Play once** runs exactly one iteration without changing saved repeat metadata. **Play repeats** uses the configured finite count. Legacy files with repeat count zero still load unchanged; choose Play once or edit to a finite count before repeated playback.
- Playback reuses the existing exclusive controller interface. A hold begins only after its report completes; durations scale by speed and round upward to milliseconds. Neutral is sent between iterations, followed by the existing (unscaled) loop-delay metadata. Empty/invalid macros cannot play.
- Progress shows action, iteration and elapsed time (including synchronization and cleanup). The displayed minimum duration excludes USB/scheduling overhead. The main loop keeps polling USB without blocking waits; active recording/playback redraw at 10 Hz to reduce interference. Rendering, polling and USB latency may lengthen intervals, never intentionally shorten their requested dwell.
- **A / B or touch Stop** cancels. Other navigation/writers remain suppressed while playback owns the controller. Exit is enabled only after the neutral cleanup report completes.
- Disconnect/transfer interruption never resumes macro actions. Cleanup waits for reconnection. If cleanup fails, ownership stays locked and **Retry cleanup** retries neutral only, not playback.
- Forced application/system exit can only request best-effort neutral release; successful delivery cannot be guaranteed if the USB service is terminated or disconnected. Do not forcibly exit with held inputs when normal Stop/cleanup is available.

## Compatibility and verification

The model, RCMS v1 codec, safe-save/recovery logic, storage root (`sdmc:/switch/RecurCIV/macros`) and saved user data format are unchanged. Existing descriptions, timestamps, speed, repeat and loop-delay metadata are retained. Normal model limits remain 1,024 steps, one hour per hold, and 24 hours per sequence.

`tests/test_macro_runtime.c` covers recording bounds/timing, combinations/sticks, speed scaling, completion-gated holds, finite repeats, Play once, cancellation, interruption and cleanup retry. `tests/test_macro_app.c` exercises actual UI/control routing, file dialogs/operations, invalid files, local-only capture, save collisions/discard safety, and the existing asynchronous USB simulator (including stale completions, cleanup timeout/retry and disconnect). Existing model/editor/storage tests continue to cover codec rejection, editing and recovery. `tests/preview.c` generates macro screen previews.

Host simulation and a successful NRO build do not replace on-device testing. Switch touchscreen/software-keyboard behavior, controller capture cadence, and receiving-device timing still require hardware verification.
