# aper

`aper` draws finite P2 and P3 Penrose tilings from a choice of starting designs.

It is a small, dependency-free C++20 program for the shell. Deterministic,
native vector PDF goes to standard output; diagnostics go to standard error.

```sh
make
./aper > tiling.pdf
open tiling.pdf
```

## Use

```text
aper [-t p2|p3] [-s seed] [-c scheme] [-n depth]
aper -h | --help
aper -V | --version
```

- `-t type` or `--tiling type` selects `p2` kite-and-dart or `p3` rhombs.
  The default is `p3`.
- `-s seed` or `--seed seed` selects the starting design. P2 provides `sun`,
  `star`, `ace`, `deuce`, `jack`, `queen`, and `king`; P3 provides `sun`,
  `star`, `thin`, and `thick`. The default is `sun`; the P3 `thin` seed needs
  a depth of at least 2.
- `-c scheme` or `--colour scheme` selects `flare`, `grove`, `electric`, or
  `tide`. The default is `flare`.
- `-n depth` or `--depth depth` sets the number of Robinson subdivisions,
  from 1 through 12.
  The default is 7.
- `-h` or `--help` prints a concise help message.
- `-V` or `--version` prints the version.

## Designs

The seven P2 seeds are the classic vertex-centred Penrose patches. The two
P3 prototile seeds complement its symmetric sun and star arrangements.

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

The geometry and presentation are intentionally separate. The selected seed and
family-specific Robinson substitution generate kites and darts or thin and thick
rhombs; their matching rules are implicit in the construction. A compact PDF
writer applies the selected colour scheme.
