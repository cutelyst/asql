#include "aresult.h"

#include <QJsonArray>
#include <QJsonObject>

AResult::AResult()
{

}

AResult::AResult(const AResult &other)
{
    d = other.d;
}

AResult::~AResult()
{

}

bool AResult::lastResulSet() const
{
    return d->lastResulSet();
}

bool AResult::error() const
{
    return d.isNull() || d->error();
}

QString AResult::errorString() const
{
    return !d.isNull() ? d->errorString() : QStringLiteral("INVALID DRIVER");
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

AResult::AResult(const QSharedPointer<AResultPrivate> &priv) : d(priv)
{

}

AResultPrivate::~AResultPrivate()
{

}

int AResultPrivate::indexOfField(const QString &name) const
{
    for (int i = 0; i < fields(); ++i) {
        if (name == fieldName(i)) {
            return i;
        }
    }
    return -1;
}
