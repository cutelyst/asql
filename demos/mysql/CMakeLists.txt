# SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

add_executable(mysql mysql.cpp)
target_link_libraries(mysql
    ASql::Core
    ASql::Mysql
    Qt::Core
)
