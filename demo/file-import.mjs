export const LIMITS = { files: 20, bytes: 1048576, characters: 200000, words: 20000 };
const textTypes = new Set(['txt', 'md', 'markdown', 'csv', 'json', 'html', 'htm', 'xml']);
export function extension(name) { return name.split('.').pop().toLowerCase(); }
export function validateFiles(files) {
  if (!files.length) throw new Error('Choose at least one file.');
  if (files.length > LIMITS.files) throw new Error('Choose at most 20 files.');
  if (files.reduce((sum, file) => sum + file.size, 0) > LIMITS.bytes)
    throw new Error('Choose files totalling at most 1 MiB.');
  for (const file of files) {
    if (!textTypes.has(extension(file.name)) && !['pdf', 'docx'].includes(extension(file.name)))
      throw new Error(`${file.name}: unsupported format. Choose TXT, Markdown, CSV, JSON, HTML, XML, PDF or DOCX.`);
  }
}

function markupText(text, xml) {
  let container;
  if (xml) {
    container = new DOMParser().parseFromString(text, 'application/xml');
    if (container.querySelector('parsererror')) throw new Error('Invalid XML.');
  } else {
    // Template contents stay detached and inert, including external resources.
    const template = document.createElement('template');
    template.innerHTML = text;
    container = template.content;
  }
  container.querySelectorAll('script, style, noscript, template').forEach(node => node.remove());
  const walk = node => {
    if (node.nodeType === 3) return node.textContent;
    const text = [...node.childNodes].map(walk).join('');
    return node.nodeType === 1 && (xml || /^(p|div|br|li|tr|td|h[1-6]|section|article)$/i.test(node.nodeName)) ? ` ${text} ` : text;
  };
  return walk(container).replace(/\s+/g, ' ').trim();
}

function parseDocument(file, signal) {
  return new Promise((resolve, reject) => {
    const worker = new Worker(new URL('./parser-worker.js', import.meta.url));
    const finish = (error, text) => {
      clearTimeout(timer);
      signal?.removeEventListener('abort', abort);
      worker.terminate();
      error ? reject(error) : resolve(text);
    };
    const abort = () => finish(new Error('Import cancelled.'));
    const timer = setTimeout(() => finish(new Error('Document parsing timed out.')), 15000);
    worker.onmessage = event => {
      if (event.data?.kind !== 'extraction-result') return;
      event.data.error ? finish(new Error(event.data.error)) : finish(null, event.data.text);
    };
    worker.onerror = () => finish(new Error('Document parser unavailable. Rebuild parser assets and reload.'));
    signal?.addEventListener('abort', abort, { once: true });
    if (signal?.aborted) abort();
    else worker.postMessage({ file, maxCharacters: LIMITS.characters });
  });
}

export async function readDocuments(files, { signal, progress = () => {} } = {}) {
  validateFiles(files);
  const documents = [];
  let characters = 0, words = 0;
  for (const [index, file] of files.entries()) {
    if (signal?.aborted) throw new Error('Import cancelled.');
    progress(`Reading ${index + 1} of ${files.length}: ${file.name}`);
    let text;
    try {
      const kind = extension(file.name);
      if (kind === 'pdf' || kind === 'docx') text = await parseDocument(file, signal);
      else {
        text = new TextDecoder('utf-8', { fatal: true }).decode(await file.arrayBuffer());
        if (text.includes('\0')) throw new Error('Binary text files are unsupported.');
        if (kind === 'json') {
          const flatten = value => value && typeof value === 'object' ? Object.entries(value).flatMap(([key, child]) => [key, ...flatten(child)]) : [String(value)];
          text = flatten(JSON.parse(text)).join(' ');
        }
        if (kind === 'csv') text = text.replace(/[,;\t]/g, ' ');
        if (['html', 'htm', 'xml'].includes(kind)) text = markupText(text, kind === 'xml');
      }
      if (!text.trim()) throw new Error('No searchable text found. Scanned documents need OCR.');
      characters += text.length;
      words += text.trim().split(/\s+/).length;
      if (characters > LIMITS.characters || words > LIMITS.words)
        throw new Error('Collection exceeds 200,000 extracted characters or 20,000 words.');
      // Sequence numbers keep equal filenames distinct; filenames remain opaque text.
      documents.push({ id: `local:${index + 1}/${file.name}`, text });
    } catch (error) { throw new Error(`${file.name}: ${error.message}`); }
  }
  if (signal?.aborted) throw new Error('Import cancelled.');
  return documents;
}
