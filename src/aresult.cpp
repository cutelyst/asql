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

bool AResult::next()
{
    return d->next();
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

void AResult::setAt(int row)
{
    d->setAt(row);
}

int AResult::at() const
{
    return d->at();
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

QString AResult::fieldName(int column) const
{
    return d->fieldName(column);
}

QVariant AResult::value(int column) const
{
    return d->value(column);
}

QStringList AResult::columnNames()
{
    QStringList columns;
    for (int i = 0; i < fields(); ++i) {
        columns.append(fieldName(i));
    }
    return columns;
}

QVariantList AResult::array()
{
    QVariantList ret;
    if (size()) {
        setAt(0);
        for (int i = 0; i < fields(); ++i) {
            ret.append(value(i));
        }
    }
    return ret;
}

QVariantHash AResult::hash()
{
    QVariantHash ret;
    if (size()) {
        setAt(0);
        for (int i = 0; i < fields(); ++i) {
            ret.insert(fieldName(i), value(i));
        }
    }
    return ret;
}

QVariantList AResult::hashes()
{
    QVariantList ret;
    if (size()) {
        setAt(0);
        const QStringList columns = columnNames();

        QVariantHash obj;
        do {
            for (int i = 0; i < fields(); ++i) {
                obj.insert(columns[i],value(i));
            }
            ret.append(obj);
        } while (next());
    }
    return ret;
}

QJsonObject AResult::jsonObject()
{
    QJsonObject ret;
    if (size()) {
        setAt(0);
        for (int i = 0; i < fields(); ++i) {
            ret.insert(fieldName(i), QJsonValue::fromVariant(value(i)));
        }
    }
    return ret;
}

QJsonArray AResult::jsonArray()
{
    QJsonArray ret;
    if (size()) {
        setAt(0);
        const QStringList columns = columnNames();

        QJsonObject obj;
        do {
            for (int i = 0; i < fields(); ++i) {
                obj.insert(columns[i], QJsonValue::fromVariant(value(i)));
            }
            ret.append(obj);
        } while (next());
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
