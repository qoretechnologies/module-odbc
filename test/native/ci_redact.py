# Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
"""Masks the test database's credentials in text that a native test runner prints to the job log."""
import os
import re

MASK = "<masked>"
# a password key: at the start, as the first key of the "conn" option, or after the separator of a connection string,
# of datasource options or of libpq options
_KEY = re.compile(r"(?:^|(?<=[;,{\s])|(?<=conn=))(?:pwd|password|passwd)\s*=[ \t]*", re.I)
_QUOTED = re.compile(r"'(?:[^'\\]|\\.)*'|\"[^\"]*\"", re.S)
# the user part of a datasource descriptor; a "/" in an option value does not start a password
_USER = re.compile(r"(?:[^:/@{(;=]*:)?[^:/@{(;=]*/")


def _braced_values(text, start):
    """Returns the forms of a value in braces, where "}}" is a literal "}"; an unclosed value runs to the end."""
    i = start + 1
    while i < len(text):
        if text[i] == "}":
            if text[i + 1:i + 2] != "}":
                inner = text[start + 1:i]
                return {text[start:i + 1], inner, inner.replace("}}", "}")}
            i += 1
        i += 1
    inner = text[start + 1:]
    return {text[start:], inner, inner.replace("}}", "}")}


def _plain_values(text, start):
    """Returns the forms of an unbraced value: it runs to ";" or the end of the connection string.

    In a datasource descriptor, a "," or the closing "}" of the options can end it first, and in libpq options white
    space can, so each of those shorter forms is masked as well.
    """
    end = text.find(";", start)
    raw = (text[start:] if end < 0 else text[start:end]).strip()
    values = {raw, raw.rstrip("}")}
    for pattern in (r",", r"\s"):
        values.add(re.split(pattern, raw, maxsplit=1)[0].rstrip("}"))
    return values


def _attribute_values(text, start):
    """Returns the forms of the value that starts at the given position."""
    if text[start:start + 1] == "{":
        return _braced_values(text, start)
    quoted = _QUOTED.match(text, start)
    if quoted:
        raw = quoted.group(0)
        return {raw, raw[1:-1], re.sub(r"\\(.)", r"\1", raw[1:-1], flags=re.S)}
    return _plain_values(text, start)


def _descriptor_values(value):
    """Returns the possible passwords of a datasource descriptor, which can contain "@" and can be malformed."""
    user = _USER.match(value)
    if not user:
        return set()
    rest = value[user.end():]
    ats = [i for i, char in enumerate(rest) if char == "@"]
    if ats:
        # the password ends at one of the "@" characters: each candidate is masked
        return {rest[:at] for at in ats}
    # no "@": the password runs to the options or to the end
    return {rest, re.split(r"[({]", rest, maxsplit=1)[0]}


def secrets(env=None):
    """Returns every credential form in the environment: a password can contain "@", ";" and braces."""
    env = os.environ if env is None else env
    found = {env[name] for name in ("QORE_DB_PASS", "PGPASSWORD") if env.get(name)}
    value = env.get("QORE_DB_CONNSTR_ODBC")
    if value:
        found.add(value)
        found |= _descriptor_values(value)
        for match in _KEY.finditer(value):
            found |= _attribute_values(value, match.end())
    return {item for item in found if item}


def redact(text, env=None):
    """Returns the text with every credential form masked, longest first."""
    for item in sorted(secrets(env), key=len, reverse=True):
        text = text.replace(item, MASK)
    return text


def redacted_repr(items, env=None):
    """Returns the repr of a list of strings, masked first: repr() escapes a value so that it would no longer match."""
    return repr([redact(str(item), env) for item in items])
