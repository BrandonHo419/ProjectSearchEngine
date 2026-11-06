# Reproducible native demonstration

Build from the repository root using the [README](../README.md). All examples
use the four unchanged files in `data/sample/`. Scores depend on that corpus;
adding documents, including empty documents, can change IDF and scores.

Output below was captured from the Stage 4 source (Stage 3 commit `25f5af0`
plus the Stage 4 patch), built on Linux with GCC 13.3.0 and the documented
C++17 flags. Only trailing spaces were removed for Markdown. Native Windows
may display platform-specific path separators; the user also verified the
prefix example on native Windows. Other Windows examples below are provided
for verification, not claimed as executed in that shell.

## 1. Prefix search and explanations

Linux / UCRT64 shell:

```bash
./search --query "fo*"
```

PowerShell:

```powershell
./search.exe --query "fo*"
$LASTEXITCODE
```

Expected exit code: `0`. Captured output:

```text
=== Mini Search Engine CLI ===
Index built successfully!
Suggestions: fox foxes

Search results (ranked):
1. data/sample/testfile3.txt | Score: 0.174208 | Total words: 11
   Snippet: Quick brown foxes are faster ...
   Matched terms (occurrences in document): foxes: 1
2. data/sample/testfile1.txt | Score: 0.143888 | Total words: 21
   Snippet: ... quick brown fox jumps over ... lazy dog. Fox is not ...
   Matched terms (occurrences in document): fox: 2
3. data/sample/testfile2.txt | Score: 0.137348 | Total words: 22
   Snippet: ... Another brown fox jumped over ... quick brown fox is used ...
   Matched terms (occurrences in document): fox: 2
```

The prefix expands to `fox` and `foxes`. Counts are whole-document counts.
The shorter third sample ranks first because TF divides by document length;
raw occurrence count alone does not determine rank.

## 2. Phrase matching through redirected input

Passing the phrase through stdin avoids differences in native argument quoting
between PowerShell versions.

Linux / UCRT64 shell (use `./search.exe` on Windows):

```bash
printf '"quick brown fox"\nx\n' | ./search
```

PowerShell:

```powershell
@('"quick brown fox"', 'x') | ./search.exe
$LASTEXITCODE
```

Expected exit code: `0`. Captured redirected output (input is not echoed):

```text
=== Mini Search Engine CLI ===
Index built successfully!

Enter search query (:help for help, x to quit):
Search results (ranked):
1. data/sample/testfile1.txt | Score: 0.318623 | Total words: 21
   Snippet: The quick brown fox jumps over ...
   Matched terms (occurrences in document): brown: 1 fox: 2 quick: 2 | Phrase occurrences: 1
2. data/sample/testfile2.txt | Score: 0.30414 | Total words: 22
   Snippet: ... The phrase quick brown fox is used ...
   Matched terms (occurrences in document): brown: 2 fox: 2 quick: 1 | Phrase occurrences: 1

Enter search query (:help for help, x to quit): Goodbye!
```

Each result reports a complete phrase occurrence separately from the
whole-document frequency of each constituent word.

## 3. Invalid syntax versus no matches

Linux / UCRT64 shell:

```bash
./search --query "fox AND"
echo $?
./search --query "unfindableterm"
echo $?
```

PowerShell:

```powershell
./search.exe --query "fox AND"
$LASTEXITCODE
./search.exe --query "unfindableterm"
$LASTEXITCODE
```

The first query exits with `2`:

```text
=== Mini Search Engine CLI ===
Index built successfully!
Invalid query: Missing search term after operator 'AND'. Use --help or :help for examples.
```

The second is valid and exits with `0`:

```text
=== Mini Search Engine CLI ===
Index built successfully!
No results found.
```

## 4. Directory input and help

```powershell
./search.exe --directory "data/sample" --query "fo*"
./search.exe --help
```

On Linux use `./search`. The directory query should show the same three
ranked documents, scores and explanations as example 1. Directory enumeration
may use native path separators. Scans are non-recursive, accept `.txt` and
`.TXT`, and skip discovered symlinks. Explicit file and directory arguments
can be combined. `--help` exits with `0` without building an index.

## Verification checklist

- Build the CLI before running `tests/search-tests.exe` or `make test`.
- Expect `Search regression checks passed.`; record symlink skips separately.
- Check prefix suggestions, phrase occurrences and invalid-query recovery.
- Keep the repository root as the working directory for bundled defaults.
- A missing explicit file returns `1` if no other document loads; it must not
  silently search the bundled samples.

For a portfolio screenshot or recording, show example 1 and the command that
produced it. Use the synthetic samples rather than personal documents.
