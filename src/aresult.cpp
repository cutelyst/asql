/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "aresult.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>

using namespace ASql;

AResult::AResult() = default;

AResult::AResult(const AResult &other)
{
    d = other.d;
}

AResult::~AResult() = default;

bool AResult::lastResulSet() const
{
    return d->lastResulSet();
}

bool AResult::error() const
{
    return !d || d->error();
}

QString AResult::errorString() const
{
    return !d ? QStringLiteral("INVALID DRIVER") : d->errorString();
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

QVariantList AResult::toHashList() const
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

QJsonArray AResult::toJsonArray() const
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

AResult &AResult::operator=(const AResult &copy)
{
    d = copy.d;
    return *this;
}

bool AResult::operator==(const AResult &other) const
{
    return d == other.d;
}

AResult::AResult(const std::shared_ptr<AResultPrivate> &priv) : d(priv)
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

QDate AResult::AColumn::toDate() const  { return d->toDate(row, column); }

QTime AResult::AColumn::toTime() const  { return d->toTime(row, column); }

QDateTime AResult::AColumn::toDateTime() const  { return d->toDateTime(row, column); }

QJsonValue AResult::AColumn::toJsonValue() const
{
    return d->toJsonValue(row, column);
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
