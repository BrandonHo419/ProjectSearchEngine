// A short-lived worker keeps document extraction off the UI thread.
self.onmessage = async ({ data: { file, maxCharacters } }) => {
  try {
    const buffer = await file.arrayBuffer();
    let text = '';
    if (file.name.toLowerCase().endsWith('.pdf')) {
      const pdfjs = await import('./vendor/pdf.mjs');
      pdfjs.GlobalWorkerOptions.workerSrc = new URL('./vendor/pdf.worker.mjs', self.location.href).href;
      const task = pdfjs.getDocument({ data: new Uint8Array(buffer), isEvalSupported: false,
        useWasm: false, useWorkerFetch: false, disableFontFace: true, stopAtErrors: true });
      try {
        const pdf = await task.promise;
        if (pdf.numPages > 100) throw new Error('PDFs are limited to 100 pages.');
        for (let i = 1; i <= pdf.numPages; i++) {
          const page = await pdf.getPage(i);
          const content = await page.getTextContent();
          text += content.items.map(item => item.str ?? '').join(' ') + '\n';
          page.cleanup();
          if (text.length > maxCharacters) throw new Error('Extracted text is too large.');
        }
      } finally { await task.destroy(); }
    } else {
      // Bound declared ZIP expansion before giving a DOCX to its parser.
      const bytes = new Uint8Array(buffer), view = new DataView(buffer);
      let end = bytes.length - 22;
      while (end >= Math.max(0, bytes.length - 65557) && view.getUint32(end, true) !== 0x06054b50) end--;
      if (end < Math.max(0, bytes.length - 65557)) throw new Error('Invalid DOCX archive.');
      const entries = view.getUint16(end + 10, true);
      let position = view.getUint32(end + 16, true), expanded = 0;
      if (entries > 500) throw new Error('DOCX archive has too many entries.');
      for (let i = 0; i < entries; i++) {
        if (position + 46 > bytes.length || view.getUint32(position, true) !== 0x02014b50)
          throw new Error('Invalid DOCX archive.');
        expanded += view.getUint32(position + 24, true);
        if (expanded > 8388608) throw new Error('DOCX expanded content exceeds 8 MiB.');
        position += 46 + view.getUint16(position + 28, true) + view.getUint16(position + 30, true) + view.getUint16(position + 32, true);
      }
      importScripts('./vendor/mammoth.browser.js');
      const result = await self.mammoth.extractRawText({ arrayBuffer: buffer }, { externalFileAccess: false });
      text = result.value;
    }
    if (text.length > maxCharacters) throw new Error('Extracted text is too large.');
    self.postMessage({ kind: 'extraction-result', text });
  } catch (error) {
    self.postMessage({ kind: 'extraction-result', error: error.name === 'PasswordException' ? 'Password-protected PDFs are unsupported.' : error.message });
  }
};
