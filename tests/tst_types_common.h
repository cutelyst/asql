/*
 * SPDX-FileCopyrightText: (C) 2025 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "CoverageObject.hpp"
#include "apool.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

/*!
 * \brief Base class for per-driver type round-trip tests.
 *
 * All test slots are defined here so they are shared across all driver-specific
 * test executables.  Each driver-specific subclass only needs to:
 *   - Override initTest() / cleanupTest() to set up/tear down its APool.
 *   - Optionally override selectParam() (Postgres needs "SELECT $1").
 */
class TestTypesBase : public CoverageObject
{
    Q_OBJECT
public:
    explicit TestTypesBase(QObject *parent = nullptr);

protected:
    /*!
     * Returns the single-parameter echo query appropriate for this driver.
     * Default is "SELECT ?" (ODBC, MySQL, SQLite).
     * Postgres subclass overrides this to return "SELECT $1".
     */
    virtual QString selectParam() const;

private Q_SLOTS:
    void testBool();
    void testInt();
    void testLongLong();
    void testDouble();
    void testString();
    void testByteArray();
    void testUuid();
    void testDateTime();
    void testDate();
    void testTime();
    void testJson();
    void testNull();
};
