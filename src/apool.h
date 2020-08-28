#ifndef APOOL_H
#define APOOL_H

#include <QObject>
#include <QUrl>

#include <adatabase.h>

#include <aqsqlexports.h>

class ASQL_EXPORT APool : public QObject
{
    Q_OBJECT
public:
    explicit APool(QObject *parent = nullptr);

    static const char *defaultConnection;

    /*!
     * \brief addDatabase connection to pool
     *
     * Every time database is called, a new database object is returned, unless an
     * idle connection (one that were previously dereferenced) is available.
     *
     * \param connectionInfo is a driver url such as postgresql://user:pass@host:port/dbname
     * \param connectionName is an identifier for such connections, for example "read-write" or "read-only-replicas"
     */
    static void addDatabase(const QString &connectionInfo, const QString &connectionName = QLatin1String(defaultConnection));
    static ADatabase database(const QString &connectionName = QLatin1String(defaultConnection));

    static void setDatabaseMaxIdleConnections(int max, const QString &connectionName = QLatin1String(defaultConnection));
};

#endif // APOOL_H
