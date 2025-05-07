/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "apreparedquery.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ASQL_PQ, "asql.prepared_query", QtInfoMsg)

using namespace ASql;

APreparedQuery::APreparedQuery() = default;

namespace {

int identificationCounter()
{
    int ret;
    static QBasicAtomicInt qPreparedStmtCount = Q_BASIC_ATOMIC_INITIALIZER(0);
    ret                                       = qPreparedStmtCount.fetchAndAddRelaxed(1) + 1;
    qDebug(ASQL_PQ) << "Created prepared query identifier" << ret;
    return ret;
}

} // namespace

APreparedQuery::APreparedQuery(QStringView query)
    : m_query(query.toUtf8())
    , m_identification(identificationCounter())
{
}

APreparedQuery::APreparedQuery(QUtf8StringView query)
    : m_query(query.data(), query.size())
    , m_identification(identificationCounter())
{
}

APreparedQuery::APreparedQuery(QStringView query, int identification)
    : m_query(query.toUtf8())
    , m_identification(identification)
{
}

APreparedQuery::APreparedQuery(QUtf8StringView query, int identification)
    : m_query(query.data(), query.size())
    , m_identification(identification)
{
}

QByteArray APreparedQuery::query() const
{
    return m_query;
}

int APreparedQuery::identification() const
{
    return m_identification;
}
