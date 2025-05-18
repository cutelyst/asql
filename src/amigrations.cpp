/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "amigrations.h"

#include "aresult.h"
#include "atransaction.h"

#include <QElapsedTimer>
#include <QFile>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTextStream>

Q_LOGGING_CATEGORY(ASQL_MIG, "asql.migrations", QtInfoMsg)

using namespace Qt::StringLiterals;

namespace ASql {

class AMigrationsPrivate
{
public:
    struct MigQuery {
        QString versionQuery;
        QString query;
        int version        = 0;
        bool noTransaction = false;
    };

    AMigrationsPrivate::MigQuery nextQuery(int versionFrom, int versionTo) const;

    QString name;
    ADatabase db;
    ADatabase noTransactionDB;
    QString data;

    QMap<int, MigQuery> up;
    QMap<int, MigQuery> down;
    int active = -1;
    int latest = -1;
};

} // namespace ASql

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

ACoroTerminator AMigrations::load(ADatabase db, QString name, ADatabase noTransactionDB)
{
    Q_D(AMigrations);
    d->name            = name;
    d->db              = db;
    d->noTransactionDB = noTransactionDB;

    auto result = co_await d->db.coExec(uR"V0G0N(
CREATE TABLE IF NOT EXISTS asql_migrations (
name text primary key,
version bigint not null check (version >= 0)
)
)V0G0N",
                                        this);
    if (!result) {
        qDebug(ASQL_MIG) << "Create migrations table" << result.error();
    }

    const QString query = [db] {
        if (db.driverName() == u"sqlite") {
            return u"SELECT version FROM asql_migrations WHERE name = ?"_s;
        }

        return u"SELECT version FROM asql_migrations WHERE name = $1"_s;
    }();

    result = co_await d->db.coExec(query,
                                   {
                                       name,
                                   },
                                   this);
    if (!result) {
        Q_EMIT ready(true, result.error());
        co_return;
    }

    auto row = result->begin();
    if (row != result->end()) {
        d->active = row.value(0).toInt();
    } else {
        d->active = 0;
    }
    Q_EMIT ready(false, QString{});
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
    QMap<int, AMigrationsPrivate::MigQuery> up;
    QMap<int, AMigrationsPrivate::MigQuery> down;

    int version        = 0;
    int latest         = -1;
    bool upWay         = true;
    bool noTransaction = false;
    d->data            = text;

    QTextStream stream(&d->data);
    static QRegularExpression re(u"^\\s*--\\s*(\\d+)\\s*(up|down)\\s*(no-transaction)?"_s,
                                 QRegularExpression::CaseInsensitiveOption);
    QString line;
    while (!stream.atEnd()) {
        stream.readLineInto(&line);
        qDebug(ASQL_MIG) << "MIG LINE" << line << upWay << version;
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            const QStringView way = match.capturedView(2);
            noTransaction         = !match.capturedView(3).isNull();
            qDebug(ASQL_MIG) << "CAPTURE" << way << match.capturedView(1).toInt() << noTransaction;
            if (way.compare(u"up", Qt::CaseInsensitive) == 0) {
                upWay   = true;
                version = match.capturedView(1).toInt();
            } else if (way.compare(u"down", Qt::CaseInsensitive) == 0) {
                upWay   = false;
                version = match.capturedView(1).toInt();
            } else {
                version = 0;
            }

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
                auto &mig   = up[version];
                mig.version = version;
                mig.query.append(line + u'\n');
                mig.noTransaction = noTransaction;
            } else {
                qDebug(ASQL_MIG) << "DOWN" << version << line;
                auto &mig   = down[version];
                mig.version = version;
                mig.query.append(line + u'\n');
                mig.noTransaction = noTransaction;
            }
        }
    }

    d->latest = latest;
    d->up     = up;
    d->down   = down;
}

