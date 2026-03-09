<!-- SPDX-FileCopyrightText: (C) 2020-2025 Daniel Nicoletti <dantti12@gmail.com>
     SPDX-License-Identifier: MIT
-->

# ASql
Qt Async Sql library

## Features
* Drivers
  - PostgreSQL
  - SQLite
* Navigate on your data with iterators
* Scoped transactions objects
* Prepared queries
* Cancellabel queries
* Thread local Connection pool
* Notifications
* Database maintainance with AMigrations class
* Conveniently converts your query data to JSON or QVariantHash
* Cache support
* Single row mode (useful for very large datasets)

## Requirements
* Qt 6.4 or later
* C++23 capable compiler (g++-14 or newer is required).

## Usage

### Creating a Pool
A connection pool is a convenient way of getting new connections without worrying about configuring it and it's lifetime, once you are done with it the connection returns to the pool. It's also possible to have a single database connection without it being attached to a pool, by creating ADatabase object directly and calling open().

```c++
using namespace ASql;

// No new connection is created at this moment
APool::create(APg::factory("postgres://user:pass@server/dbname?target_session_attrs=read-write"));
APool::create(APg::factory("postgres://user:pass@server/dbname"), "my_read_only_pool");

// Defines the maximum number of idle connections (defaults to 1)
APool::setMaxIdleConnections(10);
APool::setMaxIdleConnections(15, "my_read_only_pool");

{
    // Grabs a connection, it might be a new connection or one from the idle pool
    auto db = APool::database();

    // Grabs a connection from a read-only pool
    auto dbRO = APool::database("my_read_only_pool");

} // The scope is over, now once ADatabase db variables are
  // done with the queries they will return to the pool

```

### Performing a query without params
Please if you have user input values that you need to pass to your query, do yourself a favour and pass it as parameters, thus reducing the risk of SQL injection attacks.
```c++
auto db = APool::database();
db.exec(u"SELECT id, message FROM messages LIMIT 5", nullptr, [=] (AResult &result) {
    if (result.error()) {
        qDebug() << result.errorString();
        return;
    }

    // iterate over your data
    for (auto row : result) {
        for (int i = 0; i < result.fields(); ++i) {
            qDebug() << "data row" << row.at() << "column" << i << "value" << row.value(i);
        }
        // or explicity select the columns
        qDebug() << "ROW" << row.at() << "id" << row[0].toInt() << "msg" << row["message"].toString();
    }
});
```

### Performing multiple queries without params
When you are not sending parameters PostgreSQL allows for multiple queries, this results into multiple calls to your lambda, if one query fails the remaining queries are ignored as if they where in a transaction block. It's possible to check if the last result has arrived
```c++
// This query is at the same scope at the previou one, this mean ADatabase will queue them
db.exec(u"SELECT * FROM logs LIMIT 5; SELECT * FROM messages LIMIT 5", nullptr, [=] (AResult &result) {
    if (result.error()) {
        qDebug() << result.errorString();
        return;
    }

    if (result.lastResulSet()) {
        // do something..
    }

    // iterate over your data
    for (auto row : result) {
        for (int i = 0; i < result.fields(); ++i) {
            qDebug() << "data row" << row.at() << "column" << i << "value" << row.value(i);
        }
    }
});
```

### Performing a prepared query
Prepared queries allows the database server to avoid to repeatedly parse and plan your query execution, this doesn't always
means faster execution, this is because the planner can often make better planning when the data is known.

Our advice is that you try to mesure your execution with real data, switching from prepared to not prepared is also very trivial.

It's very important that the APreparedQuery object doesn't get deleted (by getting out of scope), this is because
it holds an unique identification for your query, in order to make this easier one can use the APreparedQueryLiteral macro.
You can also manually create a static APreparedQuery object or have your prepared query as a member of a class that isn't going to be deleted soon.
```c++
// PostgreSQL uses numered place holders, and yes you can repeat them :)
db.exec(APreparedQuery(u"INSERT INTO temp4 VALUE ($1, $2, $3, $4, $5, $6, $7) RETURNING id"),
{
     true,
     u"foo"_s,
     qint64(1234),
     QDateTime::currentDateTime(),
     123456.78,
     QUuid::createUuid(),
     QJsonObject{
          {"foo", true}
     }
}, nullptr, [=] (AResult &result) {
    if (result.error()) {
        qDebug() << result.errorString();
        return;
    }

    // Convert a single row to JSON {"id": 1234}
    qDebug() << "JSON" << result.jsonObject();
});
```

### Transactions
In async mode it might be a bit complicated to make sure your transaction rollback on error or when you are done with the database object.

