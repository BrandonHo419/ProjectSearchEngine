# Project Search Engine
[![Build and regression tests](https://github.com/BrandonHo419/Project-Search-Engine/actions/workflows/build-and-test.yml/badge.svg)](https://github.com/BrandonHo419/Project-Search-Engine/actions/workflows/build-and-test.yml)

A C++17 search engine for local text files, with an interactive command-line interface.

The engine builds an inverted index containing word frequencies and positions. It supports term searches, exact phrases, Boolean operations and prefix matching, then ranks matching documents using TF-IDF.

## Features

- Case-insensitive term searches.
- Exact phrase matching using consecutive word positions.
- Boolean `AND`, `OR` and exclusion using `-`.
- Trie-based prefix searches and word suggestions.
- TF-IDF ranking with deterministic ordering for tied scores.
- Results containing document paths, scores, word counts and snippets.
- Support for text files supplied through command-line arguments.
- Regression tests for core search behaviour.

## Getting started

### Requirements

- A compiler supporting C++17, such as GCC.
- GNU Make.
- Linux or Windows Subsystem for Linux (WSL) is recommended.

### Build

```bash
git clone https://github.com/BrandonHo419/ProjectSearchEngine.git
cd Project-Search-Engine
make
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

Paths containing spaces must be quoted.

Unreadable files are reported and skipped. If no documents can be loaded,
the program exits with an error.

Enter `x` to quit. The program also exits when standard input ends.

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
- Malformed or unsupported queries currently return no results.

### Repeatable demonstration

```bash
printf '"quick brown fox"\nfo*\nfox OR secret\nx\n' | ./search
```

This runs three queries against the sample collection and exits.

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
| `testfile*.txt` | Sample documents. |
| `extreme secret.txt` | Additional synthetic sample document. |

## Testing

Build the application and run the regression checks:

```bash
make test
```

The checks cover:

- Phrase continuity.
- Boolean operator placement.
- Prefix aggregation.
- Digits, apostrophes and hyphens in trie searches.
- Empty and malformed queries.
- Result limits.
- TF-IDF calculations.
- Repeated document indexing and rebuilding the index.
- CLI completion when redirected input ends.

Remove generated build files:

```bash
make clean
```

## Current limitations

- The index is rebuilt in memory on each run.
- Files must be supplied individually.
- Supported input is plain text.
- Full Unicode linguistic processing and stemming are unimplemented.
- Phrase and prefix expressions cannot be combined with Boolean queries.
- Snippets are approximate context windows.
- Prefix snippets can fall back to the beginning of the document.
- The CLI's match counts use literal query tokens rather than expanded
  prefix matches.
- The application currently runs locally.

## Planned improvements

- Organise sample data and add directory input.
- Improve query validation and error messages.
- Improve snippets and prefix match counts.
- Automate builds and regression tests.
- Add reproducible performance benchmarks.
- Create an interactive browser demonstration.

## Project background and contributions

This project builds on a university group search engine developed by
Brandon Ho, Duy Do and Zixi Khoo.

The original contribution records describe:

- **Brandon Ho** — initial searcher prototypes, prefix searching and
  query processing/ranking work.
- **Duy Do** — Boolean operations, phrase-search changes and integration
  fixes.
- **Zixi Khoo** — inverted indexing, word positions, normalisation,
  snippets and contributions to the CLI.

Project Search Engine continues that foundation as a portfolio project.
Subsequent maintenance and feature development are recorded in the
repository history.

Original repository:
https://github.com/BrandonHo419/mini-search-engine