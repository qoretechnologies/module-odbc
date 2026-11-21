# Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
"""Masks the test database's credentials in text that a native test runner prints to the job log."""
import os
import re

MASK = "<masked>"
# a connection string attribute: a value in braces, where "}}" is a literal "}", a quoted value, or a plain value
_ATTRIBUTE = re.compile(r"(?:^|[;{\s])(?:pwd|password|passwd)\s*=\s*"
                        r"(\{(?:[^}]|\}\})*\}|'(?:[^'\\]|\\.)*'|\"[^\"]*\"|[^;}\s]*)", re.I)
_USER = re.compile(r"[^:/]*:[^/]*/")


def _attribute_values(raw):
    """Returns the forms of an attribute value that can appear in text."""
    values = {raw}
    if len(raw) >= 2 and raw[0] == "{" and raw[-1] == "}":
        inner = raw[1:-1]
        values |= {inner, inner.replace("}}", "}")}
    elif len(raw) >= 2 and raw[0] == raw[-1] and raw[0] in "'\"":
        values.add(raw[1:-1])
    return values


def secrets(env=None):
    """Returns every credential form in the environment: a password can contain "@", ";" and braces."""
    env = os.environ if env is None else env
    found = set()
    if env.get("QORE_DB_PASS"):
        found.add(env["QORE_DB_PASS"])
    value = env.get("QORE_DB_CONNSTR_ODBC")
    if value:
        found.add(value)
        user = _USER.match(value)
        if user:
            # the password ends at one of the "@" characters after it: each candidate is masked
            for at in (i for i, char in enumerate(value) if char == "@" and i > user.end()):
                found.add(value[user.end():at])
        for match in _ATTRIBUTE.finditer(value):
            found |= _attribute_values(match.group(1))
    return {item for item in found if item}


def redact(text, env=None):
    """Returns the text with every credential form masked, longest first."""
    for item in sorted(secrets(env), key=len, reverse=True):
        text = text.replace(item, MASK)
    return text