To make this easier create an ATransaction object in a scoped manner, once it goes out of scope it will rollback the transaction if it hasn't committed or rolledback manually already.
```c++
ATransaction t(db);
t.begin();
db.exec(u"INSERT INTO temp4 VALUE ($1, $2, $3, $4, $5, $6, $7) RETURNING id",
{
     true,
     u"foo"_s,
     qint64(1234),
     QDateTime::currentDateTime(),
     123456.78,
     QUuid::createUuid(),
     QJsonObject{
          {"foo", true}
     }
}, nullptr, [=] (AResult &result) mutable {
    if (result.error()) {
        qDebug() << result.errorString();
        return; // Auto rollback
    }

    // only mutable lambdas allow for non const methods to be called
    t.commit();
});
```

### Cancelation
ASql was created with web usage in mind, namely to be used in Cutelyst but can also be used on Desktop/Mobile apps too, so in order to cancel
or avoid a crash due some invalid pointer captured by the lambda you can pass a QObject pointer, that if deleted and was set for the current
query will send a cancelation packet, this doesn't always work (due the packet arriving after the query was done), but your lambda will not be called anymore.

```c++
auto cancelator = new QObject;
db.exec(u"SELECT pg_sleep(5)", cancelator, [=] (AResult &result) {
    // This will never be called but it would crash
    // if cancelator was dangling reference and not passed as last parameter
    cancelator->setProperty("foo");

}); // notice the object being passed here

delete cancelator;
```

### Notifications (Postgres only)

Each Database object can have a single function subscribed to one notification, if the connection is closed the subscription is cleared and a new subscription has to be made, this it's handy to have the subcription call on a named lambda:

```c++
ADatabase db;
auto subscribe = [=] () mutable {
   db.subscribeToNotification(u"my_awesome_notification"_s,
     [=] (const ADatabaseNotification &notification) {
       qDebug() << "DB notification:" << notification.self << notification.name
                << notification.payload;
   }, this);
};

db.onStateChanged(nullptr, [=] (ADatabase::State state, const QString &status) mutable {
   qDebug() << "state changed" << state << status << db.isOpen();

   if (state == ADatabase::Disconnected) {
       qDebug() << "state disconnected";
       db.open(); // Try to reconnect
   } else if (state == ADatabase::Connected) {
       // subscribe to the notification again
       subscribe();
   }
});

db.open();
```

### Migrations
This feature allows for easy migration between database versions, ASql doesn't try to be smart at detecting your changes, it's up to you to write
and test for proprer SQL.

Each migration must have a positive integer number with up and down scripts, the current migration version is stored at asql_migrations table that is automatically created if you load a new migration.

```c++
auto mig = new AMigrations();

// load it from string or filename
mig->fromString(uR"V0G0N(
-- 1 up
create table messages (message text);
insert into messages values ('I ♥ Cutelyst!');
-- 1 down
drop table messages;
-- 2 up
create table log (message text);
insert into log values ('logged');
-- 2 down
drop table log;
-- 3 up
create table log (message text);
)V0G0N");

mig->connect(mig, &AMigrations::ready, [=] (bool error, const QString &erroString) {
    qDebug() << "LOADED" << error << erroString;

    // Migrate to version 2, if omitted defaults to the latest version available
    mig->migrate(2, [=] (bool error, const QString &errorString) {
        qDebug() << "MIGRATED" << error << errorString;
    });
});
mig->load(APool::database(), "my_app_foo");
```

### Coroutines
ASql fully supports C++20/23 coroutines. Use `ACoroTerminator` as the return type for fire-and-forget coroutines. Every `exec()` / `coDatabase()` call returns an awaitable directly — just `co_await` it and check the `std::expected<AResult, QString>` value.

> **Note:** Always define coroutines as free functions or static member functions, not as local lambdas. Coroutine frames outlive the statement that starts them, so a lambda that captures local variables by reference will produce dangling references.

#### Simple query via APool
The easiest entry point: `APool::exec()` grabs a connection, runs the query and resumes the coroutine — all in one `co_await`.
```c++
using namespace ASql;

ACoroTerminator runQuery()
{
    auto result = co_await APool::exec(u8"SELECT id, message FROM messages LIMIT 5");
    if (result.has_value()) {
        for (auto row : *result) {
            qDebug() << "id" << row[0].toInt() << "msg" << row["message"].toString();
        }
    } else {
        qDebug() << "Query error:" << result.error();
    }
}

// call it
runQuery();
```

#### Getting a connection and running multiple queries
`APool::coDatabase()` suspends until a pooled connection is available and then returns it wrapped in `std::expected`.
```c++
ACoroTerminator runQueries()
{
    auto db = co_await APool::coDatabase();
    if (!db) {
        qDebug() << "Could not get a connection:" << db.error();
        co_return;
    }

    auto result1 = co_await db->exec(u"SELECT now()"_s);
    if (result1.has_value()) {
        qDebug() << "now:" << result1->toJsonObject();
    }

    auto result2 = co_await db->exec(u"SELECT count(*) FROM messages"_s);
    if (result2.has_value()) {
        qDebug() << "count:" << result2->begin().value(0);
    }
}

runQueries();
```

