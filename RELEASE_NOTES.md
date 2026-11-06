# Release notes

## Unreleased — native portfolio release preparation

This describes the current native implementation and prepares release material.
It does not announce a tagged release, downloadable executable or browser demo.

### Search correctness and explanations

- Replaced unsafe fixed-alphabet prefix-trie indexing with character-keyed,
  owned children and document-level prefix aggregation.
- Added structured ranked results, whole-document matched-term counts and
  positions, complete phrase occurrence counts and match spans.
- Corrected complete and overlapping phrase matching and generated snippets
  from actual match spans, with merged context windows and excerpt limits.
- Preserved repeated-term ranking contributions and excluded negative operands
  from positive explanations.

### Query diagnostics and native input

- Added `QueryError`, `QueryResponse`, `executeQuery` and interactive `:help`.
- Distinguished malformed syntax from valid searches with no matching documents.
- Added `--file`, non-recursive `--directory`, `--query`, `--help` and `--`.
- Preserved positional files and default samples, added canonical path
  deduplication, and documented directory filtering, symlinks and exit codes.
- Returned prefix suggestions through `QueryResponse` so the CLI no longer
  repeats prefix interpretation and suggestion lookup.

### Compatibility and documentation

- Retained the existing `processQuery`, `processQueryDetailed`, `searchPhrase`
  and legacy `getSnippet` interfaces.
- Kept left-to-right Boolean evaluation and existing ranking semantics.
- Expanded engine/CLI regression coverage and native Windows build instructions.
- Added [captured demonstrations](docs/demo.md) and
  [architecture notes](docs/architecture.md).

### Verification record at preparation

| Scope | Evidence | Qualification |
| --- | --- | --- |
| Stage 3, `25f5af0` | [GitHub Actions run](https://github.com/BrandonHo419/ProjectSearchEngine/actions/runs/42037551514): Linux GCC and Windows UCRT64 passed. | Applies to that commit. |
| Stage 4 patch on Stage 3 | Linux regression suite and fresh patch application passed; 32 queries in each CLI mode matched Stage 3 output, stderr and exit codes. | Local verification, not a Stage 4 GitHub run. |
| Stage 4 on native Windows | User-provided build/test output reports `Search regression checks passed.` and the expected `fo*` suggestions/results. | Both symlink checks were skipped with `Function not implemented`; those cases are not verified by this run. |
| Stage 5 documentation | Examples captured from the Stage 4 source using Linux GCC 13.3.0; prefix output also matches the user-provided Windows output. | No application, build or CI code changes. |

At preparation, remote `main` remained at Stage 3. Stage 4 was applied locally
and verified by the user on Windows. Before tagging a release, confirm that
all intended patches are pushed and the exact release commit passes both CI
jobs. Do not treat earlier passing jobs as verification of a newer commit.

### Remaining limitations and release decisions

The index is rebuilt each run; only plain text is supported; directory scans
are non-recursive; Unicode processing is limited; phrase/prefix expressions
cannot be combined with Boolean queries. Snippets normalise whitespace and
have an excerpt limit rather than a character limit. See the
[README](README.md#current-limitations) for details.

No licence has been added. Version/tag selection, binary packaging, publication
and a browser demonstration are pending separate decisions. No executable or
release is published by this documentation update.

### Attribution

The original university project was developed by Brandon Ho, Duy Do and Zixi
Khoo. Subsequent portfolio maintenance and feature development by Brandon Ho
are recorded in repository history. Original group attribution is retained.
