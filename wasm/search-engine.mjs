import createModule from './generated/search-engine.mjs';

// The facade owns every Embind handle; callers receive ordinary JS values.
export async function createSearchEngine(moduleOptions = {}) {
  const module = await createModule(moduleOptions);
  let engine = new module.SearchEngine();
  const requireEngine = () => {
    if (!engine) throw new Error('Search engine has been disposed.');
    return engine;
  };

  return {
    loadDocuments(documents) {
      const active = requireEngine();
      if (!Array.isArray(documents)) throw new TypeError('documents must be an array.');
      // Validate and snapshot the whole batch before creating any native handles.
      const snapshot = [];
      for (const document of documents) {
        if (!document || typeof document.id !== 'string' || document.id.length === 0 ||
            typeof document.text !== 'string') {
          throw new TypeError('Each document needs a non-empty string id and string text.');
        }
        snapshot.push({ id: document.id, text: document.text });
      }
      const nativeDocuments = new module.TextDocuments();
      try {
        for (const document of snapshot) nativeDocuments.push_back(document);
        return active.loadDocuments(nativeDocuments);
      } finally {
        nativeDocuments.delete();
      }
    },
    search(query, topK = 10) {
      const active = requireEngine();
      if (typeof query !== 'string') throw new TypeError('query must be a string.');
      if (!Number.isInteger(topK) || topK < -2147483648 || topK > 2147483647) {
        throw new RangeError('topK must be a signed 32-bit integer.');
      }
      return active.search(query, topK);
    },
    dispose() {
      if (engine) {
        engine.delete();
        engine = null;
      }
    }
  };
}
