# Optional WebAssembly bridge

This build executes the existing C++ engine through JavaScript. It provides a
bridge, Node.js verification and a local browser demonstration. No site has
been deployed. Native build commands and GitHub Actions are unchanged.

## Requirements

- Emscripten **4.0.10**, pinned by `scripts/build_wasm.py`.
- Python 3 to run the build helper.
- Node.js 24 for the documented smoke/parity tests (no npm packages).
- Native C++17 GCC on PATH for the parity test's reference executable.

The optional SDK supplies its own compiler; native builds still use your usual
GCC. Keep the SDK outside the repository. The build produces ignored files
`wasm/generated/search-engine.mjs` and `wasm/generated/search-engine.wasm`.
Both are needed at runtime. Do not commit generated binaries.

### Linux / WSL

From the repository root, install and activate the SDK in a sibling directory:

```bash
git clone https://github.com/emscripten-core/emsdk.git ../emsdk
../emsdk/emsdk install 4.0.10
../emsdk/emsdk activate 4.0.10
source ../emsdk/emsdk_env.sh
python3 scripts/build_wasm.py
node tests/wasm_tests.mjs
```

If the SDK is already installed, reuse that directory and activate the pinned
version. The helper checks the compiler version rather than silently changing
it. Set `CXX` to a native compiler executable path if `g++` is not on PATH; do
not put compiler flags in `CXX`.

### Native Windows

From the repository root in PowerShell:

```powershell
git clone https://github.com/emscripten-core/emsdk.git ..\emsdk
..\emsdk\emsdk.bat install 4.0.10
..\emsdk\emsdk.bat activate 4.0.10
..\emsdk\emcmdprompt.bat
```

The SDK command prompt loads its environment. In that prompt, change back to
your repository and run:

```bat
cd /d C:\Users\brand\Documents\GitHub\ProjectSearchEngine
python scripts\build_wasm.py
node tests\wasm_tests.mjs
```

Use your actual repository path. Python and native UCRT64 `g++` must be on PATH
in that prompt; `g++` also needs its runtime DLL directory on PATH. Running an
SDK environment batch file as a PowerShell child process does not update the
parent PowerShell environment. Use the SDK prompt for these commands.

## Run the browser demonstration

After building the module, start a server **from the repository root**:

```powershell
python -m http.server 8000 --bind 127.0.0.1
```

