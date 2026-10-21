Native binding failure regression
=================================

Copyright 2026 Qore Technologies, s.r.o.

The native fixture first checks array allocation arithmetic at zero-width and
integer-overflow boundaries, without attempting oversized allocations. It then
injects one ``SQL_ERROR`` from ``SQLBindParameter`` when
binding the first NULL column. The test checks that Qore raises
``ODBC-BIND-ERROR``, that no later parameter is bound after that failure, and
that a new execution on the same statement succeeds.

Run it with a local module build and the private PostgreSQL fixture::

    export QORE_ODBC_BINARY_MODULE="$PWD/build-debug/odbc-api-$(qore --latest-module-api).qmod"
    rpm/test-postgres test --native

This runs the functional suites and the native regression together. It requires
a C++ compiler, unixODBC headers, PostgreSQL server tools, and the PostgreSQL ODBC
driver. The fixture creates and removes its own database cluster. RPM ``%check``
runs this regression against the newly built module. Installed runtime checks
run the functional suites without needing a compiler or development headers.

To run only the injected failure against an existing configured database::

    export QORE_DB_CONNSTR_ODBC='odbc:database connection string'
    python3 -B -W error test/native/run-bind-failure.py "$QORE_ODBC_BINARY_MODULE"

The helper builds the interposer in a temporary directory and preloads it only
into its test subprocess. It checks the exact binding trace as well as the
QUnit assertions; unexpected extra binding calls fail qualification.

Driver fault regressions
------------------------

``driver-faults.cpp`` is a second interposer, selected by environment variables:
``ODBC_FAULT_NEED_DATA=1`` binds a character parameter whose value is
``__need_data__`` for data at execution (a real ``SQL_DATA_AT_EXEC``
indicator), so the driver answers the execution with ``SQL_NEED_DATA``;
``ODBC_FAULT_DISCONNECT=1`` makes ``SQLDisconnect`` fail without disconnecting.
``run-driver-faults.py`` builds it and runs ``driver-faults.qtest`` once per
fault, each with a 60 second limit so that a hang fails, and checks how often
``SQLDisconnect`` is called: exactly once per closed connection. It runs with
any configured database::

    export QORE_DB_CONNSTR_ODBC='odbc:database connection string'
    python3 -B -W error test/native/run-driver-faults.py "$QORE_ODBC_BINARY_MODULE"

Connection option failures
--------------------------

``python3 test/native/run-connection-options.py build-debug/odbc-api-2.0.qmod``
checks rejected and coerced transaction/timeout attributes before network access,
including zero and the maximum unsigned 32-bit timeout. ``--valgrind`` runs each
case under Memcheck. The interposer is private to each subprocess. See
``patches/README.rst`` for native psqlODBC connection fixes and their failover test.
