#include "amigrations.h"
#include "aresult.h"
#include "atransaction.h"

#include <QFile>
#include <QTextStream>
#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(ASQL_MIG, "asql.migrations", QtInfoMsg)

class AMigrationsPrivate
{
public:
    QString name;
    ADatabase db;
    QString data;
    QMap<int, QString> up;
    QMap<int, QString> down;
    int active = -1;
    int latest = -1;
};

AMigrations::AMigrations(QObject *parent)
    : QObject(parent)
    , d_ptr(new AMigrationsPrivate)
{

}

AMigrations::~AMigrations()
{
    delete d_ptr;
}

void AMigrations::load(const ADatabase &db, const QString &name)
{
    d_ptr->name = name;
    d_ptr->db = db;
    d_ptr->db.exec(QStringLiteral(R"V0G0N(
                                  CREATE TABLE IF NOT EXISTS asql_migrations (
                                  name    text primary key,
                                  version bigint not null check (version >= 0)
                                  )
                                  )V0G0N"),
                   [=] (AResult &result) {
        if (result.error()) {
            qDebug(ASQL_MIG) << "Create migrations table" << result.errorString();
        }

        d_ptr->db.exec(QStringLiteral("SELECT version FROM asql_migrations WHERE name=$1"),
                       {name}, [=] (AResult &result2) {
            if (result2.error()) {
                Q_EMIT ready(true, result2.errorString());
                return;
            }

            if (result2.next()) {
                d_ptr->active = result2.value(0).toInt();
            } else {
                d_ptr->active = 0;
            }
            Q_EMIT ready(false, QString());
        }, this);
    }, this);
}

int AMigrations::active() const
{
    Q_D(const AMigrations);
    return d->active;
}

int AMigrations::latest() const
{
    Q_D(const AMigrations);
    return d->latest;
}

bool AMigrations::fromFile(const QString &filename)
{
    QFile file(filename);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        fromString(QString::fromUtf8(file.readAll()));
        return true;
    }
    return false;
}

void AMigrations::fromString(const QString &text)
{
    Q_D(AMigrations);
    QMap<int, QString> up;
    QMap<int, QString> down;

    int version = 0;
    int latest = -1;
    bool upWay;
    d_ptr->data = text;

    QTextStream stream(&d_ptr->data);
    QRegularExpression re(QStringLiteral("^\\s*--\\s*(\\d+)\\s*(up|down)"), QRegularExpression::CaseInsensitiveOption);
    QString line;
    while (!stream.atEnd()) {
        stream.readLineInto(&line);
        qDebug(ASQL_MIG) << "MIG LINE" << line << upWay << version;
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            const QStringRef way = match.capturedRef(2);
            qDebug(ASQL_MIG) << "CAPTURE" << way << match.capturedRef(1).toInt();
            if (way.compare(QStringLiteral("up"), Qt::CaseInsensitive) == 0) {
                upWay = true;
                version = match.capturedRef(1).toInt();
            } else if (way.compare(QStringLiteral("down"), Qt::CaseInsensitive) == 0) {
                upWay = false;
                version = match.capturedRef(1).toInt();
            } else {
                version = 0;
            }
            latest = qMax(latest, version);
        } else if (version) {
            if (upWay) {
                qDebug(ASQL_MIG) << "UP" << version << line;
                up[version].append(line + QLatin1Char('\n'));
            } else {
                qDebug(ASQL_MIG) << "DOWN" << version << line;
                down[version].append(line + QLatin1Char('\n'));
            }
        }
    }

    d->latest = latest;
    d->up = up;
    d->down = down;
}

QString AMigrations::sqlFor(int versionFrom, int versionTo) const
{
    Q_D(const AMigrations);
    QString ret;

    if (versionFrom < versionTo) {
        // up
        auto it = d->up.constBegin();
        while (it != d->up.constEnd()) {
            if (it.key() <= versionTo && it.key() > versionFrom) {
                ret.append(it.value());
            }
            ++it;
        }
    } else {
        // down
        QStringList downList;
        auto it = d->down.constBegin();
        while (it != d->down.constEnd()) {
            if (it.key() > versionTo && it.key() <= versionFrom) {
                downList.prepend(it.value());
            }
            ++it;
        }

        ret = downList.join(QLatin1Char(' '));
    }

    return ret;
}

void AMigrations::migrate(std::function<void(bool, const QString &)> cb, bool dryRun)
{
    Q_D(AMigrations);
    migrate(d->latest, cb, dryRun);
}

void AMigrations::migrate(int version, std::function<void(bool, const QString &)> cb, bool dryRun)
{
    Q_D(AMigrations);
    if (version < 0) {
        if (cb) {
            cb(true, QStringLiteral("Failed to migrate: invalid target version"));
        }
        qWarning(ASQL_MIG) << "Failed to migrate: invalid target version" << version;
        return;
    }

    ATransaction t(d->db);
    t.begin([=] (AResult &result) {
        if (result.error()) {
            cb(true, result.errorString());
            return;
        }

        d->db.exec(QStringLiteral("SELECT version FROM asql_migrations WHERE name=$1 FOR UPDATE"),
        {d->name}, [=] (AResult &result) {
            if (result.error()) {
                cb(true, result.errorString());
                return;
            }

            int active = 0;
            if (result.next()) {
                active = result.value(0).toInt();
            }

            if (active > latest()) {
                cb(true, QStringLiteral("Active version %1 is greater than the latest version %2").arg(active).arg(latest()));
                return;
            }

            const QString query = sqlFor(active, version);
            if (query.isEmpty()) {
                if (cb) {
                    cb(false, QStringLiteral("Nothing to migrate to"));
                }
                return;
            }

            d->db.exec(query, [=] (AResult &result) {
                if (result.error()) {
                    if (cb) {
                        cb(true, result.errorString());
                    }
                    return;
                }
            }, this);

            d->db.exec(QStringLiteral(R"V0G0N(
                                      INSERT INTO asql_migrations VALUES ($1, $2)
                                      ON CONFLICT (name) DO UPDATE
                                      SET version=EXCLUDED.version
                                      RETURNING version
                                      )V0G0N"),
            {d->name, version}, [=] (AResult &result) {
                if (result.error()) {
                    if (cb) {
                        cb(true, result.errorString());
                    }
                    return;
                }

                auto tAction = [=] (AResult &result) {
                    if (result.error()) {
                        if (cb) {
                            cb(true, result.errorString());
                        }
                    } else {
                        if (cb) {
                            cb(false, QString());
                        }
                    }
                };

                if (!dryRun) {
                    ATransaction(t).commit(tAction, this);
                } else {
                    ATransaction(t).rollback(tAction, this);
                }
            }, this);
        }, this);
    });

}
