# SPDX-FileCopyrightText: (C) 2020-2025 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

if (DRIVER_POSTGRES)
    add_subdirectory(async1)
endif()

if (DRIVER_SQLITE)
    add_subdirectory(sqlite)
endif()
