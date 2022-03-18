<!-- SPDX-FileCopyrightText: (C) 2020-2022 Daniel Nicoletti <dantti12@gmail.com>
     SPDX-License-Identifier: MIT
-->

# ASql
Qt Async Sql library

## Features
* PostgreSQL driver
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
* Qt, 5.10 or later (including Qt6)
* C++11 capable compiler

Currently, there is only one driver, for PostgreSQL, so PostgreSQL
libraries are required as well.

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
db.exec(u"SELECT id, message FROM messages LIMIT 5", [=] (AResult &result) {
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
db.exec(u"SELECT * FROM logs LIMIT 5; SELECT * FROM messages LIMIT 5", [=] (AResult &result) {
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
it holds an unique identification for your query, in order to make this easier one can use the APreparedQueryLiteral macro,
manually create a static APreparedQuery object or by having your query as a member of a class that isn't going to be deleted soon.
```c++
// PostgreSQL uses numered place holders, and yes you can repeat them :)
db.exec(APreparedQueryLiteral(u"INSERT INTO temp4 VALUE ($1, $2, $3, $4, $5, $6, $7) RETURNING id"),
{
     true,
     QStringLiteral("foo"),
     qint64(1234),
     QDateTime::currentDateTime(),
     123456.78,
     QUuid::createUuid(),
     QJsonObject{
          {"foo", true}
     }
}, [=] (AResult &result) {
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
     QStringLiteral("foo"),
     qint64(1234),
     QDateTime::currentDateTime(),
     123456.78,
     QUuid::createUuid(),
     QJsonObject{
          {"foo", true}
     }
}, [=] (AResult &result) mutable {
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
db.exec(u"SELECT pg_sleep(5)", [=] (AResult &result) {
    // This will never be called but it would crash
    // if cancelator was dangling reference and not passed as last parameter
    cancelator->setProperty("foo");

}, cancelator); // notice the object being passed here
    
delete cancelator;
```
### Notifications (Postgres only)

Each Database object can have a single function subscribed to one notification, if the connection is closed the subscription is cleared and a new subscription has to be made, this it's handy to have the subcription call on a named lambda:

```c++
ADatabase db;
auto subscribe = [=] () mutable {
   db.subscribeToNotification(QStringLiteral("my_awesome_notification"),
     [=] (const ADatabaseNotification &notification) {
       qDebug() << "DB notification:" << notification.self << notification.name
                << notification.payload;
   }, this);
};

db.onStateChanged([=] (ADatabase::State state, const QString &status) mutable {
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
