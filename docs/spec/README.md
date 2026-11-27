# Specifications

- [ipath-implementations.md](ipath-implementations.md) — current `IPath`
  pathfinding algorithms (inventory, guarantees, wiring); keep it current
  when adding an algorithm (procedure:
  [../briefs/ipath.md](../briefs/ipath.md)).

## tests

rules

- run unit tests after all implementations to avoid regressions

- use `itools/gen_coverage.py` and `gen_compliance.py` to update compliance documentation and test coverage.

- tests must be wired to the github-ci workflows 

- use valgrind to validate there are no memory leaks
