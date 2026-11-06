# Architecture and design notes

Project Search Engine is a local C++17 portfolio application for plain-text
search. Its current interface is a native CLI; it is not a hosted service.

## Responsibilities

| Component | Responsibility | Main interfaces |
| --- | --- | --- |
| `main.cpp` | Parse CLI options, discover files, construct the index/trie, handle interactive commands and format output. | `parseOptions`, `collectFiles`, `runQuery` |
| `Index.h` / `Index.cpp` | Retain positional postings, document lengths and token text; produce snippets. | `Indexer::buildIndex`, `searchWord`, `getSnippetFromSpans` |
| `ExactSearcher.hpp` | Retrieve term postings, combine sorted document lists and extend phrase positions. | `search`, `intersectPostings`, `unionPostings`, `searchPhrasePositions` |
| `PrefixSearcher.hpp` | Store vocabulary in a character-keyed trie, enumerate words and aggregate prefix postings. | `insert`, `getSuggestions`, `searchPrefix` |
| `query_processor.hpp` | Validate syntax, select candidates, rank documents and produce structured results. | `executeQuery`, `QueryResponse`, `SearchResult` |
| `tests/search_tests.cpp` | Exercise engine behaviour and launch the real CLI with controlled fixtures. | Regression executable |

## Index construction

1. The CLI gathers explicit files or immediate `.txt` directory entries.
   It deduplicates canonical paths while retaining the first display spelling.
   No explicit input means the bundled sample paths are used.
2. `Indexer::buildIndex` clears its previous data and reads the files. Tokens
   are whitespace-separated. `normalizeWord` lowercases bytes and removes
   punctuation except apostrophes and hyphens; empty normalised tokens are skipped.
3. Each indexed term maps to documents, occurrence counts and zero-based token
   positions. Document lengths count indexed tokens. Stored token text supports
   snippets, but original whitespace is not retained.
4. The CLI inserts the index's terms and postings into a new `PrefixSearcher`.
   Its children use `std::map` and `std::unique_ptr`, avoiding the old assumption
   that every character fits an `a`–`z` array index.

The index and trie must describe the same corpus. A caller rebuilding the index
must also construct and populate a fresh trie; there is no automatic
synchronisation or incremental-update contract. Empty readable files count in
the document total but have no postings.

## Query execution

`executeQuery(input, index, prefixSearcher, topK)` trims input and validates the
supported form before returning results. Forms are a term/Boolean sequence,
one complete quoted phrase, or one standalone prefix. Boolean operations run
left to right, with adjacent terms treated as `AND`.

Term and Boolean queries combine postings. Phrase matching carries forward
only positions that extend a complete consecutive phrase, including overlapping
occurrences. Prefix queries collect documents from the trie and expand the
prefix into vocabulary terms for scoring and suggestions.

Matching documents are ranked using the [README's TF-IDF formula](../README.md#ranking).
Repeated query terms retain their repeated score contribution. Exclusion terms
do not contribute. Document path breaks score ties deterministically.

## Response contract

| Field or type | Meaning |
| --- | --- |
| `QueryResponse::ok()` | Whether query validation succeeded; an empty result list may still be valid. |
| `error`, `message` | Error category and explanation for invalid syntax. |
| `results` | Ranked results, limited by `topK`; non-positive limits return none after validation. |
| `suggestions` | Vocabulary expanded by a valid prefix, independent of `topK`; otherwise empty. |
| `SearchResult::matches` | Positive matched terms with whole-document counts and token positions. |
| `SearchResult::spans` | Half-open token ranges `[start, end)` for snippet generation. |
| `isPhrase`, `phraseOccurrences` | Whether the result is a phrase match and its complete occurrence count. |

The CLI displays this response without reinterpreting prefix syntax.
`getSnippetFromSpans` adds two context words by default, merges overlapping or
adjacent windows and returns up to three excerpts in document order. A full
phrase span remains intact; merged excerpts have no character cap.

Compatibility helpers `processQueryDetailed` and `processQuery` retain their
existing vector return types. They cannot distinguish invalid syntax from
valid empty results; new diagnostic-aware callers should use `executeQuery`.
The legacy query-string `getSnippet` remains available, but the CLI uses spans.

## Trade-offs and limits

- Rebuilding an in-memory index on every invocation keeps storage simple, at
  the cost of repeated indexing. There is no saved index or benchmark claim.
- The trie duplicates some posting information for prefix retrieval. This is
  a design trade-off, not a measured performance improvement.
- Byte-oriented normalisation is not Unicode linguistic processing. Native
  Windows narrow-character paths also limit Unicode filename support.
- Directory traversal is intentionally non-recursive. PDF, Word, stemming,
  fuzzy matching and Boolean parentheses are not implemented.
- Query execution returns data without printing it, but the overall engine
  is not yet fully independent of filesystem I/O: `Indexer::addDocument` reads
  files and prints open-failure messages. There is no in-memory text ingestion
  interface or WebAssembly adapter yet.

A browser demonstration should reuse the C++ matching and ranking code. Its
next design decision is how to supply a small, non-sensitive corpus and expose
responses to JavaScript while preserving the native interface. Deployment,
packaged binaries and licensing remain separate decisions.
