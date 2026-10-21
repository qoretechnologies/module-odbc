#!/usr/bin/env python3
# Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
"""Offline regression for refused/coerced connection options and unsigned timeout boundaries."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('module', type=Path)
    parser.add_argument('--valgrind', action='store_true')
    args = parser.parse_args()
    module = args.module.resolve(strict=True)
    root = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix='odbc-connect-options-') as temp:
        library = Path(temp) / 'fault.so'
        subprocess.run(['c++', '-std=c++17', '-g', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                        str(root / 'connection-options.cpp'), '-ldl', '-o', str(library)], check=True)
        command = ['qore', '--exec-mode=ast', '-b', '--enable-debug', '-l', str(module),
                   str(root / 'connection-options.qtest')]
        if args.valgrind:
            command = ['valgrind', '--quiet', '--error-exitcode=99', '--leak-check=full',
                       '--errors-for-leak-kinds=definite,indirect'] + command
        result = subprocess.run(command, env=dict(os.environ, LD_PRELOAD=str(library)),
                                capture_output=True, text=True, timeout=240)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        # Autocommit is always mandatory; only unconfigured timeout defaults may reach connect.
        expected = ['ATTRIBUTE 0'] * 2
        for _ in range(4):  # each timeout, once rejected and once coerced
            expected += ['ATTRIBUTE 60', 'CONNECT', 'ATTRIBUTE 0', 'ATTRIBUTE 1', 'ATTRIBUTE 4294967295']
        trace = [line for line in result.stderr.splitlines() if line.startswith(('ATTRIBUTE ', 'CONNECT'))]
        assert trace == expected, trace
        print(result.stdout, end='')
        print('18 connection option fault cases passed')


if __name__ == '__main__':
    main()
