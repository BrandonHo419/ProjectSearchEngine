# Architecture and design notes

Project Search Engine is a local C++17 portfolio application for plain-text
search. Its interfaces are a native CLI and an optional JavaScript/WebAssembly
bridge used by the local browser demo. It is not a hosted service.

## Responsibilities

| Component | Responsibility | Main interfaces |
| --- | --- | --- |
| `main.cpp` | Parse CLI options, discover files, construct the index/trie, handle interactive commands and format output. | `parseOptions`, `collectFiles`, `runQuery` |
| `Index.h` / `Index.cpp` | Retain positional postings, document lengths and token text; produce snippets. | `Indexer::buildIndex`, `buildIndexFromText`, `addDocumentText`, `searchWord`, `getSnippetFromSpans` |
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

## In-memory document loading

`TextDocument` contains an `id` and `text`. `addDocumentText(id, text)` adds or
replaces one document; `buildIndexFromText(documents)` replaces the entire
corpus. Both use the same stream-based tokenisation as file loading, without
opening files or printing diagnostics.

- IDs are opaque, case-sensitive, non-empty strings. They are returned unchanged
  in results and used for score tie-breaking. No path canonicalisation occurs.
- Reusing an ID replaces that document; the last occurrence in a batch wins.
  File and text loading share the same identifier namespace, so a text ID equal
  to a previously loaded file path replaces that document.
- Empty or punctuation-only text is valid: it contributes a zero-length document
  to the existing IDF calculation but has no searchable postings.
- An empty batch clears the corpus. A batch with an empty ID throws
  `std::invalid_argument` before clearing the old corpus; single additions with
  empty IDs also throw before modifying it. This validation guarantee is not
  a general transaction guarantee for allocation failures.
- Both methods consume the strings during the call; callers need not retain them.
- Build a fresh trie after any additions, replacements or corpus rebuilds. Do not
  append all new postings to an old trie, which retains old vocabulary/counts.

Minimal C++17 example, compiled with `Index.cpp`:

```cpp
#include "query_processor.hpp"

int main() {
    Indexer index;
    index.buildIndexFromText({
        {"notes:one", "Quick brown fox foxes"},
        {"notes:two", "A quiet dog"}
    });
    PrefixSearcher trie;
    for (const auto& [word, postings] : index.getIndex()) {
        for (const auto& [id, data] : postings) {
            trie.insert(word, {id, data.first});
        }
    }
    const auto response = executeQuery("fo*", index, trie, 10);
    if (!response.ok() || response.results.size() != 1) return 1;
    const auto& result = response.results.front();
    const auto snippet = index.getSnippetFromSpans(result.document, result.spans);
    return result.document == "notes:one" && snippet == "Quick brown fox foxes" ? 0 : 1;
}
```

This API supplies text to the existing matching, ranking, explanation and
snippet code. The optional [WebAssembly bridge](wasm.md) consumes this API without changing
Unicode processing. Existing file-loading signatures and CLI behaviour remain.

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
- Query execution and text ingestion operate without filesystem access. The
  existing file-loading path still reads files and prints open-failure messages.
  The optional bridge calls text ingestion directly and compiles without
  filesystem support.

The optional `wasm/bindings.cpp` bridge owns an index/trie pair per engine and
replaces them together on corpus loads. `wasm/search-engine.mjs` validates JS
arguments and manages native object lifetimes. The returned response and snippet
text are produced by the existing C++ engine, not a JavaScript reimplementation.
The `demo/` page loads the bridge dynamically, fetches the complete sample
corpus, then enables search. It uses native responses for ranking/explanations
and DOM text nodes for rendering. Query and load failures have distinct states.
On real page unload it disposes the engine; back/forward-cache restores retain
it. The browser test page compares rendered results to direct bridge calls.
See [WebAssembly build and API details](wasm.md). Deployment, packaged binaries
and licensing remain separate decisions.
