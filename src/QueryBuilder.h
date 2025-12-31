/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QueryBuilder.h

    Qore ODBC module - SQL Query Builder

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

#ifndef _QORE_MODULE_ODBC_QUERYBUILDER_H
#define _QORE_MODULE_ODBC_QUERYBUILDER_H

#include <string>
#include <vector>
#include <sstream>

#include "qore/Qore.h"

namespace odbc {

//! SQL Query Builder
/** Provides a fluent interface for building SQL queries programmatically.

    Supports SELECT, INSERT, UPDATE, and DELETE queries with WHERE clauses,
    ORDER BY, LIMIT, and JOIN support.

    @par Example:
    @code
    QueryBuilder qb;
    std::string sql = qb.select({"id", "name", "email"})
                        .from("users")
                        .where("active", "=", "1")
                        .orderBy("name")
                        .limit(10)
                        .build();
    // Result: SELECT id, name, email FROM users WHERE active = ? ORDER BY name LIMIT 10
    @endcode
*/
class QueryBuilder {
public:
    //! Constructor
    DLLLOCAL QueryBuilder();

    //! Destructor
    DLLLOCAL ~QueryBuilder() = default;

    //! Reset the builder for a new query
    DLLLOCAL QueryBuilder& reset();

    //! Start a SELECT query
    /** @param columns list of column names to select
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& select(const std::vector<std::string>& columns);

    //! Start a SELECT DISTINCT query
    /** @param columns list of column names to select
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& selectDistinct(const std::vector<std::string>& columns);

    //! Start a SELECT * query
    /** @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& selectAll();

    //! Start an INSERT query
    /** @param table the table to insert into
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& insertInto(const std::string& table);

    //! Start an UPDATE query
    /** @param table the table to update
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& update(const std::string& table);

    //! Start a DELETE query
    /** @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& deleteFrom();

    //! Specify the FROM table
    /** @param table the table name
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& from(const std::string& table);

    //! Add an INNER JOIN
    /** @param table the table to join
        @param condition the join condition (e.g., "users.id = orders.user_id")
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& join(const std::string& table, const std::string& condition);

    //! Add a LEFT JOIN
    /** @param table the table to join
        @param condition the join condition
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& leftJoin(const std::string& table, const std::string& condition);

    //! Add a RIGHT JOIN
    /** @param table the table to join
        @param condition the join condition
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& rightJoin(const std::string& table, const std::string& condition);

    //! Add column/value pairs for INSERT
    /** @param columns list of column names
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& columns(const std::vector<std::string>& columns);

    //! Add placeholder values for INSERT
    /** @param count number of placeholders (uses ?)
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& values(size_t count);

    //! Add SET clause for UPDATE
    /** @param column the column to set
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& set(const std::string& column);

    //! Add multiple SET clauses for UPDATE
    /** @param columns list of columns to set
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& set(const std::vector<std::string>& columns);

    //! Add WHERE clause
    /** @param column the column name
        @param op the operator (=, <>, <, >, <=, >=, LIKE, IN, etc.)
        @param placeholder use ? as placeholder (default true)
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& where(const std::string& column, const std::string& op, bool placeholder = true);

    //! Add WHERE clause with literal value
    /** @param column the column name
        @param op the operator
        @param value the literal value
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& whereLiteral(const std::string& column, const std::string& op, const std::string& value);

    //! Add AND condition
    /** @param column the column name
        @param op the operator
        @param placeholder use ? as placeholder
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& andWhere(const std::string& column, const std::string& op, bool placeholder = true);

    //! Add OR condition
    /** @param column the column name
        @param op the operator
        @param placeholder use ? as placeholder
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& orWhere(const std::string& column, const std::string& op, bool placeholder = true);

    //! Add WHERE IS NULL condition
    /** @param column the column name
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& whereNull(const std::string& column);

    //! Add WHERE IS NOT NULL condition
    /** @param column the column name
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& whereNotNull(const std::string& column);

    //! Add WHERE IN clause
    /** @param column the column name
        @param count number of placeholders
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& whereIn(const std::string& column, size_t count);

    //! Add ORDER BY clause
    /** @param column the column to order by
        @param ascending true for ASC, false for DESC
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& orderBy(const std::string& column, bool ascending = true);

    //! Add GROUP BY clause
    /** @param column the column to group by
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& groupBy(const std::string& column);

    //! Add HAVING clause
    /** @param condition the having condition
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& having(const std::string& condition);

    //! Add LIMIT clause
    /** @param count maximum number of rows
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& limit(size_t count);

    //! Add OFFSET clause
    /** @param count number of rows to skip
        @return reference to this builder for chaining
     */
    DLLLOCAL QueryBuilder& offset(size_t count);

    //! Build the final SQL string
    /** @return the constructed SQL query
     */
    DLLLOCAL std::string build() const;

    //! Get number of placeholders in the query
    /** @return number of ? placeholders
     */
    DLLLOCAL size_t getPlaceholderCount() const { return placeholderCount; }

private:
    enum class QueryType {
        NONE,
        SELECT,
        INSERT,
        UPDATE,
        DELETE
    };

    QueryType queryType = QueryType::NONE;
    bool distinct = false;
    std::vector<std::string> selectColumns;
    std::string tableName;
    std::vector<std::string> insertColumns;
    std::vector<std::string> setColumns;
    std::vector<std::string> joinClauses;
    std::vector<std::string> whereClauses;
    std::vector<std::string> orderByClauses;
    std::vector<std::string> groupByClauses;
    std::string havingClause;
    size_t limitValue = 0;
    size_t offsetValue = 0;
    bool hasLimit = false;
    bool hasOffset = false;
    size_t placeholderCount = 0;
    size_t valueCount = 0;
};

} // namespace odbc

#endif // _QORE_MODULE_ODBC_QUERYBUILDER_H
