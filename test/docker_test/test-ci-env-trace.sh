#!/bin/bash
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# ci_source_env() (ci_env.sh) sources a file that holds credentials.  This checks, in bash and in sh, that:
# - the values never reach a shell trace, and neither does the toggling of the trace
# - the caller's tracing state is kept, whether it was on or off, also when the file nests another ci_source_env call
# - the variables are exported to child processes
# - "set -e" stays in effect inside the file, and the status of the file is returned without it
set -u
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
sentinel=FAKE-SENTINEL-NOT-A-CREDENTIAL
work=$(mktemp -d) || exit 1
if [ -z "$work" ] || [ ! -d "$work" ]; then
    echo "FAIL: cannot create a temporary directory"
    exit 1
fi
trap 'rm -rf "$work"' EXIT
fail=0

printf 'export CI_ENV_TEST_VALUE=%s\n' "$sentinel" > "$work/env.sh"
printf '. "%s/ci_env.sh"\nci_source_env "%s/env.sh"\nexport CI_ENV_TEST_NESTED=1\n' "$here" "$work" > "$work/nested.sh"
printf 'false\nexport CI_ENV_TEST_AFTER=1\n' > "$work/fails.sh"
printf 'export CI_ENV_TEST_VALUE=%s\nreturn 7\n' "$sentinel" > "$work/status.sh"
# the expected value is read from a file: on the command line, it would be in the caller's trace
printf '%s\n' "$sentinel" > "$work/expected"

# expect <shell> <name> <expected status, or "nonzero"> <expected output> <script>: the script's stdout is compared,
# its trace is searched
expect() {
    local shell=$1 name="$1: $2" expected_rc=$3 expected_out=$4 script=$5 out rc rc_ok=1
    out=$("$shell" -c ". \"$here/ci_env.sh\"; $script" 2> "$work/trace")
    rc=$?
    if [ "$expected_rc" = nonzero ]; then
        [ "$rc" != 0 ] || rc_ok=
    else
        [ "$rc" = "$expected_rc" ] || rc_ok=
    fi
    if [ -z "$rc_ok" ] || [ "$out" != "$expected_out" ]; then
        echo "FAIL: $name: expected status $expected_rc and output '$expected_out', got $rc and '$out'"
        fail=1
    elif grep -q -F "$sentinel" "$work/trace"; then
        echo "FAIL: $name: the value is in the trace"
        fail=1
    elif grep -q -E 'set [-+]x|ci_source_env_' "$work/trace"; then
        echo "FAIL: $name: the toggling of the trace is in the trace"
        fail=1
    else
        echo "ok: $name"
    fi
}

child='bash -c "[ \"\$CI_ENV_TEST_VALUE\" = \"\$(cat '$work'/expected)\" ] && echo exported"'
flag='case $- in *x*) echo on ;; *) echo off ;; esac'

for shell in bash sh; do
    expect $shell "tracing stays on" 0 on "set -x; ci_source_env '$work/env.sh'; { $flag; } 2>/dev/null"
    expect $shell "tracing stays off" 0 off "ci_source_env '$work/env.sh'; $flag"
    expect $shell "exported to a child with tracing on" 0 exported "set -x; ci_source_env '$work/env.sh'; $child"
    expect $shell "exported to a child with tracing off" 0 exported "ci_source_env '$work/env.sh'; $child"
    expect $shell "a nested call keeps tracing on" 0 on "set -x; ci_source_env '$work/nested.sh'; { $flag; } 2>/dev/null"
    expect $shell "a nested call keeps tracing off" 0 off "ci_source_env '$work/nested.sh'; $flag"
    expect $shell "a nested call exports" 0 exported "set -x; ci_source_env '$work/nested.sh'; $child"
    expect $shell "set -e applies inside the file" 1 "" "set -e; ci_source_env '$work/fails.sh'; echo continued"
    expect $shell "set -e applies to a missing file" nonzero "" \
        "set -e; ci_source_env '$work/missing.sh' 2>/dev/null; echo continued"
    expect $shell "the status is returned without set -e" 0 "7 on" \
        "set -x; ci_source_env '$work/status.sh'; { rc=\$?; echo \"\$rc \$(case \$- in *x*) echo on ;; *) echo off ;; esac)\"; } 2>/dev/null"
