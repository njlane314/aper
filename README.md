# aper

`aper` renders Penrose, Ammann–Beenker, Pinwheel, Stampfli 12-fold 1, and
two-dimensional Thue–Morse substitution systems as dense patches or rule
sheets. `aper-search` runs a small, bounded search for candidate substitution
rules and compares them with the executable reference bank.

They are small, dependency-free C++20 tools for the shell. Deterministic,
native vector PDF goes to standard output; diagnostics go to standard error.

```sh
make
./aper > patch.pdf
./aper -t p1 --rule > rule.pdf
./aper-search --rule > square-rule.pdf
```

## Use

```text
aper [-t type] [-s seed] [-c scheme] [-n depth] [-r]
aper -h | --help
aper -V | --version
```

- `-t type` or `--tiling type` selects `p1` pentagon-boat-star, `p2`
  kite-and-dart, `p3` rhombs, `ammann-beenker`, `pinwheel`, `stampfli`, or
  `thue-morse-2d`. Short and descriptive aliases include `ab`,
  `pentagon-boat-star`, `kite-dart`, `rhomb`, `stampfli-12-fold-1`, and
  `thue-morse`. The default is `p3`.
- `-s seed` or `--seed seed` selects the patch's starting design. P1 provides
  `pentagon-5`, `pentagon-3`, `pentagon-2`, `diamond`, `boat`, and `star`;
  P2 provides `sun`, `star`, `ace`, `deuce`, `jack`, `queen`, and `king`;
  P3 provides `sun`, `star`, `thin`, and `thick`; Ammann–Beenker provides
  `octagon`, `square`, and `rhomb`; Pinwheel provides `triangle`; Stampfli
  12-fold 1 provides `dodecagon`, `triangle`, `square`, and `rhomb`; and
  Thue–Morse 2D provides `a` and `b`. Their defaults are `pentagon-5`, `sun`,
  `sun`, `octagon`, `triangle`, `dodecagon`, and `a`, respectively. The P3
  `thin` seed needs a depth of at least 2.
- `-c scheme` or `--colour scheme` selects `flare`, `grove`, `electric`, or
  `tide`. The default is `flare`.
- `-n depth` or `--depth depth` sets the number of substitutions. P1 accepts
  1 through 6 and defaults to 5; P2 and P3 accept 1 through 12 and default to
  7; Ammann–Beenker accepts 1 through 6 and defaults to 4; Pinwheel accepts
  1 through 8 and defaults to 6; Stampfli 12-fold 1 accepts 1 through 3 and
  defaults to 2; Thue–Morse 2D accepts 1 through 7 and defaults to 4.
- `-r` or `--rule` draws a sheet containing every prototile and its one-step
  substitution instead of a patch. The tiling and colour options still apply;
  seed and depth are patch-only options.
- `-h` or `--help` prints a concise help message.
- `-V` or `--version` prints the version.

For example, render the Pinwheel substitution rule:

```sh
./aper -t pinwheel --rule -c tide > pinwheel-rule.pdf
```

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

`aper-search` implements a complete discovery loop over two intentionally small
control spaces. `square` has one unit-square prototile. `binary-square` has two
semantic square types, $A$ and $B$, and exhausts every pair
$\sigma_A,\sigma_B \in \{A,B\}^{2\times2}$ at inflation $\lambda=2$.

```text
exact cover → structural and area tests → strict geometry
            → multi-generation checks → canonical key → PDF
```

```sh
./aper-search > square-candidate.pdf
./aper-search --rule > square-rule.pdf
./aper-search --space binary-square --list-candidates
./aper-search --space binary-square --candidate 22 > binary-patch.pdf
./aper-search --space binary-square --candidate 22 --rule > binary-rule.pdf
```

```text
aper-search [--space name] [--candidate n] [-c scheme] [-n depth] [-r]
aper-search [--space name] [--candidate n] --classify
aper-search [--space name] --list-candidates
aper-search -l | --list-known
aper-search -h | --help
aper-search -V | --version
```

Patch depth is 1--7 (default 4); `--rule` renders the rule instead.

The one-type exact-cover search rediscovers the ordinary periodic square grid.
The binary search examines all 256 raw rules; 224 have primitive incidence
matrices and pass three generations of exact square-partition tests. One shared
square symmetry in $D_4$, together with the global exchange $A\leftrightarrow
B$, reduces these to 27 rule classes. The full result is deterministic:

```text
256 generated → 224 primitive and geometrically valid → 27 unique
                                                    → 1/7 exact bank matches
```

