/*
 * SPDX-FileCopyrightText: (C) 2026 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringView>
#include <QUrl>

namespace ASql {

inline QString redactConnectionInfo(QStringView connectionInfo)
{
    const QString raw = connectionInfo.toString();
    QUrl url(raw);
    if (url.isValid() && !url.scheme().isEmpty() &&
        (url.scheme().startsWith(QStringLiteral("postgres"), Qt::CaseInsensitive) ||
         url.scheme().startsWith(QStringLiteral("mysql"), Qt::CaseInsensitive) ||
         url.scheme().startsWith(QStringLiteral("mariadb"), Qt::CaseInsensitive) ||
         url.scheme().startsWith(QStringLiteral("sqlite"), Qt::CaseInsensitive) ||
         url.scheme().startsWith(QStringLiteral("odbc"), Qt::CaseInsensitive))) {
        if (!url.password().isEmpty()) {
            url.setPassword({});
        }
        return url.toString(QUrl::RemovePassword | QUrl::FullyEncoded);
    }

    static const QRegularExpression secretKeyValue(
        QStringLiteral("(?i)(^|;\\s*)(PWD|Password)=(?<value>[^;]*)"));
    QString redacted                   = raw;
    QRegularExpressionMatchIterator it = secretKeyValue.globalMatch(redacted);
    while (it.hasNext()) {
        const auto match      = it.next();
        const int valueStart  = match.capturedStart(QStringLiteral("value"));
        const int valueLength = match.capturedLength(QStringLiteral("value"));
        redacted.replace(valueStart, valueLength, QStringLiteral("***"));
    }
    return redacted;
}

inline QString connectionThreadName(QStringView connectionInfo)
{
    const QString raw = connectionInfo.toString();
    QUrl url(raw);
    if (url.isValid() && !url.scheme().isEmpty()) {
        QString name = QStringLiteral("asql:") + url.scheme();
        if (!url.host().isEmpty()) {
            name += QLatin1Char('/') + url.host();
        }
        return name;
    }

    if (raw.startsWith(QStringLiteral("odbc"), Qt::CaseInsensitive)) {
        return QStringLiteral("asql:odbc");
    }

    return QStringLiteral("asql:db");
}

} // namespace ASql
