# Contributing

Thank you for taking the time to contribute. Bug reports, ideas, fixes and
new features are all welcome. By participating you agree to follow the
[code of conduct](CODE_OF_CONDUCT.md).

## Reporting a problem

Open an [issue](https://github.com/DmytroPalahin/aco-tsp/issues/new/choose)
with the exact command line, **including `--seed`**, and the instance file.
Runs are deterministic for a given seed, so that is usually all it takes to
reproduce a problem. Security issues go through a private channel instead:
see [SECURITY.md](SECURITY.md).

## Getting set up

You need a C11 compiler (GCC, Clang or MSVC) and CMake 3.25 or newer.

```bash
git clone https://github.com/DmytroPalahin/aco-tsp.git
cd aco-tsp
cmake --workflow --preset dev
```

That single command configures a debug build with warnings treated as
errors, compiles it and runs every test. The other presets:

| Preset | Purpose | Command |
| --- | --- | --- |
| `dev` | Debug build, warnings are errors | `cmake --workflow --preset dev` |
| `release` | Optimised build | `cmake --workflow --preset release` |
| `sanitize` | AddressSanitizer + UndefinedBehaviorSanitizer | `cmake --workflow --preset sanitize` |
| `coverage` | Instrumented for coverage | `cmake --preset coverage` |
| `fuzz` | libFuzzer harness for the parser (Clang) | `cmake --preset fuzz` |

Build trees live in `build/<preset>`.

### Useful commands

```bash
# Run one test program, or one test of it
build/dev/tests/test_colony
build/dev/tests/test_colony test_same_seed_gives_the_same_run

# Run the tests of one kind
ctest --preset dev --label-regex unit
ctest --preset dev --label-regex cli

# Format the sources, check them with clang-tidy
cmake --build build/dev --target format
find src tests examples -name '*.c' | xargs clang-tidy -p build/dev --quiet

# Fuzz the parser for a minute
cmake --preset fuzz && cmake --build --preset fuzz
mkdir -p /tmp/corpus && cp fuzz/corpus/* /tmp/corpus/
build/fuzz/fuzz/fuzz_instance_parse -max_len=2048 -max_total_time=60 /tmp/corpus

# Build the API reference (Doxygen) and the report (latexmk)
cmake --build build/dev --target docs
cmake --build build/dev --target report
```

## Making a change

1. Create a branch from `main`.
2. Make the change, **with tests**. A bug fix starts with a test that fails
   without the fix.
3. Run `cmake --workflow --preset dev` and, for C code,
   `cmake --workflow --preset sanitize`.
4. Format with `cmake --build build/dev --target format`. CI checks the
   result with clang-format 23, whose output can differ from other versions.
5. Add a line to the `[Unreleased]` section of [CHANGELOG.md](CHANGELOG.md)
   if users can notice the change.
6. Open a pull request and fill in the template. CI must be green.

### If the algorithm changes

The numbers in the README and the report come from
`scripts/run-experiments.sh`. If a change alters the tours the solver
finds, regenerate them and rebuild the report in the same pull request:

```bash
cmake --workflow --preset release
scripts/run-experiments.sh build/release/aco-tsp
cmake --build build/release --target report
cp build/release/report/report.pdf report/report.pdf
```

## Code style

The formatter settles the layout. Beyond that:

- **C11, standard library only.** No compiler extensions, no POSIX-only or
  Windows-only functions in the library; the code must build with GCC,
  Clang and MSVC.
- **Public names start with `aco_`** (`ACO_` for macros). Everything else
  is `static`, or declared in `src/internal.h` if several files need it.
- **Errors are returned, not printed.** Library functions return an
  `aco_status`; they never write to `stderr` and never call `exit`.
- **Validate at the boundary.** Arguments coming from outside (files,
  command line, public API) are checked once, where they enter; inner
  functions rely on the invariants and state them with `assert`.
- **No allocation in the hot loop.** A colony allocates when it is created
  and never again.
- **Comments say why.** What the code does should be readable from the
  code; a comment earns its place by explaining a reason, an invariant or a
  non-obvious consequence.
- **Every public function is documented** in its header, in Doxygen
  syntax: what it does, what it requires, what it returns on failure.

## Commit messages

Write the subject in the imperative, under about 70 characters, and explain
the *why* in the body when it is not obvious:

```text
Reject TSPLIB files that repeat a section

A second NODE_COORD_SECTION used to overwrite the first one and leak it.
The parser now reports the duplicate with its line number.
```

## Releasing (maintainers)

1. Move the `[Unreleased]` entries of the changelog under a new version
   heading and update the comparison links at the bottom.
2. Set the same version in `project(... VERSION x.y.z)` in `CMakeLists.txt`
   and in `CITATION.cff`.
3. Merge, then tag the merge commit `vx.y.z` and push the tag.

The release workflow checks that the tag, the CMake version and the
changelog agree, builds the packages for Linux, macOS and Windows, typesets
the report and publishes the release.
