#ifndef ADATABASE_H
#define ADATABASE_H

#include <QObject>
#include <QSharedPointer>
#include <QVariantList>

#include <functional>

#include <aqsqlexports.h>

class AResult;

typedef std::function<void(AResult &row)> AResultFn;
typedef std::function<void(const QString &payload, bool self)> ANotificationFn;

class APreparedQuery;
class ADatabasePrivate;
class ASQL_EXPORT ADatabase
{
    Q_GADGET
public:
    enum State {
        Disconnected,
        Connecting,
        Connected
    };
    Q_ENUM(State)

    /*!
     * \brief ADatabase contructs an invalid database object
     */
    ADatabase();

    /*!
     * \brief ADatabase contructs an database object with the connection info
     * supplied by \param connectionInfo.
     *
     * Example of connection info:
     * * Just a database db1 "postgresql:///db1"
     * * Username and database "postgresql://username@/db2"
     * * Username, host, database and options "postgresql://username@example.com/db3/bng?target_session_attrs=read-write"
     */
    ADatabase(const QString &connectionInfo);

    ADatabase(const ADatabase &other);

    virtual ~ADatabase();

    /*!
     * \brief isValid checks if this is a valid connection, invalid connections
     * are the ones created with invalid drivers, or using the empty constructor.
     * \return true if valid
     */
    bool isValid();

    /*!
     * \brief open the database, the callback is called once the operation is done
     * either by success or failure, with \param describing the error.
     * \param cb
     */
    void open(std::function<void(bool isOpen, const QString &error)> cb = {});

    State state() const;

    /*!
     * \brief onStateChanged the callback is called once connection state changes
     *
     * Only one callback can be registeres per database
     *
     * \param cb
     */
    void onStateChanged(std::function<void(State state, const QString &status)> cb);

    /*!
     * \brief isOpen returns if the database connection is open.
     * \return true if open
     */
    bool isOpen() const;

    /*!
     * \brief begin a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    void begin(AResultFn cb = {}, QObject *receiver = nullptr);

    /*!
     * \brief commit a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    void commit(AResultFn cb = {}, QObject *receiver = nullptr);

    /*!
     * \brief rollback a transaction, this operation usually succeeds,
     * but one can hook up a callback to check it's result.
     *
     * \param cb
     */
    void rollback(AResultFn cb = {}, QObject *receiver = nullptr);

    /*!
     * \brief exec excutes a \param query against this database connection,
     * once done AResult object will have the retrieved data if any, always
     * check for AResult::error() to see if the query was successful.
     *
     * Postgres allows for multiple commands to be sent, in this case the callback
     * will be called once per command you must then check for AResult::lastResultSet(),
     * if one of the commands fails the subsequently ones will not be delivered, which
     * is why checking for AResult::lastResultSet() is important. This feature is
     * not supported by Postgres on the method that accepts params.
     *
     * \param query
     * \param cb
     */
    void exec(const QString &query, AResultFn cb, QObject *receiver = nullptr);

    /*!
     * \brief exec executes a prepared \param query against this database connection,
     * once done AResult object will have the retrieved data if any, always
     * check for AResult::error() to see if the query was successful.
     *
     * \note For proper usage of APreparedQuery see it's documentation.
     * \note Postgres does not allow for multiple commands on prepared queries.
     *
     * \param query
     * \param cb
     */
    void execPrepared(const APreparedQuery &query, AResultFn cb, QObject *receiver = nullptr);

    /*!
     * \brief exec executes a \param query against this database connection,
     * once done AResult object will have the retrieved data if any, always
     * check for AResult::error() to see if the query was successful.
     *
     * \param query
     * \param params
     * \param cb
     */
    void exec(const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver = nullptr);

    /*!
     * \brief exec executes a prepared \param query against this database connection
     * with the following \p params to be bound,
     * once done AResult object will have the retrieved data if any, always
     * check for AResult::error() to see if the query was successful.
     *
     * \note For proper usage of APreparedQuery see it's documentation.
     * \note Postgres does not allow for multiple commands on prepared queries.
     *
     * \param query
     * \param cb
     */
    void execPrepared(const APreparedQuery &query, const QVariantList &params, AResultFn cb, QObject *receiver = nullptr);

    /*!
     * \brief subscribeToNotification will start listening for notifications
     * described by name, it will register only one callback.
     *
     * \note If a subscription is made in a transaction block it can be rolled
     * back, in which case it will not be effective.
     * \param channel name of the channel
     * \param cb
     */
    void subscribeToNotification(const QString &channel, ANotificationFn cb, QObject *receiver = nullptr);

    /*!
     * \brief unsubscribeFromNotification tell the database we are no longer
     * interested in being notified on this channel.
     *
     * \note If an unsubscription is made in a transaction block it can be rolled
     * back, in which case it will not be effective.
     *
     * \param channel name of the channel
     */
    void unsubscribeFromNotification(const QString &channel, QObject *receiver = nullptr);

    ADatabase &operator =(const ADatabase &copy);

protected:
    friend class APool;
    QSharedPointer<ADatabasePrivate> d;
};

#endif // ADATABASE_H
