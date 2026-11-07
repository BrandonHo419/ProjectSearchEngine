const byId = id => document.getElementById(id);
const input = byId('query');
const controls = byId('search-controls');
const status = byId('engine-status');
const list = byId('result-list');
const suggestions = byId('suggestions');
const summary = byId('results-summary');
const queryError = byId('query-error');
const sampleNames = ['testfile1.txt', 'testfile2.txt', 'testfile3.txt', 'extreme secret.txt'];
const controller = new AbortController();
let engine;
let closed = false;

function element(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

function clearResults() {
  list.replaceChildren();
  suggestions.replaceChildren();
  suggestions.hidden = true;
  queryError.hidden = true;
  input.removeAttribute('aria-invalid');
}

function unavailable(message) {
  controls.disabled = true;
  status.dataset.state = 'error';
  status.textContent = 'Search engine unavailable';
  byId('load-error-message').textContent = message;
  byId('load-error').hidden = false;
  clearResults();
  summary.textContent = 'Search is unavailable until the engine and all sample documents load.';
}

function showQueryError(message) {
  queryError.textContent = message;
  queryError.hidden = false;
  input.setAttribute('aria-invalid', 'true');
  summary.textContent = 'Check the query syntax and try again.';
}

function runSearch() {
  if (!engine || controls.disabled) return;
  clearResults();
  const query = input.value;
  if (query.length > 1024) {
    showQueryError('Use a query of at most 1,024 characters.');
    return;
  }
  try {
    const response = engine.search(query);
    if (!response.ok) {
      showQueryError(response.message);
      return;
    }
    const count = response.results.length;
    summary.textContent = count
      ? `${count} ${count === 1 ? 'match' : 'matches'} for “${query.trim()}”`
      : `No matching documents for “${query.trim()}”. Try another term from the collection.`;
    if (response.suggestions.length) {
      suggestions.append(element('span', '', 'Prefix expands to:'));
      for (const word of response.suggestions) {
        const button = element('button', '', word);
        button.type = 'button';
        button.addEventListener('click', () => {
          input.value = word;
          runSearch();
        });
        suggestions.append(button);
      }
      suggestions.hidden = false;
    }
    response.results.forEach((result, index) => {
      const card = element('li', 'result');
      const heading = element('div', 'result-heading');
      const title = element('h3', 'result-title');
      title.append(element('span', 'rank', String(index + 1).padStart(2, '0')),
        element('span', 'document-name', result.document.split('/').at(-1)));
      heading.append(title, element('span', 'score', `Score ${result.score.toFixed(6)}`));
      card.append(heading,
        element('p', 'result-path', `${result.document} · ${result.wordCount} indexed words`),
        element('p', 'snippet', result.snippet));
      const matches = element('div', 'matches');
      matches.setAttribute('aria-label', 'Matched terms and whole-document counts');
      for (const match of result.matches) {
        matches.append(element('span', 'match', `${match.term} × ${match.frequency}`));
      }
      card.append(matches);
      if (result.isPhrase) {
        card.append(element('p', 'phrase-count', `Complete phrase occurrences: ${result.phraseOccurrences}`));
      }
      list.append(card);
    });
  } catch (error) {
    console.error('Search failed:', error);
    engine.dispose();
    engine = undefined;
    unavailable('The search engine encountered an unexpected error. Reload the demo to try again.');
  }
}

byId('search-form').addEventListener('submit', event => {
  event.preventDefault();
  runSearch();
});
for (const button of document.querySelectorAll('[data-query]')) {
  button.addEventListener('click', () => {
    input.value = button.dataset.query;
    runSearch();
  });
}
byId('reload').addEventListener('click', () => location.reload());

// Keep an engine restored from the back/forward cache; dispose on real unload.
window.addEventListener('pagehide', event => {
  if (event.persisted) return;
  closed = true;
  controller.abort();
  engine?.dispose();
  engine = undefined;
});

async function initialise() {
  try {
    // Dynamic import lets the page explain a missing generated build.
    const { createSearchEngine } = await import('../wasm/search-engine.mjs');
    engine = await createSearchEngine();
    if (closed) {
      engine.dispose();
      engine = undefined;
      return;
    }
    const documents = await Promise.all(sampleNames.map(async name => {
      const response = await fetch(new URL(`../data/sample/${encodeURIComponent(name)}`, import.meta.url), {
        signal: controller.signal
      });
      if (!response.ok) throw new Error(`Sample document could not be loaded: ${name}`);
      return { id: `data/sample/${name}`, text: await response.text() };
    }));
    if (closed) return;
    const count = engine.loadDocuments(documents);
    for (const doc of documents) {
      const details = element('details');
      details.append(element('summary', '', doc.id.split('/').at(-1)),
        element('pre', 'document-text', doc.text));
      byId('documents').append(details);
    }
    status.dataset.state = 'ready';
    status.textContent = `Engine ready · ${count} documents · searches stay in this browser`;
    controls.disabled = false;
    runSearch();
  } catch (error) {
    controller.abort();
    engine?.dispose();
    engine = undefined;
    if (closed) return;
    console.error('Demo initialisation failed:', error);
    unavailable('The engine or sample collection could not be loaded. Check the build and local server, then reload.');
  }
}

initialise();