done

# setup_postgres_on_host (postgres_lib.sh) under "set -x", with stub tools: for each value, nothing of it reaches the
# output, the trace or a command line, it is neither split nor executed, and it reaches a child process unchanged
mkdir -p "$work/bin" "$work/tmp" "$work/badsed"
cat > "$work/bin/qore" <<'STUB'
#!/bin/sh
case "$*" in
    *FAKE-SENTINEL-NOT-A-CREDENTIAL*)
        : > "${STUB_DIR}/args-leak"
        exit 99 ;;
esac
case "$*" in
    *get_random_string*)
        case "${STUB_FAIL:-}" in
            name) exit 9 ;;
            badname) printf 'x;drop;abc'; exit 0 ;;
            emptyname) exit 0 ;;
        esac
        printf abcdefghij
        exit 0 ;;
    *parse_to_qore_value*)
        if [ "${STUB_FAIL:-}" = map ]; then
            echo "PARSE-ERROR: bad value in ${POSTGRES_PASSWORD_MAP}" >&2
            exit 3
        fi
        printf '<<PGMAP>>%s<<END>>' "${POSTGRES_PASSWORD_MAP#pgsql=}"
        exit 0 ;;
    *getServerVersion*)
        case "${STUB_FAIL:-}" in
            connection)
                echo "DATASOURCE-PARSE-ERROR: missing database name in 'pgsql:u/${POSTGRES_PASSWORD_MAP}'" >&2
                exit 4 ;;
            noisy)
                echo "180000 ${POSTGRES_PASSWORD_MAP}"
                exit 0 ;;
        esac
        echo 180000
        exit 0 ;;
