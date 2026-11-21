#!/usr/bin/python3
# Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
"""Checks that the native test runners mask every form of a credential.  All values are fake."""
import unittest

from ci_redact import redact

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


if __name__ == "__main__":
    unittest.main()
