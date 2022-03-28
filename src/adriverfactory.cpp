/*
 * SPDX-FileCopyrightText: (C) 2021 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#include "adriverfactory.h"

#include <adatabase.h>

using namespace ASql;

ADriverFactory::ADriverFactory()
{

}

ADriver *ADriverFactory::createRawDriver() const
{
    return nullptr;
}

std::shared_ptr<ADriver> ADriverFactory::createDriver() const
{
    return {};
}

ADatabase ADriverFactory::createDatabase() const
{
    return {};
}

ADriverFactory::~ADriverFactory() = default;
