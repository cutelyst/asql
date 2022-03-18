/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "apreparedquery.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ASQL_PQ, "asql.prepared_query", QtInfoMsg)

using namespace ASql;

APreparedQuery::APreparedQuery()
{

}

static QByteArray identificationCounter() {
    QByteArray ret;
    static QBasicAtomicInt qPreparedStmtCount = Q_BASIC_ATOMIC_INITIALIZER(0);
    ret = "asql_" + QByteArray::number(qPreparedStmtCount.fetchAndAddRelaxed(1) + 1, 16);
    qDebug(ASQL_PQ) << "Created prepared query identifier" << ret;
    return ret;
}

APreparedQuery::APreparedQuery(const QString &query)
    : m_query(query.toUtf8())
    , m_identification(identificationCounter())
{
}

APreparedQuery::APreparedQuery(QStringView query)
    : m_query(query.toUtf8())
    , m_identification(identificationCounter())
{
}

APreparedQuery::APreparedQuery(const QString &query, const QString &identification)
    : m_query(query.toUtf8())
    , m_identification(identification.toUtf8())
{

}

QByteArray APreparedQuery::query() const
{
    return m_query;
}

QByteArray APreparedQuery::identification() const
{
    return m_identification;
}
