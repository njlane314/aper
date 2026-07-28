# aper

`aper` renders finite planar polygon substitutions as dense patches or rule
sheets. Rule geometry lives in declarative `.aper` files; `RuleLibrary`, the
iteration engine, validators, search, and PDF renderer are rule-independent.
Six legacy projector-based presentations remain built in while they are
migrated to the same data boundary. `aper-search` enumerates bounded geometric
rule spaces and compares survivors with the executable rule library.

They are small, dependency-free C++20 tools for the shell. Deterministic,
native vector PDF goes to standard output; diagnostics go to standard error.

```sh
make
./aper > patch.pdf
./aper --library rules --tiling square-chair --rule > square-chair-rule.pdf
./aper-search --definition > candidate.aper
./aper --file candidate.aper -n 5 > candidate.pdf
make rules-check
```

## Use

```text
aper [-L library] [-t type | -f file] [-s seed] [-c scheme] [-n depth] [-r]
aper [-L library] [-t type | -f file] --definition
aper -h | --help
aper -V | --version
```

- `-t type` or `--tiling type` selects any library identifier. Six legacy
  systems are built in: `p1`, `p2`, `p3`, `ammann-beenker`, `pinwheel`, and
  `stampfli`. The distributed rule files add the nine systems listed below.
  Short aliases include `ab`, `kite-dart`, `rhomb`, and `thue-morse`. The
  default is `p3`.
- `-f file` or `--file file` reads a complete substitution system from a
  declarative `.aper` file; `-` reads standard input. It is mutually exclusive
  with `--tiling`.
- `-L directory` or `--library directory` recursively loads `.aper` files and
  makes their identifiers available to `--tiling`. The local `rules/` directory
  is loaded automatically when present, otherwise the installed library is
  used. Built-ins are added as fallbacks; a duplicate identifier must describe
  the same canonical rule.
- `-s seed` or `--seed seed` selects the patch's starting design. P1 provides
  `pentagon-5`, `pentagon-3`, `pentagon-2`, `diamond`, `boat`, and `star`;
  P2 provides `sun`, `star`, `ace`, `deuce`, `jack`, `queen`, and `king`;
  P3 provides `sun`, `star`, `thin`, and `thick`; Ammann–Beenker provides
  `octagon`, `square`, and `rhomb`; Pinwheel provides `triangle`; Stampfli
  12-fold 1 provides `dodecagon`, `triangle`, `square`, and `rhomb`; and
  Thue–Morse 2D provides `a` and `b`; Chair and Domino each provide a seed of
  the same name. Their defaults follow that order. The P3 `thin` seed needs a
  depth of at least 2. File-defined systems declare their own seeds.
- `-c scheme` or `--colour scheme` selects `flare`, `grove`, `electric`, or
  `tide`. The default is `flare`.
- `-n depth` or `--depth depth` sets the number of substitutions. P1 accepts
  1 through 6 and defaults to 5; P2 and P3 accept 1 through 12 and default to
  7; Ammann–Beenker accepts 1 through 6 and defaults to 4; Pinwheel accepts
  1 through 8 and defaults to 6; Stampfli 12-fold 1 accepts 1 through 3 and
  defaults to 2; Thue–Morse 2D accepts 1 through 7 and defaults to 4; Chair and
  Domino accept 1 through 8 and default to 5. File-defined systems declare
  their own limits.
- `-r` or `--rule` draws a sheet containing every prototile and its one-step
  substitution instead of a patch. The tiling and colour options still apply;
  seed and depth are patch-only options.
- `--definition` writes the selected system as normalised `.aper` text. It fails
  closed if a legacy presentation cannot be represented exactly by format v1.
- `-h` or `--help` prints a concise help message.
- `-V` or `--version` prints the version.

For example, render the Pinwheel substitution rule:

```sh
./aper -t pinwheel --rule -c tide > pinwheel-rule.pdf
```

## Rule files

