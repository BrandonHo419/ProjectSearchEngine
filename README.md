# Project Search Engine
[![Build and regression tests](https://github.com/BrandonHo419/ProjectSearchEngine/actions/workflows/build-and-test.yaml/badge.svg)](https://github.com/BrandonHo419/ProjectSearchEngine/actions/workflows/build-and-test.yaml)

A C++17 search engine for local text files, with interactive and single-query command-line modes.

The engine builds an inverted index containing word frequencies and positions. It supports term searches, exact phrases, Boolean operations and prefix matching, then ranks matching documents using TF-IDF.

[Verified demonstration](docs/demo.md) · [Architecture](docs/architecture.md) · [Release notes](RELEASE_NOTES.md)

## Features

- Case-insensitive term searches.
- Exact phrase matching using consecutive word positions.
- Boolean `AND`, `OR` and exclusion using `-`.
- Trie-based prefix searches and word suggestions.
- TF-IDF ranking with deterministic ordering for tied scores.
- Results containing document paths, scores, word counts and snippets.
- Individual text files or non-recursive directory input.
- Interactive searches or one query per invocation with documented exit codes.
- Regression tests for core search behaviour.

## Getting started

### Requirements

- A C++17 compiler with standard filesystem support. CI uses GCC on Linux and
  MSYS2 UCRT64 GCC on native Windows.
- GNU Make for the Linux/WSL commands below. Native Windows builds use direct
  compiler commands and do not require Make.
- Run commands from the repository root so bundled sample paths resolve.

### Linux / WSL build

```bash
git clone https://github.com/BrandonHo419/ProjectSearchEngine.git
cd ProjectSearchEngine
make
```

### Native Windows (MSYS2 UCRT64)

In the MSYS2 UCRT64 terminal, install GCC if needed:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

In the UCRT64 terminal, change to the cloned repository. Build and test with
the same compiler commands used by Windows CI. These commands also work in
PowerShell when the UCRT64 compiler and its runtime DLLs are available on PATH:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic main.cpp Index.cpp -o search.exe
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -I. tests/search_tests.cpp Index.cpp -o tests/search-tests.exe
./tests/search-tests.exe
./search.exe
```

### Run with the sample documents

Run from the repository root:

```bash
./search
```

The default collection contains `testfile1.txt`, `testfile2.txt`,
`testfile3.txt` and `extreme secret.txt`. These are small sample documents.

### Run with your own documents

Supply one or more text-file paths:

```bash
./search notes.txt article.txt "project notes.txt"
```

You can also use explicit file and directory options, and run one query:

```bash
./search --help
./search --file "project notes.txt"
./search --directory "my notes"
./search --directory "my notes" --query "fox OR dog"
./search --query "fo*"
```

On native Windows, use `./search.exe` in place of `./search`.
Paths and queries containing spaces must be quoted. Quote wildcards too so
that the shell does not expand them. Query-language double quotes for phrases
must reach the executable intact; shell quoting varies by terminal.

### Input and command-line rules

- `--file PATH`, `--directory PATH` and positional files can be combined and
  repeated. Each value option consumes its next argument literally.
- Directories contribute immediate regular files with a case-insensitive `.txt`
  extension, in sorted path order. Subdirectories and symbolic-link entries
  are skipped. Explicit directory paths may resolve through symbolic links.
- Explicit files may have any extension and may be symbolic links to regular
  files. Their contents must still be plain text.
- Canonical path duplicates (including `.` aliases and resolved file symlinks)
  are indexed once. The first supplied/discovered spelling is displayed.
  Distinct hard-link paths are not deduplicated. Canonical spelling follows
  the platform filesystem; this is not Unicode or case-folded path matching.
- Empty readable files count as documents, contribute to the existing IDF
  calculation, and never match a search.
- Missing, unreadable and non-regular inputs are reported and skipped. Other
  valid inputs remain searchable. Directory traversal errors are reported;
  files discovered before an error may still be used.
- With no input paths, the bundled samples are loaded relative to the current
  working directory. Explicit inputs never fall back to the samples.
- `--query QUERY` may appear once. It runs one search and exits without reading
  standard input or displaying an interactive prompt. In this mode `x` and
  `:help` are search text, not interactive commands. Blank queries are errors.
- `--help` prints command-line and query help without accessing input files.
  Arguments must still be syntactically valid.
- `--` ends option parsing: all following arguments are literal file paths.
  For example, `./search --query fox -- --notes.txt` searches an option-looking
  filename. Unknown `--options` and missing option values are errors.

| Exit code | Meaning |
| --- | --- |
| `0` | Completed successfully, including a valid search with no matches or help. |
| `1` | No documents could be loaded. |
| `2` | Invalid command-line arguments or invalid single-query syntax. |

If some inputs fail but at least one document loads, valid searches still
return `0`. Loading is checked before single-query syntax, so no loaded
documents produces `1` even if the query is also invalid.

Enter `:help` for query examples or `x` to quit. Blank lines are ignored.
The program also exits when standard input ends.

## Search examples

| Query | Behaviour |
| --- | --- |
| `fox` | Find documents containing `fox`. |
| `quick brown` | Find documents containing both terms. |
| `fox AND dog` | Find documents containing both terms. |
| `fox OR secret` | Find documents containing either term. |
| `fox - dog` | Find documents containing `fox` and exclude those containing `dog`. |
| `"quick brown fox"` | Find the complete phrase in the specified order. |
| `fo*` | Find indexed words beginning with `fo`. |

With the bundled sample documents, `"quick brown fox"` matches
`testfile1.txt` and `testfile2.txt`.

### Query rules

- Search terms are case-insensitive.
- Boolean operators must be uppercase.
- Adjacent terms imply `AND`.
- Boolean operations are evaluated from left to right.
- The exclusion operator must be separate, as in `fox - dog`.
- Phrase searches require double quotes around the complete query.
- Prefix searches support a single prefix followed by `*`.
- Parentheses and combinations of phrase/prefix searches with Boolean
  expressions are currently unsupported.
- Invalid syntax produces an `Invalid query:` explanation. A valid query
  with no matching documents produces `No results found.`
- Lowercase `and` and `or` are ordinary terms. Quote reserved operator
  words to search them literally, e.g. `"AND"`.
- `fox -dog` searches the literal hyphenated term `-dog`; it is not exclusion.
- Empty phrases, punctuation-only terms, unmatched or embedded double quotes,
  and unsupported wildcard placement are rejected. Quotes have no escape syntax.
- Parentheses outside phrases are rejected. Inside a quoted phrase they are
  ordinary punctuation handled by the existing normalisation rules.

The supported forms are a sequence of terms with optional `AND`, `OR` or `-`
operators between terms; one complete quoted phrase; or one standalone prefix
with a trailing `*`. Adjacent terms imply `AND`. For example,
`123 OR dog AND fox` means `(123 OR dog) AND fox`; the parentheses here explain
evaluation and cannot be entered as query syntax.

| Input | Response |
| --- | --- |
| `fox AND` | Missing search term after `AND`. |
| `"quick brown` | Unmatched double quote. |
| `*fox`, `fo**` | Invalid prefix wildcard placement. |
| `fox OR do*` | Mixed prefix expressions are unsupported. |
| `"quick" "brown fox"` | Use one quoted phrase as the entire query. |
| `unfindableterm` | Valid search; no results if the term is absent. |
| `:help` | Show help at the interactive prompt. |

Errors return to the prompt so the query can be corrected. Invalid prefix
queries do not produce suggestions. `help` remains an ordinary search term.

### Repeatable demonstration

```bash
printf '"quick brown fox"\nfo*\nfox OR secret\nx\n' | ./search
```

This runs three queries against the sample collection and exits. See the
[demonstration guide](docs/demo.md) for PowerShell equivalents, captured output
and expected exit codes.

## Implementation

### Indexing

`Indexer` reads whitespace-separated tokens, normalises case and
punctuation, and builds an inverted index.

For each indexed term, it records:

- Documents containing the term.
- The term's frequency in each document.
- Its positions within each document.

Document lengths and content are retained for ranking and snippets.

### Matching

`ExactSearcher` retrieves term postings and combines them for Boolean
queries. Phrase matching retains positions that extend the complete
phrase consecutively.

`PrefixSearcher` stores indexed terms in a trie. Prefix searches collect
matching words and aggregate their document frequencies.

### Ranking

Matching documents receive a score calculated from the query's scoring
terms:

```text
TF = term occurrences in document / indexed words in document

IDF = ln((indexed document count + 1) /
         (documents containing the term + 1)) + 1

Document score = sum(TF × IDF)
```

Phrase queries score their constituent terms after phrase filtering.
Prefix queries score matching indexed terms. Excluded terms do not
contribute to the score.

Repeated query terms contribute repeatedly. Results are ordered by
descending score, with document path used to break ties.

The CLI displays up to ten results.

### Result explanations

`processQueryDetailed()` returns each document's score, matched terms with
whole-document occurrence counts and positions, and matching spans.
`executeQuery()` returns a `QueryResponse` containing results, a `QueryError`
code, an explanatory message and prefix suggestions. `ok()` distinguishes
valid empty results from invalid syntax. Validation still occurs when the result limit is zero
or negative; valid queries with those limits return no results.

`processQueryDetailed()` and `processQuery()` preserve their vector return
types for compatibility, returning empty vectors for invalid input. Use
`executeQuery()` when diagnostics are needed.

`QueryResponse::suggestions` contains the alphabetically ordered indexed words
expanded by a valid prefix query. Invalid queries, non-prefix queries and
prefixes with no matching vocabulary return an empty list. Suggestions are
independent of the document result limit, including zero or negative limits.
The CLI displays this list directly; it does not reinterpret prefix syntax or
repeat the trie lookup.

### Engine and presentation responsibilities

- `executeQuery()` owns query validation, matching, ranking, explanations and
  prefix suggestions. It returns data without printing query output.
- `Indexer::getSnippetFromSpans()` turns engine match spans into snippet text.
- `main.cpp` owns file discovery, interactive commands, exit codes and output
  formatting. Both interactive and single-query modes use the same renderer.

A future interface can consume the same query response and snippet helper.
File loading and trie construction remain the caller's responsibility; there
is no browser adapter or in-memory document-loading API yet.

### Explanation details

The CLI only displays positive terms actually present in a result. Prefixes
expand to their matched words: `fo*` against `fox fox foxes` reports
`fox: 2 foxes: 1`. Terms are displayed alphabetically and repeated query
terms are explained once, while their existing ranking contribution remains.

Phrase results separately report complete phrase occurrences, including
overlapping occurrences. For example, `"a a"` against `a a a` reports
`a: 3` and `Phrase occurrences: 2`.

Snippets use matching token positions, with two context words on each side.
Phrase snippets use complete phrase spans rather than isolated constituent
words. Overlapping or adjacent windows merge; up to three excerpts are shown.
Ellipses indicate omitted text, including later excerpts beyond the limit.

## Repository structure

| File | Responsibility |
| --- | --- |
| `main.cpp` | Command-line interaction and result presentation. |
| `Index.h` / `Index.cpp` | Index construction, document content and snippets. |
| `ExactSearcher.hpp` | Term matching, posting operations and phrase matching. |
| `PrefixSearcher.hpp` | Trie storage, prefix matching and suggestions. |
| `query_processor.hpp` | Query interpretation and ranking. |
| `makefile` | Build, test and clean targets. |
| `tests/search_tests.cpp` | Search regression checks. |
| `docs/demo.md` | Reproducible commands and captured output. |
| `docs/architecture.md` | Data flow, interfaces and design trade-offs. |
| `RELEASE_NOTES.md` | Unreleased changes and verification status. |
| `data/sample/testfile*.txt` | Sample documents. |
| `data/sample/extreme secret.txt` | Additional synthetic sample document. |

## Testing

Build the application and run the regression checks from the repository root.
The tests also launch the built CLI with redirected input.

```bash
make test
```

The checks cover:

- Phrase continuity.
- Boolean operator placement.
- Prefix aggregation.
- Digits, apostrophes and hyphens in trie searches.
- Empty and malformed queries, error categories and useful explanations.
- CLI help, recovery after errors, no-result messages and EOF handling.
- Directory filtering, paths containing spaces, canonical duplicate paths and
  mixed inputs.
- Single-query execution, option errors, exit codes and sample fallback rules.
- Symlink exclusion and explicit file aliases where symlink creation is permitted.
- Result limits and engine-provided prefix suggestions independent of those limits.
- TF-IDF calculations.
- Expanded prefix counts, positive-only explanations and repeated terms.
- Phrase occurrences, overlapping phrases and document-edge snippets.
- Merged snippet windows, excerpt limits and invalid snippet spans.
- Repeated document indexing and rebuilding the index.
- CLI completion when redirected input ends.

Successful completion prints `Search regression checks passed.` The suite uses
C++ assertions: do not compile tests with `-DNDEBUG`. Some systems cannot create
symbolic links; the suite prints an explicit skip message for those cases.
A skipped symlink check is not evidence that symlink behaviour passed.

Remove generated build files:

```bash
make clean
```

## Current limitations

- The index is rebuilt in memory on each run.
- Directory scans are non-recursive; only `.txt` files are discovered.
- Command-line file paths use the platform narrow-character encoding; full
  Unicode path support on Windows is not guaranteed.
- Supported input is plain text.
- Full Unicode linguistic processing and stemming are unimplemented.
- Phrase and prefix expressions cannot be combined with Boolean queries.
- Snippets use indexed tokens, so original whitespace is not preserved.
- At most three merged excerpts are shown, in document order. Adjacent
  matches can merge into a long excerpt; there is no character limit.
- The legacy `getSnippet(filename, query)` helper remains available for
  compatibility; the CLI uses match spans through `getSnippetFromSpans`.
- The application currently runs locally.

## Planned improvements

- Add reproducible performance benchmarks.
- Create an interactive browser demonstration.

## Project background and contributions

This project builds on a university group search engine developed by
Brandon Ho, Duy Do and Zixi Khoo.

Project Search Engine continues that foundation as a portfolio project.
Brandon Ho's subsequent development includes prefix-trie repairs, regression
coverage, structured results and match-based snippets, query diagnostics,
directory input, single-query execution and engine-provided suggestions.
The [commit history](https://github.com/BrandonHo419/ProjectSearchEngine/commits/main/)
and [release notes](RELEASE_NOTES.md) record that work. These improvements do
not replace the attribution of the original group project.

No licence has been added. Licensing and publication of a packaged executable
remain separate decisions.
