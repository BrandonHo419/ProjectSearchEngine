# Project Search Engine

A C++17 search engine for text files, with a command-line interface and a browser demo.

[Try the live demo](https://brandonho419.github.io/ProjectSearchEngine/demo/)

## How it works

The engine builds an index of words and their positions in each document. It uses
that index to find matching documents, ranks them with TF-IDF, and shows short
snippets explaining each match. Prefix searches use a trie to find matching words.

The browser demo runs the same C++ engine through WebAssembly. It searches four
sample documents locally in your browser.

The demo can also search local TXT, Markdown, CSV, JSON, HTML, XML, PDF and DOCX
files. File contents stay in the browser. Scanned documents need OCR and are not
supported. Use **Restore sample documents** to return to the bundled collection.

## Search examples

| Query | Finds |
| --- | --- |
| `fox` | Documents containing fox |
| `"quick brown fox"` | The exact phrase |
| `fo*` | Words starting with fo |
| `fox AND dog` | Both words |
| `fox OR dog` | Either word |
| `fox - dog` | Fox without dog |

Boolean operations run left to right. Parentheses and combined phrase/prefix
expressions are not supported. Text processing is byte-oriented, without stemming
or full Unicode language support.

## Run locally

Use a C++17 compiler. From the repository root on Windows with GCC on PATH:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic main.cpp Index.cpp -o search.exe
./search.exe
```

On Linux:

```bash
make
./search
```

The bundled samples are used by default. Enter a query, `:help` for examples,
or `x` to quit.

## Tests

Windows:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -I. tests/search_tests.cpp Index.cpp -o tests/search-tests.exe
./tests/search-tests.exe
```

Linux: `make test`.

## Browser build

Activate Emscripten **4.0.10** and use Node.js 24, then run:

```text
npm ci --ignore-scripts --omit=optional
npm run prepare:parsers
python scripts/build_wasm.py
node tests/wasm_tests.mjs
python -m http.server 8000 --bind 127.0.0.1
```

Open `http://127.0.0.1:8000/demo/`. Python, Node.js and native GCC must be on PATH.
Generated WebAssembly files are ignored by Git; GitHub Actions builds them for deployment.

## Development

I keep detailed technical notes, benchmark results and project history on the
[development branch](https://github.com/BrandonHo419/ProjectSearchEngine/tree/development).

Document extraction uses pinned PDF.js and Mammoth builds served with the demo.
Their licence files are included in the generated parser assets.
