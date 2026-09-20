#!/usr/bin/env python3
"""Checks the Djehuti Suite's standing requirements against the source tree.

Run from the repository root (CI runs it on every pull request):

    python scripts/check-requirements.py

Exits nonzero, listing every violation, if a rule is broken. The rules come from the user's own
requirements for FRust in the suite:

  R1  No external tools. The applications never start the FRust compiler, Frate, a linker or any
      other program to compile or build.
  R2  The FRust libraries know nothing about the operating system's file system or console. All
      file access and logging goes through a HostEnvironment (the VFS, in the Suite).
  R3  The applications keep no build-machine file paths and read no built-in resource from a path.
  R4  No library ships with an application (checked on the built executable by the release
      workflow; this script checks the packaging rules in source).

A rule that still has known exceptions lists them in ALLOW below, each with the reason. An exception is
a debt to remove, not a permission.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# --------------------------------------------------------------------------------------
# R1: no external tools, in the applications that have been converted so far.
# --------------------------------------------------------------------------------------

R1_ROOTS = ["apps/CreationStation/Source", "apps/CreationStation/CMakeLists.txt", "shared"]
R1_PATTERN = re.compile(
    r"frust_compiler|frate\.exe|frate_cli|CS_FRATE_EXECUTABLE|CS_FRUST_COMPILER|--emit-obj|\blink\.exe\b|"
    r"juce::ChildProcess|\bChildProcess\b|CreateProcess|\b_spawn\w*\b|\bsystem\s*\("
)

# path fragment -> reason
R1_ALLOW = {
    "shared/AssetSystem/src/SuiteVfsServiceClient.cpp": "starts the VFS service, which is a separate service by design",
    "shared/AssetSystem/src/SuiteProcessRegistry.cpp": "VFS service bookkeeping",
    "shared/Services/tests/SuiteProjectHandoffMultiProcessSmoke.cpp": "a test that starts a second copy of itself to prove two processes can share a project",
    "scripts/": "this checker",
}

# --------------------------------------------------------------------------------------
# R2: environment-only code: no file, console or process API.
# --------------------------------------------------------------------------------------

R2_FILES = [
    # FRust compiler and Frate libraries (the parts an application links)
    "third_party/FrustLang/projects/01_language_paradigms/02_functional/CompilerApi.cpp",
    "third_party/FrustLang/projects/01_language_paradigms/02_functional/CompilerApi.h",
    "third_party/FrustLang/projects/01_language_paradigms/02_functional/CompilerFrontend.h",
    "third_party/FrustLang/projects/01_language_paradigms/02_functional/EnvironmentCompile.cpp",
    "third_party/FrustLang/projects/01_language_paradigms/02_functional/EnvironmentCompile.h",
    "third_party/FrustLang/projects/01_language_paradigms/02_functional/HostEnvironment.h",
    "third_party/FrustLang/projects/01_language_paradigms/02_functional/MemoryEnvironment.h",
    "third_party/FrustLang/projects/01_language_paradigms/02_functional/ModuleLoader.cpp",
    "third_party/FrustLang/projects/05_frate/src/PodArchive.cpp",
    "third_party/FrustLang/projects/05_frate/src/PodBuild.cpp",
    "third_party/FrustLang/projects/05_frate/src/PodEnvironment.cpp",
    "third_party/FrustLang/projects/05_frate/src/FrateConfig.cpp",
    "third_party/FrustLang/projects/05_frate/src/FrateRegistryClient.cpp",
    # the Suite's FRust front end and Station's use of it
    "shared/FrustHost/src/SuiteFrust.cpp",
    "shared/FrustHost/src/VfsEnvironment.cpp",
    "shared/FrustPluginRuntime/src/PluginRuntime.cpp",
    "apps/CreationStation/Source/Language/StationFrustPodService.cpp",
    "apps/CreationStation/Source/Views/DslPanel.cpp",
]
R2_PATTERN = re.compile(
    r"std::i?o?fstream|std::filesystem|\bfopen\b|\bfreopen\b|juce::File\b|juce::FileInputStream|"
    r"juce::FileOutputStream|juce::TemporaryFile|juce::ChildProcess|\bgetenv\b|createTempFile|"
    r"\bstd::cout\b|\bstd::cerr\b|\bstd::clog\b|\bprintf\s*\(|\bfprintf\s*\("
)
R2_ALLOW = {
    "apps/CreationStation/Source/Views/DslPanel.cpp": "loadSourceFromFile: the user picks a source file to import through the OS file dialog (allowed for import/export)",
}

# --------------------------------------------------------------------------------------
# R3: no build-machine paths, no built-in resource read from a path.
# --------------------------------------------------------------------------------------

R3_ROOTS = ["apps/CreationStation/Source"]
R3_PATTERN = re.compile(r"CS_SIGNAL_LAB_\w+|CS_[A-Z_]*_EXECUTABLE|CMAKE_CURRENT_SOURCE_DIR")
R3_ALLOW: dict[str, str] = {}

# --------------------------------------------------------------------------------------
# R4: packaging rules (source side).
# --------------------------------------------------------------------------------------

R4_FILES = ["apps/CreationStation/CMakeLists.txt", "apps/CreationStation/.github/workflows/release.yml"]
R4_PATTERN = re.compile(r"install\s*\(\s*(?:FILES|PROGRAMS)[^)]*\.dll|copy[^\n]*\.dll|\bTARGET_RUNTIME_DLLS\b", re.I)
R4_ALLOW: dict[str, str] = {}


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def source_files(entry: str):
    path = ROOT / entry
    if path.is_file():
        yield path
    elif path.is_dir():
        for p in sorted(path.rglob("*")):
            if p.is_file() and p.suffix in {".cpp", ".h", ".hpp", ".cmake", ".txt", ".yml"} and "/build/" not in rel(p) + "/":
                yield p


def strip_comments(text: str) -> str:
    """Removes // and /* */ comments so a rule is not tripped by prose that names a forbidden thing."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def scan(files, pattern, allow, comments_count=False):
    problems = []
    for path in files:
        name = rel(path)
        if any(fragment in name for fragment in allow):
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        body = text if comments_count or path.suffix in {".yml", ".txt", ".cmake"} else strip_comments(text)
        for number, line in enumerate(body.splitlines(), start=1):
            if line.strip().startswith("#") and path.suffix in {".txt", ".cmake", ".yml"}:
                continue
            match = pattern.search(line)
            if match:
                problems.append(f"{name}:{number}: {match.group(0)}")
    return problems


