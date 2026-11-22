#!/usr/bin/python3
# Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
"""Checks that the native test runners mask every form of a credential.  All values are fake."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

from ci_redact import redact, redacted_repr

DRIVER = "conn=DRIVER=PostgreSQL Unicode;Server=db"


class RedactTest(unittest.TestCase):
    def check(self, env, *values):
        for value in values:
            with self.subTest(value=value):
                # the whole value must go: a partial mask would leave the rest of a password in the log
                self.assertTrue(redact(f"before {value} after", env) == "before <masked> after",
                                "the value is not fully masked")

    def test_whole_connection_string(self):
        value = f"odbc:fake_user/fake-secret-1@db(UTF8){{{DRIVER}}}"
        self.check({"QORE_DB_CONNSTR_ODBC": value}, value, "fake-secret-1")

    def test_password_with_at_signs(self):
        value = f"odbc:fake_user/fake@secret@2@db(UTF8){{{DRIVER}}}"
        self.check({"QORE_DB_CONNSTR_ODBC": value}, value, "fake@secret@2")

    def test_malformed_connection_string(self):
        value = "odbc:fake_user/fake-secret-3"
        self.check({"QORE_DB_CONNSTR_ODBC": value}, value)

    def test_password_variable(self):
        self.check({"QORE_DB_PASS": "fake-secret-4"}, "fake-secret-4")

    def test_plain_attribute(self):
        value = f"odbc:@(UTF8){{{DRIVER};UID=u;PWD=fake-secret-5;Port=1}}"
        self.check({"QORE_DB_CONNSTR_ODBC": value}, "fake-secret-5")

    def test_attribute_names_are_not_case_sensitive(self):
        for name in ("pwd", "Pwd", "PASSWORD", "Password", "passwd"):
            value = f"odbc:@(UTF8){{{DRIVER};{name}=fake-secret-6}}"
            self.check({"QORE_DB_CONNSTR_ODBC": value}, "fake-secret-6")

    def test_braced_attribute_with_escaped_brace(self):
        value = f"odbc:@(UTF8){{{DRIVER};PWD={{fake}}}}secret;7}};Port=1}}"
        self.check({"QORE_DB_CONNSTR_ODBC": value}, "{fake}}secret;7}", "fake}}secret;7", "fake}secret;7")

    def test_quoted_attribute(self):
        value = f"odbc:@(UTF8){{{DRIVER};pqopt={{password='fake secret 8'}}}}"
        self.check({"QORE_DB_CONNSTR_ODBC": value}, "'fake secret 8'", "fake secret 8")

    def test_longest_form_first(self):
        value = "odbc:fake_user/ab@cd@db"
        self.assertEqual("x <masked> y", redact(f"x {value} y", {"QORE_DB_CONNSTR_ODBC": value}))

    def test_other_text_is_unchanged(self):
        value = "odbc:fake_user/fake-secret-9@db"
        text = "ODBC_BIND_TRACE bind column 1\nRan 1 test case"
        self.assertEqual(text, redact(text, {"QORE_DB_CONNSTR_ODBC": value}))

    def test_no_credentials(self):
        self.assertEqual("text", redact("text", {}))

    def test_plain_value_runs_to_the_semicolon(self):
        value = f"odbc:@(UTF8){{{DRIVER};PWD=fake secret 10;Port=1}}"
        env = {"QORE_DB_CONNSTR_ODBC": value}
        self.check(env, "fake secret 10")
        # the first word of a password with spaces is an ordinary word elsewhere
        self.assertEqual("fake", redact("fake", env))

    def test_comma_inside_a_value(self):
        env = {"QORE_DB_CONNSTR_ODBC": "PWD=fake,secret-20;Port=1"}
        self.check(env, "fake,secret-20")
        self.assertEqual("fake", redact("fake", env))

    def test_libpq_value_ends_before_the_next_key(self):
        env = {"QORE_DB_CONNSTR_ODBC": "odbc:@(UTF8){pqopt={password=fake-secret-21 host=db}}"}
        self.check(env, "fake-secret-21")
        self.assertEqual("host=db", redact("host=db", env))

    def test_quoted_value_followed_by_plain_text(self):
        env = {"QORE_DB_CONNSTR_ODBC": "PWD='fake'secret-22;Port=1"}
        self.check(env, "'fake'secret-22", "fakesecret-22")

    def test_descriptor_with_at_in_its_options(self):
        env = {"QORE_DB_CONNSTR_ODBC": "odbc:fake_user/fake-secret-23(UTF8){x=a@b}"}
        self.check(env, "fake-secret-23")

    def test_plain_value_before_a_datasource_option(self):
        value = f"odbc:@(UTF8){{{DRIVER};PWD=fake-secret-11,min=1}}"
        self.check({"QORE_DB_CONNSTR_ODBC": value}, "fake-secret-11")

    def test_key_after_a_comma(self):
        value = "odbc:@(UTF8){min=1,pwd=fake-secret-12}"
        self.check({"QORE_DB_CONNSTR_ODBC": value}, "fake-secret-12")

    def test_key_at_the_start_and_after_white_space(self):
        for value in ("PWD=fake-secret-13", "Server=db; PWD = fake-secret-13", "odbc:@(UTF8){conn=PWD=fake-secret-13}"):
            self.check({"QORE_DB_CONNSTR_ODBC": value}, "fake-secret-13")

    def test_a_key_that_only_ends_in_pwd_is_not_a_password(self):
        value = "odbc:@(UTF8){conn=NOTPWD=visible-value}"
        self.assertEqual("visible-value", redact("visible-value", {"QORE_DB_CONNSTR_ODBC": value}))

    def test_unclosed_brace(self):
        value = "odbc:@(UTF8){conn=PWD={fake-secret-14"
        self.check({"QORE_DB_CONNSTR_ODBC": value}, "fake-secret-14")

    def test_quoted_value_with_an_escaped_quote(self):
        value = r"odbc:@(UTF8){pqopt={password='fake\'secret 15'}}"
        self.check({"QORE_DB_CONNSTR_ODBC": value}, r"'fake\'secret 15'", r"fake\'secret 15", "fake'secret 15")

    def test_pgpassword(self):
        self.check({"PGPASSWORD": "fake-secret-16"}, "fake-secret-16")

    def test_malformed_string_without_at(self):
        for value in ("odbc:fake_user/fake-secret-17", "fake_user/fake-secret-17",
                      "odbc:fake_user/fake-secret-17(UTF8)", "odbc:fake_user/fake-secret-17{min=1}"):
            self.check({"QORE_DB_CONNSTR_ODBC": value}, "fake-secret-17")

    def test_a_path_in_an_option_is_not_a_password(self):
        value = "odbc:@(UTF8){conn=DRIVER=/usr/lib/fake.so}"
        self.assertEqual("/usr/lib", redact("/usr/lib", {"QORE_DB_CONNSTR_ODBC": value}))

    def test_redacted_repr_with_a_backslash_and_both_quotes(self):
        secret = "fake\\'\"SECRET18"
        out = redacted_repr([f"line {secret} end"], {"QORE_DB_PASS": secret})
        self.assertEqual("['line <masked> end']", out)


# A stub of the Qore binary: prints the credential, then fails, hangs, prints bytes that are not text, or succeeds with
# an unexpected trace.  It reads the credential from its environment.
STUB_QORE = r"""#!/bin/sh
for arg; do
    if [ "$arg" = -ne ]; then
        printf 1
        exit 0
    fi
