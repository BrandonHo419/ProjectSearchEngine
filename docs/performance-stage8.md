# Stage 8: query-local postings cache

The ranking loop now copies each distinct scoring term’s postings once per query.
The ordered scoring-term loop, including repeated terms, is preserved. The cache
is local to each execution and cannot retain stale corpus data. Public APIs are unchanged.

Native regressions, fixture tests, five report tests and 72 exact before/after
JSON response comparisons passed. Commit `49cd244` subsequently passed [WASM parity and packaged browser checks](https://github.com/BrandonHo419/ProjectSearchEngine/actions/runs/42378391080).

The table compares the recorded Stage 7 run with this Stage 8 run on the same
environment and deterministic corpora. Values are median milliseconds per query
including snippets. Runs were sequential, not interleaved; ratios are observations
for these synthetic workloads, not general performance guarantees.

| Documents | Workload | Before ms | After ms |
| ---: | --- | ---: | ---: |
| 100 | common_term | 0.4927 | 0.0942 |
| 100 | rare_term | 0.0222 | 0.0227 |
| 100 | phrase | 0.3682 | 0.1215 |
| 100 | prefix | 1.8898 | 0.2493 |
| 100 | boolean_and | 0.0423 | 0.0457 |
| 100 | boolean_or | 0.0925 | 0.0623 |
| 100 | exclusion | 0.4191 | 0.0974 |
| 100 | missing | 0.0008 | 0.0008 |
| 500 | common_term | 17.7882 | 0.3199 |
| 500 | rare_term | 0.0770 | 0.0564 |
| 500 | phrase | 10.2867 | 0.7423 |
| 500 | prefix | 60.2134 | 1.8823 |
| 500 | boolean_and | 0.2462 | 0.1506 |
| 500 | boolean_or | 1.0711 | 0.1945 |
| 500 | exclusion | 13.7499 | 0.3636 |
| 500 | missing | 0.0014 | 0.0010 |
| 1000 | common_term | 70.6234 | 1.1345 |
| 1000 | rare_term | 0.1706 | 0.1197 |
| 1000 | phrase | 45.2835 | 1.5056 |
| 1000 | prefix | 301.3633 | 4.2251 |
| 1000 | boolean_and | 1.1987 | 0.2608 |
| 1000 | boolean_or | 5.5377 | 0.4895 |
| 1000 | exclusion | 53.4570 | 1.2330 |
| 1000 | missing | 0.0019 | 0.0019 |

Raw evidence: [before](performance-baseline.json), [after](performance-stage8.json).
Run the commands in [performance.md](performance.md) to reproduce on Windows.
The cache trades temporary memory for fewer copies; peak memory was not measured.
Some very small workloads may be dominated by cache/measurement overhead.
