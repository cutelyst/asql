/*
 * SPDX-FileCopyrightText: (C) 2021 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#ifndef APG_H
#define APG_H

#include "adriverfactory.h"

#include <aqsqlexports.h>

class APgPrivate;
class ASQL_PG_EXPORT APg : public ADriverFactory
{
public:
    APg(const QString &connectionInfo);
    ~APg();

    static std::shared_ptr<ADriverFactory> factory(const QString &connectionInfo);

    ADriver *createRawDriver() const final;
    std::shared_ptr<ADriver> createDriver() const final;
    ADatabase database() const final;

private:
    APgPrivate *d;
};

#endif // APG_H
