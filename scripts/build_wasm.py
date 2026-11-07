#!/usr/bin/env python3
"""Build the optional bridge with the pinned Emscripten SDK on PATH."""
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
VERSION = "4.0.10"


def main():
    compiler = shutil.which("em++")
    if not compiler:
        sys.exit("em++ not found. Activate Emscripten 4.0.10; see README.md (Browser build).")
    version = subprocess.run([compiler, "--version"], check=True, capture_output=True, text=True)
    if not re.search(r"\b" + re.escape(VERSION) + r"\b", version.stdout):
        sys.exit(f"Expected Emscripten {VERSION}. Activate the pinned SDK; see README.md (Browser build).")
    output = ROOT / "wasm" / "generated"
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-pedantic", "-I.",
        "wasm/bindings.cpp", "Index.cpp", "--bind", "--no-entry", "-fexceptions",
        "-sMODULARIZE=1", "-sEXPORT_ES6=1", "-sEXPORT_NAME=createSearchModule",
        "-sENVIRONMENT=web,node", "-sALLOW_MEMORY_GROWTH=1", "-sFILESYSTEM=0",
        "-o", str(output / "search-engine.mjs")
    ], cwd=ROOT, check=True)
    print("Built wasm/generated/search-engine.mjs and search-engine.wasm")


if __name__ == "__main__":
    main()
