#include <QCoreApplication>
#include <QCommandLineParser>
#include <QLoggingCategory>
#include <QFile>

#include <QElapsedTimer>
#include <iostream>

#include "adatabase.h"
#include "amigrations.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("Cutelyst"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("org.cutelyst"));
    QCoreApplication::setApplicationName(QStringLiteral("ASqlMigration"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "ASql database migration tool."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("source"), QCoreApplication::translate("main", "Migration file(s)."));

    QCommandLineOption confirmOption(QStringLiteral("y"), QCoreApplication::translate("main", "Automatically confirm migration"));
    parser.addOption(confirmOption);

    QCommandLineOption dryRunOption({ QStringLiteral("d"), QStringLiteral("dry-run") },
                                    QCoreApplication::translate("main", "Do not actually commit changes to the database."));
    parser.addOption(dryRunOption);

    QCommandLineOption showSqlOption({ QStringLiteral("s"), QStringLiteral("show-sql") },
                                     QCoreApplication::translate("main", "Show migration SQL."));
    parser.addOption(showSqlOption);

    QCommandLineOption connOption({ QStringLiteral("c"), QStringLiteral("connection") },
                                  QCoreApplication::translate("main", "Connection URL to the database."),
                                  QCoreApplication::translate("main", "url"));
    parser.addOption(connOption);

    QCommandLineOption nameOption({ QStringLiteral("n"), QStringLiteral("name") },
                                  QCoreApplication::translate("main", "Migration name."),
                                  QCoreApplication::translate("main", "name"));
    parser.addOption(nameOption);

    QCommandLineOption targetVersionOption(QStringLiteral("target"),
                                           QCoreApplication::translate("main", "Migrate database to target <version>."),
                                           QCoreApplication::translate("main", "version"));
    parser.addOption(targetVersionOption);

    // Process the actual command line arguments given by the user
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.empty()) {
        std::cerr << qPrintable(QCoreApplication::translate("main", "No migration file(s) specified.")) << std::endl;
        return 1;
    }

    const bool confirm = parser.isSet(confirmOption);
    const bool showSql = parser.isSet(showSqlOption);
    const bool dryRun = parser.isSet(dryRunOption);
    int targetVersion = -1;
    if (parser.isSet(targetVersionOption)) {
        bool ok;
        targetVersion = parser.value(targetVersionOption).toInt(&ok);
        if (!ok || targetVersion < 0) {
            std::cerr << qPrintable(QCoreApplication::translate("main", "Invalid target version %1.").arg(parser.value(targetVersionOption))) << std::endl;
            return 2;
        }
    }

    if (!parser.isSet(connOption)) {
        std::cerr << qPrintable(QCoreApplication::translate("main", "Connection URL not set.")) << std::endl;
        return 3;
    }

    const QString name = parser.value(nameOption);
    if (name.isEmpty()) {
        std::cerr << qPrintable(QCoreApplication::translate("main", "Migration name not set.")) << std::endl;
        return 4;
    }

    const QString conn = parser.value(connOption);

    QString sql;
    for (const QString &file : args) {
        QFile f(file);
        if (f.open(QFile::ReadOnly | QFile::Text)) {
            sql.append(QString::fromLocal8Bit(f.readAll()));
        } else {
            std::cerr << qPrintable(QCoreApplication::translate("main", "Failed to open migration file: %1.").arg(file)) << std::endl;
            return 5;
        }
    }

    ADatabase db(conn);
    db.open([=] (bool error, const QString &errorString) {
        if (error) {
            std::cerr << qPrintable(QCoreApplication::translate("main", "Failed to open database: %1.").arg(errorString)) << std::endl;
            qApp->exit(6);
            return;
        }

        auto mig = new AMigrations;
        mig->fromString(sql);
        mig->connect(mig, &AMigrations::ready, [targetVersion, mig, confirm, dryRun, showSql] (bool error, const QString &errorString) {
            if (error) {
                std::cerr << qPrintable(QCoreApplication::translate("main", "Failed to initialize migrations: %1.").arg(errorString)) << std::endl;
                qApp->exit(7);
                return;
            }

            const int newVersion = targetVersion != -1 && targetVersion <= mig->latest() ? targetVersion :  mig->latest();

            if (mig->active() == newVersion) {
                std::cerr << qPrintable(QCoreApplication::translate("main", "Database is already at target version: %1.").arg(QString::number(mig->active()))) << std::endl;
                qApp->exit(0);
                return;
            }

            if (showSql) {
                std::cout <<qPrintable(QCoreApplication::translate("main", "Migration SQL:")) << std::endl
                         << qPrintable(mig->sqlFor(mig->active(), newVersion)) << std::endl;
            }

            if (!confirm) {
                std::cout << qPrintable(QCoreApplication::translate("main", "Do you wanto to migrate the database from %1 to %2? [y/n] ")
                                        .arg(QString::number(mig->active())).arg(QString::number(newVersion)));
                std::string value;
                std::cin >> value;
                if (value != "y") {
                    qApp->exit(8);
                    return;
                }
            }

            QElapsedTimer t;
            t.start();
            mig->migrate(newVersion, [t] (bool error, const QString &errorString) {
                if (error) {
                    std::cerr << qPrintable(QCoreApplication::translate("main", "Failed to migrate: %1.").arg(errorString)) << std::endl;
                    qApp->exit(9);
                } else {
                    std::cout << qPrintable(QCoreApplication::translate("main", "Migration finished with success: '%1'. Took %2 ms")
                                            .arg(errorString).arg(t.elapsed())) << std::endl;
                    qApp->exit(0);
                }
            }, dryRun);
        });

        mig->load(db, name);
    });

    return app.exec();
}
