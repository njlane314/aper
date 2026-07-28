# aper catalogue

This directory contains the authored surface of the `aper` catalogue. The
catalogue is deliberately built from three separate layers:

- `aper-catalogue.tex` is the reviewed document shell and statement of method;
- `systems.tsv` records the local presentation choices used for executable
  `aper` plates;
- `data/encyclopedia.tsv` is the versioned metadata snapshot;
- `.build/catalogue/reference-assets.tsv` is the fetched reference-art
  manifest.

Fetched source HTML and assets live under `.cache/encyclopedia`; converted
plates, manifests, TeX, and PDF are generated below `.build/catalogue`. The
offline builder edits neither authored directory nor snapshot and performs no
network access.

From the repository root, build with:

```sh
make catalogue
```

or invoke the underlying offline builder directly:

```sh
tools/build-catalogue
```

The result is `.build/catalogue/aper-catalogue.pdf`. A full build requires
`aper`, `aper-search`, `pdflatex`, `latexmk`, and the reference-art manifest.
PDF, PNG, and JPEG reference assets are included directly. SVG, GIF, TIFF,
BMP, WebP, and EPS inputs require an available deterministic converter; the
builder reports the exact missing program rather than silently dropping art.
The fetch stage accepts `rsvg-convert` or Inkscape for SVG, and ImageMagick or
macOS `sips` for GIF.

`systems.tsv` has one row for each locally encoded executable system. Its
columns are system identifier, optional `.aper` definition path, display title,
Encyclopedia record, patch seed, patch depth, and colour scheme. A cached rule
or patch diagram is still reference artwork and never makes a system native.
The builder rejects identifier or source-record disagreement among these rows,
`data/encyclopedia.tsv`, definitions, and `aper-search --list-known`.

The search appendix is generated from a bounded geometric control:

```sh
./aper-search --space polyomino --cells 3 --list-candidates
```

It enumerates the two free connected triominoes and uses exact cover to
dissect each twofold inflation into four rotated or reflected copies. The
L-triomino result rediscovers the classical Chair rep-tile; the straight
I-triomino is periodic. They test the search and validation machinery and are
not novelty or aperiodicity claims.

For development before the reference manifest or candidate CLI is available,
an explicitly incomplete syntax build is possible:

```sh
tools/build-catalogue --smoke --output-dir .build/catalogue-smoke
```

Smoke output is not a catalogue deliverable: it replaces uncached reference
art with labelled placeholders and may omit search candidates.

The compiled catalogue incorporates metadata and artwork from the Tilings
Encyclopedia and is distributed under CC BY-NC-SA 2.0. Its title page and every
reference-only plate carry attribution and change information. The catalogue
does not treat those diagrams as executable substitution data. Independently
authored `aper` software and rule encodings remain ISC-licensed.
