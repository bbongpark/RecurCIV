# Phase 4: automatic sleep protection and keyboard UI

## Preserved hardware baseline

The user reports the complete 87-message CivSwitch payload ran successfully on
real Switch 1 and Switch 2 hardware, displaying:

```
Got unstable rw primitive
Got stable rw primitive
ROP setup finished
CivSwitch 1.0 by Gezine
network down - waiting for network..
```

This is user-supplied hardware verification of the existing implementation, not a
new hardware test performed by the coding agent. Phase 4 does not change payload
bytes, scheduler/automation timing, the USB adapter, descriptors/identity/endpoints,
controller passthrough, Live Keyboard pacing, or Macro Studio storage.

## Auto-sleep API investigation

Installed `C:/devkitPro/libnx/include/switch/services/applet.h`, lines 868–880,
declares `Result appletSetAutoSleepDisabled(bool)` and
`Result appletIsAutoSleepDisabled(bool *)`, both requiring firmware 5.0.0+.
Upstream libnx's `nx/source/services/applet.c` implements them as checked
ISelfController commands 68/69, not application-only functions. SelfController is
obtained in both application and library-applet initialization. The implementation
checks firmware/service availability and returns errors; mode or permission success
is never assumed. No sleep lock, system sleep-disable-until-shutdown, fake input,
manual-sleep interception, wait loop, or global system-setting change is used.

`civ_sleep.c` is a small guard around the existing USB lease. Immediately before
an explicit start it reads the previous override. If already disabled, it leaves
it untouched. Otherwise it requests true and reads back the value. A successful
setter without successful readback is not advertised as protection.

An unsupported/failed query does not mutate an unknown prior state. A failed or
unconfirmed setter is treated as potentially applied and rolled back to the saved
state when the start is rejected. The first protection failure prevents USB start
and displays a warning/result. After manually configuring Switch sleep settings,
the user can explicitly choose START ANYWAY. That fallback retains an accurate
unconfirmed-protection warning throughout the run. It is never automatic. If the
original override could not be restored, RETRY START tries restoration first;
the original saved state is not overwritten and unprotected start is blocked.

The guard follows actual `civ_usb_owned()`, independently of screen navigation.
It stays held during acquisition, cancellation, disconnect, cleanup failure and
cleanup retry. Only actual lease release restores the previous override. Failed
restoration is displayed, retained for an explicit start/exit retry, and is not
retried every frame. Application exit restores after the existing forced USB
lifetime fence. A process kill that bypasses application cleanup cannot guarantee
a final IPC call. Physical Power-button sleep remains outside this override.

The same API path is attempted in applet and full application mode. Confirmation
means the applet override was read back; Phase 4 still needs hardware validation
of system behavior in both modes, including manual sleep/resume and exit.

## UI inventory and changes

Before Phase 4, the keyboard-menu Delay, Caps Lock and Exit chips were real
touch targets, backed by ZR/X/Plus actions. Caps used host HID LED feedback, not
a local inferred toggle. The connection card and USB label were status-only.
Compose's Previous/Next/Edit/Send/Back buttons were real actions; Caps, Auto-indent,
Diagnostics and Enter pause footer elements were also touch targets, while the
ZR delay chip was another settings action. L/R/X/ZR shortcuts shared the same
underlying values and send routines.

The keyboard menu now has USB KEYBOARD in the shared 32px section typography,
three uniform full-width cards (Compose & Send, Live Keyboard, Send Text File),
and the shared footer. Menu-level X/ZR/Plus and the three chip hitboxes are removed.
Back returns Home, where the existing Plus exit remains available. Diagnostics
remains available from Home and the keyboard menu's documented Minus shortcut.

Compose review keeps six preview rows, metadata and Previous/Next pagination.
SEND TEXT / EDIT / BACK are equal-sized primary buttons; there is no second Back
in its footer. Compact Delay, Enter pause, Auto-indent and Caps controls display
current values and use the existing single settings store. D-pad/left stick moves
focus, A selects, B returns, X edits, and ZR/R/L preserve their settings shortcuts.
All visible buttons share the same geometry with touch routing. Disabled controls
are visible but emit no action. D-pad paging is replaced by focusable Previous/Next
buttons; it is not hidden behind an undocumented shortcut.

Selecting Compose still opens the software keyboard. Cancelling an empty first
edit now leaves an empty review with EDIT selected and Send disabled. Existing
draft edits are retained on cancellation. Accepted text still goes through the
same preflight and explicit Send confirmation, with the unchanged 3-second
countdown, typer pacing, USB checks, cancellation and cleanup. Auto-indent re-plans
nonempty text and preserves settings focus; it does not create a second setting.
File-browser/parsing/transmission logic is unchanged.

Compose has no Diagnostics button or Minus action. Caps remains a target-side
toggle, separate from the local software keyboard's Shift/Caps. Actual host LED
reports still establish on/off; no report means “Toggle Caps (no host LED)” rather
than a guessed state or indefinite “Unknown”. Some targets never send HID LED
output. This cannot be solved truthfully by locally inferring a lock state, and
no USB protocol change was made to pretend otherwise.

## Host verification and previews

New tests exercise sleep save/set/readback/restore and failure paths without
per-frame IPC, production CIV start/ownership/cleanup integration, and production
keyboard/Compose routing, rendering, settings, focus, labels and hitboxes. Existing
typer, Compose pagination/validation, USB, controller, Live Keyboard, storage and
all 23,688-action oracle tests remain mandatory. Horizon modal dialogs and physical
system sleep are not fully simulated by host tests.

`tests/preview.c` generates 1280×720 PPMs (PNG copies can be created with Pillow):
`keyboard-menu`, `compose-empty`, `compose-review`, `compose-pages`,
`compose-settings-focus`, `compose-send-confirmation`, `live-normal`, `files-dark`,
`controller-menu`, and `civ-initial`, all under `build/`.

Hardware follow-up: test inactivity longer than the configured sleep timeout
during automation and blocked cleanup, prior-override restoration on all outcomes,
applet/application mode, deliberate Power-button sleep, editor cancellation and
pagination, settings/caps feedback, file sends, and the preserved full CIV run.
