#include <QCoreApplication>
#include <QLoggingCategory>

#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QUuid>
#include <QUrl>

#include "../../src/apool.h"
#include "../../src/adatabase.h"
#include "../../src/atransaction.h"
#include "../../src/aresult.h"
#include "../../src/amigrations.h"
#include "../../src/acache.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    auto obj = new QObject;
    {
        ADatabase db(QStringLiteral("postgres:///?target_session_attrs=read-write"));
        db.open([db, obj] (bool ok, const QString &status) {
            qDebug() << "OPEN value" << ok << status;

            ADatabase(db).exec(QStringLiteral("SELECT now()"), [db] (AResult &result) {
                if (result.error()) {
                    qDebug() << "SELECT error" << result.errorString();
                    return;
                }

                if (result.next()) {
                    qDebug() << "SELECT value" << result.value(0);
                }
            }, obj);
        });
    }

    QTimer::singleShot(2000, [=] {
        qDebug() << "Delete Obj";
        delete obj;
    });

    app.exec();
}
