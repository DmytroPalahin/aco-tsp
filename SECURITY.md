# Security Policy

## Supported versions

| Version | Supported |
| --- | :---: |
| 2.x | yes |
| 1.x | no (archived coursework, not maintained) |

## What counts as a vulnerability

The library parses files that may come from an untrusted source. Memory
corruption, out-of-bounds access, undefined behaviour or unbounded resource
consumption triggered by a crafted instance file is a security bug, and so
is any such problem reachable through the public API with arguments that
respect its documented contract.

A wrong or poor tour is an ordinary bug: please use the issue tracker.

## Reporting

Please **do not open a public issue**. Report privately through
[GitHub security advisories](https://github.com/DmytroPalahin/aco-tsp/security/advisories/new)
and include:

- the version or commit;
- the input that triggers the problem (a fuzzer crash file is perfect);
- the command line, compiler and platform.

You can expect an acknowledgement within a week. Once a fix is ready it is
released and the advisory is published, crediting you unless you prefer to
remain anonymous.

## How the project guards against these bugs

- The parser is fuzzed with libFuzzer on every push, and the whole test
  suite runs under AddressSanitizer, UndefinedBehaviorSanitizer and
  Valgrind.
- clang-tidy and CodeQL analyse every change.
- The parser checks that a file is long enough for the instance size it
  announces before allocating memory for it, and caps that size.
