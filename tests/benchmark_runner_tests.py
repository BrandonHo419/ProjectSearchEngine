"""Check measurement accounting and fail-closed report parsing."""
import importlib.util
from pathlib import Path
import unittest
import sys
sys.dont_write_bytecode = True

spec = importlib.util.spec_from_file_location('benchmark_runner', Path(__file__).resolve().parents[1] / 'scripts/benchmark.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def fixture():
    rows = ['{}', 'workload,sample,iterations,total_ms,checksum']
    for name in module.WORKLOADS:
        loops = 1 if name in ('index', 'trie') else 3
        for sample, milliseconds in enumerate([3, 9, 6]):
            rows.append(f'{name},{sample},{loops},{milliseconds},42')
    return '\n'.join(rows) + '\n'


class RunnerTests(unittest.TestCase):
    def test_batch_normalization_and_median(self):
        report = module.parse_output(fixture(), 3, 3)
        self.assertEqual(report['summary']['index'], dict(median_ms=6, min_ms=3, max_ms=9))
        self.assertEqual(report['summary']['common_term'], dict(median_ms=2, min_ms=1, max_ms=3))
        self.assertEqual(len(report['measurements']), 30)

    def test_incomplete_output(self):
        with self.assertRaises(ValueError):
            module.parse_output(fixture().replace('index,0,1,3,42\n', ''), 3, 3)

    def test_repeated_sample(self):
        with self.assertRaises(ValueError):
            module.parse_output(fixture().replace('index,1,', 'index,0,'), 3, 3)

    def test_invalid_elapsed(self):
        for value in ['nan', 'inf', '-1']:
            with self.assertRaises(ValueError):
                module.parse_output(fixture().replace('index,0,1,3,42', f'index,0,1,{value},42'), 3, 3)

    def test_changed_output(self):
        with self.assertRaises(ValueError):
            module.parse_output(fixture().replace('index,0,1,3,42', 'index,0,1,3,43'), 3, 3)


if __name__ == '__main__':
    unittest.main()