esac
exit 0
STUB
cat > "$work/bin/psql" <<'STUB'
#!/bin/sh
case "$*" in
    *FAKE-SENTINEL-NOT-A-CREDENTIAL*)
        : > "${STUB_DIR}/args-leak"
        exit 99 ;;
    *"select 1"*|*"SELECT 1"*) ;;
    *)
        # a statement with a password must run with VERBOSITY=sqlstate: psql would print it in an error
        sql=$(cat)
        case "$sql" in
            *password*)
                case "$*" in
                    *VERBOSITY=sqlstate*) ;;
                    *)
                        : > "${STUB_DIR}/verbosity-lost"
                        exit 98 ;;
                esac ;;
        esac
        # a model of the server: an object is a file.  As in psql reading a script, a failed statement ends it with
        # status 3 under ON_ERROR_STOP; otherwise psql goes on to the next statement and exits with 0
        server="${STUB_DIR}/server"
        stop=
        case "$*" in
            *ON_ERROR_STOP=1*) stop=1 ;;
        esac
        while IFS= read -r line; do
            failed=
            case "$line" in
                "create database "*)
                    name=${line#create database }
                    name=${name%% *}
                    if [ "${STUB_PSQL_FAIL:-}" = "create database" ] || [ -e "$server/db-$name" ]; then
                        failed=1
                    else
                        : > "$server/db-$name"
                    fi ;;
                "create user "*)
                    name=${line#create user }
                    name=${name%% *}
                    if [ "${STUB_PSQL_FAIL:-}" = "create user" ] || [ -e "$server/role-$name" ]; then
                        failed=1
                    else
                        : > "$server/role-$name"
                    fi ;;
                "grant "*)
                    # the connection to the new database is lost
                    if [ "${STUB_PSQL_FAIL:-}" = grant ]; then
                        exit 2
                    fi ;;
                "drop database "*)
                    if [ -n "${STUB_PSQL_SIGNAL:-}" ]; then
                        kill -TERM "$PPID"
                    fi
                    name=${line#drop database }
                    name=${name%% *}
                    if [ "${STUB_PSQL_FAIL:-}" = "drop database" ] || [ ! -e "$server/db-$name" ]; then
                        failed=1
                    else
                        rm -f "$server/db-$name"
                    fi ;;
                "drop owned by "*)
                    name=${line#drop owned by }
                    name=${name%;}
                    if [ ! -e "$server/role-$name" ]; then
                        failed=1
                    fi ;;
                "drop role "*)
                    name=${line#drop role }
                    name=${name%;}
                    if [ "${STUB_PSQL_FAIL:-}" = "drop role" ] || [ ! -e "$server/role-$name" ]; then
                        failed=1
                    else
                        rm -f "$server/role-$name"
                    fi ;;
            esac
            if [ -n "$failed" ]; then
                echo "ERROR:  42000" >&2
                if [ -n "$stop" ]; then
                    exit 3
                fi
            fi
        done <<SQL
$sql
SQL
        ;;
esac
exit 0
STUB
cat > "$work/bin/getent" <<'STUB'
#!/bin/sh
[ "${STUB_GETENT:-fail}" = ok ]
STUB
printf '#!/bin/sh\nexit 1\n' > "$work/badsed/sed"
chmod +x "$work/bin/getent"
chmod +x "$work/bin/qore" "$work/bin/psql" "$work/badsed/sed"

# reset_server [<object>...]: the model of the server holds the objects of its administrator and the given ones
reset_server() {
    local object
    rm -rf "$work/server"
    mkdir "$work/server"
    for object in db-postgres role-postgres "$@"; do
        : > "$work/server/$object"
    done
}

# run_lib <shell> <stub failure> <value> <script>: runs the script with the helpers loaded and the stubs first in PATH
run_lib() {
    if [ -z "${STUB_SERVER_SET:-}" ]; then
        reset_server
    fi
    (cd "$work" && PATH="$work/bin:$PATH" STUB_DIR="$work" POSTGRES_PASSWORD_MAP="pgsql=$3" STUB_FAIL="$2" STUB_GETENT="${STUB_GETENT:-fail}" \
        RUNNER_HOST=pgsql ENV_FILE="$work/setup-env.sh" TMPDIR="$work/tmp" "$1" -c "
        . \"$here/ci_env.sh\"
        . \"$here/postgres_lib.sh\"
        $4" 2>&1)
}

# check_output <name> <status> <expected status> <expected output> <output>
check_output() {
    if [ "$2" != "$3" ]; then
        echo "FAIL: $1: expected status $3, got $2"
        fail=1
    elif [ -e "$work/args-leak" ]; then
        echo "FAIL: $1: the value is on a command line"
        fail=1
    elif [ -e "$work/verbosity-lost" ]; then
        echo "FAIL: $1: a statement with a password ran without VERBOSITY=sqlstate"
        fail=1
    elif [ -e "$work/executed" ]; then
        echo "FAIL: $1: the value was executed"
        fail=1
    elif [ -n "$(ls -A "$work/tmp")" ]; then
        echo "FAIL: $1: a temporary file was left behind"
        fail=1
    elif printf '%s' "$5" | grep -q -F "$sentinel"; then
        echo "FAIL: $1: the value is in the output or the trace"
        fail=1
    elif ! printf '%s\n' "$5" | grep -q -F -- "$4"; then
        echo "FAIL: $1: the expected result is not in the output"
        fail=1
    else
        echo "ok: $1"
    fi
}

# check_setup <shell> <name> <expected status> <expected output> <value> [<stub failure>]
check_setup() {
    local out rc
    : > "$work/setup-env.sh"
    rm -f "$work/executed" "$work/args-leak" "$work/verbosity-lost"
    printf '%s' "$5" > "$work/setup-expected"
    out=$(run_lib "$1" "${6:-}" "$5" "
        set -e
        set -x
        setup_postgres_on_host
        { case \$- in *x*) echo tracing-on ;; *) echo tracing-off ;; esac; } 2>/dev/null
        bash -c '[ \"\$PGPASSWORD\" = \"\$(cat setup-expected)\" ] && echo exported'
        ")
    rc=$?
    check_output "$1: setup: $2" "$rc" "$3" "$4" "$out"
}

# check_direct <shell> <function> <stub failure> <expected status> <tracing: on or off>: the function keeps the value
# out of a trace and restores the caller's tracing state, on success and on failure
check_direct() {
    local out rc start=
    if [ "$5" = on ]; then
        start="set -x"
    fi
    rm -f "$work/executed" "$work/args-leak" "$work/verbosity-lost"
    out=$(run_lib "$1" "$3" "$sentinel" "
        $start
        $2
        rc=\$?
        { case \$- in *x*) t=on ;; *) t=off ;; esac; echo \"result=\$rc tracing-\$t\"; } 2>/dev/null
        ")
    rc=$?
    check_output "$1: $2: failure mode '${3:-none}', tracing $5" "$rc" 0 "result=$4 tracing-$5" "$out"
}

# check_odbc <shell> <ok or fail: whether the name of the service resolves>: both paths of the database setup
check_odbc() {
    local out rc
    : > "$work/setup-env.sh"
    rm -f "$work/executed" "$work/args-leak" "$work/verbosity-lost"
    out=$(STUB_GETENT="$2" run_lib "$1" "" "$sentinel" "
        set -e
        set -x
        setup_odbc_database
        { case \$- in *x*) echo tracing-on ;; *) echo tracing-off ;; esac; } 2>/dev/null
        bash -c '[ -n \"\$QORE_DB_CONNSTR_ODBC\" ] && echo exported'
        ")
    rc=$?
    check_output "$1: odbc setup, service name $2" "$rc" 0 tracing-on "$out"
    if printf '%s\n' "$out" | grep -q -E 'QORE_DB_PASS=|PGPASSWORD=|QORE_DB_CONNSTR_ODBC='; then
        echo "FAIL: $1: odbc setup, service name $2: a credential assignment is in the trace"
        fail=1
    elif ! printf '%s\n' "$out" | grep -q exported; then
        echo "FAIL: $1: odbc setup, service name $2: the connection string is not exported"
        fail=1
    else
        echo "ok: $1: odbc setup, service name $2: no assignment traced, connection string exported"
    fi
}