#### Transactions
`ADatabase::beginTransaction()` returns an `AExpectedTransaction`. `co_await` it to start the transaction; the returned `ATransaction` will automatically roll back when it goes out of scope unless `commit()` is called.
```c++
ACoroTerminator runTransaction()
{
    auto db = co_await APool::coDatabase();
    if (!db) {
        qDebug() << "Connection error:" << db.error();
        co_return;
    }

    auto transaction = co_await db->beginTransaction();
    if (!transaction) {
        qDebug() << "BEGIN error:" << transaction.error();
        co_return;
    }

    auto result = co_await db->exec(u"INSERT INTO messages (message) VALUES ($1) RETURNING id"_s,
                                    {u"Hello from coroutine!"_s});
    if (!result) {
        qDebug() << "INSERT error:" << result.error();
        co_return; // transaction rolls back automatically
    }
    qDebug() << "Inserted id:" << result->begin().value(0).toInt();

    auto commit = co_await transaction->commit();
    if (!commit) {
        qDebug() << "COMMIT error:" << commit.error();
    }
}

runTransaction();
```

#### Lifetime management with co_yield
`co_yield` a `QObject*` pointer to tie the coroutine's lifetime to that object. If the object is destroyed while the coroutine is suspended (e.g. waiting for a slow query), the coroutine is destroyed and the function is never resumed — preventing dangling-pointer crashes.
```c++
ACoroTerminator runWithLifetime()
{
    auto *guard = new QObject;
    QTimer::singleShot(500, guard, [guard] { delete guard; }); // simulate early teardown

    co_yield guard; // coroutine is destroyed when guard is destroyed

    auto result = co_await APool::exec(u8"SELECT now(), pg_sleep(1)", guard);
    if (result.has_value()) {
        qDebug() << "result:" << result->toJsonObject();
    } else {
        qDebug() << "error (or cancelled):" << result.error();
    }
}

runWithLifetime();
```

#### Cached queries
Use `ACache::coExec()` inside a coroutine the same way as `ADatabase::exec()`.
```c++
ACoroTerminator runCached(ACache *cache)
{
    // First call hits the database
    auto result = co_await cache->coExec(u"SELECT now()"_s);
    if (result.has_value()) {
        qDebug() << "fresh result:" << result->toJsonObject();
    }

    // Second call with the same query returns the cached result immediately
    auto cached = co_await cache->coExec(u"SELECT now()"_s);
    if (cached.has_value()) {
        qDebug() << "cached result:" << cached->toJsonObject();
    }
}

auto cache = new ACache;
cache->setDatabase(APool::database());
runCached(cache);
```

### Caching
ASql can cache AResults in a transparent way, if exec() is called with the same query string or with same query string combined with the same parameters an unique entry is created on the Cache object.
```c++
auto cache = new ACache;
cache->setDatabase(APool::database());

// This query does not exist in cache so it will arive at the database
cache->exec(u"SELECT now()", [=] (AResult &result) {
    qDebug() << "CACHED 1" << result.errorString() << result.size();
    if (result.error()) {
        qDebug() << "Error" << result.errorString();
    }

    for (auto row : result) {
        for (int i = 0; i < result.fields(); ++i) {
            qDebug() << "cached 1" << row.at() << i << row.value(i);
        }
    }
});

QTimer::singleShot(2000, [=] {
    // this query will fetch the cached result, unless 2s were not enough in such case it will be queued
    cache->exec(u"SELECT now()", [=] (AResult &result) {
        qDebug() << "CACHED 2" << result.errorString() << result.size();
        if (result.error()) {
            qDebug() << "Error" << result.errorString();
        }

        for (auto row : result) {
            for (int i = 0; i < result.fields(); ++i) {
                qDebug() << "cached 2" << row.at() << i << row.value(i);
            }
        }
    });

    // Manually clears the cache
    bool ret = cache->clear("SELECT now()");
    qDebug() << "CACHED - CLEARED" << ret;

    // Since we cleared it this will result in a new db call
    cache->exec(u"SELECT now()", [=] (AResult &result) {
        qDebug() << "CACHED 3" << result.errorString() << result.size();
        if (result.error()) {
            qDebug() << "Error 3" << result.errorString();
        }

        for (auto row : result) {
            for (int i = 0; i < result.fields(); ++i) {
                qDebug() << "cached 3" << row.at() << i << row.value(i);
            }
        }
    });
});
```
