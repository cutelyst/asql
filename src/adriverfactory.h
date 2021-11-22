/*
 * SPDX-FileCopyrightText: (C) 2021 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#ifndef ADRIVERFACTORY_H
#define ADRIVERFACTORY_H

#include <aqsqlexports.h>

#include <memory>

class ADriver;
class ADatabase;
class ASQL_EXPORT ADriverFactory
{
public:
    ADriverFactory();
    virtual ~ADriverFactory();

    virtual ADriver *createRawDriver() const;
    virtual std::shared_ptr<ADriver> createDriver() const;
    virtual ADatabase createDatabase() const;
};

#endif // ADRIVERFACTORY_H
