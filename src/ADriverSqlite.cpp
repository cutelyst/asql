#include "ADriverSqlite.hpp"

#include <QLoggingCategory>
#include <QMetaMethod>
#include <QUrl>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(ASQL_SQLITE, "asql.sqlite", QtInfoMsg)

using namespace Qt::StringLiterals;

namespace ASql {

ADriverSqlite::ADriverSqlite(const QString &connInfo)
    : ADriver{connInfo}
    , m_thread{connInfo}
{
    m_thread.moveToThread(&m_thread);
    connect(&m_thread, &ASqliteThread::openned, this, [this](OpenPromise promisse) {
        if (promisse.isOpen) {
            setState(ADatabase::State::Connected, {});
        } else {
            setState(ADatabase::State::Disconnected, promisse.error);
        }

        qDebug() << "OPEN" << promisse.isOpen << promisse.error << QThread::currentThread();

        if (!promisse.checkReceiver || !promisse.receiver.isNull()) {
            if (promisse.cb) {
                qDebug() << "OPEN";
                promisse.cb(promisse.isOpen, promisse.error);
            }
        }
    });

    m_thread.start();
}

ADriverSqlite::~ADriverSqlite()
{
    m_thread.requestInterruption();
    m_thread.wait();
    m_thread.quit();
}

bool ADriverSqlite::isValid() const
{
    return true;
}

void ADriverSqlite::open(QObject *receiver, std::function<void(bool, const QString &)> cb)
{
    qCritical() << Q_FUNC_INFO << QThread::currentThread() << m_thread.isRunning();
    setState(ADatabase::State::Connecting, {});

    OpenPromise data{
        .cb            = cb,
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };
    QMetaObject::invokeMethod(&m_thread, &ASqliteThread::open, Qt::QueuedConnection, data);
}

bool ADriverSqlite::isOpen() const
{
    return m_state == ADatabase::State::Connected;
}

void ADriverSqlite::setState(ADatabase::State state, const QString &status)
{
    m_state = state;
    if (m_stateChangedCb && (!m_stateChangedReceiverSet || !m_stateChangedReceiver.isNull())) {
        m_stateChangedCb(state, status);
    }
}

ADatabase::State ADriverSqlite::state() const
{
    return m_state;
}

void ADriverSqlite::onStateChanged(QObject *receiver,
                                   std::function<void(ADatabase::State, const QString &)> cb)
{
    m_stateChangedCb          = cb;
    m_stateChangedReceiver    = receiver;
    m_stateChangedReceiverSet = receiver;
}

void ADriverSqlite::begin(const std::shared_ptr<ADriver> &db, QObject *receiver, AResultFn cb)
{
    exec(db, u"BEGIN", {}, receiver, cb);
}

void ADriverSqlite::commit(const std::shared_ptr<ADriver> &db, QObject *receiver, AResultFn cb)
{
    exec(db, u"COMMIT", {}, receiver, cb);
}

void ADriverSqlite::rollback(const std::shared_ptr<ADriver> &db, QObject *receiver, AResultFn cb)
{
    exec(db, u"ROLLBACK", {}, receiver, cb);
}

void ADriverSqlite::exec(const std::shared_ptr<ADriver> &db,
                         QUtf8StringView query,
                         const QVariantList &params,
                         QObject *receiver,
                         AResultFn cb)
{
    QueryPromise data{
        .driver        = db,
        .params        = params,
        .cb            = cb,
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };
    data.query.setRawData(query.data(), query.size());

    QMetaObject::invokeMethod(&m_thread, &ASqliteThread::query, Qt::QueuedConnection, data);
}

void ADriverSqlite::exec(const std::shared_ptr<ADriver> &db,
                         QStringView query,
                         const QVariantList &params,
                         QObject *receiver,
                         AResultFn cb)
{
    QueryPromise data{
        .driver        = db,
        .query         = query.toUtf8(),
        .params        = params,
        .cb            = cb,
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };

    QMetaObject::invokeMethod(&m_thread, &ASqliteThread::query, Qt::QueuedConnection, data);
}

void ADriverSqlite::exec(const std::shared_ptr<ADriver> &db,
                         const APreparedQuery &query,
                         const QVariantList &params,
                         QObject *receiver,
                         AResultFn cb)
{
    QueryPromise data{
        .driver        = db,
        .preparedQuery = query,
        .params        = params,
        .cb            = cb,
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };

    QMetaObject::invokeMethod(&m_thread, &ASqliteThread::query, Qt::QueuedConnection, data);
}

void ADriverSqlite::setLastQuerySingleRowMode()
{
}

bool ADriverSqlite::enterPipelineMode(std::chrono::milliseconds timeout)
{
    return false;
}

bool ADriverSqlite::exitPipelineMode()
{
    return false;
}

ADatabase::PipelineStatus ADriverSqlite::pipelineStatus() const
{
    return ADatabase::PipelineStatus::Off;
}

bool ADriverSqlite::pipelineSync()
{
    return false;
}

int ADriverSqlite::queueSize() const
{
    // TODO
    return 0;
}

void ADriverSqlite::subscribeToNotification(const std::shared_ptr<ADriver> &db,
                                            const QString &name,
                                            QObject *receiver,
                                            ANotificationFn cb)
{
}

QStringList ADriverSqlite::subscribedToNotifications() const
{
    return {};
}

void ADriverSqlite::unsubscribeFromNotification(const std::shared_ptr<ADriver> &db,
                                                const QString &name)
{
}

ASqliteThread::ASqliteThread(const QString &connInfo)
    : m_uri(connInfo)
{
}

ASqliteThread::~ASqliteThread()
{
    sqlite3_close(m_db);
}

void ASqliteThread::open(OpenPromise promise)
{
    qCritical() << Q_FUNC_INFO << QThread::currentThread();

    QUrl uri{m_uri};
    uri.setScheme(u"file"_s);

    const QUrlQuery query{uri.query()};

    qCritical() << Q_FUNC_INFO << uri << QThread::currentThread();
    qCritical() << Q_FUNC_INFO << uri.toLocalFile() << QThread::currentThread();
    qCritical() << Q_FUNC_INFO << uri.toLocalFile() << QThread::currentThread();

    const bool openReadOnlyOption = query.hasQueryItem(u"READONLY"_s);
    const bool sharedCache        = query.hasQueryItem(u"SHAREDCACHE"_s);
    const bool openUriOption      = query.hasQueryItem(u"URI"_s);

    int openMode =
        (openReadOnlyOption ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE));
    openMode |= (sharedCache ? SQLITE_OPEN_SHAREDCACHE : SQLITE_OPEN_PRIVATECACHE);
    if (openUriOption) {
        openMode |= SQLITE_OPEN_URI;
    }
    openMode |= SQLITE_OPEN_NOMUTEX;

    const int res = sqlite3_open_v2(uri.toLocalFile().toUtf8().constData(), &m_db, openMode, NULL);
    if (res == SQLITE_OK) {
        promise.isOpen = true;
    } else {
        const char *sqliteError = sqlite3_errmsg(m_db);
        promise.error           = u"Failed to open database: %1"_s.arg(
            sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
        sqlite3_close_v2(m_db);
    }

    Q_EMIT openned(promise);
}

void ASqliteThread::query(QueryPromise promise)
{
}

bool ASqliteThread::prepare(QueryPromise promise)
{
}

} // namespace ASql

#include "moc_ADriverSqlite.cpp"
