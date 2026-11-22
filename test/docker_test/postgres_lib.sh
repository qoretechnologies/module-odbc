#!/bin/sh

# The functions that handle credentials do not trace them: they turn tracing off and restore the caller's state.
# ci_source_env() is in ci_env.sh, which the job sources first.

# The exit and signal handlers are armed only while the ownership state is known to be this job's own.  The limit of this
# rule: the environment file runs as shell code with the administrator's credentials, so one that redefines these
# functions is not defended against.  These flags are set by this job, so a value from the environment does not count.
command unset -v odbc_cleanup_installed odbc_setup_started 2>/dev/null || :

# pg_error_codes <output> [<text to print if the output has no code>]
#
# Prints the exception code of the output of a failed command, and nothing else of it: the description of an exception
# can contain the connection string.  Only the codes that these commands raise are accepted, so that a line of a
# multi-line value that merely looks like a code is not reported.
pg_error_codes() {
    { local pg_codes_flags=$-; set +x; } 2>/dev/null
    local code
    code=$(printf '%s\n' "$1" | sed -n -E \
        's/^( \* <[a-z-]+>:-?[0-9]+: )?(DBI:PGSQL:(ARRAY-BIND-ERROR|ARRAY-ERROR|BIND-ERROR|CONNECTION-ERROR|COPY-ERROR|ENCODING-ERROR|ERROR|EXEC-EXCEPTION|TIMEZONE-ERROR|TYPE-ERROR|UNKNOWN-CHARACTER-SET)|DATASOURCE-PARSE-ERROR|PARSE-ERROR|PARSE-EXCEPTION|LOAD-MODULE-ERROR): .*/\2/p' | head -n 1)
    printf '%s\n' "${code:-${2:-}}"
    {
        case $pg_codes_flags in
            *x*) set -x ;;
        esac
    } 2>/dev/null
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

# pg_valid_test_name <name>: true only for a name of the shape that this job generates
pg_valid_test_name() {
    case "$1" in
        qore_test_??????????) ;;
        *) return 1 ;;
    esac
    case "${1#qore_test_}" in
        *[!abcdefghijklmnopqrstuvwxyz0123456789]*) return 1 ;;
    esac
}

# pg_valid_target_args: true only if the recorded PSQL_ARGS is empty or names a host, so that it cannot add a command, a
# file or an option to the statements that create and drop the test objects.  It takes no argument, and the shells do
# not expand the word of a "case" in a trace (bash prints it unexpanded), so the recorded host is not in the trace.
pg_valid_target_args() {
    case "${odbc_target_args:-}" in
        "") return 0 ;;
        "-h "*) ;;
        *) return 1 ;;
    esac
    case "${odbc_target_args#-h }" in
        ""|-*|*[!ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.:_-]*) return 1 ;;
    esac
}

# Clears the job's record of what it created.  If a value cannot be cleared, such as a read-only one, it is not trusted:
# the exit and signal handlers are removed, so that they drop nothing, and this fails.  "command" bypasses a function
# named "unset" or "trap", unless "command" itself is redefined, and keeps a read-only error from ending a POSIX shell.
pg_clear_ownership() {
    command unset -v odbc_owned_db odbc_owned_role odbc_target_args odbc_target_host odbc_target_hostaddr \
        odbc_target_port odbc_target_service odbc_target_servicefile odbc_target_sysconfdir \
        odbc_target_session_attrs odbc_target_load_balance 2>/dev/null || :
    if [ -n "${odbc_owned_db+x}${odbc_owned_role+x}${odbc_target_args+x}${odbc_target_host+x}${odbc_target_hostaddr+x}" ] \
        || [ -n "${odbc_target_port+x}${odbc_target_service+x}${odbc_target_servicefile+x}${odbc_target_sysconfdir+x}" ] \
        || [ -n "${odbc_target_session_attrs+x}${odbc_target_load_balance+x}" ]; then
        command trap - EXIT INT TERM
        echo "ERROR: cannot clear the record of the test database and role; nothing will be dropped" >&2
        return 1
    fi
}

# Prints a new name for the test database and role; fails if it cannot generate one of the exact shape.
# NOTE: must convert to lower case only, or the psql commands will fail
pg_new_test_name() {
    local suffix name
    suffix=$(qore -lUtil -ne 'printf("%s", get_random_string(10));') || return 1
    name=qore_test_$(printf '%s' "${suffix}" | tr A-Z a-z)
    pg_valid_test_name "${name}" || return 1
    printf '%s\n' "${name}"
}

