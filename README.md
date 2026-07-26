# aper

`aper` draws finite P3 Penrose tilings as PDF.

It is a small, dependency-free C++20 program for the shell. Deterministic,
native vector PDF goes to standard output; diagnostics go to standard error.

![A Penrose tiling rendered by aper](doc/aper.png)

```sh
make
./aper > tiling.pdf
open tiling.pdf
```

## Use

```text
aper [-n depth]
aper -h | --help
aper -V | --version
```

- `-n depth` or `--depth depth` sets the number of Robinson subdivisions,
  from 1 through 12.
  The default is 7.
- `-h` or `--help` prints a concise help message.
- `-V` or `--version` prints the version.

## Examples

| Depth 2 | Depth 4 | Depth 6 |
| :---: | :---: | :---: |
| ![Penrose tiling at depth 2](doc/aper-2.png) | ![Penrose tiling at depth 4](doc/aper-4.png) | ![Penrose tiling at depth 6](doc/aper-6.png) |
| `./aper -n 2 > tiling-2.pdf` | `./aper -n 4 > tiling-4.pdf` | `./aper -n 6 > tiling-6.pdf` |

The same arguments produce the same PDF, making `aper` suitable for scripts,
pipes, and generated documents.

## Build

A C++20 compiler and `make` are sufficient.

```sh
make
make check
make install PREFIX=/usr/local
```

The geometry and presentation are intentionally separate: a fixed ten-triangle
sun seed and Robinson subdivision generate the thick and thin rhombs, while a
compact PDF writer gives them their final form.
