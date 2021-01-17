# SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

add_executable(async1 async1.cpp)
target_link_libraries(async1
    ASqlQt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Core
)

add_executable(transactions transactions.cpp)
target_link_libraries(transactions
    ASqlQt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Core
)

add_executable(deleter deleter.cpp)
target_link_libraries(deleter
    ASqlQt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Core
)

add_executable(prepared prepared.cpp)
target_link_libraries(prepared
    ASqlQt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Core
)

add_executable(migrations migrations.cpp)
target_link_libraries(migrations
    ASqlQt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Core
)
