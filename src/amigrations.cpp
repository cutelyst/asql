/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "amigrations.h"
#include "aresult.h"
#include "atransaction.h"

#include <QFile>
#include <QTextStream>
#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(ASQL_MIG, "asql.migrations", QtInfoMsg)

namespace ASql {

class AMigrationsPrivate
{
public:
    std::pair<int, QString> nextQuery(int versionFrom, int versionTo) const;

    QString name;
    ADatabase db;
    QString data;
    QMap<int, QString> up;
    QMap<int, QString> down;
    int active = -1;
    int latest = -1;
};

}

using namespace ASql;

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
    d_ptr->db.exec(uR"V0G0N(
CREATE TABLE IF NOT EXISTS public.asql_migrations (
name text primary key,
version bigint not null check (version >= 0)
)
)V0G0N",
                   [=] (AResult &result) {
        if (result.error()) {
            qDebug(ASQL_MIG) << "Create migrations table" << result.errorString();
        }

        d_ptr->db.exec(u"SELECT version FROM public.asql_migrations WHERE name=$1",
                       {name}, [=] (AResult &result2) {
            if (result2.error()) {
                Q_EMIT ready(true, result2.errorString());
                return;
            }

            if (result2.size()) {
                d_ptr->active = result2.constBegin().value(0).toInt();
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
    } else {
        qCritical(ASQL_MIG) << "Failed to open migrations" << filename << file.errorString();
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
    bool upWay = true;
    d_ptr->data = text;

    QTextStream stream(&d_ptr->data);
    QRegularExpression re(QStringLiteral("^\\s*--\\s*(\\d+)\\s*(up|down)"), QRegularExpression::CaseInsensitiveOption);
    QString line;
    while (!stream.atEnd()) {
        stream.readLineInto(&line);
        qDebug(ASQL_MIG) << "MIG LINE" << line << upWay << version;
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 2))
            const QStringView way = match.capturedView(2);
            qDebug(ASQL_MIG) << "CAPTURE" << way << match.capturedView(1).toInt();
            if (way.compare(u"up", Qt::CaseInsensitive) == 0) {
                upWay = true;
                version = match.capturedView(1).toInt();
            } else if (way.compare(u"down", Qt::CaseInsensitive) == 0) {
                upWay = false;
                version = match.capturedView(1).toInt();
            } else {
                version = 0;
            }
#else
            const QStringRef way = match.capturedRef(2);
            qDebug(ASQL_MIG) << "CAPTURE" << way << match.capturedRef(1).toInt();
            if (way.compare(QLatin1String("up"), Qt::CaseInsensitive) == 0) {
                upWay = true;
                version = match.capturedRef(1).toInt();
            } else if (way.compare(QLatin1String("down"), Qt::CaseInsensitive) == 0) {
                upWay = false;
                version = match.capturedRef(1).toInt();
            } else {
                version = 0;
            }
#endif
            if (upWay && up.contains(version)) {
                qFatal("Duplicated UP version %d", version);
            }
            if (!upWay && down.contains(version)) {
                qFatal("Duplicated DOWN version %d", version);
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
    return sqlListFor(versionFrom, versionTo).join(QLatin1Char('\n'));
}

QStringList AMigrations::sqlListFor(int versionFrom, int versionTo) const
{
    QStringList ret;
    Q_D(const AMigrations);

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
        auto it = d->down.constBegin();
        while (it != d->down.constEnd()) {
            if (it.key() > versionTo && it.key() <= versionFrom) {
                ret.prepend(it.value());
            }
            ++it;
        }
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

        d->db.exec(QStringLiteral("SELECT version FROM public.asql_migrations WHERE name=$1 FOR UPDATE"),
        {d->name}, [=] (AResult &result) {
            if (result.error()) {
                cb(true, result.errorString());
                return;
            }

            int active = 0;
            if (result.size()) {
                active = result[0][0].toInt();
            }

            if (active > latest()) {
                cb(true, QStringLiteral("Current version %1 is greater than the latest version %2").arg(active).arg(latest()));
                return;
            }

            const std::pair<int, QString> query = d->nextQuery(active, version);
            qDebug(ASQL_MIG) << "Migrate current version" << active << "ẗo" << query.first << "target version" << version << !query.second.isEmpty();
            if (query.second.isEmpty()) {
                if (cb) {
                    cb(false, QStringLiteral("Done."));
                }
                return;
            }

            d->db.exec(query.second, [=] (AResult &result) {
                if (result.error()) {
                    if (cb) {
                        cb(true, result.errorString());
                    }
                } else if (result.lastResulSet()) {
                    if (!dryRun) {
                        ATransaction(t).commit([=] (AResult &result) {
                            if (result.error()) {
                                if (cb) {
                                    cb(true, result.errorString());
                                }
                            } else {
                                qInfo(ASQL_MIG) << "Migrated from" << active << "to" << query.first;
                                if (dryRun) {
                                    if (cb) {
                                        cb(false, QStringLiteral("Done."));
                                    }
                                } else {
                                    migrate(version, cb, dryRun);
                                }
                            }
                        }, this);
                    } else {
                        ATransaction(t).rollback([=] (AResult &result) {
                            if (cb) {
                                cb(true, result.errorString());
                            }
                        }, this);
                    }
                }
            }, this);
        }, this);
    });
}

std::pair<int, QString> AMigrationsPrivate::nextQuery(int versionFrom, int versionTo) const
{
    std::pair<int, QString> ret;

    if (versionFrom < versionTo) {
        // up
        auto it = up.constBegin();
        while (it != up.constEnd()) {
            if (it.key() <= versionTo && it.key() > versionFrom) {
                ret = {
                    it.key(),
                    QStringLiteral(R"V0G0N(
                    INSERT INTO public.asql_migrations VALUES ('%1', %2)
                    ON CONFLICT (name) DO UPDATE
                    SET version=EXCLUDED.version
                    RETURNING version;
                    %3
                    )V0G0N").arg(name).arg(it.key()).arg(it.value())
                };
                break;
            }
            ++it;
        }
    } else {
        // down
        auto it = down.constBegin();
        while (it != down.constEnd()) {
            if (it.key() > versionTo && it.key() <= versionFrom) {
                ret = {
                    it.key() - 1,
                    QStringLiteral(R"V0G0N(
                    INSERT INTO public.asql_migrations VALUES ('%1', %2)
                    ON CONFLICT (name) DO UPDATE
                    SET version=EXCLUDED.version
                    RETURNING version;
                    %3
                    )V0G0N").arg(name).arg(it.key() - 1).arg(it.value())
                };
            }
            ++it;
        }
    }

    return ret;
}

#include "moc_amigrations.cpp"
