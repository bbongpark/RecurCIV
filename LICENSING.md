# RecurCIV licensing and source availability

## Combined software

RecurCIV v1.1.0 is distributed as a combined software work under **GNU GPL
version 3 only (`GPL-3.0-only`)**, including its embedded automatic Civilization
VII payload. The complete terms are in `COPYING`. You may use, study, modify,
and redistribute the software under those terms, without warranty, subject
to applicable law. This is a license statement, not a legal opinion.

RecurCIV-specific contributions in `source/`, `include/`, `tests/`, `tools/`,
`build.py`, and project documentation are offered under GPLv3. These licensing
and initial v1.0.0 release-packaging changes were prepared on 2026-11-05. The then-existing
functional C source, headers, tests, USB implementation, UI, fonts, icon,
`build.py`, and approved `payload.txt` were not modified by this preparation.
See `CHANGELOG.md` for RecurCIV's functional changes from Plume USB.

## Original licenses are retained

| Component | Applicable original terms | Notice/source |
|---|---|---|
| Plume USB portions | MIT; copyright 2026 Plume USB contributors | `licences/Plume-USB-MIT.txt` |
| CivSwitch-authored portions | MIT; copyright 2026 Gezine | `licences/CivSwitch-MIT.txt`, `upstream/CivSwitch/` |
| Netflix-N-Hack / ufm42 exploit portions | GNU GPLv3 | `upstream/Netflix-N-Hack/`, `upstream/Netflix-N-Hack-ufm42/`, `COPYING` |
| Y2JB reference acknowledged by upstream | MIT; copyright 2025 Gezine | `licences/Y2JB-MIT.txt`; original references retained in upstream source |
| libnx 4.12.0 | ISC-style permission notice | `third_party/libnx-LICENSE`, `upstream/runtime/libnx-4.12.0.tar.gz` |
| stb_truetype | Original MIT or public-domain options | `third_party/stb_truetype.h`, `licences/stb_truetype-LICENSE.txt` |
| Manrope / Outfit fonts | SIL Open Font License 1.1 | `assets/*-OFL.txt`; original `.ttf` metadata retained |
| newlib / libm / libsysbase | Component-specific permissive terms | `licences/newlib-*`, `docs/runtime-licensing.md`, `upstream/runtime/` |
| GCC runtime portions | GPLv3 with Runtime Library Exception and LGPL soft-fp linking permission, as applicable | `licences/GCC-*`, `upstream/runtime/gcc/` |

Do not remove original copyright, permission, attribution, exception, or
no-warranty notices. The combined-work GPL designation does not relicense
the fonts, erase MIT permissions on separable upstream code, or claim
ownership of other authors' contributions. Do not infer an upstream
"or later" grant from RecurCIV's chosen version designation.

The Manrope font's embedded metadata says copyright 2019; its accompanying
upstream OFL notice says copyright 2018. Both original notices are retained,
not silently rewritten. Outfit's original 2021 notice is likewise retained.

## Payload corresponding source

The payload is not merely arbitrary text: it contains an encoded, compressed
JavaScript program derived from GPL-covered exploit code. CivSwitch's MIT
notice alone does not replace those obligations. Its exact editable source
and generator are included under `upstream/CivSwitch/`; original exploit
reference sources and their notices are included separately. See
`docs/recurciv-payload.md` for immutable revisions, hashes, and reproduction.

The approved root payload remains byte-for-byte unchanged. Regeneration is
an audit operation in a temporary directory, not part of ordinary building,
and never replaces the approved payload automatically.

## Binary distribution and corresponding source

Distribute the SD-card ZIP with its accompanying `COPYING`, license map,
credits, and `licences/` directory intact. The standalone `.nro` is not a
self-contained legal-notice package: accompany it with the matching notices
archive, and retain those notices when redistributing it.

For the v1.1.0 public GitHub release, provide `RecurCIV-v1.1.0-source.zip`
beside the binaries, at no additional charge, with an explicit link in the
release description. That archive must include the complete application
source, payload and its editable sources/generator, required assets, tests,
build instructions, and runtime source snapshots/patches. A changing branch
or a promise to provide source later is not the selected GPLv3 section 6(d)
distribution method. Keep the matching source available while offering the
binaries. Do not label planned GitHub URLs as already published.

## Review boundaries

`docs/runtime-licensing.md` records the runtime review and
`docs/release-review.md` records verification and any remaining release gates.
No application UI change is made by this licensing preparation. The GPLv3
section 5(d) upstream-interface exception and its limits are discussed in
the runtime/licensing review; publication must not silently assume that
external documentation cures an independently required in-app notice.

Confirm rights to RecurCIV-specific contributions and the icon before
publication. Open-source license compliance does not by itself resolve
anti-circumvention, trademark, contract, or other jurisdiction-specific law.
RecurCIV is not endorsed by Nintendo, Firaxis/2K, Netflix, or the upstream
projects. No game executable, console firmware, proprietary SDK, or console
signing key is included in this release preparation.
