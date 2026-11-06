# Macro timing investigation and correction

## Confirmed findings

The old player added completion-observation latency to **every** step's hold.
This is a confirmed software timing defect, not a diagnosis of all movement
differences in Hades. A successful USB completion is not proof that a game has
processed the report, and exact in-game positions still require hardware testing.

1. **Recording clock:** `main.c` uses monotonic `armGetSystemTick()`, converted
   to integer milliseconds. It timestamps after reading the current `PadState`,
   sticks and touch, before capture/forwarding. These are application sampling
   timestamps, not raw HID event timestamps or USB completion timestamps.
2. **Recorded intervals:** `macro_record_sample()` commits the previous full
   snapshot for `now - changed` at a detected change. `macro_record_stop()`
   commits the final interval. For monotonic sampled input, durations sum exactly
   to Stop time minus recording start, within the existing capacity/time limits.
   Same-millisecond transitions collapse; changes between samples can be missed.
3. **Old playback origin:** `macro_play_start()` acquires exclusive ownership and
   synchronizes neutral. `MP_PENDING` used to set `completed_at` when the normal
   application loop observed READY; `MP_HOLD` then waited the entire scaled
   duration **after that observation** before submitting the next report.
4. **Previous state extension:** the previous controller state remains active
   until the next report actually arrives. Waiting for completion and then a
   full hold therefore adds unnecessary latency. In addition,
   `gamepad_poll()` continuously reposts the old desired state before
   `macro_play_tick()` runs. A newly accepted state may wait behind this old
   refresh and then its own transfer. The fixed 100 ms tests below exhibit
   **two transfer delays of extra hold per step** before the fix.
5. **Accumulation:** restarting each hold after completion makes this overhead
   cumulative rather than just startup/cleanup overhead. The tests reproduce
   100–1,500 ms of accumulated drift across 50–75 ordinary steps.
6. **Neutral:** ordinary step transitions have no automatically inserted
   neutral. Neutral is intentional for initial synchronization, between repeats,
   recorded/edited neutral steps, cancellation and final cleanup.
7. **Analog bytes:** `controller_monitor_report()` maps physical sticks to the
   existing 8-bit axes, inverts Y, and applies its existing small center deadzone.
   The recorder stores that mapped report. Playback copies those exact eight
   saved bytes without another deadzone, interpolation or remapping.
8. **Live versus recorded:** `controller_mode_tick()` forwards only the latest
   mapped state when the exclusive session is READY. `macro_live_tick()` records
   every sampled change even while USB is pending. Consequently a locally
   recorded transition is not necessarily part of the original delivered input.
9. **Scheduling:** sampling, USB polling, macro updates and rendering run on the
   same thread. Rendering is throttled to 10 Hz during active macros, but an
   individual render/framebuffer wait or scheduler stall can delay input sampling
   and playback deadlines. The 1 ms loop sleep is not a guaranteed 1 ms period.
10. **Elapsed versus content:** displayed playback elapsed time starts at
    acquisition, and includes synchronization, delivery delays, scheduling and
    cleanup. It is not expected to equal the sum of scaled stored durations.
    The tests separately measure host-active content and full application elapsed.

## Deterministic measurement method

`tests/test_macro_timing.c` uses the **production player and USB implementation**
with the existing host USB simulator. A fake host consumes each posted transfer
after 1, 2, 5 or 10 ms, independently of when the application next polls it.
This models host delivery at completion, not actual Switch/game processing.
The application order matches production: USB poll, then macro update.

Host-active time runs from arrival of the first saved state until arrival of
final neutral. Every arriving state must match the next saved snapshot exactly;
the test rejects extra neutral, missing/reordered states, and competing writes
to an uncompleted DMA transfer. After the fix, it additionally checks that every
logical submission interval is at least its full requested duration, even after
late ticks/completions. No elapsed-time catch-up is used.

The baseline was run and reported **before changing production playback code**.
The saved pre-fix runtime is retained locally in
`build/timing-before-macro_runtime.c`. Compiling the timing test against that
source with `TIMING_BASELINE` disables only the new timing assertions, not the
report/order/cleanup checks. The same extended scenarios were rerun against both
implementations. Normal `tests/run-tests.cmd` runs the corrected regression.

Per-state intended duration, actual duration and cumulative drift are written to:

- `build/macro-timing-before.csv`, `build/macro-timing-before.log`
- `build/macro-timing-after.csv`, `build/macro-timing-after.log`

Build-directory measurements and the baseline source copy are local artifacts,
not changes to the macro file format or application metadata.

## Before / after results

### Consecutive 100 ms directional states, 1 ms application ticks

All durations below are simulated host-active milliseconds. Parentheses show
drift against the intended content duration. Each individual state was 100 ms
after the fix; before it was 102 / 104 / 110 / 120 ms respectively.

| States | Intended | USB delay | Before | After |
|---:|---:|---:|---:|---:|
| 50 | 5,000 | 1 | 5,100 (+100) | 5,000 (+0) |
| 50 | 5,000 | 2 | 5,200 (+200) | 5,000 (+0) |
| 50 | 5,000 | 5 | 5,500 (+500) | 5,000 (+0) |
| 50 | 5,000 | 10 | 6,000 (+1,000) | 5,000 (+0) |
| 60 | 6,000 | 1 | 6,120 (+120) | 6,000 (+0) |
| 60 | 6,000 | 2 | 6,240 (+240) | 6,000 (+0) |
| 60 | 6,000 | 5 | 6,600 (+600) | 6,000 (+0) |
| 60 | 6,000 | 10 | 7,200 (+1,200) | 6,000 (+0) |
| 75 | 7,500 | 1 | 7,650 (+150) | 7,500 (+0) |
| 75 | 7,500 | 2 | 7,800 (+300) | 7,500 (+0) |
| 75 | 7,500 | 5 | 8,250 (+750) | 7,500 (+0) |
| 75 | 7,500 | 10 | 9,000 (+1,500) | 7,500 (+0) |

