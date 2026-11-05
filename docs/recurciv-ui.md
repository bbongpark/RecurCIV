# CIV Exploit UI (Phase 3)

**Hardware / Phase 4 update:** the user reports successful full 87-message execution
on Switch 1 → Switch 2, reaching CivSwitch's receiver/waiting screen. The existing
automation path is preserved. [Phase 4 notes](phase4-sleep-ui.md) describe temporary
auto-sleep prevention and keyboard UI polish; the earlier hardware checklist below
is retained as historical validation guidance, not a claim that the run failed.

Home > CIV Exploit now opens CIV VII AUTOMATION. The legacy internal
`SCREEN_CIV_PLACEHOLDER` enum is retained for routing compatibility; it renders
the dedicated automation screen, not a placeholder. All screens use the existing
View router, shared RecurCIV header/footer, navy cards, slate borders and gold focus.

## Preparing and starting

The user must manually prepare Civilization VII on Switch 2:
Multiplayer > Wireless > Create Game > Host Lobby, chat focused, English-US keyboard.
RecurCIV does not navigate these menus. The approved minimum schedule is about
16m 33s; USB completion waits and UI loop cadence make real execution longer.

Use D-pad/left stick to select START PAYLOAD or BACK, then A; B goes Back when
safe. Footer controls are also touch targets. There is no startup, screen-entry,
held-button, completion, or reconnect auto-start. A fresh action is required.

`civ_route.c` is UI routing/presentation policy, not another scheduler. Start
checks current ownership, application availability, controller exclusivity,
pending keyboard work, the existing runner preflight (an unticked validation-only
instance), and actual USB readiness. It then calls `civ_usb_start()` exactly once.
Only an accepted request becomes the active display. A rejected request stays on
the preparation screen with its error. A start accepted with blocked acquisition
fencing shows cleanup-required state; acceptance does not promise payload delivery.

The payload, Phase 1 scheduler, Phase 2 adapter and low-level USB implementation
are unchanged. No TCP sender or installer is started by this UI.

## Running, cancellation and navigation

The main loop continues keyboard/gamepad polling and adapter ticking before UI
input/rendering. The owned/CIV branch routes only CIV controls, not normal modes,
diagnostics, background idle sends or passthrough. Input stays local.

The screen shows actual message/character progress, stage, elapsed MM:SS and a bar
based exclusively on completed messages. No payload content is rendered or logged.
Elapsed time includes acquisition and cleanup and freezes when the lease is released.

Cancel by touching CANCEL or holding **L + R + Minus for one second**. A and B do
not cancel a running sequence. Back, diagnostics, other modes, exit shortcuts,
and duplicate starts are swallowed while owned. Cancel calls only the existing
adapter cancellation API; the display remains on CIV until neutral cleanup.

Cleanup failure/fencing blockage shows USB CLEANUP REQUIRED and RETRY CLEANUP
(touch or A after releasing previous input). Retry invokes only the existing
cleanup-only API, never payload replay. Disconnect waits for reconnection and
neutral cleanup; it never resumes input. Menu navigation remains locked until
actual ownership is released. Then A/B or touch Back returns Home. Another run
requires returning to the preparation screen and explicitly starting again.

Touch and controller actions share one per-loop arbitration point. Controller
actions take precedence over a simultaneous touch. Held actions/contacts are
release-gated across screen and run-state changes; multitouch cannot activate.

## Outcomes and error precision

Only FINISHED plus confirmed neutral cleanup and released ownership is shown as
ALL SUBMISSIONS SENT. The display asks the user to verify the receiver/waiting
screen on Switch 2; it does not claim exploit success or in-game acceptance.

Start rejection distinguishes not-ready, conflicting/pending work, preflight
rejection, adapter rejection, and restart-required state. Runtime errors expose
the actual `CivRunnerError` and `UsbCivResult` numeric values. Phase 2 does not
expose a distinct timeout cause: the UI honestly groups it as USB TRANSFER FAILURE
/ TIMEOUT rather than inferring a cause or adding a competing timeout scheduler.

The existing forced-shutdown API is terminal until process restart and never
claims neutral delivery. Its exposed `shutdown_unresolved` condition maps to
APPLICATION RESTART REQUIRED with no false Retry/Back recovery. OS-forced exit
closes the process; no promise is made that a new frame can be shown during exit.

## Verification and previews

`tests/test_civ_ui.c` uses production routing, rendering, adapter, scheduler and
USB implementation with the deterministic usb:ds simulator. It covers starts,
held input/touch arbitration, conflicts, stage/progress display, local cancellation,
disconnect, timeout/transfer faults, blocked cleanup/retry, completion, safe Back,
and restart-required state. Rendering assertions check shared chrome, focus,
footer hints, hitboxes and message-based bar geometry. Main-loop checks remain
structural; the complete Horizon event/render/USB loop is not simulated.

`tests/preview.c` emits these 1280x720 previews in `build/`:

- `civ-initial.ppm`
- `civ-running.ppm`
- `civ-mid-progress.ppm`
- `civ-cancellation-pending.ppm`
- `civ-cancelled.ppm`
- `civ-completed.ppm`
- `civ-usb-not-ready.ppm`
- `civ-transfer-failure.ppm`
- `civ-cleanup-unresolved.ppm`
- `civ-retry-cleanup.ppm`

PNG copies can be made with Pillow; previews contain synthetic presentation data,
not evidence of a hardware run. Both the host suite and the original PicoSwitch
oracle comparison must pass before distributing a build. Payload/metadata tests
must be run against the final NRO, whose size/hash identify the tested artifact.

## Hardware checklist (not yet performed)

1. Verify Home/Start focus, left-stick/D-pad, A/B, touch targets and release gates.
2. Verify not-ready/conflict rejection and that local Start is not sent to Switch 2.
3. Prepare the game exactly as above; explicitly start and monitor message/stage
   progress while confirming the actual keyboard/controller sequence is accepted.
4. Cancel by touch and by the local chord at several stages. Verify no further
   payload progression, neutral delivery, and ownership-safe return to Home.
5. Disconnect/suspend/reset during typing and cleanup; verify no auto-resume,
   blocked navigation, cleanup-only retry, and safe use of ordinary modes afterward.
6. Verify all 87 submissions and the expected receiver/waiting screen separately.
   Do not treat USB completion alone as successful exploit execution.
7. Verify forced exit behavior and restart if cleanup could not be confirmed.

Actual usb:ds IPC latency, one-shot gamepad delivery, target layout, game acceptance,
render cadence and end-to-end runtime still require hardware verification.