QString AMigrations::sqlFor(int versionFrom, int versionTo) const
{
    return sqlListFor(versionFrom, versionTo).join(u'\n');
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
                ret.append(it.value().query);
            }
            ++it;
        }
    } else {
        // down
        auto it = d->down.constBegin();
        while (it != d->down.constEnd()) {
            if (it.key() > versionTo && it.key() <= versionFrom) {
                ret.prepend(it.value().query);
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

ACoroTerminator AMigrations::migrate(int targetVersion,
                                     std::function<void(bool, const QString &)> cb,
                                     bool dryRun)
{
    Q_D(AMigrations);

    QElapsedTimer elapsed;
    elapsed.start();

    if (targetVersion < 0) {
        if (cb) {
            cb(true, u"Failed to migrate: invalid target version"_s);
        }
        qWarning(ASQL_MIG) << "Failed to migrate: invalid target version" << targetVersion;
        co_return;
    }

    auto t = co_await d->db.coBegin();
    if (!t) {
        cb(true, t.error());
        co_return;
    }

    const QString query = [db = d->db] {
        if (db.driverName() == u"sqlite") {
            return u"SELECT version FROM asql_migrations WHERE name = ?"_s;
        }

        return u"SELECT version FROM asql_migrations WHERE name = $1 FOR UPDATE"_s;
    }();

    auto result = co_await d->db.coExec(query,
                                        {
                                            d->name,
                                        },
                                        this);
    if (!result) {
        cb(true, result.error());
        co_return;
    }

    int active = 0;
    auto row   = result->begin();
    if (row != result->end()) {
        active = row[0].toInt();
    }

    if (active > latest()) {
        cb(true,
           u"Current version %1 is greater than the latest version %2"_s.arg(active).arg(latest()));
        co_return;
    }

    const auto migration = d->nextQuery(active, targetVersion);
    if (migration.query.isEmpty()) {
        if (cb) {
            cb(false, u"Done."_s);
        }
        co_return;
    }

    qDebug(ASQL_MIG) << "Migrating current version" << active << "ẗo" << migration.version
                     << "target version" << targetVersion << "transaction"
                     << !migration.noTransaction << "has query" << !migration.query.isEmpty();
    if (migration.noTransaction) {
        qWarning(ASQL_MIG) << "Migrating from" << active << "to" << migration.version
                           << "without a transaction, might fail to update the version.";

        if (dryRun) {
            qCritical(ASQL_MIG) << "Cannot dry run a migration that requires no transaction: "
                                << migration.version;
            if (cb) {
                cb(true, u"Done."_s);
            }
            co_return;
        }

        result = co_await d->db.coExec(migration.versionQuery, this);
        if (!result) {
            qCritical(ASQL_MIG) << "Failed to update version" << result.error();
            cb(true, result.error());
            co_return;
        }
    }

    ADatabase db   = migration.noTransaction ? d->noTransactionDB : d->db;
    auto awaitable = db.execMulti(
        migration.noTransaction ? migration.query : migration.versionQuery + migration.query, this);
    while (true) {
        result = co_await awaitable;
        if (!result) {
            qCritical(ASQL_MIG) << "Failed to migrate"
                                << (active < migration.version ? migration.version
                                                               : migration.version + 1)
                                << (active < migration.version ? "up" : "down");
            if (cb) {
                cb(true, result.error());
            }

            co_return;
        } else if (result->lastResultSet()) {
            if (migration.noTransaction || !dryRun) {
                result = co_await t->coCommit(this);
                if (!result) {
                    if (cb) {
                        cb(true, result.error());
                    }
                }

                qInfo(ASQL_MIG) << "Migrated from" << active << "to" << migration.version << "in"
                                << elapsed.elapsed() << "ms";
                if (dryRun) {
                    if (cb) {
                        cb(false, u"Done."_s);
                    }
                } else {
                    migrate(targetVersion, cb, dryRun);
                }
            } else {
                if (cb) {
                    cb(true, u"Rolling back"_s);
                }
            }

            co_return;
        }
    }
}

AMigrationsPrivate::MigQuery AMigrationsPrivate::nextQuery(int versionFrom, int versionTo) const
{
    AMigrationsPrivate::MigQuery ret;

    QString query = uR"V0G0N(
INSERT INTO asql_migrations
    (name, version)
VALUES
    ('%1', %2)
ON CONFLICT (name) DO UPDATE
SET version = EXCLUDED.version
RETURNING version
)V0G0N"_s;

    if (versionFrom < versionTo) {
        // up
        auto it = up.constBegin();
        while (it != up.constEnd()) {
            if (it.key() <= versionTo && it.key() > versionFrom) {
                ret = {query.arg(name).arg(it.key()),
                       it.value().query,
                       it.key(),
                       it.value().noTransaction};
                break;
            }
            ++it;
        }
    } else {
        // down
        auto it = down.constBegin();
        while (it != down.constEnd()) {
            if (it.key() > versionTo && it.key() <= versionFrom) {
                ret = {query.arg(name).arg(it.key() - 1),
                       it.value().query,
                       it.key() - 1,
                       it.value().noTransaction};
            }
            ++it;
        }
    }

    return ret;
}

#include "moc_amigrations.cpp"
