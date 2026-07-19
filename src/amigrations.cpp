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

namespace {

QString versionUpdateQuery(const ASql::ADatabase &db)
{
    if (db.driverName() == u"sqlite") {
        return uR"V0G0N(
INSERT INTO asql_migrations
    (name, version)
VALUES
    (?, ?)
ON CONFLICT (name) DO UPDATE
SET version = excluded.version
RETURNING version;
)V0G0N"_s;
    }

    return uR"V0G0N(
INSERT INTO asql_migrations
    (name, version)
VALUES
    ($1, $2)
ON CONFLICT (name) DO UPDATE
SET version = EXCLUDED.version
RETURNING version;
)V0G0N"_s;
}

} // namespace

namespace ASql {

class AMigrationsPrivate
{
public:
    struct MigQuery {
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

ACoroTerminator AMigrations::loadCoroutine(std::shared_ptr<ACoroData<void>> data,
                                           AMigrations *q,
                                           ADatabase db,
                                           QString name,
                                           ADatabase noTransactionDB)
{
    AMigrationsPrivate *d = q->d_ptr;
    d->name               = std::move(name);
    d->db                 = std::move(db);
    d->noTransactionDB    = std::move(noTransactionDB);

    if (d->db.driverName() == u"postgres") {
        auto result = co_await d->db.exec(u"SET search_path TO public", q);
        if (!result) {
            qDebug(ASQL_MIG) << "Failed to set search_path to public" << result.error();
            data->deliverDirect(std::unexpected(result.error()));
            co_return;
        }

        if (d->noTransactionDB.isValid()) {
            result = co_await d->noTransactionDB.exec(u"SET search_path TO public", q);
            if (!result) {
                qDebug(ASQL_MIG) << "Failed to set search_path to public on no-transaction db"
                                 << result.error();
                data->deliverDirect(std::unexpected(result.error()));
                co_return;
            }
        }
    }

    auto result = co_await d->db.exec(uR"V0G0N(
CREATE TABLE IF NOT EXISTS asql_migrations (
name text primary key,
version bigint not null check (version >= 0)
)
)V0G0N",
                                      q);
    if (!result) {
        qDebug(ASQL_MIG) << "Create migrations table" << result.error();
        data->deliverDirect(std::unexpected(result.error()));
        co_return;
    }

    const QString query = d->db.driverName() == u"sqlite"
                              ? u"SELECT version FROM asql_migrations WHERE name = ?"_s
                              : u"SELECT version FROM asql_migrations WHERE name = $1"_s;

    result = co_await d->db.exec(query,
                                 {
                                     d->name,
                                 },
                                 q);
    if (!result) {
        data->deliverDirect(std::unexpected(result.error()));
        co_return;
    }

