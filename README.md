# RecurCIV

**USB Controller, Keyboard, Macro Studio & Civilization VII Automation**

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
- Create actions with simultaneous buttons, D-pad directions, and analog stick positions.
- Adjust hold durations and insert delays.
- Reorder and remove actions.
- Undo and redo edits.
- Configure playback speed from 25% to 200%.
- Play once or repeat a macro a specified number of times.
- Stop playback and release controller inputs safely.

**Recording behavior in v1.0.0:** Inputs are recorded locally and are not forwarded to the connected USB host while recording. You can review and save the recorded sequence, then play it back on the connected device.

Recording requires touchscreen access to stop. Recording is limited to 1,024 states or ten minutes, whichever comes first.

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

1. Download `RecurCIV-v1.0.0.zip` from the published release, when available.
2. Extract it to the root of your Switch SD card.
3. Confirm the application is located at:

```text
sdmc:/switch/RecurCIV/RecurCIV.nro
```

4. Launch RecurCIV through the Homebrew Menu.

Full application mode is recommended.

Keep the accompanying license notices in `switch/RecurCIV/`. They are part
of the distribution and do not change application operation. If downloading
the standalone NRO instead, also obtain the matching
`RecurCIV-v1.0.0-notices.zip`. Complete corresponding source is supplied as
`RecurCIV-v1.0.0-source.zip` beside the binaries. These are release packaging
instructions, not a claim that the release has already been published.

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

The Civilization VII automation has reached the CivSwitch exploit execution stage in testing. Macro Studio recording and playback still require further on-device verification.

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

The embedded payload is checked against its expected SHA-256 hash and line count during building.

Portable tests are provided in the `tests` directory.

The original editable payload source and generator are included in
`upstream/CivSwitch/`. See [payload provenance and reproduction](docs/recurciv-payload.md).
Ordinary building does not regenerate or change the approved payload.

Release checks and packaging:

```powershell
py tools/run_portable_tests.py
py tools/verify_release.py
py tools/package_release.py
```

The portable test wrapper deliberately excludes UI preview generation;
preview images are not hardware verification.

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

- Live macro recording that simultaneously controls the connected game.
- Faster access to Macro Studio from controller mode.
- Continued macro compatibility and hardware testing.
- Additional usability improvements.

## Acknowledgments

Thanks to the original Plume USB contributors, Gezine for CivSwitch, and the Nintendo Switch homebrew community.