def main() -> int:
    failed = False

    def report(rule: str, title: str, problems: list[str]):
        nonlocal failed
        if problems:
            failed = True
            print(f"\n{rule} FAILED: {title}")
            for p in problems:
                print(f"  {p}")
        else:
            print(f"{rule} ok: {title}")

    r1_files = [p for entry in R1_ROOTS for p in source_files(entry)]
    report("R1", "no application or shared code starts the compiler, Frate, a linker or another program",
           scan(r1_files, R1_PATTERN, R1_ALLOW))

    missing = [f for f in R2_FILES if not (ROOT / f).is_file()]
    if missing:
        failed = True
        print("\nR2 FAILED: files the rule covers are missing (rename or moved without updating this script):")
        for m in missing:
            print(f"  {m}")
    r2_files = [ROOT / f for f in R2_FILES if (ROOT / f).is_file()]
    report("R2", "the FRust libraries, SuiteFrust and Station's FRust code touch no file, console or process",
           scan(r2_files, R2_PATTERN, R2_ALLOW))

    r3_files = [p for entry in R3_ROOTS for p in source_files(entry)]
    report("R3", "Station has no build-machine path and reads no built-in resource from a path",
           scan(r3_files, R3_PATTERN, R3_ALLOW))

    r4_files = [ROOT / f for f in R4_FILES if (ROOT / f).is_file()]
    report("R4", "Station's packaging copies no library file", scan(r4_files, R4_PATTERN, R4_ALLOW))

    print("\nRequirements check: " + ("FAILED" if failed else "passed"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
