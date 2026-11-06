# Changelog

All notable RecurCIV releases will be documented here.

## [1.1.0] — 2026-11-05

### Added

- Live controller macro recording alongside the existing local-only recorder.
- Persistent controller passthrough before, during and after recording, with
  neutral synchronization and release-to-arm protection at session entry.
- Physical Capture-button short-press Start/Stop controls using supported libnx
  applet notifications, plus touchscreen Start/Stop and explicit session Exit.
- Visible touchscreen fallback when Capture registration is unavailable, safe
  notification queuing/debounce, and restoration of normal Capture handling on exit.
- Recording Review/Edit/Save/Load/Replay after live-session neutral cleanup, with
  explicit confirmation before replacing unsaved captured data.
- Reproducible, notice-inclusive install/source packaging and source/NRO manifests.

### Fixed

- Removed accumulating USB completion-observation latency from playback holds
  in tested fixed-latency scenarios. Each full scaled hold begins at successful
  state submission; completion still gates ordered transitions without skipped
  states, competing transfers or compressed catch-up holds.
- Improved preservation of captured data through disconnects, recording limits
  and cleanup failures, including neutral-only cleanup retries and no automatic resume.
- Expanded deterministic timing, Capture, live-session, USB and UI regression tests.

### Compatibility and known limitations

- Local recording, RCMS files/storage, playback speed scaling and finite repeats
  remain supported. USB descriptors/device identity and the approved Civ VII
  payload are unchanged; keyboard and Civ VII functionality are preserved.
- Recorded physical input may not exactly match what the host received during
  latest-state live passthrough. Analog sampling and USB/application scheduling
  can introduce small movement differences; slow transports can stretch playback.
- Capture behavior depends on the homebrew launch environment. Short-press
  screenshots are reserved during live sessions; broader long-hold/restoration
  testing remains necessary. Touchscreen fallback, exit and recovery are supported.
- USB compatibility across Switch hardware, docks and cable setups is not fully
  verified. Native Switch 2 execution is untested.

See `docs/macro-studio.md` and `docs/macro-timing.md` for workflow and measurements.

## [1.0.0] — 2026-11-05

### Initial Release

- Rebranded the application as RecurCIV.
- Integrated USB keyboard input, text-file sending, and Live Keyboard functionality.
- Added USB controller functionality.
- Added Macro Studio for creating, recording, editing, and playing controller macros.
- Added Macro Library for managing saved macros.
- Added adjustable macro playback speed and repeat counts.
- Added Civilization VII exploit automation based on CivSwitch.
- Added USB diagnostics and SD-card-based settings and macro storage.
- Updated application metadata, icon, and installation paths.

### Licensing and release preparation

- Applied GPLv3 to the combined distribution while preserving original MIT
  and third-party terms, copyright notices, and credits.
- Included the exact editable CivSwitch payload source, generator, pinned
  dependencies, provenance, and byte-for-byte reproduction instructions.
- Added runtime licensing/source records and notice-inclusive release packaging.
- Added release verification tooling without changing existing functional
  source, USB implementation, UI, build.py, assets, tests, or approved payload.

### Known Limitations

- Macro recording captures inputs locally without forwarding them to the connected USB host.
- Macro recording and playback require additional hardware verification.
- USB compatibility depends on the receiving device and its supported interfaces.

### Planned

- Live controller input forwarding during macro recording.
- Direct Macro Studio access from controller mode.
- Additional hardware compatibility testing and usability improvements.
