# Release notes

## Portfolio milestone — Stages 6–9

Commit `49cd244` passed native Windows/Linux checks and the Pages build/deploy
workflow. The hosted demo passed live query and recovery checks on 8 November
2026 (Adelaide). See [verification and project summary](docs/project-summary.md).

Stage 8 caches scoring postings per query; measured before/after results and
limits are recorded in [the performance report](docs/performance-stage8.md).
Stage 9 consolidates documentation only. No version tag or binary release is
created by this milestone. Earlier stage entries below are historical records.

## Unreleased — GitHub Pages deployment (Stage 6D)

- Add a separate Pages workflow with Emscripten 4.0.10 and native/WASM checks.
- Gate publication on browser regressions under the repository URL prefix.
- Package only explicitly named demo assets and synthetic samples; exclude tests.
- Restrict deployment to main and isolate Pages/OIDC write permissions.
- Document Pages setup, local package checks and first-live verification.
- Hosting and live verification completed; see the milestone evidence above.


## Unreleased — interactive browser demonstration (Stage 6C)

- Add a responsive, keyboard-accessible local demo using the real WebAssembly engine.
- Show sample queries, suggestions, scores, snippets, matched-term counts and phrase counts.
- Include query help, inspectable sample texts, loading/error states and reload recovery.
- Render query/document content as text and search only after all four samples load.
- Add a dependency-free browser regression page and local HTTP serving instructions.
- Keep native builds and CI unchanged; no public deployment is included.


## Unreleased — optional WebAssembly bridge (Stage 6B)

- Add an Emscripten 4.0.10 build producing an ES module and WebAssembly binary.
- Expose in-memory corpus loading and full structured search responses to JS.
- Rebuild index/trie pairs together and provide explicit engine disposal.
- Add Node/native parity checks and bridge validation/lifecycle tests.
- Keep native builds and CI unchanged; the browser UI follows in Stage 6C (above).

This incremental patch follows Stage 6A, which the user verified on Windows
with symlink checks skipped. Stage 6B built with Emscripten 4.0.10 on Linux and
passed 112 native comparisons plus validation/reload/disposal checks under Node
24.22.1. Native Windows execution of the optional toolchain remains pending.

## Unreleased — in-memory ingestion (Stage 6A)

- Add `TextDocument`, `Indexer::addDocumentText` and `buildIndexFromText`.
- Share stream tokenisation between file and text loading while preserving the
  existing native CLI and file-loading signatures.
- Define opaque document IDs, last-write-wins replacement, valid empty text,
  empty-batch clearing and rejection of empty IDs before corpus mutation.
- Add file/text equivalence tests for scores, ordering, phrases, matched terms,
  positions, snippets, suggestions and query errors, plus replacement/reset cases.
- Document trie rebuilding and a standalone C++ API example. Browser bindings
  and WebAssembly builds were deferred to Stage 6B (above).

Baseline: Stage 5 commit `f20a4fb`, whose [Linux and Windows CI jobs passed](https://github.com/BrandonHo419/ProjectSearchEngine/actions/runs/42305267826).
Stage 6A passed local Linux checks and a user-provided native Windows regression
run (symlink cases skipped). Its CI verification remains pending until committed
and pushed; it does not inherit the baseline commit's CI result.

## Native portfolio release preparation (Stages 1–5, unreleased)

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


## Unreleased — reproducible native benchmarks (Stage 7)

- Add deterministic in-memory synthetic corpora and eight query workloads.
- Measure fresh indexing/trie builds and warm query-plus-snippet batches.
- Retain raw timings, fixture fingerprints, source hashes and environment details.
- Add fixture semantics and report-accounting checks; keep timing out of CI gates.
- Document a Linux baseline and a focused query-scaling hypothesis for Stage 8.
- Preserve engine behavior, dependencies, native/WASM builds and CI configuration.
