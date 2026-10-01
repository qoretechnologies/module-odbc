RPM packaging
=============

``qore-odbc-module.spec`` builds Fedora, Enterprise Linux 10 and openSUSE
packages using the installed Qore 3 SDK and unixODBC. The runtime package
contains the native driver and its metadata; the noarch ``-doc`` package
contains the public API reference and database test examples. Database-specific
ODBC drivers remain a user choice and are installed separately.

Build and database tests
-----------------------

Prepare a pinned source archive with ``qore-packaging/tools/packaging.py`` and
build it with ``qore-packaging/tools/build-local.py``. Tests and documentation
are enabled by default. ``--without tests`` and ``--without docs`` are available
for diagnosis, but release qualification uses both.

The test dependencies are PostgreSQL server, Python 3 and the Unicode
PostgreSQL ODBC driver (``postgresql-odbc`` on Fedora/EL, ``psqlODBC`` on SUSE).
``rpm/test-postgres test`` discovers the installed driver and server tools,
creates a private temporary cluster as the unprivileged build user and runs the
complete suite with Qore debugging enabled. It listens only on a private Unix
socket, uses no network access and changes no system database configuration.
Inherited PostgreSQL connection settings are cleared. A failed startup or test
command still stops and removes the cluster; a failed shutdown preserves the
cluster for diagnosis and makes the test fail. No database credentials are
required. A connection preflight prevents a missing database from being
reported as a successful skipped suite.

Installed qualification
-----------------------

Install the RPM and test dependencies in a minimal target image without the
Qore SDK or compiler. As an unprivileged user, from the extracted source tree::

    QORE_RPM_TEST_TMP=/tmp/odbc-installed rpm/tests-installed-runtime

This copies only test fixtures outside the checkout, clears module overrides,
and runs the same database suite against the installed native driver.
``python3 -B -W error rpm/test_postgres_fixture.py -v`` checks fixture failure
paths; ``python3 -B -W error test/test_docs.py build -v`` checks generated API
pages and the overview's binding-function links.

Other ODBC vendors have distinct type, transaction and connection semantics.
Their integration suites remain required when qualifying those vendors;
PostgreSQL coverage is not a claim of testing every ODBC implementation.
