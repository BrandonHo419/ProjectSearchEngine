# Local document import

I use PDF.js 6.5.438 and Mammoth 1.13.0 for browser-side text extraction.
Install the locked dependencies with `npm ci --ignore-scripts --omit=optional`,
then run `npm run prepare:parsers`. These commands copy the pinned browser builds
and licence files into ignored `demo/vendor/`. The Pages workflow runs them
before packaging. No CDN is used at runtime.

TXT, Markdown, CSV and JSON use UTF-8 decoding. CSV delimiters are treated as
word boundaries; JSON keys and scalar values become searchable text. HTML is
parsed in a detached inert template; scripts/styles are excluded. XML must be
well formed. Markup is never inserted into the visible document. PDF extraction
uses embedded text; no OCR is performed. DOCX extraction uses raw text rather
than HTML conversion. Layout, formatting and reading order may differ from the
original document, especially in complex PDFs and tables.

Each PDF/DOCX parser runs in a separate short-lived worker, terminated after
15 seconds, completion, failure or page exit. The importer ignores PDF.js
internal messages and accepts only its own extraction-result protocol.

Limits: 20 files; 1 MiB combined source bytes; 200,000 extracted characters;
20,000 whitespace-separated words; 100 PDF pages. DOCX archive metadata is
checked for at most 500 entries and 8 MiB declared expansion. These are practical
bounds, not a guarantee of bounded memory for all malformed compressed inputs.
Password-protected PDFs, scanned/image-only documents, legacy DOC, spreadsheets
and presentations are unsupported. A file extension is only a format hint;
parser failures are shown as errors, never indexed as binary data.

Extraction completes for the whole batch before replacing the corpus through
the existing transactional engine wrapper. A failed batch keeps the old corpus
and results available. Numbered opaque document IDs keep duplicate filenames
distinct. Sample query buttons hide for local collections. Restoring samples
replaces local documents, clears previews/errors and restores the sample buttons.
There is no upload endpoint, persistence, telemetry or query submission.

Verification: 78 packaged Chromium browser checks and four packaging tests
passed locally. The browser checks include real synthetic PDF/DOCX fixtures,
every supported format, duplicate names, corrupt/unsupported files, empty text,
size/text limits, cancellation, failed-batch preservation and sample restoration.
The WebAssembly outputs used in this UI test were retrieved from the deployed
site; C++ sources were not changed. A fresh CI build still needs to pass after
applying the patch. npm audit reported no known vulnerabilities for the installed
dependency graph at preparation time; that is not a guarantee about all parser
inputs or bundled third-party code.

Run the packaged regression suite after building WASM:

```text
npm ci --ignore-scripts --omit=optional
npm run prepare:parsers
python tests/package_demo_tests.py
python scripts/package_demo.py _site
node scripts/test_browser.mjs _site
```

Use a fresh `_site` directory. Node 24 and Chrome/Chromium are required by the
browser runner; set `CHROME_BIN` if the executable is not named `google-chrome`.
