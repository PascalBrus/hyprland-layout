"""Finds code names in the docs that no longer exist in the code.

    python3 -I .githooks/doc-names.py

A camelCase or PascalCase name in backticks in a doc below must appear
somewhere in src/, tests/, cmake/, CMakeLists.txt or examples/. Run from the
repo root. Exits 1 and lists each stale name, with its file and line.
"""

import re
import subprocess
import sys

DOCS = ["README.md", "CODING_STANDARDS.md", "GLOSSARY.md", "CLAUDE.md"]

# Hyprland's and git's names, which the docs cite but this code never uses.
EXTERNAL = {"DwindleAlgorithm", "LuaBindingsDispatchers", "MonocleAlgorithm", "hooksPath"}

SPAN = re.compile(r"`([^`\n]+)`")
NAME = re.compile(r"\b([a-z]+[A-Z]\w*|[A-Z][a-z]+[A-Z]\w*)\b")


def git_files(*paths):
    out = subprocess.run(["git", "ls-files", *paths], capture_output=True, text=True, check=True)
    return out.stdout.split()


code = set()
for path in git_files("src", "tests", "cmake", "CMakeLists.txt", "examples"):
    with open(path, errors="ignore") as f:
        code.update(re.findall(r"\w+", f.read()))

stale = []
for doc in git_files(*DOCS):
    with open(doc) as f:
        for n, line in enumerate(f, 1):
            for span in SPAN.findall(line):
                for name in NAME.findall(span):
                    if name not in code and name not in EXTERNAL:
                        stale.append(f"{doc}:{n}: `{name}` is not in the code")

if stale:
    print("\n".join(stale))
    print("doc-names: rename or remove these, or add a Hyprland name to EXTERNAL", file=sys.stderr)
    sys.exit(1)
