/*
 * SPDX-FileCopyrightText: (C) 2020-2022 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef ARESULT_H
#define ARESULT_H

#include <QVariant>
#include <memory>

#include <asqlexports.h>

namespace ASql {

class ASQL_EXPORT AResultPrivate
{
public:
    virtual ~AResultPrivate();

    virtual bool lastResulSet() const = 0;
    virtual bool error() const = 0;
    virtual QString errorString() const = 0;

    virtual int size() const = 0;
    virtual int fields() const = 0;
    virtual int numRowsAffected() const = 0;

    virtual int indexOfField(const QString &name) const;
    virtual int indexOfField(QStringView name) const;
    virtual int indexOfField(QLatin1String name) const;
    virtual QString fieldName(int column) const = 0;
    virtual QVariant value(int row, int column) const = 0;

    virtual bool isNull(int row, int column) const = 0;
    virtual bool toBool(int row, int column) const = 0;
    virtual int toInt(int row, int column) const = 0;
    virtual qint64 toLongLong(int row, int column) const = 0;
    virtual quint64 toULongLong(int row, int column) const = 0;
    virtual double toDouble(int row, int column) const = 0;
    virtual QString toString(int row, int column) const = 0;
    virtual std::string toStdString(int row, int column) const = 0;
    virtual QDate toDate(int row, int column) const = 0;
    virtual QTime toTime(int row, int column) const = 0;
    virtual QDateTime toDateTime(int row, int column) const = 0;
    virtual QJsonValue toJsonValue(int row, int column) const = 0;
    virtual QByteArray toByteArray(int row, int column) const = 0;
};

class ASQL_EXPORT AResult
{
public:
    AResult();
    AResult(const std::shared_ptr<AResultPrivate> &priv);
    AResult(const AResult &other);
    virtual ~AResult();

    bool lastResulSet() const;
    bool error() const;
    QString errorString() const;

    int size() const;
    int fields() const;
    int numRowsAffected() const;

    int indexOfField(const QString &name) const;
    int indexOfField(QStringView name) const;
    QString fieldName(int column) const;

    /*!
     * \brief columnNames returns the column names
     * \return
     */
    QStringList columnNames() const;

    /*!
     * \brief hash returns the first row as a QHash object
     * \return
     */
    QVariantHash toHash() const;

    /*!
     * \brief toHashList returns all rows as a list of QVariantHash objects
     * \return
     */
    QVariantList toHashList() const;

    /*!
     * \brief toJsonObject returns the first row as a JSON object
     * \return
     */
    QJsonObject toJsonObject() const;

    /*!
     * \brief toJsonArray returns all rows as an array of JSON objects.
     * \return
     */
    QJsonArray toJsonArray() const;

    AResult &operator=(const AResult &copy);
    bool operator==(const AResult &other) const;

    class ASQL_EXPORT AColumn {
    public:
        std::shared_ptr<AResultPrivate> d;
        int row;
        int column;

        explicit inline AColumn(std::shared_ptr<AResultPrivate> data, int _row, int _column) : d(data), row(_row), column(_column) { }

        inline QString fieldName() const { return d->fieldName(column); }

        inline QVariant value() const { return d->value(row, column); }
        inline bool isNull() const { return d->isNull(row, column); }
        inline bool toBool() const { return d->toBool(row, column); }
        inline int toInt() const { return d->toInt(row, column); };
        inline qint64 toLongLong() const { return d->toLongLong(row, column); };
        inline quint64 toULongLong() const { return d->toULongLong(row, column); };
        inline double toDouble() const { return d->toDouble(row, column); };
        inline QString toString() const  { return d->toString(row, column); }
        inline std::string toStdString() const  { return d->toStdString(row, column); }
        QDate toDate() const;
        QTime toTime() const;
        QDateTime toDateTime() const;
        QJsonValue toJsonValue() const;
        inline QByteArray toByteArray() const  { return d->toByteArray(row, column); }
    };

    class ASQL_EXPORT ARow {
    public:
        std::shared_ptr<AResultPrivate> d;
        int row;

        explicit inline ARow(std::shared_ptr<AResultPrivate> data, int index) : d(data), row(index) { }

        /*!
         * \brief toHash returns the row as a QVariantHash object
         * \return
         */
        QVariantHash toHash() const;

        /*!
         * \brief toHash returns the row as a QVariantList object
         * \return
         */
        QVariantList toList() const;

        /*!
         * \brief toJsonObject returns the row as a JSON object
         * \return
         */
        QJsonObject toJsonObject() const;

        inline int at() const { return row; }
        inline QVariant value(int column) const { return d->value(row, column); }
        inline QVariant value(const QString &name) const { return d->value(row, d->indexOfField(name)); }
        inline QVariant value(QLatin1String name) const { return d->value(row, d->indexOfField(name)); }
        inline QVariant value(QStringView name) const { return d->value(row, d->indexOfField(name)); }
        inline AColumn operator[](int column) const { return AColumn(d, row, column); }
        inline AColumn operator[](const QString &name) const { return AColumn(d, row, d->indexOfField(name)); }
        inline AColumn operator[](QLatin1String name) const { return AColumn(d, row, d->indexOfField(name)); }
        inline AColumn operator[](QStringView name) const { return AColumn(d, row, d->indexOfField(name)); }
    };

    class ASQL_EXPORT const_iterator {
    public:
        std::shared_ptr<AResultPrivate> d;
        int i;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = ARow;
        using reference = ARow;

        inline const_iterator() : i(0) { }
        explicit inline const_iterator(std::shared_ptr<AResultPrivate> data, int index) : d(data), i(index) { }
        inline const_iterator(const const_iterator &o) : d(o.d), i(o.i) {}

        inline ARow operator*() const { return ARow(d, i); }

        inline int at() const { return i; }
        inline QVariant value(int column) const { return d->value(i, column); }
        inline QVariant value(const QString &name) const { return d->value(i, d->indexOfField(name)); }
        inline QVariant value(QLatin1String name) const { return d->value(i, d->indexOfField(name)); }
        inline QVariant value(QStringView name) const { return d->value(i, d->indexOfField(name)); }
        inline AColumn operator[](int column) const { return AColumn(d, i, column); }
        inline AColumn operator[](const QString &name) const { return AColumn(d, i, d->indexOfField(name)); }
        inline AColumn operator[](QLatin1String name) const { return AColumn(d, i, d->indexOfField(name)); }
        inline AColumn operator[](QStringView name) const { return AColumn(d, i, d->indexOfField(name)); }

        inline bool operator==(const const_iterator &o) const { return i == o.i; }
        inline bool operator!=(const const_iterator &o) const { return i != o.i; }
        inline bool operator<(const const_iterator& other) const { return i < other.i; }
        inline bool operator<=(const const_iterator& other) const { return i <= other.i; }
        inline bool operator>(const const_iterator& other) const { return i > other.i; }
        inline bool operator>=(const const_iterator& other) const { return i >= other.i; }
        inline const_iterator &operator++() { ++i; return *this; }
        inline const_iterator operator++(int) { const_iterator n = *this; ++i; return n; }
        inline const_iterator &operator--() { i--; return *this; }
        inline const_iterator operator--(int) { const_iterator n = *this; i--; return n; }
        inline const_iterator &operator+=(int j) { i+=j; return *this; }
        inline const_iterator &operator-=(int j) { i-=j; return *this; }
        inline const_iterator operator+(int j) const { return const_iterator(d, i+j); }
        inline const_iterator operator-(int j) const { return const_iterator(d, i-j); }
        inline int operator-(const_iterator j) const { return i - j.i; }
    };
    friend class const_iterator;

    // stl style
    inline const_iterator begin() const { return const_iterator(d, 0); }
    inline const_iterator constBegin() const { return const_iterator(d, 0); }
    inline const_iterator end() const { return const_iterator(d, size()); }
    inline const_iterator constEnd() const { return const_iterator(d, size()); }

    inline ARow operator[](int row) const { return ARow(d, row); }

protected:
    std::shared_ptr<AResultPrivate> d;
};

}

Q_DECLARE_METATYPE(ASql::AResult)

#endif // ARESULT_H
