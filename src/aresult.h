/*
 * SPDX-FileCopyrightText: (C) 2020-2023 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <asql_export.h>
#include <memory>

#include <QVariant>

namespace ASql {

class ASQL_EXPORT AResultPrivate
{
public:
    virtual ~AResultPrivate();

    virtual bool lastResulSet() const   = 0;
    virtual bool hasError() const       = 0;
    virtual QString errorString() const = 0;

    virtual QByteArray query() const       = 0;
    virtual QVariantList queryArgs() const = 0;

    virtual int size() const            = 0;
    virtual int fields() const          = 0;
    virtual int numRowsAffected() const = 0;

    virtual int indexOfField(const QString &name) const;
    virtual int indexOfField(QStringView name) const;
    virtual int indexOfField(QLatin1String name) const;
    virtual QString fieldName(int column) const       = 0;
    virtual QVariant value(int row, int column) const = 0;

    virtual bool isNull(int row, int column) const             = 0;
    virtual bool toBool(int row, int column) const             = 0;
    virtual int toInt(int row, int column) const               = 0;
    virtual qint64 toLongLong(int row, int column) const       = 0;
    virtual quint64 toULongLong(int row, int column) const     = 0;
    virtual double toDouble(int row, int column) const         = 0;
    virtual QString toString(int row, int column) const        = 0;
    virtual std::string toStdString(int row, int column) const = 0;
    virtual QDate toDate(int row, int column) const            = 0;
    virtual QTime toTime(int row, int column) const            = 0;
    virtual QDateTime toDateTime(int row, int column) const    = 0;
    virtual QJsonValue toJsonValue(int row, int column) const  = 0;
    virtual QCborValue toCborValue(int row, int column) const  = 0;
    virtual QByteArray toByteArray(int row, int column) const  = 0;
};

class ASQL_EXPORT AResult
{
public:
    AResult();
    AResult(const std::shared_ptr<AResultPrivate> &priv);
    AResult(std::shared_ptr<AResultPrivate> &&priv);
    AResult(const AResult &other);
    virtual ~AResult();

    [[nodiscard]] bool lastResulSet() const;
    [[nodiscard]] bool hasError() const;
    [[nodiscard]] QString errorString() const;

    /*!
     * \brief returns the query data sent to the database
     * \return
     */
    [[nodiscard]] QByteArray query() const;

    /*!
     * \brief returns the query arguments sent to the database
     * \return
     */
    [[nodiscard]] QVariantList queryArgs() const;

    [[nodiscard]] int size() const;
    [[nodiscard]] int fields() const;
    [[nodiscard]] int numRowsAffected() const;

    [[nodiscard]] int indexOfField(const QString &name) const;
    [[nodiscard]] int indexOfField(QStringView name) const;
    [[nodiscard]] QString fieldName(int column) const;

    /*!
     * \brief columnNames returns the column names
     * \return
     */
    [[nodiscard]] QStringList columnNames() const;

    /*!
     * \brief hash returns the first row as a QHash object
     * \return
     */
    [[nodiscard]] QVariantHash toHash() const;

    /*!
     * \brief toHashList returns all rows as a list of QVariantHash objects
     * \return
     */
    [[nodiscard]] QVariantList toListHash() const;

    /*!
     * \brief toJsonObject returns the first row as a JSON object
     * \return
     */
    [[nodiscard]] QJsonObject toJsonObject() const;

    /*!
     * \brief toCborMap returns the first row as a Cbor map
     * \return
     */
    [[nodiscard]] QCborMap toCborMap() const;

    /*!
     * \brief toJsonArray returns all rows as an array of JSON objects.
     * \return [ {"col1": 1,  "col2": "foo"}, {"col1": 2,  "col2": "bar"} ]
     */
    [[nodiscard]] QJsonArray toJsonArrayObject() const;

    /*!
     * \brief toJsonObjectArray returns all rows as JSON object with columns as keys and rows as
     * arrays
     *
     * This is a more compact representation than \sa toJsonArrayObject.
     *
     * \return { "col1": [1, 2], "col2": ["foo", "bar"] }
     */
    [[nodiscard]] QJsonObject toJsonObjectArray() const;

    /*!
     * \brief toJsonObjectArray returns all rows as JSON object with columns as keys and rows as
     * arrays
     *
     * This is a more compact representation than \sa toJsonArrayObject.
     *
     * \return { "columns": ["col1", "col2"], "rows": [ [1, "foo"], [2, "bar"] ] }
     */
    [[nodiscard]] QJsonObject toJsonObjectIndexed(QStringView columnKey = u"columns",
                                                  QStringView rowsKey   = u"rows") const;

    /*!
     * \brief toCborArrayMap returns all rows as an array of Cbor maps.
     * \return
     */
    [[nodiscard]] QCborArray toCborArrayMap() const;

    /*!
     * \brief toCborMapArray returns returns all rows as Cbor map with columns as keys and rows as
     * arrays \return
     */
    [[nodiscard]] QCborMap toCborMapArray() const;

    /*!
     * \brief toCborMapIndexed returns returns all rows as Cbor map with columns as keys and rows as
     * arrays, indexed by params \return
     */
    [[nodiscard]] QCborMap toCborMapIndexed(QStringView columnKey = u"columns",
                                            QStringView rowsKey   = u"rows") const;

    AResult &operator=(const AResult &copy);
    bool operator==(const AResult &other) const;

    class ASQL_EXPORT AColumn
    {
    public:
        std::shared_ptr<AResultPrivate> d;
        int row;
        int column;

        explicit inline AColumn(std::shared_ptr<AResultPrivate> data, int _row, int _column)
            : d(data)
            , row(_row)
            , column(_column)
        {
        }

        [[nodiscard]] inline QString fieldName() const { return d->fieldName(column); }

        [[nodiscard]] inline QVariant value() const { return d->value(row, column); }
        [[nodiscard]] inline bool isNull() const { return d->isNull(row, column); }
        [[nodiscard]] inline bool toBool() const { return d->toBool(row, column); }
        [[nodiscard]] inline int toInt() const { return d->toInt(row, column); }
        [[nodiscard]] inline qint64 toLongLong() const { return d->toLongLong(row, column); }
        [[nodiscard]] inline quint64 toULongLong() const { return d->toULongLong(row, column); }
        [[nodiscard]] inline double toDouble() const { return d->toDouble(row, column); }
        [[nodiscard]] inline QString toString() const { return d->toString(row, column); }
        [[nodiscard]] inline std::string toStdString() const { return d->toStdString(row, column); }
        [[nodiscard]] QDate toDate() const;
        [[nodiscard]] QTime toTime() const;
        [[nodiscard]] QDateTime toDateTime() const;
        [[nodiscard]] QJsonValue toJsonValue() const;
        [[nodiscard]] QCborValue toCborValue() const;
        [[nodiscard]] inline QByteArray toByteArray() const { return d->toByteArray(row, column); }
    };

    class ASQL_EXPORT ARow
    {
    public:
        std::shared_ptr<AResultPrivate> d;
        int row;

        explicit inline ARow(std::shared_ptr<AResultPrivate> data, int index)
            : d(data)
            , row(index)
        {
        }

        /*!
         * \brief toHash returns the row as a QVariantHash object
         * \return
         */
        [[nodiscard]] QVariantHash toHash() const;

        /*!
         * \brief toHash returns the row as a QVariantList object
         * \return
         */
        [[nodiscard]] QVariantList toList() const;

        /*!
         * \brief toJsonObject returns the row as a JSON object
         * \return
         */
        [[nodiscard]] QJsonObject toJsonObject() const;

        /*!
         * \brief toJsonObject returns the row as a Cbor map
         * \return
         */
        [[nodiscard]] QCborMap toCborMap() const;

        [[nodiscard]] inline int at() const { return row; }
        [[nodiscard]] inline QVariant value(int column) const { return d->value(row, column); }
        [[nodiscard]] inline QVariant value(const QString &name) const
        {
            return d->value(row, d->indexOfField(name));
        }

        [[nodiscard]] inline QVariant value(QLatin1String name) const
        {
            return d->value(row, d->indexOfField(name));
        }

        [[nodiscard]] inline QVariant value(QStringView name) const
        {
            return d->value(row, d->indexOfField(name));
        }

        [[nodiscard]] inline AColumn operator[](int column) const
        {
            return AColumn(d, row, column);
        }

        [[nodiscard]] inline AColumn operator[](const QString &name) const
        {
            return AColumn(d, row, d->indexOfField(name));
        }

        [[nodiscard]] inline AColumn operator[](QLatin1String name) const
        {
            return AColumn(d, row, d->indexOfField(name));
        }

        [[nodiscard]] inline AColumn operator[](QStringView name) const
        {
            return AColumn(d, row, d->indexOfField(name));
        }
    };

    class ASQL_EXPORT const_iterator
    {
    public:
        std::shared_ptr<AResultPrivate> d;
        int i;
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = ARow;
        using reference         = ARow;

        inline const_iterator()
            : i(0)
        {
        }
        explicit inline const_iterator(std::shared_ptr<AResultPrivate> data, int index)
            : d(data)
            , i(index)
        {
        }
        inline const_iterator(const const_iterator &o)
            : d(o.d)
            , i(o.i)
        {
        }

        [[nodiscard]] inline ARow operator*() const { return ARow(d, i); }

        [[nodiscard]] inline int at() const { return i; }

        /*!
         * \brief hash returns the row as a QHash object
         * \return
         */
        [[nodiscard]] QVariantHash toHash() const;

        /*!
         * \brief toJsonObject returns the row as a JSON object
         * \return
         */
        [[nodiscard]] QJsonObject toJsonObject() const;

        /*!
         * \brief toCborMap returns the row as a Cbor map
         * \return
         */
        [[nodiscard]] QCborMap toCborMap() const;

        [[nodiscard]] inline QVariant value(int column) const { return d->value(i, column); }

        [[nodiscard]] inline QVariant value(const QString &name) const
        {
            return d->value(i, d->indexOfField(name));
        }

        [[nodiscard]] inline QVariant value(QLatin1String name) const
        {
            return d->value(i, d->indexOfField(name));
        }

        [[nodiscard]] inline QVariant value(QStringView name) const
        {
            return d->value(i, d->indexOfField(name));
        }

        [[nodiscard]] inline AColumn operator[](int column) const { return AColumn(d, i, column); }

        [[nodiscard]] inline AColumn operator[](const QString &name) const
        {
            return AColumn(d, i, d->indexOfField(name));
        }

        [[nodiscard]] inline AColumn operator[](QLatin1String name) const
        {
            return AColumn(d, i, d->indexOfField(name));
        }

        [[nodiscard]] inline AColumn operator[](QStringView name) const
        {
            return AColumn(d, i, d->indexOfField(name));
        }

        inline bool operator==(const const_iterator &o) const { return i == o.i; }
        inline bool operator!=(const const_iterator &o) const { return i != o.i; }
        inline bool operator<(const const_iterator &other) const { return i < other.i; }
        inline bool operator<=(const const_iterator &other) const { return i <= other.i; }
        inline bool operator>(const const_iterator &other) const { return i > other.i; }
        inline bool operator>=(const const_iterator &other) const { return i >= other.i; }
        inline const_iterator &operator++()
        {
            ++i;
            return *this;
        }

        inline const_iterator operator++(int)
        {
            const_iterator n = *this;
            ++i;
            return n;
        }

        inline const_iterator &operator--()
        {
            i--;
            return *this;
        }

        inline const_iterator operator--(int)
        {
            const_iterator n = *this;
            i--;
            return n;
        }

        inline const_iterator &operator+=(int j)
        {
            i += j;
            return *this;
        }

        inline const_iterator &operator-=(int j)
        {
            i -= j;
            return *this;
        }

        inline const_iterator operator+(int j) const { return const_iterator(d, i + j); }
        inline const_iterator operator-(int j) const { return const_iterator(d, i - j); }
        inline int operator-(const_iterator j) const { return i - j.i; }
    };
    friend class const_iterator;

    // stl style
    inline const_iterator begin() const { return const_iterator(d, 0); }
    inline const_iterator constBegin() const { return const_iterator(d, 0); }
    inline const_iterator end() const { return const_iterator(d, size()); }
    inline const_iterator constEnd() const { return const_iterator(d, size()); }

    [[nodiscard]] inline ARow operator[](int row) const { return ARow(d, row); }

protected:
    std::shared_ptr<AResultPrivate> d;
};

#define AColumnIndex(result, columnName) \
    ([result]() Q_DECL_NOEXCEPT -> int { \
        static const int ix = result.indexOfField(columnName); \
        return ix; \
    }()) /**/

#define AColumn(row, columnName) \
    ([row]() Q_DECL_NOEXCEPT -> AResult::AColumn { \
        static const int ix = row.d->indexOfField(columnName); \
        return row[ix]; \
    }()) /**/

} // namespace ASql

Q_DECLARE_METATYPE(ASql::AResult)
