#ifndef ACACHE_H
#define ACACHE_H

#include <QObject>

#include <adatabase.h>

#include <aqsqlexports.h>

class ACachePrivate;
class ASQL_EXPORT ACache : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ACache)
public:
    explicit ACache(QObject *parent = nullptr);

    void setDatabase(const ADatabase &db);

    /*!
     * \brief clear que requested query from the cache, do not call this from the exec callback
     * \param query
     * \param params
     * \return
     */
    bool clear(const QString &query, const QVariantList &params = QVariantList());
    void exec(const QString &query, AResultFn cb, QObject *receiver = nullptr);
    void exec(const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver = nullptr);

private:
    ACachePrivate *d_ptr;
};

#endif // ACACHE_H
