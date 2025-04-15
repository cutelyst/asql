/*
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "aresult.h"

#include <QCborArray>
#include <QCborMap>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

using namespace ASql;
using namespace Qt::StringLiterals;

AResult::AResult() = default;

AResult::AResult(const AResult &other)
{
    d = other.d;
}

AResult::~AResult() = default;

bool AResult::lastResultSet() const
{
    return d->lastResultSet();
}

bool AResult::hasError() const
{
    return !d || d->hasError();
}

QString AResult::errorString() const
{
    return !d ? u"INVALID DRIVER"_s : d->errorString();
}

QByteArray AResult::query() const
{
    return d->query();
}

QVariantList AResult::queryArgs() const
{
    return d->queryArgs();
}

int AResult::size() const
{
    return d->size();
}

int AResult::fields() const
{
    return d->fields();
}

int AResult::numRowsAffected() const
{
    return d->numRowsAffected();
}

int AResult::indexOfField(const QString &name) const
{
    return d->indexOfField(name);
}

int AResult::indexOfField(QStringView name) const
{
    return d->indexOfField(name);
}

QString AResult::fieldName(int column) const
{
    return d->fieldName(column);
}

QStringList AResult::columnNames() const
{
    QStringList columns;
    columns.reserve(d->fields());
    for (int i = 0; i < fields(); ++i) {
        columns.append(fieldName(i));
    }
    return columns;
}

QVariantHash AResult::toHash() const
{
    QVariantHash ret;
    auto it = constBegin();
    if (it != constEnd()) {
        ret.reserve(d->fields());
        for (int i = 0; i < fields(); ++i) {
            ret.insert(fieldName(i), it.value(i));
        }
    }
    return ret;
}

QVariantList AResult::toListHash() const
{
    QVariantList ret;
    auto it = constBegin();
    if (it != constEnd()) {
        ret.reserve(d->size());
        const QStringList columns = columnNames();

        do {
            QVariantHash obj;
            obj.reserve(d->fields());
            for (int i = 0; i < fields(); ++i) {
                obj.insert(columns[i], it.value(i));
            }
            ret.append(obj);
            ++it;
        } while (it != constEnd());
    }
    return ret;
}

QJsonObject AResult::toJsonObject() const
{
    QJsonObject ret;
    auto it = constBegin();
    if (it != constEnd()) {
        for (int i = 0; i < fields(); ++i) {
            ret.insert(fieldName(i), QJsonValue::fromVariant(it.value(i)));
        }
    }
    return ret;
}

QCborMap AResult::toCborMap() const
{
    QCborMap ret;
    auto it = constBegin();
    if (it != constEnd()) {
        for (int i = 0; i < fields(); ++i) {
            ret.insert(fieldName(i), QCborValue::fromVariant(it.value(i)));
        }
    }
    return ret;
}

QJsonArray AResult::toJsonArrayObject() const
{
    QJsonArray ret;
    auto it = constBegin();
    if (it != constEnd()) {
        const QStringList columns = columnNames();

        do {
            QJsonObject obj;
            for (int i = 0; i < fields(); ++i) {
                obj.insert(columns[i], QJsonValue::fromVariant(it.value(i)));
            }
            ret.append(obj);
            ++it;
        } while (it != constEnd());
    }
    return ret;
}

QJsonObject AResult::toJsonObjectArray() const
{
    QJsonObject ret;
    std::vector<QJsonArray> columnsData;
    columnsData.resize(fields());

    for (const auto &row : *this) {
        for (int i = 0; i < fields(); ++i) {
            columnsData[i].append(QJsonValue::fromVariant(row.value(i)));
        }
    }

    for (int i = 0; i < fields(); ++i) {
        ret.insert(fieldName(i), columnsData[i]);
    }

    return ret;
}

QJsonObject AResult::toJsonObjectIndexed(QStringView columnKey, QStringView rowsKey) const
{
    QJsonObject ret;

    QJsonArray columns;
    for (int i = 0; i < fields(); ++i) {
        columns.append(fieldName(i));
    }
    ret.insert(columnKey, columns);

    QJsonArray rows;
    for (const auto &row : *this) {
        QJsonArray rowArray;
        for (int i = 0; i < fields(); ++i) {
            rowArray.append(QJsonValue::fromVariant(row.value(i)));
        }
        rows.append(rowArray);
    }
    ret.insert(rowsKey, rows);

    return ret;
}

QCborArray AResult::toCborArrayMap() const
{
    QCborArray ret;
    auto it = constBegin();
    if (it != constEnd()) {
        const QStringList columns = columnNames();

        do {
            QCborMap obj;
            for (int i = 0; i < fields(); ++i) {
                obj.insert(columns[i], QCborValue::fromVariant(it.value(i)));
            }
            ret.append(obj);
            ++it;
        } while (it != constEnd());
    }
    return ret;
}

QCborMap AResult::toCborMapArray() const
{
    QCborMap ret;
    std::vector<QCborArray> columnsData;
    columnsData.resize(fields());

    for (const auto &row : *this) {
        for (int i = 0; i < fields(); ++i) {
            columnsData[i].append(QCborValue::fromVariant(row.value(i)));
        }
    }

    for (int i = 0; i < fields(); ++i) {
        ret.insert(fieldName(i), columnsData[i]);
    }

    return ret;
}

QCborMap AResult::toCborMapIndexed(QStringView columnKey, QStringView rowsKey) const
{
    QCborMap ret;

    QCborArray columns;
    for (int i = 0; i < fields(); ++i) {
        columns.append(fieldName(i));
    }
    ret.insert(columnKey, columns);

    QCborArray rows;
    for (const auto &row : *this) {
        QCborArray rowArray;
        for (int i = 0; i < fields(); ++i) {
            rowArray.append(QCborValue::fromVariant(row.value(i)));
        }
        rows.append(rowArray);
    }
    ret.insert(rowsKey, rows);

    return ret;
}

AResult &AResult::operator=(const AResult &copy)
{
    d = copy.d;
    return *this;
}

bool AResult::operator==(const AResult &other) const
{
    return d == other.d;
}

AResult::AResult(const std::shared_ptr<AResultPrivate> &priv)
    : d(priv)
{
}

AResult::AResult(std::shared_ptr<AResultPrivate> &&priv)
    : d(std::move(priv))
{
}

AResultPrivate::~AResultPrivate() = default;

int AResultPrivate::indexOfField(const QString &name) const
{
    for (int i = 0; i < fields(); ++i) {
        if (name == fieldName(i)) {
            return i;
        }
    }
    return -1;
}

int AResultPrivate::indexOfField(QStringView name) const
{
    for (int i = 0; i < fields(); ++i) {
        if (name == fieldName(i)) {
            return i;
        }
    }
    return -1;
}

int AResultPrivate::indexOfField(QLatin1String name) const
{
    for (int i = 0; i < fields(); ++i) {
        if (name == fieldName(i)) {
            return i;
        }
    }
    return -1;
}

QDate AResult::AColumn::toDate() const
{
    return d->toDate(row, column);
}

QTime AResult::AColumn::toTime() const
{
    return d->toTime(row, column);
}

QDateTime AResult::AColumn::toDateTime() const
{
    return d->toDateTime(row, column);
}

QJsonValue AResult::AColumn::toJsonValue() const
{
    return d->toJsonValue(row, column);
}

QCborValue AResult::AColumn::toCborValue() const
{
    return d->toCborValue(row, column);
}

QVariantHash AResult::ARow::toHash() const
{
    QVariantHash ret;
    ret.reserve(d->fields());
    for (int i = 0; i < d->fields(); ++i) {
        ret.insert(d->fieldName(i), value(i));
    }
    return ret;
}

QVariantList AResult::ARow::toList() const
{
    QVariantList ret;
    ret.reserve(d->fields());
    for (int i = 0; i < d->fields(); ++i) {
        ret.append(value(i));
    }
    return ret;
}

QJsonObject AResult::ARow::toJsonObject() const
{
    QJsonObject ret;
    for (int i = 0; i < d->fields(); ++i) {
        ret.insert(d->fieldName(i), QJsonValue::fromVariant(value(i)));
    }
    return ret;
}

QCborMap AResult::ARow::toCborMap() const
{
    QCborMap ret;
    for (int i = 0; i < d->fields(); ++i) {
        ret.insert(d->fieldName(i), QCborValue::fromVariant(value(i)));
    }
    return ret;
}

QVariantHash AResult::const_iterator::toHash() const
{
    QVariantHash ret;
    if (d) {
        ret.reserve(d->fields());
        for (int i = 0; i < d->fields(); ++i) {
            ret.insert(d->fieldName(i), value(i));
        }
    }
    return ret;
}

QJsonObject AResult::const_iterator::toJsonObject() const
{
    QJsonObject ret;
    if (d) {
        for (int i = 0; i < d->fields(); ++i) {
            ret.insert(d->fieldName(i), QJsonValue::fromVariant(value(i)));
        }
    }
    return ret;
}

QCborMap AResult::const_iterator::toCborMap() const
{
    QCborMap ret;
    if (d) {
        for (int i = 0; i < d->fields(); ++i) {
            ret.insert(d->fieldName(i), QCborValue::fromVariant(value(i)));
        }
    }
    return ret;
}
