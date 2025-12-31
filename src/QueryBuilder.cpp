/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QueryBuilder.cpp

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

#include "QueryBuilder.h"

namespace odbc {

QueryBuilder::QueryBuilder() {
}

QueryBuilder& QueryBuilder::reset() {
    queryType = QueryType::NONE;
    distinct = false;
    selectColumns.clear();
    tableName.clear();
    insertColumns.clear();
    setColumns.clear();
    joinClauses.clear();
    whereClauses.clear();
    orderByClauses.clear();
    groupByClauses.clear();
    havingClause.clear();
    limitValue = 0;
    offsetValue = 0;
    hasLimit = false;
    hasOffset = false;
    placeholderCount = 0;
    valueCount = 0;
    return *this;
}

QueryBuilder& QueryBuilder::select(const std::vector<std::string>& columns) {
    queryType = QueryType::SELECT;
    distinct = false;
    selectColumns = columns;
    return *this;
}

QueryBuilder& QueryBuilder::selectDistinct(const std::vector<std::string>& columns) {
    queryType = QueryType::SELECT;
    distinct = true;
    selectColumns = columns;
    return *this;
}

QueryBuilder& QueryBuilder::selectAll() {
    queryType = QueryType::SELECT;
    distinct = false;
    selectColumns.clear();
    selectColumns.push_back("*");
    return *this;
}

QueryBuilder& QueryBuilder::insertInto(const std::string& table) {
    queryType = QueryType::INSERT;
    tableName = table;
    return *this;
}

QueryBuilder& QueryBuilder::update(const std::string& table) {
    queryType = QueryType::UPDATE;
    tableName = table;
    return *this;
}

QueryBuilder& QueryBuilder::deleteFrom() {
    queryType = QueryType::DELETE;
    return *this;
}

QueryBuilder& QueryBuilder::from(const std::string& table) {
    tableName = table;
    return *this;
}

QueryBuilder& QueryBuilder::join(const std::string& table, const std::string& condition) {
    joinClauses.push_back("INNER JOIN " + table + " ON " + condition);
    return *this;
}

QueryBuilder& QueryBuilder::leftJoin(const std::string& table, const std::string& condition) {
    joinClauses.push_back("LEFT JOIN " + table + " ON " + condition);
    return *this;
}

QueryBuilder& QueryBuilder::rightJoin(const std::string& table, const std::string& condition) {
    joinClauses.push_back("RIGHT JOIN " + table + " ON " + condition);
    return *this;
}

QueryBuilder& QueryBuilder::columns(const std::vector<std::string>& cols) {
    insertColumns = cols;
    return *this;
}

QueryBuilder& QueryBuilder::values(size_t count) {
    valueCount = count;
    placeholderCount += count;
    return *this;
}

QueryBuilder& QueryBuilder::set(const std::string& column) {
    setColumns.push_back(column);
    placeholderCount++;
    return *this;
}

QueryBuilder& QueryBuilder::set(const std::vector<std::string>& cols) {
    for (const auto& col : cols) {
        setColumns.push_back(col);
        placeholderCount++;
    }
    return *this;
}

QueryBuilder& QueryBuilder::where(const std::string& column, const std::string& op, bool placeholder) {
    std::string clause = column + " " + op;
    if (placeholder) {
        clause += " ?";
        placeholderCount++;
    }
    whereClauses.push_back(clause);
    return *this;
}

QueryBuilder& QueryBuilder::whereLiteral(const std::string& column, const std::string& op, const std::string& value) {
    whereClauses.push_back(column + " " + op + " " + value);
    return *this;
}

QueryBuilder& QueryBuilder::andWhere(const std::string& column, const std::string& op, bool placeholder) {
    std::string clause = "AND " + column + " " + op;
    if (placeholder) {
        clause += " ?";
        placeholderCount++;
    }
    whereClauses.push_back(clause);
    return *this;
}

QueryBuilder& QueryBuilder::orWhere(const std::string& column, const std::string& op, bool placeholder) {
    std::string clause = "OR " + column + " " + op;
    if (placeholder) {
        clause += " ?";
        placeholderCount++;
    }
    whereClauses.push_back(clause);
    return *this;
}

QueryBuilder& QueryBuilder::whereNull(const std::string& column) {
    whereClauses.push_back(column + " IS NULL");
    return *this;
}

QueryBuilder& QueryBuilder::whereNotNull(const std::string& column) {
    whereClauses.push_back(column + " IS NOT NULL");
    return *this;
}

QueryBuilder& QueryBuilder::whereIn(const std::string& column, size_t count) {
    std::ostringstream oss;
    oss << column << " IN (";
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) oss << ", ";
        oss << "?";
        placeholderCount++;
    }
    oss << ")";
    whereClauses.push_back(oss.str());
    return *this;
}

