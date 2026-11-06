# RecurCIV

**USB Controller, Keyboard, Macro Studio & Civilization VII Automation**

**Current release: [v1.1.0](https://github.com/OR3OTHUG/RecurCIV/releases/tag/v1.1.0)**

RecurCIV is a Nintendo Switch homebrew application combining USB controller input, keyboard functionality, controller macro creation and playback, and automated Civilization VII exploit delivery.

Built on [Plume USB](https://github.com/MichelSama/plume-usb), with Civilization VII automation based on [CivSwitch](https://github.com/Gezine/CivSwitch).

## Features

### USB Keyboard

- **Compose & Send:** Enter text using the Switch software keyboard and send it to a connected USB host.
- **Live Keyboard:** Type individual characters using an on-screen keyboard.
- **Text File Sending:** Select a `.txt` file from the SD card and send its contents.
- Adjustable typing delays and Enter timing.
- Optional auto-indent compensation for editors that automatically indent new lines.
- USB connection diagnostics and logging.

Keyboard output uses the US QWERTY layout. Text files support UTF-8 input with printable ASCII characters, tabs and line breaks, up to 256 KiB. Unsupported characters are rejected before transmission.

### USB Controller

Use RecurCIV to send controller inputs to a compatible connected USB host.

The controller interface supports buttons, D-pad directions, and analog stick inputs.

For game control, ensure the receiving console recognizes RecurCIV as the intended active controller.

### Macro Studio

Create and edit controller macros directly on your Switch.

- Record controller inputs locally.
- Choose **Live Record** to capture physical controller inputs while forwarding
  them through the existing USB controller passthrough.
- Create actions with simultaneous buttons, D-pad directions, and analog stick positions.
- Adjust hold durations and insert delays.
- Reorder and remove actions.
- Undo and redo edits.
- Configure playback speed from 25% to 200%.
- Play once or repeat a macro a specified number of times.
- Stop playback and release controller inputs safely.

**New in v1.1.0:** Macro Studio has separate **Local Record**
and **Live Record** options. Enter Live Record to acquire passthrough immediately;
neutral synchronization and release of entry controls precede forwarding.
Physical **Capture** short presses (or touchscreen Start/Stop) toggle recording
without stopping passthrough. Unsaved captures require explicit replacement
confirmation. Touch **Exit live**, wait for neutral cleanup, then Review/Edit/Save.
Capture is never forwarded or recorded. Disconnects and cleanup failures retain
captured data; touchscreen Retry cleanup retries neutral output without resuming
recording or playback.

Playback now times each full scaled hold from successful state submission,
while still waiting for completion before advancing. This eliminates cumulative
USB completion latency in the tested fixed-latency scenarios without skipping
states or compressing catch-up holds. For sixty 100 ms states with simulated
5 ms USB transfers, active playback improved from **6,600 ms to 6,000 ms**.
See [timing measurements and limitations](docs/macro-timing.md).

Captured timestamps reflect sampled physical input, not USB completion.
Not every sampled transition is guaranteed to reach the host; playback timing
can differ. See [Macro Studio](docs/macro-studio.md) for the workflow and limits.

Local recording requires touchscreen Stop. Live Capture controls require supported
applet notifications (Horizon 3.0.0+); failures show touchscreen fallback. Short-press
screenshots are reserved during live sessions and restored on exit. Physical
delivery, long-hold behavior and screenshot restoration need hardware verification.
Touchscreen access is still required for session exit/recovery and replacement
confirmation. Recording is limited to 1,024 states or ten minutes.

### Macro Library

Manage saved macros directly within RecurCIV:

- Load and play macros.
- Edit existing macros.
- Rename or duplicate macros.
- Delete macros with confirmation.

Macros are stored at:

```text
sdmc:/switch/RecurCIV/macros/
```

### Importing and Sharing Macros

RecurCIV supports manually importing compatible macro files.

1. Obtain a RecurCIV-compatible macro file.
2. Access your Switch SD card using a computer or file-transfer utility.
3. Open `switch/RecurCIV/macros/`.
4. Copy the macro file into that directory.
5. Launch RecurCIV and open **Macro Library**.
6. Select the imported macro to review, edit, or play it.

Imported macros must use the supported RCMS format. Invalid or incompatible files may be rejected.

### Civilization VII Automation

RecurCIV includes an automated keyboard-input sequence for the Civilization VII exploit workflow associated with CivSwitch.

The sequence is embedded in the application, removing the need to manually type the exploit input.

Successful exploit execution does not automatically complete any subsequent network-based payload-loading steps.

## Installation

1. Download `RecurCIV-v1.1.0.zip` from the [v1.1.0 release](https://github.com/OR3OTHUG/RecurCIV/releases/tag/v1.1.0).
2. Extract it to the root of your Switch SD card.
3. Confirm the application is located at:

```text
sdmc:/switch/RecurCIV/RecurCIV.nro
```

4. Launch RecurCIV through the Homebrew Menu.

Full application mode is recommended.

Keep the accompanying license notices in `switch/RecurCIV/`. They are part
of the distribution and do not change application operation. The install ZIP
includes the NRO and its notices; a separate notices download is not required.
Complete corresponding source, assets, build/test tools, payload sources and
runtime source snapshots are supplied in `RecurCIV-v1.1.0-source.zip` beside it.
Do not replace existing macros or settings when upgrading; the RCMS format and
storage paths are unchanged.

## USB Connection

Connect the Switch running RecurCIV to the target USB host using a compatible data connection.

For a Switch 2 dock, a USB-A-to-USB-C data cable can connect the dock's USB host port to the Switch running RecurCIV.

Direct USB-C-to-USB-C connections depend on USB role negotiation and may not behave identically.

Wait for the appropriate USB connection status before sending keyboard or controller inputs.

For controller macros, configure the receiving device so RecurCIV is recognized as the intended controller.

## Keyboard Controls

| Control | Function |
|---|---|
| D-pad / Left stick | Navigate menus |
| A | Select or confirm |
| B | Back or cancel |
| L / R | Previous or next file page |
| ZR | Cycle character delay |
| R in text preview | Change Enter pause |
| L in text preview | Toggle auto-indent compensation |
| X | Toggle target Caps Lock |
| Minus | USB diagnostics |

### Live Keyboard Controls

| Control | Function |
|---|---|
| D-pad / Left stick | Move selection |
| A | Type selected key |
| B | Backspace |
| X | Shift / CAPS |
| Y | Space |
| Plus | Close Live Keyboard |

Touchscreen controls are available in supported screens.

## Text File Compatibility

- Maximum file size: 256 KiB.
- UTF-8 with or without BOM.
- Printable ASCII, tabs, and line breaks.
- US QWERTY keyboard mapping.
- Unsupported characters and invalid UTF-8 are rejected.
- No additional Enter is appended after the file.

Typing speed depends on the connected USB host and target application. Increase the character or Enter delay if characters are missed.

Auto-indent compensation is intended for editors that automatically copy the previous line's indentation. Test it with a short sample before sending large indented files.

## USB Diagnostics

RecurCIV can record USB configuration, transfer, and error information.

Diagnostic log:

```text
sdmc:/switch/RecurCIV/usb.log
```

If a connection fails, try reconnecting the cable, resetting USB from diagnostics, and checking whether the receiving device recognizes the USB interface.

## Compatibility

RecurCIV currently runs on a homebrew-enabled Nintendo Switch and has been used with a Switch 2 as a connected USB target.

Native execution on future homebrew-capable Switch hardware depends on the available homebrew environment, libnx support, and USB services.

Native Switch 2 execution has not been tested; a Switch 2 used as a connected
USB target is not evidence of native compatibility.

The Civilization VII automation has reached the CivSwitch exploit execution stage in testing. Live recording, Capture controls and playback have received hardware testing, but broader on-device verification remains necessary.

### Known limitations

- Recorded inputs describe sampled physical controller states, not necessarily
  everything the USB host received during live passthrough. Brief transitions
  can be recorded locally but omitted by latest-state forwarding.
- Analog stick sampling, application/render scheduling, USB polling and game
  frames can introduce small movement differences. Slow transfers or late ticks
  can stretch playback; identical final in-game positions are not guaranteed.
- Capture notifications depend on the homebrew launch environment. Registration
  failures retain touchscreen controls; long-hold/video and screenshot restoration
  behavior still need wider hardware verification.
- USB compatibility across Switch hardware, docks, cables and role-negotiation
  setups is not fully verified. Native Switch 2 execution has not been tested.

## Building From Source

Requirements:

- Python 3
- devkitPro
- devkitA64
- libnx
- Nintendo Switch development tools

Run:

```powershell
py build.py
```

The build produces:

```text
build/RecurCIV.nro
```

The build requires the included assets and `payload.txt`.

Debug checkout paths are remapped so the same source and toolchain can produce
a byte-identical NRO from different checkout directories.

The embedded payload is checked against its expected SHA-256 hash and line count during building.

The complete host test suite (including UI previews) runs from a Visual Studio
x64 Native Tools environment after the ARM build:

```powershell
cmd.exe /d /c tests\run-tests.cmd
```

The original editable payload source and generator are included in
`upstream/CivSwitch/`. See [payload provenance and reproduction](docs/recurciv-payload.md).
Ordinary building does not regenerate or change the approved payload.

Release packaging from a clean, committed source tree after building/testing:

```powershell
py tools/package_release.py
py tools/package_release.py --verify-only
```

The packager includes the complete committed source and original legal notices,
and validates both archives against that commit and the built NRO. A matching
manifest records the commit, source-file hashes and NRO SHA-256. Generated builds,
logs, previews and `node_modules` are excluded. Host tests and preview images are
not substitutes for hardware verification.

## Credits and Licensing

RecurCIV incorporates work from:

- **Plume USB** — MichelSama and contributors.
- **CivSwitch** — Gezine.
- **Netflix-N-Hack and ufm42's fork** — upstream exploit authors and contributors.
- **Y2JB** — Gezine; referenced and acknowledged by the upstream exploit code.
- **libnx** — libnx contributors.
- **stb_truetype** — Sean Barrett.
- **Manrope and Outfit** — font authors and contributors.

The **combined RecurCIV software distribution is GPLv3 (`GPL-3.0-only`)**,
including its embedded automation payload. Original Plume USB and
CivSwitch MIT notices remain intact; other dependencies and fonts retain
their respective licenses. CivSwitch's MIT notice does not remove GPL
obligations inherited from its exploit dependencies.

See [LICENSE](LICENSE), [COPYING](COPYING), [LICENSING.md](LICENSING.md),
[CREDITS.md](CREDITS.md), and [runtime licensing review](docs/runtime-licensing.md).
Original sources, revision references, notices, and checksums are included
under `upstream/`, `licences/`, and `third_party/`.

## Planned Improvements

- On-device verification of live macro recording and receiving-game behavior.
- Faster access to Macro Studio from controller mode.
- Continued macro compatibility and hardware testing.
- Additional usability improvements.

## Acknowledgments

Thanks to the original Plume USB contributors, Gezine for CivSwitch, and the Nintendo Switch homebrew community.
