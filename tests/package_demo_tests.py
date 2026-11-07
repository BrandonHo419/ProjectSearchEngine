"""Publication boundary regression tests; no SDK required."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('package_demo', Path(__file__).resolve().parents[1] / 'scripts/package_demo.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'
        self.output = self.root / 'output'
        for name in module.ASSETS:
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'fixture')

    def test_allowlist_and_byte_preservation(self):
        (self.source / 'data/sample/private.txt').write_text('not public')
        (self.source / 'demo/unrelated.txt').write_text('not public')
        module.package(self.source, self.output)
        files = {p.relative_to(self.output).as_posix() for p in self.output.rglob('*') if p.is_file()}
        self.assertEqual(files, set(module.ASSETS) | {'index.html', '.nojekyll'})
        for name in module.ASSETS:
            self.assertEqual((self.output / name).read_bytes(), b'fixture')
        self.assertIn('url=demo/', (self.output / 'index.html').read_text())

    def test_missing_asset_creates_no_package(self):
        (self.source / module.ASSETS[-1]).unlink()
        with self.assertRaises(ValueError):
            module.package(self.source, self.output)
        self.assertFalse(self.output.exists())

    def test_nonempty_output_is_preserved(self):
        self.output.mkdir()
        marker = self.output / 'keep.txt'
        marker.write_text('keep')
        with self.assertRaises(ValueError):
            module.package(self.source, self.output)
        self.assertEqual(marker.read_text(), 'keep')

    def test_linked_asset_is_rejected(self):
        path = self.source / module.ASSETS[0]
        path.unlink()
        try:
            path.symlink_to(self.source / module.ASSETS[1])
        except OSError:
            self.skipTest('Symlinks unavailable')
        with self.assertRaises(ValueError):
            module.package(self.source, self.output)
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main()