A `.aper` file contains data, not drawing code: named polygonal prototiles,
their replacements, seeds, display fills, depth bounds, and optional source
provenance. Child coordinates are written in the inflated parent, so a Chair
rule is simply:

```text
aper 1
id chair
name "Chair"
inflation 2
depths 1 5 8
default-seed chair

tile chair 0
polygon 0 0  2 0  2 1  1 1  1 2  0 2
end

rule chair
child chair 0 0 0 normal
child chair 0 4 -90 normal
child chair 1 1 0 normal
child chair 4 0 90 normal
end

seed chair 1
place chair 0 0 1 0 normal
end
```

Every tile type may have its own polygon and rule; children and seed placements
may rotate or reflect. The reader rejects malformed references, inconsistent
areas, non-simple polygons, escaped children, overlaps, gaps, and unmatched
edges before rendering. See [Chair](rules/chair.aper),
[Domino](rules/domino.aper), and the two-type test fixture
[square](tests/data/square.aper).

The shipped library includes Chair, Domino, Thue–Morse 2D, Square Chair,
Squiral Block, Period Tripling 2D, Pentomino, and two Tromino systems. Run
`make rules-check` to parse, canonicalise, iterate, and render every definition;
see the [rule-library notes](rules/README.md).

The v1 format deliberately covers finite two-dimensional polygonal
substitutions with one scalar inflation and similarity transforms. General
affine inflation, curved or fractal boundaries, infinitely many tile types,
and multiscale rules need explicit later extensions; calling that narrower
class “any substitution” would be misleading.

For flat square-lattice rule diagrams, a strict importer can recover rule data
without embedding the particular tiling in code:

```sh
tools/reconstruct-lattice-rule --id recovered --cells 5 diagram.png > recovered.aper
./aper --file recovered.aper --rule > recovered-rule.pdf
```

It currently accepts one simply connected polyomino parent beside one complete
replacement patch, with integer inflation and grid-aligned rotations or
reflections. It proves an exact cover and rejects ambiguous, overlapping,
gapped, or misaligned interpretations. The included Pentomino definition was
recovered through this path; general diagrams still require reviewed
transcription or a future model extension.

## Mathematical idea

