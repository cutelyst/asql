#ifndef ARESULT_H
#define ARESULT_H

#include <QVariant>
#include <QSharedPointer>

#include <aqsqlexports.h>

class ASQL_EXPORT AResultPrivate
{
public:
    virtual ~AResultPrivate();

    virtual bool next() = 0;
    virtual bool lastResulSet() const = 0;
    virtual bool error() const = 0;
    virtual QString errorString() const = 0;

    virtual void setAt(int row) = 0;
    virtual int at() const = 0;
    virtual int size() const = 0;
    virtual int fields() const = 0;
    virtual int numRowsAffected() const = 0;

    virtual QString fieldName(int column) const = 0;
    virtual QVariant value(int column) const = 0;
};

class ASQL_EXPORT AResult
{
public:
    AResult();
    AResult(const QSharedPointer<AResultPrivate> &priv);
    AResult(const AResult &other);
    virtual ~AResult();

    bool next();
    bool lastResulSet() const;
    bool error() const;
    QString errorString() const;

    void setAt(int row);
    int at() const;
    int size() const;
    int fields() const;
    int numRowsAffected() const;

    QString fieldName(int column) const;
    QVariant value(int column) const;

    /*!
     * \brief columnNames returns the column names
     * \return
     */
    QStringList columnNames();

    /*!
     * \brief hash returns the first row as a variant list
     * \return
     */
    QVariantList array();

    /*!
     * \brief hash returns the first row as a JSON object
     * \return
     */
    QVariantHash hash();

    /*!
     * \brief hashes returns the first row as a JSON object
     * \return
     */
    QVariantList hashes();

    /*!
     * \brief jsonObject returns the first row as a JSON object
     * \return
     */
    QJsonObject jsonObject();

    /*!
     * \brief jsonArray returns all rows as an array of JSON objects.
     * \return
     */
    QJsonArray jsonArray();

    AResult &operator=(const AResult &copy);
    bool operator==(const AResult &other) const;

protected:
    QSharedPointer<AResultPrivate> d;
};

#endif // ARESULT_H