# Records where this job connects, so that the cleanup drops its objects on the server that it created them on, whatever
# the job changes later in PSQL_ARGS or in the libpq variables that select the server: PGHOST, PGHOSTADDR, PGPORT,
# PGSERVICE, PGSERVICEFILE, PGSYSCONFDIR, PGTARGETSESSIONATTRS and PGLOADBALANCEHOSTS.  PGDATABASE and PGUSER are
# overridden on the command line.  A service without PGSERVICEFILE is still looked up through HOME.
pg_record_target() {
    odbc_target_args=${PSQL_ARGS:-}
    odbc_target_host=${PGHOST:-}
    odbc_target_hostaddr=${PGHOSTADDR:-}
    odbc_target_port=${PGPORT:-}
    odbc_target_service=${PGSERVICE:-}
    odbc_target_servicefile=${PGSERVICEFILE:-}
    odbc_target_sysconfdir=${PGSYSCONFDIR:-}
    odbc_target_session_attrs=${PGTARGETSESSIONATTRS:-}
    odbc_target_load_balance=${PGLOADBALANCEHOSTS:-}
    readonly odbc_target_args odbc_target_host odbc_target_hostaddr odbc_target_port odbc_target_service \
        odbc_target_servicefile odbc_target_sysconfdir odbc_target_session_attrs odbc_target_load_balance
}

# pg_target_psql <database> <psql option>...: runs psql as the administrator on the recorded target, in a subshell, with
# only the recorded connection settings; startup files are ignored
pg_target_psql() (
    { set +x; } 2>/dev/null
    pg_target_db=$1
    shift
    command unset -v PGHOST PGHOSTADDR PGPORT PGSERVICE PGSERVICEFILE PGSYSCONFDIR PGTARGETSESSIONATTRS PGLOADBALANCEHOSTS 2>/dev/null || :
    # a read-only setting cannot be replaced, so psql would connect with it
    if [ -n "${PGHOST+x}${PGHOSTADDR+x}${PGPORT+x}${PGSERVICE+x}${PGSERVICEFILE+x}${PGSYSCONFDIR+x}" ] \
        || [ -n "${PGTARGETSESSIONATTRS+x}${PGLOADBALANCEHOSTS+x}" ]; then
        echo "ERROR: a read-only PostgreSQL connection setting cannot be replaced; not connecting" >&2
        exit 1
    fi
    if [ -n "${odbc_target_host:-}" ]; then
        export PGHOST="${odbc_target_host}" || exit 1
    fi
    if [ -n "${odbc_target_hostaddr:-}" ]; then
        export PGHOSTADDR="${odbc_target_hostaddr}" || exit 1
    fi
    if [ -n "${odbc_target_port:-}" ]; then
        export PGPORT="${odbc_target_port}" || exit 1
    fi
    if [ -n "${odbc_target_service:-}" ]; then
        export PGSERVICE="${odbc_target_service}" || exit 1
    fi
    if [ -n "${odbc_target_servicefile:-}" ]; then
        export PGSERVICEFILE="${odbc_target_servicefile}" || exit 1
    fi
    if [ -n "${odbc_target_sysconfdir:-}" ]; then
        export PGSYSCONFDIR="${odbc_target_sysconfdir}" || exit 1
    fi
    if [ -n "${odbc_target_session_attrs:-}" ]; then
        export PGTARGETSESSIONATTRS="${odbc_target_session_attrs}" || exit 1
    fi
    if [ -n "${odbc_target_load_balance:-}" ]; then
        export PGLOADBALANCEHOSTS="${odbc_target_load_balance}" || exit 1
    fi
    # the recorded arguments are empty or "-h <host>"; the host is passed as one word, whatever IFS is
    if [ -n "${odbc_target_args:-}" ]; then
        exec psql -X -Upostgres -d "${pg_target_db}" -h "${odbc_target_args#-h }" "$@"
    fi
    exec psql -X -Upostgres -d "${pg_target_db}" "$@"
)

# pg_create_test_objects <name>: creates the database, then the role, and records each only after its own create
# succeeded, so that the exit handler does not drop an object that this job's create did not make.
# VERBOSITY=sqlstate: an error is reported by its code, without the statement, which holds a password
pg_create_test_objects() {
    # the name was read again after the environment file was sourced, and it goes into SQL run as the administrator
    pg_valid_test_name "$1" || return 1
    if ! pg_valid_target_args; then
        echo "ERROR: PSQL_ARGS may only name a host" >&2
        return 1
    fi
    cat <<EOF | pg_target_psql postgres -v ON_ERROR_STOP=1 -v VERBOSITY=sqlstate || return
create database $1 encoding = 'utf8';
EOF
    odbc_owned_db=$1
    # CREATEROLE lets SqlUtil's PgsqlBoundedTransaction.qtest create (and drop) the role its row-security case needs
    cat <<EOF | pg_target_psql postgres -v ON_ERROR_STOP=1 -v VERBOSITY=sqlstate || return
create user $1 password '${PG_TEST_PASS}' createrole;
EOF
    odbc_owned_role=$1
    # only the vector extension is optional on a shared host, so only it runs without ON_ERROR_STOP
    cat <<EOF | pg_target_psql "$1" -v VERBOSITY=sqlstate || return
create extension if not exists vector;
EOF
    cat <<EOF | pg_target_psql "$1" -v ON_ERROR_STOP=1 -v VERBOSITY=sqlstate || return
grant create, connect, temp on database $1 to $1;
grant select on all tables in schema pg_catalog to $1;
grant all on schema public to $1;
EOF
}

