# Instances

Every file here can be passed straight to `aco-tsp solve`. The two formats
are described in [docs/file-formats.md](../docs/file-formats.md).

## `course/`: the instances of the 2023 assignment

Symmetric distance matrices in the triangular format, renamed from the
original `TSP_5.txt`, `TSP.10` ... `TSP.100` so that they sort naturally. They are *not*
Euclidean: the distances are arbitrary integers, so the triangle inequality
does not hold and there are no coordinates to plot.

| File | Cities | Distances | Best known tour | Proven optimal |
| --- | ---: | --- | ---: | :---: |
| `tsp005.txt` | 5 | 3 to 77 | 64 | yes |
| `tsp010.txt` | 10 | 1 to 5 | 10 | yes |
| `tsp020.txt` | 20 | 1 to 98 | 201 | yes |
| `tsp030.txt` | 30 | 1 to 100 | 240 | no |
| `tsp040.txt` | 40 | 1 to 100 | 263 | no |
| `tsp050.txt` | 50 | 1 to 100 | 254 | no |
| `tsp100.txt` | 100 | 1 to 1000 | 2552 | no |

The optima of the first three were proven with `--algorithm exact`. The
other values are the shortest tours found during the experiments of the
report (Ant System with 2-opt, 20 seeds); a shorter tour may exist.

## `random/`: reproducible Euclidean instances

Cities drawn uniformly from the integer grid `[0, 1000]^2`, in TSPLIB
`EUC_2D` format. They were produced by the tool itself and can be
regenerated bit for bit on any platform:

```bash
aco-tsp generate --cities 50  --seed 50  --output data/random/random050.tsp
aco-tsp generate --cities 100 --seed 100 --output data/random/random100.tsp
aco-tsp generate --cities 200 --seed 200 --output data/random/random200.tsp
```

`random100.tsp` is the instance used for the parameter study, the
convergence curves and the tour pictures of the report and the README.

## Larger benchmarks

The classic [TSPLIB](http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/)
instances with `EUC_2D`, `CEIL_2D` or `EXPLICIT` distances (for example
`berlin52`, `eil76`, `kroA100`, `gr24`) load as they are and come with
published optima. They are not redistributed here.
