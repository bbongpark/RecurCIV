# Changelog

All notable RecurCIV releases will be documented here.

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
