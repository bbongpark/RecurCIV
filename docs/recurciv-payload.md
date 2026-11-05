# Embedded payload provenance and exact reproduction

## Approved input: do not replace or edit

The root `payload.txt` contains **21,762 bytes and 87 nonempty lines**.
Its SHA-256 is:

```text
6ea468ca392558ab2f8f2384200745a8619187cebc5357d5e985fd3ffdada37d
```

`build.py` rejects a missing/altered payload and embeds its bytes without
translation. The automatic Civilization VII input workflow remains intact;
this licensing preparation does not change its timing, routes, USB behavior,
controller identity, exploit contents, or application UI. Ordinary building
uses the approved file, not a newly generated replacement.

## Exact editable upstream source

The included `upstream/CivSwitch/` originals come from:

```text
https://github.com/Gezine/CivSwitch
3238a3e3340fd19a9dbd5cde56f3b50f9bc5d97f
```

- `core.js`: original readable source, SHA-256
  `1fe1aaa68dc8c0534d79d65b1dd545f66c6b3fb3d547f01c967cdc5f70e7bdf4`.
- `PicoSwitch/compress.js`: original Terser/deflate/base-25 transport generator.
- `PicoSwitch/package.json`: original upstream dependency declaration.
- `README.md` and `LICENSE`: original attribution, workflow, and MIT notice.

`upstream/SOURCES.json` records original download URLs and exact file hashes.
The separate reference snapshots are Netflix-N-Hack commit
`605eb93ba5fa589f4f1274973e191e6c7fff1b9d` and ufm42 fork commit
`1208a6ba6ca7131ec1843022093eefe5a8c51d2c`. These are documented reference
snapshots, not an unsupported claim that CivSwitch used exactly those commits.
The exact source used to reconstruct this payload is the pinned CivSwitch source.

## Reproduce safely into temporary storage

Requirements: Python 3, Node.js/npm, and access to the npm registry on first
installation. The audit environment used Node.js **v26.13.0**, Terser
**5.51.2**, and Node zlib **1.3.2.1-motley-456ae73**. Compression can differ
between runtime versions; the approved hash is the final authority.

From the project root:

```powershell
py tools/reproduce_payload.py
```

The helper copies upstream inputs plus `tools/payload-repro/package*.json`
into a temporary directory, runs `npm ci --ignore-scripts`, then invokes the
original generator. Dependencies are pinned in the supplied lockfile.
It checks upstream-input hashes, generated length/line count/hash, and exact
equality with the approved root payload. It **never overwrites `payload.txt`**
and never executes the generated exploit JavaScript. Dependency installation
and the generator execute build tooling, not the exploit itself.

Equivalent manual steps in a disposable directory:

1. Copy `core.js` to the directory root.
2. Create `PicoSwitch/` and copy the original `compress.js` into it.
3. Copy the helper's pinned `package.json` and `package-lock.json` there.
4. Run `npm ci --ignore-scripts --no-audit --no-fund` inside `PicoSwitch/`.
5. Run `node compress.js` there; output is `PicoSwitch/payload.txt`.
6. Compare its raw bytes and SHA-256 with the approved project-root input.

The minified program contained in the approved transport is 26,004 UTF-8
bytes with SHA-256
`f08f9f21bb0722fe67b57c13bdd4912ff72e97c782d636e4c8381c2c690b20eb`.
Both minified content and complete transport were independently reproduced
byte-for-byte during licensing investigation on 2026-11-05.

## License scope

The readable exploit source contains code derived from GPLv3-licensed
Netflix-N-Hack and its ufm42 fork. Their GPL requirements are not removed
by CivSwitch's MIT notice. RecurCIV's combined distribution is consequently
offered under GPLv3; original MIT and other upstream notices are preserved.
See `COPYING`, `LICENSING.md`, `CREDITS.md`, and the included upstream originals.

An editable source bundle and generator are supplied instead of treating
encoded/minified payload text as the preferred form for modification.
No upstream license header has been injected into the approved payload,
because that would change its bytes; notices accompany the distribution.