`GeometricValidator` rejects non-simple polygons, escaped or overlapping
children, gaps, area errors, and unmatched atomic edges. Source-specific
canonicalisation is important here: applying a different rotation to each
parent would incorrectly merge distinct symbolic rules. The search instead
uses one global $D_4$ frame and one global type relabelling.

These are control experiments, not 27 claims of new geometric tilings: after
forgetting $A/B$, every patch is the periodic square grid. Candidate 22
rediscovers the known two-dimensional Thue–Morse rule; its exact match is
computed with the same global $D_4$/type-swap equivalence as the search.
An unmatched fingerprint says nothing about the other 250 reference-only bank
records, alternate presentations, mutual local derivability, or aperiodicity.
The result instead establishes a tested seam for searches on triangular
lattices, polyominoes, or exact algebraic coordinates. The known P3 rule is
also passed through the strict multi-generation validator as a regression
control, but is not yet reconstructed by enumeration.

### Known-rule bank

The bank has two deliberately separate layers:

- the included `data/encyclopedia.tsv` snapshot indexes 257 Encyclopedia
  pages, with titles, page URLs at the snapshot date, and classification tags;
- `KnownTilingBank` fingerprints seven locally encoded `TilingSystem` objects
  linked to their corresponding Encyclopedia records.

```sh
./aper-search --list-known
./aper-search --classify
rg -i 'chair|rhomb|pinwheel' data/encyclopedia.tsv
```

`--classify` reports canonical equality with an encoded substitution rule. Its
square control currently prints `no-exact-match`; that means only that it is
absent from the seven-rule executable bank, not that the periodic square grid
is new. Names, refinements, projected presentations, MLD equivalence, and
aperiodicity require broader analysis.

Refresh the metadata snapshot with one network request:

```sh
make encyclopedia-bank
```

The Encyclopedia provides diagrams rather than machine-readable child
transforms. Its metadata snapshot is therefore reference-only; a page enters
the executable bank only after its rule is reconstructed independently and
passes structural validation. Literal partition rules can additionally pass
`GeometricValidator`; projection-based presentation systems may contain
overlapping construction pieces by design. The snapshot has its own attribution
and CC BY-NC-SA 2.0 notice in `data/encyclopedia.NOTICE.md`; the `aper` code
remains ISC-licensed. Because that licence is non-commercial and share-alike,
the data is installed only on request with `make install-encyclopedia`.

### Complete catalogue

The opt-in catalogue places every patch directly beside its substitution rule
on an A4 landscape plate:

```sh
make catalogue          # populate the cache as needed, then build offline
make catalogue-offline  # require an already complete cache
```

The document contains all 257 records in the metadata snapshot, followed by all
27 canonical binary-square search survivors. Seven bank plates are regenerated
as native vector PDFs from executable `TilingSystem` objects. The other 250 use
checksummed Encyclopedia reference artwork: these are visual references, not
machine-readable rules reconstructed by `aper`. Every record has a published
patch; 15 source pages provide no rule image and therefore receive an explicit
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
seeds complement its symmetric sun and star arrangements. The four added
families use only convex triangles and quadrilaterals; Thue–Morse uses two
semantic types of the same square.

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
| **Square search control · Electric** | **Discovered square substitution · Electric** |
| ![Square-grid control patch discovered by aper-search](doc/aper-search-square-patch-electric.png) | ![Square subdivision discovered by aper-search](doc/aper-search-square-rule-electric.png) |
| `./aper-search -c electric > square.pdf` | `./aper-search --rule -c electric > square-rule.pdf` |

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
and depth limits. Arbitrary discovered polygons use the unspecialised
`generic_polygon` presentation role; their `PrototileId` remains their semantic
identity. Projection objects assemble construction triangles into familiar
visible tiles only for patch presentation.

A candidate retains every structural validation diagnostic before a catalogue
accepts it, and catalogue references remain stable as more candidates are
appended. `DiscoveryEngine` accepts candidates incrementally from a
`CandidateSource`, applies incidence and area tests, invokes the stricter
`GeometricValidator`, checks several generations, and removes duplicate
`canonical_key()` serialisations. Generated candidates, accepted candidates,
and expanded tiles all have independent bounds. The strict validator remains
search-only: presentation systems may legitimately use overlapping
construction pieces that are projected or deduplicated later.

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
uses complementary checkerboard blocks on two semantic square types. A compact
PDF writer applies the selected colour scheme.
