# Tilings Encyclopedia reference data

`encyclopedia.tsv` is a metadata-only snapshot derived from the
[Tilings Encyclopedia substitution index](https://tilings.math.uni-bielefeld.de/substitution/).
It contains page titles, slugs and page URLs recorded by that snapshot,
classification labels, and fifteen locally authored `aper` system mappings. It does
not contain the site's prose, images, or substitution geometry. A `-` in the
`aper_system` column denotes a reference-only record.

Please cite:

> D. Frettlöh, F. Gähler, E. Harriss: Tilings Encyclopedia,
> https://tilings.math.uni-bielefeld.de/

The derived metadata file is distributed under the source site's
[CC BY-NC-SA 2.0 licence](https://creativecommons.org/licenses/by-nc-sa/2.0/).
The transformations made by `tools/update-encyclopedia-bank` are whitespace
normalisation, sorting by slug, URL construction, and addition of `aper` system
identifiers. The independently authored `aper` source code and `.aper` rule
encodings remain under the repository's ISC licence.
