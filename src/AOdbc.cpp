/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "AOdbc.hpp"

#include "ADriverOdbc.hpp"

#include <adatabase.h>

#include <QLoggingCategory>
#include <QUrlQuery>

using namespace ASql;
using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcAOdbc, "asql.odbc")

namespace {

void removeQueryItemInsensitive(QUrlQuery &query, QStringView key)
{
    auto items = query.queryItems();
    for (auto it = items.begin(); it != items.end();) {
        if (it->first.compare(key, Qt::CaseInsensitive) == 0) {
            it = items.erase(it);
        } else {
            ++it;
        }
    }
    query.setQueryItems(items);
}

bool hasQueryItemInsensitive(const QUrlQuery &query, QStringView key)
{
    auto items = query.queryItems();
    for (const auto &item : items) {
        if (item.first.compare(key, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QString queryItemValueInsensitive(const QUrlQuery &query, QStringView key)
{
    auto items = query.queryItems();
    for (const auto &item : items) {
        if (item.first.compare(key, Qt::CaseInsensitive) == 0) {
            return item.second;
        }
    }
    return {};
}

QString urlToOdbcConnectionString(const QUrl &url)
{
    QUrlQuery query(url);

    QString uri;
    uri.append(u"SERVER=" + url.host() + u";");

    if (url.port() != -1) {
        uri.append(u"PORT=%1;"_s.arg(QString::number(url.port())));
    } else if (hasQueryItemInsensitive(query, u"PORT")) {
        uri.append(u"PORT=%1;"_s.arg(queryItemValueInsensitive(query, u"PORT")));
    }

    if (url.path() != u"/") {
        uri.append(u"DATABASE=%1;"_s.arg(url.path().mid(1)));
    } else if (hasQueryItemInsensitive(query, u"DATABASE")) {
        uri.append(u"DATABASE=%1;"_s.arg(queryItemValueInsensitive(query, u"DATABASE")));
    }

    if (url.scheme() == u"odbc") {
        uri.append(u"ENCRYPT=NO;");
    } else if (url.scheme() == u"odbcs") {
        uri.append(u"ENCRYPT=YES;");
    }

    if (!url.userName().isEmpty()) {
        uri.append(u"UID=" + url.userName() + u";");
    }

    if (!url.password().isEmpty()) {
        uri.append(u"PWD=" + url.password() + u";");
    }

    for (const auto &item : query.queryItems()) {
        uri.append(item.first + u"=" + item.second + u";");
    }
    qCDebug(lcAOdbc) << "Constructed ODBC connection string:" << uri;
    return uri;
}

} // namespace

namespace ASql {

class AOdbcPrivate
{
public:
    QString connection;
};

} // namespace ASql

AOdbc::AOdbc(const QString &connectionInfo)
    : d(std::make_unique<AOdbcPrivate>())
{
    d->connection = connectionInfo;
}

AOdbc::~AOdbc() = default;

std::shared_ptr<ADriverFactory> AOdbc::factory(const QUrl &url)
{
    QString uri = urlToOdbcConnectionString(url);
    return AOdbc::factory(uri);
}

std::shared_ptr<ADriverFactory> AOdbc::factory(const QString &connectionInfo)
{
    return std::make_shared<AOdbc>(connectionInfo);
}

std::shared_ptr<ADriverFactory> AOdbc::factory(QStringView connectionInfo)
{
    return AOdbc::factory(connectionInfo.toString());
}

ADatabase AOdbc::database(const QString &connectionInfo)
{
    ADatabase ret(std::make_shared<AOdbc>(connectionInfo));
    return ret;
}

ADatabase AOdbc::database(const QUrl &connectionInfo)
{
    QString uri = urlToOdbcConnectionString(connectionInfo);
    ADatabase ret(std::make_shared<AOdbc>(uri));
    return ret;
}

ADriver *AOdbc::createRawDriver() const
{
    return new ADriverOdbc(d->connection);
}

std::shared_ptr<ADriver> AOdbc::createDriver() const
{
    return std::make_shared<ADriverOdbc>(d->connection);
}

ADatabase AOdbc::createDatabase() const
{
    return ADatabase(std::make_shared<ADriverOdbc>(d->connection));
}
