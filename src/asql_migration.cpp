/*
 * SPDX-FileCopyrightText: (C) 2020-2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "adatabase.h"
#include "amigrations.h"

#if defined(DRIVER_POSTGRES)
#    include "apg.h"
#endif

#if defined(DRIVER_SQLITE)
#    include "ASqlite.hpp"
#endif

#include <iostream>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

using namespace ASql;
using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(u"Cutelyst"_s);
    QCoreApplication::setOrganizationDomain(u"org.cutelyst"_s);
    QCoreApplication::setApplicationName(u"ASqlMigration"_s);
    QCoreApplication::setApplicationVersion(u"0.2.0"_s);

    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QCoreApplication::translate("main", "ASql database migration tool."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(u"source"_s,
                                 QCoreApplication::translate("main", "Migration file(s)."));

    QCommandLineOption confirmOption(
        u"y"_s, QCoreApplication::translate("main", "Automatically confirm migration"));
    parser.addOption(confirmOption);

    QCommandLineOption dryRunOption(
        {u"d"_s, u"dry-run"_s},
        QCoreApplication::translate("main", "Do not actually commit changes to the database."));
    parser.addOption(dryRunOption);

    QCommandLineOption showSqlOption({u"s"_s, u"show-sql"_s},
                                     QCoreApplication::translate("main", "Show migration SQL."));
    parser.addOption(showSqlOption);

    QCommandLineOption connOption(
        {u"c"_s, u"connection"_s},
        QCoreApplication::translate("main", "Connection URL to the database."),
        QCoreApplication::translate("main", "url"));
    parser.addOption(connOption);

    QCommandLineOption nameOption({u"n"_s, u"name"_s},
                                  QCoreApplication::translate("main", "Migration name."),
                                  QCoreApplication::translate("main", "name"));
    parser.addOption(nameOption);

    QCommandLineOption targetVersionOption(
        u"target"_s,
        QCoreApplication::translate("main", "Migrate database to target <version>."),
        QCoreApplication::translate("main", "version"));
    parser.addOption(targetVersionOption);

    // Process the actual command line arguments given by the user
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.empty()) {
        std::cerr << qPrintable(
                         QCoreApplication::translate("main", "No migration file(s) specified."))
                  << std::endl;
        return 1;
    }

    const bool confirm = parser.isSet(confirmOption);
    const bool showSql = parser.isSet(showSqlOption);
    const bool dryRun  = parser.isSet(dryRunOption);
    int targetVersion  = -1;
    if (parser.isSet(targetVersionOption)) {
        bool ok;
        targetVersion = parser.value(targetVersionOption).toInt(&ok);
        if (!ok || targetVersion < 0) {
            std::cerr << qPrintable(
                             QCoreApplication::translate("main", "Invalid target version %1.")
                                 .arg(parser.value(targetVersionOption)))
                      << std::endl;
            return 2;
        }
    }

    if (!parser.isSet(connOption)) {
        std::cerr << qPrintable(QCoreApplication::translate("main", "Connection URL not set."))
                  << std::endl;
        return 3;
    }

    QString name = parser.value(nameOption);
    if (name.isEmpty()) {
        // Use the first filename as the migration name
        name = QFileInfo(args.value(0)).baseName();
        if (name.isEmpty()) {
            std::cerr << qPrintable(QCoreApplication::translate("main", "Migration name not set."))
                      << std::endl;
            return 4;
        }
    }

    const QString conn = parser.value(connOption);

    QString sql;
    for (const QString &file : args) {
        QFile f(file);
        if (f.open(QFile::ReadOnly | QFile::Text)) {
            sql.append(QString::fromLocal8Bit(f.readAll()));
        } else {
            std::cerr << qPrintable(QCoreApplication::translate(
                                        "main", "Failed to open migration file: %1.")
                                        .arg(file))
                      << std::endl;
            return 5;
        }
    }

    ADatabase db;
    ADatabase noTransactionDB;
#if defined(DRIVER_POSTGRES)
    if (conn.startsWith(u"postgres://") || conn.startsWith(u"postgresql://")) {
        db              = APg::database(conn);
        noTransactionDB = APg::database(conn);
        noTransactionDB.open();
    }
#endif

#if defined(DRIVER_SQLITE)
    if (conn.startsWith(u"sqlite://")) {
        db              = ASqlite::database(conn);
        noTransactionDB = ASqlite::database(conn);
        noTransactionDB.open();
    }
#endif

    if (!db.isValid()) {
        std::cerr << qPrintable(
                         QCoreApplication::translate("main", "No driver for uri: %1.").arg(conn))
                  << std::endl;
        return 5;
    }

    db.open(nullptr, [=](bool isOpen, const QString &errorString) {
        if (!isOpen) {
            std::cerr << qPrintable(
                             QCoreApplication::translate("main", "Failed to open database: %1.")
                                 .arg(errorString))
                      << std::endl;
            qApp->exit(6);
            return;
        }

        auto mig = new AMigrations;
        mig->fromString(sql);
        mig->connect(mig, &AMigrations::ready, [=](bool error, const QString &errorString) {
            if (error) {
                std::cerr << qPrintable(QCoreApplication::translate(
                                            "main", "Failed to initialize migrations: %1.")
                                            .arg(errorString))
                          << std::endl;
                qApp->exit(7);
                return;
            }

            const int newVersion = targetVersion != -1 && targetVersion <= mig->latest()
                                       ? targetVersion
                                       : mig->latest();

            if (mig->active() == newVersion) {
                std::cerr << qPrintable(QCoreApplication::translate(
                                            "main", "Database is already at target version: %1.")
                                            .arg(QString::number(mig->active())))
                          << std::endl;
                qApp->exit(0);
                return;
            }

            if (showSql) {
                std::cout << qPrintable(QCoreApplication::translate("main", "Migration SQL:"))
                          << std::endl
                          << qPrintable(mig->sqlFor(mig->active(), newVersion)) << std::endl;
            }

            if (!confirm || newVersion < mig->active()) {
                if (newVersion < mig->active()) {
                    std::cout << qPrintable(
                        QCoreApplication::translate(
                            "main", "Do you want to ROLLBACK '%1' from %2 to %3? [yes/no] ")
                            .arg(name)
                            .arg(QString::number(mig->active()))
                            .arg(QString::number(newVersion)));
                } else {
                    std::cout << qPrintable(
                        QCoreApplication::translate(
                            "main", "Do you wanto to migrate '%1' from %2 to %3? [y/n] ")
                            .arg(name)
                            .arg(QString::number(mig->active()))
                            .arg(QString::number(newVersion)));
                }

                std::string value;
                std::cin >> value;
                if ((newVersion < mig->active() && value != "yes") ||
                    (newVersion > mig->active() && value != "y")) {
                    qApp->exit(8);
                    return;
                }
            }

            QElapsedTimer t;
            t.start();
            mig->migrate(newVersion, [t](bool error, const QString &errorString) {
                if (error) {
                    std::cerr << qPrintable(QCoreApplication::translate("main", "Error: %1.")
                                                .arg(errorString))
                              << std::endl;
                    qApp->exit(9);
                } else {
                    std::cout << qPrintable(QCoreApplication::translate(
                                                "main",
                                                "Migration finished with success: '%1'. Took %2 ms")
                                                .arg(errorString)
                                                .arg(t.elapsed()))
                              << std::endl;
                    qApp->exit(0);
                }
            }, dryRun);
        });

        mig->load(db, name, noTransactionDB);
    });

    return app.exec();
}
