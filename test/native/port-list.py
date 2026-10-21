#!/usr/bin/env python3
# Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
"""Check that native psqlODBC preserves two five-digit ports during libpq failover.

Pass a driver name or absolute native library path. The first loopback server accepts TCP but
never replies; the second rejects authentication. No database, firewall changes or sleeps are used.
"""
import argparse
import os
from pathlib import Path
import socket
import struct
import subprocess
import threading


def receive(sock, length):
    data = b''
    while len(data) < length:
        part = sock.recv(length - len(data))
        if not part:
            raise EOFError('incomplete startup packet')
        data += part
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('module', type=Path)
    parser.add_argument('driver')
    args = parser.parse_args()
    reached = threading.Event()
    stop = threading.Event()
    errors = []
    with socket.socket() as silent, socket.socket() as target:
        for sock in (silent, target):
            sock.bind(('127.0.0.1', 0))
            sock.listen()
        ports = [sock.getsockname()[1] for sock in (silent, target)]
        assert all(10000 <= p <= 65535 for p in ports), ports

        def reject():
            try:
                with target.accept()[0] as peer:
                    if stop.is_set():
                        return
                    reached.set()
                    peer.settimeout(5)
                    length, = struct.unpack('!I', receive(peer, 4))
                    assert 8 <= length <= 65536, length
                    receive(peer, length - 4)
                    fields = b'SFATAL\0VFATAL\0C28P01\0Mport list fixture reached\0\0'
                    peer.sendall(b'E' + struct.pack('!I', len(fields) + 4) + fields)
            except Exception as error:
                errors.append(error)

        thread = threading.Thread(target=reject)
        thread.start()
        try:
            env = os.environ | {'ODBC_PORT_TEST_DRIVER': args.driver,
                                'ODBC_PORT_TEST_PORTS': ','.join(map(str, ports))}
            script = r'''%modern
%requires odbc
Datasource ds({"type": "odbc", "user": "fixture", "pass": "fixture", "options": {
    "login-timeout": 1, "conn": sprintf("DRIVER={%s};SERVER=127.0.0.1,127.0.0.1;PORT=%s;DATABASE=fixture;SSLmode=disable;Gssencmode=disable;",
        ENV.ODBC_PORT_TEST_DRIVER, ENV.ODBC_PORT_TEST_PORTS)}});
try {
    ds.open();
} catch (hash<ExceptionInfo> error) {
    printf("%s\n", error.desc);
}
'''
            result = subprocess.run(['qore', '-b', '--enable-debug', '-l',
                                     str(args.module.resolve(strict=True)), '-'],
                                    input=script, text=True, capture_output=True, env=env, timeout=10)
            assert result.returncode == 0, result.stderr
            assert not result.stderr, result.stderr
            assert reached.is_set(), 'second port was not reached: ' + result.stdout
            assert 'port list fixture reached' in result.stdout, result.stdout
            assert not errors, errors
            print('native psqlODBC preserved both five-digit ports and failed over after the login timeout')
        finally:
            stop.set()
            if not reached.is_set():
                with socket.create_connection(target.getsockname(), timeout=2):
                    pass
            thread.join(6)
            assert not thread.is_alive(), 'fixture did not stop'


if __name__ == '__main__':
    main()