    auto row = result->begin();
    if (row != result->end()) {
        d->active = row.value(0).toInt();
    } else {
        d->active = 0;
    }
    data->deliverDirect(std::expected<void, QString>{});
}

ACoroTerminator AMigrations::migrateCoroutine(std::shared_ptr<ACoroData<void>> data,
                                              AMigrations *q,
                                              int targetVersion,
                                              bool dryRun)
{
    AMigrationsPrivate *d = q->d_ptr;

    const int resolvedTarget = targetVersion < 0 ? q->latest() : targetVersion;
    if (resolvedTarget < 0) {
        data->deliverDirect(
            std::unexpected(QStringLiteral("Failed to migrate: invalid target version")));
        co_return;
    }

    QElapsedTimer elapsed;
    elapsed.start();

    const auto readVersionQuery = [db = d->db] {
        return db.driverName() == u"sqlite"
                   ? u"SELECT version FROM asql_migrations WHERE name = ?"_s
                   : u"SELECT version FROM asql_migrations WHERE name = $1"_s;
    };

    while (true) {
        auto readResult = co_await d->db.exec(readVersionQuery(), {d->name}, q);
        if (!readResult) {
            data->deliverDirect(std::unexpected(readResult.error()));
            co_return;
        }

        int active = 0;
        auto row   = readResult->begin();
        if (row != readResult->end()) {
            active = row[0].toInt();
        }

        if (active > q->latest()) {
            data->deliverDirect(std::unexpected(
                u"Current version %1 is greater than the latest version %2"_s.arg(active).arg(
                    q->latest())));
            co_return;
        }

        const auto migration = d->nextQuery(active, resolvedTarget);
        if (migration.query.isEmpty()) {
            data->deliverDirect(std::expected<void, QString>{});
            co_return;
        }

        auto t = co_await d->db.begin();
        if (!t) {
            data->deliverDirect(std::unexpected(t.error()));
            co_return;
        }

        const QString query =
            d->db.driverName() == u"sqlite"
                ? u"SELECT version FROM asql_migrations WHERE name = ?"_s
                : u"SELECT version FROM asql_migrations WHERE name = $1 FOR UPDATE"_s;

        auto result = co_await d->db.exec(query,
                                          {
                                              d->name,
                                          },
                                          q);
        if (!result) {
            data->deliverDirect(std::unexpected(result.error()));
            co_return;
        }

        active = 0;
        row    = result->begin();
        if (row != result->end()) {
            active = row[0].toInt();
        }

        if (active > q->latest()) {
            data->deliverDirect(std::unexpected(
                u"Current version %1 is greater than the latest version %2"_s.arg(active).arg(
                    q->latest())));
            co_return;
        }

        const auto lockedMigration = d->nextQuery(active, resolvedTarget);
        if (lockedMigration.query.isEmpty()) {
            auto rollback = co_await t->rollback(q);
            if (!rollback) {
                data->deliverDirect(std::unexpected(rollback.error()));
                co_return;
            }
            data->deliverDirect(std::expected<void, QString>{});
            co_return;
        }

        qDebug(ASQL_MIG) << "Migrating current version" << active << "to" << lockedMigration.version
                         << "target version" << resolvedTarget << "transaction"
                         << !lockedMigration.noTransaction << "has query"
                         << !lockedMigration.query.isEmpty();
        if (lockedMigration.noTransaction) {
            qWarning(ASQL_MIG) << "Migrating from" << active << "to" << lockedMigration.version
                               << "without a transaction, might fail to update the version.";

            if (dryRun) {
                qCritical(ASQL_MIG) << "Cannot dry run a migration that requires no transaction: "
                                    << lockedMigration.version;
                data->deliverDirect(std::unexpected(
                    QStringLiteral("Cannot dry run a no-transaction migration step")));
                co_return;
            }
        }

        result = co_await d->db.exec(versionUpdateQuery(d->db),
                                     {
                                         d->name,
                                         lockedMigration.version,
                                     },
                                     q);
        if (!result) {
            qCritical(ASQL_MIG) << "Failed to update version" << result.error();
            data->deliverDirect(std::unexpected(result.error()));
            co_return;
        }

        ADatabase db   = lockedMigration.noTransaction ? d->noTransactionDB : d->db;
        auto awaitable = db.execMulti(lockedMigration.query, q);
        while (true) {
            result = co_await awaitable;
            if (!result) {
                qCritical(ASQL_MIG)
                    << "Failed to migrate"
                    << (active < lockedMigration.version ? lockedMigration.version
                                                         : lockedMigration.version + 1)
                    << (active < lockedMigration.version ? "up" : "down");
                data->deliverDirect(std::unexpected(result.error()));
                co_return;
            }

            if (!result->lastResultSet()) {
                continue;
            }

            if (dryRun && !lockedMigration.noTransaction) {
                data->deliverDirect(std::expected<void, QString>{});
                co_return;
            }

            result = co_await t->commit(q);
            if (!result) {
                data->deliverDirect(std::unexpected(result.error()));
                co_return;
            }

            qInfo(ASQL_MIG) << "Migrated from" << active << "to" << lockedMigration.version << "in"
                            << elapsed.elapsed() << "ms";
            break;
        }
    }
}

AMigrations::AMigrations(QObject *parent)
    : QObject(parent)
    , d_ptr(new AMigrationsPrivate)
{
}

AMigrations::~AMigrations()
{
    delete d_ptr;
}

AMigrations::AExpectedMigration
    AMigrations::load(ADatabase db, QString name, ADatabase noTransactionDB)
{
    AExpectedMigration coro(nullptr);
    loadCoroutine(coro.m_data, this, std::move(db), std::move(name), std::move(noTransactionDB));
    return coro;
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

AMigrations::AExpectedMigration AMigrations::migrate(int targetVersion, bool dryRun)
{
    AExpectedMigration coro(nullptr);
    migrateCoroutine(coro.m_data, this, targetVersion, dryRun);
    return coro;
}

AMigrationsPrivate::MigQuery AMigrationsPrivate::nextQuery(int versionFrom, int versionTo) const
{
    AMigrationsPrivate::MigQuery ret;

    if (versionFrom < versionTo) {
        // up
        auto it = up.constBegin();
        while (it != up.constEnd()) {
            if (it.key() <= versionTo && it.key() > versionFrom) {
                ret = {it.value().query, it.key(), it.value().noTransaction};
                break;
            }
            ++it;
        }
    } else {
        // down — one step from versionFrom (not the highest key in range)
        const auto it = down.constFind(versionFrom);
        if (it != down.constEnd() && versionFrom > versionTo) {
            ret = {it.value().query, it.key() - 1, it.value().noTransaction};
        }
    }

    return ret;
}

#include "moc_amigrations.cpp"
