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