### Four-direction square

Sixty mapped analog samples: Up / Right / Down / Left, fifteen slightly different
stick snapshots per direction, exactly 2,000 ms per direction (8,000 ms total).
Fourteen holds of 133 ms and one of 138 ms form each direction.

| USB delay | Before active time | After active time | After individual hold range |
|---:|---:|---:|---:|
| 1 ms | 8,120 ms | 8,000 ms | 133–138 ms |
| 2 ms | 8,184 ms | 8,000 ms | 132–138 ms |
| 5 ms | 8,420 ms | 8,000 ms | 130–140 ms |
| 10 ms | 9,000 ms | 8,000 ms | 130–140 ms |

The residual per-state variation is transfer-phase quantization, not deliberate
shortening of requested durations. Correct total duration does not prove exact
per-direction displacement or exact in-game positions.

### Scheduling, variable latency and transport saturation

| Scenario | Intended | Before active time | After active time |
|---|---:|---:|---:|
| 60 × 100 ms, 5 ms USB, 7 ms application tick | 6,000 | 7,140 | 6,300 |
| 60 × 100 ms, 5 ms USB, 40 ms app blackout each second | 6,000 | 6,650 | 6,030 |
| 60 × 100 ms, USB delays cycle 1/2/5/10 ms | 6,000 | 6,473 | 6,005 |
| 60 × 2 ms rapid analog/button states, 1 ms USB | 120 | 240 | 120 |
| Same, 2 ms USB | 120 | 360 | 240 |
| Same, 5 ms USB | 120 | 600 | 600 |
| Same, 10 ms USB | 120 | 1,200 | 1,200 |

The variable-latency corrected run had individual 100 ms states lasting
91–108 ms at the fake host. All logical submission intervals remained at least
100 ms: differing delivery latencies can shorten or lengthen host-observed holds.

For the 60 × 100 ms, fixed 5 ms USB case, **full application elapsed** changed
from 6,620 ms to 6,020 ms, while host-active content changed from 6,600 to 6,000 ms.
The remaining 20 ms is startup/cleanup/dispatch overhead, not per-step drift.

At saturation, the unchanged continuous-refresh transport can require two URBs
per distinct transition. The correction must not silently omit the states or
manufacture shorter logical holds to force a 120 ms timeline through a 1,200 ms
delivery path. It preserves every state and stretches timing instead. Optimizing
that transport would be a separate compatibility-sensitive change.

### Recording and live sampling

The same sixty physical 100 ms states were sampled independently of USB:

| Sampling scenario | Recorded total | Individual recorded hold range | Maximum change-detection delay |
|---|---:|---:|---:|
| 1 ms ticks | 6,000 ms | 100–100 ms | 0 ms |
| 7 ms ticks | 6,006 ms | 98–105 ms | 6 ms |
| 1 ms ticks with 40 ms periodic blackout | 6,000 ms | 60–140 ms | 40 ms |

The stored durations exactly match the sampled change times, including the
observed Stop time; the recorder does not introduce USB-dependent drift. It
cannot recover the true input history during a blocked application interval.

A production live-coordinator test sampled sixty distinct states at 2 ms each
with 10 ms USB transfers: **60 captured, 6 distinct states delivered**, including
a final drain of the last desired state. Replaying all sixty captured states
cannot exactly reproduce the original six-state delivered trajectory.

## Correction and safety policy

Only the player's timing origin changes. Successful exclusive-session acceptance
sets `submitted_at`; the hold deadline uses that time and the full existing
scaled duration. Observed completion is still tracked but does not restart the
hold. When completion arrives after the deadline, the player can advance one
state in that tick rather than wasting another iteration. The newly accepted
state gets its full duration **from that new submission**, not from an expired
global deadline. There is no fixed latency subtraction, burst catch-up, report
reordering, interpolation, skipped button edge or additional queue.

Pending transfers still gate submissions; existing endpoint ownership, URB IDs,
stale-completion fencing, timeout handling and neutral cleanup are untouched.
Regression cases cover permanently pending transfers through the real timeout,
failed endpoint posts, disconnect/reconnect, cancellation, cleanup retry, speed
rounding, Play once and finite repeats. Reconnection never resumes the macro.

No changes were made to recording, live passthrough, controller mapping, Capture
handling, USB descriptors/endpoints/identity, keyboard, Civ VII, RCMS format,
storage or version metadata.

## Remaining hardware work

Repeat the Hades straight-line and four-direction tests, preferably comparing
known saved step durations as well as positions. Actual USB service timing,
HID polling, render/scheduler gaps, game-frame sampling, stick quantization,
collisions and game mechanics are not measured by the host simulator. A
completion timestamp is not a receiving-game timestamp. The fix removes a
confirmed accumulating software delay; it cannot guarantee deterministic game
movement or accurate replay of transitions omitted during original passthrough.