# pg_write_env <file> <name>: writes the settings of the test database to the environment file; returns the first failure
pg_write_env() {
    ci_env_export "$1" QORE_DB_USER "$2" || return
    ci_env_export "$1" QORE_DB_PASS "${PG_TEST_PASS}" || return
    ci_env_export "$1" QORE_DB_NAME "$2" || return
    ci_env_export "$1" QORE_DB_HOST "${RUNNER_HOST:=pgsql}" || return
    systemdb="pgsql:$2/${PG_TEST_PASS}@$2%${RUNNER_HOST:=pgsql}"
    ci_env_export "$1" QORE_DB_CONNSTR_PGSQL "${systemdb}" || return
    ci_env_export "$1" QORE_DB_CONNSTR "${systemdb}" || return

    # if we have a password for the local postgres user, then use it and connect to localhost as well
    if [ -n "${POSTGRES_PASSWORD_MAP}" ]; then
        pg_read_map_password || return
        if [ -n "${PGPASSWORD}" ]; then
            ci_env_export "$1" PGPASSWORD "${PGPASSWORD}" || return
            ci_env_export "$1" PSQL_ARGS "-h ${RUNNER_HOST:=pgsql}" || return
        fi
    fi
}

setup_postgres_on_host() {
    { local pg_trace_flags=$-; set +x; } 2>/dev/null
    local pg_env="${ENV_FILE:-/tmp/env.sh}"
    local PG_TEST_PASS=qore
    local user pg_setup_status=0
    # add env vars to environment file and load it
    if ! user=$(pg_new_test_name); then
        echo "ERROR: cannot generate the name of the test database" >&2
        {
            case $pg_trace_flags in
                *x*) set -x ;;
            esac
            return 1
        } 2>/dev/null
    fi
    pg_write_env "${pg_env}" "${user}" || pg_setup_status=$?
    if [ "${pg_setup_status}" != 0 ]; then
        {
            case $pg_trace_flags in
                *x*) set -x ;;
            esac
            return "${pg_setup_status}"
        } 2>/dev/null
    fi

    # the environment file can set any variable: the flag is read-only first, and the handlers are disarmed until the
    # record is clear, as nothing is owned yet
    readonly odbc_cleanup_installed
    if [ -n "${odbc_cleanup_installed:-}" ]; then
        command trap - EXIT INT TERM
    fi
    # not in an AND-OR list, so that "set -e" stays in effect inside the file
    ci_source_env "${pg_env}"
    pg_setup_status=$?
    if [ "${pg_setup_status}" != 0 ]; then
        {
            case $pg_trace_flags in
                *x*) set -x ;;
            esac
            return "${pg_setup_status}"
        } 2>/dev/null
    fi

    if ! pg_clear_ownership; then
        {
            case $pg_trace_flags in
                *x*) set -x ;;
            esac
            return 1
        } 2>/dev/null
    fi
    pg_record_target
    if [ -n "${odbc_cleanup_installed:-}" ]; then
        odbc_cleanup_arm
    fi
    pg_create_test_objects "${user}" || pg_setup_status=$?
    if [ "${pg_setup_status}" != 0 ]; then
        echo "ERROR: cannot create the test database and role (status ${pg_setup_status})" >&2
        {
            case $pg_trace_flags in
                *x*) set -x ;;
            esac
            return "${pg_setup_status}"
        } 2>/dev/null
    fi
    echo created pgsql user ${user} and db ${user}

    # make sure we can access the DB
    pg_check_connection || pg_setup_status=$?

    {
        case $pg_trace_flags in
            *x*) set -x ;;
        esac
        return "${pg_setup_status}"
    } 2>/dev/null
}

