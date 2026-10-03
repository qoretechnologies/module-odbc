#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Verify the public API links and executable metadata guide examples."""
from html.parser import HTMLParser
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

BUILD = Path(sys.argv.pop(1)).resolve()
SOURCE = Path(__file__).resolve().parents[1]


class Links(HTMLParser):
    def __init__(self, path):
        super().__init__()
        self.links = []
        self.feed(path.read_text())

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "a" and "href" in attrs:
            self.links.append(attrs["href"])


class DocumentationTests(unittest.TestCase):
    def test_binding_function_and_constant_have_public_pages(self):
        tree = ET.parse(BUILD / "odbc.tag")
        for name in ("odbc_bind", "ODBCT_SLONG"):
            with self.subTest(name=name):
                members = [m for m in tree.findall(".//member") if m.findtext("name") == name]
                self.assertTrue(members, name)
                for member in members:
                    page = member.findtext("anchorfile")
                    self.assertTrue((BUILD / "docs/odbc/html" / page).is_file(), page)

    def test_binding_guide_links_to_the_binding_function(self):
        tree = ET.parse(BUILD / "odbc.tag")
        targets = {m.findtext("anchorfile") + "#" + m.findtext("anchor")
                   for m in tree.findall(".//member") if m.findtext("name") == "odbc_bind"}
        links = Links(BUILD / "docs/odbc/html/odbcbindguide.html").links
        self.assertTrue(targets.intersection(links), targets)

    def test_metadata_guide_links_to_each_documented_api(self):
        config = (BUILD / "Doxyfile").read_text()
        match = re.search(r'"([^"\n]*qore\.tag)=([^"\n]+)"', config)
        self.assertIsNotNone(match, "Qore API tag file must be configured")
        tree = ET.parse(match[1])
        links = set(Links(BUILD / "docs/odbc/html/odbcschemaguide.html").links)
        for cls, methods in {
            "Datasource": ("describe", "selectRows", "getSQLStatement", "commit", "rollback"),
            "SQLStatement": ("prepare", "exec", "describe", "close"),
        }.items():
            compound = tree.find(f"./compound[name='Qore::SQL::{cls}']")
            self.assertIsNotNone(compound)
            for method in methods:
                with self.subTest(cls=cls, method=method):
                    targets = {match[2].rstrip("/") + "/" + m.findtext("anchorfile") + "#"
                               + m.findtext("anchor") for m in compound.findall("member")
                               if m.findtext("name") == method}
                    self.assertTrue(targets.intersection(links), (cls, method, targets))

    def test_metadata_guide_and_release_notes_do_not_advertise_missing_functions(self):
        for page in ("odbcschemaguide.html", "odbcreleasenotesguide.html"):
            text = (BUILD / "docs/odbc/html" / page).read_text()
            for function in ("tables", "columns", "primary_keys", "foreign_keys", "indexes",
                             "procedures", "type_info"):
                self.assertNotIn("odbc_get_" + function, text)

    def test_binding_guide_links_directly_to_options(self):
        links = Links(BUILD / "docs/odbc/html/odbcbindguide.html").links
        self.assertIn("odbcoptionsguide.html", links)
        self.assertNotIn("index.html#odbcoptions", links)

    def test_metadata_examples(self):
        source = (SOURCE / "docs/guide-odbcschema.dox.tmpl").read_text()
        examples = re.findall(r"@code\{\.py\}\n(.*?)\s*@endcode", source, re.DOTALL)
        self.assertEqual(2, len(examples))
        script = "%modern\n%requires odbc\n"
        for index, example in enumerate(examples):
            script += f"sub example{index}(Datasource ds) {{\n{example}\n}}\n"
        # Set a connection string for integration coverage; otherwise still parse every example
        # against the local binary module to reject missing methods and type errors.
        if os.environ.get("QORE_DB_CONNSTR_ODBC_DOCS"):
            script += '''
Datasource ds(ENV.QORE_DB_CONNSTR_ODBC_DOCS);
ds.exec("CREATE TEMP TABLE users (id INTEGER, name VARCHAR(50))");
ds.commit();
on_exit ds.rollback();
example0(ds);
example1(ds);
SQLStatement stmt = ds.getSQLStatement();
on_exit stmt.close();
stmt.prepare("SELECT id, name FROM users WHERE 1 = 0");
stmt.exec();
hash<auto> columns = stmt.describe();
@assert(columns.hasKey("id"));
@assert(columns.hasKey("name"));
@assert(!stmt.fetchRows());
@assert(!ds.selectRows("SELECT id FROM users WHERE 1 = 0"));
'''
        env = dict(os.environ, QORE_MODULE_DIR=str(BUILD))
        env.pop("LD_PRELOAD", None)
        with tempfile.TemporaryDirectory(prefix="odbc-doc-examples-") as directory:
            path = Path(directory) / "examples.qr"
            path.write_text(script)
            result = subprocess.run(["qore", "--enable-debug", "-r", str(path)],
                                    env=env, capture_output=True, text=True, timeout=60)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertEqual("", result.stderr)


if __name__ == "__main__":
    unittest.main()
