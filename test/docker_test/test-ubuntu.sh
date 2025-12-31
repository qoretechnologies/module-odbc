#!/bin/bash

set -e
set -x

ENV_FILE=/tmp/env.sh

. ${ENV_FILE}

apt update && apt install odbc-postgresql valgrind -y

# Check if we're running with postgres service in CI (k8s)
if getent hosts postgres > /dev/null 2>&1; then
    echo "Using postgres service from CI"
    export OMQ_DB_USER=postgres
    export OMQ_DB_PASS=omq
    export OMQ_DB_NAME=postgres
    export OMQ_DB_HOST=postgres
    export QORE_DB_CONNSTR_ODBC="odbc:${OMQ_DB_USER}/${OMQ_DB_PASS}@(UTF8){conn=DRIVER=PostgreSQL Unicode;Server=${OMQ_DB_HOST};Database=${OMQ_DB_NAME}}"

    # Wait for PostgreSQL to be ready
    printf "waiting on PostgreSQL server: "
    waited=0
    while true; do
        if PGPASSWORD=${OMQ_DB_PASS} psql -h ${OMQ_DB_HOST} -U ${OMQ_DB_USER} -d ${OMQ_DB_NAME} -c "SELECT 1" > /dev/null 2>&1; then
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
    . test/docker_test/postgres_lib.sh
    setup_postgres_on_host
    export QORE_DB_CONNSTR_ODBC="odbc:${OMQ_DB_USER}/omq@(UTF8){conn=DRIVER=PostgreSQL Unicode;Server=${OMQ_DB_HOST};Database=${OMQ_DB_NAME}}"
fi

# setup MODULE_SRC_DIR env var
cwd=`pwd`
if [ -z "${MODULE_SRC_DIR}" ]; then
    if [ -e "$cwd/src/odbc-module.cpp" ]; then
        MODULE_SRC_DIR=$cwd
    else
        MODULE_SRC_DIR=$WORKDIR/module-odbc
    fi
fi
echo "export MODULE_SRC_DIR=${MODULE_SRC_DIR}" >> ${ENV_FILE}

echo "export QORE_UID=999" >> ${ENV_FILE}
echo "export QORE_GID=999" >> ${ENV_FILE}

. ${ENV_FILE}

export MAKE_JOBS=4

# build module and install
echo && echo "-- building module --"
mkdir -p ${MODULE_SRC_DIR}/build
cd ${MODULE_SRC_DIR}/build
cmake .. -DCMAKE_BUILD_TYPE=debug -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}
make -j${MAKE_JOBS}
make install

# add Qore user and group
groupadd -o -g ${QORE_GID} qore
useradd -o -m -d /home/qore -u ${QORE_UID} -g ${QORE_GID} qore

# own everything by the qore user
chown -R qore:qore ${MODULE_SRC_DIR}

# run the tests
export QORE_MODULE_DIR=${MODULE_SRC_DIR}/qlib:${QORE_MODULE_DIR}
cd ${MODULE_SRC_DIR}
for test in test/*.qtest; do
    gosu qore:qore qore $test -vv
    RESULTS="$RESULTS $?"
done

# Cleanup if using shared host
if ! getent hosts postgres > /dev/null 2>&1; then
    . test/docker_test/postgres_lib.sh
    cleanup_postgres_on_host
fi

# check the results
for R in $RESULTS; do
    if [ "$R" != "0" ]; then
        exit 1 # fail
    fi
done
