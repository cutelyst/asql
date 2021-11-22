#include "adriverfactory.h"

ADriverFactory::ADriverFactory()
{

}

ADriver *ADriverFactory::createRawDriver() const
{
    qDebug("Fac RAW");
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
