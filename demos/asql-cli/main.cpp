/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../../src/acoroexpected.h"
#include "../../src/adatabase.h"
#include "../../src/aresult.h"

#ifdef ASQL_DRIVER_POSTGRES
#    include "../../src/apg.h"
#endif
#ifdef ASQL_DRIVER_SQLITE
#    include "../../src/ASqlite.hpp"
#endif
#ifdef ASQL_DRIVER_MYSQL
#    include "../../src/amysql.h"
#endif
#ifdef ASQL_DRIVER_ODBC
#    include "../../src/AOdbc.hpp"
#endif

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

using namespace ASql;
using namespace Qt::StringLiterals;

static void printResult(const AResult &result)
{
    QTextStream out(stdout);

    if (result.hasError()) {
        QTextStream err(stderr);
        err << "Error: " << result.errorString() << "\n";
        return;
    }

    const int numFields = result.fields();
    const int numRows   = result.size();

    if (numFields == 0) {
        out << "Query OK, " << result.numRowsAffected() << " row(s) affected.\n";
        return;
    }

    // Collect column names and compute widths
    QList<QString> headers;
    QList<int> widths;
    for (int c = 0; c < numFields; ++c) {
        QString name = result.fieldName(c);
        headers.append(name);
        widths.append(name.length());
    }

    // Collect all cell strings and update column widths
    QList<QList<QString>> rows;
    for (const auto &row : result) {
        QList<QString> cells;
        for (int c = 0; c < numFields; ++c) {
            QString val = row.value(c).toString();
            if (val.length() > widths[c]) {
                widths[c] = val.length();
            }
            cells.append(std::move(val));
        }
        rows.append(std::move(cells));
    }

    // Build separator line
    QString sep = u"+"_s;
    for (int c = 0; c < numFields; ++c) {
        sep += QString(widths[c] + 2, u'-') + u"+"_s;
    }

    // Print header
    out << sep << "\n";
    out << "|"_L1;
    for (int c = 0; c < numFields; ++c) {
        out << " "_L1 << headers[c].leftJustified(widths[c]) << " |"_L1;
    }
    out << "\n" << sep << "\n";

    // Print rows
    for (const auto &cells : rows) {
        out << "|"_L1;
        for (int c = 0; c < numFields; ++c) {
            out << " "_L1 << cells[c].leftJustified(widths[c]) << " |"_L1;
        }
        out << "\n";
    }

    out << sep << "\n";
    out << numRows << " row(s)\n";
}

auto runQuery(ADatabase db, QString query) -> ACoroTerminator
{
    auto result = co_await db.exec(QStringView{query}, nullptr);
    if (result.has_value()) {
        printResult(*result);
    } else {
        QTextStream err(stderr);
        err << "Error: " << result.error() << "\n";
    }
    qApp->quit();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"asql-cli"_s);
    QCoreApplication::setApplicationVersion(u"1.0"_s);

    QCommandLineParser parser;
    parser.setApplicationDescription(u"ASql command-line query tool"_s);
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption connOption(
        {u"c"_s, u"connection"_s},
        u"Database connection URL (e.g. postgresql:///mydb or sqlite:///path/to/db)"_s,
        u"url"_s);
    parser.addOption(connOption);
    parser.addPositionalArgument(u"query"_s, u"SQL query to execute"_s, u"[query]"_s);

    parser.process(app);

    if (!parser.isSet(connOption)) {
        QTextStream err(stderr);
        err << "Error: --connection/-c is required.\n\n";
        parser.showHelp(1);
    }

    const QString connUrl = parser.value(connOption);
    const QStringList pos = parser.positionalArguments();
    const QString query   = pos.isEmpty() ? u"SELECT now()"_s : pos.join(u' ');

    // Select driver by URL scheme
    ADatabase db;
    QUrl url(connUrl);
    const QString scheme = url.scheme();

#ifdef ASQL_DRIVER_POSTGRES
    if (scheme == u"postgresql"_s || scheme == u"postgres"_s) {
        db = APg::database(connUrl);
    } else
#endif
#ifdef ASQL_DRIVER_SQLITE
        if (scheme == u"sqlite"_s) {
        db = ASqlite::database(connUrl);
    } else
#endif
#ifdef ASQL_DRIVER_MYSQL
        if (scheme == u"mysql"_s) {
        db = AMysql::database(connUrl);
    } else
#endif
#ifdef ASQL_DRIVER_ODBC
        if (scheme == u"odbc"_s || scheme == u"odbcs"_s) {
        db = AOdbc::database(url);
    } else
#endif
    {
        QTextStream err(stderr);
        err << "Error: unsupported or unknown URL scheme '" << scheme << "'.\n";
        err << "Supported schemes:";
#ifdef ASQL_DRIVER_POSTGRES
        err << " postgresql";
#endif
#ifdef ASQL_DRIVER_SQLITE
        err << " sqlite";
#endif
#ifdef ASQL_DRIVER_MYSQL
        err << " mysql";
#endif
#ifdef ASQL_DRIVER_ODBC
        err << " odbc odbcs";
#endif
        err << "\n";
        return 1;
    }

    // Spinner
    static const char spinChars[] = {'|', '/', '-', '\\'};
    int spinIdx                   = 0;
    auto *spinner                 = new QTimer(&app);
    spinner->setInterval(100);
    QObject::connect(spinner, &QTimer::timeout, spinner, [&spinIdx] {
        fprintf(stderr, "\r%c ", spinChars[spinIdx % 4]);
        fflush(stderr);
        ++spinIdx;
    });
    spinner->start();

    db.open(nullptr, [&app, &db, &query, spinner](bool isOpen, const QString &error) {
        spinner->stop();
        fprintf(stderr, "\r  \r"); // clear spinner
        fflush(stderr);

        if (!isOpen) {
            QTextStream err(stderr);
            err << "Connection error: " << error << "\n";
            app.exit(1);
            return;
        }

        runQuery(db, query);
    });

    return app.exec();
}
