#!/bin/sh

# The functions that handle credentials do not trace them: they turn tracing off and restore the caller's state.
# ci_source_env() is in ci_env.sh, which the job sources first.

# pg_error_codes <output> [<text to print if the output has no code>]
#
# Prints the exception code of the output of a failed command, and nothing else of it: the description of an exception
# can contain the connection string.  Only the codes that these commands raise are accepted, so that a line of a
# multi-line value that merely looks like a code is not reported.
pg_error_codes() {
    local code
    code=$(printf '%s\n' "$1" | sed -n -E \
        's/^( \* <[a-z-]+>:-?[0-9]+: )?(DBI:PGSQL:(ARRAY-BIND-ERROR|ARRAY-ERROR|BIND-ERROR|CONNECTION-ERROR|COPY-ERROR|ENCODING-ERROR|ERROR|EXEC-EXCEPTION|TIMEZONE-ERROR|TYPE-ERROR|UNKNOWN-CHARACTER-SET)|DATASOURCE-PARSE-ERROR|PARSE-ERROR|PARSE-EXCEPTION|LOAD-MODULE-ERROR): .*/\2/p' | head -n 1)
    printf '%s\n' "${code:-${2:-}}"
}

# Checks that the connection in QORE_DB_CONNSTR_PGSQL works; it is read from the environment, so it is on no command
# line, and a failure reports only its exception code.
pg_check_connection() {
    { local pg_check_flags=$-; set +x; } 2>/dev/null
    local out status=0
    out=$(qore -nX '(new Datasource(ENV.QORE_DB_CONNSTR_PGSQL)).getServerVersion()' 2>&1) || status=$?
    if [ "${status}" != 0 ]; then
        echo "ERROR: cannot connect to the test database: $(pg_error_codes "${out}" "(no error code in the output)")" >&2
    else
        echo "verified the connection to the test database"
    fi
    {
        case $pg_check_flags in
            *x*) set -x ;;
        esac
        return "${status}"
    } 2>/dev/null
}

# Sets PGPASSWORD from POSTGRES_PASSWORD_MAP for RUNNER_HOST.  The map is read from the environment, so it is neither in
# the program text nor on a command line, and a failure reports only its exception code.
pg_read_map_password() {
    { local pg_map_flags=$-; set +x; } 2>/dev/null
    local out status=0
    # the markers separate the value from anything else on the merged output; no temporary file holds the output
    out=$(PG_MAP_HOST="${RUNNER_HOST:=pgsql}" POSTGRES_PASSWORD_MAP="${POSTGRES_PASSWORD_MAP}" qore -l Util -ne \
        'printf("<<PGMAP>>%s<<END>>", parse_to_qore_value(ENV.POSTGRES_PASSWORD_MAP){ENV.PG_MAP_HOST});' 2>&1) || status=$?
    PGPASSWORD=
    if [ "${status}" = 0 ]; then
        case "${out}" in
            *"<<PGMAP>>"*"<<END>>"*)
                PGPASSWORD=${out#*"<<PGMAP>>"}
                PGPASSWORD=${PGPASSWORD%"<<END>>"*}
                ;;
            *)
                status=1
                ;;
        esac
    fi
    if [ "${status}" != 0 ]; then
        echo "ERROR: cannot read the PostgreSQL password map: $(pg_error_codes "${out}" "(no error code in the output)")" >&2
    fi
    {
        case $pg_map_flags in
            *x*) set -x ;;
        esac
        return "${status}"
    } 2>/dev/null
}

