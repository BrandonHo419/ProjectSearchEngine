import { copyFile, mkdir, readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
const root = new URL('../', import.meta.url);
const assets = [
  ['pdfjs-dist', '6.5.438', 'build/pdf.mjs', 'pdf.mjs'],
  ['pdfjs-dist', '6.5.438', 'build/pdf.worker.mjs', 'pdf.worker.mjs'],
  ['pdfjs-dist', '6.5.438', 'LICENSE', 'PDFJS-LICENSE'],
  ['mammoth', '1.13.0', 'mammoth.browser.js', 'mammoth.browser.js'],
  ['mammoth', '1.13.0', 'LICENSE', 'MAMMOTH-LICENSE'],
];
await mkdir(new URL('demo/vendor/', root), { recursive: true });
for (const [name, version, source, target] of assets) {
  const base = new URL(`node_modules/${name}/`, root);
  const metadata = JSON.parse(await readFile(new URL('package.json', base), 'utf8'));
  if (metadata.version !== version) throw new Error(`Expected ${name} ${version}; run npm ci.`);
  await copyFile(new URL(source, base), new URL(`demo/vendor/${target}`, root));
}
console.log('Prepared pinned browser parsers in ' + fileURLToPath(new URL('demo/vendor/', root)));
