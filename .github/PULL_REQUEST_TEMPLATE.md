## What and why

<!-- What does this change do, and what problem does it solve? Link the issue if there is one. -->

## How it was tested

<!-- New or changed tests, commands you ran, instances you tried. -->

## Checklist

- [ ] `cmake --workflow --preset dev` passes (build with warnings as errors, all tests)
- [ ] `cmake --workflow --preset sanitize` passes if the change touches C code
- [ ] `cmake --build build/dev --target format` has been run
- [ ] Public API changes are documented in the headers and listed in `CHANGELOG.md`
- [ ] Results quoted in the README or the report still hold (re-run `scripts/run-experiments.sh` if the algorithm changed)
