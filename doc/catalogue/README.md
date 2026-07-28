# aper catalogue

This directory contains the authored surface of the `aper` catalogue. The
catalogue is deliberately built from three separate layers:

- `aper-catalogue.tex` is the reviewed document shell and statement of method;
- `systems.tsv` records the local presentation choices used for native
  `aper` plates;
- `data/encyclopedia.tsv` and `.build/catalogue/reference-assets.tsv` are the
  versioned metadata snapshot and the separately fetched reference-art
  manifest.

Everything else is generated below `.build/catalogue`. In particular, the
builder never edits this directory, refreshes the Encyclopedia snapshot, or
uses the network.

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

`systems.tsv` has one row for each executable known system. Its columns are the
`aper` system identifier, display title, corresponding Encyclopedia record,
patch seed, patch depth, and colour scheme. The builder rejects any difference
between these rows, `data/encyclopedia.tsv`, and
`aper-search --list-known`.

For development before the reference manifest or candidate CLI is available,
an explicitly incomplete syntax build is possible:

```sh
tools/build-catalogue --smoke --output-dir .build/catalogue-smoke
```

Smoke output is not a catalogue deliverable: it replaces uncached reference
art with labelled placeholders and may omit search candidates.

The compiled catalogue incorporates metadata and artwork from the Tilings
Encyclopedia and is distributed under CC BY-NC-SA 2.0. Its title page and every
reference plate carry attribution and change information. The independently
authored `aper` software remains ISC-licensed.
