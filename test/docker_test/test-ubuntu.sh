#!/bin/bash

set -e
set -x

ENV_FILE=/tmp/env.sh

# the environment file holds credentials: see ci_env.sh
. "$(dirname "${BASH_SOURCE[0]}")/ci_env.sh"
# the helper keeps credentials out of the trace: a failure here fails the job before it is relied on
"$(dirname "${BASH_SOURCE[0]}")/test-ci-env-trace.sh"

ci_source_env "${ENV_FILE}"

apt update && apt install odbc-postgresql valgrind -y

# the test database
. "$(dirname "${BASH_SOURCE[0]}")/postgres_lib.sh"
# a failed test should not leave what this job created on a shared host
odbc_cleanup_install
setup_odbc_database

# setup MODULE_SRC_DIR env var
cwd=`pwd`
if [ -z "${MODULE_SRC_DIR}" ]; then
    if [ -e "$cwd/src/odbc-module.cpp" ]; then
        MODULE_SRC_DIR=$cwd
    else
        MODULE_SRC_DIR=$WORKDIR/module-odbc
    fi
fi
ci_env_export "${ENV_FILE}" MODULE_SRC_DIR "${MODULE_SRC_DIR}"

echo "export QORE_UID=999" >> ${ENV_FILE}
echo "export QORE_GID=999" >> ${ENV_FILE}

ci_source_env "${ENV_FILE}"

export MAKE_JOBS=4

# the tests must load the driver of this tree: reject relative module paths in them, and require the build directory
# in the module path of every test that loads the driver
${MODULE_SRC_DIR}/test/docker_test/check-module-paths.sh odbc

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
export QORE_MODULE_DIR=${MODULE_SRC_DIR}/qlib:${MODULE_SRC_DIR}/build:${QORE_MODULE_DIR}
cd ${MODULE_SRC_DIR}
# the tests must load the modules that this job built, not the copies that the image already has
${MODULE_SRC_DIR}/test/docker_test/check-loaded-modules.sh ${MODULE_SRC_DIR}/build gosu qore:qore

for test in test/*.qtest; do
    gosu qore:qore qore $test -vv
    RESULTS="$RESULTS $?"
done

# ... and they still do once the tests have run
${MODULE_SRC_DIR}/test/docker_test/check-loaded-modules.sh ${MODULE_SRC_DIR}/build gosu qore:qore

# Exercise native connection-option failures without a database.
python3 -B -W error test/native/run-connection-options.py "build/odbc-api-$(qore --latest-module-api).qmod"
RESULTS="$RESULTS $?"

# The runners below mask credentials in what they print.
python3 -B -W error test/native/test_redact.py
RESULTS="$RESULTS $?"

# Exercise injected binding failures and driver faults against the test database.
python3 -B -W error test/native/run-bind-failure.py "build/odbc-api-$(qore --latest-module-api).qmod"
RESULTS="$RESULTS $?"
python3 -B -W error test/native/run-driver-faults.py "build/odbc-api-$(qore --latest-module-api).qmod"
RESULTS="$RESULTS $?"

# check the results
for R in $RESULTS; do
    if [ "$R" != "0" ]; then
        exit 1 # fail
    fi
done
