/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "CoverageObject.hpp"
#include "apreparedquery.h"

#include <QObject>

/*!
 * \brief Base class for per-driver prepared-query tests.
 *
 * Tests verify that APreparedQuery objects:
 *  - correctly execute a parameterised query on first use (lazy prepare),
 *  - reuse the server-side prepared statement across repeated calls,
 *  - work correctly on every connection in the pool (each connection
 *    independently prepares on first use), and
 *  - work via the APreparedQueryLiteral convenience macro.
 *
 * Each driver-specific subclass only needs to override initTest() /
 * cleanupTest() and optionally preparedParam() (Postgres uses "$1").
 */
class TestPreparedBase : public CoverageObject
{
    Q_OBJECT
public:
    explicit TestPreparedBase(QObject *parent = nullptr);

protected:
    /*!
     * Returns the single-parameter echo query for this driver.
     * Default is "SELECT ?" (MySQL, SQLite, ODBC).
     * Postgres overrides this to return "SELECT $1".
     */
    virtual QString preparedParam() const;

    /*!
     * Returns an APreparedQuery built via APreparedQueryLiteral for this driver.
     * Overriding in a derived class places the static APreparedQuery in that
     * method's scope, which is the intended usage of the macro.
     */
    virtual ASql::APreparedQuery preparedParamLiteral() const;

private Q_SLOTS:
    void testPreparedReuse();
    void testPreparedNoParams();
    void testPreparedLiteral();
    void testPreparedMultipleConnections();
};
