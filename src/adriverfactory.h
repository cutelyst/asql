/*
 * SPDX-FileCopyrightText: (C) 2021-2022 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#ifndef ADRIVERFACTORY_H
#define ADRIVERFACTORY_H

#include <asqlexports.h>
#include <memory>

namespace ASql {

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

} // namespace ASql

#endif // ADRIVERFACTORY_H
