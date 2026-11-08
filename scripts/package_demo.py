#!/usr/bin/env python3
"""Copy only public demo assets into a new, empty output directory."""
from pathlib import Path
import argparse
import shutil

ROOT = Path(__file__).resolve().parents[1]
ASSETS = (
    'demo/index.html', 'demo/styles.css', 'demo/app.mjs',
    'demo/file-import.mjs', 'demo/parser-worker.js',
    'demo/vendor/pdf.mjs', 'demo/vendor/pdf.worker.mjs',
    'demo/vendor/mammoth.browser.js', 'demo/vendor/PDFJS-LICENSE', 'demo/vendor/MAMMOTH-LICENSE',
    'wasm/search-engine.mjs', 'wasm/generated/search-engine.mjs',
    'wasm/generated/search-engine.wasm',
    'data/sample/testfile1.txt', 'data/sample/testfile2.txt',
    'data/sample/testfile3.txt', 'data/sample/extreme secret.txt',
)


def package(source, output):
    # Validate everything before creating output; never copy directory contents.
    for name in ASSETS:
        path = source / name
        if not path.is_file() or any(p.is_symlink() for p in (path, *path.parents)):
            raise ValueError(f'Missing or linked asset: {name}')
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise ValueError('Output must be a new or empty directory.')
    output.mkdir(parents=True, exist_ok=True)
    for name in ASSETS:
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / name, target)
    (output / '.nojekyll').touch()
    (output / 'index.html').write_text(
        '<!doctype html><html lang="en"><meta charset="utf-8">'
        '<meta http-equiv="refresh" content="0;url=demo/">'
        '<title>Search engine demonstration</title>'
        '<a href="demo/">Open the search demonstration</a></html>\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    package(ROOT, args.output)
    print(f'Packaged {len(ASSETS)} assets into {args.output}')
