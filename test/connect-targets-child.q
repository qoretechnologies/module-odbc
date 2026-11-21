#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Child process of connect-targets.qtest: opens each datasource in the file given (a JSON list of hashes with "db"),
# from a sandboxed Program whose network policy allows everything but DeniedAddress, and prints a
# JSON list with the result of each: {"ok": true} if it connected, else the error and its description.  It runs in
# a process of its own so that the test can give it the environment that the ODBC driver manager reads (ODBCSYSINI,
# ODBCINI, HOME).

%modern
%prepend-module-path "${SCRIPT_DIR}/../build"
%requires odbc
%requires json

#! the address that the sandbox denies
const DeniedAddress = "10.255.255.2";

#! the sandboxed code: it opens a datasource that it creates
const SandboxedSource = "
hash<auto> sub try_open(hash<auto> conn) {
    try {
        # a datasource string (\"connstr\"), or a connection string or DSN (\"db\")
        Datasource ds = conn.connstr ? new Datasource(conn.connstr) : new Datasource({\"type\": \"odbc\",
            \"db\": conn.db});
        ds.open();
        ds.close();
        return {\"ok\": True};
    } catch (hash<ExceptionInfo> ex) {
        return {\"err\": ex.err, \"desc\": ex.desc};
    }
}
";

Program p(PO_NEW_STYLE);
p.parse(SandboxedSource, "sandboxed");
SandboxManager sm();
sm.setNetworkDefaultPolicy(True);
sm.addNetworkDeniedIPRange(DeniedAddress + "/32");
p.setSandboxManager(sm);

list<auto> rv = ();
foreach hash<auto> conn in (parse_json(ReadOnlyFile::readTextFile(ARGV[0]))) {
    rv += p.callFunction("try_open", conn);
}
print(make_json(rv) + "\n");
