#!/usr/bin/env python3
"""Compile, validate and measure the native engine with no third-party packages."""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import io
import json
import math
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FLAGS = ['-std=c++17', '-O2', '-Wall', '-Wextra', '-pedantic', '-I.']
SOURCES = ['Index.cpp', 'Index.h', 'ExactSearcher.hpp', 'PrefixSearcher.hpp',
           'query_processor.hpp', 'benchmarks/corpus.hpp',
           'benchmarks/search_benchmark.cpp', 'tests/benchmark_tests.cpp',
           'scripts/benchmark.py', 'tests/benchmark_runner_tests.py']
WORKLOADS = ['index', 'trie', 'common_term', 'rare_term', 'phrase', 'prefix',
             'boolean_and', 'boolean_or', 'exclusion', 'missing']


def run(command):
    return subprocess.check_output(command, cwd=ROOT, text=True, encoding='utf-8', errors='replace')


def git(*arguments):
    try:
        return run(['git', *arguments]).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def cpu_name():
    if sys.platform.startswith('linux'):
        for line in Path('/proc/cpuinfo').read_text().splitlines():
            if line.startswith('model name'):
                return line.split(':', 1)[1].strip()
    return platform.processor() or os.environ.get('PROCESSOR_IDENTIFIER', 'unreported')


def parse_output(output, samples, iterations):
    metadata, table = output.split('\n', 1)
    corpus = json.loads(metadata)
    rows = []
    for row in csv.DictReader(io.StringIO(table)):
        name = row['workload']
        sample, loops = int(row['sample']), int(row['iterations'])
        elapsed, checksum = float(row['total_ms']), int(row['checksum'])
        if (name not in WORKLOADS or not 0 <= sample < samples or
                loops != (1 if name in ('index', 'trie') else iterations) or
                not math.isfinite(elapsed) or elapsed < 0 or checksum < 0):
            raise ValueError('Invalid benchmark measurement')
        rows.append(dict(workload=name, sample=sample, iterations=loops,
                         total_ms=elapsed, ms_per_operation=elapsed / loops,
                         checksum=checksum))
    if len(rows) != len(WORKLOADS) * samples:
        raise ValueError('Incomplete benchmark output')
    summary = {}
    for name in WORKLOADS:
        selected = [row for row in rows if row['workload'] == name]
        if {row['sample'] for row in selected} != set(range(samples)):
            raise ValueError('Missing or repeated samples')
        if len({row['checksum'] for row in selected}) != 1:
            raise ValueError('Query/build output changed between samples')
        values = [row['ms_per_operation'] for row in selected]
        summary[name] = dict(median_ms=statistics.median(values), min_ms=min(values), max_ms=max(values))
    return dict(corpus=corpus, summary=summary, measurements=rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sizes', nargs='+', type=int, default=[100, 500, 1000])
    parser.add_argument('--words', type=int, default=128)
    parser.add_argument('--seed', type=int, default=1234)
    parser.add_argument('--samples', type=int, default=5)
    parser.add_argument('--iterations', type=int, default=3)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'g++'), help='Compiler executable, not shell flags')
    parser.add_argument('--output', type=Path, help='New JSON file; default is stdout')
    parser.add_argument('--check-only', action='store_true', help='Compile and run benchmark fixture tests only')
    args = parser.parse_args()
    if (not all(1 <= n <= 10000 and n * args.words <= 2000000 for n in args.sizes)
            or not 16 <= args.words <= 4096 or not 0 <= args.seed <= 2147483647
            or not 3 <= args.samples <= 100 or not 1 <= args.iterations <= 1000):
        parser.error('Invalid dimensions/seed/repetition counts; see docs/performance.md.')
    if args.output and args.output.exists():
        parser.error('Output already exists; choose a new filename.')
    compiler = shutil.which(args.cxx)
    if not compiler:
        parser.error('C++ compiler not found; set --cxx to its executable path.')
    subprocess.run([sys.executable, '-B', str(ROOT / 'tests/benchmark_runner_tests.py')], cwd=ROOT, check=True, stdout=sys.stderr)
    with tempfile.TemporaryDirectory(prefix='search-benchmark-') as temp:
        suffix = '.exe' if os.name == 'nt' else ''
        test = str(Path(temp) / ('fixture-tests' + suffix))
        executable = str(Path(temp) / ('benchmark' + suffix))
        subprocess.run([compiler, *FLAGS, 'tests/benchmark_tests.cpp', 'Index.cpp', '-o', test], cwd=ROOT, check=True)
        print(run([test]).strip(), file=sys.stderr)
        if args.check_only:
            return
        subprocess.run([compiler, *FLAGS, 'benchmarks/search_benchmark.cpp', 'Index.cpp', '-o', executable], cwd=ROOT, check=True)
        report = dict(schema_version=1, corpus_version=1,
                      started_utc=datetime.now(timezone.utc).isoformat(),
                      environment=dict(os=platform.platform(), machine=platform.machine(),
                                       cpu=cpu_name(), logical_cpus=os.cpu_count(),
                                       compiler=run([compiler, '--version']).strip(), flags=FLAGS,
                                       python=platform.python_version()),
                      source=dict(git_head=git('rev-parse', 'HEAD'),
                                  git_status=git('status', '--short'),
                                  sha256={name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in SOURCES}),
                      method=dict(clock='std::chrono::steady_clock', top_k=10,
                                  build_warmups=1, query_warmups_per_workload=1,
                                  samples=args.samples, query_iterations=args.iterations,
                                  query_scope='executeQuery + returned-result snippets + response destruction'),
                      runs=[])
        for size in args.sizes:
            print(f'Measuring {size} documents x {args.words} words...', file=sys.stderr)
            output = run([executable, str(size), str(args.words), str(args.seed), str(args.samples), str(args.iterations)])
            report['runs'].append(parse_output(output, args.samples, args.iterations))
        payload = json.dumps(report, indent=2) + '\n'
        if args.output:
            with args.output.open('x', encoding='utf-8', newline='\n') as handle:
                handle.write(payload)
            print(f'Wrote {args.output}', file=sys.stderr)
        else:
            print(payload, end='')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
