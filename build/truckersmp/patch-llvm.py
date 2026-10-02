#!/usr/bin/env python3
"""Select Apple's dead-strip linker option in the pinned LLVM iOS build."""
import argparse
from pathlib import Path


OLD = '''      if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        # ld64's implementation'''
NEW = '''      if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin|iOS")
        # ld64's implementation'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "root", nargs="?", type=Path,
        help="LLVM source root (defaults to repo/toolchains/llvm-project)",
    )
    args = parser.parse_args()
    root = args.root or Path(__file__).resolve().parents[2] / "toolchains/llvm-project"
    source = root / "llvm/cmake/modules/AddLLVM.cmake"
    text = source.read_text(encoding="utf-8")

    old_count = text.count(OLD)
    new_count = text.count(NEW)
    if old_count == 1 and new_count == 0:
        source.write_text(text.replace(OLD, NEW, 1), encoding="utf-8")
    elif old_count == 0 and new_count == 1:
        return
    else:
        raise SystemExit(
            f"Expected exactly one pinned AddLLVM.cmake linker anchor; "
            f"found old={old_count}, patched={new_count}: {source}"
        )


if __name__ == "__main__":
    main()