A tiling covers the plane without gaps or overlapping interiors. A
[substitution tiling](https://tilings.math.uni-bielefeld.de/glossary/substitution/)
begins with finitely many *prototile types*—types, not necessarily different
shapes. A rule expands each type $T_i$ by a linear factor $\lambda > 1$, dissects
the result into transformed prototiles, and may then be iterated. Its finite
stages are *supertiles*: nested patches in which the same organisation
reappears at larger scales.

Let $M_{ij}$ count tiles of type $j$ in the substitution of $T_i$, and let $a$
be the vector of prototile areas. In the plane, a consistent self-similar rule
obeys

$$
M a = \lambda^2 a.
$$

The Perron–Frobenius data of $M$ therefore connects geometry with combinatorial
growth and, for primitive substitutions, tile frequencies.

A full tiling $T$ is nonperiodic when $T + x = T$ implies $x = 0$. A system is
aperiodic only when every full-plane tiling it admits is nonperiodic;
substitution alone does not guarantee this. Such tilings are interesting
because a finite recursive description can produce deterministic long-range
order, recurring motifs at unbounded scales, and noncrystallographic rotational
order forbidden to a periodic lattice. They connect geometry and algebra with
dynamical systems, diffraction, and models of quasicrystals.

`aper` renders finite substitution patches and their rules. A large patch may
suggest aperiodicity, but it cannot prove it.

## Search

`aper-search` now performs a geometric search. For each free connected
polyomino $P$ with 1--6 cells, it places every rotated or reflected copy of
$P$ on the integer lattice and solves the exact-cover problem

$$
2P = P_1 \sqcup P_2 \sqcup P_3 \sqcup P_4.
$$

Each solution becomes an ordinary `TilingSystem`; no renderer knows which
polyomino or rule produced it.

```text
shape enumeration → exact cover → algebraic and geometric validation
                  → multi-generation checks → canonical key → .aper or PDF
```

```sh
./aper-search --list-candidates
./aper-search --candidate 0 > chair.pdf
./aper-search --candidate 0 --rule > chair-rule.pdf
./aper-search --candidate 0 --definition > chair.aper
./aper --file chair.aper -n 5 > chair-from-file.pdf
./aper-search --cells 2 --list-candidates
```

```text
aper-search [--space name] [--cells n] [--candidate n] [--bank dir] [-c scheme] [-n depth] [-r]
aper-search [--space name] [--cells n] [--candidate n] [--bank dir] --definition
aper-search [--space name] [--cells n] [--candidate n] [--bank dir] --classify
aper-search [--space name] [--cells n] [--bank dir] --list-candidates
aper-search [--bank dir] {-l | --list-known}
aper-search -h | --help
aper-search -V | --version
```

Patch depth is 1--7 (default 4); `--rule` renders the rule, `--definition`
writes reusable `.aper` text, and `--bank DIR` selects the declarative library
used for exact comparison. The default three-cell search is small but genuinely
geometric:

```text
2 free triominoes → 2 exact covers → 2 validated rules
                                      ├── Chair: exact bank match
                                      └── I-triomino: periodic control
```

The complete supported sweep is deterministic:

| Cells | Exact covers | Unique rules | Exact bank matches |
| ---: | ---: | ---: | :--- |
| 1 | 1 | 1 | — |
| 2 | 5 | 4 | Domino |
| 3 | 2 | 2 | Chair |
| 4 | 6 | 6 | — |
| 5 | 3 | 3 | Pentomino |
| 6 | 3 | 3 | — |
| **Total** | **20** | **19** | **3** |

Thus 16 exact fingerprints are absent from the fifteen-rule bank. Most are
expected periodic grids, alternate rep-tile dissections, or known systems in a
different presentation; they are candidates for analysis, not discoveries.

`GeometricValidator` rejects non-simple polygons, escaped or overlapping
children, gaps, area errors, and unmatched atomic edges. The Chair match is an
independent rediscovery: the search knows only connected cells and exact cover,
while the bank rule was encoded separately. Two- and five-cell runs likewise
contain exact Domino and Pentomino matches.

`square` and `binary-square` remain explicit pipeline controls. The latter
still exhausts 256 labelled $2\times2$ rules and reduces them to 27 classes,
including two-dimensional Thue–Morse. Their uncoloured geometry is only the
periodic square grid, which is why they are no longer the default search.

An unmatched exact fingerprint is a prompt for investigation, not a novelty or
aperiodicity claim. Equivalent presentations, mutual local derivability, and
mathematical proofs lie beyond canonical coordinate equality.

### Known-rule bank

The bank has two deliberately separate layers:

- the included `data/encyclopedia.tsv` snapshot indexes 257 Encyclopedia
  pages, with titles, page URLs at the snapshot date, and classification tags;
- `KnownTilingBank` fingerprints fifteen locally encoded `TilingSystem` objects
  linked to their corresponding Encyclopedia records.

```sh
./aper-search --list-known
./aper-search --classify
rg -i 'chair|rhomb|pinwheel' data/encyclopedia.tsv
```

`--classify` reports canonical equality with an encoded substitution rule. The
default candidate reports Chair; the I-triomino reports no exact match among
the fifteen encodings. That absence is not evidence that it is new.

Refresh the metadata snapshot with one network request:

```sh
make encyclopedia-bank
```

The Encyclopedia deliberately presents substitution diagrams rather than a
machine-readable coordinate bank. Its 257-page snapshot contains no complete
planar vertex-and-placement record, so a diagram becomes executable only after
a reviewed transcription or an independent exact construction. Raster tracing
is not silently promoted to mathematics. The metadata notice is in
`data/encyclopedia.NOTICE.md`; reference-art and compiled-catalogue licensing is
documented in `doc/catalogue/README.md`. The software and `.aper` rule encodings
remain ISC-licensed.

### Complete catalogue

The opt-in catalogue places every patch directly beside its substitution rule
on an A4 landscape plate:

```sh
make catalogue          # populate the cache as needed, then build offline
make catalogue-offline  # require an already complete cache
```

The document contains all 257 records in the metadata snapshot, followed by the
two triomino-search survivors. Fifteen local plates are generated as vector PDFs
from executable geometry. Each of the remaining 242 plates uses a checksummed
source patch; 227 also use source rule artwork, while 15 show an explicit rule
placeholder rather than an invented rule.

The first fetch is deliberately gentle and may take several minutes. Detail
HTML and artwork are cached under `.cache/`; converted plates, generated TeX,
hashes, and the PDF stay under `.build/catalogue/`. Neither directory is
versioned. The complete catalogue incorporates CC BY-NC-SA 2.0 material and is
distributed under those terms; the independently authored software remains
ISC-licensed. See [the catalogue notes](doc/catalogue/README.md) for the
authored/generated boundary and direct builder commands.

## Designs

The P1 family has six prototiles: three geometrically identical pentagons with
different substitution roles, plus the diamond, boat, and star. The seven P2
seeds are the classic vertex-centred Penrose patches. The two P3 prototile
seeds complement its symmetric sun and star arrangements. Thue–Morse uses two
semantic types of the same square; Chair and Domino demonstrate concave and
rectangular file-defined rep-tiles.

Patch output is clipped to an edge-to-edge 4:3 viewport. Rule output lays out
each parent beside its inflated replacement patch. P2, P3, and Ammann–Beenker
rule sheets retain their construction triangles, so unmatched half-tiles are
not silently hidden.

### Patches and substitution rules

| Patch | Substitution rule |
| :---: | :---: |
| **P1 pentagon-boat-star · Flare** | **P1 substitution · Flare** |
| ![Clipped P1 pentagon-boat-star patch](doc/aper-p1-patch-flare.png) | ![Six P1 parent-to-replacement rules](doc/aper-p1-rules-flare.png) |
| `./aper -t p1 -n 5 > p1.pdf` | `./aper -t p1 --rule > p1-rules.pdf` |
| **P2 sun · Flare** | **P2 kite-and-dart substitution · Flare** |
| ![P2 sun patch in flare colours](doc/aper-p2-sun-flare.png) | ![P2 Robinson-triangle substitution rules](doc/aper-p2-rules-flare.png) |
| `./aper -t p2 -s sun -c flare -n 5 > p2.pdf` | `./aper -t p2 --rule -c flare > p2-rules.pdf` |
| **P3 star · Electric** | **P3 rhomb substitution · Electric** |
| ![P3 star patch in electric colours](doc/aper-p3-star-electric.png) | ![P3 Robinson-triangle substitution rules](doc/aper-p3-rules-electric.png) |
| `./aper -t p3 -s star -c electric -n 5 > p3.pdf` | `./aper -t p3 --rule -c electric > p3-rules.pdf` |
| **Ammann–Beenker octagon · Grove** | **Ammann–Beenker substitution · Grove** |
| ![Ammann–Beenker octagon patch in grove colours](doc/aper-ammann-beenker-octagon-grove.png) | ![Ammann–Beenker substitution rules](doc/aper-ammann-beenker-rules-grove.png) |
| `./aper -t ammann-beenker -c grove -n 4 > ab.pdf` | `./aper -t ammann-beenker --rule -c grove > ab-rules.pdf` |
| **Pinwheel triangle · Tide** | **Pinwheel substitution · Tide** |
| ![Pinwheel triangle patch in tide colours](doc/aper-pinwheel-triangle-tide.png) | ![Pinwheel substitution rule](doc/aper-pinwheel-rules-tide.png) |
| `./aper -t pinwheel -c tide -n 6 > pinwheel.pdf` | `./aper -t pinwheel --rule -c tide > pinwheel-rule.pdf` |
| **Stampfli dodecagon · Flare** | **Stampfli 12-fold substitution · Flare** |
| ![Stampfli dodecagon patch in flare colours](doc/aper-stampfli-dodecagon-flare.png) | ![Stampfli 12-fold substitution rules](doc/aper-stampfli-rules-flare.png) |
| `./aper -t stampfli -c flare -n 2 > stampfli.pdf` | `./aper -t stampfli --rule -c flare > stampfli-rules.pdf` |
| **Thue–Morse 2D · Electric** | **Thue–Morse substitution · Electric** |
| ![Two-dimensional Thue-Morse patch in electric colours](doc/aper-thue-morse-2d-a-electric.png) | ![Complementary two-dimensional Thue-Morse substitution rules](doc/aper-thue-morse-2d-rules-electric.png) |
| `./aper -t thue-morse-2d -c electric -n 6 > thue-morse.pdf` | `./aper -t thue-morse-2d --rule -c electric > thue-morse-rule.pdf` |
| **Chair · Grove** | **Chair substitution · Grove** |
| ![Chair rep-tile patch](doc/aper-chair-patch-grove.png) | ![Chair parent and four-child substitution](doc/aper-chair-rule-grove.png) |
| `./aper --file rules/chair.aper -c grove -n 5 > chair.pdf` | `./aper --file rules/chair.aper --rule -c grove > chair-rule.pdf` |
| **Domino · Tide** | **Domino substitution · Tide** |
| ![Domino table-tiling patch](doc/aper-domino-patch-tide.png) | ![Domino parent and four-child substitution](doc/aper-domino-rule-tide.png) |
| `./aper --file rules/domino.aper -c tide -n 5 > domino.pdf` | `./aper --file rules/domino.aper --rule -c tide > domino-rule.pdf` |
| **Pentomino · Flare** | **Pentomino substitution · Flare** |
| ![P-pentomino rep-tile patch](doc/aper-pentomino-patch-flare.png) | ![P-pentomino parent and exact four-child dissection](doc/aper-pentomino-rule-flare.png) |
| `./aper --file rules/pentomino.aper -c flare -n 4 > pentomino.pdf` | `./aper --file rules/pentomino.aper --rule -c flare > pentomino-rule.pdf` |
| **Period Tripling 2D · Electric** | **Period Tripling 2D substitution · Electric** |
| ![Two-dimensional period-tripling patch](doc/aper-period-tripling-2d-patch-electric.png) | ![Two complementary period-tripling rules](doc/aper-period-tripling-2d-rule-electric.png) |
| `./aper --file rules/period-tripling-2d.aper -c electric -n 4 > period-tripling.pdf` | `./aper --file rules/period-tripling-2d.aper --rule -c electric > period-tripling-rule.pdf` |
| **Square Chair · Flare** | **Square Chair substitution · Flare** |
| ![Four-type square-chair patch](doc/aper-square-chair-patch-flare.png) | ![Four square-chair replacement rules](doc/aper-square-chair-rule-flare.png) |
| `./aper --file rules/square-chair.aper -c flare -n 5 > square-chair.pdf` | `./aper --file rules/square-chair.aper --rule -c flare > square-chair-rule.pdf` |
| **Squiral Block · Grove** | **Squiral Block substitution · Grove** |
| ![Two-type Squiral block patch](doc/aper-squiral-block-patch-grove.png) | ![Complementary three-by-three Squiral block rules](doc/aper-squiral-block-rule-grove.png) |
| `./aper --file rules/squiral-block.aper -c grove -n 4 > squiral.pdf` | `./aper --file rules/squiral-block.aper --rule -c grove > squiral-rule.pdf` |
| **Tromino 1 · Flare** | **Tromino 1 substitution · Flare** |
| ![L-and-bar Tromino 1 patch](doc/aper-tromino1-patch-flare.png) | ![Tromino 1 replacement rules](doc/aper-tromino1-rule-flare.png) |
| `./aper --file rules/tromino1.aper -c flare -n 5 > tromino1.pdf` | `./aper --file rules/tromino1.aper --rule -c flare > tromino1-rule.pdf` |
| **Tromino 2 · Electric** | **Tromino 2 substitution · Electric** |
| ![L-and-bar Tromino 2 patch](doc/aper-tromino2-patch-electric.png) | ![Tromino 2 replacement rules](doc/aper-tromino2-rule-electric.png) |
| `./aper --file rules/tromino2.aper -c electric -n 5 > tromino2.pdf` | `./aper --file rules/tromino2.aper --rule -c electric > tromino2-rule.pdf` |
| **Chair rediscovered · Electric** | **Discovered Chair rule · Electric** |
| ![Chair patch generated by the polyomino search](doc/aper-search-chair-patch-electric.png) | ![Chair rule generated by exact cover](doc/aper-search-chair-rule-electric.png) |
| `./aper-search -c electric -n 5 > found-chair.pdf` | `./aper-search --rule -c electric > found-chair-rule.pdf` |

### P1 seeds

| Pentagon-5 · Flare | Star · Electric |
| :---: | :---: |
| ![P1 pentagon-5 seed in flare colours](doc/aper-p1-pentagon-5-flare.png) | ![P1 star seed in electric colours](doc/aper-p1-star-electric.png) |
| `./aper -t p1 -s pentagon-5 -c flare -n 5 > p1.pdf` | `./aper -t p1 -s star -c electric -n 3 > p1-star.pdf` |

### P2 vertex seeds

| Sun · Flare | Star · Electric |
| :---: | :---: |
| ![P2 sun seed in flare colours](doc/aper-p2-sun-flare.png) | ![P2 star seed in electric colours](doc/aper-p2-star-electric.png) |
| `./aper -t p2 -s sun -c flare -n 5 > sun.pdf` | `./aper -t p2 -s star -c electric -n 5 > star.pdf` |
| Ace · Tide | Deuce · Grove |
| ![P2 ace seed in tide colours](doc/aper-p2-ace-tide.png) | ![P2 deuce seed in grove colours](doc/aper-p2-deuce-grove.png) |
| `./aper -t p2 -s ace -c tide -n 5 > ace.pdf` | `./aper -t p2 -s deuce -c grove -n 5 > deuce.pdf` |
| Jack · Electric | Queen · Grove |
| ![P2 jack seed in electric colours](doc/aper-p2-jack-electric.png) | ![P2 queen seed in grove colours](doc/aper-p2-queen-grove.png) |
| `./aper -t p2 -s jack -c electric -n 5 > jack.pdf` | `./aper -t p2 -s queen -c grove -n 5 > queen.pdf` |
| King · Tide | |
| ![P2 king seed in tide colours](doc/aper-p2-king-tide.png) | |
| `./aper -t p2 -s king -c tide -n 5 > king.pdf` | |

### P3 seeds

| Sun · Flare | Star · Electric |
| :---: | :---: |
| ![P3 sun seed in flare colours](doc/aper-p3-sun-flare.png) | ![P3 star seed in electric colours](doc/aper-p3-star-electric.png) |
| `./aper -t p3 -s sun -c flare -n 5 > p3-sun.pdf` | `./aper -t p3 -s star -c electric -n 5 > p3-star.pdf` |
| Thin · Tide | Thick · Grove |
| ![P3 thin-rhomb seed in tide colours](doc/aper-p3-thin-tide.png) | ![P3 thick-rhomb seed in grove colours](doc/aper-p3-thick-grove.png) |
| `./aper -t p3 -s thin -c tide -n 6 > thin.pdf` | `./aper -t p3 -s thick -c grove -n 6 > thick.pdf` |

`flare` pairs ultramarine with vermilion, `grove` emerald with marigold,
`electric` violet with citron, and `tide` raspberry with cyan. Every scheme
keeps white paper and near-black outlines.

The same arguments produce the same PDF, making `aper` suitable for scripts,
pipes, and generated documents.

## Build

A C++20 compiler and `make` are sufficient.

```sh
make
make check
make install PREFIX=/usr/local
```

The geometry and presentation are intentionally separate. `Prototile`,
`Similarity`, `Patch`, `SubstitutionRule`, `SeedPatch`, and `TilingSystem` are
first-class values. A generic engine iterates the same patch type used for
named seeds and rule replacements; a catalogue owns names, aliases, defaults,
and depth limits. `SystemReader` and `SystemWriter` are the serialisation
boundary, while `RuleLibrary` recursively composes validated files with legacy
fallbacks. Thus file-defined, searched, and built-in literal rules meet in the
same object model, and neither iteration nor rendering switches on a tiling
name. Arbitrary polygons use the unspecialised
`generic_polygon` presentation role; their `PrototileId` remains their semantic
identity. Projection objects assemble construction triangles into familiar
visible tiles only for patch presentation.

A candidate retains every structural validation diagnostic before a catalogue
accepts it, and catalogue references remain stable as more candidates are
appended. `DiscoveryEngine` accepts candidates incrementally from a
`CandidateSource`, applies incidence and area tests, invokes the stricter
`GeometricValidator`, checks several generations, and removes duplicate
`canonical_key()` serialisations. Generated candidates, accepted candidates,
and expanded tiles all have independent bounds. File-defined literal
partitions also pass the strict validator. Some legacy presentation systems
use overlapping construction pieces that are projected or deduplicated later,
so they retain structural rather than literal-partition validation.

Each encoded system also carries structured `SourceReference` provenance.
`KnownTilingBank` builds an in-memory canonical-key index over catalogue-owned
systems, so discovery can reject exact rediscoveries without coupling geometry
to the separately licensed website snapshot. A candidate source may supply its
own equivalence when the generic key is too broad: the binary-square classifier
uses one shared $D_4$ frame for both parents, which distinguishes the
parent-forgetting $(6,6)$ rule from the Thue–Morse $(6,9)$ rule.

P1 uses the six-tile
pentagonal inflation with a golden-ratio-squared linear factor; P2 and P3 use
ordered Robinson subdivisions whose handedness carries their matching rules.
The Penrose implementations follow the Tilings Encyclopedia entries for
[pentagon-boat-star](https://tilings.math.uni-bielefeld.de/substitution/penrose-pentagon-boat-star/),
[kite-and-dart](https://tilings.math.uni-bielefeld.de/substitution/penrose-kite-dart/),
and [rhombs](https://tilings.math.uni-bielefeld.de/substitution/penrose-rhomb/).
Ammann–Beenker keeps the Encyclopedia's hidden
[triangle roles](https://tilings.math.uni-bielefeld.de/substitution/ammann-beenker-rhomb-triangle/)
until complete squares can be paired; unmatched half-squares at a finite
seed's boundary are omitted. Pinwheel uses the exact rational
[five-triangle rule](https://tilings.math.uni-bielefeld.de/substitution/pinwheel/)
and colours its growing set of orientations. Stampfli 12-fold 1 follows the
[three-tile rule](https://tilings.math.uni-bielefeld.de/substitution/stampflis-12-fold-1/),
deduplicating shared boundary tiles in finite symmetric seeds. The
[two-dimensional Thue–Morse rule](https://tilings.math.uni-bielefeld.de/substitution/thue-morse-2d/)
uses complementary checkerboard blocks on two semantic square types. Chair is
the independently rediscovered L-triomino rep-tile; Domino follows the exact
four-map example in the Encyclopedia's
[substitution glossary](https://tilings.math.uni-bielefeld.de/glossary/substitution/).
A compact PDF writer applies the selected colour scheme.
