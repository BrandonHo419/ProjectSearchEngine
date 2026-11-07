# Native performance baseline (Stage 7)

This is a measurement baseline, not an optimization or a speed guarantee.
The runner exercises the existing C++17 engine without changing query semantics.
It is optional: no dependencies, CI jobs or normal build targets are added.

## Reproduce

From the repository root with Python 3 and native GCC on PATH:

```powershell
python scripts/benchmark.py --check-only
python scripts/benchmark.py --output ../search-benchmark.json
```

On Linux use `python3`. Windows uses the same UCRT64 compiler as the CLI;
its runtime DLL directory must be on PATH. `--cxx` or `CXX` accepts a compiler
executable path, not a shell command or compiler flags. Compilation uses
`-std=c++17 -O2 -Wall -Wextra -pedantic -I.` on both platforms.
Executables are built in a system temporary directory and removed on exit.
No corpus files are created: the generated document strings are indexed in memory.
The JSON destination must be a new file, so earlier results are not overwritten.
Without `--output`, JSON goes to stdout and progress/test messages to stderr.

Explicit equivalent of the default measured run:

```text
python scripts/benchmark.py --sizes 100 500 1000 --words 128 --seed 1234 --samples 5 --iterations 3 --output ../search-benchmark.json
```

For a quick functional run, use `--sizes 20 --samples 3 --iterations 1`.
Use a new output name for each repeat. Timing is sensitive to machine load;
close heavy applications, keep power settings consistent and compare on the
same machine/compiler. Record multiple complete runs before claiming improvement.

Bounds: 1–10,000 documents per size, 16–4,096 words per document, at most
2,000,000 generated tokens per size, seed 0–2,147,483,647, 3–100 samples and
1–1,000 query iterations. Large allowed inputs can still be slow. The defaults
are intentionally modest. `--check-only` runs the report accounting tests and
compiled fixture tests; normal measurement runs those checks first as well.

## Corpus and workloads

Corpus version 1 uses unsigned 32-bit LCG arithmetic (multiplier 1664525,
increment 1013904223); the upper 16 bits modulo 256 select filler tokens
`tok0000` through `tok0255`. IDs are `synthetic:0`, `synthetic:1`, etc.
No platform-dependent random distributions or timestamps enter the text.
Every document starts with five marker tokens; remaining tokens are fillers.
The same size, word count and seed produce identical ASCII bytes on Windows/Linux.
A 64-bit FNV-1a fingerprint covers each ID and text followed by NUL separators;
it checks fixture identity, not cryptographic integrity. Source files use SHA-256.

Each document contains `common`; every twentieth contains `rare` instead of
`ordinary`. Every fourth has `amber silver comet`, while the others have
`amber quiet comet`. Marker schedules start at document zero. The first
40-document, 16-word, seed-1234 fixture has fingerprint `63c92f643dabc9d9`,
checked against an independently calculated constant in the fixture tests.

| Workload | Query | Intended selectivity |
| --- | --- | --- |
| Common term | `common` | All documents |
| Rare term | `rare` | Approximately 5% |
| Exact phrase | `"amber silver comet"` | Approximately 25%, one occurrence each |
| Prefix | `tok000*` | Up to ten filler terms; seed-dependent document matches |
| Boolean AND | `rare AND silver` | Approximately 5%; every rare marker also has silver |
| Boolean OR | `rare OR silver` | Approximately 25% |
| Exclusion | `common - silver` | Approximately 75% |
| Missing term | `absenttoken` | No matches |

Every query uses `topK=10`. This caps returned results, not candidate evaluation.
The filler vocabulary size is fixed across corpus sizes; the benchmark varies
document count while holding document length and generator rules fixed.
This is not a model of natural-language frequency distributions or arbitrary
user documents. Broad prefixes over thousands of terms are not covered.

## Timing boundaries and output

- **Index:** `buildIndexFromText` on a fresh index. Text generation, index object
  construction/destruction and trie construction are outside this measurement.
- **Trie:** insert all vocabulary postings into a fresh trie. Index construction
  and trie object construction/destruction are outside this measurement.
- **Query:** `executeQuery`, snippets for every returned result, small result
  checksum accounting and response destruction. Includes parsing, candidate
  retrieval, ranking, explanations and suggestions. No printing/serialization,
  network, browser DOM, WebAssembly startup or filesystem reads are timed.