QueryBuilder& QueryBuilder::orderBy(const std::string& column, bool ascending) {
    orderByClauses.push_back(column + (ascending ? " ASC" : " DESC"));
    return *this;
}

QueryBuilder& QueryBuilder::groupBy(const std::string& column) {
    groupByClauses.push_back(column);
    return *this;
}

QueryBuilder& QueryBuilder::having(const std::string& condition) {
    havingClause = condition;
    return *this;
}

QueryBuilder& QueryBuilder::limit(size_t count) {
    limitValue = count;
    hasLimit = true;
    return *this;
}

QueryBuilder& QueryBuilder::offset(size_t count) {
    offsetValue = count;
    hasOffset = true;
    return *this;
}

std::string QueryBuilder::build() const {
    std::ostringstream sql;

    switch (queryType) {
        case QueryType::SELECT: {
            sql << "SELECT ";
            if (distinct) {
                sql << "DISTINCT ";
            }
            for (size_t i = 0; i < selectColumns.size(); ++i) {
                if (i > 0) sql << ", ";
                sql << selectColumns[i];
            }
            sql << " FROM " << tableName;
            break;
        }

        case QueryType::INSERT: {
            sql << "INSERT INTO " << tableName;
            if (!insertColumns.empty()) {
                sql << " (";
                for (size_t i = 0; i < insertColumns.size(); ++i) {
                    if (i > 0) sql << ", ";
                    sql << insertColumns[i];
                }
                sql << ")";
            }
            if (valueCount > 0) {
                sql << " VALUES (";
                for (size_t i = 0; i < valueCount; ++i) {
                    if (i > 0) sql << ", ";
                    sql << "?";
                }
                sql << ")";
            }
            break;
        }

        case QueryType::UPDATE: {
            sql << "UPDATE " << tableName << " SET ";
            for (size_t i = 0; i < setColumns.size(); ++i) {
                if (i > 0) sql << ", ";
                sql << setColumns[i] << " = ?";
            }
            break;
        }

        case QueryType::DELETE: {
            sql << "DELETE FROM " << tableName;
            break;
        }

        default:
            return "";
    }

    // Add JOINs
    for (const auto& join : joinClauses) {
        sql << " " << join;
    }

    // Add WHERE
    if (!whereClauses.empty()) {
        sql << " WHERE ";
        for (size_t i = 0; i < whereClauses.size(); ++i) {
            if (i > 0 && whereClauses[i].substr(0, 3) != "AND" && whereClauses[i].substr(0, 2) != "OR") {
                sql << " AND ";
            } else if (i > 0) {
                sql << " ";
            }
            sql << whereClauses[i];
        }
    }

    // Add GROUP BY
    if (!groupByClauses.empty()) {
        sql << " GROUP BY ";
        for (size_t i = 0; i < groupByClauses.size(); ++i) {
            if (i > 0) sql << ", ";
            sql << groupByClauses[i];
        }
    }

    // Add HAVING
    if (!havingClause.empty()) {
        sql << " HAVING " << havingClause;
    }

    // Add ORDER BY
    if (!orderByClauses.empty()) {
        sql << " ORDER BY ";
        for (size_t i = 0; i < orderByClauses.size(); ++i) {
            if (i > 0) sql << ", ";
            sql << orderByClauses[i];
        }
    }

    // Add LIMIT
    if (hasLimit) {
        sql << " LIMIT " << limitValue;
    }

    // Add OFFSET
    if (hasOffset) {
        sql << " OFFSET " << offsetValue;
    }

    return sql.str();
}

} // namespace odbc
