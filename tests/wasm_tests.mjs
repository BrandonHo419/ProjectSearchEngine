import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createSearchEngine } from '../wasm/search-engine.mjs';

const root = fileURLToPath(new URL('../', import.meta.url));
const temporary = mkdtempSync(join(tmpdir(), 'search-wasm-'));
const oracle = join(temporary, process.platform === 'win32' ? 'reference.exe' : 'reference');
const engine = await createSearchEngine();
let second;
try {
  execFileSync(process.env.CXX || 'g++', [
    '-std=c++17', '-O2', '-Wall', '-Wextra', '-pedantic', '-I.',
    'tests/wasm_reference.cpp', 'Index.cpp', '-o', oracle
  ], { cwd: root, stdio: 'inherit' });
  const paths = ['testfile1.txt', 'testfile2.txt', 'testfile3.txt', 'extreme secret.txt']
    .map(name => `data/sample/${name}`);
  // Additional fixtures cover empty documents, overlapping phrases and escaped text.
  for (const [name, text] of [['empty.txt', ''], ['edge.txt', 'a a a "fox" \\ note\nfox']]) {
    const path = join(temporary, name);
    writeFileSync(path, text);
    paths.push(path);
  }
  const documents = paths.map(id => ({ id, text: readFileSync(resolve(root, id), 'utf8') }));
  assert.equal(engine.loadDocuments(documents), paths.length);
  const queries = ['fox', 'FOX', 'quick brown', 'fox AND dog', 'fox OR secret', 'fox - dog',
    '"quick brown fox"', '"a a"', 'fo*', ' FO* ', 'fox FOX', 'unfindableterm',
    'unfindable*', 'help', '"AND"', 'fox -dog', '', '   ', '*', 'fo**', 'fox OR do*',
    'fox AND', '"fox', '"quick" "brown fox"', '(fox)', '"fo*"', '!!!*', '!!!'];
  let comparisons = 0;
  for (const query of queries) {
    for (const limit of [10, 1, 0, -1]) {
      const native = JSON.parse(execFileSync(oracle, [query, String(limit), ...paths], {
        cwd: root, encoding: 'utf8', timeout: 10000
      }));
      const actual = engine.search(query, limit);
      // libm implementations can differ in the last floating-point bits.
      assert.equal(actual.results.length, native.results.length);
      actual.results.forEach((result, i) => {
        assert.ok(Math.abs(result.score - native.results[i].score) < 1e-12);
        result.score = native.results[i].score;
      });
      assert.deepEqual(actual, native, `${JSON.stringify(query)} / ${limit}`);
      comparisons++;
    }
  }
  assert.deepEqual(engine.search(' FO* ').suggestions, ['fox', 'foxes']);
  assert.equal(engine.search('fox AND').error, 'MissingOperand');
  const retained = engine.search('fox');
  retained.results[0].matches[0].frequency = -99;
  assert.notDeepEqual(engine.search('fox'), retained); // Results are detached JS data.

  const beforeInvalid = engine.search('fox');
  for (const documents of [null, {}, [null], [{ id: '', text: 'bad' }],
    [{ id: 'ok', text: 'valid' }, { id: 'bad', text: 7 }], Array(1)]) {
    assert.throws(() => engine.loadDocuments(documents), TypeError);
    assert.deepEqual(engine.search('fox'), beforeInvalid);
  }
  for (const query of [null, 1, {}]) assert.throws(() => engine.search(query), TypeError);
  for (const limit of [NaN, Infinity, 0.5, '10', 2147483648, -2147483649, null]) {
    assert.throws(() => engine.search('fox', limit), RangeError);
  }
  second = await createSearchEngine();
  assert.equal(second.search('fox').results.length, 0);
  assert.equal(second.loadDocuments([{ id: 'other', text: 'isolated' }]), 1);
  assert.equal(engine.search('isolated').results.length, 0);
  const snapshot = engine.search('fo*');
  assert.equal(engine.loadDocuments([{ id: 'same', text: 'obsolete' },
    { id: 'same', text: 'replacement' }, { id: 'empty', text: '' }]), 2);
  assert.equal(engine.search('obs*').suggestions.length, 0);
  assert.equal(engine.search('rep*').results[0].document, 'same');
  assert.equal(snapshot.results.length, 4); // Prior responses survive reloads.
  for (let i = 0; i < 20; i++) {
    engine.loadDocuments([{ id: 'cycle', text: i % 2 ? 'dog' : 'fox' }]);
    assert.deepEqual(engine.search(i % 2 ? 'fo*' : 'do*').suggestions, []);
  }
  engine.loadDocuments([{ id: 'unicode-雪', text: 'fox' }]);
  assert.equal(engine.search('fox').results[0].document, 'unicode-雪');
  assert.equal(engine.loadDocuments([]), 0);
  assert.equal(engine.search('fox').results.length, 0);
  assert.equal(second.search('isolated').results.length, 1);
  engine.dispose();
  engine.dispose();
  assert.throws(() => engine.search('fox'), /disposed/);
  assert.throws(() => engine.loadDocuments([]), /disposed/);
  console.log(`WebAssembly checks passed (${comparisons} native comparisons; validation, reload and disposal).`);
} finally {
  engine.dispose();
  second?.dispose();
  rmSync(temporary, { recursive: true, force: true });
}
