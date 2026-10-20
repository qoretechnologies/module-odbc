#!/usr/bin/python3
# Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
"""Check how the module handles driver faults: a statement left waiting for data at execution, and a connection
that the driver cannot disconnect.  The faults are injected by an interposer preloaded into the test process only;
each scenario runs with a time limit, so that a hang fails instead of blocking the suite."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

# fault variable, expected trace counts (a disconnect is attempted exactly once per closed connection)
SCENARIOS = (
    ("ODBC_FAULT_NEED_DATA", {"ODBC_FAULT_TRACE data at execution for column 1": 3,
                              "ODBC_FAULT_TRACE SQLDisconnect": 4}),
    ("ODBC_FAULT_DISCONNECT", {"ODBC_FAULT_TRACE SQLDisconnect refused": 2}),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path, help="locally built ODBC .qmod")
    args = parser.parse_args()
    module = args.module.resolve(strict=True)
    if not os.environ.get("QORE_DB_CONNSTR_ODBC"):
        parser.error("QORE_DB_CONNSTR_ODBC must give the test database")
    root = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="qore-odbc-faults-") as temporary:
        library = Path(temporary) / "driver-faults.so"
        subprocess.run(["c++", "-std=c++11", "-O2", "-g", "-Wall", "-Wextra", "-Werror",
                        "-shared", "-fPIC", str(root / "driver-faults.cpp"), "-ldl",
                        "-o", str(library)], check=True)
        for fault, expected in SCENARIOS:
            env = dict(os.environ, LD_PRELOAD=str(library))
            env[fault] = "1"
            try:
                result = subprocess.run(["qore", "-b", "--enable-debug", "-l", str(module),
                                         str(root / "driver-faults.qtest"), "-v"],
                                        env=env, capture_output=True, text=True, timeout=60)
            except subprocess.TimeoutExpired as e:
                print(e.stdout.decode() if isinstance(e.stdout, bytes) else (e.stdout or ""), end="")
                raise AssertionError(fault + ": the test did not end within 60 seconds")
            print(result.stdout, end="")
            print(result.stderr, end="")
            result.check_returncode()
            trace = [line for line in result.stderr.splitlines() if line.startswith("ODBC_FAULT_TRACE ")]
            counts = {line: trace.count(line) for line in expected}
            if counts != expected:
                raise AssertionError(fault + ": unexpected driver calls: " + repr(trace))
            print(fault + ": handled")


if __name__ == "__main__":
    main()
