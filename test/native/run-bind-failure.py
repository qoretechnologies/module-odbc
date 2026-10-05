#!/usr/bin/python3
# Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
"""Check immediate NULL binding failure propagation inside the PostgreSQL fixture."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path, help="locally built ODBC .qmod")
    args = parser.parse_args()
    module = args.module.resolve(strict=True)
    if not os.environ.get("QORE_DB_CONNSTR_ODBC"):
        parser.error("run inside rpm/with-postgres.py")
    root = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="qore-odbc-bind-") as temporary:
        boundary = Path(temporary) / "array-size"
        subprocess.run(["c++", "-std=c++11", "-O2", "-g", "-Wall", "-Wextra", "-Werror",
                        "-I", str(root.parent.parent / "src"), str(root / "array-size.cpp"),
                        "-o", str(boundary)], check=True)
        subprocess.run([str(boundary)], check=True)
        library = Path(temporary) / "failure.so"
        subprocess.run(["c++", "-std=c++11", "-O2", "-g", "-Wall", "-Wextra", "-Werror",
                        "-shared", "-fPIC", str(root / "bind-failure.cpp"), "-ldl",
                        "-o", str(library)], check=True)
        env = dict(os.environ, LD_PRELOAD=str(library))
        result = subprocess.run(["qore", "-b", "--enable-debug", "-l", str(module),
                                 str(root / "bind-failure.qtest"), "-v"],
                                env=env, capture_output=True, text=True, timeout=60)
        print(result.stdout, end="")
        print(result.stderr, end="")
        result.check_returncode()
        trace = [line for line in result.stderr.splitlines() if line.startswith("ODBC_BIND_TRACE ")]
        expected = ["ODBC_BIND_TRACE injected column 1", "ODBC_BIND_TRACE bind column 1",
                    "ODBC_BIND_TRACE bind column 2"]
        if trace != expected:
            raise AssertionError("Binding continued after the first failed NULL parameter: " + repr(trace))
        print("Binding stops at the injected error and a fresh execution recovers")


if __name__ == "__main__":
    main()
