# Executable rule library

Each `.aper` file is a complete substitution system: metadata, polygonal
prototiles, child similarities, and seeds. Nothing in this directory is drawing
code, and adding a definition does not require a renderer or iterator branch.

```sh
./aper --file rules/pentomino.aper --rule > rule.pdf
./aper --file rules/pentomino.aper --depth 4 > patch.pdf
make rules-check
```

`RuleLibrary` discovers definitions recursively and adds the six legacy C++
systems only as fallbacks. `tools/check-rule-library` rejects duplicate IDs,
invalid geometry, unstable normalised output, and incomplete PDF rendering.

Format v1 represents finite planar polygon substitutions with one scalar
inflation and similarity transforms. Decorations, curved or fractal boundaries,
multiscale replacements, and infinite type families remain explicit model
extensions rather than approximate encodings.
