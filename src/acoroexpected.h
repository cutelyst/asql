#pragma once

#include <adatabase.h>
#include <aresult.h>
#include <asql_export.h>
#include <atransaction.h>
#include <coroutine>
#include <expected>

#include <QPointer>

namespace ASql {

template <typename T>
class ASQL_EXPORT ACoroExpected
{
public:
    bool await_ready() const noexcept
    {
        return m_result.has_value() || !m_result.error().isEmpty(); // ACache skips suspension
    }

    bool await_suspend(std::coroutine_handle<> h) noexcept
    {
        m_handle = h;
        if (m_receiver) {
            m_destroyConn = QObject::connect(m_receiver, &QObject::destroyed, [h, this] {
                m_result = std::unexpected(QStringLiteral("QObject receiver* destroyed"));
                h.resume();
            });
        }

        return !await_ready();
    }

    std::expected<T, QString> await_resume() { return m_result; }

    ACoroExpected(QObject *receiver)
        : m_receiver(receiver)
        , m_result{std::unexpected(QString{})}
    {
        callback = [this](AResult &result) {
            if (result.error()) {
                m_result = std::unexpected(result.errorString());
            } else {
                if constexpr (std::is_same_v<T, ATransaction>) {
                    m_result = ATransaction(database, true);
                } else {
                    m_result = result;
                }
            }

            if (m_handle) {
                m_handle.resume();
            }
        };
    }

    ~ACoroExpected() { QObject::disconnect(m_destroyConn); }

protected:
    friend class ADatabase;
    friend class ACache;
    friend class ATransaction;
    std::function<void(AResult &result)> callback;
    ADatabase database;

private:
    QMetaObject::Connection m_destroyConn;
    QPointer<QObject> m_receiver;
    std::expected<T, QString> m_result;
    std::coroutine_handle<> m_handle;
};

class ASQL_EXPORT AExpectedDatabase
{
public:
    bool await_ready() const noexcept
    {
        return m_result.has_value() || !m_result.error().isEmpty();
    }

    bool await_suspend(std::coroutine_handle<> h) noexcept
    {
        m_handle = h;
        if (m_receiver) {
            m_destroyConn = QObject::connect(m_receiver, &QObject::destroyed, [this] {
                m_result = std::unexpected(QStringLiteral("QObject receiver* destroyed"));
                if (m_handle) {
                    m_handle.resume();
                }
            });
        }

        return !await_ready();
    }

    std::expected<ADatabase, QString> await_resume() { return m_result; }

    AExpectedDatabase(QObject *receiver)
        : m_receiver(receiver)
        , m_result{std::unexpected(QString{})}
    {
        callback = [this](ADatabase db) {
            if (db.isValid()) {
                m_result = db;
            } else {
                m_result =
                    std::unexpected(QStringLiteral("Could not get a valid database connection"));
            }

            if (m_handle) {
                m_handle.resume();
            }
        };
    }

    ~AExpectedDatabase() { QObject::disconnect(m_destroyConn); }

protected:
    friend class APool;
    std::function<void(ADatabase db)> callback;

private:
    QMetaObject::Connection m_destroyConn;
    QPointer<QObject> m_receiver;
    std::expected<ADatabase, QString> m_result;
    std::coroutine_handle<> m_handle;
};

/**
 * @brief The ACoroTerminator class
 * co_yield object; that should destroy this corouting
 * on their desctruction, note that if the caller of the
 * coroutine delete object; this might result into a double
 * free due coroutine dtor already be on the stack, in such
 * cases always object->deleteLater();
 */
class ACoroTerminator
{
public:
    struct promise_type {
        std::coroutine_handle<promise_type> handle;
        std::vector<QMetaObject::Connection> connections;

        void clean()
        {
            for (auto &conn : connections) {
                QObject::disconnect(conn);
            }
            connections.clear();
        }

        void return_void() noexcept {}

        ACoroTerminator get_return_object()
        {
            handle = std::coroutine_handle<promise_type>::from_promise(*this);
            return {};
        }

        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {}

        bool await_ready() const noexcept { return false; }

        std::suspend_never yield_value(QObject *obj)
        {
            auto conn = QObject::connect(obj, &QObject::destroyed, [this] {
                clean();
                if (handle) {
                    handle.destroy();
                }
            });
            connections.emplace_back(std::move(conn));
            return {};
        }
        void await_suspend(std::coroutine_handle<> h) noexcept {}
        void await_resume() const noexcept {}

        ~promise_type() { clean(); }
    };
};

} // namespace ASql