done
printf '%s\n' "$STUB_SECRET"
printf '%s\n' "$STUB_SECRET" >&2
case "$STUB_MODE" in
    fail) exit 3 ;;
    hang) exec tail -f /dev/null ;;
    binary)
        printf '\377%s\n' "$STUB_SECRET"
        printf '\377%s\n' "$STUB_SECRET" >&2
        exit 3 ;;
    trace)
        for prefix in ODBC_BIND_TRACE ODBC_FAULT_TRACE ATTRIBUTE; do
            printf '%s %s\n' "$prefix" "$STUB_SECRET" >&2
        done
        exit 0 ;;
esac
exit 3
"""
# A stub of the compiler: the output is an empty program that succeeds.
STUB_COMPILER = r"""#!/bin/sh
while [ $# -gt 0 ]; do
    if [ "$1" = -o ]; then
        printf '#!/bin/sh\nexit 0\n' > "$2"
        chmod +x "$2"
    fi
    shift
done
"""
# Runs a runner; in "hang" mode, the time limit of a test program is one second instead of the runner's own.
LAUNCH = r"""
import os, runpy, subprocess, sys
script, module = sys.argv[1:3]
real = subprocess.run
def limited(command, **kwargs):
    if os.environ["STUB_MODE"] == "hang" and "-ne" not in command and kwargs.get("timeout"):
        kwargs["timeout"] = 1
    return real(command, **kwargs)
subprocess.run = limited
sys.path.insert(0, os.path.dirname(script))
sys.argv = [script, module]
runpy.run_path(script, run_name="__main__")
"""
HERE = Path(__file__).resolve().parent
RUNNERS = ("run-connection-options.py", "run-bind-failure.py", "run-driver-faults.py")
FRAGMENT = "SECRET19"
SECRET = "fake\\'\"" + FRAGMENT


class RunnerFailureTest(unittest.TestCase):
    """No failure path of a runner prints the credential of the test database."""

    def run_runner(self, runner, mode):
        with tempfile.TemporaryDirectory(prefix="odbc-runner-") as temporary:
            work = Path(temporary)
            for name, text in (("qore", STUB_QORE), ("c++", STUB_COMPILER)):
                (work / name).write_text(text)
                (work / name).chmod(0o755)
            module = work / "odbc.qmod"
            module.write_text("")
            env = dict(os.environ, PATH=f"{work}{os.pathsep}{os.environ['PATH']}", STUB_MODE=mode, STUB_SECRET=SECRET,
                       QORE_DB_CONNSTR_ODBC=f"odbc:fake_user/{SECRET}@db(UTF8){{{DRIVER}}}", QORE_DB_PASS=SECRET)
            return subprocess.run([sys.executable, "-B", "-c", LAUNCH, str(HERE / runner), str(module)], env=env,
                                  cwd=temporary, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, timeout=120)

    def test_failure_paths(self):
        for runner in RUNNERS:
            for mode in ("fail", "hang", "binary", "trace"):
                with self.subTest(runner=runner, mode=mode):
                    result = self.run_runner(runner, mode)
                    self.assertNotEqual(0, result.returncode, "the failure is reported")
                    self.assertTrue(FRAGMENT.encode() not in result.stdout, "the credential is not in the output")
                    self.assertTrue(b"0xff" not in result.stdout, "no byte of the output is in an error")


class CiScriptTest(unittest.TestCase):
    def test_the_exit_handler_is_set_before_the_database_is_created(self):
        for script in ("test-ubuntu.sh", "test-alpine.sh"):
            text = (HERE.parent / "docker_test" / script).read_text()
            handler = text.find("\nodbc_cleanup_install\n")
            setup = text.find("\nsetup_odbc_database\n")
            self.assertTrue(0 <= handler < setup, f"{script}: the handler must be set before the database setup")
            self.assertEqual(-1, text.find("\ncleanup_postgres_on_host"), f"{script}: cleanup runs only in the handler")

    def test_every_runner_runs_without_writing_bytecode(self):
        for script in ("test-ubuntu.sh", "test-alpine.sh"):
            text = (HERE.parent / "docker_test" / script).read_text()
            lines = re.findall(r"^python3 .*test/native/\S+\.py.*$", text, re.M)
            self.assertEqual(len(RUNNERS) + 1, len(lines), script)
            for line in lines:
                self.assertTrue(line.startswith("python3 -B -W error "), f"{script}: {line}")


if __name__ == "__main__":
    unittest.main()
