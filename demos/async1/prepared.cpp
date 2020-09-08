#include <QCoreApplication>
#include <QLoggingCategory>
#include <QElapsedTimer>

#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QUuid>
#include <QUrl>

#include "../../src/apool.h"
#include "../../src/adatabase.h"
#include "../../src/atransaction.h"
#include "../../src/apreparedquery.h"
#include "../../src/aresult.h"
#include "../../src/amigrations.h"
#include "../../src/acache.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    APool::addDatabase(QStringLiteral("postgres:///?target_session_attrs=read-write"));
    APool::setDatabaseMaxIdleConnections(10);

    auto db = APool::database();

    static APreparedQuery query(QStringLiteral("SELECT now()"));
    db.execPrepared(query, [=] (AResult &result) {
        if (result.error()) {
            qDebug() << "SELECT error" << result.errorString();
            return;
        }

        if (result.next()) {
            qDebug() << "SELECT value" << result.value(0);
        }
    });

    db.execPrepared(query, [=] (AResult &result) {
        if (result.error()) {
            qDebug() << "SELECT error" << result.errorString();
            return;
        }

        if (result.next()) {
            qDebug() << "SELECT value" << result.value(0);
        }
    });

    static APreparedQuery query2(QStringLiteral("SELECT now(), $1"));
    db.execPrepared(query2, { qint64(12345) }, [=] (AResult &result) {
        if (result.error()) {
            qDebug() << "SELECT error" << result.errorString();
            return;
        }

        if (result.next()) {
            qDebug() << "SELECT value 2" << result.value(0) << result.value(1) << query2.identification();
        }
    });

    db.execPrepared(query2, { qint64(12345) }, [=] (AResult &result) {
        if (result.error()) {
            qDebug() << "SELECT error" << result.errorString();
            return;
        }

        if (result.next()) {
            qDebug() << "SELECT value 2" << result.value(0) << result.value(1) << query2.identification();
        }
    });

    auto loopFn = [=] {
        auto queryStatic = APreparedQueryLiteral("SELECT $1, now()");
        ADatabase(db).execPrepared(queryStatic,
                                   { qint64(12345) }, [=] (AResult &result) {
            if (result.error()) {
                qDebug() << "SELECT error" << result.errorString();
                return;
            }

            if (result.next()) {
                qDebug() << "SELECT value 3" << result.value(0) << result.value(1) << queryStatic.identification();
            }
        });
    };

    loopFn();
    loopFn();

    app.exec();
}
