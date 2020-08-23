#ifndef ARESULT_H
#define ARESULT_H

#include <QVariant>

#include "aqsqlexports.h"

class ASQL_EXPORT AResult
{
public:
    AResult();
    virtual ~AResult();

    virtual bool next() = 0;
    virtual bool lastResulSet() const = 0;
    virtual bool error() const = 0;
    virtual QString errorString() const = 0;

    virtual void setAt(int row) = 0;
    virtual int at() const = 0;
    virtual int size() const = 0;
    virtual int fields() const = 0;
    virtual int numRowsAffected() = 0;

    virtual QString fieldName(int column) const = 0;
    virtual QVariant value(int column) const = 0;

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

};

#endif // ARESULT_H
