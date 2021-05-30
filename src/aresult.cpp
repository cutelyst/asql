/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "aresult.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>

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
    return d == nullptr || d->error();
}

QString AResult::errorString() const
{
    return !d ? d->errorString() : QStringLiteral("INVALID DRIVER");
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
    for (int i = 0; i < fields(); ++i) {
        columns.append(fieldName(i));
    }
    return columns;
}

QVariantList AResult::array() const
{
    QVariantList ret;
    auto it = constBegin();
    if (it != constEnd()) {
        for (int i = 0; i < fields(); ++i) {
            ret.append(it.value(i));
        }
    }
    return ret;
}

QVariantHash AResult::hash() const
{
    QVariantHash ret;
    auto it = constBegin();
    if (it != constEnd()) {
        for (int i = 0; i < fields(); ++i) {
            ret.insert(fieldName(i), it.value(i));
        }
    }
    return ret;
}

QVariantList AResult::hashes() const
{
    QVariantList ret;
    auto it = constBegin();
    if (it != constEnd()) {
        const QStringList columns = columnNames();

        QVariantHash obj;
        do {
            for (int i = 0; i < fields(); ++i) {
                obj.insert(columns[i], it.value(i));
            }
            ret.append(obj);
            ++it;
        } while (it != constEnd());
    }
    return ret;
}

QJsonObject AResult::jsonObject() const
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

QJsonArray AResult::jsonArray() const
{
    QJsonArray ret;
    auto it = constBegin();
    if (it != constEnd()) {
        const QStringList columns = columnNames();

        QJsonObject obj;
        do {
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

QVariantHash AResult::ARow::hash() const
{
    QVariantHash ret;
    for (int i = 0; i < d->fields(); ++i) {
        ret.insert(d->fieldName(i), value(i));
    }
    return ret;
}

QJsonObject AResult::ARow::jsonObject() const
{
    QJsonObject ret;
    for (int i = 0; i < d->fields(); ++i) {
        ret.insert(d->fieldName(i), QJsonValue::fromVariant(value(i)));
    }
    return ret;
}
