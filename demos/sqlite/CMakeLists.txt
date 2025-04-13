# SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

add_executable(sqlite sqlite.cpp)
target_link_libraries(sqlite
    ASql::Core
    ASql::Sqlite
    Qt::Core
)
