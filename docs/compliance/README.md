# Compliance documentation

Validation sets that attest `src/` against the specification and the test
suite. Entry point for compliance material.

## Checklist

- [`src-checklist.md`](src-checklist.md) — one row per tracked source file in
  `src/`, grouped by directory. Each row: file path, first 16 hex of its
  SHA-256, a `Verified` checkbox, and notes (including which `cs3-tests`
  suites exercise it).

## Semantics

- **SHA-256** merely *detects change*. It is not an attestation by itself.
- **Verified (`[x]`)** means: at the checkpoint recorded in the table header,
  the file was built and the affected test suites passed
  (`cmake --build build && ctest --test-dir build`).
- **Never bump a hash before testing.** Testing proves correctness; the hash
  only records the exact bytes that were proven.

## Procedure on any change

1. Rebuild and re-run the affected suites (`itools/run_tests.py`).
2. Flip the relevant `[ ]` to `[x]` in `src-checklist.md`.
3. Regenerate hashes and the checkpoint table:
   `python3 itools/gen_compliance.py` (preserves checkboxes and notes).

## Baseline

The initial checkpoint (2026-11-16) records hashes with `cs3-tests` green
(22/22). Files not directly exercised by any suite stay `[ ]` until reviewed
or covered.
