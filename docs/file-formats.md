# File formats

`aco-tsp` reads two instance formats and tells them apart on its own: input
whose first non-blank character is a digit is *triangular*, anything else is
*TSPLIB*. Both describe a **symmetric** problem. Line endings (`LF` or
`CR LF`), blank lines, indentation and a UTF-8 byte-order mark are ignored.

Cities are numbered from **1** in every file, and from 0 in the C API.

## Triangular format

The format of the original assignment: the number of cities `n`, then the
strict upper triangle of the distance matrix, row by row.

```text
5
61 789 204 349
362 135 376
260 45
880
```

Here `d(1,2) = 61`, `d(1,5) = 349`, `d(2,3) = 362` and `d(4,5) = 880`. The
matrix is completed by symmetry and its diagonal is zero.

| Rule | Detail |
| --- | --- |
| Number of cities | an integer from 2 to 5000 |
| Number of distances | exactly `n (n - 1) / 2`; fewer or more is an error |
| Distances | non-negative, finite; integers or reals (`12`, `3.5`, `1e3`) |
| Layout | free: only the order of the numbers matters, not the line breaks |

## TSPLIB format

A subset of [TSPLIB 95](http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/),
enough for the classic symmetric benchmark instances.

```text
NAME : rectangle
COMMENT : a 3 by 4 rectangle
TYPE : TSP
DIMENSION : 4
EDGE_WEIGHT_TYPE : EUC_2D
NODE_COORD_SECTION
1 0 0
2 3 0
3 3 4
4 0 4
EOF
```

Keywords are case-insensitive and the spaces around `:` are optional.

| Keyword | Supported values |
| --- | --- |
| `TYPE` | `TSP` |
| `DIMENSION` | 2 to 5000 |
| `EDGE_WEIGHT_TYPE` | `EUC_2D`, `CEIL_2D`, `EXPLICIT` |
| `EDGE_WEIGHT_FORMAT` | `FULL_MATRIX`, `UPPER_ROW`, `LOWER_ROW`, `UPPER_DIAG_ROW`, `LOWER_DIAG_ROW` |
| `NODE_COORD_SECTION` | `id x y` for each city, in any order |
| `EDGE_WEIGHT_SECTION` | the matrix, in the declared layout |
| `DISPLAY_DATA_SECTION` | `id x y` for each city: coordinates used only for plotting |
| `NAME`, `COMMENT` and other `KEY : value` lines | read or ignored |

Distances follow the TSPLIB conventions:

- `EUC_2D`: Euclidean distance rounded to the nearest integer;
- `CEIL_2D`: Euclidean distance rounded up;
- `EXPLICIT`: the numbers of the file, as they are. A `FULL_MATRIX` must be
  symmetric; its diagonal is ignored (some files store a sentinel there).

Anything else (`ATSP`, `GEO` or `ATT` distances, three-dimensional
coordinates, other sections) is rejected with an *unsupported* error rather
than silently misread.

## What the tool writes

| Option | Content |
| --- | --- |
| `generate --format tsplib` | a TSPLIB `EUC_2D` instance, coordinates included |
| `generate --format triangular` | the same distances in the triangular format |
| `solve --tour FILE` | the tour as a TSPLIB `TOUR` file |
| `solve --trace FILE` | one CSV row per iteration |
| `solve --plot FILE` | one `x y city` line per stop, the first city repeated at the end |

A tour file:

```text
NAME : rectangle
COMMENT : length 14, found by aco-tsp
TYPE : TOUR
DIMENSION : 4
TOUR_SECTION
1
2
3
4
-1
EOF
```

A trace file, ready for any plotting tool (see `scripts/plot-trace.gp`):

```csv
iteration,iteration_best,iteration_mean,iteration_worst,global_best
1,13892,16234.5,19877,13892
2,13145,15322.1,18304,13145
```

`--plot` needs coordinates, so it works with `EUC_2D`, `CEIL_2D` and with
`EXPLICIT` instances that have a `DISPLAY_DATA_SECTION`; it is refused for
the triangular format, which has none.

## Error messages

Parse errors name the line they were found on:

```console
$ aco-tsp solve broken.txt
aco-tsp: broken.txt: line 3: expected a distance but found 'oops'
```

Exit status `1` means the file could not be read or solved, `2` that the
command line itself was wrong.
