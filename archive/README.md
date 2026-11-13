# Archive

`2023-coursework/` is the project as it was handed in in February 2023,
kept unmodified for reference. Nothing here is built, tested, formatted or
linted, and none of it is used by the current code.

The submission was written by **Dmytro Palahin** with a classmate for the
*Projet Informatique* course of the second preparatory year (CP2I2) at Sup
Galilée, Université Sorbonne Paris Nord, taught by Frédéric Roupin.

| Path | Content |
| --- | --- |
| `src/Function_Ant*.c`, `Function_Ant.h` | The version that was submitted: roulette-wheel selection, helper functions in a separate file |
| `src/Ant_Roulette_Probability_File.c` | The same algorithm as a single file |
| `src/Ant_Roulette_Probability_File_Generator.c` | Variant that draws random cities and plots the tour through a gnuplot pipe (Windows only) |
| `src/Ant_Max_Probabilty.c` | First experiment: always move to the most probable city |
| `src/build-*.txt` | The original compilation notes |
| `gnuplot/` | Sample gnuplot scripts and data supplied with the assignment |
| `screenshots/` | Tours obtained in 2023, named `n-<cities>-epoch-<iterations>` |
| `report/report-2023-fr.pdf` | The original report, in French |

The instances that came with the assignment now live in
[`data/course`](../data/course).

A few files are kept on the maintainer's machine but deliberately left out
of the repository, among them the assignment itself (course slides, which
are not ours to redistribute) and the Word source of the 2023 report.

## What changed since

The [report](../report/report.pdf) has a section on this. In short, the
original program:

- indexed its arrays from 1 to N although they had N elements, reading and
  writing out of bounds;
- kept every matrix in a variable-length array on the stack;
- drew the roulette wheel with `rand() % 100`, so probabilities below 1 %
  were rounded away, and (in the submitted version) never seeded the
  generator;
- had the instance, the parameters and the output file names hard-coded,
  and had to be recompiled to change any of them;
- did not check that its input files could be opened or were well formed;
- existed as four diverging copies of the same `main` function, 290 to 440
  lines each.
