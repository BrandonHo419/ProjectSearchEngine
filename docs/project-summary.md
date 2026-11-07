# Project Search Engine — portfolio summary

[Try the live demo](https://brandonho419.github.io/ProjectSearchEngine/demo/) ·
[Build and use the CLI](../README.md) · [Architecture](architecture.md)

A C++17 search engine that indexes local text, evaluates term, phrase, prefix
and Boolean queries, and explains ranked results. An optional Emscripten bridge
runs the same C++ implementation in the browser over four synthetic documents.

## Architecture and decisions

- A positional inverted index records document frequencies and token positions.
  Positions support exact phrases and match-based snippets.
- A trie supports prefix expansion. TF-IDF ranks matching documents; deterministic
  document-ID tie-breaks make equal scores reproducible.
- Structured query responses separate engine behavior from CLI/browser rendering.
  The JavaScript wrapper owns native handles and validates corpus inputs.
- The browser loads all four documents before enabling search and renders text
  through text nodes. Static Pages hosting supplies assets; queries run locally.
- Stage 8 retrieves each distinct scoring term's postings once per query while
  retaining the original term order and repeated-term score contributions.

## Performance evidence

At 1,000 synthetic documents of 128 words, recorded Linux medians changed from
70.62 to 1.14 ms for a common term and 301.36 to 4.23 ms for the tested prefix.
Measurements include returned-result snippets. These are sequential runs on one
environment with a fixed 256-token filler vocabulary, not production guarantees.
See [method and original baseline](performance.md) and [after results](performance-stage8.md)
for raw samples, source hashes, compiler/environment and limitations. Peak memory
and Windows performance have not been established by these reports.

## Verification

Verified source commit: `49cd244f87437e289c349aeabe7f4ac7d0290c98`.

- [Native Windows and Linux workflow](https://github.com/BrandonHo419/ProjectSearchEngine/actions/runs/42378391186): passed.
- [Pages build and deployment](https://github.com/BrandonHo419/ProjectSearchEngine/actions/runs/42378391080): passed, including native regressions, 112 native/WASM comparisons, packaging tests and 55 packaged browser checks.
- Live interaction checked on 8 November 2026 (Adelaide): engine ready with four
  documents; `fo*` returned three results with `testfile3.txt` first; the quoted
  phrase `"quick brown fox"` returned two results with one occurrence each;
  `fox AND` reported validation failure; `unfindableterm` returned no matches;
  returning to `fo*` recovered correctly.
- Stage 8 local verification also included 72 exact before/after response comparisons.

The workflow evidence is tied to the commit above. Future changes need their own
successful runs. This milestone does not create a Git tag, packaged executable
or licence. Historical release-note entries retain their original stage scope.

## Interview explanation

“I extended a university group search engine into a tested C++17 portfolio
project with a native CLI and a browser demonstration using WebAssembly. I
added structured query diagnostics, result explanations and reproducible
benchmarks. The benchmarks exposed poor scaling in common-term and prefix
queries. I found repeated postings-map copies inside ranking and introduced a
query-local cache, preserving score accumulation and validating full responses
before and after the change. I also built a gated static deployment workflow.”

The original group project was created by Brandon Ho, Duy Do and Zixi Khoo.
Brandon's subsequent work is documented in the commit history and release notes.

## Limitations and next decisions

Boolean evaluation is left to right; parentheses and mixed phrase/prefix
expressions are unsupported. Token handling is byte-oriented, without stemming
or full Unicode linguistic processing. Indexing and searching are synchronous.
The public demo searches only bundled synthetic documents and offers no upload
or search backend. A top-ten result limit still requires candidate evaluation.

Next proposed work: broaden benchmark vocabulary/selectivity coverage and measure
peak memory before considering larger browser corpora or further optimization.
These changes require a separately approved scope.