# check_code <shell> <expected> <output>: only a code that the commands can raise is reported
check_code() {
    local got
    got=$(CI_CODE_INPUT="$3" "$1" -c ". \"$here/postgres_lib.sh\"; pg_error_codes \"\$CI_CODE_INPUT\" none" 2>&1)
    if [ "$got" != "$2" ]; then
        echo "FAIL: $1: error code: expected '$2', got another result"
        fail=1
    else
        echo "ok: $1: error code -> $2"
    fi
}

# check_codes_guard <shell> <tracing: on or off>: pg_error_codes does not trace its work and keeps the caller's state
check_codes_guard() {
    local out start=
    if [ "$2" = on ]; then
        start="set -x"
    fi
    out=$("$1" -c ". \"$here/postgres_lib.sh\"
        $start
        pg_error_codes 'PARSE-ERROR: text' none
        { case \$- in *x*) echo tracing-on ;; *) echo tracing-off ;; esac; } 2>/dev/null" 2>&1)
    if printf '%s\n' "$out" | grep -q -E '^\++ *(local code|code=|sed |printf |head )'; then
        echo "FAIL: $1: pg_error_codes is traced"
        fail=1
    elif ! printf '%s\n' "$out" | grep -q "tracing-$2"; then
        echo "FAIL: $1: pg_error_codes did not keep tracing $2"
        fail=1
    elif ! printf '%s\n' "$out" | grep -q '^PARSE-ERROR$'; then
        echo "FAIL: $1: pg_error_codes did not report the code"
        fail=1
    else
        echo "ok: $1: pg_error_codes is not traced, tracing stays $2"
    fi
}