# Sets up the test database of the job and QORE_DB_CONNSTR_ODBC, which holds its password, without tracing it
setup_odbc_database() {
    { local odbc_trace_flags=$-; set +x; } 2>/dev/null
    local odbc_setup_status=0
    # a second setup would clear the record of the first and leave its objects behind
    if [ -n "${odbc_setup_started:-}" ]; then
        echo "ERROR: the test database of this job is already set up" >&2
        {
            case $odbc_trace_flags in
                *x*) set -x ;;
            esac
            return 1
        } 2>/dev/null
    fi
    odbc_setup_started=1
    readonly odbc_setup_started
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
            if PGPASSWORD=${QORE_DB_PASS} psql -X -h ${QORE_DB_HOST} -U ${QORE_DB_USER} -d ${QORE_DB_NAME} -c "SELECT 1" > /dev/null 2>&1; then
                echo "ready"
                break
            fi
            if [ $waited -eq 30 ]; then
                echo && echo "Waited too long for PostgreSQL to start; aborting build."
                # this failure ends the job instead of being returned: nothing is owned on this path
                exit 1
            fi
            printf .
            sleep 1
            waited=$((waited+1))
        done
    else
        echo "Using shared postgres host"
        setup_postgres_on_host
        odbc_setup_status=$?
        if [ "${odbc_setup_status}" != 0 ]; then
            # a partial record stays, so that the handlers drop what this job created; after a failure in the
            # environment file the handlers are disarmed and drop nothing
            readonly odbc_owned_db odbc_owned_role
            {
                case $odbc_trace_flags in
                    *x*) set -x ;;
                esac
                return "${odbc_setup_status}"
            } 2>/dev/null
        fi
        export QORE_DB_CONNSTR_ODBC="odbc:${QORE_DB_USER}/${QORE_DB_PASS}@(UTF8){conn=DRIVER=PostgreSQL Unicode;Server=${QORE_DB_HOST};Database=${QORE_DB_NAME}}"
    fi
    # read-only: an assignment in a file sourced later does not change what the exit handler drops
    readonly odbc_owned_db odbc_owned_role
    {
        case $odbc_trace_flags in
            *x*) set -x ;;
        esac
    } 2>/dev/null
}

# Drops only what this job recorded: the database first, then the role.  Each name must have the generated shape, so
# a name such as the administrator's is refused; QORE_DB_* is not read.  Returns the first failure.
cleanup_postgres_on_host() {
    local db_status=0 role_status=0
    if [ -n "${odbc_owned_db:-}" ]; then
        # the recorded arguments matter only when there is something to drop
        if pg_valid_target_args && pg_valid_test_name "${odbc_owned_db}"; then
            cat <<EOF | pg_target_psql postgres -v ON_ERROR_STOP=1 -v VERBOSITY=sqlstate || db_status=$?
drop database ${odbc_owned_db} with (force);
EOF
            if [ "${db_status}" = 0 ]; then
                echo dropped pgsql db ${odbc_owned_db}
            fi
        else
            echo "ERROR: not dropping the database: unexpected name or recorded PSQL_ARGS" >&2
            db_status=1
        fi
    fi
    if [ -n "${odbc_owned_role:-}" ]; then
        if pg_valid_target_args && pg_valid_test_name "${odbc_owned_role}"; then
            cat <<EOF | pg_target_psql postgres -v ON_ERROR_STOP=1 -v VERBOSITY=sqlstate || role_status=$?
drop owned by ${odbc_owned_role};
drop role ${odbc_owned_role};
EOF
            if [ "${role_status}" = 0 ]; then
                echo dropped pgsql user ${odbc_owned_role}
            fi
        else
            echo "ERROR: not dropping the role: unexpected name or recorded PSQL_ARGS" >&2
            role_status=1
        fi
    fi
    if [ "${db_status}" != 0 ]; then
        return "${db_status}"
    fi
    return "${role_status}"
}

# Clears the job's ownership state, including any value from the environment, and installs the exit handler; fails
# without installing it if the state cannot be cleared.  The state is not exported.  INT and TERM end the job through
# the handler; SIGKILL cannot be handled.
odbc_cleanup_install() {
    pg_clear_ownership || return 1
    odbc_cleanup_installed=1
    odbc_cleanup_arm
}

odbc_cleanup_arm() {
    command trap odbc_cleanup_on_exit EXIT
    command trap 'odbc_cleanup_finish 130' INT
    command trap 'odbc_cleanup_finish 143' TERM
}

# The exit handler of the job.
odbc_cleanup_on_exit() {
    odbc_cleanup_finish "$?"
}

# odbc_cleanup_finish <status>: drops what the job recorded on a shared host and exits with the status, or with that of
# the cleanup if the status is 0.  INT and TERM also end the job here.  One that arrives before they are ignored below
# runs this function itself: the cleanup still runs once, and the job exits with the status of the signal, which is a
# failure, instead of its own.  SIGKILL cannot be handled.
odbc_cleanup_finish() {
    local job_status=$1 cleanup_status=0
    command trap '' INT TERM
    command trap - EXIT
    set +e
    cleanup_postgres_on_host
    cleanup_status=$?
    if [ "${job_status}" != 0 ]; then
        exit "${job_status}"
    fi
    exit "${cleanup_status}"
}
