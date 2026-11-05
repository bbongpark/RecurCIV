# RecurCIV — Credits and Third-Party Licenses

RecurCIV builds upon open-source projects and libraries developed by their respective authors and contributors.

## Plume USB

**Project:** [MichelSama/plume-usb](https://github.com/MichelSama/plume-usb)

RecurCIV is derived from Plume USB and retains its USB keyboard functionality and associated contributions.

**License:** MIT  
**Copyright:** © 2026 Plume USB contributors

The original copyright and permission notice is retained verbatim in
`LICENSE` and `licences/Plume-USB-MIT.txt`.

## CivSwitch

**Project:** [Gezine/CivSwitch](https://github.com/Gezine/CivSwitch)

RecurCIV incorporates Civilization VII exploit automation based on CivSwitch.

**License:** MIT  
**Copyright:** © 2026 Gezine

The supplied copyright and permission notice is retained in `licences/CivSwitch-MIT.txt`.

The exact editable `core.js`, original transport generator, upstream README,
and original MIT license are in `upstream/CivSwitch/`, pinned to commit
`3238a3e3340fd19a9dbd5cde56f3b50f9bc5d97f`. This identifies CivSwitch's
original terms, not an MIT-only license for third-party GPL exploit portions.

## Netflix-N-Hack and ufm42's fork

**Projects:** [NetflixNHack/Netflix-N-Hack](https://github.com/NetflixNHack/Netflix-N-Hack)
and [ufm42/Netflix-N-Hack](https://github.com/ufm42/Netflix-N-Hack).

CivSwitch's exploit core references and adapts their JavaScript exploit code.
Their original `inject.js`, README, and GPLv3 license are retained under
`upstream/Netflix-N-Hack/` and `upstream/Netflix-N-Hack-ufm42/`.
The full combined-distribution GPLv3 terms are also in `COPYING`.
Original author names, comments, references, and credits are not replaced
with RecurCIV ownership claims. Upstream acknowledgments include autechre,
Dr.YenYen, Gezine, ufm42, and the referenced STAR Labs research.

## Y2JB

**Project:** [Gezine/Y2JB](https://github.com/Gezine/Y2JB).

Acknowledged by the original Netflix-N-Hack sources for reference/advice and
stable primitive implementation. Preserve those original source references.
The original MIT notice, copyright (c) 2025 Gezine, is retained in
`licences/Y2JB-MIT.txt` as a precaution for inherited reference-derived work.

## libnx

**Project:** [switchbrew/libnx](https://github.com/switchbrew/libnx)

Nintendo Switch homebrew development library.

The applicable license notice is retained in `third_party/libnx-LICENSE`.
The corresponding libnx 4.12.0 source snapshot is included in `upstream/runtime/`.

## C and compiler runtime libraries

The NRO statically links newlib/libm, devkitPro's libsysbase, and selected
GCC runtime routines. Their original licenses, linking exceptions, source
snapshots/patches, and the actual linked-member review are documented in
`docs/runtime-licensing.md`. Notices are retained in `licences/newlib-*` and
`licences/GCC-*`. These are not all covered by the libnx notice.

## stb_truetype

**Author:** Sean Barrett

Font rendering library, distributed under its stated MIT or public-domain licensing options.

The original licensing information is retained in `third_party/stb_truetype.h`.
It is also copied verbatim to `licences/stb_truetype-LICENSE.txt` for binary
distribution without requiring users to download a C header.

## Fonts

RecurCIV includes the Manrope and Outfit typefaces under the SIL Open Font License.

Their license notices are retained in:

- `assets/Manrope-OFL.txt`
- `assets/Outfit-OFL.txt`

The fonts are byte-identical to the Plume USB assets inspected during this
review. Manrope's embedded 2019 copyright metadata and accompanying 2018
OFL notice are both preserved unchanged; Outfit retains its 2021 notice.

## Additional Contributions

Thanks to the Plume USB contributors, including AsteroidPizza39 for the auto-indent compensation contribution, and to the Nintendo Switch homebrew community.

## License Compliance

Third-party copyrights and licenses remain the property of their respective owners. RecurCIV is not an official release of Plume USB or CivSwitch.

The combined RecurCIV software distribution is offered under GPLv3, not MIT
alone. Original permissive licenses remain applicable to their respective
components; fonts remain OFL. See `LICENSING.md` for scope and source access.
