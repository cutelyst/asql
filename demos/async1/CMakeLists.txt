# SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

add_executable(async1 async1.cpp)
target_link_libraries(async1
    ASql::Core
    ASql::Pg
    Qt::Core
)

add_executable(transactions transactions.cpp)
target_link_libraries(transactions
    ASql::Core
    ASql::Pg
    Qt::Core
)

add_executable(deleter deleter.cpp)
target_link_libraries(deleter
    ASql::Core
    ASql::Pg
    Qt::Core
)

add_executable(pipeline pipeline.cpp)
target_link_libraries(pipeline
    ASql::Core
    ASql::Pg
    Qt::Core
)

add_executable(prepared prepared.cpp)
target_link_libraries(prepared
    ASql::Core
    ASql::Pg
    Qt::Core
)

add_executable(migrations migrations.cpp)
target_link_libraries(migrations
    ASql::Core
    ASql::Migrations
    ASql::Pg
    Qt::Core
)