setup_postgres_on_host() {
    { local pg_trace_flags=$-; set +x; } 2>/dev/null
    local pg_env="${ENV_FILE:-/tmp/env.sh}"
    local PG_TEST_PASS=qore
    # add env vars to environment file and load it
    # NOTE: must convert to lower case only, or the psql commands below will fail
    user=qore_test_`qore -lUtil -ne 'printf("%s", get_random_string(10));' | tr A-Z a-z`
    ci_env_export "${pg_env}" QORE_DB_USER "${user}"
    ci_env_export "${pg_env}" QORE_DB_PASS "${PG_TEST_PASS}"
    ci_env_export "${pg_env}" QORE_DB_NAME "${user}"
    ci_env_export "${pg_env}" QORE_DB_HOST "${RUNNER_HOST:=pgsql}"
    systemdb="pgsql:${user}/${PG_TEST_PASS}@${user}%${RUNNER_HOST:=pgsql}"
    ci_env_export "${pg_env}" QORE_DB_CONNSTR_PGSQL "${systemdb}"
    ci_env_export "${pg_env}" QORE_DB_CONNSTR "${systemdb}"

    # if we have a password for the local postgres user, then use it and connect to localhost as well
    if [ -n "${POSTGRES_PASSWORD_MAP}" ]; then
        pg_read_map_password
        if [ -n "${PGPASSWORD}" ]; then
            ci_env_export "${pg_env}" PGPASSWORD "${PGPASSWORD}"
            ci_env_export "${pg_env}" PSQL_ARGS "-h ${RUNNER_HOST:=pgsql}"
        fi
    fi

    ci_source_env "${pg_env}"

    # VERBOSITY=sqlstate: an error is reported by its code, without the statement, which holds a password
    # create user for test; CREATEROLE lets SqlUtil's PgsqlBoundedTransaction.qtest create (and drop) the role its
    # row-security case needs
    cat <<EOF | psql -Upostgres -v VERBOSITY=sqlstate ${PSQL_ARGS}
create database ${QORE_DB_NAME} encoding = 'utf8';
\connect ${QORE_DB_NAME};
create extension if not exists vector;
create user ${QORE_DB_USER} password '${PG_TEST_PASS}' createrole;
grant create, connect, temp on database ${QORE_DB_NAME} to ${QORE_DB_USER};
grant select on all tables in schema pg_catalog to ${QORE_DB_USER};
grant all on schema public to ${QORE_DB_USER};
EOF
    echo created pgsql user ${QORE_DB_USER} and db ${QORE_DB_NAME}

    # make sure we can access the DB
    pg_check_connection

    {
        case $pg_trace_flags in
            *x*) set -x ;;
        esac
    } 2>/dev/null

    export POSTGRES_HOST=1
}

# Sets up the test database of the job and QORE_DB_CONNSTR_ODBC, which holds its password, without tracing it
setup_odbc_database() {
    { local odbc_trace_flags=$-; set +x; } 2>/dev/null
    if getent hosts postgres > /dev/null 2>&1; then
        echo "Using postgres service from CI"
        export QORE_DB_USER=postgres
        export QORE_DB_PASS=qore
        export QORE_DB_NAME=postgres
        export QORE_DB_HOST=postgres
        export QORE_DB_CONNSTR_ODBC="odbc:${QORE_DB_USER}/${QORE_DB_PASS}@(UTF8){conn=DRIVER=PostgreSQL Unicode;Server=${QORE_DB_HOST};Database=${QORE_DB_NAME}}"

        # Wait for PostgreSQL to be ready
        printf "waiting on PostgreSQL server: "
        waited=0
        while true; do
            if PGPASSWORD=${QORE_DB_PASS} psql -h ${QORE_DB_HOST} -U ${QORE_DB_USER} -d ${QORE_DB_NAME} -c "SELECT 1" > /dev/null 2>&1; then
                echo "ready"
                break
            fi
            if [ $waited -eq 30 ]; then
                echo && echo "Waited too long for PostgreSQL to start; aborting build."
                exit 1
            fi
            printf .
            sleep 1
            waited=$((waited+1))
        done
    else
        echo "Using shared postgres host"
        setup_postgres_on_host
        export QORE_DB_CONNSTR_ODBC="odbc:${QORE_DB_USER}/${QORE_DB_PASS}@(UTF8){conn=DRIVER=PostgreSQL Unicode;Server=${QORE_DB_HOST};Database=${QORE_DB_NAME}}"
    fi
    {
        case $odbc_trace_flags in
            *x*) set -x ;;
        esac
    } 2>/dev/null
}

cleanup_postgres_on_host() {
    if [ "${POSTGRES_HOST}" = "1" ]; then
        # drop postgresql test user
        cat <<EOF | psql -Upostgres ${PSQL_ARGS}
drop owned by ${QORE_DB_USER};
drop database ${QORE_DB_NAME} with (force);
drop role ${QORE_DB_USER};
EOF
        echo dropped pgsql user ${QORE_DB_USER}
    fi
}

