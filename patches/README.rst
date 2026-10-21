psqlODBC 16 connection backports
==============================

Copyright 2026 Qore Technologies, s.r.o.

These patches fix defects in the native PostgreSQL ODBC driver, outside the Qore
ODBC module. They target upstream ``REL-16_00_0000``. Building or installing the
Qore module alone does not apply them to a system's ODBC driver.

* ``psqlodbc-16-port-list.patch`` enlarges ``ConnInfo.port`` from ten bytes to
  ``LARGE_REGISTRY_LEN``. The ten-byte buffer silently truncates a list such as
  ``54321,54322`` to ``54321,543`` before passing it to libpq. The replacement
  holds the full list for every address fitting the existing server buffer.
* ``psqlodbc-16-connect-attribute-leak.patch`` releases the previous password,
  connection-settings and libpq-options values before replacing them. Native
  connection parsing reads attributes twice (around DSN defaults), leaking the
  first decoded allocation even for an ordinary failed connection.

Apply in a separate native-driver checkout::

    patch -p1 < /path/to/module-odbc/patches/psqlodbc-16-port-list.patch
    patch -p1 < /path/to/module-odbc/patches/psqlodbc-16-connect-attribute-leak.patch
    ./bootstrap
    CFLAGS='-std=gnu11 -O2 -g' ./configure --with-unixodbc --with-libpq
    make -j4

The C language mode is explicit because this release predates C23. Test without
installing or changing the system's driver registration::

    python3 test/native/port-list.py build-debug/odbc-api-2.0.qmod /path/to/psqlodbc/.libs/psqlodbcw.so
    QORE_TEST_ODBC_DRIVER=/path/to/psqlodbc/.libs/psqlodbcw.so \
      qore -b --enable-debug -l build-debug/odbc-api-2.0.qmod test/connection-setup.qtest

The port test uses two real loopback endpoints and verifies that the second
receives the startup packet after the first remains silent. It fails with the
unpatched driver. The connection setup suite also exercises the attribute leak
under Valgrind. The Qore regression fixture defaults to the ``PostgreSQL Unicode``
driver name used by CI; set ``QORE_TEST_ODBC_DRIVER`` for another registration.

These patches do not add SQLCancelHandle to psqlODBC. Native login-timeout support
remains necessary to bound an unresponsive peer.
