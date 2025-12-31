/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SchemaInfo.h

    Qore ODBC module - Schema introspection API

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

#ifndef _QORE_MODULE_ODBC_SCHEMAINFO_H
#define _QORE_MODULE_ODBC_SCHEMAINFO_H

#include <sql.h>
#include <sqlext.h>

#include "qore/Qore.h"

namespace odbc {

class ODBCConnection;

//! Schema introspection helper class
/** Provides methods to query database schema metadata using ODBC catalog functions.

    Example usage:
    @code
    SchemaInfo schema(connection);

    // Get all tables
    QoreListNode* tables = schema.getTables(xsink);

    // Get columns for a specific table
    QoreListNode* columns = schema.getColumns("users", xsink);

    // Get primary key
    QoreListNode* pk = schema.getPrimaryKeys("users", xsink);
    @endcode
*/
class SchemaInfo {
public:
    //! Constructor
    /** @param conn the ODBC connection to use
     */
    DLLLOCAL explicit SchemaInfo(ODBCConnection* conn);

    //! Destructor
    DLLLOCAL ~SchemaInfo() = default;

    //! Get list of tables in the database
    /** Returns a list of hashes, each containing:
        - catalog: catalog name
        - schema: schema name
        - name: table name
        - type: table type (TABLE, VIEW, etc.)
        - remarks: table remarks/comments

        @param catalogPattern optional catalog pattern (SQL LIKE pattern or nullptr for all)
        @param schemaPattern optional schema pattern (SQL LIKE pattern or nullptr for all)
        @param tablePattern optional table name pattern (SQL LIKE pattern or nullptr for all)
        @param tableType optional table type filter (e.g., "TABLE", "VIEW", or nullptr for all)
        @param xsink exception sink
        @return list of table info hashes
     */
    DLLLOCAL QoreListNode* getTables(const char* catalogPattern,
                                      const char* schemaPattern,
                                      const char* tablePattern,
                                      const char* tableType,
                                      ExceptionSink* xsink);

    //! Get list of columns for a table
    /** Returns a list of hashes, each containing:
        - catalog: catalog name
        - schema: schema name
        - table: table name
        - name: column name
        - type: SQL data type code
        - type_name: data type name
        - size: column size
        - decimal_digits: decimal digits
        - nullable: whether column is nullable
        - remarks: column remarks/comments
        - default: default value
        - ordinal_position: column position (1-based)

        @param catalogPattern optional catalog pattern
        @param schemaPattern optional schema pattern
        @param tableName table name (can be pattern)
        @param columnPattern optional column name pattern (nullptr for all)
        @param xsink exception sink
        @return list of column info hashes
     */
    DLLLOCAL QoreListNode* getColumns(const char* catalogPattern,
                                       const char* schemaPattern,
                                       const char* tableName,
                                       const char* columnPattern,
                                       ExceptionSink* xsink);

    //! Get primary key columns for a table
    /** Returns a list of hashes, each containing:
        - catalog: catalog name
        - schema: schema name
        - table: table name
        - column: column name
        - key_seq: column sequence in key (1-based)
        - pk_name: primary key constraint name

        @param catalogPattern optional catalog pattern
        @param schemaPattern optional schema pattern
        @param tableName table name
        @param xsink exception sink
        @return list of primary key column info
     */
    DLLLOCAL QoreListNode* getPrimaryKeys(const char* catalogPattern,
                                           const char* schemaPattern,
                                           const char* tableName,
                                           ExceptionSink* xsink);

    //! Get foreign keys for a table
    /** Returns a list of hashes, each containing:
        - pk_catalog, pk_schema, pk_table, pk_column: primary key table/column
        - fk_catalog, fk_schema, fk_table, fk_column: foreign key table/column
        - key_seq: column sequence
        - update_rule: update rule code
        - delete_rule: delete rule code
        - fk_name: foreign key constraint name
        - pk_name: primary key constraint name

        @param catalogPattern optional catalog pattern
        @param schemaPattern optional schema pattern
        @param tableName table name
        @param xsink exception sink
        @return list of foreign key info hashes
     */
    DLLLOCAL QoreListNode* getForeignKeys(const char* catalogPattern,
                                           const char* schemaPattern,
                                           const char* tableName,
                                           ExceptionSink* xsink);

    //! Get indexes for a table
    /** Returns a list of hashes, each containing:
        - catalog: catalog name
        - schema: schema name
        - table: table name
        - non_unique: whether index allows duplicates
        - index_name: index name
        - type: index type
        - ordinal_position: column position in index
        - column: column name
        - asc_or_desc: sort order ('A' or 'D')
        - cardinality: number of unique values
        - pages: number of pages used

        @param catalogPattern optional catalog pattern
        @param schemaPattern optional schema pattern
        @param tableName table name
        @param unique if true, return only unique indexes
        @param xsink exception sink
        @return list of index info hashes
     */
    DLLLOCAL QoreListNode* getIndexes(const char* catalogPattern,
                                       const char* schemaPattern,
                                       const char* tableName,
                                       bool unique,
                                       ExceptionSink* xsink);

    //! Get list of stored procedures
    /** Returns a list of hashes, each containing:
        - catalog: catalog name
        - schema: schema name
        - name: procedure name
        - num_input_params: number of input parameters
        - num_output_params: number of output parameters
        - num_result_sets: number of result sets
        - remarks: procedure remarks/comments
        - type: procedure type

        @param catalogPattern optional catalog pattern
        @param schemaPattern optional schema pattern
        @param procPattern optional procedure name pattern
        @param xsink exception sink
        @return list of procedure info hashes
     */
    DLLLOCAL QoreListNode* getProcedures(const char* catalogPattern,
                                          const char* schemaPattern,
                                          const char* procPattern,
                                          ExceptionSink* xsink);

    //! Get supported SQL data types
    /** Returns a list of hashes describing the data types supported by the data source.

        @param xsink exception sink
        @return list of type info hashes
     */
    DLLLOCAL QoreListNode* getTypeInfo(ExceptionSink* xsink);

private:
    //! The connection to use
    ODBCConnection* connection;

    //! Helper to fetch result set from a catalog function
    DLLLOCAL QoreListNode* fetchCatalogResults(SQLHSTMT stmt, ExceptionSink* xsink);

    //! Helper to get string from result column (handles NULL)
    DLLLOCAL QoreValue getStringColumn(SQLHSTMT stmt, int col);

    //! Helper to get integer from result column (handles NULL)
    DLLLOCAL QoreValue getIntColumn(SQLHSTMT stmt, int col);

    //! Helper to get smallint from result column (handles NULL)
    DLLLOCAL QoreValue getSmallIntColumn(SQLHSTMT stmt, int col);
};

} // namespace odbc

#endif // _QORE_MODULE_ODBC_SCHEMAINFO_H
