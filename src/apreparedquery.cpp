/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "apreparedquery.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ASQL_PQ, "asql.prepared_query", QtInfoMsg)

APreparedQuery::APreparedQuery()
{

}

APreparedQuery::APreparedQuery(const QString &query)
    : m_query(query)
{
    static QBasicAtomicInt qPreparedStmtCount = Q_BASIC_ATOMIC_INITIALIZER(0);
    m_identification = QLatin1String("asql_") + QString::number(qPreparedStmtCount.fetchAndAddRelaxed(1) + 1, 16);
    qDebug(ASQL_PQ) << "Created prepared query identifier" << m_identification;
}

APreparedQuery::APreparedQuery(const QString &query, const QString &identification)
    : m_query(query)
    , m_identification(identification)
{

}

QString APreparedQuery::query() const
{
    return m_query;
}

QString APreparedQuery::identification() const
{
    return m_identification;
}
