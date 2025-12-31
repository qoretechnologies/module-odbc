/* -*- indent-tabs-mode: nil -*- */
/*
    SchemaInfo.cpp

    Qore ODBC module - Schema introspection implementation

    Copyright (C) 2016 - 2024 Qore Technologies s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include "SchemaInfo.h"
#include "ODBCConnection.h"
#include "ODBCErrorHelper.h"

#include <cstring>

namespace odbc {

SchemaInfo::SchemaInfo(ODBCConnection* conn) : connection(conn) {
}

QoreListNode* SchemaInfo::getTables(const char* catalogPattern,
                                     const char* schemaPattern,
                                     const char* tablePattern,
                                     const char* tableType,
                                     ExceptionSink* xsink) {
    SQLHSTMT stmt;
    if (connection->allocStatementHandle(stmt, xsink)) {
        return nullptr;
    }

    SQLRETURN ret = SQLTablesA(stmt,
        (SQLCHAR*)(catalogPattern ? catalogPattern : ""), catalogPattern ? SQL_NTS : 0,
        (SQLCHAR*)(schemaPattern ? schemaPattern : ""), schemaPattern ? SQL_NTS : 0,
        (SQLCHAR*)(tablePattern ? tablePattern : ""), tablePattern ? SQL_NTS : 0,
        (SQLCHAR*)(tableType ? tableType : ""), tableType ? SQL_NTS : 0);

    if (!SQL_SUCCEEDED(ret)) {
        std::string err = "Failed to get tables";
        ODBCErrorHelper::extractDiag(SQL_HANDLE_STMT, stmt, err);
        xsink->raiseException("ODBC-SCHEMA-ERROR", err.c_str());
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);

    while ((ret = SQLFetch(stmt)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        ReferenceHolder<QoreHashNode> row(new QoreHashNode(autoTypeInfo), xsink);

        row->setKeyValue("catalog", getStringColumn(stmt, 1), xsink);
        row->setKeyValue("schema", getStringColumn(stmt, 2), xsink);
        row->setKeyValue("name", getStringColumn(stmt, 3), xsink);
        row->setKeyValue("type", getStringColumn(stmt, 4), xsink);
        row->setKeyValue("remarks", getStringColumn(stmt, 5), xsink);

        result->push(row.release(), xsink);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return result.release();
}

QoreListNode* SchemaInfo::getColumns(const char* catalogPattern,
                                      const char* schemaPattern,
                                      const char* tableName,
                                      const char* columnPattern,
                                      ExceptionSink* xsink) {
    SQLHSTMT stmt;
    if (connection->allocStatementHandle(stmt, xsink)) {
        return nullptr;
    }

    SQLRETURN ret = SQLColumnsA(stmt,
        (SQLCHAR*)(catalogPattern ? catalogPattern : ""), catalogPattern ? SQL_NTS : 0,
        (SQLCHAR*)(schemaPattern ? schemaPattern : ""), schemaPattern ? SQL_NTS : 0,
        (SQLCHAR*)(tableName ? tableName : ""), tableName ? SQL_NTS : 0,
        (SQLCHAR*)(columnPattern ? columnPattern : ""), columnPattern ? SQL_NTS : 0);

    if (!SQL_SUCCEEDED(ret)) {
        std::string err = "Failed to get columns";
        ODBCErrorHelper::extractDiag(SQL_HANDLE_STMT, stmt, err);
        xsink->raiseException("ODBC-SCHEMA-ERROR", err.c_str());
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);

    while ((ret = SQLFetch(stmt)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        ReferenceHolder<QoreHashNode> row(new QoreHashNode(autoTypeInfo), xsink);

        row->setKeyValue("catalog", getStringColumn(stmt, 1), xsink);
        row->setKeyValue("schema", getStringColumn(stmt, 2), xsink);
        row->setKeyValue("table", getStringColumn(stmt, 3), xsink);
        row->setKeyValue("name", getStringColumn(stmt, 4), xsink);
        row->setKeyValue("type", getSmallIntColumn(stmt, 5), xsink);
        row->setKeyValue("type_name", getStringColumn(stmt, 6), xsink);
        row->setKeyValue("size", getIntColumn(stmt, 7), xsink);
        row->setKeyValue("buffer_length", getIntColumn(stmt, 8), xsink);
        row->setKeyValue("decimal_digits", getSmallIntColumn(stmt, 9), xsink);
        row->setKeyValue("num_prec_radix", getSmallIntColumn(stmt, 10), xsink);
        row->setKeyValue("nullable", getSmallIntColumn(stmt, 11), xsink);
        row->setKeyValue("remarks", getStringColumn(stmt, 12), xsink);
        row->setKeyValue("default", getStringColumn(stmt, 13), xsink);
        row->setKeyValue("sql_data_type", getSmallIntColumn(stmt, 14), xsink);
        row->setKeyValue("sql_datetime_sub", getSmallIntColumn(stmt, 15), xsink);
        row->setKeyValue("char_octet_length", getIntColumn(stmt, 16), xsink);
        row->setKeyValue("ordinal_position", getIntColumn(stmt, 17), xsink);
        row->setKeyValue("is_nullable", getStringColumn(stmt, 18), xsink);

        result->push(row.release(), xsink);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return result.release();
}

QoreListNode* SchemaInfo::getPrimaryKeys(const char* catalogPattern,
                                          const char* schemaPattern,
                                          const char* tableName,
                                          ExceptionSink* xsink) {
    SQLHSTMT stmt;
    if (connection->allocStatementHandle(stmt, xsink)) {
        return nullptr;
    }

    SQLRETURN ret = SQLPrimaryKeysA(stmt,
        (SQLCHAR*)(catalogPattern ? catalogPattern : ""), catalogPattern ? SQL_NTS : 0,
        (SQLCHAR*)(schemaPattern ? schemaPattern : ""), schemaPattern ? SQL_NTS : 0,
        (SQLCHAR*)(tableName ? tableName : ""), tableName ? SQL_NTS : 0);

    if (!SQL_SUCCEEDED(ret)) {
        std::string err = "Failed to get primary keys";
        ODBCErrorHelper::extractDiag(SQL_HANDLE_STMT, stmt, err);
        xsink->raiseException("ODBC-SCHEMA-ERROR", err.c_str());
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);

    while ((ret = SQLFetch(stmt)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        ReferenceHolder<QoreHashNode> row(new QoreHashNode(autoTypeInfo), xsink);

        row->setKeyValue("catalog", getStringColumn(stmt, 1), xsink);
        row->setKeyValue("schema", getStringColumn(stmt, 2), xsink);
        row->setKeyValue("table", getStringColumn(stmt, 3), xsink);
        row->setKeyValue("column", getStringColumn(stmt, 4), xsink);
        row->setKeyValue("key_seq", getSmallIntColumn(stmt, 5), xsink);
        row->setKeyValue("pk_name", getStringColumn(stmt, 6), xsink);

        result->push(row.release(), xsink);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return result.release();
}

QoreListNode* SchemaInfo::getForeignKeys(const char* catalogPattern,
                                          const char* schemaPattern,
                                          const char* tableName,
                                          ExceptionSink* xsink) {
    SQLHSTMT stmt;
    if (connection->allocStatementHandle(stmt, xsink)) {
        return nullptr;
    }

    // Get foreign keys where this table is the FK table
    SQLRETURN ret = SQLForeignKeysA(stmt,
        nullptr, 0,  // PK catalog
        nullptr, 0,  // PK schema
        nullptr, 0,  // PK table
        (SQLCHAR*)(catalogPattern ? catalogPattern : ""), catalogPattern ? SQL_NTS : 0,
        (SQLCHAR*)(schemaPattern ? schemaPattern : ""), schemaPattern ? SQL_NTS : 0,
        (SQLCHAR*)(tableName ? tableName : ""), tableName ? SQL_NTS : 0);

    if (!SQL_SUCCEEDED(ret)) {
        std::string err = "Failed to get foreign keys";
        ODBCErrorHelper::extractDiag(SQL_HANDLE_STMT, stmt, err);
        xsink->raiseException("ODBC-SCHEMA-ERROR", err.c_str());
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);

    while ((ret = SQLFetch(stmt)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        ReferenceHolder<QoreHashNode> row(new QoreHashNode(autoTypeInfo), xsink);

        row->setKeyValue("pk_catalog", getStringColumn(stmt, 1), xsink);
        row->setKeyValue("pk_schema", getStringColumn(stmt, 2), xsink);
        row->setKeyValue("pk_table", getStringColumn(stmt, 3), xsink);
        row->setKeyValue("pk_column", getStringColumn(stmt, 4), xsink);
        row->setKeyValue("fk_catalog", getStringColumn(stmt, 5), xsink);
        row->setKeyValue("fk_schema", getStringColumn(stmt, 6), xsink);
        row->setKeyValue("fk_table", getStringColumn(stmt, 7), xsink);
        row->setKeyValue("fk_column", getStringColumn(stmt, 8), xsink);
        row->setKeyValue("key_seq", getSmallIntColumn(stmt, 9), xsink);
        row->setKeyValue("update_rule", getSmallIntColumn(stmt, 10), xsink);
        row->setKeyValue("delete_rule", getSmallIntColumn(stmt, 11), xsink);
        row->setKeyValue("fk_name", getStringColumn(stmt, 12), xsink);
        row->setKeyValue("pk_name", getStringColumn(stmt, 13), xsink);
        row->setKeyValue("deferrability", getSmallIntColumn(stmt, 14), xsink);

        result->push(row.release(), xsink);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return result.release();
}

QoreListNode* SchemaInfo::getIndexes(const char* catalogPattern,
                                      const char* schemaPattern,
                                      const char* tableName,
                                      bool unique,
                                      ExceptionSink* xsink) {
    SQLHSTMT stmt;
    if (connection->allocStatementHandle(stmt, xsink)) {
        return nullptr;
    }

    SQLRETURN ret = SQLStatisticsA(stmt,
        (SQLCHAR*)(catalogPattern ? catalogPattern : ""), catalogPattern ? SQL_NTS : 0,
        (SQLCHAR*)(schemaPattern ? schemaPattern : ""), schemaPattern ? SQL_NTS : 0,
        (SQLCHAR*)(tableName ? tableName : ""), tableName ? SQL_NTS : 0,
        unique ? SQL_INDEX_UNIQUE : SQL_INDEX_ALL,
        SQL_QUICK);

    if (!SQL_SUCCEEDED(ret)) {
        std::string err = "Failed to get indexes";
        ODBCErrorHelper::extractDiag(SQL_HANDLE_STMT, stmt, err);
        xsink->raiseException("ODBC-SCHEMA-ERROR", err.c_str());
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);

    while ((ret = SQLFetch(stmt)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        ReferenceHolder<QoreHashNode> row(new QoreHashNode(autoTypeInfo), xsink);

        row->setKeyValue("catalog", getStringColumn(stmt, 1), xsink);
        row->setKeyValue("schema", getStringColumn(stmt, 2), xsink);
        row->setKeyValue("table", getStringColumn(stmt, 3), xsink);
        row->setKeyValue("non_unique", getSmallIntColumn(stmt, 4), xsink);
        row->setKeyValue("index_qualifier", getStringColumn(stmt, 5), xsink);
        row->setKeyValue("index_name", getStringColumn(stmt, 6), xsink);
        row->setKeyValue("type", getSmallIntColumn(stmt, 7), xsink);
        row->setKeyValue("ordinal_position", getSmallIntColumn(stmt, 8), xsink);
        row->setKeyValue("column", getStringColumn(stmt, 9), xsink);
        row->setKeyValue("asc_or_desc", getStringColumn(stmt, 10), xsink);
        row->setKeyValue("cardinality", getIntColumn(stmt, 11), xsink);
        row->setKeyValue("pages", getIntColumn(stmt, 12), xsink);
        row->setKeyValue("filter_condition", getStringColumn(stmt, 13), xsink);

        result->push(row.release(), xsink);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return result.release();
}

QoreListNode* SchemaInfo::getProcedures(const char* catalogPattern,
                                         const char* schemaPattern,
                                         const char* procPattern,
                                         ExceptionSink* xsink) {
    SQLHSTMT stmt;
    if (connection->allocStatementHandle(stmt, xsink)) {
        return nullptr;
    }

    SQLRETURN ret = SQLProceduresA(stmt,
        (SQLCHAR*)(catalogPattern ? catalogPattern : ""), catalogPattern ? SQL_NTS : 0,
        (SQLCHAR*)(schemaPattern ? schemaPattern : ""), schemaPattern ? SQL_NTS : 0,
        (SQLCHAR*)(procPattern ? procPattern : ""), procPattern ? SQL_NTS : 0);

    if (!SQL_SUCCEEDED(ret)) {
        std::string err = "Failed to get procedures";
        ODBCErrorHelper::extractDiag(SQL_HANDLE_STMT, stmt, err);
        xsink->raiseException("ODBC-SCHEMA-ERROR", err.c_str());
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);

    while ((ret = SQLFetch(stmt)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        ReferenceHolder<QoreHashNode> row(new QoreHashNode(autoTypeInfo), xsink);

        row->setKeyValue("catalog", getStringColumn(stmt, 1), xsink);
        row->setKeyValue("schema", getStringColumn(stmt, 2), xsink);
        row->setKeyValue("name", getStringColumn(stmt, 3), xsink);
        row->setKeyValue("num_input_params", getIntColumn(stmt, 4), xsink);
        row->setKeyValue("num_output_params", getIntColumn(stmt, 5), xsink);
        row->setKeyValue("num_result_sets", getIntColumn(stmt, 6), xsink);
        row->setKeyValue("remarks", getStringColumn(stmt, 7), xsink);
        row->setKeyValue("type", getSmallIntColumn(stmt, 8), xsink);

        result->push(row.release(), xsink);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return result.release();
}

QoreListNode* SchemaInfo::getTypeInfo(ExceptionSink* xsink) {
    SQLHSTMT stmt;
    if (connection->allocStatementHandle(stmt, xsink)) {
        return nullptr;
    }

    SQLRETURN ret = SQLGetTypeInfo(stmt, SQL_ALL_TYPES);

    if (!SQL_SUCCEEDED(ret)) {
        std::string err = "Failed to get type info";
        ODBCErrorHelper::extractDiag(SQL_HANDLE_STMT, stmt, err);
        xsink->raiseException("ODBC-SCHEMA-ERROR", err.c_str());
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);

    while ((ret = SQLFetch(stmt)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        ReferenceHolder<QoreHashNode> row(new QoreHashNode(autoTypeInfo), xsink);

        row->setKeyValue("type_name", getStringColumn(stmt, 1), xsink);
        row->setKeyValue("data_type", getSmallIntColumn(stmt, 2), xsink);
        row->setKeyValue("column_size", getIntColumn(stmt, 3), xsink);
        row->setKeyValue("literal_prefix", getStringColumn(stmt, 4), xsink);
        row->setKeyValue("literal_suffix", getStringColumn(stmt, 5), xsink);
        row->setKeyValue("create_params", getStringColumn(stmt, 6), xsink);
        row->setKeyValue("nullable", getSmallIntColumn(stmt, 7), xsink);
        row->setKeyValue("case_sensitive", getSmallIntColumn(stmt, 8), xsink);
        row->setKeyValue("searchable", getSmallIntColumn(stmt, 9), xsink);
        row->setKeyValue("unsigned_attribute", getSmallIntColumn(stmt, 10), xsink);
        row->setKeyValue("fixed_prec_scale", getSmallIntColumn(stmt, 11), xsink);
        row->setKeyValue("auto_unique_value", getSmallIntColumn(stmt, 12), xsink);
        row->setKeyValue("local_type_name", getStringColumn(stmt, 13), xsink);
        row->setKeyValue("minimum_scale", getSmallIntColumn(stmt, 14), xsink);
        row->setKeyValue("maximum_scale", getSmallIntColumn(stmt, 15), xsink);

        result->push(row.release(), xsink);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return result.release();
}

QoreValue SchemaInfo::getStringColumn(SQLHSTMT stmt, int col) {
    char buffer[4096];
    SQLLEN indicator;

    SQLRETURN ret = SQLGetData(stmt, col, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);

    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        if (indicator == SQL_NULL_DATA) {
            return QoreValue();
        }
        return new QoreStringNode(buffer);
    }

    return QoreValue();
}

QoreValue SchemaInfo::getIntColumn(SQLHSTMT stmt, int col) {
    SQLINTEGER value;
    SQLLEN indicator;

    SQLRETURN ret = SQLGetData(stmt, col, SQL_C_SLONG, &value, sizeof(value), &indicator);

    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        if (indicator == SQL_NULL_DATA) {
            return QoreValue();
        }
        return static_cast<int64>(value);
    }

    return QoreValue();
}

QoreValue SchemaInfo::getSmallIntColumn(SQLHSTMT stmt, int col) {
    SQLSMALLINT value;
    SQLLEN indicator;

    SQLRETURN ret = SQLGetData(stmt, col, SQL_C_SSHORT, &value, sizeof(value), &indicator);

    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        if (indicator == SQL_NULL_DATA) {
            return QoreValue();
        }
        return static_cast<int64>(value);
    }

    return QoreValue();
}

} // namespace odbc
