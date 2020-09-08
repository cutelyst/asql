#include "apreparedquery.h"

APreparedQuery::APreparedQuery()
{

}

APreparedQuery::APreparedQuery(const QString &query)
    : m_query(query)
{
    static QBasicAtomicInt qPreparedStmtCount = Q_BASIC_ATOMIC_INITIALIZER(0);
    m_identification = QLatin1String("asql_") + QString::number(qPreparedStmtCount.fetchAndAddRelaxed(1) + 1, 16);
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
