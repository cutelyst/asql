/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef ASQL_EXPORT_H
#define ASQL_EXPORT_H

#include <QtCore/QtGlobal>

#if defined(ASqlQt5_EXPORTS)
#define ASQL_EXPORT Q_DECL_EXPORT
#else
#define ASQL_EXPORT Q_DECL_IMPORT
#endif

#if defined(ASqlQt5Pg_EXPORTS)
#define ASQL_PG_EXPORT Q_DECL_EXPORT
#else
#define ASQL_PG_EXPORT Q_DECL_IMPORT
#endif

#endif
