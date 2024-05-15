/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "amigrations.h"

#include "aresult.h"
#include "atransaction.h"

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

void AMigrations::load(const ADatabase &db, const QString &name, const ADatabase &noTransactionDB)
{
    d_ptr->name            = name;
    d_ptr->db              = db;
    d_ptr->noTransactionDB = noTransactionDB;
    d_ptr->db.exec(uR"V0G0N(
CREATE TABLE IF NOT EXISTS public.asql_migrations (
name text primary key,
version bigint not null check (version >= 0)
)
)V0G0N",
                   this,
                   [=, this](AResult &result) {
        if (result.error()) {
            qDebug(ASQL_MIG) << "Create migrations table" << result.errorString();
        }

        d_ptr->db.exec(u"SELECT version FROM public.asql_migrations WHERE name=$1",
                       {name},
                       this,
                       [=, this](AResult &result2) {
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
        });
    });
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
    d_ptr->data        = text;

    QTextStream stream(&d_ptr->data);
    static QRegularExpression re(u"^\\s*--\\s*(\\d+)\\s*(up|down)\\s*(no-transaction)?"_s,
                                 QRegularExpression::CaseInsensitiveOption);
    QString line;
    while (!stream.atEnd()) {
        stream.readLineInto(&line);
        qDebug(ASQL_MIG) << "MIG LINE" << line << upWay << version;
        static QRegularExpressionMatch match = re.match(line);
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

void AMigrations::migrate(int targetVersion,
                          std::function<void(bool, const QString &)> cb,
                          bool dryRun)
{
    Q_D(AMigrations);
    if (targetVersion < 0) {
        if (cb) {
            cb(true, u"Failed to migrate: invalid target version"_s);
        }
        qWarning(ASQL_MIG) << "Failed to migrate: invalid target version" << targetVersion;
        return;
    }

    ATransaction t(d->db);
    t.begin(this, [=, this](AResult &result) mutable {
        if (result.error()) {
            cb(true, result.errorString());
            return;
        }

        d->db.exec(u"SELECT version FROM public.asql_migrations WHERE name=$1 FOR UPDATE",
                   {d->name},
                   this,
                   [=, this](AResult &result) mutable {
            if (result.error()) {
                cb(true, result.errorString());
                return;
            }

            int active = 0;
            if (result.size()) {
                active = result[0][0].toInt();
            }

            if (active > latest()) {
                cb(true,
                   u"Current version %1 is greater than the latest version %2"_s.arg(active).arg(
                       latest()));
                return;
            }

            const auto migration = d->nextQuery(active, targetVersion);
            if (migration.query.isEmpty()) {
                if (cb) {
                    cb(false, u"Done."_s);
                }
                return;
            }

            qDebug(ASQL_MIG) << "Migrating current version" << active << "ẗo" << migration.version
                             << "target version" << targetVersion << "transaction"
                             << !migration.noTransaction << "has query"
                             << !migration.query.isEmpty();
            if (migration.noTransaction) {
                qWarning(ASQL_MIG) << "Migrating from" << active << "to" << migration.version
                                   << "without a transaction, might fail to update the version.";

                if (dryRun) {
                    qCritical(ASQL_MIG)
                        << "Cannot dry run a migration that requires no transaction: "
                        << migration.version;
                    if (cb) {
                        cb(true, u"Done."_s);
                    }
                    return;
                }

                d->db.exec(migration.versionQuery, this, [=](AResult &result) mutable {
                    if (result.error()) {
                        qCritical(ASQL_MIG) << "Failed to update version" << result.errorString();
                    }
                });
            }

            ADatabase db = migration.noTransaction ? d->noTransactionDB : d->db;
            db.exec(migration.noTransaction ? migration.query
                                            : migration.versionQuery + migration.query,
                    this,
                    [=, this](AResult &result) mutable {
                if (result.error()) {
                    if (cb) {
                        cb(true, result.errorString());
                    }
                } else if (result.lastResulSet()) {
                    if (migration.noTransaction || !dryRun) {
                        t.commit(this, [=, this](AResult &result) {
                            if (result.error()) {
                                if (cb) {
                                    cb(true, result.errorString());
                                }
                            } else {
                                qInfo(ASQL_MIG)
                                    << "Migrated from" << active << "to" << migration.version;
                                if (dryRun) {
                                    if (cb) {
                                        cb(false, u"Done."_s);
                                    }
                                } else {
                                    migrate(targetVersion, cb, dryRun);
                                }
                            }
                        });
                    } else {
                        t.rollback(this, [=](AResult &result) {
                            if (cb) {
                                cb(true, result.errorString());
                            }
                        });
                    }
                }
            });
        });
    });
}

AMigrationsPrivate::MigQuery AMigrationsPrivate::nextQuery(int versionFrom, int versionTo) const
{
    AMigrationsPrivate::MigQuery ret;

    static QString query = uR"V0G0N(
INSERT INTO public.asql_migrations VALUES ('%1', %2)
ON CONFLICT (name) DO UPDATE
SET version=EXCLUDED.version
RETURNING version;
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