A discarded build warms the allocator/code path. Each of five measured builds
uses fresh objects, destroyed outside the next timing. The final build is kept
for all query samples. Each workload gets one untimed warm-up query. Each of five
query samples times a batch of three sequential operations with
`std::chrono::steady_clock`. Workload order rotates between samples. The runner
reports median/min/max of **batch time divided by iterations**; these are not
per-request latency percentiles. Especially for the missing-term case, clock
and loop overhead are material. No timer-overhead subtraction is applied.

Raw samples and checksums are retained, alongside corpus dimensions/text bytes,
compiler flags/version, OS/CPU, visible logical CPU count, Python version,
Git HEAD/status and measured-source hashes. Checksums must agree between samples;
they prevent unused results and detect some inconsistencies, but do not replace
semantic regression tests. Timing values are never correctness thresholds.

CPU model and visible CPU count describe the execution environment; they do not
establish exclusive CPU allocation, clock frequency, physical RAM or container
resource limits. Memory usage is not measured. These are warm, single-process,
single-threaded native measurements, not cold-cache disk indexing throughput.

## Recorded baseline

See [the complete raw report](performance-baseline.json). The source baseline is
`f4c5fb3` plus the Stage 7 benchmark files; Stage 6D changes packaging and hosting,
not the engine being measured. Engine files remain unchanged. Git status is
recorded because this baseline was taken before committing the benchmark patch.
No Windows performance result is claimed; run the same command locally to obtain one.

Recorded 2026-11-07 on Linux-6.18.57-x86_64-with-glibc2.39, AMD EPYC 9V74 80-Core Processor (9 visible logical CPUs), GCC 13.3.0, `-O2`.

Milliseconds per operation; each cell is **median [min–max]** over five samples.

| Workload | 100 documents | 500 documents | 1,000 documents |
| --- | ---: | ---: | ---: |
| index | 4.4061 [4.3313–4.7906] | 21.6490 [20.5664–25.3268] | 49.5293 [48.3958–57.7006] |
| trie | 0.6099 [0.5898–0.6368] | 3.8488 [3.3517–4.9731] | 13.9925 [13.5441–14.9361] |
| common_term | 0.4927 [0.4560–0.5417] | 17.7882 [17.2454–18.1984] | 70.6234 [69.4232–79.9936] |
| rare_term | 0.0222 [0.0219–0.0340] | 0.0770 [0.0767–0.1175] | 0.1706 [0.1665–0.1786] |
| phrase | 0.3682 [0.3240–0.3945] | 10.2867 [10.1062–14.3348] | 45.2835 [42.6233–47.7347] |
| prefix | 1.8898 [1.6748–1.9270] | 60.2134 [58.0535–62.6218] | 301.3633 [293.4074–325.0623] |
| boolean_and | 0.0423 [0.0335–0.1719] | 0.2462 [0.2424–0.3442] | 1.1987 [1.1526–1.8962] |
| boolean_or | 0.0925 [0.0904–0.1326] | 1.0711 [0.8484–1.1894] | 5.5377 [5.2010–6.7493] |
| exclusion | 0.4191 [0.3735–0.5203] | 13.7499 [13.1923–15.6281] | 53.4570 [50.9275–58.3001] |
| missing | 0.0008 [0.0008–0.0010] | 0.0014 [0.0013–0.0017] | 0.0019 [0.0018–0.0021] |

| Documents | Text bytes | Corpus fingerprint |
| ---: | ---: | --- |
| 100 | 101,705 | `66d5eaa661f4d130` |
| 500 | 508,525 | `25b5f74595fc29cf` |
| 1000 | 1,017,050 | `96e25711c0221b99` |

## Interpretation and next experiment

The common-term and prefix workloads grow substantially faster than indexing
in this run. The top-ten output limit does not avoid ranking all candidates.
Code inspection provides a specific hypothesis: `executeQuery` calls
`index.searchWord(term)` inside its candidate/term loop, and `searchWord`
returns an entire postings map by value. Repeated map copies may account for
part of the scaling cost. These end-to-end timings do not isolate that cost;
this remains a hypothesis, not a measured attribution.

Historical Stage 8 proposal: profile or instrument the repeated postings retrieval, then
consider a query-local cache so each distinct scoring term is retrieved once.
Keep public APIs, query semantics, repeated-term scoring, explanations and
ranking tie-breaks unchanged. Compare the same corpora and queries before/after,
run native/WASM regressions, and report both gains and regressions. Stage 8
was subsequently approved and implemented. This document retains the original
baseline; see [Stage 8 results](performance-stage8.md) for the current implementation.
