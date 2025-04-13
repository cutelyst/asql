#include "ADriverSqlite.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMetaMethod>
#include <QUrl>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(ASQL_SQLITE, "asql.sqlite", QtInfoMsg)

using namespace Qt::StringLiterals;

namespace ASql {

ADriverSqlite::ADriverSqlite(const QString &connInfo)
    : ADriver{connInfo}
    , m_worker{connInfo}
{
    m_worker.moveToThread(&m_thread);

    connect(&m_worker, &ASqliteThread::openned, this, [this](OpenPromise promisse) {
        if (promisse.isOpen) {
            setState(ADatabase::State::Connected, {});
        } else {
            setState(ADatabase::State::Disconnected, promisse.error);
        }

        if (!promisse.checkReceiver || !promisse.receiver.isNull()) {
            if (promisse.cb) {
                promisse.cb(promisse.isOpen, promisse.error);
            }
        }
    });

    connect(&m_worker, &ASqliteThread::queryFinished, this, [this](QueryPromise promisse) {
        if (--m_queueSize == 0) {
            // This might not be needed if we only use coroutines
            // since db object won't go out of scope when we are waiting for a reply
            // unless ofc the user forget to co_await, in which case we
            // should try to do some cleanup or prevent it if possible.
            selfDriver.reset();
        }

        if (!promisse.checkReceiver || !promisse.receiver.isNull()) {
            if (promisse.cb) {
                AResult result{promisse.result};
                promisse.cb(result);
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
    setState(ADatabase::State::Connecting, {});

    OpenPromise data{
        .cb            = cb,
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &ASqliteThread::open, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "open", Qt::QueuedConnection, Q_ARG(ASql::OpenPromise, std::move(data)));
#endif
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

QByteArray QStringToUtf16Bytes(QStringView str)
{
    const char16_t *utf16Data = str.utf16();
    int byteSize              = str.length() * sizeof(QChar);
    return QByteArray(reinterpret_cast<const char *>(utf16Data), byteSize);
}

void ADriverSqlite::exec(const std::shared_ptr<ADriver> &db,
                         QUtf8StringView query,
                         const QVariantList &params,
                         QObject *receiver,
                         AResultFn cb)
{
    ++m_queueSize;
    selfDriver = db;

    QueryPromise data{
        .driver        = db,
        .cb            = cb,
        .result        = std::make_shared<AResultSqlite>(),
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };
    data.result->m_query.setRawData(query.data(), query.size());
    data.result->m_queryArgs = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &ASqliteThread::query, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "query", Qt::QueuedConnection, Q_ARG(ASql::QueryPromise, std::move(data)));
#endif
}

void ADriverSqlite::exec(const std::shared_ptr<ADriver> &db,
                         QStringView query,
                         const QVariantList &params,
                         QObject *receiver,
                         AResultFn cb)
{
    ++m_queueSize;

    QueryPromise data{
        .driver        = db,
        .cb            = cb,
        .result        = std::make_shared<AResultSqlite>(),
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };
    data.result->m_query = query.toUtf8();

    // data.result->m_query = QStringToUtf16Bytes(query);
    // data.result->m_utf16 = true;
    data.result->m_queryArgs = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &ASqliteThread::query, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "query", Qt::QueuedConnection, Q_ARG(ASql::QueryPromise, std::move(data)));
#endif
}

void ADriverSqlite::exec(const std::shared_ptr<ADriver> &db,
                         const APreparedQuery &query,
                         const QVariantList &params,
                         QObject *receiver,
                         AResultFn cb)
{
    ++m_queueSize;

    QueryPromise data{
        .driver        = db,
        .preparedQuery = query,
        .cb            = cb,
        .result        = std::make_shared<AResultSqlite>(),
        .receiver      = receiver,
        .checkReceiver = static_cast<bool>(receiver),
    };
    data.result->m_query     = query.query();
    data.result->m_queryArgs = params;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QMetaObject::invokeMethod(
        &m_worker, &ASqliteThread::query, Qt::QueuedConnection, std::move(data));
#else
    QMetaObject::invokeMethod(
        &m_worker, "query", Qt::QueuedConnection, Q_ARG(ASql::QueryPromise, std::move(data)));
#endif
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
    return m_queueSize;
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
    sqlite3_close_v2(m_db);
}

void ASqliteThread::open(OpenPromise promise)
{
    QUrl uri{m_uri};
    uri.setScheme(u"file"_s);

    const QUrlQuery query{uri.query()};

    const bool openReadOnlyOption = query.hasQueryItem(u"READONLY"_s);
    const bool sharedCache        = query.hasQueryItem(u"SHAREDCACHE"_s);
    const bool openUriOption      = true; // query.hasQueryItem(u"URI"_s);
    const bool memoryOption       = query.hasQueryItem(u"MEMORY"_s);

    int openMode =
        (openReadOnlyOption ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE));
    openMode |= (sharedCache ? SQLITE_OPEN_SHAREDCACHE : SQLITE_OPEN_PRIVATECACHE);
    openMode |= SQLITE_OPEN_NOMUTEX;

    const QString filename = [&]() {
        if (memoryOption) {
            return u":memory:"_s;
        } else if (openUriOption) {
            openMode |= SQLITE_OPEN_URI;
            return uri.toString(QUrl::FullyEncoded);
        } else {
            return uri.toLocalFile();
        }
    }();

    const int res = sqlite3_open_v2(filename.toUtf8().constData(), &m_db, openMode, NULL);
    if (res == SQLITE_OK) {
        promise.isOpen = true;
    } else {
        const char *sqliteError = sqlite3_errmsg(m_db);
        promise.error           = u"Failed to open database: %1"_s.arg(
            sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
        sqlite3_close_v2(m_db);
        m_db = nullptr;
    }

    Q_EMIT openned(promise);
}

void ASqliteThread::query(QueryPromise promise)
{
    auto _ = qScopeGuard([&] {
        if (promise.preparedQuery.has_value()) {
            // Make the statement ready to be used later
            if (sqlite3_reset(promise.stmt.get()) != SQLITE_OK) {
                m_preparedQueries.remove(promise.preparedQuery->identification());

                const char *sqliteError = sqlite3_errmsg(m_db);
                qWarning() << u"Failed to reset prepared statement: '%1'"_s.arg(
                    sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
                return;
            }

            if (sqlite3_clear_bindings(promise.stmt.get()) != SQLITE_OK) {
                m_preparedQueries.remove(promise.preparedQuery->identification());

                const char *sqliteError = sqlite3_errmsg(m_db);
                qWarning() << u"Failed to reset prepared statement bindings: '%1'"_s.arg(
                    sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
                return;
            }
        }
        Q_EMIT queryFinished(std::move(promise));
    });

    int res = SQLITE_OK;
    if (promise.preparedQuery.has_value()) {
        auto it = m_preparedQueries.constFind(promise.preparedQuery->identification());
        if (it != m_preparedQueries.constEnd()) {
            promise.stmt = it.value();
        } else {
            res = prepare(promise, SQLITE_PREPARE_PERSISTENT);

            if (res == SQLITE_OK) {
                m_preparedQueries.insert(promise.preparedQuery->identification(), promise.stmt);
            }
        }
    } else {
        res = prepare(promise, 0x0);
    }

    if (res != SQLITE_OK) {
        return;
    }

    const auto params = promise.result->m_queryArgs;
    for (int i = 0; i < params.size(); ++i) {
        const QVariant &value = params.at(i);
        if (value.isNull()) {
            res = sqlite3_bind_null(promise.stmt.get(), i + 1);
        } else {
            switch (value.userType()) {
            case QMetaType::QByteArray:
            {
                const QByteArray *ba = static_cast<const QByteArray *>(value.constData());
                res                  = sqlite3_bind_blob(
                    promise.stmt.get(), i + 1, ba->constData(), ba->size(), SQLITE_STATIC);
                break;
            }
            case QMetaType::Int:
            case QMetaType::Bool:
                res = sqlite3_bind_int(promise.stmt.get(), i + 1, value.toInt());
                break;
            case QMetaType::Double:
                res = sqlite3_bind_double(promise.stmt.get(), i + 1, value.toDouble());
                break;
            case QMetaType::UInt:
            case QMetaType::LongLong:
                res = sqlite3_bind_int64(promise.stmt.get(), i + 1, value.toLongLong());
                break;
            case QMetaType::QDateTime:
            {
                const QDateTime dateTime = value.toDateTime();
                const QString str        = dateTime.toString(Qt::ISODateWithMs);
                res                      = sqlite3_bind_text16(promise.stmt.get(),
                                          i + 1,
                                          str.data(),
                                          int(str.size() * sizeof(ushort)),
                                          SQLITE_TRANSIENT);
                break;
            }
            case QMetaType::QTime:
            {
                const QTime time  = value.toTime();
                const QString str = time.toString(u"hh:mm:ss.zzz");
                res               = sqlite3_bind_text16(promise.stmt.get(),
                                          i + 1,
                                          str.data(),
                                          int(str.size() * sizeof(ushort)),
                                          SQLITE_TRANSIENT);
                break;
            }
            case QMetaType::QString:
            {
                // lifetime of string == lifetime of its qvariant
                const QString *str = static_cast<const QString *>(value.constData());
                res                = sqlite3_bind_text16(promise.stmt.get(),
                                          i + 1,
                                          str->unicode(),
                                          int(str->size()) * sizeof(QChar),
                                          SQLITE_STATIC);
                break;
            }
            default:
            {
                const QString str = value.toString();
                // SQLITE_TRANSIENT makes sure that sqlite buffers the data
                res = sqlite3_bind_text16(promise.stmt.get(),
                                          i + 1,
                                          str.data(),
                                          int(str.size()) * sizeof(QChar),
                                          SQLITE_TRANSIENT);
                break;
            }
            }
        }

        if (res != SQLITE_OK) {
            promise.result->m_error = true;
            const char *sqliteError = sqlite3_errmsg(m_db);
            promise.result->m_errorString =
                u"Failed to bind parameter: %1 '%2'"_s.arg(value.toString())
                    .arg(sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
            return;
        }
    }

    QStringList columns;
    const int num_cols = sqlite3_column_count(promise.stmt.get());
    for (int i = 0; i < num_cols; ++i) {
        columns << QString::fromUtf8(sqlite3_column_name(promise.stmt.get(), i));
    }
    promise.result->m_fields = columns;

    QVariantList rows;
    while (sqlite3_step(promise.stmt.get()) == SQLITE_ROW) {
        if (num_cols == 0) {
            if (sqlite3_step(promise.stmt.get()) == 21) {
                return;
            }
            continue;
        }

        const int currentRow = rows.size();
        rows.resize(rows.size() + num_cols);

        for (int i = 0; i < num_cols; i++) {
            switch (sqlite3_column_type(promise.stmt.get(), i)) {
            case SQLITE_BLOB:
                rows[currentRow + i] = QByteArray(
                    static_cast<const char *>(sqlite3_column_blob(promise.stmt.get(), i)),
                    sqlite3_column_bytes(promise.stmt.get(), i));
                break;
            case SQLITE_INTEGER:
                rows[currentRow + i] = sqlite3_column_int64(promise.stmt.get(), i);
                break;
            case SQLITE_FLOAT:
                rows[currentRow + i] = sqlite3_column_double(promise.stmt.get(), i);
                break;
                break;
            case SQLITE_NULL:
                rows[currentRow + i] = QVariant(QMetaType::fromType<QString>());
                break;
            default:
                rows[currentRow + i] = QString(
                    reinterpret_cast<const QChar *>(sqlite3_column_text16(promise.stmt.get(), i)),
                    sqlite3_column_bytes16(promise.stmt.get(), i) / sizeof(QChar));
                break;
            }
        }
    }

    promise.result->m_rows = rows;
}

int ASqliteThread::prepare(QueryPromise &promise, int flags)
{
    const auto size = promise.result->m_query.size() + 1;

    int res;
    sqlite3_stmt *stmt = nullptr;
    if (promise.result->m_utf16) {
        res = sqlite3_prepare16_v3(
            m_db, promise.result->m_query.constData(), size, flags, &stmt, nullptr);
    } else {
        res = sqlite3_prepare_v3(
            m_db, promise.result->m_query.constData(), size, flags, &stmt, nullptr);
    }

    promise.stmt =
        std::shared_ptr<sqlite3_stmt>(stmt, [](sqlite3_stmt *stmt) { sqlite3_finalize(stmt); });

    if (res != SQLITE_OK) {
        promise.result->m_error       = true;
        const char *sqliteError       = sqlite3_errmsg(m_db);
        promise.result->m_errorString = u"Failed to prepare statement: %1"_s.arg(
            sqliteError ? QString::fromUtf8(sqliteError) : u"Unknown error"_s);
    }

    return res;
}

bool AResultSqlite::lastResulSet() const
{
    return m_lastResultSet;
}

bool AResultSqlite::hasError() const
{
    return m_error;
}

QString AResultSqlite::errorString() const
{
    return m_errorString;
}

QByteArray AResultSqlite::query() const
{
    return m_query;
}

QVariantList AResultSqlite::queryArgs() const
{
    return m_queryArgs;
}

int AResultSqlite::size() const
{
    return m_fields.empty() ? 0 : m_rows.size() / m_fields.size();
}

int AResultSqlite::fields() const
{
    return m_fields.size();
}

int AResultSqlite::numRowsAffected() const
{
    return -1;
}

int AResultSqlite::indexOfField(QLatin1String name) const
{
    return m_fields.indexOf(name);
}

QString AResultSqlite::fieldName(int column) const
{
    return m_fields.at(column);
}

QVariant AResultSqlite::value(int row, int column) const
{
    return m_rows.at(row * m_fields.size() + column);
}

bool AResultSqlite::isNull(int row, int column) const
{
    return value(row, column).isNull();
}

bool AResultSqlite::toBool(int row, int column) const
{
    return value(row, column).toBool();
}

int AResultSqlite::toInt(int row, int column) const
{
    return value(row, column).toInt();
}

qint64 AResultSqlite::toLongLong(int row, int column) const
{
    return value(row, column).toLongLong();
}

quint64 AResultSqlite::toULongLong(int row, int column) const
{
    return value(row, column).toULongLong();
}

double AResultSqlite::toDouble(int row, int column) const
{
    return value(row, column).toDouble();
}

QString AResultSqlite::toString(int row, int column) const
{
    return value(row, column).toString();
}

std::string AResultSqlite::toStdString(int row, int column) const
{
    return toString(row, column).toStdString();
}

QDate AResultSqlite::toDate(int row, int column) const
{
    return value(row, column).toDate();
}

QTime AResultSqlite::toTime(int row, int column) const
{
    return value(row, column).toTime();
}

QDateTime AResultSqlite::toDateTime(int row, int column) const
{
    return value(row, column).toDateTime();
}

QJsonValue AResultSqlite::toJsonValue(int row, int column) const
{
    auto doc = QJsonDocument::fromJson(toByteArray(row, column));
    return doc.isObject() ? doc.object() : doc.isArray() ? doc.array() : QJsonValue{};
}

QCborValue AResultSqlite::toCborValue(int row, int column) const
{
    return QCborValue::fromCbor(toByteArray(row, column));
}

QByteArray AResultSqlite::toByteArray(int row, int column) const
{
    return value(row, column).toByteArray();
}

} // namespace ASql

#include "moc_ADriverSqlite.cpp"