# check_owned <shell> <name> <expected status, or "nonzero"> <objects left on the server> <job>: the exit handler drops
# only what this job created and recorded, keeps the status of the job, and traces no credential.  OWNED_PRE lists the
# objects that exist before the job, and OWNED_FAIL is the failure of the stub tools.
check_owned() {
    local out rc expected=$3 left
    : > "$work/setup-env.sh"
    rm -f "$work/executed" "$work/args-leak" "$work/verbosity-lost" "$work/after-failure"
    # shellcheck disable=SC2086
    reset_server ${OWNED_PRE:-}
    out=$(STUB_SERVER_SET=1 run_lib "$1" "${OWNED_FAIL:-}" "$sentinel" "
        odbc_cleanup_install
        set -e
        set -x
        echo job-started
        $5
        ")
    rc=$?
    if [ "$expected" = nonzero ] && [ "$rc" != 0 ]; then
        expected=$rc
    fi
    check_output "$1: owned: $2" "$rc" "$expected" job-started "$out"
    left=$(cd "$work/server" && ls | sort | tr '\n' ' ')
    if [ -e "$work/after-failure" ]; then
        echo "FAIL: $1: owned: $2: the job continued after a failure"
        fail=1
    elif [ "$left" != "$4 " ]; then
        echo "FAIL: $1: owned: $2: the server holds '$left' instead of '$4 '"
        fail=1
    else
        echo "ok: $1: owned: $2: the server holds the expected objects"
    fi
}

newline_value="first line
$sentinel
last line"

for shell in bash sh; do
    check_setup $shell "a plain value" 0 exported "$sentinel"
    check_setup $shell "tracing is restored" 0 tracing-on "$sentinel"
    check_setup $shell "white space" 0 exported "a  b $sentinel c"
    check_setup $shell "single quotes" 0 exported "it's '$sentinel'"
    check_setup $shell "double quotes" 0 exported "\"$sentinel\""
    check_setup $shell "a command substitution" 0 exported "\$(touch executed)$sentinel"
    check_setup $shell "backticks" 0 exported "\`touch executed\`$sentinel"
    check_setup $shell "a semicolon" 0 exported "x;touch executed;$sentinel"
    check_setup $shell "backslashes" 0 exported "a\\b\\n\\\\$sentinel\\"
    check_setup $shell "newlines" 0 exported "$newline_value"
    check_setup $shell "a failed map keeps its code only" 3 PARSE-ERROR "$sentinel" map
    check_setup $shell "a failed connection keeps its code only" 4 DATASOURCE-PARSE-ERROR "$sentinel" connection

    for trace in on off; do
        check_direct $shell pg_check_connection noisy 0 $trace
        check_direct $shell pg_check_connection connection 4 $trace
        check_direct $shell pg_read_map_password "" 0 $trace
        check_direct $shell pg_read_map_password map 3 $trace
    done

    check_codes_guard $shell on
    check_codes_guard $shell off
    check_odbc $shell ok
    check_odbc $shell fail
    n=qore_test_abcdefghij
    admin="db-postgres role-postgres"
    job='setup_odbc_database; (exit 7); touch after-failure'
    check_owned $shell "a failed job" 7 "$admin" "$job"
    check_owned $shell "a job that passes" 0 "$admin" setup_odbc_database
    POSTGRES_HOST=1 QORE_DB_USER=postgres QORE_DB_NAME=postgres STUB_GETENT=ok \
        check_owned $shell "an inherited host flag on the service path" 7 "$admin" "$job"
    POSTGRES_HOST=1 QORE_DB_USER=postgres QORE_DB_NAME=postgres \
        check_owned $shell "an inherited host flag without a setup" 7 "$admin" '(exit 7); touch after-failure'
    QORE_DB_USER=inherited QORE_DB_NAME=inherited OWNED_PRE="db-inherited role-inherited" OWNED_FAIL=name \
        check_owned $shell "inherited names" 1 "db-inherited db-postgres role-inherited role-postgres" \
        'setup_odbc_database; touch after-failure'
    odbc_owned_db=postgres odbc_owned_role=postgres \
        check_owned $shell "an inherited ownership state" 7 "$admin" '(exit 7); touch after-failure'
    odbc_owned_db=qore_test_zzzzzzzzzz odbc_owned_role=qore_test_zzzzzzzzzz \
        OWNED_PRE="db-qore_test_zzzzzzzzzz role-qore_test_zzzzzzzzzz" \
        check_owned $shell "an inherited ownership state with a valid name" 7 \
        "db-postgres db-qore_test_zzzzzzzzzz role-postgres role-qore_test_zzzzzzzzzz" '(exit 7); touch after-failure'
    check_owned $shell "ownership in the environment file before the setup" 7 "$admin" \
        'printf "export odbc_owned_db=postgres\nexport odbc_owned_role=postgres\n" > "$ENV_FILE"; '"$job"
    check_owned $shell "ownership in the environment file after the setup" nonzero "$admin" \
        'setup_odbc_database; printf "export odbc_owned_db=postgres\n" >> "$ENV_FILE"; ci_source_env "$ENV_FILE"; touch after-failure'
    OWNED_PRE="db-$n" check_owned $shell "a database of the same name survives" 3 "db-postgres db-$n role-postgres" \
        'setup_odbc_database; touch after-failure'
    OWNED_PRE="role-$n" check_owned $shell "a role of the same name survives" 3 "db-postgres role-postgres role-$n" \
        'setup_odbc_database; touch after-failure'
    OWNED_PRE="db-$n role-$n" check_owned $shell "a database and a role of the same name survive" 3 \
        "db-postgres db-$n role-postgres role-$n" 'setup_odbc_database; touch after-failure'
    OWNED_FAIL=name check_owned $shell "name generation fails" 1 "$admin" 'setup_odbc_database; touch after-failure'
    OWNED_FAIL=badname check_owned $shell "a generated name of the wrong shape" 1 "$admin" \
        'setup_odbc_database; touch after-failure'
    OWNED_FAIL=emptyname check_owned $shell "an empty generated name" 1 "$admin" \
        'setup_odbc_database; touch after-failure'
    OWNED_PRE="db-qore_test_zzzzzzzzzz role-qore_test_zzzzzzzzzz" STUB_PSQL_FAIL="create database" \
        check_owned $shell "a valid name in the environment file is not owned" 3 \
        "db-postgres db-qore_test_zzzzzzzzzz role-postgres role-qore_test_zzzzzzzzzz" \
        'printf "export odbc_owned_db=qore_test_zzzzzzzzzz\nexport odbc_owned_role=qore_test_zzzzzzzzzz\n" > "$ENV_FILE"; setup_odbc_database; touch after-failure'
    check_owned $shell "a recorded name of the wrong shape is not dropped" 7 "$admin" \
        'odbc_owned_db=postgres; odbc_owned_role=postgres; (exit 7); touch after-failure'
    check_owned $shell "a recorded name of the wrong shape fails a passing job" 1 "$admin" \
        'odbc_owned_db=postgres; odbc_owned_role=postgres'
    STUB_PSQL_FAIL="create database" check_owned $shell "the database is not created" 3 "$admin" \
        'setup_odbc_database; touch after-failure'
    STUB_PSQL_FAIL="create user" check_owned $shell "the role is not created" 3 "$admin" \
        'setup_odbc_database; touch after-failure'
    STUB_PSQL_FAIL=grant check_owned $shell "the grants are lost" 2 "$admin" 'setup_odbc_database; touch after-failure'
    OWNED_FAIL=connection check_owned $shell "the connection check fails" 4 "$admin" \
        'setup_odbc_database; touch after-failure'
    STUB_PSQL_FAIL="drop database" check_owned $shell "a failed drop of the database fails the job" 3 \
        "db-postgres db-$n role-postgres" setup_odbc_database
    STUB_PSQL_FAIL="drop role" check_owned $shell "a failed drop of the role fails the job" 3 \
        "db-postgres role-postgres role-$n" setup_odbc_database
    STUB_PSQL_FAIL="drop role" check_owned $shell "a failed job keeps its status over a failed drop" 7 \
        "db-postgres role-postgres role-$n" "$job"
    check_owned $shell "TERM stops the job and cleans up" 143 "$admin" \
        'setup_odbc_database; kill -TERM $$; touch after-failure'
    STUB_PSQL_SIGNAL=1 check_owned $shell "TERM during the cleanup does not stop it" 0 "$admin" setup_odbc_database
    check_code $shell DATASOURCE-PARSE-ERROR "x
DATASOURCE-PARSE-ERROR: missing database name in 'odbc:u/$sentinel'"
    check_code $shell DBI:PGSQL:ERROR "DBI:PGSQL:ERROR: $sentinel@db: failed"
    check_code $shell none "first line
AB-CD: $sentinel"
    check_code $shell none "odbc:u/$sentinel@db"

    # a value that cannot be quoted is not written, whether or not the caller uses "set -e"
    : > "$work/sed-env.sh"
    out=$(PATH="$work/badsed:$PATH" "$shell" -c ". \"$here/ci_env.sh\"
        ci_env_export \"$work/sed-env.sh\" CI_ENV_TEST_VALUE \"\$CI_ENV_TEST_INPUT\"
        echo \"status=\$?\"" 2>&1 < /dev/null)
    if ! printf '%s\n' "$out" | grep -q '^status=1$' || [ -s "$work/sed-env.sh" ]; then
        echo "FAIL: $shell: a failed quote must fail and write nothing"
        fail=1
    else
        echo "ok: $shell: a failed quote fails and writes nothing"
    fi
done
exit $fail