On Linux use `python3`. Keep that terminal open and visit
[the local demo](http://127.0.0.1:8000/demo/) in a modern browser. Stop the server
with Ctrl+C. Serving the root is necessary because the page imports `wasm/`
and fetches `data/sample/`; do not serve only the `demo/` subdirectory or open
`index.html` through `file://`.

The demo searches the four bundled synthetic documents. It starts with `fo*`,
shows ranked results, scores, word counts, snippets, whole-document term counts
and complete phrase occurrence counts. Query examples and prefix suggestions
are clickable. Expand collection entries to inspect their source text.

Search controls stay disabled until the module and all four documents load.
A failed build/resource fetch shows setup guidance and a reload button, rather
than silently searching a partial corpus. Invalid syntax clears stale results
and remains editable; valid searches with no matches have a separate message.
The query field is capped at 1,024 characters. Other engine APIs retain their
existing limits. Queries are not placed in URLs or submitted to a server.

All queries, snippets and sample text are rendered as text, not HTML. The page
uses no external fonts, scripts or framework. Module initialisation and corpus
fetching are asynchronous; searches remain synchronous on this small corpus.

### Browser regression checks

With the same local server running, visit
[the browser test page](http://127.0.0.1:8000/tests/browser_tests.html).
It loads the real demo in an iframe and runs without additional packages.
Expect `Browser regression checks passed` with a check count. It covers initial
results, example/suggestion buttons, phrases, errors, recovery, empty results,
markup-safe query rendering and agreement with direct WebAssembly responses.

Manual checks before publishing:

- Search by pressing Enter, click examples and suggestions, and open query help.
- Inspect at desktop and narrow mobile widths; controls and text must not overflow.
- Block the generated `.wasm` request in browser developer tools and reload:
  controls must remain disabled with an explanatory error. Unblock and use Reload.
- Block any one sample document and reload: the demo must not search a partial corpus.
- Confirm no query-triggered network requests after initial loading.

## JavaScript interface

Save this as a temporary `.mjs` file in the repository root to run with Node,
or use the same imports in an ES-module browser script served over HTTP:

```javascript
import { createSearchEngine } from './wasm/search-engine.mjs';

const engine = await createSearchEngine();
try {
  const count = engine.loadDocuments([
    { id: 'notes:one', text: 'Quick brown fox foxes' },
    { id: 'notes:two', text: 'A quiet dog' }
  ]);
  console.log(count); // 2 unique document IDs
  const response = engine.search('fo*');
  console.log(response.suggestions); // ['fox', 'foxes']
  console.log(response.results[0].document); // 'notes:one'
} finally {
  engine.dispose();
}
```

`createSearchEngine(moduleOptions = {})` returns a promise. Optional settings
are forwarded to the generated Emscripten factory (for example `locateFile`
when serving the `.wasm` file from another directory). Retain the relative
module layout unless you explicitly configure resource loading.

| Method | Contract |
| --- | --- |
| `loadDocuments([{id, text}, ...])` | Replace the corpus and rebuild a fresh trie; return the unique document count. Last duplicate ID wins; empty text is valid; `[]` clears. |
| `search(query, topK = 10)` | Return a plain object containing `ok`, `error`, `message`, `suggestions` and `results`. Non-positive limits suppress results but preserve validation/suggestions. |
| `dispose()` | Free this engine's C++ object; repeated disposal is harmless. Subsequent loads/searches throw. Release JS references too when discarding the module. |

Each result includes `document`, `score`, `wordCount`, `snippet`, `matches`
(with term, frequency and positions), `spans` (half-open token ranges),
`isPhrase` and `phraseOccurrences`. Arrays/objects are copied into ordinary JS
values; callers do not manage Embind handles. Responses remain usable after
reload or disposal. Each factory call produces an independent module/engine.

Document IDs must be non-empty strings and text must be a string. Invalid
argument types throw `TypeError`; a non-integer or out-of-range signed 32-bit
`topK` throws `RangeError`. Validation occurs before replacing the old corpus.
Malformed search syntax returns `ok: false` and the named `QueryError`, rather
than throwing. The underlying C++ batch replacement is installed only after
both its new index and trie are built.

The facade guarantees cleanup of temporary native vectors. C++ exceptions are
enabled for the optional build; unexpected allocation/runtime failures are not
converted into successful empty results. An engine instance is not a security
boundary for hostile JS objects, accessors or arbitrarily large corpora.

## Verification

`node tests/wasm_tests.mjs` compiles a native reference program in a unique
system temporary directory, loads the same synthetic corpus in both engines,
and compares full responses for 28 queries at four result limits. Only scores
use a tolerance (`1e-12`) for cross-platform floating-point differences.
Temporary fixtures are removed in a `finally` block.

The suite also checks argument validation, retention after invalid input,
duplicate IDs, corpus clearing/reloading, stale-trie prevention, independent
instances, detached result objects, UTF-8 ID transport and disposal.
Expected final line:

```text
WebAssembly checks passed (112 native comparisons; validation, reload and disposal).
```

Run the existing native suite separately (`make test` or the README's Windows
commands). Node execution verifies the compiled WebAssembly and JavaScript
bridge. Run the separate browser test page for page and browser-loading checks.

## Scope and next work

- The optional build targets Node and web ES modules. Browsers need HTTP serving
  with suitable JavaScript/WebAssembly MIME types; `file://` is not the demo path.
- There is no search backend, telemetry, upload route or filesystem loading in
  this bridge. Corpus content comes from the supplied strings.
- The query grammar and byte-oriented text normalisation remain unchanged.
  UTF-8 ID transport does not add Unicode linguistic search support.
- Indexing and searching are synchronous after module initialisation. Use a small,
  trusted sample corpus; workers, cancellation and resource limits are future work.
- The local browser UI is included; deployment remains a separate stage. CI
  has not been extended to install Emscripten or run the optional/browser suites.

Implementation references: [Emscripten SDK setup](https://emscripten.org/docs/getting_started/downloads.html),
[Embind](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/embind.html),
[modularised output](https://emscripten.org/docs/compiling/Modularized-Output.html),
and [C++ exceptions](https://emscripten.org/docs/porting/exceptions.html).
